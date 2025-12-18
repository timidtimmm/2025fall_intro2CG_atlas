#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uFont;
uniform vec3 textColor;

void main(){
    vec4 tex = texture(uFont, vUV); // RGBA
    // 常見 atlas 是白字+alpha：用 alpha 做透明
    FragColor = vec4(textColor, 1.0) * tex.a;
}
