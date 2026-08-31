$input v_color0, v_color1, v_fog, v_refl, v_texcoord0, v_lightmapUV, v_extra, v_position, v_reflPbr, v_reflSun

#include <bgfx_shader.sh>
#include <newb/main.sh>

SAMPLER2D_AUTOREG(s_MatTexture);
SAMPLER2D_AUTOREG(s_SeasonsTexture);
SAMPLER2D_AUTOREG(s_LightMapTexture);
SAMPLER2D_AUTOREG(s_SunTexture);

uniform vec4 CameraPosition;
uniform vec4 ViewPositionAndTime;
uniform vec4 FogColor;

/*
  Water cloud mirror.

  The sky renders its clouds as a dome that is sampled purely by view
  direction, so a true mirror only needs the *reflected direction* - not a
  projection onto a cloud plane. The old plane-projection version broke in
  three ways:
    - it aimed the sampler at a fixed 4-block depth, which collapsed all
      parallax and left a flat smear,
    - `NL_WATER_CLOUD_HEIGHT - CameraPosition.y` flipped sign above y=192, so
      clouds vanished when the player climbed,
    - a 144..256 block distance fade erased the reflection as soon as the
      camera pulled away from the water.
  Sampling by direction fixes all three: the reflected clouds now match the
  sky exactly and keep the same size at any camera height.
*/
vec4 waterCloudReflection(
  vec3 surfacePos, vec3 viewDir, float rain, float dayFactor, vec3 horizonCol, highp float t
) {
  vec3 V = normalize(viewDir);

  // Flat mirror. Perturbing the normal before reflect() looks correct on
  // paper but the dome projection below divides by reflDir.y, so a tiny tilt
  // near the horizon is amplified by ~1/y^2 - that is what tore the reflected
  // clouds into warping, shape-shifting blobs. A flat normal keeps the
  // mirrored shapes identical to the sky.
  vec3 reflDir = vec3(-V.x, V.y, -V.z);
  // below the horizon there is no sky to mirror (also true when underwater,
  // where the reflected ray points down)
  if (reflDir.y <= 0.004) return vec4_splat(0.0);

  // Surface motion is applied here, in sky-UV space, where it is a bounded
  // translation: the clouds drift gently like a real swell instead of
  // stretching. Slow and low-frequency on purpose.
  vec2 wobble = vec2(
    sin(surfacePos.x*0.09 + 0.30*t) + sin(surfacePos.z*0.06 - 0.21*t),
    cos(surfacePos.z*0.08 + 0.26*t) + cos(surfacePos.x*0.05 - 0.18*t)
  );
  wobble *= 0.5*NL_WATER_CLOUD_REFL_RIPPLE;

  // Depth cue: shift the sampled cloud image sideways as if the mirror sat
  // NL_WATER_CLOUD_REFLECTION_DEPTH blocks below the surface. This is a
  // translation, never a scale, so the reflected clouds keep the exact same
  // size as the sky ones. Clamped so it can't blow up near the horizon.
  vec2 depthShift = clamp(
    NL_WATER_CLOUD_REFLECTION_DEPTH*reflDir.xz/max(reflDir.y, 0.08),
    -vec2_splat(64.0), vec2_splat(64.0)
  );

  vec4 clouds;
  #ifdef VIBRANT_CLOUD
    // sky-dome clouds are camera-locked, so mirror them the same way
    float domeScale = 0.8/max(reflDir.y, 0.045);
    vec2 domeUV = reflDir.xz*domeScale + depthShift*0.0025 + wobble;
    float mask = nlVibrantClouds(domeUV, 0.004*domeScale, t);
    mask *= smoothstep(0.05, 0.35, reflDir.y)*NL_SKY_CLOUD_OPACITY;
    clouds = vec4(nlVibrantCloudColor(dayFactor, sunLightTint(dayFactor, rain)), mask);
  #else
    // renderOldClouds builds uv as dir.xz*(0.8/dir.y), so pre-multiplying the
    // wobble by dir.y/0.8 makes it land as an exact, angle-independent uv
    // offset - no stretching at grazing angles.
    vec3 sampleDir = reflDir;
    sampleDir.xz += wobble*max(reflDir.y, 0.025)/0.8;
    // same sampler and same camera offset the Clouds material uses, so the
    // mirrored shapes line up 1:1 with the sky
    clouds = renderOldClouds(sampleDir, CameraPosition.xz + depthShift, t, rain, horizonCol);
  #endif

  // water is more mirror-like at grazing angles
  float fresnel = calculateFresnel(V.y, 0.02);
  clouds.a *= NL_WATER_CLOUD_MIRROR*mix(0.55, 1.0, sqrt(fresnel));
  clouds.rgb *= 1.0 - 0.45*rain;
  // distance falloff is left to the fog blend below, which already matches the
  // sky - an extra fade here is what made far reflections disappear
  return clouds;
}

/*
  Real textured sun mirror on water.

  Projects the reflected view ray into the sun's local plane (built from the
  sun direction), samples the vanilla sun texture there, and keeps only the
  bright sun pixels (luminance mask) inside a soft circular falloff (dist
  mask). The texture is bound through `SunTexture` buffer -> textures/
  environment/sun, NOT a white default - a white default rendered the mirror
  invisible.
*/
vec3 sunTextureMovement(vec3 sunDir, vec3 rayDir, out float mask) {
  vec3 forward = normalize(sunDir);
  vec3 right = normalize(cross(vec3(0.0, 1.0, 0.0), forward));
  vec3 up = cross(forward, right);

  vec2 uv = vec2(dot(rayDir, right), dot(rayDir, up));
  float sunSize = NL_WATER_SUN_QUAD_TAN;
  uv = uv / sunSize * 0.5 + 0.5;

  vec3 sunColor = texture2D(s_SunTexture, uv).rgb;
  float lum = dot(sunColor, vec3(0.299, 0.587, 0.114));
  float maskLum = smoothstep(0.0, 0.9, lum);
  float distMask = smoothstep(0.5, 0.48, length(uv - 0.5));
  mask = maskLum * distMask;
  return sunColor * mask;
}

void main() {
  #if defined(DEPTH_ONLY_OPAQUE) || defined(DEPTH_ONLY) || defined(INSTANCING)
    gl_FragColor = vec4(1.0,1.0,1.0,1.0);
    return;
  #endif

  vec4 diffuse = texture2D(s_MatTexture, v_texcoord0);
  vec4 color = v_color0;

  #ifdef ALPHA_TEST
    if (diffuse.a < 0.6) {
      discard;
    }
  #endif

  #if defined(SEASONS) && (defined(OPAQUE) || defined(ALPHA_TEST))
    diffuse.rgb *= mix(vec3(1.0,1.0,1.0), texture2D(s_SeasonsTexture, v_color1.xy).rgb * 2.0, v_color1.z);
  #endif

  vec3 glow = nlGlow(s_MatTexture, v_texcoord0, v_extra.a);

  diffuse.rgb *= diffuse.rgb;

  #if defined(TRANSPARENT) && !(defined(SEASONS) || defined(RENDER_AS_BILLBOARDS))
    if (v_extra.b > 0.9) {
      diffuse.rgb = vec3_splat(1.0 - NL_WATER_TEX_OPACITY*(1.0 - diffuse.b*1.8));
      diffuse.a = color.a;
    }
  #else
    diffuse.a = 1.0;
  #endif

  diffuse.rgb *= color.rgb;
  diffuse.rgb += glow;

  if (v_extra.b > 0.9) {
    diffuse.rgb += v_refl.rgb*v_refl.a;

    #ifndef NL_NO_WATER_CLOUD_REFL
      vec3 surfacePos = v_position+CameraPosition.xyz;

      // rebuild the sky palette so the mirrored clouds are shaded with the
      // same horizon tint the Clouds material uses (cheap: only mix() ops)
      nl_environment wenv;
      wenv.end = false;
      wenv.nether = false;
      wenv.underwater = false;
      wenv.rainFactor = v_reflPbr.w;
      wenv.dayFactor = v_reflSun.w;
      wenv.sunDir = v_reflSun.xyz;
      wenv.moonDir = -v_reflSun.xyz;
      wenv.fogCol = FogColor.rgb;
      nl_skycolor wskycol = nlOverworldSkyColors(wenv);

      vec4 cloudReflection = waterCloudReflection(
        surfacePos, v_reflPbr.xyz, wenv.rainFactor, wenv.dayFactor,
        wskycol.horizonEdge, ViewPositionAndTime.w
      );
      diffuse.rgb = mix(diffuse.rgb,cloudReflection.rgb,cloudReflection.a);

      // real textured sun mirror on water. The reflected ray uses a wavy water
      // normal (same movingNoise2D bump as nlWater), so the sun glitters as a
      // path across the surface instead of a single flat-mirror point that
      // vanishes as the sun rises.
      vec3 sunV = normalize(v_reflPbr.xyz);
      vec2 sunBump = vec2_splat(movingNoise2D(
        surfacePos.xz + surfacePos.yy, NL_WATER_WAVE_SPEED*ViewPositionAndTime.w, 0.6
      ));
      vec3 sunNrm = normalize(vec3(sunBump*NL_WATER_BUMP, 1.0));
      vec3 sunReflDir = reflect(-sunV, sunNrm);
      if (sunReflDir.y > 0.004) {
        float sunMask;
        vec3 sunTex = sunTextureMovement(wenv.sunDir, sunReflDir, sunMask);
        float sunVisible = (1.0 - wenv.rainFactor)*smoothstep(-0.12, 0.06, wenv.dayFactor);
        vec3 sunCol = sunLightTint(wenv.dayFactor, wenv.rainFactor);
        sunCol *= NL_SUNLIGHT_INTENSITY;
        diffuse.rgb += sunTex*sunCol*NL_WATER_SUN_DISC*sunVisible;
      }
    #endif
  } else if (v_refl.a > 0.0) {
    // reflective effect - only on xz plane (ground / flat smooth blocks)
    float dy = abs(dFdy(v_extra.g));
    if (dy < 0.0002) {
      float mask = v_refl.a*(clamp(v_extra.r*10.0,8.2,8.8)-7.8);

      // RTX-style: drop diffuse and push the mirror reflection forward so
      // smooth blocks (iron, diamond, quartz) read like a true mirror
      #ifdef NL_PBR_BLOCK_REFL
        // === V3 PBR block reflection (normal-map + TBN + Cook-Torrance) ===
        vec3 viewDir = v_reflPbr.xyz;
        float rainFactor = v_reflPbr.w;
        vec3 sunDir = v_reflSun.xyz;

        // Clear blocks use a smooth normal, avoiding four texture samples per
        // reflected pixel. Rain restores the detailed texture-derived normal
        // so wet surfaces keep the existing high-quality PBR response.
        vec3 texNormal = vec3(0.0, 0.0, 1.0);
        if (rainFactor > 0.001) {
          texNormal = nlTexNormal(s_MatTexture, v_texcoord0, NL_PBR_ATLAS_TEXEL, NL_PBR_NORMAL_STRENGTH);
        }

        // rain makes the ground wetter -> stronger, smoother mirror
        float pbrMask = mask;
        #ifdef NL_PBR_RAIN_BOOST
          pbrMask *= 1.0 + NL_PBR_RAIN_BOOST*rainFactor;
        #endif
        pbrMask = min(pbrMask, 1.0);

        // drop some diffuse so the mirror reads through, then apply V3 layer
        diffuse.rgb *= 1.0 - 0.6*pbrMask;
        diffuse.rgb = nlApplyPbrRefl(diffuse.rgb, v_refl.rgb, pbrMask, texNormal, v_position, viewDir, sunDir);
      #else
        diffuse.rgb *= 1.0 - 0.75*mask;
        diffuse.rgb += v_refl.rgb*mask*1.15;
      #endif
    }
  }

  diffuse.rgb = mix(diffuse.rgb, v_fog.rgb, v_fog.a);

  diffuse.rgb = colorCorrection(diffuse.rgb);

  gl_FragColor = diffuse;
}
