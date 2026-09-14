#ifndef TONEMAP_H
#define TONEMAP_H

#include "utils.h"

// ---- tone.txt linear workflow ----
// diffuse textures are sRGB-encoded; decode to linear before lighting,
// work in linear, then encode back at the end. Cleaner mids, no washed-out
// colors (replaces old diffuse*diffuse jugaad).
vec3 sRGBtoLinear(vec3 sRGB) {
  return max(mix(sRGB / 12.92, pow(0.947867*sRGB + 0.0521327, vec3_splat(2.4)), step(0.04045, sRGB)), 0.0);
}

vec3 linearToSRGB(vec3 color) {
  color = max(color, vec3_splat(0.0));
  return mix(color * 12.92, 1.055 * pow(color, vec3_splat(1.0 / 2.4)) - 0.055, step(0.0031308, color));
}

// ACES filmic (tone.txt style) - soft highlight rolloff
vec3 ACESFilm(vec3 x) {
  const float a = 1.04;
  const float b = 0.03;
  const float c = 0.93;
  const float d = 0.56;
  const float e = 0.14;
  return clamp((x*(a*x + b)) / (x*(c*x + d) + e), 0.0, 1.0);
}

// approximate inverse of ACES above (for fog color matching only)
// solves (a-yc)x^2 + (b-yd)x - y*e = 0 per channel
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

// simple highlight compressor - use on overbright things (clouds etc.)
// BEFORE tonemapping so ACES never sees clipped whites (from tone.txt)
vec3 reinhard(vec3 x) {
  return x / (1.0 + x);
}

vec3 colorCorrection(vec3 col) {
  #ifdef NL_EXPOSURE
    col *= NL_EXPOSURE;
  #endif

  // ref - https://64.github.io/tonemapping/
  #if NL_TONEMAP_TYPE == 3
    // extended reinhard tonemap - BSL-like highlight rolloff
    const float whiteScale = 0.068;
    col = col*(1.0+col*whiteScale)/(1.0+col);
  #elif NL_TONEMAP_TYPE == 4
    // aces filmic (tone.txt style) - brightness tonemap ke andar hi control
    // 0.45 = aur dark (was 0.52)
    col = ACESFilm(col*0.45);
    // highlight desat: ACES oversaturates to white, luma me mix
    // karke noon/sky detail bachao (0.55 se start, max 45% desat)
    float hl = luminance(col);
    col = mix(col, vec3_splat(hl), smoothstep(0.55, 1.0, hl)*0.45);
  #elif NL_TONEMAP_TYPE == 2
    // simple reinhard tonemap
    col = col/(1.0+col);
  #elif NL_TONEMAP_TYPE == 1
    // exponential tonemap
    col = 1.0-exp(-col*0.8);
  #endif

  // proper sRGB encode (tone.txt, replaces gamma pow)
  col = linearToSRGB(col);
  // tonemap-internal mids control: sRGB mids ko halka dabao taaki noon
  // doodh jaisa bright na lage (config values ko hath nahi lagana)
  // 1.25 = aur dark (was 1.17)
  col = pow(col, vec3_splat(1.25));

  #ifdef NL_SATURATION
    col = mix(vec3_splat(luminance(col)), col, NL_SATURATION);
  #endif

  #ifdef NL_TINT
    col *= mix(NL_TINT_LOW, NL_TINT_HIGH, col);
  #endif

  return col;
}

// inv used in fogcolor for nether (approximate - ACES is not perfectly invertible)
vec3 colorCorrectionInv(vec3 col) {
  #ifdef NL_TINT
    col /= mix(NL_TINT_LOW, NL_TINT_HIGH, col); // not accurate inverse
  #endif

  #ifdef NL_SATURATION
    col = mix(vec3_splat(dot(col,vec3(0.21, 0.71, 0.08))), col, 1.0/NL_SATURATION);
  #endif

  // inverse of post gamma + linearToSRGB above
  col = pow(col, vec3_splat(1.0/1.25));
  col = sRGBtoLinear(col);

  #if NL_TONEMAP_TYPE == 4
    // inverse highlight desat is skipped (small effect on fog mids)
    // inverse ACES with 0.45 pre-scale
    col = ACESFilmInv(col) / 0.45;
  #elif NL_TONEMAP_TYPE == 3
    float ws = 0.068;
    // inverse of x*(1+x*ws)/(1+x): solve ws*x^2 + (1-y*(1+ws))*x - y = 0 approx
    // simplified: use extended reinhard inverse
    float ws2 = 0.7966;
    col = col*(ws2 + col)/(ws2 + col*(1.0 - ws2));
  #elif NL_TONEMAP_TYPE == 2
    col = col/(max(vec3_splat(1.0)-col, vec3_splat(1e-4)));
  #elif NL_TONEMAP_TYPE == 1
    col = -log(max(vec3_splat(1.0)-col, vec3_splat(1e-4)))/0.8;
  #endif

  #ifdef NL_EXPOSURE
    col /= NL_EXPOSURE;
  #endif

  return col;
}

#endif
