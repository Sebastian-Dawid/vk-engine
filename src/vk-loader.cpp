#include <vk-loader.h>
#include <stb_image.h>

#include <vk-engine.h>
#include <vk-types.h>
#include <glm/gtx/quaternion.hpp>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>

#include <filesystem>
#include <error_fmt.h>

std::optional<allocated_image_t> load_image(engine_t* engine, fastgltf::Asset& asset, fastgltf::Image& image)
{
    allocated_image_t new_image {};
    int width, height, nr_channels;

    std::visit(
        fastgltf::visitor {
            [](auto& arg) {},
            [&](fastgltf::sources::URI& filepath) {
                assert(filepath.fileByteOffset == 0);
                assert(filepath.uri.isLocalPath());
                const std::string path(filepath.uri.path().begin(), filepath.uri.path().end());
                unsigned char* data = stbi_load(path.c_str(), &width, &height, &nr_channels, 4);
                if (data)
                {
                    vk::Extent3D image_size(width, height, 1);
                    auto ret = engine->create_image(data, image_size, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled, true);
                    assert(ret.has_value());
                    new_image = ret.value();
                    stbi_image_free(data);
                }
            },
            [&](fastgltf::sources::Vector& vector) {
                unsigned char* data = stbi_load_from_memory(vector.bytes.data(), vector.bytes.size(), &width, &height, &nr_channels, 4);
                if (data)
                {
                    vk::Extent3D image_size(width, height, 1);
                    auto ret = engine->create_image(data, image_size, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled, true);
                    assert(ret.has_value());
                    new_image = ret.value();
                    stbi_image_free(data);
                }
            },
            [&](fastgltf::sources::BufferView& view)
            {
                auto& buffer_view = asset.bufferViews[view.bufferViewIndex];
                auto& buffer = asset.buffers[buffer_view.bufferIndex];

                std::visit(fastgltf::visitor {
                        [](auto& arg){},
                        [&](fastgltf::sources::Vector& vector)
                        {
                            unsigned char* data = stbi_load_from_memory(vector.bytes.data() + buffer_view.byteOffset, buffer_view.byteLength,
                                &width, &height, &nr_channels, 4);
                            if (data)
                            {
                                vk::Extent3D image_size(width, height, 1);
                                auto ret = engine->create_image(data, image_size, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled, true);
                                assert(ret.has_value());
                                new_image = ret.value();
                                stbi_image_free(data);
                            }
                        }
                    }, buffer.data);
            } }, image.data);

    if (new_image.image == VK_NULL_HANDLE) return std::nullopt;
    return new_image;
}

vk::Filter extract_filter(fastgltf::Filter filter)
{
    switch (filter)
    {
        case fastgltf::Filter::Nearest:
        case fastgltf::Filter::NearestMipMapNearest:
        case fastgltf::Filter::NearestMipMapLinear:
            return vk::Filter::eNearest;
        case fastgltf::Filter::Linear:
        case fastgltf::Filter::LinearMipMapNearest:
        case fastgltf::Filter::LinearMipMapLinear:
        default:
            return vk::Filter::eLinear;
    }
}

vk::SamplerMipmapMode extract_mipmap_mode(fastgltf::Filter filter)
{
    switch (filter)
    {
        case fastgltf::Filter::NearestMipMapNearest:
        case fastgltf::Filter::LinearMipMapNearest:
            return vk::SamplerMipmapMode::eNearest;
        case fastgltf::Filter::NearestMipMapLinear:
        case fastgltf::Filter::LinearMipMapLinear:
        default:
            return vk::SamplerMipmapMode::eLinear;
    }
}

std::optional<std::shared_ptr<loaded_gltf_t>> load_gltf(engine_t* engine, std::string_view filepath)
{
#ifdef DEBUG
    fmt::print("[ {} ]\tLoading glTF: {}\n", INFO_FMT("INFO"), filepath);
#endif

    std::shared_ptr<loaded_gltf_t> scene = std::make_shared<loaded_gltf_t>();
    scene->creator = engine;
    loaded_gltf_t& file = *scene.get();

    fastgltf::Parser parser{};

    constexpr auto gltf_options = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble
        | fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;
    fastgltf::GltfDataBuffer data;
    data.loadFromFile(filepath);
    fastgltf::Asset gltf;
    std::filesystem::path path = filepath;

    auto type  = fastgltf::determineGltfFileType(&data);
    if (type == fastgltf::GltfType::glTF)
    {
        auto load = parser.loadGltf(&data, path.parent_path(), gltf_options);
        if (load)
        {
            gltf = std::move(load.get());
        }
        else
        {
            fmt::print(stderr, "[ {} ]\tFailed to load glTF: {}\n", ERROR_FMT("ERROR"), fastgltf::to_underlying(load.error()));
            return std::nullopt;
        }
    }
    else if (type == fastgltf::GltfType::GLB)
    {
        auto load = parser.loadGltfBinary(&data, path.parent_path(), gltf_options);
        if (load)
        {
            gltf = std::move(load.get());
        }
        else
        {
            fmt::print(stderr, "[ {} ]\tFailed to load glTF: {}\n", ERROR_FMT("ERROR"), fastgltf::to_underlying(load.error()));
            return std::nullopt;
        }
    }
    else
    {
        fmt::print(stderr, "[ {} ]\tFailed to determine glTF container!\n", ERROR_FMT("ERROR"));
        return std::nullopt;
    }

    std::vector<descriptor_allocator_growable_t::pool_size_ratio_t> sizes = {
        { vk::DescriptorType::eCombinedImageSampler, 3 },
        { vk::DescriptorType::eUniformBuffer, 3 },
        { vk::DescriptorType::eStorageBuffer, 1 }
    };

    file.descriptor_pool.init(engine->device.dev, gltf.materials.size(), sizes);

    for (fastgltf::Sampler& sampler : gltf.samplers)
    {
        vk::SamplerCreateInfo sampler_info({},
                extract_filter(sampler.magFilter.value_or(fastgltf::Filter::Nearest)),
                extract_filter(sampler.minFilter.value_or(fastgltf::Filter::Nearest)),
                extract_mipmap_mode(sampler.minFilter.value_or(fastgltf::Filter::Nearest)),
                {}, {}, {}, {}, {}, {}, {}, {}, 0, VK_LOD_CLAMP_NONE);
        auto new_sampler = engine->device.dev.createSampler(sampler_info);
        file.samplers.push_back(new_sampler);
    }

    std::vector<std::shared_ptr<mesh_asset_t>> meshes;
    std::vector<std::shared_ptr<node_t>> nodes;
    std::vector<allocated_image_t> images;
    std::vector<std::shared_ptr<gltf_material_t>> materials;

    for (fastgltf::Image& image : gltf.images)
    {
        std::optional<allocated_image_t> img = load_image(engine, gltf, image);
        if (img.has_value())
        {
            images.push_back(img.value());
            file.images[image.name.c_str()] = img.value();
        }
        else
        {
            images.push_back(engine->error_checkerboard_image);
            fmt::print("[ {} ]\tFailed to load glTF texture: {}\n", WARN_FMT("WARNING"), image.name);
        }
    }

    auto ret = engine->create_buffer(sizeof(gltf_metallic_roughness_t::material_constants_t) * gltf.materials.size(),
            vk::BufferUsageFlagBits::eUniformBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU);
    if (!ret.has_value()) return std::nullopt;
    file.material_data_buffer = ret.value();

    std::uint32_t data_index = 0;
    gltf_metallic_roughness_t::material_constants_t* scene_material_constants =
        (gltf_metallic_roughness_t::material_constants_t*)file.material_data_buffer.info.pMappedData;

    for (fastgltf::Material& mat : gltf.materials)
    {
        std::shared_ptr<gltf_material_t> new_mat = std::make_shared<gltf_material_t>();
        materials.push_back(new_mat);

        gltf_metallic_roughness_t::material_constants_t constants{
            .color_factors = glm::vec4(mat.pbrData.baseColorFactor[0], mat.pbrData.baseColorFactor[1],
                    mat.pbrData.baseColorFactor[2], mat.pbrData.baseColorFactor[3]),
            .metal_rought_factors = glm::vec4(mat.pbrData.metallicFactor, mat.pbrData.roughnessFactor, glm::vec2())
        };
        scene_material_constants[data_index] = constants;

        material_pass_e pass_type = (mat.alphaMode == fastgltf::AlphaMode::Blend) ? pass_type = material_pass_e::TRANSPARENT : material_pass_e::MAIN_COLOR;
        
        gltf_metallic_roughness_t::material_resources_t material_resources{ .color_image = engine->white_image,
            .color_sampler = engine->default_sampler_linear,
            .metal_rough_image = engine->white_image,
            .metal_rough_sampler = engine->default_sampler_linear,
            .data_buffer = file.material_data_buffer.buffer,
            .data_buffer_offset = static_cast<uint32_t>(data_index * sizeof(gltf_metallic_roughness_t::material_constants_t))
        };

        if (mat.pbrData.baseColorTexture.has_value())
        {
            std::size_t img = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
            std::size_t sampler = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value();

            material_resources.color_image = images[img];
            material_resources.color_sampler = file.samplers[sampler];
        }

        auto ret = engine->metal_rough_material.write_material(engine->device.dev, pass_type, material_resources, file.descriptor_pool);
        if (!ret.has_value()) return std::nullopt;
        new_mat->data = ret.value();
        data_index++;
    }

    std::vector<std::uint32_t> indices;
    std::vector<vertex_t> vertices;

    for (fastgltf::Mesh& mesh : gltf.meshes)
    {
        std::shared_ptr<mesh_asset_t> new_mesh = std::make_shared<mesh_asset_t>();
        meshes.push_back(new_mesh);
        file.meshes[mesh.name.c_str()] = new_mesh;
        new_mesh->name = mesh.name;

        indices.clear();
        vertices.clear();

        for (auto&& p : mesh.primitives)
        {
            surface_t new_surface{ .start_index = static_cast<uint32_t>(indices.size()),
                .count = static_cast<uint32_t>(gltf.accessors[p.indicesAccessor.value()].count)
            };
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

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, pos_accessor,
                        [&](glm::vec3 v, std::size_t idx) {
                        vertex_t new_vtx{ .position = v, .normal = {1, 0, 0}, .uv = glm::vec2(0), .color = glm::vec4(1.f) };
                        vertices[initial_vtx + idx] = new_vtx;
                        });
            }

            auto normals =p.findAttribute("NORMAL");
            if (normals != p.attributes.end())
            {
                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[normals->second],
                        [&](glm::vec3 v, std::size_t idx) {
                        vertices[initial_vtx + idx].normal = v;
                        });
            }

            auto uv = p.findAttribute("TEXCOORD_0");
            if (normals != p.attributes.end())
            {
                fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[uv->second],
                        [&](glm::vec2 v, std::size_t idx) {
                        vertices[initial_vtx + idx].uv = v;
                        });
            }

            auto colors = p.findAttribute("COLOR_0");
            if (colors != p.attributes.end())
            {
                fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[colors->second],
                        [&](glm::vec4 v, std::size_t idx) {
                        vertices[initial_vtx + idx].color = v;
                        });
            }

            if (p.materialIndex.has_value())
                new_surface.material = materials[p.materialIndex.value()];
            else
                new_surface.material = materials[0];

            glm::vec3 min_pos = vertices[initial_vtx].position;
            glm::vec3 max_pos = vertices[initial_vtx].position;
            for (std::size_t i = initial_vtx; i < vertices.size(); ++i)
            {
                min_pos = glm::min(min_pos, vertices[i].position);
                max_pos = glm::max(max_pos, vertices[i].position);
            }

            new_surface.bounds.origin = (max_pos + min_pos) / 2.f;
            new_surface.bounds.extents = (max_pos - min_pos) / 2.f;
            new_surface.bounds.sphere_radius = glm::length(new_surface.bounds.extents);

            new_mesh->surfaces.push_back(new_surface);
        }

        auto ret = engine->upload_mesh(indices, vertices);
        if (!ret.has_value()) return std::nullopt;
        new_mesh->mesh_buffer = ret.value();
    }

    for (fastgltf::Node& node : gltf.nodes)
    {
        std::shared_ptr<node_t> new_node;

        if (node.meshIndex.has_value())
        {
            new_node = std::make_shared<mesh_node_t>();
            static_cast<mesh_node_t*>(new_node.get())->mesh = meshes[*node.meshIndex];
        }
        else
        {
            new_node = std::make_shared<node_t>();
        }

        nodes.push_back(new_node);
        file.nodes[node.name.c_str()];

        std::visit(fastgltf::visitor { [&](fastgltf::Node::TransformMatrix matrix) {
                std::memcpy(&new_node->local_transform, matrix.data(), sizeof(matrix));
                }, [&](fastgltf::TRS transform) {
                glm::vec3 tl(transform.translation[0], transform.translation[1], transform.translation[2]);
                glm::quat rot(transform.rotation[3], transform.rotation[0], transform.rotation[1], transform.rotation[2]);
                glm::vec3 sc(transform.scale[0], transform.scale[1], transform.scale[2]);

                glm::mat4 tm = glm::translate(glm::mat4(1.f), tl);
                glm::mat4 rm = glm::toMat4(rot);
                glm::mat4 sm = glm::scale(glm::mat4(1.f), sc);

                new_node->local_transform = tm * rm * sm;
                } }, node.transform);
    }

    for (std::uint32_t i = 0; i < gltf.nodes.size(); ++i)
    {
        fastgltf::Node& node = gltf.nodes[i];
        std::shared_ptr<node_t>& scene_node = nodes[i];
        
        for (auto c : node.children)
        {
            scene_node->children.push_back(nodes[c]);
            nodes[c]->parent = scene_node;
        }
    }

    for (auto& node : nodes)
    {
        if (node->parent.lock() == nullptr)
        {
            file.top_nodes.push_back(node);
            node->refresh_transform(glm::mat4(1.f));
        }
    }

#ifdef DEBUG
    fmt::print("[ {} ]\tFinished loading glTF: {}\n", INFO_FMT("INFO"), filepath);
#endif

    return scene;
}

void loaded_gltf_t::draw(const glm::mat4& top_matrix, draw_context_t& ctx)
{
    for (auto& n : top_nodes)
    {
        n->draw(top_matrix, ctx);
    }
}

void loaded_gltf_t::clear_all()
{
    vk::Device dev = this->creator->device.dev;
    this->descriptor_pool.destroy_pools(dev);
    this->creator->destroy_buffer(this->material_data_buffer);

    for (auto& [k, v] : this->meshes)
    {
        this->creator->destroy_buffer(v->mesh_buffer.index_buffer);
        this->creator->destroy_buffer(v->mesh_buffer.vertex_buffer);
    }

    for (auto& [k, v] : this->images)
    {
        if (v.image == this->creator->error_checkerboard_image.image) continue;
        this->creator->destroy_image(v);
    }

    for (auto& sampler : this->samplers)
    {
        dev.destroySampler(sampler);
    }
}
