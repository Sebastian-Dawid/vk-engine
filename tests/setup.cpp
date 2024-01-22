#include <cstdlib>
#include <filesystem>
#include <vk-engine.h>

int main(int argc, char** argv)
{
    engine_t engine;
    if (!engine.init_vulkan())
    {
        return EXIT_FAILURE;
    }
    engine.load_model(std::filesystem::current_path().string() + "/tests/assets/structure.glb", "structure");
    engine.load_model(std::filesystem::current_path().string() + "/tests/assets/basicmesh.glb", "basicmesh");

    engine.run();
    return EXIT_SUCCESS;
}
