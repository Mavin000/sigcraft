#ifndef SIGCRAFT_CHUNK_MESH_H
#define SIGCRAFT_CHUNK_MESH_H

#include "imr/imr.h"

#include <cstddef>

struct ChunkNeighbors {
    const ChunkData *neighbours[3][3];
};

struct BlockTextureMapping {
    virtual std::pair<uint8_t, uint8_t> get_block_texture(BlockId, BlockFace) = 0;
};


struct ChunkMesh
{
    std::unique_ptr<imr::Buffer> buf;
    std::unique_ptr<imr::Buffer> iBuf;
    std::unique_ptr<imr::Buffer> vertexAttributesBuf;
    size_t num_verts;

    ChunkMesh(imr::Device&, ChunkNeighbors& n, BlockTextureMapping& mapping);

    struct Vertex
    {
        int16_t vx, vy, vz;
        uint8_t tt;
        uint8_t ss;
        uint8_t nnx, nny, nnz;
        uint8_t tex_info;
        uint8_t br, bg, bb, tex_id;
    };

    static_assert(sizeof(Vertex) == sizeof(uint8_t) * 16);

    struct uglyVertex
    {
        uint8_t tt;
        uint8_t ss;
        uint8_t tex_id;
        uint8_t tex_info;
    };

};

#endif
