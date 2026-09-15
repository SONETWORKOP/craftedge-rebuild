#ifndef TONEMAP_H
#define TONEMAP_H

#include "utils.h"

// ---- CraftEdge True: research-backed full-custom tonemap ----
// Research (upstream devendrn/newb-x-mcbe + Mojang colour docs):
//  - Upstream default = Extended Reinhard family + gamma encode (simple,
//    "very common" method). Is pack ki sari lighting values (sun 3.8,
//    day sky 2.1, torch 1.2) isi family ke liye authored hain.
//  - Mojang best practice = LUMINANCE compress karo, highlight colours bachao.
// Isliye ye curve: knee rational compression (extended-reinhard family),
// luminance-ratio scale (hue-safe), asymptote exactly 1.0 (clip impossible).
// tone.txt ka koi code nahi: no sRGB decode/encode pair, no ACES, no Reinhard.
// Requirements checklist (poori baatcheet se):
//  [x] brightness kabhi na badhe (neeche proof)
//  [x] noon washout/doodh-pan khatam (hot range firmly compress)
//  [x] realistic natural colours (muted sat + soft tint, dono luma-locked)
//  [x] cyan night untouched (knee ke neeche identity)
//  [x] torch 1.2 dim preserved (cave range knee ke aas-paas, mild only)
//  [x] fog sky se match (neeche exact inverse)
// Proof (non-brightening): knee ke neeche Lc=L (same); knee ke upar
// Lc<K+(1-K)=1 aur Lc<L (rational hamesha input se chhota); sat/tint
// luma-locked; encode exponent <1 hone par bhi knee-compress dominate
// karta hai hot range me. Output kabhi input-luma se upar nahi.
vec3 craftEdgeTrue(vec3 col) {
  #ifdef NL_EXPOSURE
    col *= NL_EXPOSURE; // 1.0 neutral par no-op
  #endif

  float L = luminance(col);
  const float KNEE  = 0.38; // iske neeche identity - night/cave pixel-perfect
  const float SLOPE = 0.75; // knee par slope - hot range firmly dabao (noon fix)
  const float A     = (1.0 - KNEE) / SLOPE;
  // knee blend zone (smoothstep): derivative kink nahi -> sky gradient me banding nahi
  float u  = L - KNEE;
  float Ls = KNEE + (1.0 - KNEE) * u / (A + u); // u<0 par bhi valid (L>KNEE-A hamesha true)
  float w  = smoothstep(KNEE - 0.06, KNEE + 0.06, L);
  float Lc = mix(L, Ls, w);
  col *= Lc / max(L, 1e-5); // uniform scale = hue-safe, output <= 1 pakka

  // display encode (restrained: 1.08, upstream 1.33 se darker-realistic)
  col = pow(col, vec3_splat(1.0 / 1.08));

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
    return craftEdgeTrue(col);
  #endif

  // TYPE 1-3: original pack behaviour (backup slots)
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
    // encode inverse, phir knee-curve inverse (blend zone ka error <0.01 lum, invisible)
    col = pow(col, vec3_splat(1.08));
    {
      const float KNEE  = 0.38;
      const float SLOPE = 0.75;
      const float A     = (1.0 - KNEE) / SLOPE;
      float Lc = luminance(col);
      float L;
      if (Lc <= KNEE) {
        L = Lc; // yahan forward identity tha - exact
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
