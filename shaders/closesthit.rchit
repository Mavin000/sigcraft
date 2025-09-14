#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require



hitAttributeEXT vec2 attribs;

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;

struct Vertex {
    int vx; int vy; int vz;
    uint tt;
    uint ss;
    uint nnx; uint nny; uint nnz;
    uint br; uint bg; uint bb;
};

layout(buffer_reference, scalar) buffer VertexBuffer {
    vec3 positions[];
};

layout(buffer_reference, scalar) buffer IndexBuffer {
    uint indices[];
};

layout(buffer_reference, scalar) buffer VertexDataBuffer {
    Vertex verticesData[];
};

struct DescriptorStuff {
    uint64_t vertexAddress;
    uint64_t indexAddress;
    uint64_t vertexDataAdress;
    uint vertexOffset;
    vec3 chunkOffset;
    uint time;
};

layout(scalar, set = 0, binding = 3) readonly buffer DescriptorBuffer {
    DescriptorStuff descriptors[];
};


struct RayPayload {
	vec3 hitValue;
	vec3 rayDir;
};

layout(location = 0) rayPayloadInEXT RayPayload payload;

layout(location = 1) rayPayloadEXT bool shadowHitPayload;

void main()
{

    DescriptorStuff desc = descriptors[gl_InstanceID];
    IndexBuffer ibuf = IndexBuffer(desc.indexAddress);
    VertexBuffer vbuf = VertexBuffer(desc.vertexAddress);
    VertexDataBuffer dataBuf = VertexDataBuffer(desc.vertexDataAdress);

    uint i0 = ibuf.indices[3 * gl_PrimitiveID + 0] + desc.vertexOffset;
    uint i1 = ibuf.indices[3 * gl_PrimitiveID + 1] + desc.vertexOffset;
    uint i2 = ibuf.indices[3 * gl_PrimitiveID + 2] + desc.vertexOffset;

    vec3 p0 = vbuf.positions[i0];
    vec3 p1 = vbuf.positions[i1];
    vec3 p2 = vbuf.positions[i2];

    vec3 bary = vec3(attribs.x, attribs.y, 1.0 - attribs.x - attribs.y);

    Vertex data = dataBuf.verticesData[i0];
    vec3 normal = normalize(vec3(float(data.nnx), float(data.nny), float(data.nnz)) / 255.0 * 2.0 - 1.0);

    vec3 objectPos = bary.x * p1 + bary.y * p2 + bary.z * p0;
    vec3 origin = objectPos + vec3(desc.chunkOffset) + normal * 0.001;


    vec3 sunDir = normalize(vec3(0.1,0.3,0.8));

    shadowHitPayload = false;

    traceRayEXT(
        topLevelAS,
        gl_RayFlagsOpaqueEXT,
        0xFF,
        1,  
        0,  
        1,  
        origin, 0.001,
        sunDir, 10000.0,
        1
    );

    float sunlight = shadowHitPayload ? 0.0 : 1.0;

    float ndotl = dot(normal, sunDir);
    vec3 albedo = vec3(float(data.br) / 255.0, float(data.bg) / 255.0, float(data.bb) / 255.0);

    payload.hitValue = albedo * 0.2 + clamp(abs(dot(normal, normalize(vec3(0,1,0.5)))), 0.0, 0.1) + sunlight * ndotl * albedo + sunlight * albedo * pow(clamp(dot(reflect(payload.rayDir, normal), sunDir), 0, 1), 30);
    //payload.hitValue = clamp((objectPos + vec3(desc.chunkOffset)) * 64.0, 0.0, 1.0);
    //payload.hitValue.rgb = normal * 0.5 + vec3(0.5);
}

    //float ndotl = max(abs(dot(normal, -payload.rayDir)), 0.5);

