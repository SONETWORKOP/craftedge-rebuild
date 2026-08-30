#ifndef LIGHTING_H
#define LIGHTING_H

#include "detection.h"
#include "sky.h"
#include "utils.h"
#include "noise.h"
#include "clouds.h"

vec3 sunLightTint(float dayFactor, float rain) {
  float nightFactor = step(dayFactor, 0.0);
  float dawnFactor = 1.0-dayFactor*dayFactor;
  dawnFactor *= dawnFactor*dawnFactor;
  dawnFactor *= mix(1.0, dawnFactor*dawnFactor, nightFactor);
  // warm yellow noon light with orange sunrise transition
  vec3 tint = mix(NL_NOON_SUNLIGHT_COL, NL_NIGHT_MOONLIGHT_COL, nightFactor);
  tint = mix(tint, NL_DAWN_SUNLIGHT_COL, dawnFactor);
  tint = mix(tint, vec3_splat(dot(tint, vec3_splat(0.33))), rain);
  // add warm yellow tint during day
  float dayBright = max(dayFactor, 0.0);
  tint = mix(tint, tint * vec3(1.05, 0.98, 0.85), dayBright * 0.3);
  return tint;
}

vec3 nlLighting(
  sampler2D tex, nl_skycolor skycol, nl_environment env, vec3 wPos, out vec3 torchColor, vec3 COLOR,
  vec2 uv1, vec2 lit, bool isTree, float shade, highp float t, float renderdistance, float TIME_OF_DAY, vec3 CAMERA_POS
) {
  vec3 light;

  if (env.underwater) {
    torchColor = NL_UNDERWATER_TORCH_COL;
  } else if (env.end) {
    torchColor = NL_END_TORCH_COL;
  } else if (env.nether) {
    torchColor = NL_NETHER_TORCH_COL;
  } else {
    torchColor = NL_OVERWORLD_TORCH_COL;
  }

  float torchAttenuation = (NL_TORCHLIGHT_INTENSITY*uv1.x)/(0.5-0.45*lit.x);

  #ifdef NL_BLINKING_TORCH
    torchAttenuation *= 1.0 - 0.19*noise1D(t*8.0);
  #endif

  vec3 torchLight = torchColor*torchAttenuation;
  float gameBrightness = texelFetch(tex, ivec2(0,0), 0).g;
  float lum = 0.0;

  if (env.nether || env.end) {
    // nether & end lighting

    light = env.end ? NL_END_AMBIENT : NL_NETHER_AMBIENT;
    light *= gameBrightness;

    lum = luminance(light);
    light += skycol.horizon/(1.0+lum);

  } else {
    // overworld lighting
    float nightFactor = step(env.dayFactor, 0.0);
    float dawnFactor = 1.0-env.dayFactor*env.dayFactor;
    dawnFactor *= dawnFactor*dawnFactor;
    dawnFactor *= mix(1.0, dawnFactor*dawnFactor, nightFactor);
    float nightIntensity = 1.0-(0.5+0.5*env.dayFactor);
    nightIntensity *= nightIntensity;

    float sunLightAttenuation = clamp(0.5*(((2.0*step(TIME_OF_DAY, 0.5)-1.0)*(wPos.x*cos(NL_SUN_PATH_YAW)+wPos.y*sin(NL_SUN_PATH_YAW))/renderdistance) + 1.0), 0.0, 1.0);
    sunLightAttenuation = mix(1.0, sunLightAttenuation*sunLightAttenuation, dawnFactor);
    sunLightAttenuation *= 1.0-0.4*env.rainFactor;

    // shadow cast by sun light - BSL-like deep, sharp shadows
    float shadow = step(0.93, uv1.y);
    // sharpen the sun/shade terminator so lit->shadow transition is crisp
    float litSharp = smoothstep(0.35, 0.75, lit.y);
    shadow = max(shadow, (1.0 - NL_SHADOW_INTENSITY + (0.5*NL_SHADOW_INTENSITY*nightIntensity))*litSharp);
    shadow *= shade > 0.8 ? 1.0 : 0.65;
    #ifdef NL_CLOUD_SHADOW
      // shadow cast by simple clouds
      vec3 mainLightDir = env.sunDir.y > 0.0 ? env.sunDir : env.moonDir;
      vec3 gPos = wPos + CAMERA_POS;
      float cloudRelativeHeight = gPos.y-187.0;
      float lightDirY = mainLightDir.y >= 0.0 ? max(mainLightDir.y, 0.01) : min(mainLightDir.y, -0.01);
      vec2 projectionOffset = cloudRelativeHeight*mainLightDir.xz/lightDirY;
      projectionOffset = clamp(projectionOffset, -vec2_splat(4096.0), vec2_splat(4096.0));
      vec2 projectedPos = gPos.xz + projectionOffset;
      float cloudFade = smoothstep(1.0, 0.5, length(0.002*(wPos.xz + projectionOffset)));
      cloudFade *= smoothstep(0.02, 0.12, abs(mainLightDir.y));
      cloudFade *= (1.0-dawnFactor*dawnFactor)*clamp(-0.12*(cloudRelativeHeight-7.0), 0.0, 1.0);
      // tighter smoothstep = crisper cloud shadow edges, darker core
      shadow *= 0.15 + 0.85*smoothstep(0.5, 0.15, cloudNoise2D(projectedPos*NL_CLOUD1_SCALE, t, env.rainFactor)*cloudFade);
    #endif

    // direct light from top - BSL-like strong directional
    light = (NL_SUNLIGHT_INTENSITY*shadow*sunLightAttenuation)*sunLightTint(env.dayFactor, env.rainFactor);

    // sky ambient - reduced for deeper shadow contrast
    lum = luminance(light);
    light += (skycol.horizon + skycol.zenith)*(uv1.y/(1.5+lum));

  }

  // torch light
  lum = luminance(light);
  light += torchLight/(1.0+lum);

  // game min brightness
  if (!(env.nether || env.end)) {
    lum = luminance(light);
    light += vec3_splat(gameBrightness*(NL_MIN_LIGHTING_BOOST/(1.0+lum)));
  }

  // darken at crevices
  light *= COLOR.g > 0.35 ? 1.0 : 0.8;

  // brighten tree leaves
  if (isTree) {
    light *= 1.25;
  }

  return light;
}

void nlUnderwaterLighting(inout vec3 light, inout vec3 pos, vec2 lit, vec2 uv1, vec3 tiledCpos, vec3 cPos, highp float t, vec3 horizonCol) {
  if (uv1.y < 0.9) {
    // Fade caustics out with distance: far blocks lose float precision in
    // disp()/sin() and produce black point-speckles. Distance-fade keeps
    // near caustics detailed while far blocks stay evenly lit.
    float causticFade = clamp(1.0 - length(pos.xyz)*0.045, 0.0, 1.0);
    float caustics = disp(tiledCpos, NL_WATER_WAVE_SPEED*t);
    caustics = max(caustics, 0.0);           // never subtract -> no black dots
    caustics *= 3.0*caustics*causticFade;
    light += NL_UNDERWATER_BRIGHTNESS + NL_CAUSTIC_INTENSITY*caustics*(0.15 + lit.y + lit.x*0.7);
  }
  // safe water tint: normalize() of a near-black underwater fog color
  // returns NaN and renders blocks pure black. Clamp to a tiny epsilon so
  // the tint stays a valid direction even when the fog color is ~0.
  vec3 waterTint = normalize(max(horizonCol, vec3_splat(0.0001)));
  light *= mix(waterTint, vec3_splat(0.6), lit.y*0.6);
  // floor so deep/dark/murky water never crushes blocks to pure black
  light = max(light, vec3_splat(NL_UNDERWATER_BRIGHTNESS*0.30));
  #ifdef NL_UNDERWATER_WAVE
    pos.xy += NL_UNDERWATER_WAVE*min(0.05*pos.z,0.6)*sin(t*1.2 + dot(cPos,vec3_splat(PI_HALF)));
  #endif
}

vec3 nlEntityLighting(nl_skycolor skycol, nl_environment env, vec3 pos, vec4 normal, vec3 wPos, mat4 world, vec4 tileLightCol, vec4 overlayCol, vec3 horizonEdgeCol, float t, float TIME_OF_DAY, float renderdistance, vec3 CAMERA_POS) {
  float l = tileLightCol.b;
  float tl = tileLightCol.r;
  float lum;
  vec3 light;
  if (env.nether || env.end) {
    tl = max(tl-0.6, 0.0);
    tl *= 21.0*tl;

    // nether & end lighting
    light = env.end ? NL_END_AMBIENT : NL_NETHER_AMBIENT;
    light *= min(tileLightCol.b, 0.25);

    lum = luminance(light);
    light += skycol.horizon/(1.0+lum);
  } else {
    tl = max(tl-0.08, 0.0);
    tl *= 4.0*tl;

    float nightFactor = step(env.dayFactor, 0.0);
    float dawnFactor = 1.0-env.dayFactor*env.dayFactor;
    dawnFactor *= dawnFactor*dawnFactor;
    dawnFactor *= mix(1.0, dawnFactor*dawnFactor, nightFactor);
    float nightIntensity = 1.0-(0.5+0.5*env.dayFactor);
    nightIntensity *= nightIntensity;

    float sunLightAttenuation = clamp(0.5*(((2.0*step(TIME_OF_DAY, 0.5)-1.0)*(wPos.x*cos(NL_SUN_PATH_YAW)+wPos.y*sin(NL_SUN_PATH_YAW))/renderdistance) + 1.0), 0.0, 1.0);
    sunLightAttenuation = mix(1.0, sunLightAttenuation*sunLightAttenuation, dawnFactor);
    sunLightAttenuation *= 1.0-0.5*env.rainFactor;

    // direct light from top - BSL-like strong directional
    light = (NL_SUNLIGHT_INTENSITY*l*sunLightAttenuation)*sunLightTint(env.dayFactor, env.rainFactor);
    vec3 N = normalize(mul(world, normal)).xyz;
    light *= 0.85 + max(N.y, 0.0);

    // sky ambient
    lum = luminance(light);
    light += (skycol.horizon + skycol.zenith)*(l/(1.2+lum));
  }

  // torch light
  vec3 torchColor;
  if (env.underwater) {
    torchColor = NL_UNDERWATER_TORCH_COL;
  } else if (env.end) {
    torchColor = NL_END_TORCH_COL;
  } else if (env.nether) {
    torchColor = NL_NETHER_TORCH_COL;
  } else {
    torchColor = NL_OVERWORLD_TORCH_COL;
  }

  lum = luminance(light);
  light += torchColor*(smoothstep(0.1, 0.0, tileLightCol.b-tileLightCol.r)*NL_TORCHLIGHT_INTENSITY*tl/(1.0+lum));

  // game min brightness
  lum = luminance(light);
  if (!(env.nether || env.end)) {
    lum = luminance(light);
    light += vec3_splat(min(tileLightCol.r, 0.15)*(NL_MIN_LIGHTING_BOOST/(1.0+lum)));
  }

  if (env.underwater) {
    vec3 gPos = wPos + CAMERA_POS;
    float caustics = 0.2 + 0.2*sin(dot(gPos, vec3(1.8, 2.4, 2.1)) + 0.8*t);
    light += 0.8*NL_UNDERWATER_BRIGHTNESS + NL_CAUSTIC_INTENSITY*caustics*(0.1 + tl);
    // guard against NaN from normalize() of a near-black horizon color
    light *= mix(normalize(max(skycol.horizon, vec3_splat(0.0001))), vec3_splat(0.5), tileLightCol.b*0.2);
    light = max(light, vec3_splat(NL_UNDERWATER_BRIGHTNESS*0.15));
  }

  lum = luminance(light);
  light += vec3_splat(overlayCol.a*(1.5/(1.0+lum)));

  return light;
}

float nlEntityEdgeHighlight(vec4 edgemap) {
  #ifdef NL_ENTITY_EDGE_HIGHLIGHT
    vec2 len = min(abs(edgemap.xy),abs(edgemap.zw));
    len *= len;
    len *= len;
    float ambient = len.x + len.y*(1.0-len.x);
    return NL_ENTITY_BRIGHTNESS + ambient*NL_ENTITY_EDGE_HIGHLIGHT;
  #else
    return 1.0;
  #endif
}

vec4 nlEntityEdgeHighlightPreprocess(vec2 texcoord) {
  vec4 edgeMap = fract(vec4(texcoord*128.0, texcoord*256.0));
  return 2.0*step(edgeMap, vec4_splat(0.5)) - 1.0;
}

vec4 nlLavaNoise(vec3 gPos, float t) {
  float n = movingNoise2D(gPos.xz + gPos.yy, NL_LAVA_NOISE_SPEED*t, 0.9);
  n *= n;
  return vec4(mix(vec3(0.7, 0.4, 0.0)*smoothstep(-0.1, 0.5, n), vec3_splat(1.5), n*n),n);
}

#endif
