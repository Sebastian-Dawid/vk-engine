#include <cstdint>
#include <vk-pipelines.h>
#include <fstream>
#include <error_fmt.h>

std::optional<vk::ShaderModule> vkutil::load_shader_module(const char *file_path, vk::Device device)
{
    std::ifstream file(file_path, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        fmt::print(stderr, "[ {} ]\tFailed to open file '{}'!\n", ERROR_FMT("ERROR"), fmt::styled(file_path, fmt::emphasis::bold | fmt::emphasis::underline));
        return std::nullopt;
    }

    std::size_t file_size = file.tellg();
    std::vector<std::uint32_t> buffer(file_size / sizeof(std::uint32_t));
    file.seekg(0);
    file.read((char*)buffer.data(), file_size);
    file.close();
    vk::ShaderModuleCreateInfo create_info({}, buffer);

    auto [result, module] = device.createShaderModule(create_info);
    if (result != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFailed to create shader module!\n", ERROR_FMT("ERROR"));
        return std::nullopt;
    }
    return module;
}
