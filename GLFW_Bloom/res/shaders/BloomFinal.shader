#shader vertex
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoords;

out VS_OUT
{
	vec2 TexCoords;
} vs_out;

void main()
{
	vs_out.TexCoords = aTexCoords;
	gl_Position = vec4(aPos, 1.0);
};

#shader fragment
#version 330 core
out vec4 FragColor;

in VS_OUT
{
	vec2 TexCoords;
} fs_in;

uniform sampler2D scene;
uniform sampler2D bloomBlur;

uniform bool bloom;
uniform float exposure;

void main()
{
	const float gamma = 2.2;
	vec3 hdrColor = texture(scene, fs_in.TexCoords).rgb;
	vec3 bloomColor = texture(bloomBlur, fs_in.TexCoords).rgb;

	if (bloom)
		hdrColor += bloomColor; // Additive Blending

	vec3 result = vec3(1.0) - exp(-hdrColor * exposure); // Tone Mapping
	result = pow(result, vec3(1.0 / gamma)); // Gamma Correction

	FragColor = vec4(result, 1.0);
};