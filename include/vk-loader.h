#pragma once

#include <vk-types.h>
#include <vk-descriptors.h>
#include <optional>
#include <memory>
#include <unordered_map>

struct gltf_material_t
{
    material_instance_t data;
};

struct bounds_t
{
    glm::vec3 origin;
    float sphere_radius;
    glm::vec3 extents;
};

struct surface_t
{
    std::uint32_t start_index;
    std::uint32_t count;
    bounds_t bounds;
    std::shared_ptr<gltf_material_t> material;
};

struct mesh_asset_t
{
    std::string name;
    std::vector<surface_t> surfaces;
    gpu_mesh_buffer_t mesh_buffer;
};

struct engine_t;

struct loaded_gltf_t : public renderable_i
{
    std::unordered_map<std::string, std::shared_ptr<mesh_asset_t>> meshes;
    std::unordered_map<std::string, std::shared_ptr<node_t>> nodes;
    std::unordered_map<std::string, allocated_image_t> images;
    std::unordered_map<std::string, std::shared_ptr<gltf_material_t>> materials;

    std::vector<std::shared_ptr<node_t>> top_nodes;
    std::vector<vk::Sampler> samplers;

    descriptor_allocator_growable_t descriptor_pool;
    allocated_buffer_t material_data_buffer;
    engine_t* creator;

    virtual void draw(const glm::mat4& top_matrix, draw_context_t& ctx) override;

    void clear_all();
    virtual ~loaded_gltf_t() { this->clear_all(); };
};

std::optional<std::shared_ptr<loaded_gltf_t>> load_gltf(engine_t* engine, std::string_view filepath);
