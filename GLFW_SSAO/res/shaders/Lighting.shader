#shader vertex
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoords;

out vec2 TexCoords;

void main()
{
	TexCoords = aTexCoords;
	gl_Position = vec4(aPos, 1.0);
};

#shader fragment
#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;
uniform sampler2D ssao;

struct Light
{
	vec3 Position;
	vec3 Color;

	float Linear;
	float Quadratic;
};

uniform Light light;

void main()
{
	// Get data from gBuffer
	vec3 FragPos = texture(gPosition, TexCoords).rgb;
	vec3 Normal = texture(gNormal, TexCoords).rgb;
	vec3 Diffuse = texture(gAlbedoSpec, TexCoords).rgb;
	float Specular = texture(gAlbedoSpec, TexCoords).a;
	float AmbientOcclusion = texture(ssao, TexCoords).r;

	// calculate lighting as normal
	vec3 ambient = vec3(0.3 * Diffuse * AmbientOcclusion);
	vec3 lighting = ambient;

	vec3 lightDir = normalize(light.Position - FragPos);
	vec3 diffuse = max(dot(Normal, lightDir), 0.0) * Diffuse * light.Color;

	vec3 viewDir = normalize(-FragPos);
	vec3 halfwayDir = normalize(lightDir + viewDir);
	float spec = pow(max(dot(Normal, halfwayDir), 0.0), 8.0);
	vec3 specular = light.Color * spec * Specular;

	float distance = length(light.Position - FragPos);
	float attenuation = 1.0 / (1.0 + light.Linear * distance + light.Quadratic * distance * distance);
	diffuse *= attenuation;
	specular *= attenuation;
	lighting += diffuse + specular;

	FragColor = vec4(lighting, 1.0);
};