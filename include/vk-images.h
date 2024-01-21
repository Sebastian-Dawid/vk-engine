#pragma once

#include <vulkan/vulkan.hpp>

namespace vkutil {
    void transition_image(vk::CommandBuffer cmd, vk::Image img, vk::ImageLayout current, vk::ImageLayout target);
    void copy_image_to_image(vk::CommandBuffer cmd, vk::Image src, vk::Image dst, vk::Extent2D src_size, vk::Extent2D dst_size);
    void generate_mipmaps(vk::CommandBuffer cmd, vk::Image image, vk::Extent2D image_size);
};
