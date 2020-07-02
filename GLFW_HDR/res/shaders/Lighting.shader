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

uniform bool inverse_normals;

void main()
{
	vs_out.FragPos = vec3(model * vec4(aPos, 1.0));

	vec3 n = inverse_normals ? -aNormal : aNormal;
	vs_out.Normal = mat3(transpose(inverse(model))) * n;

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

struct Light
{
	vec3 Position;
	vec3 Color;
};

uniform sampler2D diffuseMap;
uniform vec3 viewPos;

uniform Light lights[4];
uniform bool lightCube;

void main()
{
	if (!lightCube)
	{
		vec3 color = texture(diffuseMap, fs_in.TexCoords).rgb;
		vec3 normal = normalize(fs_in.Normal);

		vec3 ambient = 0.0 * color;
		vec3 lighting = vec3(0.0);

		for (int i = 0; i < 4; i++)
		{
			vec3 lightDir = normalize(lights[i].Position - fs_in.FragPos);
			float diff = max(dot(lightDir, normal), 0.0);
			vec3 diffuse = lights[i].Color * diff * color;

			vec3 viewDir = normalize(viewPos - fs_in.FragPos);
			vec3 halfwayDir = normalize(lightDir + viewDir);
			float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);
			vec3 specular = lights[i].Color * spec * color;

			vec3 result = diffuse + specular;

			float distance = length(fs_in.FragPos - lights[i].Position);
			result *= 1.0 / (distance * distance);
			lighting += result;
		}
		FragColor = vec4(ambient + lighting, 1.0);
	}
	else
		FragColor = vec4(lights[0].Color, 1.0);
};