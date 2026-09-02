#ifndef NL_CONFIG_H
#define NL_CONFIG_H

/*
  CraftEdge Visuals - Vivid & Realistic
  Based on Newb Shader (https://github.com/devendrn/newb-x-mcbe)
  Tuned for punchy, saturated colors with natural lighting and soft shadows.
*/

/* Color correction */
#define NL_TONEMAP_TYPE 4              // ACES filmic - cinematic highlight rolloff + natural desaturation
#define NL_GAMMA 1.2                   // slightly moodier midtones (less lift than vivid)
#define NL_EXPOSURE 1.12               // keep brightness (ACES *0.85 already darkens a touch)
#define NL_SATURATION 1.08             // pulled back from 1.4 -> restrained, filmic color
#define NL_TINT                        // ON: subtle teal-orange cinematic split-tone
#define NL_TINT_LOW  vec3(0.85,0.92,1.08)  // shadows lean cool/teal
#define NL_TINT_HIGH vec3(1.08,1.0,0.86)   // highlights lean warm/orange

/* Lighting - BSL-like strong directional light */
#define NL_SUNLIGHT_INTENSITY   4.8    // strong BSL-style sunlight
#define NL_TORCHLIGHT_INTENSITY 1.6    // warmer brighter torches
#define NL_SHADOW_INTENSITY     1.7    // slightly deeper shadows for cinematic mood
#define NL_MIN_LIGHTING_BOOST   0.82   // balanced ambient - dark nights but visible
//#define NL_BLINKING_TORCH
#define NL_CLOUD_SHADOW

/* Ambient */
#define NL_NETHER_AMBIENT vec3(3.0,2.16,1.89)
#define NL_END_AMBIENT    vec3(1.98,1.25,2.3)

/* Sun/moon - vivid but natural */
#define NL_DAWN_SUNLIGHT_COL   vec3(1.5,0.75,0.25)   // brighter warm orange sunrise light
#define NL_NOON_SUNLIGHT_COL   vec3(1.1,1.0,0.85)    // bright clean noon
#define NL_NIGHT_MOONLIGHT_COL vec3(0.05,0.12,0.32)  // cyan-tinted moonlight

/* Torch */
#define NL_OVERWORLD_TORCH_COL  vec3(1.0,0.55,0.2)
#define NL_UNDERWATER_TORCH_COL vec3(1.0,0.55,0.2)
#define NL_NETHER_TORCH_COL     vec3(1.0,0.5,0.18)
#define NL_END_TORCH_COL        vec3(1.0,0.55,0.28)

/* Fog - realistic atmospheric depth */
#define NL_FOG 1.3
#define NL_MIST_DENSITY 0.38            // natural distance haze
#define NL_RAIN_MIST_OPACITY 0.4        // much thicker mist in rain
#define NL_CLOUDY_FOG 0.25              // stronger overcast haze on cloudy days

/* Height fog - ground-hugging mist that thins with altitude */
#define NL_HEIGHT_FOG 0.5
#define NL_HEIGHT_FOG_START 56.0
#define NL_HEIGHT_FOG_RANGE 55.0

/* Sky */
#define NL_SKY_VOID_FACTOR     0.5
#define NL_SKY_VOID_DARKNESS   0.3
#define NL_SKY_RAIN_MIX_FACTOR 0.95

/* Sky colors - warm realistic sky */
#define NL_DAWN_ZENITH_COL   vec3(0.45,0.30,0.50)     // warm twilight purple (less pink)
#define NL_DAWN_HORIZON_COL  vec3(3.2,0.85,0.20)      // golden orange sunrise
#define NL_DAWN_EDGE_COL     vec3(3.8,1.5,0.45)       // warm golden edge (brighter)
#define NL_DAY_ZENITH_COL    vec3(0.12,0.48,2.1)      // deep realistic sky blue
#define NL_DAY_HORIZON_COL   vec3(0.55,1.1,1.65)      // soft hazy blue horizon
#define NL_DAY_EDGE_COL      vec3(1.2,1.45,1.65)      // light atmospheric haze
#define NL_NIGHT_ZENITH_COL  vec3(0.05,0.16,0.30)    // cyan zenith
#define NL_NIGHT_HORIZON_COL vec3(0.08,0.24,0.38)    // cyan horizon
#define NL_NIGHT_EDGE_COL    vec3(0.10,0.30,0.45)    // bright cyan edge

// midnight boost for the night sky: multiplies night colors so the cyan
// survives the atmosphere dimmer + ACES tonemap. 0 = no boost (old behavior)
#define NL_NIGHT_SKY_BRIGHTNESS 1.5
#define NL_RAIN_ZENITH_COL   vec3(0.35,0.38,0.42)     // overcast grey
#define NL_RAIN_HORIZON_COL  vec3(0.48,0.5,0.52)
#define NL_END_ZENITH_COL    vec3(0.08,0.001,0.1)
#define NL_END_HORIZON_COL   vec3(0.6,0.02,0.6)

/* End black hole */
#define NL_END_BLACK_HOLE
#define NL_BH_COL_LOW  vec3(0.1,0.1,1.0)
#define NL_BH_COL_HIGH vec3(2.8,0.1,0.6)
#define NL_BH_DIST  1.8
#define NL_BH_SPEED 0.3

/* Rainbow */
#define NL_RAINBOW
#define NL_RAINBOW_CLEAR 0.0
#define NL_RAINBOW_RAIN  0.45

/* Ore glow */
#define NL_GLOW_TEX 2.8
#define NL_GLOW_SHIMMER 0.9
#define NL_GLOW_SHIMMER_SPEED 0.9
#define NL_GLOW_LEAK 0.5

/* Waving */
#define NL_PLANTS_WAVE 0.055
#define NL_LANTERN_WAVE 0.16
#define NL_WAVE_SPEED 2.8
#define NL_WAVE_RANGE 14.0

/* Water - vivid, realistic reflections */
#define NL_WATER_TRANSPARENCY 0.94      // clearer, less murky water
#define NL_WATER_BUMP 0.28              // stronger ripples -> sharper reflection detail
#define NL_WATER_WAVE_SPEED  0.6        // calmer, more natural wave motion
#define NL_WATER_TEX_OPACITY 0.18       // let reflections read through more than texture
#define NL_WATER_SUN_DISC    0.35       // per-pixel sun disc mirror strength on water (0 = off)
#define NL_WATER_SUN_QUAD_TAN 0.1283    // tan of half the sun quad angular size (35*NL_SUN_SIZE/300)
#define NL_WATER_MOON_QUAD_TAN 0.1167   // tan of half the moon quad angular size (35*NL_MOON_SIZE/300)
#define NL_WATER_CLOUD_MIRROR 0.42      // soft, realistic cloud mirror on water (1.0 = full)
#define NL_WATER_CLOUD_HEIGHT 192.0     // cloud height used by legacy cloud samplers
#define NL_WATER_CLOUD_REFLECTION_DEPTH 2.0 // clouds appear this many blocks below the surface
#define NL_WATER_CLOUD_REFL_RIPPLE 0.012 // very subtle swell drift (0.0 = dead-flat mirror)
#define NL_WATER_WAVE
//#define NL_WATER_REFL_MASK             // OFF: full reflection instead of patchy masked reflection
#define NL_WATER_TINT vec3(0.28,0.7,0.88)  // slightly deeper, more natural blue-green

/* Underwater */
#define NL_UNDERWATER_BRIGHTNESS 0.85
#define NL_CAUSTIC_INTENSITY 2.1
#define NL_UNDERWATER_WAVE 0.11
#define NL_UNDERWATER_STREAKS 1.1
#define NL_UNDERWATER_TINT vec3(0.8,0.95,1.0)

/* Cloud type - old vanilla box clouds replaced by sky-dome RoundedClouds (0) */
#define NL_CLOUD_TYPE 0            // 0=vanilla cloud (opacity 0 = disabled), default sky clouds are RoundedClouds

/* Sky-dome procedural clouds (vibrant) - OFF by default, enabled via VIBRANT_CLOUD subpack */
//#define NL_SKY_CLOUDS
#define NL_SKY_CLOUD_SPEED 0.09    // drift speed (cell units/sec) - sky and water reflection
#define NL_SKY_CLOUD_DIR vec2(1.0, 0.35) // wind direction the dome clouds drift along
#define NL_SKY_CLOUD_OPACITY 0.9   // max cloud opacity

/* Soft cloud */
#define NL_CLOUD1_SCALE vec2(0.016, 0.022)
#define NL_CLOUD1_DEPTH 1.4
#define NL_CLOUD1_SPEED 0.04
#define NL_CLOUD1_DENSITY 0.55
#define NL_CLOUD1_OPACITY 0.92

/* Vanilla cloud */
#define NL_CLOUD0_THICKNESS 2.1
#define NL_CLOUD0_RAIN_THICKNESS 4.0
#define NL_CLOUD0_OPACITY 0.0      // disabled: replaced by sky-dome procedural clouds
//#define NL_CLOUD0_MULTILAYER

/* Rounded cloud */
#define NL_CLOUD2_THICKNESS 2.1
#define NL_CLOUD2_RAIN_THICKNESS 2.5
#define NL_CLOUD2_STEPS 5
#define NL_CLOUD2_SCALE vec2(0.033, 0.033)
#define NL_CLOUD2_SHAPE vec2(0.5, 0.4)
#define NL_CLOUD2_DENSITY 25.0
#define NL_CLOUD2_VELOCITY 0.8
#define NL_CLOUD2_LAYER2_OFFSET 143.0
#define NL_CLOUD2_LAYER2_THICKNESS 2.5
#define NL_CLOUD2_LAYER2_RAIN_THICKNESS 3.0
#define NL_CLOUD2_LAYER2_STEPS 3
#define NL_CLOUD2_LAYER2_SCALE vec2(0.03, 0.03)
#define NL_CLOUD2_LAYER2_SHAPE vec2(0.5, 0.4)
#define NL_CLOUD2_LAYER2_DENSITY 25.0
#define NL_CLOUD2_LAYER2_VELOCITY 0.8

/* Realistic cloud */
#define NL_CLOUD3_SCALE vec2(0.03, 0.03)
#define NL_CLOUD3_SPEED 0.005
#define NL_CLOUD3_SHADOW 0.9
#define NL_CLOUD3_SHADOW_OFFSET 0.3

/* Aurora */
#define NL_AURORA 1.3
#define NL_AURORA_TEX 1.0          // texture-based sky aurora brightness (night only)
#define NL_AURORA_TEX_LAYERS 10    // curtain layers - main cost knob (2 taps each)
#define NL_AURORA_TEX_COL1 vec3(0.6,7.5,9.5)  // cool cyan, near layers
#define NL_AURORA_TEX_COL2 vec3(0.2,4.2,8.0)  // deeper teal-blue, far layers
#define NL_AURORA_VELOCITY 0.03
#define NL_AURORA_SCALE 0.04
#define NL_AURORA_WIDTH 0.18
#define NL_AURORA_COL1 vec3(0.2,0.6,1.0)   // sky blue
#define NL_AURORA_COL2 vec3(0.35,0.85,1.0)  // bright cyan-blue
#define NL_CLOUD_AURORA_REFLECTION

/* Shooting star */
#define NL_SHOOTING_STAR 1.0
#define NL_SHOOTING_STAR_PERIOD 5.0
#define NL_SHOOTING_STAR_DELAY 16.0

/* Galaxy */
//#define NL_GALAXY_STARS 2.0
#define NL_GALAXY_VIBRANCE 0.7
#define NL_GALAXY_SPEED 0.03
#define NL_GALAXY_DAY_VISIBILITY 0.0

/* Sun/Moon */
#define NL_SUN_SIZE  1.1
#define NL_MOON_SIZE 1.0
#define NL_SUN_PATH_YAW    15.0
#define NL_MOON_PATH_YAW   17.0
#define NL_SUN_PATH_TILT   31.0
#define NL_MOON_PATH_TILT -28.0
#define NL_SUN_TILT        45.0
#define NL_MOON_TILT       45.0

/* Godrays - strong volumetric light shafts (ray-traced light look) */
#define NL_GODRAY 1.2

/* PBR block reflection (from "block reflection V3") - fragment-stage
   normal-mapped, TBN-distorted, Cook-Torrance mirror on smooth blocks */
#define NL_PBR_BLOCK_REFL              // ON: enable V3-style PBR block reflection
#define NL_PBR_ROUGHNESS 0.45          // higher = softer glint (low values cause speckles)
#define NL_PBR_METALLIC 0.0            // 0 keeps texture bright (V3 note)
#define NL_PBR_F0 vec3(0.10,0.10,0.11) // subtle reflectance (V3 iron 0.56 was too hot)
#define NL_PBR_SUNCOLOR vec3(1.0,0.95,0.85) // neutral white glint, not golden
#define NL_PBR_SPEC_INTENSITY 0.25     // overall sun glint strength
#define NL_PBR_SPEC_CLAMP 0.6          // caps GGX spikes -> kills golden fireflies
#define NL_PBR_NORMAL_STRENGTH 0.7     // bump strength (2.0 was too noisy)
#define NL_PBR_RAIN_BOOST 1.4          // extra mirror strength when raining (wet ground)
#define NL_PBR_ATLAS_TEXEL vec2(0.0009765625, 0.001953125) // 1024x512 terrain atlas

/* Ground reflection - reflective wet ground (fakes ray-traced GI look) */
#define NL_GROUND_REFL 1.2              // ON: strong mirror-like reflection (RTX look)
#define NL_GROUND_RAIN_WETNESS 1.6     // strong puddle reflections while raining
#define NL_GROUND_RAIN_PUDDLES 0.9     // more scattered, natural puddle shapes

/* Rain reflection - strong wet/puddle mirror only while raining */
#define NL_RAIN_REFL_STRENGTH 1.5      // 0 = off, higher = stronger wet ground mirror

/* Entity */
#define NL_ENTITY_BRIGHTNESS     0.68
#define NL_ENTITY_EDGE_HIGHLIGHT 0.42

/* Weather - realistic rain look */
#define NL_WEATHER_SPECK 0.5             // less glossy/artificial speck highlight
#define NL_WEATHER_RAIN_SLANT 5.0        // more natural wind-driven slant
#define NL_WEATHER_PARTICLE_SIZE 0.85    // thinner, more realistic raindrops

/* Lava */
#define NL_LAVA_NOISE
#define NL_LAVA_NOISE_SPEED 0.2

/* ---- SUBPACK CONFIG ---- */
#ifdef LITE
  #define NO_WAVE
  #undef NL_GLOW_SHIMMER
  #undef NL_LAVA_NOISE
  #undef NL_WEATHER_SPECK
  #undef NL_SHOOTING_STAR
  #undef NL_CLOUD_AURORA_REFLECTION
  #undef NL_UNDERWATER_STREAKS
  #undef NL_RAIN_MIST_OPACITY
  #undef NL_CLOUDY_FOG
  #undef NL_ENTITY_EDGE_HIGHLIGHT
  #undef NL_PBR_BLOCK_REFL
  // halve the aurora curtain: 2 texture taps per layer per sky pixel
  #undef NL_AURORA_TEX_LAYERS
  #define NL_AURORA_TEX_LAYERS 5
  // NO_WAVE / NO_FOG handling (inline, since those subpacks were removed)
  #undef NL_PLANTS_WAVE
  #undef NL_LANTERN_WAVE
  #undef NL_UNDERWATER_WAVE
  #undef NL_WATER_WAVE
  #undef NL_FOG
  #define NL_NO_WATER_CLOUD_REFL
#endif

#ifdef VIBRANT_CLOUD
  #undef NL_CLOUD_TYPE
  #define NL_CLOUD_TYPE 0
  #define NL_SKY_CLOUDS
#endif

#ifdef NO_REFLECTIONS
  #undef NL_GROUND_REFL
  #undef NL_PBR_BLOCK_REFL
  #undef NL_RAIN_REFL_STRENGTH
  #define NL_NO_GROUND_REFL
  #define NL_NO_WATER_CLOUD_REFL
#endif

#endif
