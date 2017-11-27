// Vertex Shader – file "particle.vert"

#version 330

uniform mat4 MVP;

in vec3 in_Position;
in vec3 in_Color;

out vec4 ex_Color;

void main(void)
{
	ex_Color = vec4(in_Color, 1.0);
	gl_Position = MVP * vec4(in_Position, 1.0);
}