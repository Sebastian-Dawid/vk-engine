#pragma once

#include <cstdint>
#include <deque>
#include <optional>
#include <span>
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

struct descriptor_allocator_growable_t
{
    struct pool_size_ratio_t
    {
        vk::DescriptorType type;
        float ratio;
    };

    std::vector<pool_size_ratio_t> ratios;
    std::vector<vk::DescriptorPool> full_pools;
    std::vector<vk::DescriptorPool> ready_pools;
    std::uint32_t sets_per_pool;

    bool init(vk::Device device, std::uint32_t initial_sets, std::span<pool_size_ratio_t> pool_ratios);
    void clear_pools(vk::Device device);
    void destroy_pools(vk::Device device);

    std::optional<vk::DescriptorSet> allocate(vk::Device device, vk::DescriptorSetLayout layout);

    std::optional<vk::DescriptorPool> get_pool(vk::Device device);
    std::optional<vk::DescriptorPool> create_pool(vk::Device device, std::uint32_t set_count, std::span<pool_size_ratio_t> pool_ratios);
};

struct descriptor_writer_t
{
    std::deque<vk::DescriptorImageInfo> image_infos;
    std::deque<vk::DescriptorBufferInfo> buffer_infos;
    std::vector<vk::WriteDescriptorSet> writes;

    void write_image(std::int32_t binding, vk::ImageView image, vk::Sampler sampler, vk::ImageLayout layout, vk::DescriptorType type);
    void write_buffer(std::int32_t binding, vk::Buffer buffer, std::size_t size, std::size_t offset, vk::DescriptorType type);

    void clear();
    void update_set(vk::Device device, vk::DescriptorSet set);
};
