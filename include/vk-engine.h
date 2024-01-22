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

struct engine_stats_t
{
    float fram_time;
    std::uint32_t triangle_count;
    std::uint32_t drawcall_count;
    float scene_update_time;
    float mesh_draw_time;
};

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
    bounds_t bounds;
    glm::mat4 transform;
    vk::DeviceAddress vertex_buffer_address;
};

struct draw_context_t
{
    std::vector<render_object_t> opaque_surfaces;
    std::vector<render_object_t> transparent_surfaces;
};

struct gltf_metallic_roughness_t
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

    /// Builds opaque and transparent pipelines for the given shader modules.
    ///
    /// Returns:
    /// `true` - success
    /// `false` - if pipeline creation failed
    bool build_pipelines(engine_t* engine);
    void clear_resources(vk::Device device);
    /// Creates a new `material_instance_t` based on the given `material_resources_t`.
    /// Allocates and updates the descriptor set using the given `descriptor_allocator_growable_t`.
    ///
    /// Params:
    /// * `device`               - `vk::Device` to use for allocation
    /// * `pass`                 - type of the pass this material is used in
    /// * `resources`            - resources used in the material e.g. textures, data, etc.
    /// * `descriptor_allocator` - allocator used to allocate the descriptor set
    ///
    /// Returns:
    /// * `material_instance_t` - success
    /// * `std::nullopt` - failed to allocate descriptor set
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
    gltf_metallic_roughness_t metal_rough_material;

    draw_context_t main_draw_context;
    std::unordered_map<std::string, std::shared_ptr<loaded_gltf_t>> loaded_scenes;

    engine_stats_t stats;

    /// Initializes the vulkan context. Calls all other `init_*` functions.
    /// Also calls `create_swapchain` to create the swapcahin.
    ///
    /// Returns:
    /// * `false` - if any step of the initialization failed.
    /// * `true` - if the vulkan context was created successfully
    bool init_vulkan(std::string app_name = "vk-app");
    /// Initializes the command pools and buffers for the frames and immediate submission.
    ///
    /// Returns:
    /// * `false` - if creation of any pool or buffer failed
    /// * `true` - if all pools and buffers were created successfully
    bool init_commands();
    /// Initializes the semaphores and fences for the frames and immediate submissions.
    ///
    /// Returns:
    /// * `false` - if creation of any semaphore or fence failed
    /// * `true` - if all semaphores and fences were created successfully
    bool init_sync_structures();
    /// Initializes the global descriptor allocator and the descriptor allocators for the frames.
    /// Also initializes the descriptors for the background pipeline and scene data.
    ///
    /// Returns:
    /// * `false` - if creation of any allocator or allocation of any descriptor fails
    /// * `true` - if all allocators were created and no allocations failed
    bool init_descriptors();
    /// Initializes background pipeline and material pipelines.
    /// Calls other `init_*` functions
    ///
    /// Returns:
    /// * `false` - if initialization of any pipeline failed
    /// * `true` - if all pipelines were initialized successfully
    bool init_pipelines();
    /// Initializes the background compute pipeline.
    ///
    /// Returns:
    /// * `false` - if any step of the pipeline initialization failed
    /// * `true` - if the pipeline was created successfully
    bool init_background_pipelines();
    /// Initializes ImGui. Creates Descriptor pool for ImGui.
    ///
    /// Returns:
    /// * `false` - if pool creation or ImGui font texture creation failed
    /// * `true` - if ImGui was initialized successfully
    bool init_imgui();
    /// Initializes some default textures e.g. white, grey, black and error checkerboard.
    ///
    /// Returns:
    /// * `false` - if creation of any image or sampler failed
    /// * `true` - if all textures were created successfully
    bool init_default_data();

    /// Loads the glTF Model at `path` and stores it in the `loaded_scenes` with key `name`.
    ///
    /// Supported Formats:
    /// * `.gltf`
    /// * `.glb`
    ///
    /// Params:
    /// * `path` - path to the glTF file
    /// * `name` - key to store the model under
    ///
    /// Returns:
    /// * `false` - if the model could not be loaded e.g. invalid path, buffer or texture could not be created
    /// * `true` - if the model was loaded successfully
    bool load_model(std::string path, std::string name);

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

    engine_t(std::uint32_t width = 1024, std::uint32_t height = 1024, std::string app_name = "vk-app");
    ~engine_t();
};
