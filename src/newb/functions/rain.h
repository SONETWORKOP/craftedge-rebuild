#ifndef RAIN_H
#define RAIN_H

#include "clouds.h"
#include "detection.h"
#include "noise.h"
#include "sky.h"
#include "water.h"

float nlWindblow(vec3 pos, float t){
  vec2 p = pos.xy/(1.0+pos.z);
  float val = sin(4.0*p.x + 2.0*p.y + 2.0*t + 3.0*p.y*p.x)*sin(p.y*2.0 + 0.2*t);
  val += sin(p.y - p.x + 0.2*t);
  return 0.25*val*val;
}

vec4 nlRefl(
  inout vec4 color, nl_skycolor skycol, nl_environment env, vec3 viewDir, vec3 wPos, vec3 tiledCpos,
  vec3 CAMERA_POS, vec3 torchColor, vec2 lit, float camDist, float renderDist, highp float t
) {
  vec4 wetRefl = vec4(0.0,0.0,0.0,0.0);

  #ifndef NL_NO_GROUND_REFL

  #ifndef NL_GROUND_REFL
  if (env.rainFactor > 0.0) {
  #endif

    float wetness = lit.y*lit.y;

    // clip reflection when far (better performance)
    float endDist = renderDist*0.6;
    if (camDist < endDist) {
      float cosR = max(viewDir.y, 0.0);
      float puddles = max(1.0 - NL_GROUND_RAIN_PUDDLES*fastRand(tiledCpos.xz), 0.0);

      #ifndef NL_GROUND_REFL
        wetness *= puddles;
        float reflective = wetness*env.rainFactor*NL_GROUND_RAIN_WETNESS;
      #else
        float reflective = NL_GROUND_REFL;
        if (!env.end && !env.nether) {
          // only multiply with wetness in overworld
          reflective *= wetness;

          // Keep clear-weather block reflections subtle. Rain deliberately
          // keeps the original full-strength path below.
          if (env.rainFactor <= 0.001) {
            reflective *= 0.62;
          }
        }

        // suppress reflection on green vegetation (grass, leaves, crops).
        // metallic/smooth blocks (iron, diamond, gold, quartz) have grey/cyan/
        // white tint so greenness ~0 and keep full reflection - this stops the
        // white-washed look on grass that the previous always-on reflection had.
        float greenness = color.g - max(color.r, color.b);
        greenness = clamp(greenness, 0.0, 0.5);
        reflective *= 1.0 - 0.85*smoothstep(0.05, 0.35, greenness);

        wetness *= puddles;
        reflective = mix(reflective, wetness, env.rainFactor);

        // --- rain reflection boost ---
        // while raining the ground gets a wet film: puddles turn into strong
        // mirrors. Only kicks in with rainFactor so clear weather is unchanged.
        #ifdef NL_RAIN_REFL_STRENGTH
          if (!env.end && !env.nether) {
            float wetFilm = env.rainFactor*NL_GROUND_RAIN_WETNESS*wetness;
            reflective += NL_RAIN_REFL_STRENGTH*wetFilm;
          }
        #endif
      #endif

      reflective = min(reflective, 1.0);

      if (wPos.y < 0.0) {
        vec3 reflDir = viewDir;
        reflDir.y = -reflDir.y;
        wetRefl.rgb = nlRenderSky(skycol, env, reflDir, t, false);

        // RTX-style mirror reflection (ref: block reflection V3) - reflect
        // sky + clouds clearly. Cloud contribution is strong but balanced so
        // the reflection reads like a clean mirror, not a white wash.
        vec4 cloudRefl = nlCloudAuroraReflection(skycol, env, reflDir, wPos, CAMERA_POS, t, 1.0);
        wetRefl.rgb = mix(wetRefl.rgb, cloudRefl.rgb, cloudRefl.a*0.6);

        // torch light
        wetRefl.rgb += torchColor*lit.x*NL_TORCHLIGHT_INTENSITY;

        // sharp sun specular glint - GGX-style tight highlight for a
        // metallic RTX look on iron, gold, diamond, quartz, etc.
        #if defined(NL_SUNLIGHT_INTENSITY)
          vec3 sunDir = env.sunDir.y > 0.0 ? env.sunDir : env.moonDir;
          vec3 halfVector = sunDir + viewDir;
          float halfLengthSq = dot(halfVector, halfVector);
          vec3 halfDir = halfLengthSq > 0.000001 ? halfVector/sqrt(halfLengthSq) : vec3(0.0,1.0,0.0);
          float specAngle = max(dot(vec3(0.0,1.0,0.0), halfDir), 0.0);
          float specHighlight = pow(specAngle, 512.0)*lit.y;
          wetRefl.rgb += specHighlight*NL_SUNLIGHT_INTENSITY*sunLightTint(env.dayFactor, env.rainFactor)*reflective;
        #endif

        // strong mirror fresnel (r0 higher = more mirror) for RTX-like
        // reflective blocks - reads as a clear mirror on smooth surfaces.
        // Wet ground is smoother, so raise base reflectance while raining.
        float r0 = 0.09;
        #ifdef NL_RAIN_REFL_STRENGTH
          r0 = mix(r0, 0.18, env.rainFactor);
        #endif
        wetRefl.a = calculateFresnel(cosR, r0)*reflective;
        // smoother falloff before the clip distance instead of a linear fade
        float clipFade = clamp(1.0 - camDist/endDist, 0.0, 1.0);
        wetRefl.a *= clipFade*clipFade*(3.0-2.0*clipFade);
        // alpha lift so the reflection reads as a strong mirror
        wetRefl.a = min(wetRefl.a*1.35, 1.0);
      }
    }

    // darken wet parts
    color.rgb *= 1.0 - 0.4*wetness*env.rainFactor;

  #ifndef NL_GROUND_REFL
  }
  #endif

  #endif // NL_NO_GROUND_REFL

  return wetRefl;
}

#endif
