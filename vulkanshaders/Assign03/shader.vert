#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 color;
layout(location = 2) in vec3 normal;

layout(push_constant) uniform PushConstants {
	mat4 modelMat;
	float useNormalAsColor;
} pc;

layout(location = 0) out vec4 interColor;

void main()
{
	vec4 pos = vec4(position, 1.0);
	gl_Position = pc.modelMat * pos;

	mat3 normMat = transpose(inverse(mat3(pc.modelMat)));
	vec3 world_normal = vec3(normMat * normal);

	world_normal = world_normal * 0.5 + 0.5;
	vec4 color_normal = vec4(world_normal, 1.0);

	interColor = mix(color, color_normal, pc.useNormalAsColor);
}