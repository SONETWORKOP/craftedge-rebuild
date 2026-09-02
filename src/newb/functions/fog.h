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

#endif
