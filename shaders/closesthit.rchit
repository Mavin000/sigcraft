#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
//#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

layout(location = 0) rayPayloadInEXT vec3 hitValue;
hitAttributeEXT vec2 attribs;


//struct Vertex {
//    vec3 pos;
//};

struct DescriptorStuff {
    //uint64_t vertexAddress;
    uint firstIndex;
    uint vertexOffset;
    vec3 color;
};

layout(scalar, set = 0, binding = 3) buffer DescriptorBuffer {
    DescriptorStuff descriptors[];
};

//layout(buffer_reference, scalar) buffer VertexBuffer {
//    Vertex vertices[];
//};

void main()
{
  //const vec3 barycentricCoords = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
  //hitValue = barycentricCoords;

  const vec3 argh = descriptors[40].color;

  hitValue = argh;

    //uint inst = gl_InstanceCustomIndexEXT;
    //DescriptorStuff desc = descriptors[inst];
    //VertexBuffer vb = VertexBuffer(desc.vertexAddress);
    //Vertex v0 = vb.vertices[desc.vertexOffset + desc.firstIndex];
    //hitValue = v0.pos;
}
