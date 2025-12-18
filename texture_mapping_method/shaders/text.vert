#version 330 core
layout (location = 0) in vec4 aPosUV; // x,y,u,v (NDC)
out vec2 vUV;

void main(){
    gl_Position = vec4(aPosUV.xy, 0.0, 1.0);
    vUV = aPosUV.zw;
}
