#pragma once

#define VULKAN_HPP_NO_EXCEPTIONS
#include <vulkan/vulkan.hpp>
#include <optional>
#include <cstdint>

namespace vkutil {
    std::optional<vk::ShaderModule> load_shader_module(const char* file_path, vk::Device device);
};

struct pipeline_builder_t
{
    std::vector<vk::PipelineShaderStageCreateInfo>   shader_stages;

    vk::PipelineInputAssemblyStateCreateInfo         input_assembly;
    vk::PipelineRasterizationStateCreateInfo         rasterizer;
    vk::PipelineColorBlendAttachmentState            color_blend_attachment;
    vk::PipelineMultisampleStateCreateInfo           multisampling;
    vk::PipelineLayout                               pipeline_layout;
    vk::PipelineDepthStencilStateCreateInfo          depth_stencil;
    vk::PipelineRenderingCreateInfo                  render_info;
    vk::Format                                       color_attachment_format;
    std::vector<vk::VertexInputAttributeDescription> vertex_input_attribute_descriptions;
    std::vector<vk::VertexInputBindingDescription>   vertex_input_binding_descriptions;

    pipeline_builder_t();

    void clear();
    pipeline_builder_t& set_shaders(const vk::ShaderModule vertex_shader, const vk::ShaderModule fragment_shader);
    pipeline_builder_t& set_input_topology(const vk::PrimitiveTopology topology);
    pipeline_builder_t& set_polygon_mode(const vk::PolygonMode mode);
    pipeline_builder_t& set_cull_mode(const vk::CullModeFlags cull_mode, const vk::FrontFace front_face);
    pipeline_builder_t& set_multisampling_none();
    pipeline_builder_t& disable_blending();
    pipeline_builder_t& enable_blending_additive();
    pipeline_builder_t& enable_blending_alphablend();
    pipeline_builder_t& set_color_attachment_format(const vk::Format format);
    pipeline_builder_t& set_depth_format(const vk::Format format);
    pipeline_builder_t& disable_depthtest();
    pipeline_builder_t& enable_depthtest(const bool depth_write_enable, const vk::CompareOp op);
    // TODO: Maybe overload these functions to accept vectors of bindings/attributes
    pipeline_builder_t& add_vertex_input_binding(std::uint32_t binding, std::size_t stride, vk::VertexInputRate input_rate);
    pipeline_builder_t& add_vertex_input_attribute(std::uint32_t binding, std::uint32_t location, vk::Format format, std::size_t offset);
    std::optional<vk::Pipeline> build(vk::Device dev);
};
