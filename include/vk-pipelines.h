#pragma once

#define VULKAN_HPP_NO_EXCEPTIONS
#include <vulkan/vulkan.hpp>

namespace vkutil {
    std::optional<vk::ShaderModule> load_shader_module(const char* file_path, vk::Device device);
};
