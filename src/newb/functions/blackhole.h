#ifndef BLACKHOLE_H
#define BLACKHOLE_H

#include "utils.h"

// Author: devendrn
// Title: Simple blackhole
// License: CC BY-SA 4.0
// https://creativecommons.org/licenses/by-sa/4.0/
// Adapted for bgfx shader portability and EndSky composition.
vec4 renderBlackhole(vec3 vdir, float t) {
  t *= NL_BH_SPEED;

  float r = 2.4;
  vec3 vr = vdir;
  vr.xy = mul(rmat2(-r), vr.xy);

  vec3 vd = vr - vec3(0.0, -1.0, 0.0);
  float nl = sin(15.0*vd.x + t)*sin(15.0*vd.y - t)*sin(15.0*vd.z + t);
  float a = 0.0;
  if (dot(vd.xz, vd.xz) > 1e-8) {
    a = atan2(vd.x, vd.z);
  }
  float d = NL_BH_DIST*length(vd + vec3_splat(0.003*nl));
  float d0 = (0.6-d)/0.6;
  float dm0 = 1.0-max(d0, 0.0);
  float gl = 1.0-clamp(-0.3*d0, 0.0, 1.0);
  float gla = pow(1.0-min(abs(d0), 1.0), 8.0);
  float gl8 = pow(gl, 8.0);
  float hole = 0.9*pow(dm0, 32.0) + 0.1*pow(dm0, 3.0);
  float bh = (gla + 0.8*gl8 + 0.2*gl8*gl8)*hole;

  float curve = 1.4-d;
  float curve2 = curve*curve;
  float df = sin(3.0*a - 4.0*d + 24.0*curve2*curve2 + t);
  df *= 0.9 + 0.1*sin(8.0*a + d + 4.0*t - 4.0*df);
  float df2 = df*df;
  bh *= 1.0 + df2*df2*hole*max(1.0-bh, 0.0);

  vec3 col = bh*4.0*mix(NL_BH_COL_LOW, NL_BH_COL_HIGH, min(bh, 1.0));
  return vec4(col, hole);
}

#endif
