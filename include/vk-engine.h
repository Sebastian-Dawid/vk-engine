#pragma once

#include <cstdint>
#include <functional>
#define VULKAN_HPP_NO_EXCEPTIONS
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#include <VkBootstrap.h>
#include <vk_mem_alloc.h>
#include <vk-types.h>
#include <vk-descriptors.h>
#include <vk-pipelines.h>
#include <vk-loader.h>

#include <glm/glm.hpp>
#include <camera.h>

struct mesh_node_t : public node_t
{
    std::shared_ptr<mesh_asset_t> mesh;
    virtual void draw(const glm::mat4& top_matrix, draw_context_t& ctx) override;
    virtual ~mesh_node_t() {};
};

struct render_object_t
{
    std::uint32_t index_count;
    std::uint32_t first_index;
    vk::Buffer index_buffer;

    material_instance_t* material;

    glm::mat4 transform;
    vk::DeviceAddress vertex_buffer_address;
};

struct draw_context_t
{
    std::vector<render_object_t> opaque_surfaces;
};

struct gltf_metallic_roughness
{
    material_pipeline_t opaque_pipeline;
    material_pipeline_t transparent_pipeline;

    vk::DescriptorSetLayout material_layout;

    struct material_constants_t
    {
        glm::vec4 color_factors;
        glm::vec4 metal_rought_factors;
        glm::vec4 extra[14];
    };

    struct material_resources_t
    {
        allocated_image_t color_image;
        vk::Sampler color_sampler;
        allocated_image_t metal_rough_image;
        vk::Sampler metal_rough_sampler;
        vk::Buffer data_buffer;
        std::uint32_t data_buffer_offset;
    };

    descriptor_writer_t writer;

    bool build_pipelines(engine_t* engine);
    void clear_resources(vk::Device device);

    std::optional<material_instance_t> write_material(vk::Device device, material_pass_e pass, const material_resources_t& resources,
            descriptor_allocator_growable_t& descriptor_allocator);
};

struct compute_push_constants_t
{
    glm::vec4 data1;
    glm::vec4 data2;
    glm::vec4 data3;
    glm::vec4 data4;
};

struct compute_effect_t
{
    const char* name;
    vk::Pipeline pipeline;
    vk::PipelineLayout layout;
    compute_push_constants_t data;
};

struct gpu_scene_data_t
{
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::mat4 viewproj;
    alignas(16) glm::vec4 ambient_color;
    alignas(16) glm::vec4 sunlight_dir;
    alignas(16) glm::vec4 sunlight_color;
};

struct deletion_queue_t
{
    std::vector<std::function<void()>> deletors;
    void push_function(std::function<void()>&& function);
    void flush();
};

struct frame_data_t
{
    vk::CommandPool pool;
    vk::CommandBuffer buffer;

    vk::Semaphore swapchain_semaphore, render_semaphore;
    vk::Fence render_fence;

    deletion_queue_t deletion_queue;
    descriptor_allocator_growable_t frame_descriptors;
};
constexpr std::uint32_t FRAME_OVERLAP = 2;

struct engine_t
{
    bool initialized = false;
    vkb::Instance vkb_instance;
    vk::Instance instance;
    vk::DebugUtilsMessengerEXT messenger;

    VmaAllocator allocator;

    struct window_t
    {
        GLFWwindow* win;
        std::uint32_t width;
        std::uint32_t height;
        vk::SurfaceKHR surface;
        bool resize_requested;
    } window;
 
    camera_t main_camera;

    vk::PhysicalDevice physical_device;
    struct device_t
    {
        vk::Device dev;
        struct queue_t
        {
            vk::Queue queue;
            std::uint32_t family_index;
        };
        queue_t graphics;
        queue_t present;
    } device;

    struct swapchain_t
    {
        vk::SwapchainKHR swapchain;
        vk::Format format;
        vk::Extent2D extent;
        std::vector<vk::Image> images;
        std::vector<vk::ImageView> views;
    } swapchain;

    allocated_image_t draw_image;
    allocated_image_t depth_image;
    vk::Extent2D draw_extent;
    float render_scale = 1.f;

    frame_data_t frames[FRAME_OVERLAP];
    std::size_t frame_count = 0;

    deletion_queue_t main_deletion_queue;

    descriptor_allocator_growable_t global_descriptor_allocator;
    struct
    {
        vk::DescriptorSet set;
        vk::DescriptorSetLayout layout;
    } draw_descriptor;

    struct
    {
        vk::Pipeline pipeline;
        vk::PipelineLayout layout;
    } gradient_pipeline;

    struct
    {
        vk::Fence fence;
        vk::CommandBuffer cmd;
        vk::CommandPool pool;
    } imm_submit;

    struct
    {
        vk::Pipeline pipeline;
        vk::PipelineLayout layout;
    } mesh_pipeline;

    std::vector<std::shared_ptr<mesh_asset_t>> test_meshes;

    struct
    {
        gpu_scene_data_t gpu_data;
        vk::DescriptorSetLayout layout;
    } scene_data;

    allocated_image_t white_image;
    allocated_image_t black_image;
    allocated_image_t grey_image;
    allocated_image_t error_checkerboard_image;

    vk::Sampler default_sampler_linear;
    vk::Sampler default_sampler_nearest;

    std::vector<compute_effect_t> background_effects;
    std::uint32_t current_bg_effect = 0;

    vk::DescriptorSetLayout single_image_descriptor_layout;

    material_instance_t default_data;
    gltf_metallic_roughness metal_rough_material;

    draw_context_t main_draw_context;
    std::unordered_map<std::string, std::shared_ptr<node_t>> loaded_nodes;

    bool init_vulkan();
    bool init_commands();
    bool init_sync_structures();
    bool init_descriptors();
    bool init_pipelines();
    bool init_mesh_pipeline();
    bool init_background_pipelines();
    bool init_imgui();

    bool init_default_data();

    bool create_swapchain(std::uint32_t width, std::uint32_t height);
    bool resize_swapchain();
    void destroy_swapchain();

    std::optional<allocated_buffer_t> create_buffer(std::size_t alloc_size, vk::BufferUsageFlags buffer_usage, VmaMemoryUsage memory_usage);
    void destroy_buffer(const allocated_buffer_t& buf);

    std::optional<allocated_image_t> create_image(vk::Extent3D size, vk::Format format, vk::ImageUsageFlags usage, bool mipmapped = false);
    std::optional<allocated_image_t> create_image(void* data, vk::Extent3D size, vk::Format format, vk::ImageUsageFlags usage, bool mipmapped = false);
    void destroy_image(const allocated_image_t& img);

    std::optional<gpu_mesh_buffer_t> upload_mesh(std::span<std::uint32_t> indicies, std::span<vertex_t> vertices);

    frame_data_t& get_current_frame();

    void update_scene();

    void draw_geometry(vk::CommandBuffer cmd);
    void draw_background(vk::CommandBuffer cmd);
    void draw_imgui(vk::CommandBuffer cmd, vk::ImageView target_image_view);
    bool draw();
    bool run();

    bool immediate_submit(std::function<void(vk::CommandBuffer cmd)>&& function);

    static void framebuffer_size_callback(GLFWwindow* window, int width, int height);

    engine_t();
    ~engine_t();
};
