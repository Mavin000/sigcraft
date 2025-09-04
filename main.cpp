#include "imr/imr.h"
#include "imr/util.h"

#include "world.h"

#include <cmath>
#include "nasl/nasl.h"
#include "nasl/nasl_mat.h"

#include "camera.h"
#include <iostream>

using namespace nasl;

// test anything

struct Face
{
    vec3 v0, v1, v2, v3;
};

struct Tri
{
    uint32_t i0, i1, i2;
};

struct Cube
{
    Face faces[6];
    Tri tris[6][2];
};

Cube make_cube()
{
    /*
     *  +Y
     *  ^
     *  |
     *  |
     *  D------C.
     *  |\     |\
     *  | H----+-G
     *  | |    | |
     *  A-+----B | ---> +X
     *   \|     \|
     *    E------F
     *     \
     *      \
     *       \
     *        v +Z
     *
     * Adapted from
     * https://www.asciiart.eu/art-and-design/geometries
     */
    vec3 A = {0, 0, 0};
    vec3 B = {1, 0, 0};
    vec3 C = {1, 1, 0};
    vec3 D = {0, 1, 0};
    vec3 E = {0, 0, 1};
    vec3 F = {1, 0, 1};
    vec3 G = {1, 1, 1};
    vec3 H = {0, 1, 1};

    int i = 0;
    Cube cube = {};

    auto add_face = [&](vec3 v0, vec3 v1, vec3 v2, vec3 v3, vec3 color)
    {
        cube.faces[i] = {v0, v1, v2, v3};
        /*
         * v0 --- v3
         *  |   / |
         *  |  /  |
         *  | /   |
         * v1 --- v2
         */
        uint32_t bi = i * 4;
        cube.tris[i][0] = {bi + 0, bi + 1, bi + 3};
        cube.tris[i][1] = {bi + 1, bi + 2, bi + 3};
        i++;
    };

    // top face
    add_face(H, D, C, G, vec3(0, 1, 0));
    // north face
    add_face(A, B, C, D, vec3(1, 0, 0));
    // west face
    add_face(A, D, H, E, vec3(0, 0, 1));
    // east face
    add_face(F, G, C, B, vec3(1, 0, 1));
    // south face
    add_face(E, H, G, F, vec3(0, 1, 1));
    // bottom face
    add_face(E, F, B, A, vec3(1, 1, 0));
    assert(i == 6);
    return cube;
}

// Test anything

uint16_t width = 1024;
uint16_t height = 1024;

struct
{
    mat4 matrix;
    ivec3 chunk_position;
    float time;
} push_constants;

struct UniformData
{
    mat4 viewInverse;
    mat4 projInverse;
} uniformData;

Camera camera;
CameraFreelookState camera_state = {
    .fly_speed = 100.0f,
    .mouse_sensitivity = 1,
};
CameraInput camera_input;

void camera_update(GLFWwindow *, CameraInput *input);

bool reload_shaders = false;
std::unique_ptr<imr::Buffer> ubo;
std::unique_ptr<imr::Image> storage_image;
std::vector<std::tuple<VkTransformMatrixKHR, imr::AccelerationStructure *>> instances;

struct Shaders
{

    std::unique_ptr<imr::RayTracingPipeline> pipeline;
    std::unique_ptr<imr::AccelerationStructure> bottomLevelAS;
    std::unique_ptr<imr::AccelerationStructure> topLevelAS;

    std::unique_ptr<imr::Buffer> vertexBuffer;
    std::unique_ptr<imr::Buffer> indexBuffer;
    uint32_t indexCount;

    Shaders(imr::Device &d, imr::Swapchain &swapchain)
    {
        // test
        auto cube = make_cube();

        indexCount = static_cast<uint32_t>(0);
        // TODO add vertices and points
        vertexBuffer = std::make_unique<imr::Buffer>(d, sizeof(cube.faces),
                                                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &cube.faces);

        indexBuffer = std::make_unique<imr::Buffer>(d, sizeof(cube.tris),
                                                    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &cube.tris);

        bottomLevelAS = std::make_unique<imr::AccelerationStructure>(d);
        std::vector<imr::AccelerationStructure::TriangleGeometry> geometries;
        for (int i = 0; i < 6; i++)
        {
            // Setup identity transform matrix
            VkTransformMatrixKHR transformMatrix = {
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
            };
            geometries.push_back({vertexBuffer->device_address(), indexBuffer->device_address() + i * sizeof(cube.tris[i]), 24, 2, transformMatrix});
        }

        bottomLevelAS->createBottomLevelAccelerationStructure(geometries);
        topLevelAS = std::make_unique<imr::AccelerationStructure>(d);

        VkTransformMatrixKHR transformMatrix = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
        };
        instances.emplace_back(transformMatrix, &*bottomLevelAS);

        topLevelAS->createTopLevelAccelerationStructure(instances);

        std::vector<imr::RayTracingPipeline::RT_Shader> shader;
        shader.push_back({imr::RayTracingPipeline::ShaderType::raygen, "raygen.rgen"});
        shader.push_back({imr::RayTracingPipeline::ShaderType::miss, "miss.rmiss"});
        shader.push_back({imr::RayTracingPipeline::ShaderType::closestHit, "closesthit.rchit"});

        pipeline = std::make_unique<imr::RayTracingPipeline>(d, shader);

        storage_image = std::make_unique<imr::Image>(d, VK_IMAGE_TYPE_2D, (VkExtent3D){width, height, 1}, swapchain.format(), (VkImageUsageFlagBits)(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT));
        ubo = std::make_unique<imr::Buffer>(d, sizeof(uniformData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    }
};

int main(int argc, char **argv)
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwCreateWindow(1024, 1024, "Example", nullptr, nullptr);

    if (argc < 2)
        return 0;

    glfwSetKeyCallback(window, [](GLFWwindow *window, int key, int scancode, int action, int mods)
                       {
        if (key == GLFW_KEY_R && (mods & GLFW_MOD_CONTROL))
            reload_shaders = true; });

    imr::Context context;
    VkPhysicalDeviceBufferDeviceAddressFeatures enabledBufferDeviceAddresFeatures{};
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR enabledRayTracingPipelineFeatures{};
    VkPhysicalDeviceAccelerationStructureFeaturesKHR enabledAccelerationStructureFeatures{};
    std::unique_ptr<imr::Device> device = std::make_unique<imr::Device>(context, [&](vkb::PhysicalDeviceSelector &selector)
                                                                        {
            selector.add_required_extension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
            selector.add_required_extension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);

            // Required by VK_KHR_acceleration_structure
            selector.add_required_extension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
            selector.add_required_extension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
            selector.add_required_extension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);

            // Required for VK_KHR_ray_tracing_pipeline
            selector.add_required_extension(VK_KHR_SPIRV_1_4_EXTENSION_NAME);

            // Required by VK_KHR_spirv_1_4
            selector.add_required_extension(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);

            enabledBufferDeviceAddresFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
            enabledBufferDeviceAddresFeatures.bufferDeviceAddress = VK_TRUE;
            selector.add_required_extension_features(enabledBufferDeviceAddresFeatures);

            enabledRayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
            enabledRayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
            selector.add_required_extension_features(enabledRayTracingPipelineFeatures);

            enabledAccelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
            enabledAccelerationStructureFeatures.accelerationStructure = VK_TRUE;
            //enabledAccelerationStructureFeatures.accelerationStructureHostCommands = VK_TRUE;
            selector.add_required_extension_features(enabledAccelerationStructureFeatures); });

    imr::Swapchain swapchain(*device, window);
    imr::FpsCounter fps_counter;

    auto world = World(argv[1]);

    auto prev_frame = imr_get_time_nano();
    float delta = 0;

    camera = {{0, 0, 3}, {0, 0}, 60};

    auto shaders = std::make_unique<Shaders>(*device, swapchain);

    auto &vk = device->dispatch;

    while (!glfwWindowShouldClose(window))
    {
        fps_counter.tick();
        fps_counter.updateGlfwWindowTitle(window);

        uniformData.projInverse = invert_mat4(camera_get_proj_mat4(&camera, storage_image->size().width, storage_image->size().height));
        uniformData.viewInverse = invert_mat4(camera_get_pure_view_mat4(&camera));
        ubo->uploadDataSync(0, sizeof(uniformData), &uniformData);

        swapchain.renderFrameSimplified([&](imr::Swapchain::SimplifiedRenderContext &context) 
                                        {
            auto cmdbuf = context.cmdbuf();
            camera_update(window, &camera_input);
            camera_move_freelook(&camera, &camera_input, &camera_state, delta);

            if (reload_shaders) {
                swapchain.drain();
                shaders = std::make_unique<Shaders>(*device, swapchain);
                reload_shaders = false;
            }

            if (!storage_image || storage_image->size().width != context.image().size().width || 
                        storage_image->size().height != context.image().size().height) {

                VkImageUsageFlagBits imageFlags = static_cast<VkImageUsageFlagBits>(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

                storage_image = std::make_unique<imr::Image>(*device, VK_IMAGE_TYPE_2D, context.image().size(), swapchain.format(), imageFlags);
                device->dispatch.cmdPipelineBarrier2KHR(cmdbuf, tmpPtr((VkDependencyInfo) {
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
                                    .image = storage_image->handle(),
                                    .subresourceRange = storage_image->whole_image_subresource_range()
                                    })
                            }));
                }

            // update the push constant data on the host...
            mat4 m = identity_mat4;
            mat4 flip_y = identity_mat4;
            flip_y.rows[1][1] = -1;
            m = m * flip_y;
            mat4 view_mat = camera_get_view_mat4(&camera, context.image().size().width, context.image().size().height);
            m = m * view_mat;
            m = m * translate_mat4(vec3(-0.5, -0.5f, -0.5f));

            auto& pipeline = shaders->pipeline;

            push_constants.time = ((imr_get_time_nano() / 1000) % 10000000000) / 1000000.0f;
            push_constants.matrix = m;

            auto load_chunk = [&](int cx, int cz) {
                Chunk* loaded = world.get_loaded_chunk(cx, cz);
                if (!loaded){
                    world.load_chunk(cx, cz);
                }
                loaded = world.get_loaded_chunk(cx, cz);
                assert(loaded);

                if (loaded->mesh)
                    return;

                // std::cout << "loading " << cx << ", " << cz << std::endl;
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
                if (all_neighbours_loaded) {
                    loaded->mesh = std::make_unique<ChunkMesh>(*device, n);

                    VkTransformMatrixKHR transformMatrix = {
                        1.0f, 0.0f, 0.0f, float(cx *32),
                        0.0f, 1.0f, 0.0f, 0.0f,
                        0.0f, 0.0f, 1.0f, float(cx *32),
                    };

                    std::vector<imr::AccelerationStructure::TriangleGeometry> geometry = {{loaded->mesh->buf->device_address(), loaded->mesh->iBuf->device_address(),
                                                                                           loaded->mesh->num_verts, static_cast<uint32_t>(loaded->mesh->num_verts * 3),
                                                                                           transformMatrix}};
                    if (!loaded->accel) {
                        loaded->accel = std::make_unique<imr::AccelerationStructure>(*device);
                        loaded->accel->createBottomLevelAccelerationStructure(geometry);
                    }
                    
                    instances.emplace_back(transformMatrix, loaded->accel.get());
                }
            };

            int player_chunk_x = camera.position.x / 16;
            int player_chunk_z = camera.position.z / 16;

            int radius = 3;
            for (int dx = -radius; dx <= radius; dx++) {
                for (int dz = -radius; dz <= radius; dz++) {
                    load_chunk(player_chunk_x + dx, player_chunk_z + dz);
                }
            }

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
                if (!mesh || mesh->num_verts == 0) {
                    // std::cout << "chunk with 0 vertices" << std::endl;
                    continue;
                }

                // push_constants.chunk_position = { chunk->cx, 0, chunk->cz };
                // vkCmdPushConstants(cmdbuf, pipeline->layout(), VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 0, sizeof(push_constants), &push_constants);

                // vkCmdBindVertexBuffers(cmdbuf, 0, 1, &mesh->buf->handle, tmpPtr((VkDeviceSize) 0));

                // assert(mesh->num_verts > 0);
                // vkCmdDraw(cmdbuf, mesh->num_verts, 1, 0, 0);
            }

            // recreate topLevelAS every frame using the chunks that are in instances
            // TODO: probably reset 'instances' every frame as well
            // since the bottomLevelAS are stored in the chunks, 
            // those do not need to be recreated
            shaders->topLevelAS = std::make_unique<imr::AccelerationStructure>(*device);
            shaders->topLevelAS->createTopLevelAccelerationStructure(instances);

            auto &image = context.image();

            /*
               Dispatch the ray tracing commands
               */
            vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline->pipeline());

            auto bind_helper = pipeline->create_bind_helper();
            bind_helper->set_acceleration_structure(0, 0, *shaders->topLevelAS);
            bind_helper->set_storage_image(0, 1, *storage_image);
            bind_helper->set_uniform_buffer(0, 2, *ubo);
            bind_helper->commit(cmdbuf);

            context.addCleanupAction([=, &device]()
                    { delete bind_helper; });

            auto setImageLayout = [&](imr::Image &image, VkImageLayout new_layout, VkImageLayout old_layout = VK_IMAGE_LAYOUT_UNDEFINED)
            {
                vkCmdPipelineBarrier2(cmdbuf, tmpPtr((VkDependencyInfo){
                            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                            .dependencyFlags = 0,
                            .imageMemoryBarrierCount = 1,
                            .pImageMemoryBarriers = tmpPtr((VkImageMemoryBarrier2){
                                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                    .srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                    .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
                                    .dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                    .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
                                    .oldLayout = old_layout,
                                    .newLayout = new_layout,
                                    .image = image.handle(),
                                    .subresourceRange = image.whole_image_subresource_range()})}));
            };

            // Transition ray tracing output image back to general layout
            setImageLayout(
                    *storage_image,
                    VK_IMAGE_LAYOUT_GENERAL);

            pipeline->traceRays(cmdbuf, storage_image->size().width, storage_image->size().height);

            /*
               Copy ray tracing output to swap chain image
               */

            // Prepare current swap chain image as transfer destination
            setImageLayout(context.image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            // Prepare ray tracing output image as transfer source
            setImageLayout(*storage_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

            VkImageCopy copyRegion{};
            copyRegion.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            copyRegion.srcOffset = {0, 0, 0};
            copyRegion.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            copyRegion.dstOffset = {0, 0, 0};
            copyRegion.extent = {storage_image->size().width, storage_image->size().height, 1};
            vkCmdCopyImage(cmdbuf, storage_image->handle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image.handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

            // Transition swap chain image back for presentation
            setImageLayout(
                    context.image(),
                    VK_IMAGE_LAYOUT_GENERAL,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            auto now = imr_get_time_nano();
            delta = ((float)((now - prev_frame) / 1000L)) / 1000000.0f;
            prev_frame = now;

            glfwPollEvents(); 
        });
        vk.deviceWaitIdle();
    }
    swapchain.drain();
    return 0;
}
