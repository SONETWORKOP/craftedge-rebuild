#ifndef INSTANCING
  $input v_worldPos, v_underwaterRainTimeDay
#endif

#include <bgfx_shader.sh>

#ifndef INSTANCING
  SAMPLER2D_AUTOREG(s_NoiseTexture);
  // noise texture driving the aurora curtain - declared before main.sh so the
  // shared aurora function in newb/functions/clouds.h (included by main.sh)
  // can sample it in both the Sky dome and the RenderChunk water mirror.
  SAMPLER2D_AUTOREG(s_NoiseVoxel);
  #define NL_ROUNDED_CLOUDS
  #define NL_AURORA_REFLECTION
  #include <newb/main.sh>
  uniform vec4 TimeOfDay;
  uniform vec4 Day;
  uniform vec4 FogColor;
  uniform vec4 FogAndDistanceControl;
  uniform vec4 CameraPosition;

  // the aurora borealis curtain lives in newb/functions/clouds.h
  // (nlAuroraBorealis) so the water mirror in RenderChunk draws the exact
  // same shape.
#endif

void main() {
  #ifndef INSTANCING
    vec3 viewDir = normalize(v_worldPos);

    nl_environment env;
    env.end = false;
    env.nether = false;
    env.underwater = v_underwaterRainTimeDay.x > 0.5;
    env.rainFactor = v_underwaterRainTimeDay.y;
    env.dayFactor = v_underwaterRainTimeDay.w;
    env.fogCol = FogColor.rgb;
    env = calculateSunParams(env, TimeOfDay.x, Day.x);

    nl_skycolor skycol = nlOverworldSkyColors(env);

    vec3 skyColor = nlRenderSky(skycol, env, -viewDir, v_underwaterRainTimeDay.z, true);
    #ifdef NL_SHOOTING_STAR
      skyColor += NL_SHOOTING_STAR*nlRenderShootingStar(viewDir, env.fogCol, v_underwaterRainTimeDay.z);
    #endif
    #ifdef NL_GALAXY_STARS
      skyColor += NL_GALAXY_STARS*nlRenderGalaxy(viewDir, env.fogCol, env, v_underwaterRainTimeDay.z);
    #endif

    // aurora borealis (night only, hidden by rain and underwater)
    if (!env.underwater) {
      float VdotU = clamp(viewDir.y, 0.0, 1.0);
      float nightFactor = 1.0 - smoothstep(-0.02, 0.32, env.dayFactor);
      if (nightFactor > 0.001 && VdotU > 0.15) {
        float dither = fract(sin(dot(viewDir.xy, vec2(12.9898, 78.233))) * 43758.5453);
        vec3 aurora = nlAuroraBorealis(
          viewDir, VdotU, dither, env.rainFactor,
          CameraPosition.xz, v_underwaterRainTimeDay.z
        );
        skyColor += NL_AURORA_TEX*aurora*nightFactor;
      }
    }

    // procedural vibrant clouds (cheap, no texture)
    #ifdef NL_SKY_CLOUDS
      if (!env.underwater && viewDir.y > 0.001) {
        float scale = 0.8 / viewDir.y;
        float cloudA = nlVibrantClouds(viewDir.xz*scale, 0.004*scale, v_underwaterRainTimeDay.z);
        cloudA *= smoothstep(0.05, 0.35, viewDir.y);   // horizon fade
        cloudA *= NL_SKY_CLOUD_OPACITY;

        // cloud color tinted by sky/sun, darker at night
        vec3 cloudCol = nlVibrantCloudColor(env.dayFactor, sunLightTint(env.dayFactor, env.rainFactor));
        skyColor.rgb = mix(skyColor.rgb, cloudCol, clamp(cloudA, 0.0, 1.0));
      }
    #else
      // raymarched rounded clouds (RoundedClouds from cloud.txt), the default
      // replacement for the old blocky box clouds
      if (!env.underwater && viewDir.y > 0.001) {
        float jitter = fract(sin(dot(viewDir.xy, vec2(12.9898, 78.233))) * 43758.5453);
        vec4 clouds = nlRoundedClouds(viewDir, v_underwaterRainTimeDay.z, jitter);
        float opacity = smoothstep(0.1, 0.3, viewDir.y);
        float cloudMask = clouds.a * 0.5 * opacity;
        skyColor.rgb = mix(skyColor.rgb, clouds.rgb, cloudMask);
      }
    #endif

    skyColor = colorCorrection(skyColor);

    gl_FragColor = vec4(skyColor, 1.0);
  #else
    gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
  #endif
}
