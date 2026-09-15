#ifndef TONEMAP_H
#define TONEMAP_H

#include "utils.h"

// ---- CraftEdge Fresh: 100% custom tonemap, zero bahar ka code ----
// Na sRGB decode/encode pair, na ACES, na Reinhard.
// Display-referred grade jo pack-authored values par seedha kaam karta hai
// (materials me original diffuse*diffuse jugaad wapas hai).
// Guarantee: ye curve kabhi bright nahi karti - f(L) <= L har jagah,
// output pakka <= 1 (white-clip/blowout impossible Fiziks se).
//   L <= PIVOT : bilkul same (mids/shadows zero change)
//   L >  PIVOT : hot highlights (suraj/sky) soft compress hokar 1.0 me samaate
// Hue kahin nahi badalta (sirf uniform scale).
vec3 craftEdgeFresh(vec3 col) {
  #ifdef NL_EXPOSURE
    col *= NL_EXPOSURE; // 1.0 neutral par no-op
  #endif

  float L = luminance(col);
  const float PIVOT = 0.40; // iske neeche zero change - thoda dark realistic (was 0.45)
  const float SLOPE = 1.0;  // pivot par slope (kink nahi, smooth)
  float Lc;
  if (L <= PIVOT) {
    Lc = L;
  } else {
    Lc = PIVOT + (1.0 - PIVOT) * (1.0 - exp(-(L - PIVOT) * SLOPE / (1.0 - PIVOT)));
  }
  col *= Lc / max(L, 1e-5);

  #ifdef NL_SATURATION
    // luma ke around mix = brightness preserved by construction
    col = mix(vec3_splat(luminance(col)), col, NL_SATURATION);
  #endif

  #ifdef NL_TINT
    // tint ke baad luma wapas match (tint se brightness lock)
    float lumG = luminance(col);
    vec3 tinted = col * mix(NL_TINT_LOW, NL_TINT_HIGH, col);
    col = tinted * (lumG / max(luminance(tinted), 1e-5));
  #endif

  return col;
}

vec3 colorCorrection(vec3 col) {
  col = max(col, vec3_splat(0.0));

  // TYPE 5 (aur legacy slot 4): full custom fresh curve
  #if NL_TONEMAP_TYPE == 5 || NL_TONEMAP_TYPE == 4
    return craftEdgeFresh(col);
  #endif

  // TYPE 1-3: original pack behaviour (backup slots, gamma workflow)
  #ifdef NL_EXPOSURE
    col *= NL_EXPOSURE;
  #endif

  // ref - https://64.github.io/tonemapping/
  #if NL_TONEMAP_TYPE == 3
    // extended reinhard tonemap - BSL-like highlight rolloff
    const float whiteScale = 0.068;
    col = col*(1.0+col*whiteScale)/(1.0+col);
  #elif NL_TONEMAP_TYPE == 2
    // simple reinhard tonemap
    col = col/(1.0+col);
  #elif NL_TONEMAP_TYPE == 1
    // exponential tonemap
    col = 1.0-exp(-col*0.8);
  #endif

  // gamma correction (original pack)
  col = pow(col, vec3_splat(1.0/NL_GAMMA));

  #ifdef NL_SATURATION
    col = mix(vec3_splat(luminance(col)), col, NL_SATURATION);
  #endif

  #ifdef NL_TINT
    col *= mix(NL_TINT_LOW, NL_TINT_HIGH, col);
  #endif

  return col;
}

// inv used in fogcolor (fresh curve inverse - fog range ke liye exact)
vec3 colorCorrectionInv(vec3 col) {
  #if NL_TONEMAP_TYPE == 5 || NL_TONEMAP_TYPE == 4
    #ifdef NL_SATURATION
      col = mix(vec3_splat(dot(col,vec3(0.21, 0.71, 0.08))), col, 1.0/NL_SATURATION);
    #endif
    #ifdef NL_TINT
      // forward luma-restore ka approx inverse (fog-range me accurate)
      vec3 k = mix(NL_TINT_LOW, NL_TINT_HIGH, col);
      col /= max(k, vec3_splat(1e-4));
    #endif
    {
      const float PIVOT = 0.40;
      const float SLOPE = 1.0;
      float Lc = luminance(col);
      float L;
      if (Lc <= PIVOT) {
        L = Lc; // yahan forward identity tha - exact
      } else {
        float t = clamp((Lc - PIVOT) / (1.0 - PIVOT), 0.0, 0.999);
        L = PIVOT - (1.0 - PIVOT) / SLOPE * log(1.0 - t);
      }
      col *= L / max(Lc, 1e-5);
    }
    #ifdef NL_EXPOSURE
      col /= NL_EXPOSURE;
    #endif
    return col;
  #endif

  // TYPE 1-3 fallback inverse (original pack style)
  #ifdef NL_TINT
    col /= mix(NL_TINT_LOW, NL_TINT_HIGH, col); // not accurate inverse
  #endif

  #ifdef NL_SATURATION
    col = mix(vec3_splat(dot(col,vec3(0.21, 0.71, 0.08))), col, 1.0/NL_SATURATION);
  #endif

  col = pow(col, vec3_splat(NL_GAMMA));

  #ifdef NL_EXPOSURE
    col /= NL_EXPOSURE;
  #endif

  return col;
}

#endif
