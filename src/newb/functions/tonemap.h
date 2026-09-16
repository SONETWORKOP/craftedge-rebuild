#ifndef TONEMAP_H
#define TONEMAP_H

#include "utils.h"

// ---- tone.txt (Download/tone.txt) 4-knob workflow, pack-calibrated ----
// Knobs neeche hain. Values isliye chune taaki look BILKUL SAME rahe:
// purana diffuse*diffuse(0.25) vs sRGB-decode(0.214) ka farak GAMMA 1.45
// absorb kar leta hai (mids 0.566->0.564, highlights 0.878->0.881, ~same).
// TONE_EXPOSURE = NL_EXPOSURE (upar apply hota hai, yahan dobara nahi).
#define TONE_ACES_SCALE  0.55  // aur dark (was 0.65)
#define TONE_GAMMA       1.60  // mids dark (was 1.45)
#define TONE_SHADOW_LIFT 0.0   // andhera floor (0.0 off)

// ---- vibrance strength (saturation ka samajhdaar bhai) ----
// pheeke rang uthao, jalte-neon chhodo. 0.0 off ~ 0.5 tez.
#define CE_VIBRANCE 0.25

vec3 sRGBtoLinear(vec3 sRGB) {
  return max(mix(sRGB / 12.92, pow(0.947867*sRGB + 0.0521327, vec3_splat(2.4)), step(0.04045, sRGB)), 0.0);
}

vec3 linearToSRGB(vec3 color) {
  color = max(color, vec3_splat(0.0));
  return mix(color * 12.92, 1.055 * pow(color, vec3_splat(1.0 / 2.4)) - 0.055, step(0.0031308, color));
}

// ACES filmic fit (pack ke original constants)
vec3 ACESFilm(vec3 x) {
  const float a = 1.04;
  const float b = 0.03;
  const float c = 0.93;
  const float d = 0.56;
  const float e = 0.14;
  return clamp((x*(a*x + b)) / (x*(c*x + d) + e), 0.0, 1.0);
}

// ACES ka ulta (fog match ke liye): (a-yc)x^2+(b-yd)x-ye=0 hal karo
vec3 ACESFilmInv(vec3 y) {
  const float a = 1.04;
  const float b = 0.03;
  const float c = 0.93;
  const float d = 0.56;
  const float e = 0.14;
  vec3 A = vec3_splat(a) - y*vec3_splat(c);
  vec3 B = vec3_splat(b) - y*vec3_splat(d);
  vec3 C = y*vec3_splat(e);
  vec3 disc = max(B*B + 4.0*A*C, vec3_splat(0.0));
  vec3 x = (-B + sqrt(disc)) / max(2.0*A, vec3_splat(1e-5));
  return max(x, vec3_splat(0.0));
}

vec3 colorCorrection(vec3 col) {
  #ifdef NL_EXPOSURE
    col *= NL_EXPOSURE;
  #endif

  #if NL_TONEMAP_TYPE == 4
    // tone.txt path: apna encode/gamma (neeche tail wala gamma skip hota hai)
    col = ACESFilm(col * TONE_ACES_SCALE);
    // natural colours: highlights ka neon nikaldo (luma-safe, brightness same)
    float hd = luminance(col);
    col = mix(col, vec3_splat(hd), smoothstep(0.55, 1.0, hd) * 0.4);
    col = linearToSRGB(col);
    col = pow(col, vec3_splat(TONE_GAMMA));
    col += TONE_SHADOW_LIFT * (vec3_splat(1.0) - col);
  #else
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

  // gamma correction
  col = pow(col, vec3_splat(1.0/NL_GAMMA));
  #endif

  #ifdef NL_SATURATION
    col = mix(vec3_splat(luminance(col)), col, NL_SATURATION);
  #endif

  #ifdef NL_TINT
    col *= mix(NL_TINT_LOW, NL_TINT_HIGH, col);
  #endif

  // vibrance: pheeke rang uthao, jalte-neon chhodo.
  // Luma ke around mix = brightness lock. satAmt zyda = boost kam.
  // (max/min 2-args nested - shader me 3-args nahi chalta)
  {
    float lumV = luminance(col);
    float satAmt = max(col.r, max(col.g, col.b)) - min(col.r, min(col.g, col.b));
    satAmt = max(satAmt, 0.0);
    float vib = CE_VIBRANCE * (1.0 - clamp(satAmt * 1.5, 0.0, 1.0));
    col = mix(vec3_splat(lumV), col, 1.0 + vib);
  }

  return col;
}

// inv used in fogcolor for nether
vec3 colorCorrectionInv(vec3 col) {
  // vibrance inverse (approx, fog-range me accurate - vib chhota hai)
  {
    float lumV = luminance(col);
    float satAmt = max(col.r, max(col.g, col.b)) - min(col.r, min(col.g, col.b));
    satAmt = max(satAmt, 0.0);
    float vib = CE_VIBRANCE * (1.0 - clamp(satAmt * 1.5, 0.0, 1.0));
    col = mix(vec3_splat(lumV), col, 1.0 / (1.0 + vib));
  }
  #if NL_TONEMAP_TYPE == 4
    // tone.txt path ka ulta (order reverse): sat -> tint -> shadow -> gamma
    // -> encode -> desat(skip, fog-range me ~0) -> ACES -> exposure
    #ifdef NL_TINT
      col /= mix(NL_TINT_LOW, NL_TINT_HIGH, col); // not accurate inverse
    #endif
    #ifdef NL_SATURATION
      col = mix(vec3_splat(dot(col,vec3(0.21, 0.71, 0.08))), col, 1.0/NL_SATURATION);
    #endif
    col = (col - TONE_SHADOW_LIFT * vec3_splat(1.0)) / max(vec3_splat(1.0) - vec3_splat(TONE_SHADOW_LIFT), vec3_splat(1e-4));
    col = pow(col, vec3_splat(1.0 / TONE_GAMMA));
    col = sRGBtoLinear(col);
    col = ACESFilmInv(col) / TONE_ACES_SCALE;
    #ifdef NL_EXPOSURE
      col /= NL_EXPOSURE;
    #endif
    return col;
  #endif
  #ifdef NL_TINT
    col /= mix(NL_TINT_LOW, NL_TINT_HIGH, col); // not accurate inverse
  #endif

  #ifdef NL_SATURATION
    col = mix(vec3_splat(dot(col,vec3(0.21, 0.71, 0.08))), col, 1.0/NL_SATURATION);
  #endif

  // incomplete
  // extended reinhard only
  float ws = 0.7966;
  col = pow(col, vec3_splat(NL_GAMMA));
  col = col*(ws + col)/(ws + col*(1.0 - ws));

  #ifdef NL_EXPOSURE
    col /= NL_EXPOSURE;
  #endif

  return col;
}

#endif
