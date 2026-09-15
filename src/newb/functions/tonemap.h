#ifndef TONEMAP_H
#define TONEMAP_H

#include "utils.h"

// ---- CraftEdge True: custom tonemap (ONLY tonemap in this file) ----
// Research: upstream simple-family curves + Mojang luminance philosophy +
// Khronos PBR Neutral (mids exact, slow highlight desat) + Hable filmic
// (toe/shoulder/linear + desat-to-white as feature).
// Design (display-referred, pack-authored values: sun 3.8, sky 2.1, torch 1.2):
//   toe (TOE 1.2): night/caves thode dark + rich, black crush nahi (0->0)
//   knee (0.38): iske aas-paas mids same (vanilla jaisi brightness)
//   shoulder (SLOPE 0.60, asymptote 1.0): sunset/sunrise/noon ka hot range
//     firmly dabao - jalna/blowout khatam, suraj soft white
//   colours: highlight slow desat-to-white + muted sat + teal-orange tint,
//     teeno luminance-locked (natural + cinematic, neon nahi)
// Hue kahin nahi badalta (sirf uniform scale). Output pakka <= 1.
// Fog ke liye neeche exact inverse hai.
vec3 craftEdgeTrue(vec3 col) {
  #ifdef NL_EXPOSURE
    col *= NL_EXPOSURE; // 1.0 neutral par no-op
  #endif

  float L = luminance(col);
  const float KNEE  = 0.38; // night/cave iske neeche (toe), mids iske paas
  const float TOE   = 1.2;  // night dark + rich shadows
  const float SLOPE = 0.60; // hot range firmly tame (sunset/sunrise/noon fix)
  const float A     = (1.0 - KNEE) / SLOPE;
  float Ltoe = (L <= 0.0) ? 0.0 : KNEE * pow(L / KNEE, TOE);
  float u    = L - KNEE;
  float Lsh  = KNEE + (1.0 - KNEE) * u / (A + u); // L>KNEE-A par valid (hamesha true)
  // wide knee blend: slope change smooth, sky gradient me banding nahi
  float Lc = mix(Ltoe, Lsh, smoothstep(KNEE - 0.08, KNEE + 0.08, L));
  col *= Lc / max(L, 1e-5); // uniform scale = hue-safe

  // highlights me slow desat-to-white (neon clipping khatam, natural look)
  {
    float removed = max(L - Lc, 0.0);
    float g = 1.0 - 1.0 / (0.15 * removed + 1.0);
    col = mix(col, vec3_splat(luminance(col)), g);
  }

  // display encode (restrained: upstream se darker-realistic)
  col = pow(col, vec3_splat(1.0 / 1.08));

  #ifdef NL_SATURATION
    col = mix(vec3_splat(luminance(col)), col, NL_SATURATION);
  #endif

  #ifdef NL_TINT
    float lumG = luminance(col);
    vec3 tinted = col * mix(NL_TINT_LOW, NL_TINT_HIGH, col);
    col = tinted * (lumG / max(luminance(tinted), 1e-5));
  #endif

  return col;
}

vec3 colorCorrection(vec3 col) {
  return craftEdgeTrue(max(col, vec3_splat(0.0)));
}

// inv used in fogcolor (toe+shoulder inverse - fog range ke liye exact;
// highlight-desat ka inverse skip: fog-range me g<0.01, invisible)
vec3 colorCorrectionInv(vec3 col) {
  #ifdef NL_SATURATION
    col = mix(vec3_splat(dot(col,vec3(0.21, 0.71, 0.08))), col, 1.0/NL_SATURATION);
  #endif
  #ifdef NL_TINT
    vec3 k = mix(NL_TINT_LOW, NL_TINT_HIGH, col);
    col /= max(k, vec3_splat(1e-4));
  #endif
  col = pow(col, vec3_splat(1.08));
  {
    const float KNEE  = 0.38;
    const float TOE   = 1.2;
    const float SLOPE = 0.60;
    const float A     = (1.0 - KNEE) / SLOPE;
    float Lc = luminance(col);
    // blend-zone ka error <0.01 lum (invisible); bahar exact
    float L;
    if (Lc <= KNEE) {
      L = (Lc <= 0.0) ? 0.0 : KNEE * pow(Lc / KNEE, 1.0 / TOE);
    } else {
      float t = clamp((Lc - KNEE) / (1.0 - KNEE), 0.0, 0.999);
      L = KNEE + A * t / (1.0 - t);
    }
    col *= L / max(Lc, 1e-5);
  }
  #ifdef NL_EXPOSURE
    col /= NL_EXPOSURE;
  #endif
  return col;
}

#endif
