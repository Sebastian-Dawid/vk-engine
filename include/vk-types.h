#pragma once

#include <glm/glm.hpp>
#include <memory>
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

struct draw_context_t;

struct renderable_i
{
    virtual void draw(const glm::mat4& top_matrix, draw_context_t& ctx) = 0;
};

struct node_t : public renderable_i
{
    std::weak_ptr<node_t> parent;
    std::vector<std::shared_ptr<node_t>> children;

    glm::mat4 local_transform;
    glm::mat4 world_transform;

    void refresh_transform(const glm::mat4& parent_matrix)
    {
        this->world_transform = parent_matrix * this->local_transform;
        for (auto c : this->children)
        {
            c->refresh_transform(this->world_transform);
        }
    }

    virtual void draw(const glm::mat4& top_matrix, draw_context_t& ctx) override
    {
        for (auto& c : this->children)
        {
            c->draw(top_matrix, ctx);
        }
    }
    virtual ~node_t(){};
};
