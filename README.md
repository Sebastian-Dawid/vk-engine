# vk-engine

## Build Library

The default build target is `debug`.
```bash
$ make lib # CONFIG=debug/release
```

## Build Examples

The default build target is `debug`. The default example is `setup`.
```bash
$ make run # CONFIG=debug/release, ARGS=<cmd line args>
```

## References
* [Vulkan Guide](https://vkguide.dev/)
* [Vulkan Tutorial](https://vulkan-tutorial.com/)
* [Sascha Willems (Vulkan Repo)](https://github.com/SaschaWillems/Vulkan)

## Used Libraries
* [Vulkan-Hpp](https://github.com/KhronosGroup/Vulkan-Hpp)
* [VMA](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)
* [VkBootstrap](https://github.com/charles-lunarg/vk-bootstrap)
* [glm](https://github.com/g-truc/glm)
* [fmt](https://github.com/fmtlib/fmt)
* [ImGui](https://github.com/ocornut/imgui)
* [fastgltf](https://github.com/spnda/fastgltf)
    * [simdjson](https://github.com/simdjson/simdjson)
* [stb](https://github.com/nothings/stb)
