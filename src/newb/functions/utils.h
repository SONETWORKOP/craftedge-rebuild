#ifndef UTILS_H
#define UTILS_H

#define PI 3.141592
#define PI_HALF 1.570796
#define PI_QUART 0.785398

mat2 rmat2(float t) {
  float sint = sin(t);
  float cost = cos(t);
  return mtxFromRows(vec2(cost, -sint), vec2(sint, cost));
}

float degToRad(float t) { return 0.0174533*t; }

float luminance(vec3 x) { return dot(x, vec3(0.21, 0.71, 0.08)); }

// terrain atlas grid (tiles): texture se naapo, hardcode nahi.
// 1024x512 par (64,32), atlas badle to khud dhal jaata hai (1.26.50-proof).
// Tile hamesha 16px hota hai, isliye /16.
vec2 nlAtlasGrid(sampler2D tex) { return vec2(textureSize(tex, 0)) / 16.0; }

#endif
