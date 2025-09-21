#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require



hitAttributeEXT vec2 attribs;

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;

layout(binding = 2, set = 0) uniform Ubo
{
	mat4 viewInverse;
	mat4 projInverse;
	float time;
} ubo;

struct Vertex {
    int vx; int vy; int vz;
    uint tt;
    uint ss;
    uint nnx; uint nny; uint nnz;
    uint br; uint bg; uint bb;
    uint tex_id;
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

struct ChunkRenderingData {
    uint64_t vertexAddress;
    uint64_t indexAddress;
    uint64_t vertexDataAdress;
    uint vertexOffset;
    vec3 chunkOffset;
};

layout(scalar, set = 0, binding = 3) readonly buffer DescriptorBuffer {
    ChunkRenderingData descriptors[];
};

layout(set = 0, binding = 4)
uniform sampler nn;

layout(set = 0, binding = 5)
uniform texture2D textures[15];

struct RayPayload {
    bool isHit;
	vec3 color;
	vec3 rayDir;
    vec3 hitPos;
    vec3 hitNormal;
};

layout(location = 0) rayPayloadInEXT RayPayload payload;






void main()
{
    payload.isHit = true;

    ChunkRenderingData desc = descriptors[gl_InstanceID];
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

    payload.hitPos = origin;
    payload.hitNormal = normal;
    //vec3 albedo = texture(sampler2D(textures[nonuniformEXT(2)], nn), uv).xyz;
    vec3 albedo = vec3(float(data.br) / 255.0, float(data.bg) / 255.0, float(data.bb) / 255.0);


    payload.color = albedo * 0.2 + clamp(abs(dot(normal, normalize(vec3(0,1,0.5)))), 0.0, 0.1);// + sunlight * ndotl * albedo + sunlight * albedo * pow(clamp(dot(reflect(payload.rayDir, normal), sunDir), 0, 1), 30);

}
