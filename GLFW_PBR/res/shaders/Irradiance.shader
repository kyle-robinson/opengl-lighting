#shader vertex
#version 330 core
layout(location = 0) in vec3 aPos;

out vec3 localPos;

uniform mat4 projection;
uniform mat4 view;

void main()
{
	localPos = aPos;
	gl_Position = projection * view * vec4(localPos, 1.0);
}

#shader fragment
#version 330 core
out vec4 FragColor;
in vec3 localPos;

uniform samplerCube environmentMap;
const float PI = 3.14159265359;

void main()
{
	vec3 normal = normalize(localPos); // sample direction = hemisphere's orientation
	vec3 irradiance = vec3(0.0);

	// Convolution Code
	vec3 up = vec3(0.0, 1.0, 0.0);
	vec3 right = cross(up, normal);
	up = cross(normal, right);

	float sampleDelta = 0.025;
	float nrSamples = 0.0;
	for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
	{
		for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
		{
			vec3 tangentSample = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta)); // spherical to cartesian
			vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal; // tangent to world

			irradiance += texture(environmentMap, sampleVec).rgb * cos(theta) * sin(theta);
			nrSamples++;
		}
	}
	irradiance = PI * irradiance * (1.0 / float(nrSamples));
	FragColor = vec4(irradiance, 1.0);
}