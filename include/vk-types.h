#pragma once

#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>

struct allocated_image_t
{
    vk::Image image;
    vk::ImageView view;
    VmaAllocation allocation;
    vk::Extent3D extent;
    vk::Format format;
};
