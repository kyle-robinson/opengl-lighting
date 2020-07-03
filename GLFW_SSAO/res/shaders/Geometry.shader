#shader vertex
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;

out VS_OUT
{
	vec3 FragPos;
	vec3 Normal;
	vec2 TexCoords;
} vs_out;

uniform bool invertedNormals;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

void main()
{
	vec4 viewPos = view * model * vec4(aPos, 1.0);
	vs_out.FragPos = viewPos.xyz;
	vs_out.TexCoords = aTexCoords;
	vs_out.Normal = mat3(transpose(inverse(view * model))) * (invertedNormals ? -aNormal : aNormal);
	gl_Position = projection * viewPos;
};

#shader fragment
#version 330 core
layout(location = 0) out vec3 gPosition;
layout(location = 1) out vec3 gNormal;
layout(location = 2) out vec4 gAlbedoSpec;

in VS_OUT
{
	vec3 FragPos;
	vec3 Normal;
	vec2 TexCoords;
} fs_in;

uniform sampler2D texture_diffuse1;
uniform sampler2D texture_specular1;
uniform bool renderTexture;

void main()
{
	gPosition = fs_in.FragPos;
	gNormal = normalize(fs_in.Normal);
	gAlbedoSpec.rgb = (renderTexture ? texture(texture_diffuse1, fs_in.TexCoords).rgb : vec3(0.95));
	gAlbedoSpec.a = (renderTexture ? texture(texture_specular1, fs_in.TexCoords).r : 0.0);
};