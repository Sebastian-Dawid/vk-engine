#pragma once

#include <glm/glm.hpp>
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

struct allocated_buffer_t
{
    vk::Buffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
};

struct vertex_t
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec4 color;
};

struct gpu_mesh_buffer_t
{
    allocated_buffer_t index_buffer;
    allocated_buffer_t vertex_buffer;
    vk::DeviceAddress vertex_buffer_address;
};

struct gpu_draw_push_constants_t
{
    glm::mat4 world;
    vk::DeviceAddress vertex_buffer;
};
