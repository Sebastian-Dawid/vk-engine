#include <cstdlib>
#include <filesystem>
#include <vk-engine.h>

int main(int argc, char** argv)
{
    std::string file = "/tests/assets/structure.glb";
    if (argc > 1) file = argv[1];
    std::string pwd = std::filesystem::current_path().string();
    engine_t engine(2048, 2048, "setup-test", true, true);
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

    engine.run();
    return EXIT_SUCCESS;
}
