#include <cstdlib>
#include <filesystem>
#include <vk-engine.h>

int main(int argc, char** argv)
{
    std::string file = "/tests/assets/structure.glb";
    if (argc > 1) file = argv[1];
    engine_t engine(2048, 2048, "setup-test");
    if (!engine.init_vulkan("setup-test"))
    {
        return EXIT_FAILURE;
    }
    engine.load_model(std::filesystem::current_path().string() + file, "structure");

    engine.run();
    return EXIT_SUCCESS;
}
