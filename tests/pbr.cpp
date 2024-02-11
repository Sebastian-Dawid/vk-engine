#include <imgui.h>
#include <filesystem>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <vk-engine.h>

int main()
{
    std::string file = "/tests/assets/sgb.glb";
    std::string pwd = std::filesystem::current_path().string();
    engine_t engine(1024, 1024, "pbr", true, true);

    camera_t cam{ .position = glm::vec3(0.f, 0.f, 2.f) };
    glfwSetWindowUserPointer(engine.window.win, &cam);

    engine.init_pipelines = [&]() -> bool { return engine.init_background_pipelines(); };

    if (!engine.init_vulkan("pbr")) return EXIT_FAILURE;
    std::vector<vk::VertexInputBindingDescription> input_bindings = { vk::VertexInputBindingDescription(0, sizeof(glm::mat4), vk::VertexInputRate::eInstance) };
    std::vector<vk::VertexInputAttributeDescription> input_attriubtes = { vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32A32Sfloat, 0),
        vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32A32Sfloat, sizeof(float) * 4),
        vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32B32A32Sfloat, sizeof(float) * 8),
        vk::VertexInputAttributeDescription(3, 0, vk::Format::eR32G32B32A32Sfloat, sizeof(float) * 12)
    };
    std::vector<vk::Format> formats = { engine.draw_image.format, engine.draw_image.format, engine.draw_image.format };
    if (!engine.metal_rough_material.build_pipelines(&engine, pwd + "/tests/build/shaders/pbr.vert.spv", pwd + "/tests/build/shaders/pbr.frag.spv",
                sizeof(gpu_draw_push_constants_t), { {0, vk::DescriptorType::eUniformBuffer}, {1, vk::DescriptorType::eCombinedImageSampler}, {2, vk::DescriptorType::eCombinedImageSampler} },
                {engine.scene_data.layout}, input_bindings, input_attriubtes, formats)) return EXIT_FAILURE;
    engine.load_model(pwd + file, "sgb");
    engine.loaded_scenes["sgb"]->transform.push_back(glm::scale(glm::mat4(1), glm::vec3(0.01f, 0.01f, 0.01f)));

    engine.define_imgui_windows = [&]()
    {
        if (ImGui::Begin("Resolution"))
        {
            ImGui::SliderFloat("Render Scale",&engine.render_scale, 0.01f, 1.f);
            ImGui::Text("Render Resolution: (%d, %d)", engine.draw_extent.width, engine.draw_extent.height);
            ImGui::Text("Window Resolution: (%d, %d)", engine.swapchain.extent.width, engine.swapchain.extent.height);
            ImGui::Text("Buffer Resolution: (%d, %d)", engine.draw_image.extent.width, engine.draw_image.extent.height);
            ImGui::End();
        }

        if (ImGui::Begin("Light"))
        {
            ImGui::ColorEdit4("ambient color", (float*) &engine.scene_data.gpu_data.ambient_color);
            ImGui::ColorEdit4("light color", (float*) &engine.scene_data.gpu_data.sunlight_color);
            ImGui::InputFloat4("light dir", (float*) &engine.scene_data.gpu_data.sunlight_dir);
            ImGui::End();
        }
    };

    engine.input_handler = [&]()
    {
        if (glfwGetKey(engine.window.win, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(engine.window.win, GLFW_TRUE);
        cam.process_glfw_event(engine.window.win);
    };

    engine.update = [&]()
    {
        engine.background_effects[0].data.data3.x = engine.window.width;
        engine.background_effects[0].data.data3.y = engine.window.height;
        engine.background_effects[0].data.data3.z = engine.render_scale;
        cam.update();

        engine.scene_data.gpu_data.view = cam.get_view_matrix();
        engine.scene_data.gpu_data.proj = glm::perspective(glm::radians(70.f), (float)engine.window.width / (float)engine.window.height, .1f, 10000.f);
        engine.scene_data.gpu_data.proj[1][1] *= -1;
        engine.scene_data.gpu_data.viewproj = engine.scene_data.gpu_data.proj * engine.scene_data.gpu_data.view ;
    };

    for (std::uint8_t i = 0; i < 3; ++i)
    {
        engine.color_images.push_back(engine.create_image(engine.draw_image.extent, engine.draw_image.format,
            vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eColorAttachment).value());
    }
    engine.main_deletion_queue.push_function([&](){ for (auto img : engine.color_images) engine.destroy_image(img); });

    engine.draw_cmd = [&](vk::CommandBuffer cmd, std::uint32_t swapchain_img_idx) -> vk::ImageLayout
    {
        vkutil::transition_image(cmd, engine.draw_image.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);
        vkutil::transition_image(cmd, engine.depth_image.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthAttachmentOptimal);

        vk::ClearValue clear_value;
        clear_value.depthStencil.depth = 1.f;
        std::vector<vk::RenderingAttachmentInfo> color_attachments = {
            vk::RenderingAttachmentInfo(engine.color_images[0].view, vk::ImageLayout::eColorAttachmentOptimal, {}, {}, {}, vk::AttachmentLoadOp::eClear,
                    vk::AttachmentStoreOp::eStore, vk::ClearValue(vk::ClearColorValue(std::array<float, 4>{0}))),
            vk::RenderingAttachmentInfo(engine.color_images[1].view, vk::ImageLayout::eColorAttachmentOptimal, {}, {}, {}, vk::AttachmentLoadOp::eClear,
                    vk::AttachmentStoreOp::eStore, vk::ClearValue(vk::ClearColorValue(std::array<float, 4>{0}))),
            vk::RenderingAttachmentInfo(engine.color_images[2].view, vk::ImageLayout::eColorAttachmentOptimal, {}, {}, {}, vk::AttachmentLoadOp::eClear,
                    vk::AttachmentStoreOp::eStore, vk::ClearValue(vk::ClearColorValue(std::array<float, 4>{0})))
        };
        vk::RenderingAttachmentInfo depth_attachment(engine.depth_image.view, vk::ImageLayout::eDepthAttachmentOptimal, {}, {}, {}, vk::AttachmentLoadOp::eClear,
                vk::AttachmentStoreOp::eStore, clear_value);

        engine.draw_geometry(cmd, color_attachments, depth_attachment);

        vkutil::transition_image(cmd, engine.draw_image.image, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eTransferSrcOptimal);
        vkutil::transition_image(cmd, engine.swapchain.images[swapchain_img_idx], vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

        vkutil::copy_image_to_image(cmd, engine.draw_image.image, engine.swapchain.images[swapchain_img_idx], engine.draw_extent, engine.swapchain.extent);
        return vk::ImageLayout::eTransferDstOptimal;
    };

    engine.run();
    return EXIT_SUCCESS;
}
