#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require


layout(location = 0) rayPayloadInEXT vec3 hitValue;
hitAttributeEXT vec2 attribs;

struct Vertex {
    int vx; int vy; int vz;
    uint tt;
    uint ss;
    uint nnx; uint nny; uint nnz;
    uint pad;
    uint br; uint bg; uint bb;
    uint pad2;
};

layout(buffer_reference, scalar) buffer VertexBuffer {
    //Vertex vertices[];
    vec3 positions[];
};

layout(buffer_reference, scalar) buffer IndexBuffer {
    uint indices[];
};

struct DescriptorStuff {
    uint64_t vertexAddress;
    uint64_t indexAddress;
    uint firstIndex;
    uint vertexOffset;
};

layout(scalar, set = 0, binding = 3) readonly buffer DescriptorBuffer {
    DescriptorStuff descriptors[];
};


void main()
{

  DescriptorStuff desc = descriptors[gl_InstanceID];
  IndexBuffer ibuf = IndexBuffer(desc.indexAddress);
  VertexBuffer vbuf = VertexBuffer(desc.vertexAddress);

  uint i0 = ibuf.indices[desc.firstIndex + 3 * gl_PrimitiveID + 0] + desc.vertexOffset;
  uint i1 = ibuf.indices[desc.firstIndex + 3 * gl_PrimitiveID + 1] + desc.vertexOffset;
  uint i2 = ibuf.indices[desc.firstIndex + 3 * gl_PrimitiveID + 2] + desc.vertexOffset;


  //Vertex v = vbuf.vertices[gl_PrimitiveID];
  //uint64_t x = (desc.vertexAddress >> 6)%65536;
  //float y = int(x);
  //vec3 argh = vec3( y / 255.0  , y / 4096.0, y / 65536.0);
  vec3 position = vbuf.positions[i0];
  hitValue = vec3(position/16.0);

}
