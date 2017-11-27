#version 330 core

// Authors : Matteo Marcuzzo, Meshal Marakkar

// Based on and adapted from the multiple lights tutorial by http://learnopengl.com/
// Available: https://learnopengl.com/#!Lighting/Multiple-lights
// [Accessed: December 2016]

layout (location = 0) in vec3 in_Position;
layout (location = 2) in vec3 in_Normal;
layout (location = 3) in vec2 in_TexCoord;

out vec3 Normal;
out vec3 FragPos;
out vec2 TexCoords;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

void main()
{
    gl_Position = projection * view *  model * vec4(in_Position, 1.0f);
    FragPos = vec3(model * vec4(in_Position, 1.0f));
    Normal = mat3(transpose(inverse(model))) * in_Normal;  
    TexCoords = in_TexCoord;
} 