#pragma once

#include <cstdint>
#define VULKAN_HPP_NO_EXCEPTIONS
#include <vulkan/vulkan.hpp>

struct descriptor_layout_builder_t
{
    std::vector<vk::DescriptorSetLayoutBinding> bindings;

    descriptor_layout_builder_t& add_binding(std::uint32_t binding, vk::DescriptorType type);
    void clear();
    std::optional<vk::DescriptorSetLayout> build(vk::Device device, vk::ShaderStageFlags shader_stages);
};

struct descriptor_allocator_t
{
    struct pool_size_ratio_t
    {
        vk::DescriptorType type;
        float ratio;
    };

    vk::DescriptorPool pool;

    bool init_pool(vk::Device device, std::uint32_t max_sets, std::span<pool_size_ratio_t> pool_ratios);
    void clear_descriptors(vk::Device device);
    void destroy_pool(vk::Device device);

    std::optional<vk::DescriptorSet> allocate(vk::Device device, vk::DescriptorSetLayout layout);
};
