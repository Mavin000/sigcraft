#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

struct RayPayload {
	vec3 hitValue;
	vec3 rayDir;
};

layout(location = 0) rayPayloadInEXT RayPayload payload;

layout(binding = 2, set = 0) uniform Ubo
{
	mat4 viewInverse;
	mat4 projInverse;
	float time;
} ubo;

struct DescriptorStuff {
    uint64_t vertexAddress;
    uint64_t indexAddress;
    uint64_t vertexDataAdress;
    uint vertexOffset;
    vec3 chunkOffset;
};

layout(scalar, set = 0, binding = 3) readonly buffer DescriptorBuffer {
    DescriptorStuff descriptors[];
};

void main()
{
    float angle = ubo.time * 2.0 * 3.14159265359;

    vec3 emitter = normalize(vec3(0.0, sin(angle), cos(angle)));
    float emitterRadius = 0.05;
    float cosTheta = dot(normalize(payload.rayDir), emitter);


    if (cosTheta > cos(emitterRadius)) {
        payload.hitValue = vec3(1.0, 0.95, 0.6) * 2.0;
    } else {
        float t = 0.5 * (payload.rayDir.y + 1.0);
        payload.hitValue = mix(vec3(0.2, 0.3, 0.8), vec3(0.6, 0.8, 1.0), t);
    }
    
}