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
    alignas(16) glm::vec3 position;
    alignas(16) glm::vec3 normal;
    alignas(16) glm::vec2 uv;
    alignas(16) glm::vec4 color;
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

enum struct material_pass_e : std::uint8_t
{
    MAIN_COLOR,
    TRANSPARENT,
    OTHER
};

struct material_pipeline_t
{
    vk::Pipeline pipeline;
    vk::PipelineLayout layout;
};

struct material_instance_t
{
    material_pipeline_t* pipeline;
    vk::DescriptorSet material_set;
    material_pass_e pass_type;
};
