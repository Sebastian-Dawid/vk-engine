#include <imgui.h>
#include <filesystem>
#include <glm/ext/matrix_transform.hpp>
#include <vk-engine.h>

int main()
{
    std::string file = "/tests/assets/sgb.glb";
    std::string pwd = std::filesystem::current_path().string();
    engine_t engine(1024, 1024, "pbr", true, true);
    if (!engine.init_vulkan("pbr")) return EXIT_FAILURE;
    if (!engine.metal_rough_material.build_pipelines(&engine, pwd + "/tests/build/shaders/mesh.vert.spv", pwd + "/tests/build/shaders/mesh.frag.spv",
                sizeof(gpu_draw_push_constants_t), { {0, vk::DescriptorType::eUniformBuffer}, {1, vk::DescriptorType::eCombinedImageSampler}, {2, vk::DescriptorType::eCombinedImageSampler} },
                {engine.scene_data.layout}, { vk::VertexInputBindingDescription(0, sizeof(glm::mat4), vk::VertexInputRate::eInstance) },
                { vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32A32Sfloat, 0),
                vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32A32Sfloat, sizeof(float) * 4),
                vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32B32A32Sfloat, sizeof(float) * 8),
                vk::VertexInputAttributeDescription(3, 0, vk::Format::eR32G32B32A32Sfloat, sizeof(float) * 12)})) return EXIT_FAILURE;
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

    engine.run();
    return EXIT_SUCCESS;
}
