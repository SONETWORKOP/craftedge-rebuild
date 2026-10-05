$input v_texcoord0

#include <bgfx_shader.sh>

#ifndef INSTANCING
  #include <newb/config.h>
  #include <newb/functions/tonemap.h>

  uniform vec4 SunMoonColor;
  uniform vec4 FogColor;
  uniform vec4 FogAndDistanceControl;

  SAMPLER2D_AUTOREG(s_SunMoonTexture);

  bool sunmoonUnderwater(vec3 FOG_COLOR, vec2 FOG_CONTROL) {
    return FOG_CONTROL.x == 0.0 && FOG_CONTROL.y < 0.8 && (FOG_COLOR.b > FOG_COLOR.r || FOG_COLOR.g > FOG_COLOR.r);
  }
#endif

void main() {
  #ifndef INSTANCING
    // underwater sun/moon pura hatao - paani ke andar disc render nahi hoga
    if (sunmoonUnderwater(FogColor.rgb, FogAndDistanceControl.xy)) {
      gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
    } else {
    vec4 color = texture2D(s_SunMoonTexture, v_texcoord0);
    color.rgb *= SunMoonColor.rgb;
    color.rgb *= 4.4*color.rgb;
    float tr = 1.0 - SunMoonColor.a;
    color.a *= 1.0 - tr*tr;
    color.rgb = colorCorrection(color.rgb);
    gl_FragColor = color;
    }
  #else
    gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
  #endif
}
