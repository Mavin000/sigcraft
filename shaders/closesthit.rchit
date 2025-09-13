#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require
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
};

layout(scalar, set = 0, binding = 3) readonly buffer DescriptorBuffer {
    DescriptorStuff descriptors[];
};


struct RayPayload {
	vec3 hitValue;
	vec3 rayDir;
};

layout(location = 0) rayPayloadInEXT RayPayload payload;

void main()
{

    DescriptorStuff desc = descriptors[gl_InstanceID];
    IndexBuffer ibuf = IndexBuffer(desc.indexAddress);
    VertexBuffer vbuf = VertexBuffer(desc.vertexAddress);
    VertexDataBuffer dataBuf = VertexDataBuffer(desc.vertexDataAdress);

    uint i0 = ibuf.indices[3 * gl_PrimitiveID + 0] + desc.vertexOffset;
    uint i1 = ibuf.indices[3 * gl_PrimitiveID + 1] + desc.vertexOffset;
    uint i2 = ibuf.indices[3 * gl_PrimitiveID + 2] + desc.vertexOffset;



    Vertex v = dataBuf.verticesData[i0];

    vec3 normal = normalize(vec3(float(v.nnx), float(v.nny), float(v.nnz)) / 255.0 * 2.0 - 1.0);
    float ndotl = max(abs(dot(normal, -payload.rayDir)), 0.5);
    payload.hitValue = vec3(float(v.br) / 255.0, float(v.bg) / 255.0, float(v.bb) / 255.0) * ndotl;


}
