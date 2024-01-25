#include <filesystem>
#include <vk-engine.h>
#include <vk-images.h>
#include <error_fmt.h>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include <glm/gtx/transform.hpp>
#include <chrono>

#ifndef BASE_DIR
#define BASE_DIR ""
#endif

bool is_visible(const render_object_t& obj, const glm::mat4& viewproj)
{
    std::array<glm::vec3, 8> corners {
        glm::vec3( 1,  1,  1),
        glm::vec3( 1,  1, -1),
        glm::vec3( 1, -1,  1),
        glm::vec3( 1, -1, -1),
        glm::vec3(-1,  1,  1),
        glm::vec3(-1,  1, -1),
        glm::vec3(-1, -1,  1),
        glm::vec3(-1, -1, -1),
    };

    glm::mat4 matrix = viewproj * obj.transform;

    glm::vec3 min( 1.5,  1.5,  1.5);
    glm::vec3 max(-1.5, -1.5, -1.5);

    for (auto& c : corners)
    {
        glm::vec4 v = matrix * glm::vec4(obj.bounds.origin + (c * obj.bounds.extents), 1.f);
        v.x = v.x / v.w;
        v.y = v.y / v.y;
        v.z = v.z / v.w;

        min = glm::min(glm::vec3(v), min);
        max = glm::max(glm::vec3(v), max);
    }

    if (min.z > 1.f || max.z < 0.f || min.x > 1.f || max.x < -1.f || min.y > 1.f || max.y < -1.f) return false;
    return true;
}

void mesh_node_t::draw(const glm::mat4& top_matrix, draw_context_t& ctx)
{
    glm::mat4 node_matrix = top_matrix * this->world_transform;
    for (auto& s : mesh->surfaces)
    {
        render_object_t def{ .index_count = s.count,
            .first_index = s.start_index,
            .index_buffer = this->mesh->mesh_buffer.index_buffer.buffer,
            .material = &s.material->data,
            .bounds = s.bounds,
            .transform = node_matrix,
            .vertex_buffer_address = mesh->mesh_buffer.vertex_buffer_address
        };
        ctx.opaque_surfaces.push_back(def);
    }
    node_t::draw(top_matrix, ctx);
}

bool gltf_metallic_roughness_t::build_pipelines(engine_t* engine, std::string vertex, std::string fragment,
        std::size_t push_constants_size, std::vector<std::tuple<std::uint32_t, vk::DescriptorType>> bindings,
        std::vector<vk::DescriptorSetLayout> external_layouts)
{
    auto vert_shader = vkutil::load_shader_module(vertex.c_str(), engine->device.dev);
    if (!vert_shader.has_value()) return false;
    auto frag_shader = vkutil::load_shader_module(fragment.c_str(), engine->device.dev);
    if (!frag_shader.has_value()) return false;

    vk::PushConstantRange matrix_range(vk::ShaderStageFlagBits::eVertex, 0, push_constants_size);
    descriptor_layout_builder_t layout_builder;
    for (const auto& [bind, type] : bindings) layout_builder.add_binding(bind, type);
    // TODO: Should the stages also be an input?
    auto ret_layout = layout_builder
        .build(engine->device.dev, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);
    if (!ret_layout.has_value()) return false;
    this->material_layout = ret_layout.value();

    external_layouts.push_back(this->material_layout);
    vk::PipelineLayoutCreateInfo mesh_layout_info({}, external_layouts, matrix_range);

    auto [result, new_layout] = engine->device.dev.createPipelineLayout(mesh_layout_info);
    if (result != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFailed to create pipeline layout!\n", ERROR_FMT("ERROR"));
        return false;
    }

    this->opaque_pipeline.layout = new_layout;
    this->transparent_pipeline.layout = new_layout;

    pipeline_builder_t pipeline_builder;
    pipeline_builder.pipeline_layout = new_layout;
    // TODO: Pipeline settings should probably be an input too
    auto ret_pipeline = pipeline_builder.set_shaders(vert_shader.value(), frag_shader.value())
        .set_input_topology(vk::PrimitiveTopology::eTriangleList)
        .set_polygon_mode(vk::PolygonMode::eFill)
        .set_cull_mode(vk::CullModeFlagBits::eNone, vk::FrontFace::eClockwise)
        .set_multisampling_none()
        .disable_blending()
        .enable_depthtest(true, vk::CompareOp::eLess)
        .set_color_attachment_format(engine->draw_image.format)
        .set_depth_format(engine->depth_image.format)
        .build(engine->device.dev);
    if (!ret_pipeline.has_value()) return false;
    this->opaque_pipeline.pipeline = ret_pipeline.value();
    ret_pipeline = pipeline_builder.enable_blending_additive()
        .enable_depthtest(false, vk::CompareOp::eLess)
        .build(engine->device.dev);
    if (!ret_pipeline.has_value()) return false;
    this->transparent_pipeline.pipeline = ret_pipeline.value();

    engine->device.dev.destroyShaderModule(vert_shader.value());
    engine->device.dev.destroyShaderModule(frag_shader.value());

    engine->main_deletion_queue.push_function([=, this]() {
            engine->device.dev.destroyDescriptorSetLayout(this->material_layout);
            engine->device.dev.destroyPipelineLayout(this->opaque_pipeline.layout);
            engine->device.dev.destroyPipeline(this->opaque_pipeline.pipeline);
            engine->device.dev.destroyPipeline(this->transparent_pipeline.pipeline);
            });

    return true;
}

std::optional<material_instance_t> gltf_metallic_roughness_t::write_material(vk::Device device, material_pass_e pass, const material_resources_t& resources,
        descriptor_allocator_growable_t& descriptor_allocator)
{
    material_instance_t material;
    material.pass_type = pass;
    material.pipeline = (pass == material_pass_e::TRANSPARENT) ? &this->transparent_pipeline : &this->opaque_pipeline;
    auto ret = descriptor_allocator.allocate(device, this->material_layout);
    if (!ret.has_value()) return std::nullopt;
    material.material_set = ret.value();

    this->writer.clear();
    for (const auto& buf : resources.buffers)
        this->writer.write_buffer(buf.binding, buf.buffer, buf.size, buf.offset, buf.type);
    for (const auto& img : resources.images)
        this->writer.write_image(img.binding, img.image.view, img.sampler, img.layout, img.type);

    this->writer.update_set(device, material.material_set);

    return material;
}

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

engine_t* loaded_engine = nullptr;

void engine_t::framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    loaded_engine->resize_swapchain();
}

engine_t::engine_t(std::uint32_t width, std::uint32_t height, std::string app_name, bool show_stats, bool use_imgui) : use_imgui(use_imgui)
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    if ((this->window.win = glfwCreateWindow(width, height, app_name.c_str(), NULL, NULL)) == nullptr)
    {
        fmt::print(stderr, "[ {} ]\tFailed to create GLFW window!\n", ERROR_FMT("ERROR"));
        return;
    }
    this->window.width = width;
    this->window.height = height;
    loaded_engine = this;
    this->main_camera = camera_t{ .velocity = glm::vec3(0.f), .position = glm::vec3(0.f, 0.0f, 1.f), .pitch = 0, .yaw = 0 };
    glfwSetFramebufferSizeCallback(this->window.win, engine_t::framebuffer_size_callback);
    glfwSetWindowUserPointer(this->window.win, &this->main_camera);
    glfwSetCursorPosCallback(this->window.win, cursor_pos_callback);
    this->show_stats = show_stats;
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

        this->loaded_scenes.clear();

        for (std::size_t i = 0; i < FRAME_OVERLAP; ++i)
        {
            this->device.dev.destroyCommandPool(this->frames[i].pool);

            this->device.dev.destroyFence(this->frames[i].render_fence);
            this->device.dev.destroySemaphore(this->frames[i].render_semaphore);
            this->device.dev.destroySemaphore(this->frames[i].swapchain_semaphore);

            this->frames[i].deletion_queue.flush();
        }

        // WARN: flush main deletion queue only after deletion queues of the frames have been flushed
        // since they rely on the allocator that is destroyed in the main deletion queue
        this->main_deletion_queue.flush();

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
        auto start = std::chrono::system_clock::now();

        glfwPollEvents();
        if (glfwGetKey(this->window.win, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(this->window.win, GLFW_TRUE);

        this->main_camera.process_glfw_event(this->window.win);

        if (this->window.resize_requested)
        {
            if (!this->resize_swapchain()) return false;
        }

        if (this->use_imgui)
        {
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            // NOTE: Setting this function is the responsibility of the consumer of the engine.
            this->define_imgui_windows();

            if (this->show_stats)
            {
                if (ImGui::Begin("Stats"))
                {
                    ImGui::Text("Frametime:   %f ms", this->stats.fram_time);
                    ImGui::Text("Draw time:   %f ms", this->stats.mesh_draw_time);
                    ImGui::Text("Update time: %f ms", this->stats.scene_update_time);
                    ImGui::Text("Triangles:   %i", this->stats.triangle_count);
                    ImGui::Text("Draws:       %i", this->stats.drawcall_count);
                    ImGui::End();
                }
            }

            ImGui::Render();
        }

        if (!draw()) return false;

        auto end = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        this->stats.fram_time = elapsed.count() / 1000.f;
    }

    return true;
}

void engine_t::update_scene()
{
    auto start = std::chrono::system_clock::now();

    this->background_effects[0].data.data3.x = this->window.width;
    this->background_effects[0].data.data3.y = this->window.height;
    this->background_effects[0].data.data3.z = this->render_scale;
    this->main_camera.update();

    this->main_draw_context.opaque_surfaces.clear();
    this->main_draw_context.transparent_surfaces.clear();

    for (auto& [k, v] : this->loaded_scenes)
    {
        v->draw(v->transform, this->main_draw_context);
    }

    this->scene_data.gpu_data.view = this->main_camera.get_view_matrix();
    this->scene_data.gpu_data.proj = glm::perspective(glm::radians(70.f), (float)this->window.width / (float)this->window.height, .1f, 10000.f);
    this->scene_data.gpu_data.proj[1][1] *= -1;
    this->scene_data.gpu_data.viewproj = this->scene_data.gpu_data.proj * this->scene_data.gpu_data.view ;

    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    this->stats.scene_update_time = elapsed.count() / 1000.f;
}

void engine_t::draw_geometry(vk::CommandBuffer cmd)
{
    this->stats.drawcall_count = 0;
    this->stats.triangle_count = 0;
    auto start = std::chrono::system_clock::now();

    std::vector<std::uint32_t> opaque_draws;
    opaque_draws.reserve(this->main_draw_context.opaque_surfaces.size());

    for (std::uint32_t i = 0; i < this->main_draw_context.opaque_surfaces.size(); ++i)
    {
        // NOTE: Frustum culling on small scenes appears to perform worse than just rendering off screen objects too.
        // TODO: Needs further testing.
        if (is_visible(this->main_draw_context.opaque_surfaces[i], this->scene_data.gpu_data.viewproj))
            opaque_draws.push_back(i);
    }

    // BUG: This seems to break transparent objects when. Only occasionally though.
    /* std::sort(opaque_draws.begin(), opaque_draws.end(), [&](const auto& i, const auto& j) {
            const render_object_t& a = this->main_draw_context.opaque_surfaces[i];
            const render_object_t& b = this->main_draw_context.opaque_surfaces[j];
            if (a.material == b.material) return a.index_buffer < b.index_buffer;
            return a.material < b.material;
        });
    */
    
    vk::RenderingAttachmentInfo color_attachment(this->draw_image.view, vk::ImageLayout::eGeneral);
    vk::RenderingAttachmentInfo depth_attachment(this->depth_image.view, vk::ImageLayout::eDepthAttachmentOptimal, {}, {}, {}, vk::AttachmentLoadOp::eClear,
            vk::AttachmentStoreOp::eStore);
    depth_attachment.clearValue.depthStencil.depth = 1.f;
    vk::RenderingInfo render_info({}, { vk::Offset2D(0, 0), this->draw_extent }, 1, {}, color_attachment, &depth_attachment);
    cmd.beginRendering(render_info);

    auto ret_buf = create_buffer(sizeof(gpu_scene_data_t), vk::BufferUsageFlagBits::eUniformBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU);
    if (!ret_buf.has_value()) return;

    allocated_buffer_t gpu_scene_data_buffer = ret_buf.value();
    this->get_current_frame().deletion_queue.push_function([=, this]() {
            this->destroy_buffer(gpu_scene_data_buffer);
        });

    gpu_scene_data_t* scene_uniform_data = (gpu_scene_data_t*)gpu_scene_data_buffer.info.pMappedData;
    *scene_uniform_data = this->scene_data.gpu_data;

    auto ret = this->get_current_frame().frame_descriptors.allocate(this->device.dev, this->scene_data.layout);
    if (!ret_buf.has_value())
    {
        cmd.endRendering();
        return;
    }

    vk::DescriptorSet global_descriptor = ret.value();
    descriptor_writer_t writer;
    writer.write_buffer(0, gpu_scene_data_buffer.buffer, sizeof(gpu_scene_data_t), 0, vk::DescriptorType::eUniformBuffer);
    writer.update_set(this->device.dev, global_descriptor);

    material_pipeline_t* last_pipeline = nullptr;
    material_instance_t* last_material = nullptr;
    vk::Buffer last_index_buffer = {};

    auto draw = [&](const render_object_t& obj)
    {
        if (obj.material != last_material)
        {
            last_material = obj.material;
            if (obj.material->pipeline != last_pipeline)
            {
                last_pipeline = obj.material->pipeline;
                cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, obj.material->pipeline->pipeline);
                cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, obj.material->pipeline->layout, 0, global_descriptor, {});

                vk::Viewport viewport(0, 0, this->draw_extent.width, this->draw_extent.height, 0, 1);
                cmd.setViewport(0, viewport);
                vk::Rect2D scissor(vk::Offset2D(0, 0), this->draw_extent);
                cmd.setScissor(0, scissor);
            }

            cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, obj.material->pipeline->layout, 1, obj.material->material_set, {});
        }
        if (obj.index_buffer != last_index_buffer)
        {
            last_index_buffer = obj.index_buffer;
            cmd.bindIndexBuffer(obj.index_buffer, 0, vk::IndexType::eUint32);
        }

        gpu_draw_push_constants_t push_constants{ .world = obj.transform,
            .vertex_buffer = obj.vertex_buffer_address };
        cmd.pushConstants(obj.material->pipeline->layout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(gpu_draw_push_constants_t), &push_constants);
        cmd.drawIndexed(obj.index_count, 1, obj.first_index, 0, 0);

        this->stats.drawcall_count++;
        this->stats.triangle_count += obj.index_count / 3;
    };

    for (auto& r : opaque_draws)
    {
        draw(this->main_draw_context.opaque_surfaces[r]);
    }
    for (auto& r : this->main_draw_context.transparent_surfaces) draw(r);

    cmd.endRendering();

    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    this->stats.mesh_draw_time = elapsed.count() / 1000.f;
}

void engine_t::draw_background(vk::CommandBuffer cmd)
{
    compute_effect_t& selected = this->background_effects[this->current_bg_effect];
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, selected.pipeline);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, this->gradient_pipeline.layout, 0, this->draw_descriptor.set, {});

    cmd.pushConstants(this->gradient_pipeline.layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(selected.data), &selected.data);
    cmd.dispatch(std::ceil(this->draw_extent.width / 16.0), std::ceil(this->draw_extent.height / 16.0), 1);
}

void engine_t::draw_imgui(vk::CommandBuffer cmd, vk::ImageView target_image_view)
{
    vk::RenderingAttachmentInfo color_attachment(target_image_view, vk::ImageLayout::eGeneral);
    vk::RenderingInfo render_info({}, { vk::Offset2D(0, 0), this->swapchain.extent }, 1, {}, color_attachment);
    cmd.beginRendering(render_info);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    cmd.endRendering();
}

bool engine_t::draw()
{
    this->update_scene();

    vk::Result result = this->device.dev.waitForFences(this->get_current_frame().render_fence, true, 1000000000);
    if (result != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFailed to wait on render fence!\n", ERROR_FMT("ERROR"));
        return false;
    }

    this->get_current_frame().deletion_queue.flush();
    this->get_current_frame().frame_descriptors.clear_pools(this->device.dev);

    std::uint32_t swapchain_img_idx;
    std::tie(result, swapchain_img_idx) = this->device.dev.acquireNextImageKHR(this->swapchain.swapchain, 1000000000,
            this->get_current_frame().swapchain_semaphore, nullptr);
    if (result == vk::Result::eErrorOutOfDateKHR)
    {
        this->window.resize_requested = true;
        return true;
    }
    else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
    {
        fmt::print(stderr, "[ {} ]\tFailed to aquire image!\n", ERROR_FMT("ERROR"));
        return false;
    }

    this->draw_extent.width = std::min(this->swapchain.extent.width, this->draw_image.extent.width) * this->render_scale;
    this->draw_extent.height = std::min(this->swapchain.extent.height, this->draw_image.extent.height) * this->render_scale;

    if ((result = this->device.dev.resetFences(this->get_current_frame().render_fence)) != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFailed to reset render semaphore!\n", ERROR_FMT("ERROR"));
        return false;
    }

    vk::CommandBuffer cmd = this->get_current_frame().buffer;
    if (result = cmd.reset(); result != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFailed to reset command buffer!\n", ERROR_FMT("ERROR"));
        return false;
    }

    // this->draw_extent.width = this->draw_image.extent.width;
    // this->draw_extent.height = this->draw_image.extent.height;
    vk::CommandBufferBeginInfo begin_info(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    if (result = cmd.begin(&begin_info); result != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFailed to begin recording command buffer!\n", ERROR_FMT("ERROR"));
        return false;
    }

    vkutil::transition_image(cmd, this->draw_image.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);

    this->draw_background(cmd);
    
    vkutil::transition_image(cmd, this->draw_image.image, vk::ImageLayout::eGeneral, vk::ImageLayout::eColorAttachmentOptimal);
    vkutil::transition_image(cmd, this->depth_image.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthAttachmentOptimal);

    this->draw_geometry(cmd);

    vkutil::transition_image(cmd, this->draw_image.image, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eTransferSrcOptimal);
    vkutil::transition_image(cmd, this->swapchain.images[swapchain_img_idx], vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
    
    vkutil::copy_image_to_image(cmd, this->draw_image.image, this->swapchain.images[swapchain_img_idx], this->draw_extent, this->swapchain.extent);

    vkutil::transition_image(cmd, this->swapchain.images[swapchain_img_idx], vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eColorAttachmentOptimal);
    
    if (this->use_imgui) this->draw_imgui(cmd, this->swapchain.views[swapchain_img_idx]);
    
    vkutil::transition_image(cmd, this->swapchain.images[swapchain_img_idx], vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR);

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
    result = this->device.present.queue.presentKHR(&present_info);
    if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR)
    {
        this->window.resize_requested = true;
        return true;
    }
    else if (result != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFailed to present!\n", ERROR_FMT("ERROR"));
        return false;
    }

    this->frame_count++;
    return true;
}

bool engine_t::init_vulkan(std::string app_name)
{
    if (this->window.win == nullptr) return false;
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
        .set_app_name(app_name.c_str())
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

    this->main_deletion_queue.push_function([&]() {
            vmaDestroyAllocator(this->allocator);
            });

    if (!this->create_swapchain(this->window.width, this->window.height)) return false;

    // get screen resolution
    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());

    this->draw_image.extent = vk::Extent3D(mode->width, mode->height, 1);//this->window.width, this->window.height, 1);
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

    this->depth_image.format = vk::Format::eD32Sfloat;
    this->depth_image.extent = this->draw_image.extent;
    vk::ImageCreateInfo dimg_info({}, vk::ImageType::e2D, this->depth_image.format, this->depth_image.extent, 1, 1, vk::SampleCountFlagBits::e1,
        vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment);
    vmaCreateImage(this->allocator, (VkImageCreateInfo*)&dimg_info, &rimg_alloc_info, (VkImage*)&this->depth_image.image, &this->depth_image.allocation, nullptr);

    vk::ImageViewCreateInfo dview_info({}, this->depth_image.image, vk::ImageViewType::e2D, this->depth_image.format, {},
            vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1));
    std::tie(result, this->depth_image.view) = this->device.dev.createImageView(dview_info);
    if (result != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tCreating depth image view failed.\n", ERROR_FMT("ERROR"));
        return false;
    }

    this->main_deletion_queue.push_function([=, this]() {
            this->device.dev.destroyImageView(this->draw_image.view);
            vmaDestroyImage(this->allocator, (VkImage)this->draw_image.image, this->draw_image.allocation);
            
            this->device.dev.destroyImageView(this->depth_image.view);
            vmaDestroyImage(this->allocator, (VkImage)this->depth_image.image, this->depth_image.allocation);
            });

    if (!this->init_commands()) return false;
    if (!this->init_sync_structures()) return false;
    if (!this->init_descriptors()) return false;
    if (!this->init_pipelines()) return false;
    if (this->use_imgui)
    {
        if (!this->init_imgui()) return false;
    }
    if (!this->init_default_data()) return false;
    
    this->scene_data.gpu_data.ambient_color = glm::vec4(.1f);
    this->scene_data.gpu_data.sunlight_color = glm::vec4(glm::vec3(.5f), 1.f);
    this->scene_data.gpu_data.sunlight_dir = glm::vec4(0, 1, 0.5, 1.f);

    this->initialized = true;
    return true;
}

bool engine_t::init_commands()
{
    vk::CommandPoolCreateInfo pool_info(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, this->device.graphics.family_index);
    vk::Result result;
        std::vector<vk::CommandBuffer> buf;
    for (std::size_t i = 0; i < FRAME_OVERLAP; ++i)
    {
        std::tie(result, this->frames[i].pool) = this->device.dev.createCommandPool(pool_info);
        if (result != vk::Result::eSuccess)
        {
            fmt::print(stderr, "[ {} ]\tFailed to create command pool!\n", ERROR_FMT("ERROR"));
            return false;
        }
        vk::CommandBufferAllocateInfo alloc_info(this->frames[i].pool, vk::CommandBufferLevel::ePrimary, 1);
        std::tie(result, buf) = this->device.dev.allocateCommandBuffers(alloc_info);
        if (result != vk::Result::eSuccess)
        {
            fmt::print(stderr, "[ {} ]\tFailed to create command buffer!\n", ERROR_FMT("ERROR"));
            return false;
        }
        this->frames[i].buffer = buf[0];
    }

    std::tie(result, this->imm_submit.pool) = this->device.dev.createCommandPool(pool_info);
    if (result != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFailed to create command pool!\n", ERROR_FMT("ERROR"));
        return false;
    }
    vk::CommandBufferAllocateInfo cmd_alloc_info(this->imm_submit.pool, vk::CommandBufferLevel::ePrimary, 1);
    std::tie(result, buf) = this->device.dev.allocateCommandBuffers(cmd_alloc_info);
    if (result != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFailed to create command buffer!\n", ERROR_FMT("ERROR"));
        return false;
    }
    this->imm_submit.cmd = buf[0];

    this->main_deletion_queue.push_function([=, this]() {
            this->device.dev.destroyCommandPool(this->imm_submit.pool);
            });

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
    
    std::tie(result, this->imm_submit.fence) = this->device.dev.createFence(fence_info);
    if (result != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFailed to create fence!\n", ERROR_FMT("ERROR"));
        return false;
    }
    this->main_deletion_queue.push_function([=, this]() {
            this->device.dev.destroyFence(this->imm_submit.fence);
            });

    return true;
}

bool engine_t::init_descriptors()
{
    std::vector<descriptor_allocator_growable_t::pool_size_ratio_t> sizes = { { vk::DescriptorType::eStorageImage, 1 } };
    if (!this->global_descriptor_allocator.init(this->device.dev, 10, sizes)) return false;

    {
        descriptor_layout_builder_t builder;
        auto ret = builder.add_binding(0, vk::DescriptorType::eStorageImage).build(this->device.dev, vk::ShaderStageFlagBits::eCompute);
        if (!ret.has_value()) return false;
        this->draw_descriptor.layout = ret.value();
    }

    {
        descriptor_layout_builder_t builder;
        auto ret = builder.add_binding(0, vk::DescriptorType::eUniformBuffer)
            .build(this->device.dev, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);
        if (!ret.has_value()) return false;
        this->scene_data.layout = ret.value();
    }

    auto ret = this->global_descriptor_allocator.allocate(this->device.dev, this->draw_descriptor.layout);
    if (!ret.has_value()) return false;
    this->draw_descriptor.set = ret.value();

    {
        descriptor_writer_t writer;
        writer.write_image(0, this->draw_image.view, VK_NULL_HANDLE, vk::ImageLayout::eGeneral, vk::DescriptorType::eStorageImage);
        writer.update_set(this->device.dev, this->draw_descriptor.set);
    }

    for (std::size_t i = 0; i < FRAME_OVERLAP; ++i)
    {
        std::vector<descriptor_allocator_growable_t::pool_size_ratio_t> frame_sizes = {
            { vk::DescriptorType::eStorageImage, 3 },
            { vk::DescriptorType::eStorageBuffer, 3 },
            { vk::DescriptorType::eUniformBuffer, 3 },
            { vk::DescriptorType::eCombinedImageSampler, 4 }
        };

        this->frames[i].frame_descriptors = descriptor_allocator_growable_t{};
        if (!this->frames[i].frame_descriptors.init(this->device.dev, 1000, frame_sizes)) return false;

        this->main_deletion_queue.push_function([&, i]() {
                this->frames[i].frame_descriptors.destroy_pools(this->device.dev);
                });
    }

    this->main_deletion_queue.push_function([&]() {
            this->device.dev.destroyDescriptorSetLayout(this->scene_data.layout);
            this->device.dev.destroyDescriptorSetLayout(this->draw_descriptor.layout);
            this->global_descriptor_allocator.destroy_pools(this->device.dev);
            });

    return true;
}

bool engine_t::init_pipelines()
{
    if (!this->init_background_pipelines()) return false;
    return true;//this->metal_rough_material.build_pipelines(this);
}

bool engine_t::init_background_pipelines()
{
    vk::Result result;
    vk::PushConstantRange push_constant(vk::ShaderStageFlagBits::eCompute, 0, sizeof(compute_push_constants_t));
    vk::PipelineLayoutCreateInfo compute_layout({}, this->draw_descriptor.layout, push_constant);
    std::tie(result, this->gradient_pipeline.layout) = this->device.dev.createPipelineLayout(compute_layout);
    if (result != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFailed to create pipeline layout!\n", ERROR_FMT("ERROR"));
        return false;
    }

    std::string base_dir = BASE_DIR;
    auto gradient_shader = vkutil::load_shader_module((base_dir + "/tests/build/shaders/gradient_color.comp.spv").c_str(), this->device.dev);
    if (!gradient_shader.has_value()) return false;
    auto sky_shader = vkutil::load_shader_module((base_dir + "/tests/build/shaders/sky.comp.spv").c_str(), this->device.dev);
    if (!sky_shader.has_value()) return false;

    compute_effect_t gradient{ .name = "gradient", .layout = this->gradient_pipeline.layout };
    gradient.data.data1 = glm::vec4(1, 0, 0, 1);
    gradient.data.data2 = glm::vec4(0, 0, 1, 1);

    vk::PipelineShaderStageCreateInfo stage_info({}, vk::ShaderStageFlagBits::eCompute, gradient_shader.value(), "main");
    vk::ComputePipelineCreateInfo pipeline_info({}, stage_info, this->gradient_pipeline.layout);
    std::tie(result, gradient.pipeline) = this->device.dev.createComputePipeline({}, pipeline_info);
    if (result != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFailed to create compute pipeline!\n", ERROR_FMT("ERROR"));
        return false;
    }

    compute_effect_t sky{ .name = "sky", .layout = this->gradient_pipeline.layout };
    sky.data.data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);

    stage_info.setModule(sky_shader.value());
    pipeline_info.setStage(stage_info);
    std::tie(result, sky.pipeline) = this->device.dev.createComputePipeline({}, pipeline_info);
    if (result != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFailed to create compute pipeline!\n", ERROR_FMT("ERROR"));
        return false;
    }

    this->background_effects.push_back(gradient);
    this->background_effects.push_back(sky);

    this->device.dev.destroyShaderModule(sky_shader.value());
    this->device.dev.destroyShaderModule(gradient_shader.value());
    this->main_deletion_queue.push_function([&]() {
            this->device.dev.destroyPipelineLayout(this->gradient_pipeline.layout);
            for (auto& e : this->background_effects)
                this->device.dev.destroyPipeline(e.pipeline);
            });

    return true;
}

bool engine_t::init_imgui()
{
    vk::DescriptorPoolSize pool_sizes[] = {
        { vk::DescriptorType::eSampler, 1000 },
        { vk::DescriptorType::eCombinedImageSampler, 1000 },
        { vk::DescriptorType::eSampledImage, 1000 },
        { vk::DescriptorType::eStorageImage, 1000 },
        { vk::DescriptorType::eUniformTexelBuffer, 1000 },
        { vk::DescriptorType::eStorageTexelBuffer, 1000 },
        { vk::DescriptorType::eUniformBuffer, 1000 },
        { vk::DescriptorType::eStorageBuffer, 1000 },
        { vk::DescriptorType::eUniformBufferDynamic, 1000 },
        { vk::DescriptorType::eStorageBufferDynamic, 1000 },
        { vk::DescriptorType::eInputAttachment, 1000 },
    };

    vk::DescriptorPoolCreateInfo pool_info(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 1000, pool_sizes);
    auto [result, imgui_pool] = this->device.dev.createDescriptorPool(pool_info);
    if (result != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFailed to create imgui descriptor pool!\n", ERROR_FMT("ERROR"));
        return false;
    }

    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForVulkan(this->window.win, true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = this->instance;
    init_info.PhysicalDevice = this->physical_device;
    init_info.Device = this->device.dev;
    init_info.Queue = this->device.graphics.queue;
    init_info.DescriptorPool = imgui_pool;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.UseDynamicRendering = true;
    init_info.ColorAttachmentFormat = (VkFormat)this->swapchain.format;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info, VK_NULL_HANDLE);
    if (!this->immediate_submit([&](vk::CommandBuffer cmd) { ImGui_ImplVulkan_CreateFontsTexture(); })) return false;

    ImGui_ImplVulkan_DestroyFontsTexture();

    this->main_deletion_queue.push_function([=, this]() {
            ImGui_ImplVulkan_Shutdown();
            this->device.dev.destroyDescriptorPool(imgui_pool);
            });

    return true;
}

bool engine_t::init_default_data()
{
    std::uint32_t white = 0xFFFFFFFF;
    auto wi = this->create_image((void*)&white, vk::Extent3D(1, 1, 1), vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled);
    if (!wi.has_value()) return false;
    this->white_image = wi.value();
    
    std::uint32_t grey = 0xAAAAAAFF;
    auto gi = this->create_image((void*)&grey, vk::Extent3D(1, 1, 1), vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled);
    if (!gi.has_value()) return false;
    this->grey_image = gi.value();

    std::uint32_t black = 0xFF000000;
    auto bi = this->create_image((void*)&black, vk::Extent3D(1, 1, 1), vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled);
    if (!bi.has_value()) return false;
    this->black_image = bi.value();

    std::uint32_t magenta = 0xFFFF00FF;
    std::array<std::uint32_t, 16 * 16> pixels;
    for (std::size_t x = 0; x < 16; ++x)
        for (std::size_t y = 0; y < 16; ++y)
            pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;

    auto err_img = this->create_image(pixels.data(), vk::Extent3D(16, 16, 1), vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled);
    if (!err_img.has_value()) return false;
    this->error_checkerboard_image = err_img.value();

    vk::Result result;
    vk::SamplerCreateInfo sampler({}, vk::Filter::eNearest, vk::Filter::eNearest);
    std::tie(result, this->default_sampler_nearest) = this->device.dev.createSampler(sampler);
    if (result != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFailed to create sampler!\n", ERROR_FMT("ERROR"));
        return false;
    }

    sampler = vk::SamplerCreateInfo({}, vk::Filter::eLinear, vk::Filter::eLinear);
    std::tie(result, this->default_sampler_linear) = this->device.dev.createSampler(sampler);
    if (result != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFailed to create sampler!\n", ERROR_FMT("ERROR"));
        return false;
    }

    this->main_deletion_queue.push_function([&]() {
            this->destroy_image(this->white_image);
            this->destroy_image(this->grey_image);
            this->destroy_image(this->black_image);
            this->destroy_image(this->error_checkerboard_image);
            this->device.dev.destroySampler(this->default_sampler_nearest);
            this->device.dev.destroySampler(this->default_sampler_linear);
            });

    return true;
}

bool engine_t::load_model(std::string path, std::string name)
{
    auto structured_file = load_gltf(this, path);
    if (!structured_file.has_value()) return false;
    this->loaded_scenes[name] = structured_file.value();
    return true;
}

bool engine_t::immediate_submit(std::function<void(vk::CommandBuffer cmd)>&& function)
{
    vk::Result result = this->device.dev.resetFences(this->imm_submit.fence);
    if (result != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFailed to reset fence!\n", ERROR_FMT("ERROR"));
        return false;
    }
    if (result = this->imm_submit.cmd.reset(); result != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFailed to reset command buffer!\n", ERROR_FMT("ERROR"));
        return false;
    }

    vk::CommandBufferBeginInfo begin_info(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    if (result = this->imm_submit.cmd.begin(begin_info); result != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFailed to begin recording command buffer!\n", ERROR_FMT("ERROR"));
        return false;
    }
    function(this->imm_submit.cmd);
    if (result = this->imm_submit.cmd.end(); result != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFailed to end recording command buffer!\n", ERROR_FMT("ERROR"));
        return false;
    }

    vk::CommandBufferSubmitInfo cmd_info(this->imm_submit.cmd);
    vk::SubmitInfo2 submit_info({}, {}, cmd_info);

    if (result = this->device.graphics.queue.submit2(submit_info, this->imm_submit.fence); result != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFailed to submit command buffer!\n", ERROR_FMT("ERROR"));
        return false;
    }

    if (result = this->device.dev.waitForFences(this->imm_submit.fence, true, 9999999999); result != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFailed to wait for fences!\n", ERROR_FMT("ERROR"));
        return false;
    }

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

bool engine_t::resize_swapchain()
{
    if (this->device.dev.waitIdle() != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFaild to wait!\n", ERROR_FMT("ERROR"));
        return false;
    }
    this->destroy_swapchain();
    int w, h;
    glfwGetWindowSize(this->window.win, &w, &h);
    this->window.width = w;
    this->window.height = h;
    
    if (!this->create_swapchain(w, h)) return false;

    this->window.resize_requested = false;
    return true;
}

void engine_t::destroy_swapchain()
{
    for (vk::ImageView view : this->swapchain.views)
        this->device.dev.destroyImageView(view);
    this->device.dev.destroySwapchainKHR(this->swapchain.swapchain);
    this->swapchain.images.clear();
    this->swapchain.views.clear();
}

std::optional<allocated_buffer_t> engine_t::create_buffer(std::size_t alloc_size, vk::BufferUsageFlags usage, VmaMemoryUsage memory_usage)
{
    vk::BufferCreateInfo buffer_info({}, alloc_size, usage);
    VmaAllocationCreateInfo vma_alloc_info{ .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT, .usage = memory_usage };
    allocated_buffer_t buf;
    if (vmaCreateBuffer(this->allocator, (VkBufferCreateInfo*)&buffer_info, &vma_alloc_info, (VkBuffer*)&buf.buffer, &buf.allocation, &buf.info) != VK_SUCCESS)
    {
        fmt::print(stderr, "[ {} ]\tFailed to create buffer!\n", ERROR_FMT("ERROR"));
        return std::nullopt;
    }
    return buf;
}

void engine_t::destroy_buffer(const allocated_buffer_t& buf)
{
    vmaDestroyBuffer(this->allocator, (VkBuffer)buf.buffer, buf.allocation);
}

std::optional<allocated_image_t> engine_t::create_image(vk::Extent3D size, vk::Format format, vk::ImageUsageFlags usage, bool mipmapped)
{
    allocated_image_t new_img;
    new_img.format = format;
    new_img.extent = size;

    vk::ImageCreateInfo img_info({}, vk::ImageType::e2D, format, size, 1, 1, vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal, usage);
    if (mipmapped) img_info.mipLevels = static_cast<std::uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;

    VmaAllocationCreateInfo alloc_info = { .usage = VMA_MEMORY_USAGE_GPU_ONLY, .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };
    if (vmaCreateImage(this->allocator, (VkImageCreateInfo*)&img_info, &alloc_info, (VkImage*)&new_img.image, &new_img.allocation, nullptr) != VK_SUCCESS)
    {
        fmt::print(stderr, "[ {} ]\tFailed to create image!\n", ERROR_FMT("ERROR"));
        return std::nullopt;
    }

    vk::ImageAspectFlags aspect_flag = vk::ImageAspectFlagBits::eColor;
    if (format == vk::Format::eD32Sfloat) aspect_flag = vk::ImageAspectFlagBits::eDepth;

    vk::ImageViewCreateInfo view_info({}, new_img.image, vk::ImageViewType::e2D, format, {}, vk::ImageSubresourceRange(aspect_flag, 0, 1, 0, 1));
    view_info.subresourceRange.levelCount = img_info.mipLevels;

    vk::Result result;
    std::tie(result, new_img.view) = this->device.dev.createImageView(view_info);
    if (result != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFailed to create image view!\n", ERROR_FMT("ERROR"));
        return std::nullopt;
    }

    return new_img;
}

std::optional<allocated_image_t> engine_t::create_image(void* data, vk::Extent3D size, vk::Format format, vk::ImageUsageFlags usage, bool mipmapped)
{
    std::size_t data_size = size.depth * size.width * size.height * 4;
    auto upload_buffer = create_buffer(data_size, vk::BufferUsageFlagBits::eTransferSrc, VMA_MEMORY_USAGE_CPU_TO_GPU);
    if (!upload_buffer.has_value()) return std::nullopt;
    std::memcpy(upload_buffer.value().info.pMappedData, data, data_size);
    auto new_img = this->create_image(size, format, usage | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc, mipmapped);
    if (!new_img.has_value()) return std::nullopt;

    this->immediate_submit([&](vk::CommandBuffer cmd) {
            vkutil::transition_image(cmd, new_img.value().image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

            vk::BufferImageCopy copy_region(0, 0, 0, vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1), {}, size);
            cmd.copyBufferToImage(upload_buffer.value().buffer, new_img.value().image, vk::ImageLayout::eTransferDstOptimal, copy_region);

            if (mipmapped)
                vkutil::generate_mipmaps(cmd, new_img.value().image, vk::Extent2D(new_img.value().extent.width, new_img.value().extent.height));
            else
                vkutil::transition_image(cmd, new_img.value().image, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
            });

    this->destroy_buffer(upload_buffer.value());

    return new_img.value();
}

void engine_t::destroy_image(const allocated_image_t& img)
{
    this->device.dev.destroyImageView(img.view);
    vmaDestroyImage(this->allocator, img.image, img.allocation);
}

std::optional<gpu_mesh_buffer_t> engine_t::upload_mesh(std::span<std::uint32_t> indices, std::span<vertex_t> vertices)
{
    const std::size_t vertex_buffer_size = vertices.size() * sizeof(vertex_t);
    const std::size_t index_buffer_size = indices.size() * sizeof(std::uint32_t);
    gpu_mesh_buffer_t buf;
    
    auto ret = this->create_buffer(vertex_buffer_size, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst
            | vk::BufferUsageFlagBits::eShaderDeviceAddress, VMA_MEMORY_USAGE_GPU_ONLY);
    if (!ret.has_value()) return std::nullopt;
    buf.vertex_buffer = ret.value();

    vk::BufferDeviceAddressInfo device_address_info(buf.vertex_buffer.buffer);
    buf.vertex_buffer_address = this->device.dev.getBufferAddress(&device_address_info);

    ret = this->create_buffer(index_buffer_size, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, VMA_MEMORY_USAGE_GPU_ONLY);
    if (!ret.has_value()) return std::nullopt;
    buf.index_buffer = ret.value();
    
    auto staging = this->create_buffer(vertex_buffer_size + index_buffer_size, vk::BufferUsageFlagBits::eTransferSrc, VMA_MEMORY_USAGE_CPU_ONLY);
    if (!staging.has_value()) return std::nullopt;

    void* data = staging.value().info.pMappedData;
    std::memcpy(data, vertices.data(), vertex_buffer_size);
    std::memcpy((char*)data + vertex_buffer_size, indices.data(), index_buffer_size);

    this->immediate_submit([&](vk::CommandBuffer cmd)
            {
            vk::BufferCopy vertex_copy( {}, {}, vertex_buffer_size );
            cmd.copyBuffer(staging.value().buffer, buf.vertex_buffer.buffer, vertex_copy);
            vk::BufferCopy index_copy( vertex_buffer_size, 0, index_buffer_size );
            cmd.copyBuffer(staging.value().buffer, buf.index_buffer.buffer, index_copy);
            });

    this->destroy_buffer(staging.value());

    return buf;
}

frame_data_t& engine_t::get_current_frame()
{
    return this->frames[this->frame_count % FRAME_OVERLAP];
}
