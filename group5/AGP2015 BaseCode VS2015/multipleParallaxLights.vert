#version 330 core

// Authors : Matteo Marcuzzo, Meshal Marakkar

layout (location = 0) in vec3 in_Position;
layout (location = 2) in vec3 in_Normal;
layout (location = 3) in vec2 in_TexCoord;
layout (location = 5) in vec4 in_Tangent;

out vec2 TexCoords;
out vec3 FragPos;
out vec3 tangentViewPos;
out vec3 tangentFragPos;

out vec3 worldNormal;
out vec3 worldTangent;
out vec3 bitangent;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform vec3 cameraPos;

out mat3 TBN;

void main()
{
    gl_Position = projection * view *  model * vec4(in_Position, 1.0f);
    FragPos = vec3(model * vec4(in_Position, 1.0f));
    TexCoords = in_TexCoord;

	mat3 normalMatrix = transpose(inverse(mat3(view)));
	worldNormal	= normalize(normalMatrix * in_Normal);
    worldTangent	= normalize(normalMatrix * in_Tangent.xyz);
    bitangent	= cross(worldNormal, worldTangent) * in_Tangent.w;

	vec3 T = normalize(mat3(model) * in_Tangent.xyz);
    vec3 N = normalize(mat3(model) * in_Normal); 
	vec3 Bitangent = cross(N, T) * in_Tangent.w;
	vec3 B = normalize(mat3(model) * Bitangent);

    TBN = transpose(mat3(T, B, N));
	tangentViewPos  = TBN * cameraPos;
    tangentFragPos  = TBN * FragPos;
} 