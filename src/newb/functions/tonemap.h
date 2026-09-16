#ifndef TONEMAP_H
#define TONEMAP_H

#include "utils.h"

// ---- CraftEdge Vivid Light: vivid lighting, over-bright nahi ----
// Poori file fresh hai (purana sab delete). Display-referred grade:
//   airy toe: deep shadows halke lift (vibrant-visuals jaisi hawa),
//     zyada se zyada +0.03, raat gehri hi rehti hai
//   mids: punchy, identity ke paas (colours exact, vivid)
//   shoulder: hot highlights (suraj/sky/noon) firmly 1.0 me samaao,
//     white-clip/blowout impossible
//   colours: highlight slow desat-to-white + muted sat + teal-orange,
//     sab luminance-locked (neon nahi, natural + cinematic)
// Hue kahin nahi badalta (sirf uniform scale). Output pakka <= ~1.0.
// Fog ke liye neeche exact inverse hai.
vec3 craftEdgeVividLight(vec3 col) {
  #ifdef NL_EXPOSURE
    col *= NL_EXPOSURE; // 1.0 neutral par no-op
  #endif

  float L = luminance(col);
  // airy toe + firm shoulder, knee blend se judaa (banding nahi)
  const float KNEE = 0.5;
  const float AIRY = 0.5;  // shadow lift strength (max +0.03)
  const float SLOPE = 0.8; // hot range firmly tame
  const float A = (1.0 - KNEE) / SLOPE;
  float Ltoe = L + AIRY * L * (KNEE - L); // L=0 -> 0, L=KNEE -> KNEE
  float u    = L - KNEE;
  float Lsh  = KNEE + (1.0 - KNEE) * u / (A + u); // L>KNEE-A par valid
  float Lc = mix(Ltoe, Lsh, smoothstep(KNEE - 0.06, KNEE + 0.06, L));
  col *= Lc / max(L, 1e-5); // uniform scale = hue-safe

  // highlights me slow desat-to-white (neon clipping khatam, natural look)
  {
    float removed = max(L - Lc, 0.0);
    float g = 1.0 - 1.0 / (0.15 * removed + 1.0);
    col = mix(col, vec3_splat(luminance(col)), g);
  }

  // display encode (restrained: darker-realistic)
  col = pow(col, vec3_splat(1.0 / 1.08));

  #ifdef NL_SATURATION
    col = mix(vec3_splat(luminance(col)), col, NL_SATURATION);
  #endif

  #ifdef NL_TINT
    float lumG = luminance(col);
    vec3 tinted = col * mix(NL_TINT_LOW, NL_TINT_HIGH, col);
    col = tinted * (lumG / max(luminance(tinted), 1e-5));
  #endif

  // BSL-style warm grade (research): BSL ki pehchan saturation nahi,
  // golden-warm sunlight hai. Soft cool shadows + golden highlights.
  // Luma ke hisaab se smooth blend + luma lock = brightness same, sirf mood.
  {
    float cl = luminance(col);
    vec3 warm = col * mix(vec3(0.97,0.985,1.03), vec3(1.05,1.0,0.93), smoothstep(0.0, 1.0, cl));
    col = warm * (cl / max(luminance(warm), 1e-5));
  }

  return col;
}

vec3 colorCorrection(vec3 col) {
  return craftEdgeVividLight(max(col, vec3_splat(0.0)));
}

// inv used in fogcolor (toe+shoulder inverse - fog range ke liye exact;
// highlight-desat ka inverse skip: fog-range me g<0.01, invisible)
vec3 colorCorrectionInv(vec3 col) {
  // warm grade inverse (approx, fog-range me accurate)
  {
    float cl = luminance(col);
    vec3 ck = mix(vec3(0.97,0.985,1.03), vec3(1.05,1.0,0.93), smoothstep(0.0, 1.0, cl));
    col /= max(ck, vec3_splat(1e-4));
  }
  #ifdef NL_TINT
    vec3 k = mix(NL_TINT_LOW, NL_TINT_HIGH, col);
    col /= max(k, vec3_splat(1e-4));
  #endif
  #ifdef NL_SATURATION
    col = mix(vec3_splat(dot(col,vec3(0.21, 0.71, 0.08))), col, 1.0/NL_SATURATION);
  #endif
  col = pow(col, vec3_splat(1.08));
  {
    const float KNEE = 0.5;
    const float AIRY = 0.5;
    const float SLOPE = 0.8;
    const float A = (1.0 - KNEE) / SLOPE;
    float Lc = luminance(col);
    float L;
    if (Lc <= KNEE) {
      // toe inverse: AIRY*L^2 - (1+AIRY*KNEE)*L + Lc = 0 (exact quadratic)
      float b = 1.0 + AIRY * KNEE;
      float disc = max(b * b - 4.0 * AIRY * Lc, 0.0);
      L = (b - sqrt(disc)) / (2.0 * AIRY);
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
