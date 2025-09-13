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
    int16_t vx; int16_t vy; int16_t vz;
    uint8_t tt;
    uint8_t ss;
    uint8_t nnx; uint8_t nny; uint8_t nnz;
    uint8_t pad;
    uint8_t br; uint8_t bg; uint8_t bb;
    uint8_t pad2;
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


layout(location=0) rayPayloadInEXT vec3 hitValue;
layout(location=1) rayPayloadInEXT vec3 rayDir;

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
    hitValue = vec3(float(v.br) / 255.0, float(v.bg) / 255.0, float(v.bb) / 255.0);


}
