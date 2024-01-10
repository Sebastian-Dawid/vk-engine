#include <cstdlib>
#include <vk-engine.h>

int main(int argc, char** argv)
{
    engine_t engine;
    if (!engine.init_vulkan())
    {
        return EXIT_FAILURE;
    }
    engine.run();
    return EXIT_SUCCESS;
}
