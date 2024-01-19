#pragma once

#include <vk-types.h>
#include <optional>
#include <memory>
#include <unordered_map>
#include <filesystem>

struct gltf_material_t
{
    material_instance_t data;
};

struct surface_t
{
    std::uint32_t start_index;
    std::uint32_t count;
    std::shared_ptr<gltf_material_t> material;
};

struct mesh_asset_t
{
    std::string name;
    std::vector<surface_t> surfaces;
    gpu_mesh_buffer_t mesh_buffer;
};

struct engine_t;

std::optional<std::vector<std::shared_ptr<mesh_asset_t>>> load_gltf_meshes(engine_t* engine, std::filesystem::path filepath);
