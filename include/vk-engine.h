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

#include <glm/glm.hpp>

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
    } window;
    
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
    vk::Extent2D draw_extent;

    frame_data_t frames[FRAME_OVERLAP];
    std::size_t frame_count = 0;

    deletion_queue_t main_deletion_queue;

    descriptor_allocator_t global_descriptor_allocator;
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
    } triangle_pipeline;

    struct
    {
        vk::Pipeline pipeline;
        vk::PipelineLayout layout;
    } mesh_pipeline;

    gpu_mesh_buffer_t rectangle;

    std::vector<compute_effect_t> background_effects;
    std::uint32_t current_bg_effect = 0;

    bool init_vulkan();
    bool init_commands();
    bool init_sync_structures();
    bool init_descriptors();
    bool init_pipelines();
    bool init_mesh_pipeline();
    bool init_triangle_pipelines();
    bool init_background_pipelines();
    bool init_imgui();

    bool init_default_data();

    bool create_swapchain(std::uint32_t width, std::uint32_t height);
    void destroy_swapchain();

    std::optional<allocated_buffer_t> create_buffer(std::size_t alloc_size, vk::BufferUsageFlags buffer_usage, VmaMemoryUsage memory_usage);
    void destroy_buffer(const allocated_buffer_t& buf);

    std::optional<gpu_mesh_buffer_t> upload_mesh(std::span<std::uint32_t> indicies, std::span<vertex_t> vertices);

    frame_data_t& get_current_frame();

    void draw_geometry(vk::CommandBuffer cmd);
    void draw_background(vk::CommandBuffer cmd);
    void draw_imgui(vk::CommandBuffer cmd, vk::ImageView target_image_view);
    bool draw();
    bool run();

    bool immediate_submit(std::function<void(vk::CommandBuffer cmd)>&& function);

    engine_t();
    ~engine_t();
};
