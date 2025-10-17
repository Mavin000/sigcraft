#include "imr/imr.h"
#include "imr/util.h"

#include "world.h"

#include <cmath>
#include "nasl/nasl.h"
#include "nasl/nasl_mat.h"

#include "camera.h"
#include <map>

using namespace nasl;





struct DescriptorPackage{
    VkDeviceAddress vertexAddress;
    VkDeviceAddress indexAddress;
    VkDeviceAddress vertexAttributesAddress;
    uint32_t vertexOffset;
    vec3 chunkOffset;
};

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
    float time;
} uniformData;

Camera camera;
CameraFreelookState camera_state = {
    .fly_speed = 100.0f,
    .mouse_sensitivity = 1,
};
CameraInput camera_input;

void camera_update(GLFWwindow *, CameraInput *input);

bool reload_shaders = false;
bool rebuildTLAS = true;
std::unique_ptr<imr::Buffer> ubo;
std::unique_ptr<imr::Image> storage_image;
std::map<std::pair<int, int>, std::tuple<VkTransformMatrixKHR, imr::AccelerationStructure *, DescriptorPackage>> loadedChunkData;
std::vector<std::tuple<VkTransformMatrixKHR, imr::AccelerationStructure *>> instances;

std::unique_ptr<imr::Buffer> descriptorBuffer;
std::vector<DescriptorPackage> globalDescriptors;



struct Shaders
{

    std::unique_ptr<imr::RayTracingPipeline> pipeline;
    std::unique_ptr<imr::AccelerationStructure> bottomLevelAS;
    std::unique_ptr<imr::AccelerationStructure> topLevelAS;

    std::unique_ptr<imr::Buffer> vertexBuffer;
    std::unique_ptr<imr::Buffer> indexBuffer;
    uint32_t indexCount;
    vec3 red = {1, 0, 0};
    std::vector<imr::AccelerationStructure::TriangleGeometry> geometries;

    Shaders(imr::Device &d, imr::Swapchain &swapchain)
    {


        bottomLevelAS = std::make_unique<imr::AccelerationStructure>(d);
        topLevelAS = std::make_unique<imr::AccelerationStructure>(d);

        VkTransformMatrixKHR transformMatrix = {
            0.001f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.001f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.001f, 0.0f,
        };

        std::vector<std::unique_ptr<imr::ShaderModule>> shader_modules;
        std::vector<std::unique_ptr<imr::ShaderEntryPoint>> entry_pts;

        auto load_shader = [&](std::string filename, std::string fn, VkShaderStageFlagBits stage) -> imr::ShaderEntryPoint* {
            imr::ShaderModule& module = *shader_modules.emplace_back(std::make_unique<imr::ShaderModule>(d, std::move(filename)));
            imr::ShaderEntryPoint& entry_point = *entry_pts.emplace_back(std::make_unique<imr::ShaderEntryPoint>(module, stage, fn));
            return &entry_point;
        };

        auto raygen = load_shader("raygen.rgen.spv", "main", VK_SHADER_STAGE_RAYGEN_BIT_KHR);
        auto anyhit = load_shader("rayanyhit.rahit.spv", "main", VK_SHADER_STAGE_ANY_HIT_BIT_KHR);
        auto hit = load_shader("closesthit.rchit.spv", "main", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
        auto shadowhit = load_shader("shadowhit.rchit.spv", "main", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
        auto shadowanyhit = load_shader("shadowanyhit.rahit.spv", "main", VK_SHADER_STAGE_ANY_HIT_BIT_KHR);
        auto miss = load_shader("miss.rmiss.spv", "main", VK_SHADER_STAGE_MISS_BIT_KHR);
        auto shadowmiss = load_shader("shadowmiss.rmiss.spv", "main", VK_SHADER_STAGE_MISS_BIT_KHR);

        std::vector<imr::RayTracingPipeline::HitShadersTriple> hits;
        hits.push_back({
            .closest_hit = hit,
            .any_hit = anyhit,
        });
        hits.push_back({
            .closest_hit = shadowhit,
            .any_hit = shadowanyhit,
        });

        std::vector<imr::ShaderEntryPoint*> misses;
        misses.emplace_back(miss);
        misses.emplace_back(shadowmiss);

        pipeline = std::make_unique<imr::RayTracingPipeline>(d, raygen, hits, misses);

        storage_image = std::make_unique<imr::Image>(d, VK_IMAGE_TYPE_2D, (VkExtent3D){width, height, 1}, swapchain.format(), (VkImageUsageFlagBits)(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT));
        ubo = std::make_unique<imr::Buffer>(d, sizeof(uniformData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        descriptorBuffer = std::make_unique<imr::Buffer>(d, sizeof(DescriptorPackage), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    }
};

#include "load_png.h"
#include <filesystem>

struct Textures {
    std::filesystem::path find_textures_path() {
        const char* loc = imr_get_executable_location();
        std::filesystem::path path = std::filesystem::path(loc).parent_path();
        while (true) {
            if (std::filesystem::exists(path.string() + "/assets/minecraft/textures"))
                return std::filesystem::path(path.string() + "/assets/minecraft/textures");
            auto parent = path.parent_path();
            if (parent == path) {
                throw std::runtime_error("failed to find path");
            } else {
                path = parent;
            }
        }
    }

    void load_all_textures() {
        auto textures_path = find_textures_path();
        auto block_textures_path = std::filesystem::path(textures_path.string() + "/block");
        size_t i = 0;
        for (auto& file : std::filesystem::directory_iterator(block_textures_path)) {
            if (file.is_regular_file() && file.path().extension() == ".png") {
                auto texture_name = file.path().stem().string();
                printf("[%zu] Loading block texture: %s\n", i, texture_name.c_str());
                //texture_name = texture_name.substr(0, texture_name.length() - 4);
                printf("Loading block texture: %s\n", texture_name.c_str());

                std::unique_ptr<imr::Image> texture;
                load_texture(file.path(), texture);
                block_textures_map[texture_name] = block_textures.size();
                block_textures.emplace_back(std::move(texture));
                i++;
            }
        }
        printf("Loaded %zu block textures\n", block_textures.size());
    }

    void load_texture(std::filesystem::path path, std::unique_ptr<imr::Image>& dst) {
        auto& vk = _device.dispatch;

        auto img = load_png(path.string());
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
        load_all_textures();
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
    std::vector<std::unique_ptr<imr::Image>> block_textures;
    std::unordered_map<std::string, int> block_textures_map;
};

int framebufferWidth = 1024;
int framebufferHeight = 1024;

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    framebufferWidth = width;
    framebufferHeight = height;
}

int main(int argc, char **argv) {

    auto startingTime = imr_get_time_nano();

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwCreateWindow(1024, 1024, "Example", nullptr, nullptr);

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (argc < 2)
        return 0;

    glfwSetKeyCallback(window, [](GLFWwindow *window, int key, int scancode, int action, int mods)
                       {
        if (key == GLFW_KEY_R && (mods & GLFW_MOD_CONTROL))
            reload_shaders = true; });

    imr::Context context;
    VkPhysicalDeviceFeatures physicalFeatures{};
    VkPhysicalDeviceBufferDeviceAddressFeatures enabledBufferDeviceAddressFeatures{};
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

            physicalFeatures.shaderInt64 = VK_TRUE;
            selector.set_required_features(physicalFeatures);

            enabledBufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
            enabledBufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;
            selector.add_required_extension_features(enabledBufferDeviceAddressFeatures);

            enabledRayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
            enabledRayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
            selector.add_required_extension_features(enabledRayTracingPipelineFeatures);

            enabledAccelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
            enabledAccelerationStructureFeatures.accelerationStructure = VK_TRUE;
            selector.add_required_extension_features(enabledAccelerationStructureFeatures); });

    imr::Swapchain swapchain(*device, window);
    imr::FpsCounter fps_counter;

    auto world = World(argv[1]);

    auto prev_frame = imr_get_time_nano();
    float delta = 0;

    camera = {{0, 150, 3}, {0, 0}, 60};

    auto shaders = std::make_unique<Shaders>(*device, swapchain);

    auto &vk = device->dispatch;
    auto textures = std::make_unique<Textures>(*device);

    struct Texturer : BlockTextureMapping {
        Texturer(Textures& textures) : _textures(textures) {}

        unsigned int id_safe(std::string texture_name) {
            auto found = _textures.block_textures_map.find(texture_name);
            //printf("Looking for texture '%s'\n", texture_name.c_str());
            
            if (found != _textures.block_textures_map.end()){
                //printf("Found texture '%s' with id %d\n", texture_name.c_str(), found->second);
                return found->second;
            }
            return 4;
        }

        std::pair<uint16_t, uint8_t> get_block_texture(BlockId id, BlockFace face) override {
            switch (id) {
                case BlockAir:break;
                case BlockStone: return {id_safe("stone"),0};
                case BlockCobbleStone: return {id_safe("cobblestone"),0};
                case BlockMossyCobbleStone: return {id_safe("mossy_cobblestone"),0};
                case BlockDirt: return {id_safe("dirt"),0};
                case BlockTallGrass:
                case BlockGrass: {
                    if (face == TOP)
                        return {id_safe("grass_block_top"),1};
                    if (face == BOTTOM)
                        return {id_safe("dirt"),0};
                    return {id_safe("grass_block_side"),0};
                }
                case BlockSand: return {id_safe("sand"),0};
                case BlockSandStone: {
                    if (face == TOP )
                        return {id_safe("sandstone_top"),0};
                    if (face == BOTTOM)
                        return {id_safe("sandstone_bottom"),0};
                    return {id_safe("sandstone"),0};
                }
                case BlockGravel: return {id_safe("gravel"),0};
                case BlockPlanks: return {id_safe("oak_planks"),0};
                case BlockBedrock: return {id_safe("bedrock"),0};
                case BlockWater: return {id_safe("water"),2};
                case BlockLeaves: return {id_safe("azalea_leaves"),0};
                case BlockWood: {
                    if (face == TOP || face == BOTTOM)
                        return {id_safe("oak_log_top"),0};
                    return {id_safe("oak_log"),0};
                }
                case BlockSnow: return {id_safe("snow"),0};
                case BlockLava: return {id_safe("lava"),0};\
                case BlockWhiteTerracotta: return {id_safe("white_terracotta"),0};
                case BlockQuartz: {
                    if (face == TOP)
                        return {id_safe("quartz_block_top"),0};
                    if (face == BOTTOM)
                        return {id_safe("quartz_block_bottom"),0};
                    return {id_safe("quartz_block_side"),0};
                }
                case BlockSlab: {
                    return {id_safe("oak_planks"),0};
                }
                case BlockUnknown:break;
            }
            return {id_safe("notex"),0};
        }

        std::unordered_map<int, int> _cache;
        Textures& _textures;
    } texturer(*textures);

    while (!glfwWindowShouldClose(window))
    {
        fps_counter.tick();
        fps_counter.updateGlfwWindowTitle(window);
        rebuildTLAS = false;

        uniformData.projInverse = invert_mat4(camera_get_proj_mat4(&camera, framebufferWidth, framebufferHeight));
        uniformData.viewInverse = invert_mat4(camera_get_pure_view_mat4(&camera));
        if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS)
            uniformData.time = fmod((imr_get_time_nano()-startingTime) / 1e9, 60.0f) / 60.0f;
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
                rebuildTLAS = true;
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
                    loaded->mesh = std::make_unique<ChunkMesh>(*device, n, texturer);

                    //printf("fff %f\n", transformMatrix.matrix[0][3]);

                    VkTransformMatrixKHR identityMatrix = {
                        1.0f, 0.0f, 0.0f, 0.f,
                        0.0f, 1.0f, 0.0f, 0.f,
                        0.0f, 0.0f, 1.0f, 0.f,
                    };
                    std::vector<imr::AccelerationStructure::TriangleGeometry> geometry = {{loaded->mesh->buf->device_address(), loaded->mesh->iBuf->device_address(),
                                                                                           loaded->mesh->num_verts, static_cast<uint32_t>(loaded->mesh->num_verts / 3),
                                                                                           identityMatrix}};
                    if (!loaded->accel) {
                        loaded->accel = std::make_unique<imr::AccelerationStructure>(*device);
                        loaded->accel->createBottomLevelAccelerationStructure(geometry);
                    }

                    DescriptorPackage desc = {
                        loaded->mesh->buf->device_address(),
                        loaded->mesh->iBuf->device_address(),
                        loaded->mesh->vertexAttributesBuf->device_address(),
                        0,
                        vec3(cx * 16, 0, cz * 16)
                    };

                    VkTransformMatrixKHR transformMatrix = {
                        1.0f, 0.0f, 0.0f, float(cx * 16),
                        0.0f, 1.0f, 0.0f, 0,
                        0.0f, 0.0f, 1.0f, float(cz * 16),
                    };

                    loadedChunkData[{cx, cz}] = {transformMatrix, loaded->accel.get(), desc};
                }
            };

            int player_chunk_x = camera.position.x / 16;
            int player_chunk_z = camera.position.z / 16;

                int radius = 12;
                for (int dx = -radius; dx <= radius; dx++) {
                    for (int dz = -radius; dz <= radius; dz++) {
                        load_chunk(player_chunk_x + dx, player_chunk_z + dz);
                    }
                }


            for (auto chunk : world.loaded_chunks()) {
                if (abs(chunk->cx - player_chunk_x) > radius || abs(chunk->cz - player_chunk_z) > radius) {
                    loadedChunkData.erase({chunk->cx, chunk->cz});
                    std::unique_ptr<ChunkMesh> stolen = std::move(chunk->mesh);
                    if (stolen) {
                        ChunkMesh* released = stolen.release();
                        context.frame().addCleanupAction([=]() {
                            delete released;
                        });
                    }
                    world.unload_chunk(chunk);
                    rebuildTLAS = true;
                    

                    continue;
                }

                auto& mesh = chunk->mesh;
                if (!mesh || mesh->num_verts == 0) {
                    continue;
                }
            }

            if(rebuildTLAS && !loadedChunkData.empty()) {
                instances.clear();
                instances.reserve(loadedChunkData.size());
                globalDescriptors.clear();
                globalDescriptors.reserve(loadedChunkData.size());
                for (auto& [_, val] : loadedChunkData) {
                    auto& [transform, blas, desc] = val;
                    instances.emplace_back(transform, blas);
                    globalDescriptors.emplace_back(desc);
                }

              
                shaders->topLevelAS = std::make_unique<imr::AccelerationStructure>(*device);
                shaders->topLevelAS->createTopLevelAccelerationStructure(instances);
                descriptorBuffer = std::make_unique<imr::Buffer>(*device, sizeof(DescriptorPackage) * globalDescriptors.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
                descriptorBuffer->uploadDataSync(0, sizeof(DescriptorPackage) * globalDescriptors.size(), globalDescriptors.data());

            }

            auto &image = context.image();

            /*
               Dispatch the ray tracing commands
               */
            vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline->pipeline());





            auto bind_helper = pipeline->create_bind_helper();
            bind_helper->set_acceleration_structure(0, 0, *shaders->topLevelAS);
            bind_helper->set_storage_image(0, 1, storage_image->whole_image_view());
            bind_helper->set_uniform_buffer(0, 2, *ubo);
            bind_helper->set_storage_buffer(0, 3, *descriptorBuffer);
            bind_helper->set_sampler(0, 4, textures->sampler);
            for (int i = 0; i < textures->block_textures.size(); i++)
                bind_helper->set_texture_image(0, 5, textures->block_textures[i]->whole_image_view(), i);
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
    }
    swapchain.drain();
    return 0;
}
