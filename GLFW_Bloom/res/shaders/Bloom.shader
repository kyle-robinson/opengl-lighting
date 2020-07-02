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
	vs_out.FragPos = vec3(model * vec4(aPos, 1.0));
	vs_out.Normal = mat3(transpose(inverse(model))) * aNormal;
	vs_out.TexCoords = aTexCoords;
	gl_Position = projection * view * model * vec4(aPos, 1.0);
};

#shader fragment
#version 330 core
layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 BrightColor;

in VS_OUT
{
	vec3 FragPos;
	vec3 Normal;
	vec2 TexCoords;
} fs_in;

struct Light
{
	vec3 Position;
	vec3 Color;
};

uniform sampler2D diffuseMap;
uniform sampler2D specularMap;
uniform vec3 viewPos;

uniform Light lights[4];
uniform float intensity;
uniform float threshold;
uniform bool disco;

void main()
{
	vec3 color = texture(diffuseMap, fs_in.TexCoords).rgb;
	vec3 normal = normalize(fs_in.Normal);

	vec3 ambient = 0.01 * color;
	vec3 lighting = vec3(0.0);

	for (int i = 0; i < 4; i++)
	{
		vec3 lightDir = normalize(lights[i].Position - fs_in.FragPos);
		float diff = max(dot(lightDir, normal), 0.0);
		vec3 diffuse = lights[i].Color * diff * color;

		vec3 viewDir = normalize(viewPos - fs_in.FragPos);
		vec3 halfwayDir = normalize(lightDir + viewDir);
		float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);
		vec3 specular = lights[i].Color * spec * texture(disco ? specularMap : diffuseMap, fs_in.TexCoords).rgb;

		vec3 result = diffuse + specular;

		float distance = length(fs_in.FragPos - lights[i].Position);
		result *= intensity / (distance * distance);
		lighting += result;
	}
	FragColor = vec4(ambient + lighting, 1.0);

	float brightness = dot(FragColor.rgb, vec3(0.2126, 0.7152, 0.0722));
	if (brightness > threshold)
		BrightColor = vec4(FragColor.rgb, 1.0);
	else
		BrightColor = vec4(0.0, 0.0, 0.0, 1.0);
};