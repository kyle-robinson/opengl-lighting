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

uniform sampler2D hdrBuffer;
uniform bool hdr;
uniform float exposure;

void main()
{
	const float gamma = 2.2;
	vec3 hdrColor = texture(hdrBuffer, fs_in.TexCoords).rgb;
	
	if (hdr)
	{
		//vec3 mapped = hdrColor / (hdrColor + vec3(1.0)); // Reinhard tone mapping
		vec3 mapped = vec3(1.0) - exp(-hdrColor * exposure); // Exposure
		mapped = pow(mapped, vec3(1.0 / gamma)); // Gamma Correction

		FragColor = vec4(mapped, 1.0);
	}
	else
	{
		vec3 result = pow(hdrColor, vec3(1.0 / gamma));
		FragColor = vec4(result, 1.0);
	}

};