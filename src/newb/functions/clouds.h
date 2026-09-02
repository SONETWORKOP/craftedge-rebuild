#ifndef CLOUDS_H
#define CLOUDS_H

#include "detection.h"
#include "noise.h"
#include "sky.h"

// simple clouds 2D noise
float cloudNoise2D(vec2 p, highp float t, float rain) {
  t *= NL_CLOUD1_SPEED;
  p += t;
  p.y += 3.0*sin(0.3*p.x + 0.1*t);

  vec2 p0 = floor(p);
  vec2 u = p-p0;
  u *= u*(3.0-2.0*u);
  vec2 v = 1.0-u;

  // multi-octave noise for more natural cloud shapes
  float n = mix(
    mix(rand(p0),rand(p0+vec2(1.0,0.0)), u.x),
    mix(rand(p0+vec2(0.0,1.0)),rand(p0+vec2(1.0,1.0)), u.x),
    u.y
  );
  n *= 0.5 + 0.5*sin(p.x*0.6 - 0.5*t)*sin(p.y*0.6 + 0.8*t);

  // second octave for detail
  vec2 p1 = p*2.5 + vec2(1.7, 3.2);
  vec2 p1f = floor(p1);
  vec2 u1 = p1-p1f;
  u1 *= u1*(3.0-2.0*u1);
  float n2 = mix(
    mix(rand(p1f),rand(p1f+vec2(1.0,0.0)), u1.x),
    mix(rand(p1f+vec2(0.0,1.0)),rand(p1f+vec2(1.0,1.0)), u1.x),
    u1.y
  );
  n = mix(n, n2, 0.35);

  n = min(n*(1.0+rain), 1.0);
  return n*n;
}

// hash13 - ported from reference clouds.txt (puffy cellular clouds)
float nlHash13(vec3 p) {
  p = fract(p * 0.1031);
  p += dot(p, p.yzx + 33.33);
  return fract((p.x + p.y) * p.z);
}

// Rounded cellular clouds adapted from Download/code.txt for the old-cloud pass.
float nlOldCloudHash(vec2 p) {
  return fract(cos(p.x + p.y*332.0)*335.552);
}

float nlOldCloudCell(vec2 uv, float size, float radius, float softness) {
  vec2 f = fract(uv)-0.5;
  vec2 q = abs(f)-vec2_splat(size-radius);
  float d = length(max(q,0.0))+min(max(q.x,q.y),0.0)-radius;
  return smoothstep(softness,-softness,d);
}

vec4 renderOldClouds(
    vec3 viewDir, vec2 cameraPos, highp float time, float rain, vec3 horizonCol
) {
  float invY = 0.8/max(viewDir.y,0.025);
  vec2 uv = viewDir.xz*invY+cameraPos*0.0025;
  float drift = -time*0.07;

  uv *= vec2(5.0,10.0);
  float minDist = 1.0;
  float shadeDist = 1.0;
  for (int i=0; i<3; i++) {
    uv /= 1.007;
    vec2 localUV = fract(uv+drift);
    vec2 baseCell = floor(uv+drift);

    for (int dx=0; dx<=1; dx++) {
      for (int dy=0; dy<=1; dy++) {
        vec2 offset = vec2(float(dx),float(dy));
        float occupied = step(0.85,nlOldCloudHash(baseCell+offset));
        vec2 local = localUV-offset;

        vec2 q = abs(local)-vec2_splat(0.5);
        float d = length(max(q,0.0))+min(max(q.x,q.y),0.0)-0.22;
        minDist = min(minDist,mix(1.0,d,occupied));

        vec2 shadeLocal = local-vec2(0.0,0.10);
        vec2 shadeQ = abs(shadeLocal)-vec2(0.54,0.41);
        float shadeD = length(max(shadeQ,0.0))+min(max(shadeQ.x,shadeQ.y),0.0)-0.16;
        shadeDist = min(shadeDist,mix(1.0,shadeD,occupied));
      }
    }
  }

  float alpha = smoothstep(0.03,-0.03,minDist);
  float shade = smoothstep(0.22,-0.22,shadeDist);
  alpha = clamp(alpha-shade*alpha*0.25,0.0,1.0);
  alpha *= smoothstep(0.05,0.35,viewDir.y);

  vec3 shadowCol = mix(horizonCol*0.55,vec3(0.48,0.55,0.7),0.4);
  vec3 color = mix(vec3_splat(1.0),shadowCol,0.3*shade);
  color *= 1.0-0.6*rain;
  return vec4(color,alpha);
}

/* ---- Sky-dome procedural "vibrant" clouds ----
   Shared by the Sky material and the water cloud reflection so both stages
   draw the exact same cloud shapes. Previously the water reflection carried
   its own copy of this noise, which drifted out of sync with the sky. */
float nlVibrantCloudNoise(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  f = f*f*(3.0-2.0*f);
  return mix(mix(nlOldCloudHash(i),          nlOldCloudHash(i+vec2(1.0,0.0)), f.x),
             mix(nlOldCloudHash(i+vec2(0.0,1.0)), nlOldCloudHash(i+vec2(1.0,1.0)), f.x), f.y);
}

// uv = viewDir.xz*0.8/viewDir.y (sky-dome projection), px = antialias width
float nlVibrantClouds(vec2 uv, float px, highp float time) {
  // directional wind drift. Applied after the uv scale below, so it is in cell
  // units - the same space renderOldClouds drifts in. A scalar drift here used
  // to push the clouds diagonally at a fixed 45 degrees.
  vec2 drift = -time*NL_SKY_CLOUD_SPEED*NL_SKY_CLOUD_DIR;

  uv *= 11.0;
  uv.y *= 2.0;

  float minDist = 1.0;
  float shadeDist = 1.0;

  for (int i=0; i<2; i++) {
    uv /= 1.005;
    vec2 localUV = fract(uv+drift)-0.5;
    vec2 baseCell = floor(uv+drift);
    vec2 cellOffset = vec2(localUV.x > 0.0 ? 0.0 : -1.0,
                           localUV.y > 0.0 ? 0.0 : -1.0);

    for (int dx=0; dx<=1; dx++) {
      for (int dy=0; dy<=1; dy++) {
        vec2 offset = cellOffset+vec2(float(dx),float(dy));
        float occupied = step(0.7, nlVibrantCloudNoise((baseCell+offset)/2.8));
        vec2 local = localUV-offset;

        vec2 q = abs(local)-vec2_splat(0.5);
        float d = length(max(q,0.0))+min(max(q.x,q.y),0.0)-0.22;
        minDist = min(minDist, mix(1.0,d,occupied));

        vec2 shadeLocal = local-vec2(0.01,0.01);
        vec2 shadeQ = abs(shadeLocal)-vec2_splat(0.39);
        float shadeD = length(max(shadeQ,0.0))+min(max(shadeQ.x,shadeQ.y),0.0)-0.16;
        shadeDist = min(shadeDist, mix(1.0,shadeD,occupied));
      }
    }
  }

  float alpha = smoothstep(px,-px,minDist);
  float shade = smoothstep(px*8.0,-px*8.0,shadeDist);
  return clamp(alpha-shade*alpha*0.15, 0.0, 1.0);
}

// sunTint comes from sunLightTint() (lighting.h is included after this header)
vec3 nlVibrantCloudColor(float dayFactor, vec3 sunTint) {
  vec3 col = vec3_splat(1.1)*mix(0.35, 1.0, clamp(dayFactor+0.25, 0.0, 1.0));
  return col*(0.7+0.5*sunTint);
}

// soft multi-octave value noise (fBm) - smooth amorphous cloud shapes.
// bilinear-interpolated hash cells give organic puffs instead of the hard
// 8px-cube cells, so clouds read thick and realistic.
float cloudFbm2D(vec2 p, highp float t) {
  // slow drift + gentle sway
  p += NL_CLOUD1_SPEED*t;
  p.y += 3.0*sin(0.35*p.x + 0.12*t);

  float amp = 1.0;
  float freq = 1.0;
  float n = 0.0;
  float total = 0.0;
  for (int i = 0; i < 4; i++) {
    vec2 q = p*freq;
    vec2 c = floor(q);
    vec2 f = q - c;
    f = f*f*(3.0-2.0*f);
    float v = mix(
      mix(nlHash13(vec3(c, 0.0)), nlHash13(vec3(c+vec2(1.0,0.0), 0.0)), f.x),
      mix(nlHash13(vec3(c+vec2(0.0,1.0), 0.0)), nlHash13(vec3(c+vec2(1.0,1.0), 0.0)), f.x),
      f.y
    );
    n += amp*v;
    total += amp;
    amp *= 0.52;
    freq *= 2.13;
  }
  return n/total;
}

// realistic cloud density - soft fBm with gentle coverage ramp
float cloudDensity2D(vec2 p, highp float t, float rain) {
  float n = cloudFbm2D(p, t);
  // wide soft ramp so clouds have fluffy rounded edges, not cube steps
  float cover = 0.44 - 0.18*rain;
  n = smoothstep(cover, cover+0.42, n);
  // curvature makes the cores fuller and the edges feathered
  n = n*n*(1.45 - 0.45*n);
  return n;
}

// simple clouds - box-style crisp puffs for reflections
vec4 renderCloudsSimple(nl_skycolor skycol, vec3 pos, highp float t, float rain, vec3 viewDir, vec3 sunDir, vec3 sunCol) {
  pos.xz *= NL_CLOUD1_SCALE;
  float d = cloudDensity2D(pos.xz, t, rain);
  // crisp step so reflected clouds match the vanilla Box cloud look
  d = smoothstep(0.08, 0.5, d);

  // vibrant cloud base - brighter, punchier white with a touch more contrast
  vec3 cloudWhite = vec3(1.0, 1.0, 1.02);
  vec3 cloudShadow = mix(skycol.horizon * 0.5, vec3(0.42, 0.5, 0.68), 0.45);
  vec3 cloudBase = mix(cloudShadow, cloudWhite, smoothstep(0.0, 0.6, d));

  // box-style crisp puffy edges
  vec4 col = vec4(cloudBase, smoothstep(0.05, 0.55, d));

  // stronger brightness ramp on thick/tall cloud cores for a puffier, more
  // voluminous look instead of a flat wash of white
  float brightness = smoothstep(0.2, 0.7, d);
  col.rgb += brightness * vec3(0.25, 0.22, 0.16);

  // deeper bottom shadow for stronger volumetric contrast (vibrant look
  // leans into punchy light/dark separation rather than soft grey)
  float bottomShadow = smoothstep(0.0, 0.5, d) * 0.3;
  col.rgb -= vec3(0.14, 0.16, 0.19) * (1.0 - bottomShadow);

  // forward-scattering: brighten clouds facing the sun
  float mu = dot(normalize(viewDir), normalize(sunDir));
  float forwardScatter = pow(max(mu, 0.0), 4.0);
  col.rgb += forwardScatter * sunCol * 1.1 * col.a;

  // warm edge glow - sunlight hitting cloud edges
  float edgeGlow = pow(max(1.0 - abs(mu), 0.0), 6.0);
  col.rgb += edgeGlow * vec3(1.0, 0.85, 0.55) * 0.4 * col.a;

  // rim light on cloud edges - sun silhouette
  float rimLight = pow(max(1.0 + mu, 0.0), 8.0) * 0.5;
  col.rgb += rimLight * sunCol * col.a;

  // slight extra saturation push so clouds don't wash out flat white
  float cloudLum = dot(col.rgb, vec3(0.299, 0.587, 0.114));
  col.rgb = mix(vec3_splat(cloudLum), col.rgb, 1.15);

  // darken during rain
  col.rgb *= 1.0 - 0.7*rain;
  return col;
}

// rounded clouds

// rounded clouds 3D density map
float cloudDf(vec3 pos, float rain, vec2 boxiness) {
  boxiness *= 0.999;
  vec2 p0 = floor(pos.xz);
  vec2 u = max((pos.xz-p0-boxiness.x)/(1.0-boxiness.x), 0.0);
  u *= u*(3.0 - 2.0*u);

  vec4 r = vec4(rand(p0), rand(p0+vec2(1.0,0.0)), rand(p0+vec2(1.0,1.0)), rand(p0+vec2(0.0,1.0)));
  r = smoothstep(0.1001+0.2*rain, 0.1+0.2*rain*rain, r); // rain transition

  float n = mix(mix(r.x,r.y,u.x), mix(r.w,r.z,u.x), u.y);

  // round y
  n *= 1.0 - 1.5*smoothstep(boxiness.y, 2.0 - boxiness.y, 2.0*abs(pos.y-0.5));

  n = max(1.25*(n-0.2), 0.0); // smoothstep(0.2, 1.0, n)
  n *= n*(3.0 - 2.0*n);
  return n;
}

vec4 renderCloudsRounded(
    vec3 vDir, vec3 vPos, float rain, float time, vec3 horizonCol, vec3 zenithCol,
    const int steps, const float thickness, const float thickness_rain, const float speed,
    const vec2 scale, const float density, const vec2 boxiness
) {
  float height = 7.0*mix(thickness, thickness_rain, rain);
  float stepsf = float(steps);

  // scaled ray offset
  vec3 deltaP;
  deltaP.y = 1.0;
  deltaP.xz = height*scale*vDir.xz/(0.02+0.98*abs(vDir.y));

  // local cloud pos
  vec3 pos;
  pos.y = 0.0;
  pos.xz = scale*(vPos.xz + vec2(1.0,0.5)*(time*speed));
  pos += deltaP;

  deltaP /= -stepsf;

  // alpha, gradient
  vec2 d = vec2(0.0,1.0);
  for (int i=1; i<=steps; i++) {
    float m = cloudDf(pos, rain, boxiness);
    d.x += m;
    d.y = mix(d.y, pos.y, m);
    pos += deltaP;
  }
  d.x *= smoothstep(0.03, 0.1, d.x);
  d.x /= (stepsf/density) + d.x;

  if (vPos.y < 0.0) { // view from top
    d.y = 1.0 - d.y;
  }

  // realistic cloud colors - white/grey with depth
  vec3 cloudTop = vec3(0.92, 0.95, 1.0);
  vec3 cloudBottom = mix(vec3(0.5, 0.55, 0.65), horizonCol * 0.6, 0.3);
  vec4 col = vec4(mix(cloudBottom, cloudTop, d.y), d.x);
  col.rgb += dot(col.rgb, vec3(0.12,0.1,0.08))*d.y*d.y;
  col.rgb *= 1.0 - 0.75*rain;
  return col;
}

float cloudsNoiseVr(vec2 p, float t) {
  float n = fastVoronoi2(p + t, 1.8);
  n *= fastVoronoi2(3.0*p + t, 1.5);
  n *= fastVoronoi2(9.0*p + t, 0.4);
  n *= fastVoronoi2(27.0*p + t, 0.1);
  //n *= fastVoronoi2(82.0*pos + t, 0.02); // more quality
  return n*n;
}

vec4 renderClouds(vec2 p, float t, float rain, vec3 horizonCol, vec3 zenithCol, const vec2 scale, const float velocity, const float shadow) {
  p *= scale;
  t *= velocity;

  // layer 1
  float a = cloudsNoiseVr(p, t);
  float b = cloudsNoiseVr(p + NL_CLOUD3_SHADOW_OFFSET*scale, t);

  // layer 2
  p = 1.4 * p.yx + vec2(7.8, 9.2);
  t *= 0.5;
  float c = cloudsNoiseVr(p, t);
  float d = cloudsNoiseVr(p + NL_CLOUD3_SHADOW_OFFSET*scale, t);

  // higher = less clouds thickness
  // lower separation between x & y = sharper
  vec2 tr = vec2(0.6, 0.7) - 0.12*rain;
  a = smoothstep(tr.x, tr.y, a);
  c = smoothstep(tr.x, tr.y, c);

  // shadow
  b *= smoothstep(0.2, 0.8, b);
  d *= smoothstep(0.2, 0.8, d);

  vec4 col;
  col.a = a + c*(1.0-a);
  // realistic white/grey clouds with depth shadow
  vec3 cloudWhite = vec3(0.93, 0.95, 1.0);
  vec3 cloudShadow = mix(horizonCol * 0.5, vec3(0.5, 0.55, 0.65), 0.4);
  col.rgb = mix(cloudShadow, cloudWhite, shadow*mix(b, d, c));
  col.rgb *= 1.0-0.65*rain;

  return col;
}

// --- RoundedClouds from Download/cloud.txt (raymarched clouds) ---
// Guarded by NL_ROUNDED_CLOUDS: it needs s_NoiseTexture (textures/environment/
// clouds), which only materials that define NL_ROUNDED_CLOUDS declare.
#ifdef NL_ROUNDED_CLOUDS
float nlCloudFbm(vec3 p) {
  vec2 uv = p.xy * 0.025;
  uv = fract(uv);
  return texture2D(s_NoiseTexture, uv).r;
}

vec4 nlRoundedClouds(vec3 viewDir, float time, float jitter) {
  float cloudBase = 1.1;
  float cloudTop = 1.3;
  int steps = 32;
  vec3 bottomColor = vec3(0.5, 0.55, 0.6);
  vec3 sideColor = vec3(0.9, 0.95, 1.0);
  vec3 colors = mix(sideColor, bottomColor, 0.5);
  float stepSize = (cloudTop - cloudBase) / float(steps);

  vec3 rayOrigin = vec3(0.0, 0.0, 0.0);
  vec3 cloudAccum = vec3(0.35, 0.35, 0.35);
  float alphaAccum = 0.0;
  float viewLift = step(0.0, viewDir.y);

  float prevDensity = 0.0;

  for (int i = 0; i < steps; i++) {
    float height = cloudBase + stepSize * (float(i) + jitter);
    float t = height / max(viewDir.y, 0.001);
    t = min(t, 55.0);
    vec3 pos = rayOrigin + viewDir * t;

    vec3 noisePos = vec3(pos.xz * 0.9 + time * 0.05, height * 0.8);
    float base = nlCloudFbm(noisePos);

    float heightNorm = (height - cloudBase) / (cloudTop - cloudBase);
    float heightFactor = smoothstep(0.2, 0.9, heightNorm) * (1.0 - smoothstep(0.6, 1.0, heightNorm));
    heightFactor *= smoothstep(0.2, 0.6, base);

    float density = base;
    density = 2.4 * clamp(density - 0.0, 0.0, 1.0);
    density = pow(density, 1.8) * heightFactor;

    float alpha = 1.0 - smoothstep(0.01, 0.0, density);
    alpha *= (1.0 - alphaAccum) * viewLift;

    vec3 TopColor = vec3(1.0, 0.925, 0.875);
    vec3 BottomColor = vec3(0.0, 0.15, 0.25);
    float heightNormal = smoothstep(-0.2, 1.0, heightNorm);
    vec3 coloredScattering = mix(BottomColor, TopColor, heightNormal);
    cloudAccum += vec3(1.0, 1.0, 1.0) * coloredScattering * alpha;
    alphaAccum += alpha;

    if (alphaAccum > 0.98 && viewDir.y < 0.9) break;
  }

  return vec4(cloudAccum, alphaAccum);
}
#endif

// aurora is rendered on clouds layer
#ifdef NL_AURORA
vec4 renderAurora(vec3 p, float t, float rain, vec3 FOG_COLOR) {
  t *= NL_AURORA_VELOCITY;
  p.xz *= NL_AURORA_SCALE;
  p.xz += 0.05*sin(p.x*4.0 + 20.0*t);

  float d0 = sin(p.x*0.1 + t + sin(p.z*0.2));
  float d1 = sin(p.z*0.1 - t + sin(p.x*0.2));
  float d2 = sin(p.z*0.1 + 1.0*sin(d0 + d1*2.0) + d1*2.0 + d0*1.0);
  d0 *= d0; d1 *= d1; d2 *= d2;
  d2 = d0/(1.0 + d2/NL_AURORA_WIDTH);

  float mask = (1.0-0.8*rain)*max(1.0 - 4.0*max(FOG_COLOR.b, FOG_COLOR.g), 0.0);
  return vec4(NL_AURORA*mix(NL_AURORA_COL1,NL_AURORA_COL2,d1),1.0)*d2*mask;
}
#endif

// Texture-based aurora borealis - the night sky's curtain. Moved out of the
// Sky fragment (which alone used to draw it) so the water mirror in RenderChunk
// can sample the identical shape. Requires the material to bind s_NoiseVoxel.
#ifdef NL_AURORA_REFLECTION
float nlAurPow2(float x) { return x*x; }
float nlAurPow1_5(float x) { return x*sqrt(max(x, 0.0)); }
float nlAurClamp01(float x) { return clamp(x, 0.0, 1.0); }
float nlAurSqrt1(float x) { return sqrt(max(x, 0.0)); }

vec3 nlAuroraBorealis(vec3 vDir, float VdotU, float dither, float rain, vec2 camPosXZ, highp float t) {
  float visibility = nlAurSqrt1(nlAurClamp01(VdotU*1.5 - 0.225)) - rain;
  visibility *= 1.0 - VdotU*0.9;
  if (visibility <= 0.0) return vec3_splat(0.0);

  vec3 aurora = vec3_splat(0.0);

  vec3 wpos = vDir;
  wpos.xz /= max(0.0001, wpos.y);
  vec2 cameraPositionM = camPosXZ*0.0075;
  cameraPositionM.x += t*0.04;

  const int sampleCountP = NL_AURORA_TEX_LAYERS + 10;
  float ditherM = dither + 5.0;
  float auroraAnimate = t*0.001;

  for (int i = 0; i < NL_AURORA_TEX_LAYERS; i++) {
    float current = nlAurPow2((float(i) + ditherM)/float(sampleCountP));

    vec2 planePos = wpos.xz*(0.8 + current)*11.0 + cameraPositionM;
    planePos = floor(planePos)*0.0007;

    float n = texture2D(s_NoiseVoxel, planePos).b;
    n = nlAurPow2(nlAurPow2(nlAurPow2(nlAurPow2(1.0 - 2.0*abs(n - 0.5)))));
    n *= nlAurPow1_5(texture2D(s_NoiseVoxel, planePos*100.0 + auroraAnimate).b);

    float currentM = 1.0 - current;
    aurora += n*currentM*mix(NL_AURORA_TEX_COL1, NL_AURORA_TEX_COL2, nlAurPow2(nlAurPow2(currentM)));
  }

  aurora *= 1.3;
  return aurora*visibility/float(NL_AURORA_TEX_LAYERS);
}
#endif

vec4 nlCloudAuroraReflection(nl_skycolor skycol, nl_environment env, vec3 viewDir, vec3 wPos, vec3 CAMERA_POS, highp float t, float cloudAmount) {
  vec2 cloudPos = wPos.xz;
  float viewDirY = viewDir.y >= 0.0 ? max(viewDir.y, 0.01) : min(viewDir.y, -0.01);
  float surfaceY = wPos.y + CAMERA_POS.y;
  // renderCloudsSimple / renderAurora are plane samplers, so project the
  // reflected ray onto the real cloud layer. Using a tiny depth here (the old
  // behaviour) collapsed the projection and killed all parallax.
  vec2 projectionOffset = (NL_WATER_CLOUD_HEIGHT-surfaceY)*viewDir.xz/viewDirY;
  cloudPos += clamp(projectionOffset, -vec2_splat(4096.0), vec2_splat(4096.0));
  float fade = clamp(2.0 - 0.005*length(cloudPos), 0.0, 1.0);
  cloudPos += CAMERA_POS.xz;

  vec4 refl = vec4_splat(0.0);

  #ifdef NL_AURORA
    vec4 aurora = renderAurora(cloudPos.xyy, t, env.rainFactor, env.fogCol);
    aurora.a *= fade;
    refl = vec4(2.0*aurora.rgb*aurora.a, aurora.a);
  #endif

  // Cloud reflection always uses the lightweight Simple cloud algorithm,
  // regardless of which cloud subpack (Simple/Vanilla/Rounded/Box) is active
  // for the main sky. This keeps reflections cheap and working in all cases -
  // previously this was gated to NL_CLOUD_TYPE == 1 only, so reflections
  // silently disappeared on blocks/water whenever a different cloud subpack
  // was selected.
  // cloudAmount = 0.0 lets a caller take only the aurora layer (water uses its
  // own per-pixel cloud mirror so it must not draw a second cloud shape here).
  if (cloudAmount > 0.0) {
    vec3 mainSunDir = env.sunDir.y > 0.0 ? env.sunDir : env.moonDir;
    vec4 clouds = renderCloudsSimple(skycol, cloudPos.xyy, t, env.rainFactor, viewDir, mainSunDir, vec3(1.0,0.95,0.85));
    clouds.a *= fade*cloudAmount;
    refl = vec4(mix(refl.rgb, clouds.rgb, clouds.a), min(refl.a + clouds.a, 1.0));
  }

  return refl;
}

#endif
