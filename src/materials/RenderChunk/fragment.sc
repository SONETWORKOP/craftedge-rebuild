$input v_color0, v_color1, v_fog, v_refl, v_texcoord0, v_lightmapUV, v_extra, v_position, v_reflPbr, v_reflSun, v_sunMoon

#include <bgfx_shader.sh>
SAMPLER2D_AUTOREG(s_NoiseTexture);
// noise texture driving the aurora curtain - shared with the Sky dome via
// newb/functions/clouds.h so the water mirror draws the same aurora shape
SAMPLER2D_AUTOREG(s_NoiseVoxel);
#define NL_ROUNDED_CLOUDS
#define NL_AURORA_REFLECTION
#include <newb/main.sh>

SAMPLER2D_AUTOREG(s_MatTexture);
SAMPLER2D_AUTOREG(s_SeasonsTexture);
SAMPLER2D_AUTOREG(s_LightMapTexture);
SAMPLER2D_AUTOREG(s_SunTexture);
SAMPLER2D_AUTOREG(s_MoonTexture);

uniform vec4 CameraPosition;
uniform vec4 ViewPositionAndTime;
uniform vec4 FogColor;
uniform vec4 MoonPhase;

// (Water cloud-mirror HATAYA - paani me clouds reflection nahi.
// Sirf aurora aks + sun/moon disc mirror main() me rahenge.)

/*
  Real textured sun/moon mirror on water.

  Projects the reflected view ray into the celestial body's local plane (built
  from its direction), samples the vanilla sun or moon texture there, and keeps
  only the bright pixels (luminance mask) inside a soft circular falloff (dist
  mask). `quadTan` is half the body's angular size; `cellScale`/`cellOffset`
  select a sub-rectangle of the texture (the moon_phases.png 4x2 grid). The
  textures are bound through the `SunTexture`/`MoonTexture` buffers -> textures/
  environment/sun / moon_phases, NOT a white default - a white default rendered
  the mirror invisible.
*/
vec3 celestialTextureMovement(
  sampler2D tex, vec3 bodyDir, vec3 rayDir, float quadTan,
  vec2 cellScale, vec2 cellOffset, out float mask
) {
  vec3 forward = normalize(bodyDir);
  vec3 right = normalize(cross(vec3(0.0, 1.0, 0.0), forward));
  vec3 up = cross(forward, right);

  vec2 uv = vec2(dot(rayDir, right), dot(rayDir, up));
  uv = uv / quadTan * 0.5 + 0.5;

  float distMask = smoothstep(0.5, 0.48, length(uv - 0.5));
  vec2 texUV = uv*cellScale + cellOffset;

  vec3 bodyColor = texture2D(tex, texUV).rgb;
  float lum = dot(bodyColor, vec3(0.299, 0.587, 0.114));
  float maskLum = smoothstep(0.0, 0.9, lum);
  mask = maskLum * distMask;
  return bodyColor * mask;
}

void main() {
  #if defined(DEPTH_ONLY_OPAQUE) || defined(DEPTH_ONLY) || defined(INSTANCING)
    gl_FragColor = vec4(1.0,1.0,1.0,1.0);
    return;
  #endif

  vec4 diffuse = texture2D(s_MatTexture, v_texcoord0);
  vec4 color = v_color0;

  #ifdef ALPHA_TEST
    if (diffuse.a < 0.6) {
      discard;
    }
  #endif

  #if defined(SEASONS) && (defined(OPAQUE) || defined(ALPHA_TEST))
    diffuse.rgb *= mix(vec3(1.0,1.0,1.0), texture2D(s_SeasonsTexture, v_color1.xy).rgb * 2.0, v_color1.z);
  #endif

  vec3 glow = nlGlow(s_MatTexture, v_texcoord0, v_extra.a);

  #if defined(TRANSPARENT) && !(defined(SEASONS) || defined(RENDER_AS_BILLBOARDS))
    if (v_extra.b > 0.9) {
      diffuse.rgb = vec3_splat(1.0 - NL_WATER_TEX_OPACITY*(1.0 - diffuse.b*1.8));
      diffuse.a = color.a;
    }
  #else
    diffuse.a = 1.0;
  #endif

  diffuse.rgb *= color.rgb;
  // tone.txt: sRGB -> linear after vertex color (replaces diffuse*diffuse)
  diffuse.rgb = sRGBtoLinear(diffuse.rgb);
  diffuse.rgb += glow;

  if (v_extra.b > 0.9) {
    diffuse.rgb += v_refl.rgb*v_refl.a;

    #ifndef NL_NO_WATER_CLOUD_REFL
      // rebuild the sky palette so the mirrored clouds are shaded with the
      // same horizon tint the Clouds material uses (cheap: only mix() ops)
      nl_environment wenv;
      wenv.end = false;
      wenv.nether = false;
      wenv.underwater = false;
      wenv.rainFactor = v_reflPbr.w;
      wenv.dayFactor = v_reflSun.w;
      wenv.sunDir = v_reflSun.xyz;
      wenv.moonDir = v_sunMoon.xyz;
      wenv.fogCol = FogColor.rgb;

      // cloud-mirror HATAYA - paani me clouds reflection nahi.
      // sun/moon disc mirror bhi HATAYA (v1 code remove) - sirf AURORA ka aks rahega.
      // (Medium subpack me ye bhi band - perf ke liye.)
      #ifndef NL_NO_WATER_CLOUD_AURORA_REFL
      #ifdef NL_AURORA_REFLECTION
        {
          vec3 aurV = normalize(v_reflPbr.xyz);
          vec3 aurReflDir = vec3(-aurV.x, aurV.y, -aurV.z);
          if (aurReflDir.y > 0.004) {
            float aurVdotU = clamp(aurReflDir.y, 0.0, 1.0);
            float aurNight = 1.0 - smoothstep(-0.02, 0.32, wenv.dayFactor);
            if (aurNight > 0.001 && aurVdotU > 0.15) {
              float aurDither = fract(sin(dot(aurReflDir.xy, vec2(12.9898, 78.233))) * 43758.5453);
              vec3 aurora = NL_AURORA_TEX*aurNight*nlAuroraBorealis(
                aurReflDir, aurVdotU, aurDither, wenv.rainFactor, CameraPosition.xz, ViewPositionAndTime.w
              );
              #ifdef NL_WATER_AURORA_MIRROR
                float auroraAmt = NL_WATER_AURORA_MIRROR;
              #else
                float auroraAmt = 0.55;
              #endif
              diffuse.rgb += aurora*auroraAmt;
            }
          }
        }
      #endif
      #endif

      // sun/moon mirror HATAYA (sun Reflection Demo V1 code remove) - paani me sun/moon aks nahi banega.
    #endif

    // ---- RAIN-ONLY splash rings - bina barish rivers saaf, barish me chhalle ----
    // v_sunMoon.w = camera underwater flag -> andar splash band
    #ifdef NL_WATER_SPLASH
      {
        vec3 splashWorld = v_position + CameraPosition.xyz;
        float splashDist = length(v_position.xz);
        float splashFade = clamp(1.0 - splashDist/24.0, 0.0, 1.0);
        float rainGate = smoothstep(0.02, 0.25, v_reflPbr.w);
        if (splashFade*rainGate > 0.002 && v_sunMoon.w < 0.5) {
          float splashAmp = 2.0*v_reflPbr.w*rainGate;
          float sp = nlRainSplash(splashWorld.xz, ViewPositionAndTime.w);
          float dayLight = clamp(v_reflSun.w*0.5 + 0.5, 0.25, 1.0);
          diffuse.rgb += sp*splashFade*splashAmp*NL_WATER_SPLASH_INTENSITY*vec3(0.22, 0.48, 0.95)*dayLight;
        }
      }
    #endif
  } else if (v_refl.a > 0.0) {
    // reflective effect - only on xz plane (ground / flat smooth blocks)
    float dy = abs(dFdy(v_extra.g));
    if (dy < 0.0002) {
      float mask = v_refl.a*(clamp(v_extra.r*10.0,8.2,8.8)-7.8);

      // RTX-style: drop diffuse and push the mirror reflection forward so
      // smooth blocks (iron, diamond, quartz) read like a true mirror
      #ifdef NL_PBR_BLOCK_REFL
        // === V3 PBR block reflection (normal-map + TBN + Cook-Torrance) ===
        vec3 viewDir = v_reflPbr.xyz;
        float rainFactor = v_reflPbr.w;
        vec3 sunDir = v_reflSun.xyz;

        // Clear blocks use a smooth normal, avoiding four texture samples per
        // reflected pixel. Rain restores the detailed texture-derived normal
        // so wet surfaces keep the existing high-quality PBR response.
        vec3 texNormal = vec3(0.0, 0.0, 1.0);
        if (rainFactor > 0.001) {
          texNormal = nlTexNormal(s_MatTexture, v_texcoord0, NL_PBR_ATLAS_TEXEL, NL_PBR_NORMAL_STRENGTH);
        }

        // rain makes the ground wetter -> stronger, smoother mirror
        float pbrMask = mask;
        #ifdef NL_PBR_RAIN_BOOST
          pbrMask *= 1.0 + NL_PBR_RAIN_BOOST*rainFactor;
        #endif
        pbrMask = min(pbrMask, 1.0);

        // drop some diffuse so the mirror reads through, then apply V3 layer
        diffuse.rgb *= 1.0 - 0.6*pbrMask;
        diffuse.rgb = nlApplyPbrRefl(diffuse.rgb, v_refl.rgb, pbrMask, texNormal, v_position, viewDir, sunDir);
      #else
        diffuse.rgb *= 1.0 - 0.75*mask;
        diffuse.rgb += v_refl.rgb*mask*1.15;
      #endif
    }
  }

  // ---- GROUND rain splash - sirf barish me, thos zameen, pattiyon par nahi ----
  // underwater camera par band
  #ifdef NL_WATER_SPLASH
  #ifndef ALPHA_TEST
    if (v_extra.b < 0.9 && v_sunMoon.w < 0.5) {
      float grain = v_reflPbr.w;
      if (grain > 0.02) {
        vec3 gWorld = v_position + CameraPosition.xyz;
        float gDist = length(v_position.xz);
        float gFade = clamp(1.0 - gDist/22.0, 0.0, 1.0);
        float flatM = 1.0 - smoothstep(0.0002, 0.0012, abs(dFdy(v_extra.g)));
        float vegGreen = diffuse.g - max(diffuse.r, diffuse.b);
        float vegM = 1.0 - smoothstep(0.015, 0.09, vegGreen);
        float gLight = clamp(v_lightmapUV.y*1.4, 0.12, 1.0);
        if (gFade*flatM*vegM > 0.003) {
          float gsp = nlRainSplashGround(gWorld.xz*1.25 + 7.7, ViewPositionAndTime.w*1.15);
          diffuse.rgb += gsp*gFade*grain*flatM*vegM*gLight*NL_WATER_SPLASH_INTENSITY*0.6*vec3(0.28, 0.52, 0.95);
        }
      }
    }
  #endif
  #endif

  diffuse.rgb = mix(diffuse.rgb, v_fog.rgb, v_fog.a);

  // water.txt (Download/water.txt): underwater extinction, tonemap se pehle.
  // v_sunMoon.w = camera underwater flag (vertex packs it), v_extra.b = water pixels.
  if (v_sunMoon.w > 0.5) {
    float uwWaterFlag = step(0.9, v_extra.b);
    diffuse.rgb = nlUnderwaterScatter(diffuse.rgb, normalize(v_reflSun.xyz), uwWaterFlag);
  }

  diffuse.rgb = colorCorrection(diffuse.rgb);

  gl_FragColor = diffuse;
}
