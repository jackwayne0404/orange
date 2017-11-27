// Fragment Shader – file "particle.frag"

#version 330
precision highp float; // needed only for version 1.30

uniform sampler2D textureUnit0;
uniform sampler2D textureUnit1;
uniform float ex_alpha;

in  vec4 ex_Color;
out vec4 out_Color;

void main(void) {
	vec2 p=gl_PointCoord*2.0-vec2(1.0);
	if (dot(p,p)>1.0) discard;
	out_Color = ex_Color * texture(textureUnit0, gl_PointCoord) + texture(textureUnit1, gl_PointCoord), ex_alpha;
	//out_Color[3] = ex_alpha;
}
