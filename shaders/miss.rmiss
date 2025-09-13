#version 460
#extension GL_EXT_ray_tracing : enable

struct RayPayload {
	vec3 hitValue;
	vec3 rayDir;
};

layout(location = 0) rayPayloadInEXT RayPayload payload;

void main()
{
    float t = 0.5 * (payload.rayDir.y + 1.0);

    payload.hitValue = mix(vec3(0.2, 0.3, 0.8), vec3(0.6, 0.8, 1.0), t);
}