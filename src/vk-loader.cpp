#include <vk-loader.h>
#include <stb_image.h>

#include <vk-engine.h>
#include <vk-types.h>
#include <glm/gtx/quaternion.hpp>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>

#include <error_fmt.h>

std::optional<std::vector<std::shared_ptr<mesh_asset_t>>> load_gltf_meshes(engine_t *engine, std::filesystem::path filepath)
{
#ifdef DEBUG
    fmt::print("[ {} ]\tLoading GLFT: {}\n", INFO_FMT("INFO"), filepath.c_str());
#endif
    fastgltf::GltfDataBuffer data;
    data.loadFromFile(filepath);

    constexpr auto gltf_options = fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;

    fastgltf::Asset gltf;
    fastgltf::Parser parser{};

    auto load = parser.loadGltfBinary(&data, filepath.parent_path(), gltf_options);
    if (load)
    {
        gltf = std::move(load.get());
    }
    else
    {
        fmt::print(stderr, "[ {} ]\tFailed to load GLTF!\n", ERROR_FMT("ERROR"));
        return std::nullopt;
    }

    std::vector<std::shared_ptr<mesh_asset_t>> meshes;

    std::vector<std::uint32_t> indices;
    std::vector<vertex_t> vertices;
    for (fastgltf::Mesh& mesh : gltf.meshes)
    {
        mesh_asset_t new_mesh;
        new_mesh.name = mesh.name;

        indices.clear();
        vertices.clear();

        for (auto&& p : mesh.primitives)
        {
            surface_t new_surface;
            new_surface.start_index = indices.size();
            new_surface.count = gltf.accessors[p.indicesAccessor.value()].count;

            std::size_t initial_vtx = vertices.size();

            {
                fastgltf::Accessor& index_accessor = gltf.accessors[p.indicesAccessor.value()];
                indices.reserve(indices.size() + index_accessor.count);

                fastgltf::iterateAccessor<std::uint32_t>(gltf, index_accessor,
                        [&](std::uint32_t idx) {
                        indices.push_back(idx + initial_vtx);
                        });
            }

            {
                fastgltf::Accessor& pos_accessor = gltf.accessors[p.findAttribute("POSITION")->second];
                vertices.resize(vertices.size() + pos_accessor.count);

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, pos_accessor, [&](glm::vec3 v, std::size_t idx) {
                        vertex_t new_vtx;
                        new_vtx.position = v;
                        new_vtx.normal = { 1, 0, 0 };
                        new_vtx.color = glm::vec4(1.f);
                        new_vtx.uv = { 0, 0 };
                        vertices[initial_vtx + idx] = new_vtx;
                        });
            }

            auto normals = p.findAttribute("NORMAL");
            if (normals != p.attributes.end())
            {
                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[normals->second], [&](glm::vec3 v, std::size_t idx) {
                        vertices[initial_vtx + idx].normal = v;
                        });
            }

            auto uv = p.findAttribute("TEXCOORD_0");
            if (uv != p.attributes.end())
            {
                fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[uv->second], [&](glm::vec2 v, std::size_t idx) {
                        vertices[initial_vtx + idx].uv = v;
                        });
            }

            auto color = p.findAttribute("COLOR_0");
            if (color != p.attributes.end())
            {
                fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[color->second], [&](glm::vec4 v, std::size_t idx) {
                        vertices[initial_vtx + idx].color = v;
                        });
            }

            new_mesh.surfaces.push_back(new_surface);
        }
        constexpr bool override_colors = true;
        if (override_colors)
        {
            for (vertex_t& vtx : vertices) vtx.color = glm::vec4(vtx.normal, 1.f);
        }
        auto ret = engine->upload_mesh(indices, vertices);
        if (!ret.has_value()) return std::nullopt;
        new_mesh.mesh_buffer = ret.value();
        meshes.emplace_back(std::make_shared<mesh_asset_t>(std::move(new_mesh)));
    }

#ifdef DEBUG
    fmt::print("[ {} ]\tFinished loading GLFT: {}\n", INFO_FMT("INFO"), filepath.c_str());
#endif
    return meshes;
}
