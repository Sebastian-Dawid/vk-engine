#include <vk-engine.h>
#include <VkBootstrap.h>
#include <fmt/core.h>
#include <fmt/color.h>

#define ERROR_FMT(val) fmt::styled(val, fmt::emphasis::bold | fmt::fg(fmt::terminal_color::red))
#define WARN_FMT(val) fmt::styled(val, fmt::emphasis::bold | fmt::fg(fmt::terminal_color::yellow))
#define VERBOSE_FMT(val) fmt::styled(val, fmt::emphasis::bold | fmt::fg(fmt::terminal_color::cyan))

#define VALIDATION_FMT(val) fmt::styled(val, fmt::emphasis::bold | fmt::fg(fmt::terminal_color::magenta))
#define PERFORMANCE_FMT(val) fmt::styled(val, fmt::emphasis::bold | fmt::fg(fmt::terminal_color::yellow))
#define GENERAL_FMT(val) fmt::styled(val, fmt::emphasis::bold | fmt::fg(fmt::terminal_color::blue))

bool engine_t::init_vulkan()
{
    auto debug_callback = [] (VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void *pUserData) -> VkBool32
    {
        auto severity = vkb::to_string_message_severity (messageSeverity);
        auto type = vkb::to_string_message_type (messageType);

        FILE* fd = stdout;
        if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        {
            fd = stderr;
            fmt::print(fd, "[ {} ]\t", ERROR_FMT(severity));
        }
        else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
            fmt::print(fd, "[ {} ]\t", WARN_FMT(severity));
        else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
            fmt::print(fd, "[ {} ]\t", VERBOSE_FMT(severity));

        if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) fmt::print(fd, "[ {} ]\t", GENERAL_FMT(type));
        else if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) fmt::print(fd, "[ {} ]\t", PERFORMANCE_FMT(type));
        else if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) fmt::print(fd, "[ {} ]\t", VALIDATION_FMT(type));

        fmt::print(fd, "{}\n", pCallbackData->pMessage);
        return VK_FALSE;
    };

    vkb::InstanceBuilder builder;
    vkb::Result<vkb::Instance> inst_ret = builder
        .set_app_name("test")
        .set_debug_messenger_severity(VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        .set_debug_messenger_type(VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
        .request_validation_layers()
        .set_debug_callback(debug_callback)
        .require_api_version(1,3,0)
        .build();

    if (!inst_ret)
    {
        fmt::print(stderr, "[ {} ]\tInstance creation failed with error: {}\n",
                fmt::styled("ERROR", fmt::emphasis::bold | fmt::fg(fmt::terminal_color::red)),
                inst_ret.error().message());
        return false;
    }
    vkb::Instance vkb_instance = inst_ret.value();
    this->instance = vkb_instance.instance;

    /*vk::Result result;
    this->loader.init(this->instance);
    vk::DebugUtilsMessengerCreateInfoEXT debug_info = vk::DebugUtilsMessengerCreateInfoEXT({},
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose
                | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
                | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
                vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
                | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
                | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
                debug_callback);
    std::tie(result, this->messenger) = this->instance.createDebugUtilsMessengerEXT(debug_info, nullptr, this->loader);*/

    VkSurfaceKHR surface;
    glfwCreateWindowSurface(this->instance, this->window.win, nullptr, &surface);
    this->window.surface = vk::SurfaceKHR(surface);

    vkb::PhysicalDeviceSelector selector{vkb_instance};
    vkb::Result<vkb::PhysicalDevice> phys_ret = selector
        .set_surface(surface)
        .set_minimum_version(1, 3)
        .require_dedicated_transfer_queue()
        .select();

    if (!phys_ret)
    {
        fmt::print(stderr, "[ {} ]\tPhysical device selection failed with error: {}\n",
                fmt::styled("ERROR", fmt::emphasis::bold | fmt::fg(fmt::terminal_color::red)),
                phys_ret.error().message());
        return false;
    }

    this->physical_device = vk::PhysicalDevice(phys_ret.value());
    vkb::DeviceBuilder device_builder{ phys_ret.value() };
    auto dev_ret = device_builder.build();
    if (!dev_ret)
    {
        fmt::print(stderr, "[ {} ]\tDevice creation failed with error: {}\n",
                fmt::styled("ERROR", fmt::emphasis::bold | fmt::fg(fmt::terminal_color::red)),
                dev_ret.error().message());
        return false;
    }

    vkb::Device vkb_device = dev_ret.value();
    this->device.dev = vk::Device(vkb_device.device);

    auto gq_ret = vkb_device.get_queue(vkb::QueueType::graphics);
    if (!gq_ret)
    {
        fmt::print(stderr, "[ {} ]\tGetting graphics queue failed with error: {}\n",
                fmt::styled("ERROR", fmt::emphasis::bold | fmt::fg(fmt::terminal_color::red)),
                gq_ret.error().message());
        return false;
    }
    this->device.gqueue = vk::Queue(gq_ret.value());

    auto pq_ret = vkb_device.get_queue(vkb::QueueType::present);
    if (!pq_ret)
    {
        fmt::print(stderr, "[ {} ]\tGetting present queue failed with error: {}\n",
                fmt::styled("ERROR", fmt::emphasis::bold | fmt::fg(fmt::terminal_color::red)),
                pq_ret.error().message());
        return false;
    }
    this->device.pqueue = vk::Queue(pq_ret.value());

    return true;
}

engine_t::engine_t()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    this->window.win = glfwCreateWindow(1024, 1024, "test", NULL, NULL);
}
