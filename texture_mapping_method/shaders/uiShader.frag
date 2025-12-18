#version 330 core
out vec4 FragColor;
uniform vec4 uColor;  // RGBA, A 用來做半透明

void main()
{
    FragColor = uColor;
}
