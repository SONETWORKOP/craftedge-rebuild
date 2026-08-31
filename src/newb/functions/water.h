#ifndef WATER_H
#define WATER_H

#include "utils.h"
#include "detection.h"
#include "sky.h"
#include "clouds.h"
#include "noise.h"

// fresnel - Schlick's approximation
float calculateFresnel(float cosR, float r0) {
  float a = 1.0-cosR;
  float a2 = a*a;
  return r0 + (1.0-r0)*a2*a2*a;
}

vec4 nlWater(
  inout vec4 color, inout vec3 wPos, nl_skycolor skycol, nl_environment env, vec4 COLOR, vec3 viewDir,
  vec3 cPos, vec3 tiledCpos, vec3 gPos, vec3 CAMERA_POS, vec3 light, vec3 torchColor, vec2 lit,
  float fractCposY, float camDist, highp float t
) {

  vec2 bump = vec2_splat(movingNoise2D(gPos.xz + gPos.yy, NL_WATER_WAVE_SPEED*t, 0.6));

  vec3 nrm;
  if (fractCposY > 0.0) { // top plane
    nrm.xz = bump*NL_WATER_BUMP;
    nrm.y = -1.0;
    /*if (fractCposY>0.8 || fractCposY<0.9) { // flat plane
    } else { // slanted plane and highly slanted plane
    }*/
  } else { // reflection for side plane
    bump *= 0.5 + 0.5*sin(3.0*t*NL_WATER_WAVE_SPEED + cPos.y*PI_HALF);
    float viewDirXZLengthSq = dot(viewDir.xz, viewDir.xz);
    vec2 sideDir = viewDirXZLengthSq > 0.000001 ? viewDir.xz/sqrt(viewDirXZLengthSq) : vec2(1.0,0.0);
    nrm.xz = sideDir + bump.y*(1.0-viewDir.xz*viewDir.xz)*NL_WATER_BUMP;
    nrm.y = bump.x*NL_WATER_BUMP;
  }
  nrm = normalize(nrm);

  float cosR = dot(nrm, viewDir);
  vec3 reflDir = viewDir - 2.0*cosR*nrm ; // reflect(viewDir, nrm)

  vec3 waterRefl = nlRenderSky(skycol, env, reflDir, t, false);

  #if defined(NL_CLOUD_AURORA_REFLECTION)
    if (reflDir.y < 0.0) {
      // Clouds themselves are mirrored per-pixel in the RenderChunk fragment
      // stage (waterCloudReflection), which matches the sky shapes exactly.
      // Only take the aurora layer here so two different cloud shapes don't
      // stack on the water surface.
      #ifdef NL_NO_WATER_CLOUD_REFL
        float vertexCloudAmount = 1.0;
      #else
        float vertexCloudAmount = 0.0;
      #endif
      vec4 cloudRefl = nlCloudAuroraReflection(skycol, env, reflDir, wPos, CAMERA_POS, t, vertexCloudAmount);
      waterRefl = mix(waterRefl, cloudRefl.rgb, cloudRefl.a);
    }
  #endif

  // torch light reflection
  float tc = 0.5+0.5*sin(16.0*reflDir.x)*sin(16.0*reflDir.z);
  waterRefl += torchColor*NL_TORCHLIGHT_INTENSITY*lit.x*tc*tc;

  // sharp sun specular highlight (lightweight Blinn-Phong, avoids full BRDF cost)
  #if defined(NL_SUNLIGHT_INTENSITY)
    vec3 sunDir = env.sunDir.y > 0.0 ? env.sunDir : env.moonDir;
    vec3 halfVector = sunDir + viewDir;
    float halfLengthSq = dot(halfVector, halfVector);
    vec3 halfDir = halfLengthSq > 0.000001 ? halfVector/sqrt(halfLengthSq) : nrm;
    float specAngle = max(dot(nrm, halfDir), 0.0);
    float specHighlight = pow(specAngle, 256.0)*lit.y;
    waterRefl += specHighlight*NL_SUNLIGHT_INTENSITY*sunLightTint(env.dayFactor, env.rainFactor);

    // broad sun/moon glitter path: a wide reflection along the horizon-facing
    // direction, strongest at low sun angles (sunrise/sunset). The tight
    // pow(256) dot above alone is invisible at grazing angles, this spreads it.
    float sunAlign = max(dot(reflDir, sunDir), 0.0);
    float glitter = pow(sunAlign, NL_WATER_SUN_GLITTER_SHARPNESS)*lit.y;
    waterRefl += glitter*NL_WATER_SUN_GLITTER*NL_SUNLIGHT_INTENSITY*sunLightTint(env.dayFactor, env.rainFactor);
  #endif

  // mask sky reflection under shade
  if (!env.end) {
    waterRefl *= 0.08 + lit.y*1.05;
  }

  #ifdef NL_WATER_REFL_MASK
    float mask = 0.05+0.05*sin(reflDir.x*12.0)*sin(reflDir.z*6.0);
    waterRefl *= smoothstep(mask-0.2,mask+0.13,reflDir.y*reflDir.y);
  #endif

  cosR = abs(cosR);
  // realistic water fresnel base reflectance (~0.02) instead of glass-like 0.07
  float fresnel = calculateFresnel(cosR, 0.02);
  float opacity = 1.0-cosR;

  color.rgb *= 0.22*NL_WATER_TINT*(1.0-0.8*fresnel);
  color.a = mix(COLOR.a*NL_WATER_TRANSPARENCY, 1.0, opacity*opacity);

  #ifdef NL_WATER_WAVE
    if (camDist < 14.0) {
      wPos.y -= 0.5*(bump.x+0.5)*NL_WATER_BUMP;
    }
  #endif

  return vec4(waterRefl, fresnel);
}

#endif
