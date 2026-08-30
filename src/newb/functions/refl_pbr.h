#ifndef REFL_PBR_H
#define REFL_PBR_H

/*
  PBR block reflection - ported/adapted from user's "block reflection V3".
  Original was a standalone #version 310 es Shadertoy/ShaderRed demo that used
  a dedicated `ironblock` sampler with getNormal()/getTBN()/brdf(). Here it is
  adapted to CraftEdge (newb-x / bgfx .sc) so it runs in the RenderChunk
  fragment stage on smooth/reflective blocks, using the block's own texture
  (s_MatTexture) as the normal source and screen-space derivatives for the TBN.
*/

#include "utils.h"

#ifndef NL_PBR_ROUGHNESS
  #define NL_PBR_ROUGHNESS 0.2
#endif
#ifndef NL_PBR_METALLIC
  #define NL_PBR_METALLIC 0.0
#endif
#ifndef NL_PBR_SUNCOLOR
  #define NL_PBR_SUNCOLOR vec3(1.0, 0.5, 0.1)
#endif
// iron-like base reflectance (F0) from V3
#ifndef NL_PBR_F0
  #define NL_PBR_F0 vec3(0.56, 0.57, 0.58)
#endif
// max specular energy - clamps GGX spikes that show as golden fireflies
#ifndef NL_PBR_SPEC_CLAMP
  #define NL_PBR_SPEC_CLAMP 1.0
#endif
#ifndef NL_PBR_SPEC_INTENSITY
  #define NL_PBR_SPEC_INTENSITY 1.0
#endif

// --- luminance (BT.601), same as V3 ---
float nlLum601(vec3 color) {
  return color.r*0.299 + color.g*0.587 + color.b*0.114;
}

// --- fresnel (Schlick + roughness), from V3 ---
float nlFresnelSchlickRoughness(vec3 N, vec3 V, float F0, float roughness) {
  float cosTheta = clamp(dot(normalize(N), normalize(V)), 0.0, 1.0);
  return F0 + (max(1.0 - roughness, F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

// --- normal from texture luminance gradient (V3 getNormal) ---
// texelSize = 1.0 / texture resolution (approx)
vec3 nlTexNormal(sampler2D tex, vec2 coord, vec2 texelSize, float strength) {
  // v_texcoord0 is the atlas UV. Keep taps inside the 16-texel tile selected
  // by that UV so the normal gradient cannot read a neighboring block.
  vec2 tileSize = 16.0*texelSize;
  vec2 tileMin = floor(coord/tileSize)*tileSize;
  vec2 tileMax = tileMin + tileSize;
  vec2 uvR = clamp(coord + vec2(texelSize.x, 0.0), tileMin + texelSize, tileMax - texelSize);
  vec2 uvL = clamp(coord - vec2(texelSize.x, 0.0), tileMin + texelSize, tileMax - texelSize);
  vec2 uvD = clamp(coord + vec2(0.0, texelSize.y), tileMin + texelSize, tileMax - texelSize);
  vec2 uvU = clamp(coord - vec2(0.0, texelSize.y), tileMin + texelSize, tileMax - texelSize);
  float lumR = nlLum601(texture2D(tex, uvR).rgb);
  float lumL = nlLum601(texture2D(tex, uvL).rgb);
  float lumD = nlLum601(texture2D(tex, uvD).rgb);
  float lumU = nlLum601(texture2D(tex, uvU).rgb);

  vec2 gradient = vec2(lumR - lumL, lumD - lumU) * strength;
  // limit gradient so high-contrast texels (ore specks, block edges) can't
  // create near-horizontal normals that catch a full specular spike
  gradient = clamp(gradient, vec2_splat(-0.6), vec2_splat(0.6));
  float lenSq = dot(gradient, gradient);
  vec3 normal = vec3(gradient, sqrt(max(0.0, 1.0 - lenSq)));
  return normalize(normal);
}

// --- TBN basis from a geometric normal (V3 getTBN) ---
mat3 nlGetTBN(vec3 normal) {
  vec3 T = vec3(abs(normal.y) + normal.z, 0.0, normal.x);
  vec3 B = vec3(0.0, -abs(normal.x) - abs(normal.z), abs(normal.y));
  vec3 N = normal;
  // bgfx: build rows then transpose (matches V3 transpose(mat3(T,B,N)))
  return transpose(mtxFromRows(T, B, N));
}

// --- Cook-Torrance specular (V3 brdf, specular-only) ---
// NOTE: V3's original brdf() returned diffuse+specular. Here the caller
// already has the shaded/reflected color, so returning the diffuse term again
// double-counted the albedo and the raw GGX spike produced golden "firefly"
// speckles on the ground. This returns the specular lobe only, energy-clamped.
vec3 nlBrdfSpec(vec3 lightDir, vec3 viewDir, float roughness, vec3 normal, vec3 reflectance, vec3 sunCol) {
  // clamp roughness so the GGX lobe can't become a singular spike
  float r = clamp(roughness, 0.25, 1.0);
  float alpha = r*r;
  vec3 halfVector = lightDir + viewDir;
  float halfLengthSq = dot(halfVector, halfVector);
  vec3 H = halfLengthSq > 0.000001 ? halfVector/sqrt(halfLengthSq) : normal;

  float NdotV = clamp(dot(normal, viewDir), 0.001, 1.0);
  float NdotL = clamp(dot(normal, lightDir), 0.0, 1.0);
  float NdotH = clamp(dot(normal, H), 0.0, 1.0);
  float VdotH = clamp(dot(viewDir, H), 0.0, 1.0);

  // Fresnel
  vec3 F0 = reflectance;
  vec3 F = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);

  // Geometry (Schlick-GGX)
  float k = alpha*0.5;
  float geometry = (NdotL / (NdotL*(1.0-k)+k)) * (NdotV / (NdotV*(1.0-k)+k));

  // GGX NDF
  float lowerTerm = NdotH*NdotH * (alpha*alpha - 1.0) + 1.0;
  float ndf = (alpha*alpha) / (PI * lowerTerm*lowerTerm);

  vec3 spec = (F*ndf*geometry) / (4.0*NdotL*NdotV + 0.001);
  spec *= sunCol * NdotL;

  // hard clamp kills leftover fireflies from per-pixel normal noise
  return min(spec, vec3_splat(NL_PBR_SPEC_CLAMP));
}

/*
  nlApplyPbrRefl - the fragment-stage V3 reflection layer.
  - baseColor    : shaded block color (in/out)
  - reflColor    : sky/cloud reflection color from vertex (v_refl.rgb)
  - reflMask     : reflection strength (v_refl.a related)
  - texNormal    : normal sampled from block texture (nlTexNormal)
  - worldPos     : v_position (world position)
  - viewDir      : direction from surface to camera
  - sunDir       : sun/moon direction (from env)
  Returns modified baseColor with the V3 PBR mirror reflection blended in.

  Fragment-stage only: uses dFdx/dFdy, which don't exist in the vertex stage.
*/
#if BGFX_SHADER_TYPE_FRAGMENT
vec3 nlApplyPbrRefl(
  vec3 baseColor, vec3 reflColor, float reflMask, vec3 texNormal,
  vec3 worldPos, vec3 viewDir, vec3 sunDir
) {
  // geometric normal from screen-space derivatives (V3 uses cross(dFdx,dFdy))
  vec3 dpx = dFdx(worldPos);
  vec3 dpy = dFdy(worldPos);
  vec3 Ngeo = normalize(cross(dpx, dpy));
  if (dot(Ngeo, viewDir) < 0.0) Ngeo = -Ngeo;

  mat3 TBN = nlGetTBN(Ngeo);
  vec3 worldNormal = normalize(mul(texNormal, TBN));

  vec3 V = normalize(viewDir);

  // fresnel-roughness controls how mirror-like the surface reads (V3)
  float fresnel = nlFresnelSchlickRoughness(worldNormal, V, 0.6, NL_PBR_ROUGHNESS);
  fresnel = clamp(fresnel*reflMask, 0.0, 1.0);

  // blend albedo with sky/cloud reflection using fresnel (V3: base = base*(1-F) + sky*F)
  vec3 col = baseColor*(1.0 - fresnel) + reflColor*fresnel;

  // Cook-Torrance sun/moon specular glint (specular only - adding the BRDF
  // diffuse term here double-counted albedo and caused golden speckles)
  vec3 specular = nlBrdfSpec(sunDir, V, NL_PBR_ROUGHNESS, worldNormal, NL_PBR_F0, NL_PBR_SUNCOLOR);
  col += specular*reflMask*NL_PBR_SPEC_INTENSITY;

  return col;
}
#endif // BGFX_SHADER_TYPE_FRAGMENT

#endif
