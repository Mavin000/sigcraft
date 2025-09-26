extern "C"
{

#include "enklume/block_data.h"
}

#include "chunk_mesh.h"
#include "nasl/nasl.h"

#include <assert.h>
#include <vector>

#define MINUS_X_FACE(V) \
V(0, 0, 0, 0, 0, -1, 0, 0) \
V(0, 1, 0, 0, 1, -1, 0, 0) \
V(0, 1, 1, 1, 1, -1, 0, 0) \
V(0, 0, 0, 0, 0, -1, 0, 0) \
V(0, 1, 1, 1, 1, -1, 0, 0) \
V(0, 0, 1, 1, 0, -1, 0, 0)

#define PLUS_X_FACE(V) \
V(1, 1, 0, 1, 1, 1, 0, 0) \
V(1, 0, 0, 1, 0, 1, 0, 0) \
V(1, 1, 1, 0, 1, 1, 0, 0) \
V(1, 1, 1, 0, 1, 1, 0, 0) \
V(1, 0, 0, 1, 0, 1, 0, 0) \
V(1, 0, 1, 0, 0, 1, 0, 0)

#define MINUS_Z_FACE(V) \
V(1, 1, 0, 0, 1, 0, 0, -1) \
V(0, 0, 0, 1, 0, 0, 0, -1) \
V(1, 0, 0, 0, 0, 0, 0, -1) \
V(0, 1, 0, 1, 1, 0, 0, -1) \
V(0, 0, 0, 1, 0, 0, 0, -1) \
V(1, 1, 0, 0, 1, 0, 0, -1)

#define PLUS_Z_FACE(V) \
V(1, 0, 1, 1, 0, 0, 0, 1) \
V(0, 0, 1, 0, 0, 0, 0, 1) \
V(1, 1, 1, 1, 1, 0, 0, 1) \
V(1, 1, 1, 1, 1, 0, 0, 1) \
V(0, 0, 1, 0, 0, 0, 0, 1) \
V(0, 1, 1, 0, 1, 0, 0, 1)

#define MINUS_Y_FACE(V) \
V(0, 0, 0, 0, 0, 0, -1, 0) \
V(1, 0, 1, 1, 1, 0, -1, 0) \
V(1, 0, 0, 1, 0, 0, -1, 0) \
V(1, 0, 1, 1, 1, 0, -1, 0) \
V(0, 0, 0, 0, 0, 0, -1, 0) \
V(0, 0, 1, 0, 1, 0, -1, 0)

#define PLUS_Y_FACE(V) \
V(1, 1, 1, 1, 0, 0, 1, 0) \
V(0, 1, 0, 0, 1, 0, 1, 0) \
V(1, 1, 0, 1, 1, 0, 1, 0) \
V(0, 1, 1, 0, 0, 0, 1, 0) \
V(0, 1, 0, 0, 1, 0, 1, 0) \
V(1, 1, 1, 1, 0, 0, 1, 0)

#define CUBE(V)     \
    MINUS_X_FACE(V) \
    PLUS_X_FACE(V)  \
    MINUS_Z_FACE(V) \
    PLUS_Z_FACE(V)  \
    MINUS_Y_FACE(V) \
    PLUS_Y_FACE(V)

// #define V(cx, cy, cz, t, s, nx, ny, nz) tmp[0] = (int) ((cx + 1) / 2) + (float) x; tmp[1] = (int) ((cy + 1) / 2) + (float) y; tmp[2] = (int) ((cz + 1) / 2) + (float) z; tmp[3] = t; tmp[4] = s; g.push_back(tmp[0]); g.push_back(tmp[1]); g.push_back(tmp[2]); g.push_back(tmp[3]); g.push_back(tmp[4]);
#define V(cx, cy, cz, t, s, nx, ny, nz) \
v.vx = cx + x;             \
v.vy = cy + y;             \
v.vz = cz + z;             \
v.tt = t * 255;             \
v.ss = s * 255;             \
v.nnx = nx * 127 + 128;            \
v.nny = ny * 127 + 128;            \
v.nnz = nz * 127 + 128;            \
v.br = color.x * 255;            \
v.bg = color.y * 255;            \
v.bb = color.z * 255;            \
v.tex_id = tid;           \
v.tex_info = tex_info;       \
add_vertex();

static void paste_minus_x_face(std::vector<uint8_t>& g, nasl::vec3 color, unsigned x, unsigned y, unsigned z, std::tuple <uint8_t, uint8_t> tex) {
    auto [tid, tex_info] = tex;
    ChunkMesh::Vertex v;
    auto add_vertex = [&]()
    {
        uint8_t tmp[sizeof(v)];
        memcpy(tmp, &v, sizeof(v));
        for (auto b : tmp)
            g.push_back(b);
    };
    MINUS_X_FACE(V)
}

static void paste_plus_x_face(std::vector<uint8_t>& g, nasl::vec3 color, unsigned x, unsigned y, unsigned z, std::tuple <uint8_t, uint8_t> tex) {
    auto [tid, tex_info] = tex;
    ChunkMesh::Vertex v;
    auto add_vertex = [&]()
    {
        uint8_t tmp[sizeof(v)];
        memcpy(tmp, &v, sizeof(v));
        for (auto b : tmp)
            g.push_back(b);
    };
    PLUS_X_FACE(V)
}

static void paste_minus_y_face(std::vector<uint8_t>& g, nasl::vec3 color, unsigned x, unsigned y, unsigned z, std::tuple <uint8_t, uint8_t> tex) {
    auto [tid, tex_info] = tex;
    ChunkMesh::Vertex v;
    auto add_vertex = [&]()
    {
        uint8_t tmp[sizeof(v)];
        memcpy(tmp, &v, sizeof(v));
        for (auto b : tmp)
            g.push_back(b);
    };
    MINUS_Y_FACE(V)
}

static void paste_plus_y_face(std::vector<uint8_t>& g, nasl::vec3 color, unsigned x, unsigned y, unsigned z, std::tuple <uint8_t, uint8_t> tex) {
    auto [tid, tex_info] = tex;
    ChunkMesh::Vertex v;
    auto add_vertex = [&]()
    {
        uint8_t tmp[sizeof(v)];
        memcpy(tmp, &v, sizeof(v));
        for (auto b : tmp)
            g.push_back(b);
    };
    PLUS_Y_FACE(V)
}

static void paste_minus_z_face(std::vector<uint8_t>& g, nasl::vec3 color, unsigned x, unsigned y, unsigned z, std::tuple <uint8_t, uint8_t> tex) {
    auto [tid, tex_info] = tex;
    ChunkMesh::Vertex v;
    auto add_vertex = [&]()
    {
        uint8_t tmp[sizeof(v)];
        memcpy(tmp, &v, sizeof(v));
        for (auto b : tmp)
            g.push_back(b);
    };
    MINUS_Z_FACE(V)
}

static void paste_plus_z_face(std::vector<uint8_t>& g, nasl::vec3 color, unsigned x, unsigned y, unsigned z, std::tuple <uint8_t, uint8_t> tex) {
    auto [tid, tex_info] = tex;
    float tmp[5];
    ChunkMesh::Vertex v;
    auto add_vertex = [&]()
    {
        uint8_t tmp[sizeof(v)];
        memcpy(tmp, &v, sizeof(v));
        for (auto b : tmp)
            g.push_back(b);
    };
    PLUS_Z_FACE(V)
}

#undef V

static BlockData access_safe(const ChunkData *chunk, ChunkNeighbors &neighbours, int x, int y, int z)
{
    unsigned int i, k;
    if (x < 0)
    {
        i = 0;
    }
    else if (x < CUNK_CHUNK_SIZE)
    {
        i = 1;
    }
    else
    {
        i = 2;
    }

    if (y < 0 || y > CUNK_CHUNK_MAX_HEIGHT)
        return BlockAir;

    if (z < 0)
    {
        k = 0;
    }
    else if (z < CUNK_CHUNK_SIZE)
    {
        k = 1;
    }
    else
    {
        k = 2;
    }
    // assert(!neighbours || neighbours[1][1][1] == chunk);
    if (i == 1 && k == 1)
    {
        return chunk_get_block_data(chunk, x, y, z);
    }
    else
    {
        if (neighbours.neighbours[i][k])
            return chunk_get_block_data(neighbours.neighbours[i][k], x & 15, y, z & 15);
    }

    return BlockAir;
}

void chunk_mesh(const ChunkData* chunk, ChunkNeighbors& neighbours, std::vector<uint8_t>& g, size_t* num_verts, BlockTextureMapping& mapping) {
    auto shouldAddFace = [&](ChunkData const* c, int x, int y, int z, BlockData currentBlock) {
        return access_safe(c, neighbours, x, y, z) == BlockAir || (currentBlock != BlockWater && access_safe(c, neighbours, x, y, z) == BlockWater)|| (currentBlock != BlockLeaves && access_safe(c, neighbours, x, y, z) == BlockLeaves);
    };

    *num_verts = 0;
    for (int section = 0; section < CUNK_CHUNK_SECTIONS_COUNT; section++)
    {
        for (int x = 0; x < CUNK_CHUNK_SIZE; x++)
            for (int y = 0; y < CUNK_CHUNK_SIZE; y++)
                for (int z = 0; z < CUNK_CHUNK_SIZE; z++)
                {
                    int world_y = y + section * CUNK_CHUNK_SIZE;
                    BlockData block_data = access_safe(chunk, neighbours, x, world_y, z);
                    if (block_data != BlockAir)
                    {
                        nasl::vec3 color;
                        color.x = block_colors[block_data].r;
                        color.y = block_colors[block_data].g;
                        color.z = block_colors[block_data].b;
                        if (shouldAddFace(chunk, x, world_y + 1, z, block_data)) {
                            paste_plus_y_face(g, color, x, world_y, z, mapping.get_block_texture(static_cast<BlockId>(block_data), TOP));
                            *num_verts += 6;
                        }
                        if (shouldAddFace(chunk, x, world_y - 1, z, block_data)) {
                            paste_minus_y_face(g, color, x, world_y, z, mapping.get_block_texture(static_cast<BlockId>(block_data), BOTTOM));
                            *num_verts += 6;
                        }

                        if (shouldAddFace(chunk, x + 1, world_y, z, block_data)) {
                            paste_plus_x_face(g, color, x, world_y, z, mapping.get_block_texture(static_cast<BlockId>(block_data), EAST));
                            *num_verts += 6;
                        }
                        if (shouldAddFace(chunk, x - 1, world_y, z, block_data)) {
                            paste_minus_x_face(g, color, x, world_y, z, mapping.get_block_texture(static_cast<BlockId>(block_data), WEST));
                            *num_verts += 6;
                        }

                        if (shouldAddFace(chunk, x, world_y, z + 1, block_data)) {
                            paste_plus_z_face(g, color, x, world_y, z, mapping.get_block_texture(static_cast<BlockId>(block_data), SOUTH));
                            *num_verts += 6;
                        }
                        if (shouldAddFace(chunk, x, world_y, z - 1, block_data)) {
                            paste_minus_z_face(g, color, x, world_y, z, mapping.get_block_texture(static_cast<BlockId>(block_data), NORTH));
                            *num_verts += 6;
                        }
                    }
                }
    }
}

ChunkMesh::ChunkMesh(imr::Device& d, ChunkNeighbors& n, BlockTextureMapping& mapping) {
    std::vector<uint8_t> g;
    chunk_mesh(n.neighbours[1][1], n, g, &num_verts, mapping);

    // fprintf(stderr, "%zu vertices, totalling %zu KiB of data\n", num_verts, num_verts * sizeof(float) * 5 / 1024);
    // fflush(stderr);

    std::vector<float> vertexPositions;
    vertexPositions.reserve(num_verts * 3);

    auto verts = reinterpret_cast<const ChunkMesh::Vertex*>(g.data());
    for (size_t i = 0; i < num_verts; i++) {
        const auto& v = verts[i];
        vertexPositions.push_back(static_cast<float>(v.vx));
        vertexPositions.push_back(static_cast<float>(v.vy));
        vertexPositions.push_back(static_cast<float>(v.vz));

    }

    size_t vertexBufferSize = vertexPositions.size() * sizeof(float);
    buf = std::make_unique<imr::Buffer>(d, vertexBufferSize,
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            );
    buf->uploadDataSync(0, vertexBufferSize, vertexPositions.data());

    std::vector<uint32_t> indexBuffer;
    for (uint32_t i = 0; i < num_verts; i++) {
        indexBuffer.push_back(i);
    }

    size_t buffer_size = sizeof(uint32_t) * num_verts;
    void* buffer = indexBuffer.data();

    iBuf = std::make_unique<imr::Buffer>(d, buffer_size,
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    iBuf->uploadDataSync(0, buffer_size, buffer);


    std::vector<uglyVertex> g2;
    g2.resize(num_verts);
    for (size_t i = 0; i < num_verts; i++) {
        const auto& v = verts[i];
        uglyVertex uv;
        uv.tt = v.tt;
        uv.ss = v.ss;
        uv.tex_id = v.tex_id;
        uv.tex_info = v.tex_info;
        g2[i] = uv;

    }

    vertexAttributesBuf = std::make_unique<imr::Buffer>(d, g2.size() * sizeof(uglyVertex),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vertexAttributesBuf->uploadDataSync(0, g2.size() * sizeof(uglyVertex), g2.data());

}

