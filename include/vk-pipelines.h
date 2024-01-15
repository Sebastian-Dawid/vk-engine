#pragma once

#define VULKAN_HPP_NO_EXCEPTIONS
#include <vulkan/vulkan.hpp>

namespace vkutil {
    std::optional<vk::ShaderModule> load_shader_module(const char* file_path, vk::Device device);
};

struct pipeline_builder_t
{
    std::vector<vk::PipelineShaderStageCreateInfo>  shader_stages;

    vk::PipelineInputAssemblyStateCreateInfo        input_assembly;
    vk::PipelineRasterizationStateCreateInfo        rasterizer;
    vk::PipelineColorBlendAttachmentState           color_blend_attachment;
    vk::PipelineMultisampleStateCreateInfo          multisampling;
    vk::PipelineLayout                              pipeline_layout;
    vk::PipelineDepthStencilStateCreateInfo         depth_stencil;
    vk::PipelineRenderingCreateInfo                 render_info;
    vk::Format                                      color_attachment_format;

    pipeline_builder_t();

    void clear();
    pipeline_builder_t& set_shaders(vk::ShaderModule vertex_shader, vk::ShaderModule fragment_shader);
    pipeline_builder_t& set_input_topology(vk::PrimitiveTopology topology);
    pipeline_builder_t& set_polygon_mode(vk::PolygonMode mode);
    pipeline_builder_t& set_cull_mode(vk::CullModeFlags cull_mode, vk::FrontFace front_face);
    pipeline_builder_t& set_multisampling_none();
    pipeline_builder_t& disable_blending();
    pipeline_builder_t& set_color_attachment_format(vk::Format format);
    pipeline_builder_t& set_depth_format(vk::Format format);
    pipeline_builder_t& disable_depthtest();
    vk::Pipeline build(vk::Device dev);
};
