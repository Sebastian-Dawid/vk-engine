#pragma once

#include <vulkan/vulkan.hpp>

namespace vkutil {
    void transition_image(vk::CommandBuffer cmd, vk::Image img, vk::ImageLayout current, vk::ImageLayout target);
};
