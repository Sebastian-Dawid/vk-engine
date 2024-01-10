#pragma once
#define VULKAN_HPP_NO_EXCEPTIONS
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

/// Primary type of the lib that should handle most vk related stuff
/// (especially setup)
struct engine_t
{
    vk::Instance instance;
    vk::DebugUtilsMessengerEXT messenger;
    vk::DispatchLoaderDynamic loader;
    struct window_t
    {
        GLFWwindow* win;
        vk::SurfaceKHR surface;
    } window;
    vk::PhysicalDevice physical_device;
    struct device_t
    {
        vk::Device dev;
        vk::Queue gqueue;
        vk::Queue pqueue;
    } device;

    bool init_vulkan();

    engine_t();
};
