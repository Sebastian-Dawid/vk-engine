#include <cstdint>
#include <vk-pipelines.h>
#include <fstream>
#include <error_fmt.h>

std::optional<vk::ShaderModule> vkutil::load_shader_module(const char *file_path, vk::Device device)
{
    std::ifstream file(file_path, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        fmt::print(stderr, "[ {} ]\tFailed to open file '{}'!\n", ERROR_FMT("ERROR"), fmt::styled(file_path, fmt::emphasis::bold | fmt::emphasis::underline));
        return std::nullopt;
    }

    std::size_t file_size = file.tellg();
    std::vector<std::uint32_t> buffer(file_size / sizeof(std::uint32_t));
    file.seekg(0);
    file.read((char*)buffer.data(), file_size);
    file.close();
    vk::ShaderModuleCreateInfo create_info({}, buffer);

    auto [result, module] = device.createShaderModule(create_info);
    if (result != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFailed to create shader module!\n", ERROR_FMT("ERROR"));
        return std::nullopt;
    }
    return module;
}

pipeline_builder_t::pipeline_builder_t()
{
    this->clear();
}

void pipeline_builder_t::clear()
{
    this->input_assembly         = vk::PipelineInputAssemblyStateCreateInfo();
    this->rasterizer             = vk::PipelineRasterizationStateCreateInfo();
    this->color_blend_attachment = vk::PipelineColorBlendAttachmentState();
    this->multisampling          = vk::PipelineMultisampleStateCreateInfo();
    this->pipeline_layout        = vk::PipelineLayout();
    this->depth_stencil          = vk::PipelineDepthStencilStateCreateInfo();
    this->render_info            = vk::PipelineRenderingCreateInfo();
    this->shader_stages.clear();
}

pipeline_builder_t& pipeline_builder_t::set_shaders(vk::ShaderModule vertex_shader, vk::ShaderModule fragment_shader)
{
    this->shader_stages.clear();
    this->shader_stages.push_back(vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eVertex, vertex_shader, "main"));
    this->shader_stages.push_back(vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eFragment, fragment_shader, "main"));
    return *this;
}

pipeline_builder_t& pipeline_builder_t::set_input_topology(vk::PrimitiveTopology topology)
{
    this->input_assembly.topology = topology;
    this->input_assembly.primitiveRestartEnable = vk::False;
    return *this;
}

pipeline_builder_t& pipeline_builder_t::set_polygon_mode(vk::PolygonMode mode)
{
    this->rasterizer.polygonMode = mode;
    this->rasterizer.lineWidth = 1.f;
    return *this;
}

pipeline_builder_t& pipeline_builder_t::set_cull_mode(vk::CullModeFlags cull_mode, vk::FrontFace front_face)
{
    this->rasterizer.cullMode = cull_mode;
    this->rasterizer.frontFace = front_face;
    return *this;
}

pipeline_builder_t& pipeline_builder_t::set_multisampling_none()
{
    this->multisampling.sampleShadingEnable = vk::False;
    this->multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
    this->multisampling.minSampleShading = 1.f;
    this->multisampling.pSampleMask = nullptr;
    this->multisampling.alphaToCoverageEnable = vk::False;
    this->multisampling.alphaToOneEnable = vk::False;
    return *this;
}

pipeline_builder_t& pipeline_builder_t::disable_blending()
{
    this->color_blend_attachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
        | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    this->color_blend_attachment.blendEnable = vk::False;
    return *this;
}

pipeline_builder_t& pipeline_builder_t::set_color_attachment_format(vk::Format format)
{
    this->color_attachment_format = format;
    this->render_info.setColorAttachmentFormats(this->color_attachment_format);
    return *this;
}

pipeline_builder_t& pipeline_builder_t::set_depth_format(vk::Format format)
{
    this->render_info.depthAttachmentFormat = format;
    return *this;
}

pipeline_builder_t& pipeline_builder_t::disable_depthtest()
{
    this->depth_stencil = vk::PipelineDepthStencilStateCreateInfo();
    this->depth_stencil.maxDepthBounds = 1.f;
    return *this;
}

vk::Pipeline pipeline_builder_t::build(vk::Device dev)
{
    vk::PipelineViewportStateCreateInfo viewport_state({}, 1, {}, 1);
    vk::PipelineColorBlendStateCreateInfo color_blending({}, vk::False, vk::LogicOp::eCopy, this->color_blend_attachment);
    vk::PipelineVertexInputStateCreateInfo vertex_input_info;
    vk::DynamicState state[] = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
    vk::PipelineDynamicStateCreateInfo dynamic_info({}, state);
    vk::GraphicsPipelineCreateInfo pipeline_info({}, this->shader_stages, &vertex_input_info, &this->input_assembly, {}, &viewport_state, &this->rasterizer,
            &this->multisampling, &this->depth_stencil, &color_blending, &dynamic_info, this->pipeline_layout, {}, {}, {}, {}, &this->render_info);
    auto [result, pipeline] = dev.createGraphicsPipeline({}, pipeline_info);
    if (result != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFailed to create pipeline!\n", ERROR_FMT("ERROR"));
        return VK_NULL_HANDLE;
    }
    return pipeline;
}
