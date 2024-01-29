#include "imgui.h"
#include <cstdlib>
#include <filesystem>
#include <glm/ext/matrix_clip_space.hpp>
#include <vk-engine.h>

int main(int argc, char** argv)
{
    std::string file = "/tests/assets/structure.glb";
    if (argc > 1) file = argv[1];
    std::string pwd = std::filesystem::current_path().string();
    engine_t engine(2048, 2048, "setup-test", true, true);
    
    camera_t cam{ .position = glm::vec3(0.f, 0.f, 2.f) };
    glfwSetWindowUserPointer(engine.window.win, &cam);
    
    if (!engine.init_vulkan("setup-test"))
    {
        return EXIT_FAILURE;
    }
    if (!engine.metal_rough_material.build_pipelines(&engine, pwd + "/tests/build/shaders/mesh.vert.spv", pwd + "/tests/build/shaders/mesh.frag.spv",
                sizeof(gpu_draw_push_constants_t), { {0, vk::DescriptorType::eUniformBuffer}, {1, vk::DescriptorType::eCombinedImageSampler}, {2, vk::DescriptorType::eCombinedImageSampler} },
                { engine.scene_data.layout }))
    {
        return EXIT_FAILURE;
    }
    engine.load_model(pwd + file, "structure");

    engine.define_imgui_windows = [&]()
    {
        if (ImGui::Begin("background"))
        {
            compute_effect_t& selected = engine.background_effects[engine.current_bg_effect];
            ImGui::Text("Selected effect: %s", selected.name);
            ImGui::SliderInt("Effect Index", (int*) &engine.current_bg_effect, 0, engine.background_effects.size() - 1);
            ImGui::InputFloat4("data1", (float*) &selected.data.data1);
            ImGui::InputFloat4("data2", (float*) &selected.data.data2);

            ImGui::Text("Light:");
            ImGui::ColorEdit4("ambient color", (float*) &engine.scene_data.gpu_data.ambient_color);
            ImGui::ColorEdit4("light color", (float*) &engine.scene_data.gpu_data.sunlight_color);
            ImGui::InputFloat4("light dir", (float*) &engine.scene_data.gpu_data.sunlight_dir);

            ImGui::Text("Info:");
            ImGui::SliderFloat("Render Scale",&engine.render_scale, 0.01f, 1.f);
            ImGui::Text("Render Resolution: (%d, %d)", engine.draw_extent.width, engine.draw_extent.height);
            ImGui::Text("Window Resolution: (%d, %d)", engine.swapchain.extent.width, engine.swapchain.extent.height);
            ImGui::Text("Buffer Resolution: (%d, %d)", engine.draw_image.extent.width, engine.draw_image.extent.height);

            ImGui::Text("Misc:");
            ImGui::SliderFloat("Cam Speed",&cam.speed, 1.f, 10.f);

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

    engine.run();
    return EXIT_SUCCESS;
}
