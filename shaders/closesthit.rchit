#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require



hitAttributeEXT vec2 attribs;

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
};

layout(scalar, set = 0, binding = 3) readonly buffer DescriptorBuffer {
    DescriptorStuff descriptors[];
};


struct RayPayload {
	vec3 hitValue;
	vec3 rayDir;
};

layout(location = 0) rayPayloadInEXT RayPayload payload;

struct ShadowPayload {
    bool occluded;
};

layout(location = 1) rayPayloadEXT ShadowPayload shadowPayload;

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

    vec3 objectPos = bary.x * p1 + bary.y * p2 + bary.z * p0;

    payload.hitValue = clamp((objectPos + vec3(desc.chunkOffset)) / 64.0, 0.0, 1.0);
}



    //Vertex data = dataBuf.verticesData[i0];

    //vec3 normal = normalize(vec3(float(data.nnx), float(data.nny), float(data.nnz)) / 255.0 * 2.0 - 1.0);
    //float ndotl = max(abs(dot(normal, -payload.rayDir)), 0.5);