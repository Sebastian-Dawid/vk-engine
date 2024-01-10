#include <vk-engine.h>
#include <vk-images.h>
#include <fmt/core.h>
#include <fmt/color.h>

#define ERROR_FMT(val) fmt::styled(val, fmt::emphasis::bold | fmt::fg(fmt::terminal_color::red))
#define WARN_FMT(val) fmt::styled(val, fmt::emphasis::bold | fmt::fg(fmt::terminal_color::yellow))
#define VERBOSE_FMT(val) fmt::styled(val, fmt::emphasis::bold | fmt::fg(fmt::terminal_color::cyan))

#define VALIDATION_FMT(val) fmt::styled(val, fmt::emphasis::bold | fmt::fg(fmt::terminal_color::magenta))
#define PERFORMANCE_FMT(val) fmt::styled(val, fmt::emphasis::bold | fmt::fg(fmt::terminal_color::yellow))
#define GENERAL_FMT(val) fmt::styled(val, fmt::emphasis::bold | fmt::fg(fmt::terminal_color::blue))

#ifndef DEBUG
#define DEBUG
#endif

engine_t::engine_t()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    if ((this->window.win = glfwCreateWindow(1024, 1024, "test", NULL, NULL)) == nullptr)
    {
        fmt::print(stderr, "[ {} ]\tFailed to create GLFW window!\n", ERROR_FMT("ERROR"));
        return;
    }
    this->window.width = 1024;
    this->window.height = 1024;
}

engine_t::~engine_t()
{
    if (this->initialized)
    {
        vk::Result result;
        if ((result = this->device.dev.waitIdle()) != vk::Result::eSuccess)
        {
            fmt::print(stderr, "[ {} ]\tWaiting on device to finish failed!\n", ERROR_FMT("ERROR"));
            abort();
        }

        for (std::size_t i = 0; i < FRAME_OVERLAP; ++i)
        {
            this->device.dev.destroyCommandPool(this->frames[i].pool);

            this->device.dev.destroyFence(this->frames[i].render_fence);
            this->device.dev.destroySemaphore(this->frames[i].render_semaphore);
            this->device.dev.destroySemaphore(this->frames[i].swapchain_semaphore);
        }

        this->destroy_swapchain();
        this->device.dev.destroy();
        this->instance.destroySurfaceKHR(this->window.surface);
        vkb::destroy_instance(this->vkb_instance);
    }
    glfwDestroyWindow(this->window.win);
    glfwTerminate();
}

bool engine_t::run()
{
    while (!glfwWindowShouldClose(this->window.win))
    {
        glfwPollEvents();
        if (glfwGetKey(this->window.win, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(this->window.win, GLFW_TRUE);
        if (!draw()) return false;
    }

    return true;
}

bool engine_t::draw()
{
    vk::Result result = this->device.dev.waitForFences(this->get_current_frame().render_fence, true, 1000000000);
    if (result != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFailed to wait on render fence!\n", ERROR_FMT("ERROR"));
        return false;
    }
    if ((result = this->device.dev.resetFences(this->get_current_frame().render_fence)) != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFailed to reset render semaphore!\n", ERROR_FMT("ERROR"));
        return false;
    }
    std::uint32_t swapchain_img_idx;
    std::tie(result, swapchain_img_idx) = this->device.dev.acquireNextImageKHR(this->swapchain.swapchain, 1000000000,
            this->get_current_frame().swapchain_semaphore, nullptr);
    if (result != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFailed to aquire image!\n", ERROR_FMT("ERROR"));
        return false;
    }

    vk::CommandBuffer cmd = this->get_current_frame().buffer;
    if (result = cmd.reset(); result != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFailed to reset command buffer!\n", ERROR_FMT("ERROR"));
        return false;
    }

    vk::CommandBufferBeginInfo begin_info(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    if (result = cmd.begin(&begin_info); result != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFailed to begin recording command buffer!\n", ERROR_FMT("ERROR"));
        return false;
    }

    vkutil::transition_image(cmd, this->swapchain.images[swapchain_img_idx], vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
    float flash = std::abs(sin(this->frame_count / 120.f));
    vk::ClearColorValue clear_value(std::array<float, 4>{ 0.f, 0.f, flash, 1.f });

    vk::ImageSubresourceRange clear_range(vk::ImageAspectFlagBits::eColor, 0, vk::RemainingMipLevels, 0, vk::RemainingArrayLayers);
    cmd.clearColorImage(this->swapchain.images[swapchain_img_idx], vk::ImageLayout::eGeneral, clear_value, clear_range);
    vkutil::transition_image(cmd, this->swapchain.images[swapchain_img_idx], vk::ImageLayout::eGeneral, vk::ImageLayout::ePresentSrcKHR);

    if (result = cmd.end(); result != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFailed to end recording command buffer!\n", ERROR_FMT("ERROR"));
        return false;
    }

    vk::CommandBufferSubmitInfo cmd_info(cmd);
    vk::SemaphoreSubmitInfo wait_info(this->get_current_frame().swapchain_semaphore, 1, vk::PipelineStageFlagBits2::eColorAttachmentOutput, 0);
    vk::SemaphoreSubmitInfo signal_info(this->get_current_frame().render_semaphore, 1, vk::PipelineStageFlagBits2::eAllGraphics, 0);
    vk::SubmitInfo2 submit({}, wait_info, cmd_info, signal_info);
    if (result = this->device.graphics.queue.submit2(submit, this->get_current_frame().render_fence); result != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFailed to submit to graphics queue!\n", ERROR_FMT("ERROR"));
        return false;
    }

    vk::PresentInfoKHR present_info(this->get_current_frame().render_semaphore, this->swapchain.swapchain, swapchain_img_idx);
    if (result = this->device.present.queue.presentKHR(&present_info); result != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFailed to present!\n", ERROR_FMT("ERROR"));
        return false;
    }

    this->frame_count++;
    return true;
}

bool engine_t::init_vulkan()
{
#ifdef DEBUG
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
        if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
            fmt::print(fd, "[ {} ]\t", WARN_FMT(severity));
        if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
            fmt::print(fd, "[ {} ]\t", VERBOSE_FMT(severity));

        if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) fmt::print(fd, "[ {} ]\t", GENERAL_FMT(type));
        if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) fmt::print(fd, "[ {} ]\t", PERFORMANCE_FMT(type));
        if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) fmt::print(fd, "[ {} ]\t", VALIDATION_FMT(type));

        fmt::print(fd, "{}\n", pCallbackData->pMessage);
        return VK_FALSE;
    };
#endif

    vkb::InstanceBuilder builder;
    vkb::Result<vkb::Instance> inst_ret = builder
        .set_app_name("test")
#ifdef DEBUG
        .set_debug_messenger_severity(VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        .set_debug_messenger_type(VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
        .set_debug_callback(debug_callback)
        .request_validation_layers()
#endif
        .require_api_version(1,3,0)
        .build();

    if (!inst_ret)
    {
        fmt::print(stderr, "[ {} ]\tInstance creation failed with error: {}\n",
                fmt::styled("ERROR", fmt::emphasis::bold | fmt::fg(fmt::terminal_color::red)),
                inst_ret.error().message());
        return false;
    }
    this->vkb_instance = inst_ret.value();
    this->instance = vkb_instance.instance;
    this->messenger = vkb_instance.debug_messenger;

    VkSurfaceKHR surface;
    glfwCreateWindowSurface(this->instance, this->window.win, nullptr, &surface);
    this->window.surface = vk::SurfaceKHR(surface);

    vkb::PhysicalDeviceSelector selector{vkb_instance};
    vkb::Result<vkb::PhysicalDevice> phys_ret = selector
        .set_surface(surface)
        .set_minimum_version(1, 3)
        .set_required_features_13(VkPhysicalDeviceVulkan13Features{
                .synchronization2 = true,
                .dynamicRendering = true })
        .set_required_features_12(VkPhysicalDeviceVulkan12Features{
                .descriptorIndexing = true,
                .bufferDeviceAddress = true })
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
    vkb::Result<vkb::Device> dev_ret = device_builder.build();
    if (!dev_ret)
    {
        fmt::print(stderr, "[ {} ]\tDevice creation failed with error: {}\n",
                fmt::styled("ERROR", fmt::emphasis::bold | fmt::fg(fmt::terminal_color::red)),
                dev_ret.error().message());
        return false;
    }

    vkb::Device vkb_device = dev_ret.value();
    this->device.dev = vk::Device(vkb_device.device);

    vkb::Result<VkQueue> gq_ret = vkb_device.get_queue(vkb::QueueType::graphics);
    if (!gq_ret)
    {
        fmt::print(stderr, "[ {} ]\tGetting graphics queue failed with error: {}\n",
                fmt::styled("ERROR", fmt::emphasis::bold | fmt::fg(fmt::terminal_color::red)),
                gq_ret.error().message());
        return false;
    }
    this->device.graphics.queue = vk::Queue(gq_ret.value());
    vkb::Result<std::uint32_t> gqi_ret = vkb_device.get_queue_index(vkb::QueueType::graphics);
    if (!gqi_ret)
    {
        fmt::print(stderr, "[ {} ]\tGetting graphics queue family failed with error: {}\n",
                fmt::styled("ERROR", fmt::emphasis::bold | fmt::fg(fmt::terminal_color::red)),
                gqi_ret.error().message());
        return false;
    }
    this->device.graphics.family_index = gqi_ret.value();

    vkb::Result<VkQueue> pq_ret = vkb_device.get_queue(vkb::QueueType::present);
    if (!pq_ret)
    {
        fmt::print(stderr, "[ {} ]\tGetting present queue failed with error: {}\n",
                fmt::styled("ERROR", fmt::emphasis::bold | fmt::fg(fmt::terminal_color::red)),
                pq_ret.error().message());
        return false;
    }
    this->device.present.queue = vk::Queue(pq_ret.value());
    vkb::Result<std::uint32_t> pqi_ret = vkb_device.get_queue_index(vkb::QueueType::present);
    if (!gqi_ret)
    {
        fmt::print(stderr, "[ {} ]\tGetting present queue family failed with error: {}\n",
                fmt::styled("ERROR", fmt::emphasis::bold | fmt::fg(fmt::terminal_color::red)),
                pqi_ret.error().message());
        return false;
    }
    this->device.present.family_index = gqi_ret.value();

    if (!this->create_swapchain(this->window.width, this->window.height)) return false;
    if (!this->init_commands()) return false;
    if (!this->init_sync_structures()) return false;

    this->initialized = true;
    return true;
}

bool engine_t::init_commands()
{
    vk::CommandPoolCreateInfo pool_info(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, this->device.graphics.family_index);
    vk::Result result;
    for (std::size_t i = 0; i < FRAME_OVERLAP; ++i)
    {
        std::tie(result, this->frames[i].pool) = this->device.dev.createCommandPool(pool_info);
        if (result != vk::Result::eSuccess)
        {
            fmt::print(stderr, "[ {} ]\tFailed to create command pool!\n", ERROR_FMT("ERROR"));
            return false;
        }
        vk::CommandBufferAllocateInfo alloc_info(this->frames[i].pool, vk::CommandBufferLevel::ePrimary, 1);
        std::vector<vk::CommandBuffer> buf;
        std::tie(result, buf) = this->device.dev.allocateCommandBuffers(alloc_info);
        if (result != vk::Result::eSuccess)
        {
            fmt::print(stderr, "[ {} ]\tFailed to create command buffer!\n", ERROR_FMT("ERROR"));
            return false;
        }
        this->frames[i].buffer = buf[0];
    }
    return true;
}

bool engine_t::init_sync_structures()
{
    vk::FenceCreateInfo fence_info(vk::FenceCreateFlagBits::eSignaled);
    vk::SemaphoreCreateInfo semaphore_info;
    vk::Result result;
    for (std::size_t i = 0; i < FRAME_OVERLAP; ++i)
    {
        std::tie(result, this->frames[i].render_fence) = this->device.dev.createFence(fence_info);
        if (result != vk::Result::eSuccess)
        {
            fmt::print(stderr, "[ {} ]\tFailed to create render fence!\n", ERROR_FMT("ERROR"));
            return false;
        }
        std::tie(result, this->frames[i].render_semaphore) = this->device.dev.createSemaphore(semaphore_info);
        if (result != vk::Result::eSuccess)
        {
            fmt::print(stderr, "[ {} ]\tFailed to create render semaphore!\n", ERROR_FMT("ERROR"));
            return false;
        }
        std::tie(result, this->frames[i].swapchain_semaphore) = this->device.dev.createSemaphore(semaphore_info);
        if (result != vk::Result::eSuccess)
        {
            fmt::print(stderr, "[ {} ]\tFailed to create swapchain semaphore!\n", ERROR_FMT("ERROR"));
            return false;
        }
    }
    return true;
}

bool engine_t::create_swapchain(std::uint32_t width, std::uint32_t height)
{
    vkb::SwapchainBuilder builder{this->physical_device, this->device.dev, this->window.surface};
    this->swapchain.format = vk::Format::eB8G8R8A8Srgb;

    vkb::Result<vkb::Swapchain> sc_ret = builder
        .set_desired_format((VkSurfaceFormatKHR)vk::SurfaceFormatKHR(this->swapchain.format, vk::ColorSpaceKHR::eSrgbNonlinear))
        .set_desired_present_mode((VkPresentModeKHR)vk::PresentModeKHR::eFifo)
        .set_desired_extent(width, height)
        .add_image_usage_flags((VkImageUsageFlags)vk::ImageUsageFlagBits::eTransferDst)
        .build();

    if (!sc_ret)
    {
        fmt::print(stderr, "[ {} ]\tCreating swapchain failed with error: {}\n",
                fmt::styled("ERROR", fmt::emphasis::bold | fmt::fg(fmt::terminal_color::red)),
                sc_ret.error().message());
        return false;
    }

    this->swapchain.swapchain = sc_ret.value().swapchain;
    this->swapchain.extent = sc_ret.value().extent;
    auto imgs = sc_ret.value().get_images().value();
    auto views = sc_ret.value().get_image_views().value();
    for (VkImage img : imgs)
        this->swapchain.images.push_back(img);
    for (VkImageView view : views)
        this->swapchain.views.push_back(view);

    return true;
}

void engine_t::destroy_swapchain()
{
    for (vk::ImageView view : this->swapchain.views)
        this->device.dev.destroyImageView(view);
    this->device.dev.destroySwapchainKHR(this->swapchain.swapchain);
}

frame_data_t& engine_t::get_current_frame()
{
    return this->frames[this->frame_count % FRAME_OVERLAP];
}
