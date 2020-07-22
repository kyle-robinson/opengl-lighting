#shader vertex
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoords;
layout(location = 2) in vec3 aNormal;

out VS_OUT
{
	vec2 TexCoords;
	vec3 WorldPos;
	vec3 Normal;
} vs_out;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

void main()
{
	vs_out.TexCoords = aTexCoords;
	vs_out.WorldPos = vec3(model * vec4(aPos, 1.0));
	//vs_out.Normal = transpose(inverse(mat3(model))) * aNormal;
	vs_out.Normal = mat3(model) * aNormal;
	gl_Position = projection * view * model * vec4(aPos, 1.0);
};

#shader fragment
#version 330 core
out vec4 FragColor;

in VS_OUT
{
	vec2 TexCoords;
	vec3 WorldPos;
	vec3 Normal;
} fs_in;

// Material sample textures
uniform sampler2D albedoMap;
uniform sampler2D normalMap;
uniform sampler2D metallicMap;
uniform sampler2D roughnessMap;
uniform sampler2D aoMap;

// Material parameters
uniform vec3 albedoF;
uniform float metallicF;
uniform float roughnessF;
uniform float aoF;

uniform bool aoTexture;
uniform bool texture_none;

// IBL
uniform samplerCube irradianceMap;
uniform samplerCube prefilterMap;
uniform sampler2D brdfLUT;

// Lights
uniform vec3 lightPositions[5];
uniform vec3 lightColors[5];
uniform vec3 viewPos;
uniform bool lightSource;

const float PI = 3.14159265359;

vec3 getNormalFromMap();
float DistributionGGX(vec3 N, vec3 H, float roughness);
float GeometrySchlickGGX(float NdotV, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);
vec3 fresnelSchlick(float cosTheta, vec3 F0);
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness);

void main()
{
	// material properties
	vec3 albedo = (texture_none ? albedoF : pow(texture(albedoMap, fs_in.TexCoords).rgb, vec3(2.2)));
	float metallic = (texture_none ? metallicF : texture(metallicMap, fs_in.TexCoords).r);
	float roughness = (texture_none ? roughnessF : texture(roughnessMap, fs_in.TexCoords).r);// (roughnessTexture ? texture(roughnessMap, fs_in.TexCoords).r : roughnessF));
	float ao = (texture_none ? aoF : (aoTexture ? texture(aoMap, fs_in.TexCoords).r : aoF));
	
	vec3 N = (texture_none ? fs_in.Normal : getNormalFromMap()); // Get normals from map
	vec3 V = normalize(viewPos - fs_in.WorldPos); // view direction
	vec3 R = reflect(-V, N);

	vec3 F0 = vec3(0.04); // surface reflection at 0 incidence
	F0 = mix(F0, albedo, metallic);

	// reflectance equation
	vec3 Lo = vec3(0.0);
	for (int i = 0; i < 5; ++i)
	{
		vec3 L = normalize(lightPositions[i] - fs_in.WorldPos); // light direction
		vec3 H = normalize(V + L); // halfway direction

		// calculate per-light radiance
		float distance = length(lightPositions[i] - fs_in.WorldPos);
		float attenuation = 1.0 / (distance * distance);
		vec3 radiance = lightColors[i] * attenuation;

		// cook-torrance brdf
		float NDF = DistributionGGX(N, H, roughness);
		float G = GeometrySmith(N, V, L, roughness);
		vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
		//vec3 F = fresnelSchlickRoughness(max(dot(H, V), 0.0), F0, roughness);

		vec3 numerator = NDF * G * F;
		float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0); // 0.001 is to prevent divide by 0
		vec3 specular = numerator / max(denominator, 0.001);

		vec3 kS = F; // kS = fresnel
		vec3 kD = vec3(1.0) - kS; // energy conservation: diffuse and specular can't be above 1.0

		kD *= 1.0 - metallic; // multiply kD by the inverse metalness so that only non-metals have diffuse lighting

		float NdotL = max(dot(N, L), 0.0); // scale light by NdotL
		Lo += (kD * albedo / PI + specular) * radiance * NdotL; // add to outgoing radiance Lo
	}

	// ambient lighting (using IBL as the ambient term)
	vec3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
	
	vec3 kS = F;
	vec3 kD = 1.0 - kS;
	kD *= 1.0 - metallic;
	vec3 irradiance = texture(irradianceMap, N).rgb;
	vec3 diffuse = irradiance * albedo;

	// sample both the prefilter and  the BRDF lut combining them together as per the Split-Sum approximation to get IBL specular part
	const float MAX_REFLECTION_LOD = 4.0;
	vec3 prefilteredColor = textureLod(prefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;
	vec2 envBRDF = texture(brdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
	vec3 specular = prefilteredColor * (F * envBRDF.x + envBRDF.y);

	//vec3 ambient = vec3(0.03) * albedo * ao;
	vec3 ambient = (kD * diffuse + specular) * ao;
	vec3 color = ambient + Lo;

	color = color / (color + vec3(1.0)); // HDR tonemapping
	color = pow(color, vec3(1.0 / 2.2)); // gamma correction

	FragColor = (lightSource ? vec4(lightColors[0], 1.0) : vec4(color, 1.0));
	//FragColor = vec4(color, 1.0);
};

vec3 getNormalFromMap()
{
	vec3 tangentNormal = texture(normalMap, fs_in.TexCoords).xyz * 2.0 - 0.5;

	vec3 Q1 = dFdx(fs_in.WorldPos);
	vec3 Q2 = dFdy(fs_in.WorldPos);
	vec2 st1 = dFdx(fs_in.TexCoords);
	vec2 st2 = dFdy(fs_in.TexCoords);

	vec3 N = normalize(fs_in.Normal);
	vec3 T = normalize(Q1 * st2.t - Q2 * st1.t);
	vec3 B = -normalize(cross(N, T));
	mat3 TBN = mat3(T, B, N);

	return normalize(TBN * tangentNormal);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH = max(dot(N, H), 0.0);
	float NdotH2 = NdotH * NdotH;

	float nom = a2;
	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom = PI * denom * denom;

	return nom / denom; // prevent divide by 0 for (roughness = 0.0) and (NdotH = 1.0)
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r * r) / 8.0;

	float nom = NdotV;
	float denom = NdotV * (1.0 - k) + k;

	return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float ggx2 = GeometrySchlickGGX(NdotV, roughness);
	float ggx1 = GeometrySchlickGGX(NdotL, roughness);

	return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}