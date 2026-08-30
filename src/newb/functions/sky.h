#ifndef SKY_H
#define SKY_H

#include "detection.h"
#include "noise.h"

struct nl_skycolor {
  vec3 zenith;
  vec3 horizon;
  vec3 horizonEdge;
};

// rainbow spectrum
vec3 spectrum(float x) {
  vec3 s = vec3(x-0.5, x, x+0.5);
  s = smoothstep(1.0,0.0,abs(s));
  return s*s;
}

vec3 getUnderwaterCol(vec3 FOG_COLOR) {
  return 2.0*NL_UNDERWATER_TINT*FOG_COLOR*FOG_COLOR;
}

vec3 getEndZenithCol() {
  return NL_END_ZENITH_COL;
}

vec3 getEndHorizonCol() {
  return NL_END_HORIZON_COL;
}

nl_skycolor nlEndSkyColors(nl_environment env) {
  nl_skycolor s;
  s.zenith = getEndZenithCol();
  s.horizon = getEndHorizonCol();
  s.horizonEdge = s.horizon;
  return s;
}

nl_skycolor nlOverworldSkyColors(nl_environment env) {
  nl_skycolor s;
  float f = 1.0 + 2.0*(1.0-max(-env.dayFactor, 0.0));
  float nightFactor = step(env.dayFactor, 0.0);
  s.zenith = mix(NL_DAY_ZENITH_COL, NL_NIGHT_ZENITH_COL*f, nightFactor);
  s.horizon = mix(NL_DAY_HORIZON_COL, NL_NIGHT_HORIZON_COL*f, nightFactor);
  s.horizonEdge = mix(NL_DAY_EDGE_COL, NL_NIGHT_EDGE_COL*f, nightFactor);

  float dawnFactor = 1.0-env.dayFactor*env.dayFactor;
  dawnFactor *= dawnFactor*dawnFactor;
  dawnFactor *= mix(1.0, dawnFactor*dawnFactor, nightFactor);
  s.zenith = mix(s.zenith, NL_DAWN_ZENITH_COL, dawnFactor);
  s.horizon = mix(s.horizon, NL_DAWN_HORIZON_COL, dawnFactor);
  s.horizonEdge = mix(s.horizonEdge, NL_DAWN_EDGE_COL, dawnFactor);

  float zh = dot(s.zenith, vec3_splat(0.33));
  float hh = dot(s.horizon, vec3_splat(0.33));
  float rainMix = env.rainFactor*NL_SKY_RAIN_MIX_FACTOR;
  s.zenith = mix(s.zenith, NL_RAIN_ZENITH_COL*zh, rainMix);
  s.horizon = mix(s.horizon, NL_RAIN_HORIZON_COL*hh, rainMix);
  s.horizonEdge = mix(s.horizonEdge, s.horizon, env.rainFactor);

  if (env.underwater) {
    vec3 underwaterFog = env.fogCol*env.fogCol*NL_UNDERWATER_TINT;
    s.zenith = mix(2.0*underwaterFog, underwaterFog*zh, 0.8);
    s.horizon = mix(2.0*underwaterFog, underwaterFog*hh, 0.8);
    s.horizonEdge = s.horizon;
  }

  return s;
}

nl_skycolor nlSkyColors(nl_environment env) {
  if (env.end) {
    return nlEndSkyColors(env);
  }
  return nlOverworldSkyColors(env);
}


vec3 renderOverworldSky(nl_skycolor skyCol, nl_environment env, vec3 viewDir, bool isSkyPlane) {
  float avy = abs(viewDir.y);
  float mask = 0.5 + (0.5*viewDir.y/(0.4 + avy));

  vec2 g = clamp(0.5 - 0.5*vec2(dot(env.sunDir, viewDir), dot(env.moonDir, viewDir)), 0.0, 1.0);
  vec2 g1 = 1.0-mix(sqrt(g), g, env.rainFactor);
  vec2 g2 = g1*g1;
  vec2 g4 = g2*g2;
  vec2 g8 = g4*g4;
  float mg8 = (g8.x+g8.y)*mask*(1.0-0.9*env.rainFactor);

  // realistic smooth sky gradient
  float vh = 1.0 - viewDir.y*viewDir.y;
  float vh2 = vh*vh;
  vh2 = mix(vh2, mix(1.0, vh2*vh2, NL_SKY_VOID_FACTOR), step(viewDir.y, 0.0));
  vh2 = mix(vh2, 1.0, mg8);
  float vh4 = vh2*vh2;

  // smoother atmospheric gradient
  float gradient1 = vh4*vh4;
  float gradient2 = 0.88*gradient1 + 0.12*vh2;
  gradient1 *= gradient1;
  gradient1 = mix(gradient1*gradient1, 1.0, mg8);
  gradient2 = mix(gradient2, 1.0, mg8);

  float dawnFactor = 1.0-env.dayFactor*env.dayFactor;
  float df = mix(1.0, g2.x, dawnFactor*dawnFactor);

  // 3-layer realistic sky: zenith -> horizonEdge -> horizon
  vec3 sky = mix(skyCol.horizon, skyCol.horizonEdge, gradient1*df*df);
  sky = mix(skyCol.zenith, sky, gradient2*df);

  // atmospheric brightness curve
  sky *= 0.42+0.58*gradient2;
  // near-sun bloom. day keeps the strong 9.0 quadratic term; dawn/dusk tames
  // it to 4.5 so the rich orange horizon survives ACES instead of clipping to white
  float mg8Bloom = mix(9.0, 4.5, dawnFactor);
  sky *= (1.0 + (2.8*mg8 + mg8Bloom*mg8*mg8)*mask)*mix(1.0, mask, NL_SKY_VOID_DARKNESS);

  // sun/moon glow disc
  if (!isSkyPlane) {
    float source = max(0.0, (mg8-0.22)/0.78);
    source *= source;
    source *= source;
    sky *= 1.0 + 20.0*source*(1.0-env.rainFactor);
  }

  float nightFactor = step(env.dayFactor, 0.0);
  float dayBrightness = max(env.dayFactor, 0.0);

  // SUN - realistic yellow sun with warm glow
  float sunAngle = max(dot(env.sunDir, viewDir), 0.0);
  // sun disc core
  float sunDisc = pow(sunAngle, 200.0*NL_SUN_SIZE);
  // sun outer glow
  float sunGlow = pow(sunAngle, 8.0);
  // sun atmospheric scatter - yellow rays like sunset during day
  float sunScatter = pow(sunAngle, 3.0);
  // warm halo around sun
  float sunHalo = pow(sunAngle, 12.0);

  // dawn warming: at sunrise/sunset the sun's own light travels through more
  // atmosphere, so the disc/glow/halo shift from white-yellow toward gold-orange
  float sunWarm = 0.55*dawnFactor;

  // yellow-white sun core -> warm gold at dawn (drop green & blue, keep red)
  vec3 sunCore = vec3(1.5, 1.4 - 0.55*sunWarm, 1.0 - 0.72*sunWarm) * sunDisc;
  // warm yellow glow -> deeper orange at dawn
  vec3 sunGlowCol = mix(vec3(1.4, 1.0, 0.3), vec3(1.55, 0.80, 0.18), sunWarm) * sunGlow * 0.4;
  // atmospheric yellow scatter - strong during day
  vec3 sunScatterCol = vec3(1.2, 0.85, 0.2) * sunScatter * 0.15 * dayBrightness;
  // warm halo -> golden-orange at dawn
  vec3 sunHaloCol = mix(vec3(1.3, 0.9, 0.35), vec3(1.5, 0.72, 0.24), sunWarm) * sunHalo * 0.2;

  // sunrise/sunset - extra orange scatter. use dawnFactor^2 so this warm halo
  // stays tied to true twilight and doesn't leak orange onto the daytime sun
  vec3 dawnScatter = vec3(2.6, 0.72, 0.09) * sunScatter * 0.3 * dawnFactor*dawnFactor;

  vec3 sunLight = (sunCore + sunGlowCol + sunScatterCol + sunHaloCol + dawnScatter);
  // fade out sun contribution below horizon (smooth, avoids hard pop at sunset)
  sunLight *= (1.0 - env.rainFactor) * smoothstep(-0.12, 0.06, env.dayFactor);
  sky += sunLight;

  // MOON - realistic pale moon glow
  float moonAngle = max(dot(env.moonDir, viewDir), 0.0);
  float moonDisc = pow(moonAngle, 250.0*NL_MOON_SIZE);
  float moonGlow = pow(moonAngle, 15.0);
  // wide atmospheric halo - real moonlight scatters far across the sky
  float moonHalo = pow(moonAngle, 4.0);

  vec3 moonLight = vec3(0.35, 0.4, 0.55) * moonDisc * 1.5;
  moonLight += vec3(0.15, 0.2, 0.35) * moonGlow * 0.3;
  moonLight += vec3(0.10, 0.14, 0.26) * moonHalo * 0.12;
  moonLight *= (1.0 - env.rainFactor);
  sky += moonLight;

  #ifdef NL_RAINBOW
    float rainbowFade = 0.5 + 0.5*viewDir.y;
    rainbowFade *= rainbowFade;
    rainbowFade *= mix(NL_RAINBOW_CLEAR, NL_RAINBOW_RAIN, env.rainFactor);
    rainbowFade *= 0.5+0.5*env.dayFactor;
    sky += spectrum(24.2*(0.85-g.x))*rainbowFade*skyCol.horizon;
  #endif

  return sky;
}

vec3 renderEndSky(vec3 horizonCol, vec3 zenithCol, vec3 viewDir, float t) {
  t *= 0.1;
  float a = atan2(viewDir.x, viewDir.z);

  float n1 = 0.5 + 0.5*sin(3.0*a + t + 10.0*viewDir.x*viewDir.y);
  float n2 = 0.5 + 0.5*sin(5.0*a + 0.5*t + 5.0*n1 + 0.1*sin(40.0*a -4.0*t));

  float waves = 0.7*n2*n1 + 0.3*n1;

  float grad = 0.5 + 0.5*viewDir.y;
  float streaks = waves*(1.0 - grad*grad*grad);
  streaks += (1.0-streaks)*smoothstep(1.0-waves, -1.0, viewDir.y);

  float f = 0.3*streaks + 0.7*smoothstep(1.0, -0.5, viewDir.y);
  float h = streaks*streaks;
  float g = h*h;
  g *= g;

  vec3 sky = mix(zenithCol, horizonCol, f*f);
  sky += (0.1*streaks + 2.0*g*g*g + h*h*h)*vec3(2.0,0.5,0.0);
  sky += 0.25*streaks*spectrum(sin(2.0*viewDir.x*viewDir.y+t));

  return sky;
}

vec3 nlRenderSky(nl_skycolor skycol, nl_environment env, vec3 viewDir, float t, bool isSkyPlane) {
  vec3 sky;
  viewDir.y = -viewDir.y;

  if (env.end) {
    sky = renderEndSky(skycol.horizon, skycol.zenith, viewDir, t);
  } else {
    sky = renderOverworldSky(skycol, env, viewDir, isSkyPlane);
    #ifdef NL_UNDERWATER_STREAKS
      // if (env.underwater) {
      //   float a = atan2(viewDir.x, viewDir.z);
      //   float grad = 0.5 + 0.5*viewDir.y;
      //   grad *= grad;
      //   float spread = (0.5 + 0.5*sin(3.0*a + 0.2*t + 2.0*sin(5.0*a - 0.4*t)));
      //   spread *= (0.5 + 0.5*sin(3.0*a - sin(0.5*t)))*grad;
      //   spread += (1.0-spread)*grad;
      //   float streaks = spread*spread;
      //   streaks *= streaks;
      //   streaks = (spread + 3.0*grad*grad + 4.0*streaks*streaks);
      //   sky += 2.0*streaks*skycol.horizon;
      // }
    #endif
  }

  return sky;
}

// shooting star
// draws a single streak; seed shifts randomization so each has its own
// direction/size/position while sharing the same fade-in/out timing (t0,t1)
float nlShootingStarStreak(vec3 viewDir, float t, float t0, float t1, float h0, float seed) {
  // randomize size, rotation, add motion, add skew
  float r = fract(sin(h0 + seed) * 43758.545313);
  float a = 6.2831*r;
  float cosa = cos(a);
  float sina = sin(a);
  vec2 uv = viewDir.xz * (6.0 + 4.0*r);
  uv = vec2(cosa*uv.x + sina*uv.y, -sina*uv.x + cosa*uv.y);
  uv.x += t1 - t;
  // spread the three streaks apart so they don't overlap
  uv.x -= 2.0*r + 3.5 + 2.0*seed;
  uv.y += viewDir.y * 3.0 + 1.6*(fract(sin(h0*1.7 + seed*3.1)*13758.31) - 0.5);

  // draw star
  float g = 1.0-min(abs((uv.x-0.95))*20.0, 1.0); // source glow
  float s = 1.0-min(abs(8.0*uv.y), 1.0); // line
  s *= s*s*smoothstep(-1.0+1.96*t1, 0.98-t, uv.x); // decay tail
  s *= s*s*smoothstep(1.0, 0.98-t0, uv.x); // decay source
  s *= 1.0-t1; // fade in
  s *= 1.0-t0; // fade out
  s *= 0.7 + 16.0*g*g;
  return s;
}

vec3 nlRenderShootingStar(vec3 viewDir, vec3 FOG_COLOR, float t) {
  // transition vars
  float h = t / (NL_SHOOTING_STAR_DELAY + NL_SHOOTING_STAR_PERIOD);
  float h0 = floor(h);
  t = (NL_SHOOTING_STAR_DELAY + NL_SHOOTING_STAR_PERIOD) * (h-h0);
  t = min(t/NL_SHOOTING_STAR_PERIOD, 1.0);
  float t0 = t*t;
  float t1 = 1.0-t0;
  t1 *= t1; t1 *= t1; t1 *= t1;

  // four simultaneous streaks from different directions
  float s = nlShootingStarStreak(viewDir, t, t0, t1, h0, 0.0);
  s += nlShootingStarStreak(viewDir, t, t0, t1, h0, 1.0);
  s += nlShootingStarStreak(viewDir, t, t0, t1, h0, 2.0);
  s += nlShootingStarStreak(viewDir, t, t0, t1, h0, 3.0);

  s *= max(1.0-FOG_COLOR.r-FOG_COLOR.g-FOG_COLOR.b, 0.0); // fade out during day
  return s*vec3(0.8, 0.9, 1.0);
}

// Galaxy stars - needs further optimization
vec3 nlRenderGalaxy(vec3 vdir, vec3 fogColor, nl_environment env, float t) {
  if (env.underwater) {
    return vec3_splat(0.0);
  }

  t *= NL_GALAXY_SPEED;

  // keep unrotated view dir for horizon extinction and moon proximity
  vec3 vdirOrig = vdir;

  // rotate space
  float cosb = sin(0.2*t);
  float sinb = cos(0.2*t);
  vdir.xy = mul(mat2(cosb, sinb, -sinb, cosb), vdir.xy);

  // noise
  float n1 = noise3D(15.0*vdir + sin(0.85*t + 1.3));
  float n2 = noise3D(50.0*vdir + 1.0*n1 + sin(0.7*t + 1.0));
  float n3 = noise3D(200.0*vdir - 10.0*sin(0.4*t + 0.500));

  // stars
  n3 = smoothstep(0.04,0.3,n3+0.02*n2);
  // no gd band term => stars spread uniformly, no milky-way strip
  float st = n1*n2*n3*n3;
  st = (1.0-st)/(1.0+400.0*st);
  vec3 stars = (0.65 + 0.35*sin(vec3(8.0,6.0,10.0)*(2.0*n1+0.8*n2) + vec3(0.0,0.4,0.82)))*st;

  // faint smooth green tinge (low-freq noise, not patchy/dotty)
  stars += (0.012*n1*n1)*vec3(0.12, 0.40, 0.18);

  stars *= mix(1.0, NL_GALAXY_DAY_VISIBILITY, env.dayFactor);

  // atmospheric extinction - stars dim near the horizon
  stars *= 0.35 + 0.65*abs(vdirOrig.y);

  // moonlight washout - stars fade near the bright moon
  float moonProx = max(dot(env.moonDir, vdirOrig), 0.0);
  stars *= 1.0 - 0.55*pow(moonProx, 3.0);

  return stars*(1.0-env.rainFactor);
}


#endif
