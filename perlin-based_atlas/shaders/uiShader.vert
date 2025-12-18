#version 330 core
layout (location = 0) in vec2 aPos;   // (-1,-1) ~ (1,1) 的方形

uniform vec2 uPos;   // 按鈕中心位置 (NDC, -1~1)
uniform vec2 uSize;  // 按鈕半尺寸    (NDC)

void main()
{
    vec2 pos = uPos + aPos * uSize;  // scale + translate
    gl_Position = vec4(pos, 0.0, 1.0);
}
