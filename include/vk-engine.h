#pragma once

#include <cstdint>
#include <functional>
#define VULKAN_HPP_NO_EXCEPTIONS
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#include <VkBootstrap.h>
#include <vk_mem_alloc.h>
#include <vk-types.h>

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

    bool init_vulkan();
    bool init_commands();
    bool init_sync_structures();

    bool create_swapchain(std::uint32_t width, std::uint32_t height);
    void destroy_swapchain();

    frame_data_t& get_current_frame();

    void draw_background(vk::CommandBuffer cmd);
    bool draw();
    bool run();

    engine_t();
    ~engine_t();
};
