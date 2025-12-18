#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;
layout (location = 3) in vec3 aOffset;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vBaseColor;
out vec2 vSeedXZ;     // for stable random per instance
out float vLocalY;    // for canopy mask

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;

void main() {
    // instancing position
    vec3 localPos = aPos + aOffset;
    vec4 worldPos4 = u_model * vec4(localPos, 1.0);
    vWorldPos = worldPos4.xyz;

    // correct normal with scaling
    mat3 normalMat = transpose(inverse(mat3(u_model)));
    vNormal = normalize(normalMat * aNormal);

    vBaseColor = aColor;

    // stable seed (do NOT use world pos because u_model changes per chunk)
    vSeedXZ = aOffset.xz;
    vLocalY = aPos.y;

    gl_Position = u_projection * u_view * worldPos4;
}
