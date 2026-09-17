#ifndef FOG_H
#define FOG_H

float nlRenderFogFade(float relativeDist, vec3 FOG_COLOR, vec2 FOG_CONTROL, bool isEnd) {
  #ifdef NL_FOG
    float fade = smoothstep(FOG_CONTROL.x, FOG_CONTROL.y, relativeDist);

    // exponential distance fog - closer to how real atmospheric haze builds up
    float expFade = 1.0 - exp(-relativeDist*relativeDist*0.9);
    fade = max(fade, expFade*0.35);

    // misty effect
    float density = NL_MIST_DENSITY*(19.0 - 18.0*FOG_COLOR.g);
    fade += (1.0-fade)*(0.3-0.3*exp(-relativeDist*relativeDist*density));

    float fog = NL_FOG * fade;
    #ifdef NL_END_FOG
      fog *= isEnd ? NL_END_FOG : 1.0;
    #endif

    return clamp(fog, 0.0, 1.0);
  #else
    return 0.0;
  #endif
}

float nlRenderHeightFog(float fade, float worldHeight, float relativeDist) {
  #if defined(NL_FOG) && defined(NL_HEIGHT_FOG)
    // thicker fog near ground, thins out with altitude (valley mist look)
    float heightFactor = 1.0 - smoothstep(NL_HEIGHT_FOG_START, NL_HEIGHT_FOG_START+NL_HEIGHT_FOG_RANGE, worldHeight);
    fade += (1.0-fade)*heightFactor*NL_HEIGHT_FOG*relativeDist;
    return clamp(fade, 0.0, 1.0);
  #else
    return fade;
  #endif
}

float nlRenderGodRayIntensity(vec3 cPos, vec3 worldPos, float t, vec2 uv1, float relativeDist, vec3 FOG_COLOR) {
  vec3 offset = cPos - 16.0*fract(worldPos*0.0625);
  offset = abs(2.0*fract(offset*0.0625)-1.0);
  offset = offset*offset*(3.0-2.0*offset);

  vec3 nrmof = normalize(worldPos);
  float u = nrmof.z/length(nrmof.zy);
  float diff = dot(offset,vec3(0.1,0.2,1.0)) + 0.07*t;
  float mask = nrmof.x*nrmof.x;

  float vol = sin(7.0*u + 1.5*diff)*sin(3.0*u + diff);
  vol *= vol*mask*uv1.y*(1.0-mask*mask);
  vol *= relativeDist*relativeDist;

  // dawn/dusk only - back to original Newb Shader behavior
  vol *= clamp(3.0*(FOG_COLOR.r-FOG_COLOR.b), 0.0, 1.0);

  vol = smoothstep(0.0, 0.1, vol);
  return vol;
}

vec3 nlGodRayTint(vec3 FOG_COLOR) {
  // warm yellow-gold tint for light shafts, blending toward deeper orange
  // at dawn/dusk when FOG_COLOR itself is already warm (higher red-blue diff)
  float dawnDusk = clamp(3.0*(FOG_COLOR.r-FOG_COLOR.b), 0.0, 1.0);
  vec3 dayRayTint = vec3(1.0, 0.92, 0.55);
  vec3 dawnRayTint = vec3(1.0, 0.75, 0.35);
  return mix(dayRayTint, dawnRayTint, dawnDusk);
}

// ---- ESTN-style sunbeams (suraj se nikalti radial kiranen) ----
// Self-contained 1D value noise (koi include nahi chahiye).
float nlBeamNoise(float x) {
  float i = floor(x);
  float f = x - i;
  float u = f * f * (3.0 - 2.0 * f);
  float a = fract(sin(i * 12.9898) * 43758.5453);
  float b = fract(sin((i + 1.0) * 12.9898) * 43758.5453);
  return mix(a, b, u);
}
// rel = camera-se-pixel ray (blocks), sunDir = suraj disha.
// facing = suraj ki taraf dekhne ka lobe (out), return = kiran pattern 0..~1.
// Center-seam guard: bilkul suraj-axis par pattern 0 (sparkle nahi).
float nlSunBeamPattern(vec3 rel, vec3 sunDir, out float facing) {
  vec3 sd = normalize(sunDir);
  vec3 up = abs(sd.y) > 0.99 ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
  vec3 t1 = normalize(cross(sd, up));
  vec3 t2 = cross(sd, t1);
  float dist = max(length(rel), 1e-4);
  vec3 rd = rel / dist;
  float sunAmt = max(dot(rd, sd), 0.0);
  // tight sun lobe + anti-sun hemisphere DEAD (moon side par zero).
  // ESTN screen-center mask ka 3D equivalent - v51 me wide tha isliye
  // chand ke piche aur suraj se door bagal me dikha. 60°+ par hard zero.
  float facing = pow(sunAmt, 6.0) * smoothstep(0.05, 0.5, sunAmt);
  float perp = length(vec2(dot(rd, t1), dot(rd, t2)));
  float ang = atan(dot(rd, t2), dot(rd, t1));
  float pattern = pow(nlBeamNoise(ang * 0.15915494 * 75.1), 1.75) * 1.75;
  pattern *= smoothstep(0.0, 0.05, perp);
  pattern *= smoothstep(8.0, 40.0, dist); // paas ka geometry clean
  return pattern;
}

// ---- fog.txt port (Download/fog.txt) ----
// Sunset glow cue 0..1 from fog color (godrays wali same trick).
float nlSunsetGlow(vec3 fogColor) {
  return clamp(3.0*(fogColor.r - fogColor.b), 0.0, 1.0);
}
// fog.txt getFog(): linear+quadratic exp fog with sunset/rain/nether/
// underwater rules. Engine-scale numbers pack space me map kiye:
//   fogDensity = relativeDist (0..1) space, *15.0 -> *6.0 (clear din me
//   pack fade jeette, sunset/rain me snippet jeete - change dikhega),
//   nether 30.0 -> 3.0 (30.0 pack me sab kuch 100% fog kar deta).
// Underwater exp(-dist*12.0) exact rakha (self-normalizing hai).
float nlGetFog(float dist, vec2 fogDensity, float rain, float sunsetSunrise, bool underwater, bool nether) {
  float fogStrength = mix(0.4, 0.6, sunsetSunrise);
  fogStrength = mix(fogStrength, 1.2, rain);
  if (nether) fogStrength = 3.0;

  float q1 = dist * fogDensity.x * fogStrength;
  float q2 = dist * dist * fogDensity.y * fogStrength * fogStrength;
  float fogFactor = 1.0 - exp(-(q1 + q2) * 6.0);

  if (underwater) {
    fogFactor = 1.0 - exp(-dist * 12.0);
  }

  return clamp(fogFactor, 0.0, 1.0);
}

#endif
