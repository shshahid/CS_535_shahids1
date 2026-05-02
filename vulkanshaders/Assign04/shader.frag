#version 450

layout(location = 0) in vec4 interColor;
layout(location = 1) in vec3 interPos;
layout(location = 2) in vec3 interNormal;

struct PointLight {
	vec4 pos;
	vec4 color;
};

layout(set = 0, binding = 1) uniform UBOFragment {
	vec4 cameraPos;
	uint lightCnt;
} ubo;

layout(set = 0, binding = 2, std430) readonly buffer SSBOLights {
	PointLight allLights[];
};

layout(location = 0) out vec4 out_color;

void main()
{
	vec3 N = normalize(interNormal);
	vec3 finalColor = vec3(0, 0, 0);

	for(int i = 0; i < ubo.lightCnt ; i++) {
		vec3 L = normalize(vec3(allLights[i].pos) - interPos);
		float diffuseComp = max(0, dot(N, L));
		vec3 diffuseColor = vec3(allLights[i].color) * vec3(interColor) * diffuseComp;
		vec3 V = normalize(vec3(ubo.cameraPos) - interPos);
		vec3 H = normalize(L + V);
		float specComp = pow(max(0, dot(N,H)), 500);
		vec3 specColor = vec3(allLights[i].color) * specComp * diffuseComp;
		finalColor += diffuseColor + specColor;
	}

	finalColor = (finalColor) / (finalColor + vec3(1.0f, 1.0f, 1.0f));
	out_color = vec4(finalColor, 1.0);
}