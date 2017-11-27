#version 330 core

// Based on and adapted from the point shadows and shadow mapping tutorial by http://learnopengl.com/
// Available: https://learnopengl.com/#!Advanced-Lighting/Shadows/Point-Shadows, http://learnopengl.com/#!Advanced-Lighting/Shadows/Shadow-Mapping
// [Accessed: December 2016]

layout (location = 0) in vec3 position;
layout (location = 2) in vec3 normal;
layout (location = 3) in vec2 texCoords;

out vec2 TexCoords;

out VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
} vs_out;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

void main()
{
    gl_Position = projection * view * model * vec4(position, 1.0f);
    vs_out.FragPos = vec3(model * vec4(position, 1.0));
    vs_out.Normal = transpose(inverse(mat3(model))) * normal;
    vs_out.TexCoords = texCoords;
}  