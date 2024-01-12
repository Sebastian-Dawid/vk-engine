#include <vk-engine.h>
#include <vk-images.h>
#include <error_fmt.h>

void deletion_queue_t::push_function(std::function<void()>&& function)
{
    this->deletors.push_back(function);
}

void deletion_queue_t::flush()
{
    for (auto it = this->deletors.rbegin(); it != this->deletors.rend(); ++it)
    {
        (*it)();
    }
    this->deletors.clear();
}

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

        this->main_deletion_queue.flush();

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

void engine_t::draw_background(vk::CommandBuffer cmd)
{
    /*
    float flash = std::abs(sin(this->frame_count / 120.f));
    vk::ClearColorValue clear_value(std::array<float, 4>{ 0.f, 0.f, flash, 1.f });

    vk::ImageSubresourceRange clear_range(vk::ImageAspectFlagBits::eColor, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS);
    cmd.clearColorImage(this->draw_image.image, vk::ImageLayout::eGeneral, clear_value, clear_range);
    */
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, this->gradient_pipeline.pipeline);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, this->gradient_pipeline.layout, 0, this->draw_descriptor.set, {});
    cmd.dispatch(std::ceil(this->draw_image.extent.width / 16.0), std::ceil(this->draw_image.extent.height / 16.0), 1);
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

    this->draw_extent.width = this->draw_image.extent.width;
    this->draw_extent.height = this->draw_image.extent.height;
    vk::CommandBufferBeginInfo begin_info(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    if (result = cmd.begin(&begin_info); result != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFailed to begin recording command buffer!\n", ERROR_FMT("ERROR"));
        return false;
    }

    vkutil::transition_image(cmd, this->draw_image.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);

    this->draw_background(cmd);
    
    vkutil::transition_image(cmd, this->draw_image.image, vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferSrcOptimal);
    vkutil::transition_image(cmd, this->swapchain.images[swapchain_img_idx], vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
    
    vkutil::copy_image_to_image(cmd, this->draw_image.image, this->swapchain.images[swapchain_img_idx], this->draw_extent, this->swapchain.extent);

    vkutil::transition_image(cmd, this->swapchain.images[swapchain_img_idx], vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR);


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
        if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
            fmt::print(fd, "[ {} ]\t", INFO_FMT(severity));

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
#ifdef INFO_LAYERS
                | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
#endif
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
                ERROR_FMT("ERROR"),
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
        fmt::print(stderr, "[ {} ]\tPhysical device selection failed with error: {}\n", ERROR_FMT("ERROR"), phys_ret.error().message());
        return false;
    }

    this->physical_device = vk::PhysicalDevice(phys_ret.value());
    vkb::DeviceBuilder device_builder{ phys_ret.value() };
    vkb::Result<vkb::Device> dev_ret = device_builder.build();
    if (!dev_ret)
    {
        fmt::print(stderr, "[ {} ]\tDevice creation failed with error: {}\n", ERROR_FMT("ERROR"), dev_ret.error().message());
        return false;
    }

    vkb::Device vkb_device = dev_ret.value();
    this->device.dev = vk::Device(vkb_device.device);

    vkb::Result<VkQueue> gq_ret = vkb_device.get_queue(vkb::QueueType::graphics);
    if (!gq_ret)
    {
        fmt::print(stderr, "[ {} ]\tGetting graphics queue failed with error: {}\n", ERROR_FMT("ERROR"), gq_ret.error().message());
        return false;
    }
    this->device.graphics.queue = vk::Queue(gq_ret.value());
    vkb::Result<std::uint32_t> gqi_ret = vkb_device.get_queue_index(vkb::QueueType::graphics);
    if (!gqi_ret)
    {
        fmt::print(stderr, "[ {} ]\tGetting graphics queue family failed with error: {}\n", ERROR_FMT("ERROR"), gqi_ret.error().message());
        return false;
    }
    this->device.graphics.family_index = gqi_ret.value();

    vkb::Result<VkQueue> pq_ret = vkb_device.get_queue(vkb::QueueType::present);
    if (!pq_ret)
    {
        fmt::print(stderr, "[ {} ]\tGetting present queue failed with error: {}\n", ERROR_FMT("ERROR"), pq_ret.error().message());
        return false;
    }
    this->device.present.queue = vk::Queue(pq_ret.value());
    vkb::Result<std::uint32_t> pqi_ret = vkb_device.get_queue_index(vkb::QueueType::present);
    if (!gqi_ret)
    {
        fmt::print(stderr, "[ {} ]\tGetting present queue family failed with error: {}\n", ERROR_FMT("ERROR"), pqi_ret.error().message());
        return false;
    }
    this->device.present.family_index = gqi_ret.value();

    VmaAllocatorCreateInfo allocator_info = {};
    allocator_info.physicalDevice = this->physical_device;
    allocator_info.device = this->device.dev;
    allocator_info.instance = this->instance;
    allocator_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocator_info, &this->allocator);

    this->main_deletion_queue.push_function([&]() { vmaDestroyAllocator(this->allocator); });

    if (!this->create_swapchain(this->window.width, this->window.height)) return false;

    this->draw_image.extent = vk::Extent3D(this->window.width, this->window.height, 1);
    this->draw_image.format = vk::Format::eR16G16B16A16Sfloat;
    vk::ImageCreateInfo rimg_info({}, vk::ImageType::e2D, this->draw_image.format, this->draw_image.extent, 1, 1, vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eColorAttachment);
    VmaAllocationCreateInfo rimg_alloc_info = {};
    rimg_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    rimg_alloc_info.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vmaCreateImage(this->allocator, (VkImageCreateInfo*)&rimg_info, &rimg_alloc_info, (VkImage*)&this->draw_image.image, &this->draw_image.allocation, nullptr);

    vk::ImageViewCreateInfo rview_info({}, this->draw_image.image, vk::ImageViewType::e2D, this->draw_image.format, {},
            vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));
    vk::Result result;
    std::tie(result, this->draw_image.view) = this->device.dev.createImageView(rview_info);
    if (result != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tCreating render image view failed.\n", ERROR_FMT("ERROR"));
        return false;
    }

    this->main_deletion_queue.push_function([=, this]() {
            this->device.dev.destroyImageView(this->draw_image.view);
            vmaDestroyImage(this->allocator, (VkImage)this->draw_image.image, this->draw_image.allocation);
            });

    if (!this->init_commands()) return false;
    if (!this->init_sync_structures()) return false;
    if (!this->init_descriptors()) return false;
    if (!this->init_pipelines()) return false;

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

bool engine_t::init_descriptors()
{
    std::vector<descriptor_allocator_t::pool_size_ratio_t> sizes = { { vk::DescriptorType::eStorageImage, 1 } };
    this->global_descriptor_allocator.init_pool(this->device.dev, 10, sizes);
    {
        descriptor_layout_builder_t builder;
        auto ret = builder.add_binding(0, vk::DescriptorType::eStorageImage).build(this->device.dev, vk::ShaderStageFlagBits::eCompute);
        if (!ret.has_value()) return false;
        this->draw_descriptor.layout = ret.value();
    }

    auto ret = this->global_descriptor_allocator.allocate(this->device.dev, this->draw_descriptor.layout);
    if (!ret.has_value()) return false;
    this->draw_descriptor.set = ret.value();

    vk::DescriptorImageInfo img_info({}, this->draw_image.view, vk::ImageLayout::eGeneral);
    vk::WriteDescriptorSet draw_img_write(this->draw_descriptor.set, 0, {}, 1, vk::DescriptorType::eStorageImage, &img_info);
    this->device.dev.updateDescriptorSets(draw_img_write, {});

    this->main_deletion_queue.push_function([&]() {
            this->device.dev.destroyDescriptorSetLayout(this->draw_descriptor.layout);
            this->global_descriptor_allocator.destroy_pool(this->device.dev);
            });

    return true;
}

bool engine_t::init_pipelines()
{
    return this->init_background_pipelines();
}

bool engine_t::init_background_pipelines()
{
    vk::Result result;
    vk::PipelineLayoutCreateInfo compute_layout({}, this->draw_descriptor.layout);
    std::tie(result, this->gradient_pipeline.layout) = this->device.dev.createPipelineLayout(compute_layout);
    if (result != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFailed to create pipeline layout!\n", ERROR_FMT("ERROR"));
        return false;
    }

    auto compute_draw_shader = vkutil::load_shader_module("tests/build/shaders/gradient.comp.spv", this->device.dev);
    if (!compute_draw_shader.has_value()) return false;
    vk::PipelineShaderStageCreateInfo stage_info({}, vk::ShaderStageFlagBits::eCompute, compute_draw_shader.value(), "main");
    vk::ComputePipelineCreateInfo pipeline_info({}, stage_info, this->gradient_pipeline.layout);
    std::tie(result, this->gradient_pipeline.pipeline) = this->device.dev.createComputePipeline({}, pipeline_info);
    if (result != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFailed to create compute pipeline!\n", ERROR_FMT("ERROR"));
        return false;
    }

    this->device.dev.destroyShaderModule(compute_draw_shader.value());
    this->main_deletion_queue.push_function([&]() {
            this->device.dev.destroyPipelineLayout(this->gradient_pipeline.layout);
            this->device.dev.destroyPipeline(this->gradient_pipeline.pipeline);
            });

    return true;
}

bool engine_t::create_swapchain(std::uint32_t width, std::uint32_t height)
{
    vkb::SwapchainBuilder builder{this->physical_device, this->device.dev, this->window.surface};
    this->swapchain.format = vk::Format::eB8G8R8A8Unorm;

    vkb::Result<vkb::Swapchain> sc_ret = builder
        .set_desired_format((VkSurfaceFormatKHR)vk::SurfaceFormatKHR(this->swapchain.format, vk::ColorSpaceKHR::eSrgbNonlinear))
        .set_desired_present_mode((VkPresentModeKHR)vk::PresentModeKHR::eFifo)
        .set_desired_extent(width, height)
        .add_image_usage_flags((VkImageUsageFlags)vk::ImageUsageFlagBits::eTransferDst)
        .build();

    if (!sc_ret)
    {
        fmt::print(stderr, "[ {} ]\tCreating swapchain failed with error: {}\n", ERROR_FMT("ERROR"), sc_ret.error().message());
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
