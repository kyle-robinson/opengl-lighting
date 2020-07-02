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

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

void main()
{
	vs_out.FragPos = aPos;
	vs_out.Normal = aNormal;
	vs_out.TexCoords = aTexCoords;
	gl_Position = projection * view * model * vec4(aPos, 1.0);
};

#shader fragment
#version 330 core
out vec4 FragColor;

in VS_OUT
{
	vec3 FragPos;
	vec3 Normal;
	vec2 TexCoords;
} fs_in;

struct Material
{
	sampler2D texture_diffuse1;
	float shininess;
};

uniform vec3 viewPos;
uniform vec3 lightPositions[4];
uniform vec3 lightColors[4];
uniform Material material;
uniform bool blinn;
uniform bool gamma;

vec3 BlinnPhong(vec3 normal, vec3 fragPos, vec3 lightPos, vec3 lightColor)
{
	vec3 lightDir = normalize(lightPos - fragPos);
	float diff = max(dot(lightDir, normal), 0.0);
	vec3 diffuse = diff * lightColor;

	vec3 viewDir = normalize(viewPos - fragPos);
	vec3 reflectDir = reflect(-lightDir, normal);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);

	if (blinn)
	{
		vec3 halfwayDir = normalize(lightDir + viewDir);
		spec = pow(max(dot(normal, halfwayDir), 0.0), material.shininess * 2.0);
	}
	vec3 specular = spec * lightColor;

	float max_distance = 1.5f;
	float distance = length(lightPos - fragPos);
	float attenuation = max_distance / (gamma ? distance * distance : distance);

	diffuse *= attenuation;
	specular *= attenuation;

	return diffuse + specular;
};

void main()
{
	vec3 color = vec3(texture(material.texture_diffuse1, fs_in.TexCoords));
	vec3 lighting = vec3(0.0);

	for (int i = 0; i < 4; ++i)
		lighting += BlinnPhong(normalize(fs_in.Normal), fs_in.FragPos, lightPositions[i], lightColors[i]);

	color *= lighting;

	if (gamma)
		color = pow(color, vec3(1.0 / 2.2));

	FragColor = vec4(color, 1.0);
};