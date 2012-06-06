#version 130

uniform sampler2D texture;
in vec4 pos;

const float PixelWeight[13]   = float[13](0.159577, 0.147308, 0.115877, 0.077674, 0.044368, 0.021596, 0.008958, 0.003166, 0.000954, 0.000245, 0.000054, 0.000010, 0.000002);
const float blur_offset = 1.0/400.0;

void main() {
  vec2 p = vec2(pos.x, 1-pos.y);
  gl_FragColor = texture2D(texture, p);
  
  for ( int i = 1; i < 13; i++ ){
    gl_FragColor += texture2D(texture, vec2(p.x - blur_offset * i, p.y)) * PixelWeight[i];
    gl_FragColor += texture2D(texture, vec2(p.x + blur_offset * i, p.y)) * PixelWeight[i];
    gl_FragColor += texture2D(texture, vec2(p.x, p.y - blur_offset * i)) * PixelWeight[i];
    gl_FragColor += texture2D(texture, vec2(p.x, p.y + blur_offset * i)) * PixelWeight[i];
  }
  gl_FragColor.xyz *= 0.365;
}
