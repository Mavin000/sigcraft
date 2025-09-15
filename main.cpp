#include "imr/imr.h"
#include "imr/util.h"

#include "world.h"

#include <cmath>
#include "nasl/nasl.h"
#include "nasl/nasl_mat.h"

#include "camera.h"

using namespace nasl;

struct {
    mat4 matrix;
    ivec3 chunk_position;
    float time;
} push_constants;

Camera camera;
CameraFreelookState camera_state = {
    .fly_speed = 100.0f,
    .mouse_sensitivity = 1,
};
CameraInput camera_input;

void camera_update(GLFWwindow*, CameraInput* input);

bool reload_shaders = false;

struct Shaders {
    std::vector<std::string> files = { "basic.vert.spv", "basic.frag.spv" };

    std::vector<std::unique_ptr<imr::ShaderModule>> modules;
    std::vector<std::unique_ptr<imr::ShaderEntryPoint>> entry_points;
    std::unique_ptr<imr::GraphicsPipeline> pipeline;

    Shaders(imr::Device& d, imr::Swapchain& swapchain) {
        imr::GraphicsPipeline::RenderTargetsState rts;
        rts.color.push_back((imr::GraphicsPipeline::RenderTarget) {
            .format = swapchain.format(),
            .blending = {
                .blendEnable = false,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
            }
        });
        imr::GraphicsPipeline::RenderTarget depth = {
            .format = VK_FORMAT_D32_SFLOAT
        };
        rts.depth = depth;

        VkVertexInputBindingDescription bindings[] = {
            {
                .binding = 0,
                .stride = sizeof(ChunkMesh::Vertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
            },
        };

        VkVertexInputAttributeDescription attributes[] = {
            {
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R16G16B16_SINT,
                .offset = 0,
            },
            {
                .location = 1,
                .binding = 0,
                .format = VK_FORMAT_R8G8B8_SNORM,
                .offset = offsetof(ChunkMesh::Vertex, nnx),
            },
            {
                .location = 2,
                .binding = 0,
                .format = VK_FORMAT_R8G8B8_UNORM,
                .offset = offsetof(ChunkMesh::Vertex, br),
            },
            {
                .location = 3,
                .binding = 0,
                .format = VK_FORMAT_R8G8_UNORM,
                .offset = offsetof(ChunkMesh::Vertex, tt),
            },
        };

        VkPipelineVertexInputStateCreateInfo vertex_input {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = sizeof(bindings) / sizeof(bindings[0]),
            .pVertexBindingDescriptions = &bindings[0],
            .vertexAttributeDescriptionCount = sizeof(attributes) / sizeof(attributes[0]),
            .pVertexAttributeDescriptions = &attributes[0],
        };

        VkPipelineRasterizationStateCreateInfo rasterization {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,

            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .frontFace = VK_FRONT_FACE_CLOCKWISE,

            .lineWidth = 1.0f,
        };

        imr::GraphicsPipeline::StateBuilder stateBuilder = {
            .vertexInputState = vertex_input,
            .inputAssemblyState = imr::GraphicsPipeline::simple_triangle_input_assembly(),
            .viewportState = imr::GraphicsPipeline::one_dynamically_sized_viewport(),
            .rasterizationState = rasterization,
            .multisampleState = imr::GraphicsPipeline::one_spp(),
            .depthStencilState = imr::GraphicsPipeline::simple_depth_testing(),
        };

        std::vector<imr::ShaderEntryPoint*> entry_point_ptrs;
        for (auto filename : files) {
            VkShaderStageFlagBits stage;
            if (filename.ends_with("vert.spv"))
                stage = VK_SHADER_STAGE_VERTEX_BIT;
            else if (filename.ends_with("frag.spv"))
                stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            else
                throw std::runtime_error("Unknown suffix");
            modules.push_back(std::make_unique<imr::ShaderModule>(d, std::move(filename)));
            entry_points.push_back(std::make_unique<imr::ShaderEntryPoint>(*modules.back(), stage, "main"));
            entry_point_ptrs.push_back(entry_points.back().get());
        }
        pipeline = std::make_unique<imr::GraphicsPipeline>(d, std::move(entry_point_ptrs), rts, stateBuilder);
    }
};

#include "load_png.h"
#include <filesystem>

struct Textures {
    void load_texture(std::string name, std::unique_ptr<imr::Image>& dst) {
        auto& vk = _device.dispatch;
        const char* loc = imr_get_executable_location();
        std::filesystem::path path = std::filesystem::path(loc).parent_path();
        while (true) {
            if (std::filesystem::exists(path.string() + "/" + name))
                break;
            auto parent = path.parent_path();
            if (parent == path) {
                throw std::runtime_error("failed to find path");
            } else {
                path = parent;
            }
        }
        auto img = load_png(path.string() + "/" + name);
        assert(img);
        dst = std::make_unique<imr::Image>(_device, VK_IMAGE_TYPE_2D, VkExtent3D({ img->width, img->height, 1}), VK_FORMAT_R8G8B8A8_UNORM, VkImageUsageFlagBits(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT));

        size_t size = img->width * img->height * 4;
        auto staging = imr::Buffer(_device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        staging.uploadDataSync(0, size, img->pixels.get());

        _device.executeCommandsSync([&](VkCommandBuffer cmdbuf) {
            vk.cmdPipelineBarrier2KHR(cmdbuf, tmpPtr((VkDependencyInfo) {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .dependencyFlags = 0,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = tmpPtr((VkImageMemoryBarrier2) {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                    .srcStageMask = 0,
                    .srcAccessMask = 0,
                    .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                    .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
                    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                    .image = dst->handle(),
                    .subresourceRange = dst->whole_image_subresource_range()
                })
            }));

            vkCmdCopyBufferToImage2(cmdbuf, tmpPtr<VkCopyBufferToImageInfo2>({
                .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
                .srcBuffer = staging.handle,
                .dstImage = dst->handle(),
                .dstImageLayout = VK_IMAGE_LAYOUT_GENERAL,
                .regionCount = 1,
                .pRegions = tmpPtr<VkBufferImageCopy2>({
                    .sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
                    .bufferRowLength = (uint32_t) img->width,
                    .bufferImageHeight = (uint32_t) img->height,
                    .imageSubresource = dst->whole_image_subresource_layers(),
                    .imageExtent = dst->size(),
                }),
            }));
        });
    }

    Textures(imr::Device& d) : _device(d) {
        load_texture("textures/blocks/bricks.png", bricks);
        VkSamplerCreateInfo create_sampler = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_NEAREST,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        };
        vkCreateSampler(_device.device, &create_sampler, nullptr, &sampler);
    }

    ~Textures() {
        vkDestroySampler(_device.device, sampler, nullptr);
    }

    VkSampler sampler;

    imr::Device& _device;
    std::unique_ptr<imr::Image> bricks;
};

int main(int argc, char** argv) {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwCreateWindow(1024, 1024, "Example", nullptr, nullptr);

    if (argc < 2)
        return 0;

    glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_R && (mods & GLFW_MOD_CONTROL))
            reload_shaders = true;
    });

    imr::Context context;
    imr::Device device(context);
    imr::Swapchain swapchain(device, window);
    imr::FpsCounter fps_counter;

    auto world = World(argv[1]);

    auto prev_frame = imr_get_time_nano();
    float delta = 0;

    camera = {{0, 0, 3}, {0, 0}, 60};

    std::unique_ptr<imr::Image> depthBuffer;

    auto shaders = std::make_unique<Shaders>(device, swapchain);
    auto textures = std::make_unique<Textures>(device);

    auto& vk = device.dispatch;
    while (!glfwWindowShouldClose(window)) {
        fps_counter.tick();
        fps_counter.updateGlfwWindowTitle(window);

        swapchain.renderFrameSimplified([&](imr::Swapchain::SimplifiedRenderContext& context) {
            camera_update(window, &camera_input);
            camera_move_freelook(&camera, &camera_input, &camera_state, delta);

            if (reload_shaders) {
                swapchain.drain();
                shaders = std::make_unique<Shaders>(device, swapchain);
                reload_shaders = false;
            }

            auto& image = context.image();
            auto cmdbuf = context.cmdbuf();

            if (!depthBuffer || depthBuffer->size().width != context.image().size().width || depthBuffer->size().height != context.image().size().height) {
                VkImageUsageFlagBits depthBufferFlags = static_cast<VkImageUsageFlagBits>(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
                depthBuffer = std::make_unique<imr::Image>(device, VK_IMAGE_TYPE_2D, context.image().size(), VK_FORMAT_D32_SFLOAT, depthBufferFlags);

                vk.cmdPipelineBarrier2KHR(cmdbuf, tmpPtr((VkDependencyInfo) {
                    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                    .dependencyFlags = 0,
                    .imageMemoryBarrierCount = 1,
                    .pImageMemoryBarriers = tmpPtr((VkImageMemoryBarrier2) {
                        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                        .srcStageMask = 0,
                        .srcAccessMask = 0,
                        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
                        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                        .image = depthBuffer->handle(),
                        .subresourceRange = depthBuffer->whole_image_subresource_range()
                    })
                }));
            }

            vk.cmdClearColorImage(cmdbuf, image.handle(), VK_IMAGE_LAYOUT_GENERAL, tmpPtr((VkClearColorValue) {
                .float32 = { 0.0f, 0.0f, 0.0f, 1.0f },
            }), 1, tmpPtr(image.whole_image_subresource_range()));

            vk.cmdClearDepthStencilImage(cmdbuf, depthBuffer->handle(), VK_IMAGE_LAYOUT_GENERAL, tmpPtr((VkClearDepthStencilValue) {
                .depth = 1.0f,
                .stencil = 0,
            }), 1, tmpPtr(depthBuffer->whole_image_subresource_range()));

            // This barrier ensures that the clear is finished before we run the dispatch.
            // before: all writes from the "transfer" stage (to which the clear command belongs)
            // after: all writes from the "compute" stage
            vk.cmdPipelineBarrier2KHR(cmdbuf, tmpPtr((VkDependencyInfo) {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .dependencyFlags = 0,
                .memoryBarrierCount = 1,
                .pMemoryBarriers = tmpPtr((VkMemoryBarrier2) {
                    .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                    .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .dstStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                    .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
                })
            }));

            // update the push constant data on the host...
            mat4 m = identity_mat4;
            mat4 flip_y = identity_mat4;
            flip_y.rows[1][1] = -1;
            m = m * flip_y;
            mat4 view_mat = camera_get_view_mat4(&camera, context.image().size().width, context.image().size().height);
            m = m * view_mat;
            m = m * translate_mat4(vec3(-0.5, -0.5f, -0.5f));

            auto& pipeline = shaders->pipeline;
            vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline());

            push_constants.time = ((imr_get_time_nano() / 1000) % 10000000000) / 1000000.0f;

            context.frame().withRenderTargets(cmdbuf, { &image }, &*depthBuffer, [&]() {
                //for (auto pos : positions) {
                //    mat4 cube_matrix = m;
                //    cube_matrix = cube_matrix * translate_mat4(pos);

                //    push_constants_batched.matrix = cube_matrix;
                //    vkCmdPushConstants(cmdbuf, pipeline->layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push_constants_batched), &push_constants_batched);
                //    vkCmdDraw(cmdbuf, 12 * 3, 1, 0, 0);
                //}

                push_constants.matrix = m;

                auto load_chunk = [&](int cx, int cz) {
                    auto loaded = world.get_loaded_chunk(cx, cz);
                    if (!loaded)
                        world.load_chunk(cx, cz);
                    else {
                        if (loaded->mesh)
                            return;

                        bool all_neighbours_loaded = true;
                        ChunkNeighbors n = {};
                        for (int dx = -1; dx < 2; dx++) {
                            for (int dz = -1; dz < 2; dz++) {
                                int nx = cx + dx;
                                int nz = cz + dz;

                                auto neighborChunk = world.get_loaded_chunk(nx, nz);
                                if (neighborChunk)
                                    n.neighbours[dx + 1][dz + 1] = &neighborChunk->data;
                                else
                                    all_neighbours_loaded = false;
                            }
                        }
                        if (all_neighbours_loaded)
                            loaded->mesh = std::make_unique<ChunkMesh>(device, n);
                    }
                };

                int player_chunk_x = camera.position.x / 16;
                int player_chunk_z = camera.position.z / 16;

                int radius = 24;
                for (int dx = -radius; dx <= radius; dx++) {
                    for (int dz = -radius; dz <= radius; dz++) {
                        load_chunk(player_chunk_x + dx, player_chunk_z + dz);
                    }
                }

                auto binding_helper = pipeline->create_bind_helper();
                binding_helper->set_sampler(0, 0, textures->sampler);
                binding_helper->set_texture_image(0, 1, *textures->bricks);
                binding_helper->commit(cmdbuf);

                for (auto chunk : world.loaded_chunks()) {
                    if (abs(chunk->cx - player_chunk_x) > radius || abs(chunk->cz - player_chunk_z) > radius) {
                        std::unique_ptr<ChunkMesh> stolen = std::move(chunk->mesh);
                        if (stolen) {
                            ChunkMesh* released = stolen.release();
                            context.frame().addCleanupAction([=]() {
                                delete released;
                            });
                        }
                        world.unload_chunk(chunk);
                        continue;
                    }

                    auto& mesh = chunk->mesh;
                    if (!mesh || mesh->num_verts == 0)
                        continue;

                    push_constants.chunk_position = { chunk->cx, 0, chunk->cz };
                    vkCmdPushConstants(cmdbuf, pipeline->layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push_constants), &push_constants);

                    vkCmdBindVertexBuffers(cmdbuf, 0, 1, &mesh->buf->handle, tmpPtr((VkDeviceSize) 0));

                    assert(mesh->num_verts > 0);
                    vkCmdDraw(cmdbuf, mesh->num_verts, 1, 0, 0);
                }

                context.addCleanupAction([=]() {
                    delete binding_helper;
                });
            });

            auto now = imr_get_time_nano();
            delta = ((float) ((now - prev_frame) / 1000L)) / 1000000.0f;
            prev_frame = now;

            glfwPollEvents();
        });
    }

    swapchain.drain();
    return 0;
}
