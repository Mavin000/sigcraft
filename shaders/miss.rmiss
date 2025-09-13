#version 460
#extension GL_EXT_ray_tracing : enable

layout(location=0) rayPayloadInEXT vec3 hitValue;
layout(location=1) rayPayloadInEXT vec3 rayDir;

void main()
{
    hitValue = rayDir;
    //hitValue = vec3(0.5, 0.7, 1.0);
}