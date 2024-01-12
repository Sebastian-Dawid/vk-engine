#include <vk-descriptors.h>
#include <error_fmt.h>

descriptor_layout_builder_t& descriptor_layout_builder_t::add_binding(std::uint32_t binding, vk::DescriptorType type)
{
    vk::DescriptorSetLayoutBinding new_bind(binding, type, 1);
    this->bindings.push_back(new_bind);
    return *this;
}

void descriptor_layout_builder_t::clear()
{
    this->bindings.clear();
}

std::optional<vk::DescriptorSetLayout> descriptor_layout_builder_t::build(vk::Device device, vk::ShaderStageFlags shader_stages)
{
    for (auto& bind : this->bindings)
    {
        bind.stageFlags |= shader_stages;
    }

    vk::DescriptorSetLayoutCreateInfo info({}, this->bindings);
    auto [result, set] = device.createDescriptorSetLayout(info);
    if (result != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFailed to create descriptor set layout!\n", ERROR_FMT("ERROR"));
        return std::nullopt;
    }
    return set;
}

bool descriptor_allocator_t::init_pool(vk::Device device, std::uint32_t max_sets, std::span<pool_size_ratio_t> pool_ratios)
{
    std::vector<vk::DescriptorPoolSize> pool_sizes;
    for (pool_size_ratio_t ratio : pool_ratios)
        pool_sizes.push_back(vk::DescriptorPoolSize(ratio.type, std::uint32_t(ratio.ratio * max_sets)));

    vk::DescriptorPoolCreateInfo pool_info({}, max_sets, pool_sizes);
    vk::Result result;
    std::tie(result, this->pool) = device.createDescriptorPool(pool_info);
    if (result != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFailed to create descriptor pool!\n", ERROR_FMT("ERROR"));
        return false;
    }
    return true;
}

void descriptor_allocator_t::clear_descriptors(vk::Device device)
{
    device.resetDescriptorPool(this->pool);
}

void descriptor_allocator_t::destroy_pool(vk::Device device)
{
    device.destroyDescriptorPool(this->pool);
}

std::optional<vk::DescriptorSet> descriptor_allocator_t::allocate(vk::Device device, vk::DescriptorSetLayout layout)
{
    vk::DescriptorSetAllocateInfo alloc_info(this->pool, layout);
    auto [result, sets] = device.allocateDescriptorSets(alloc_info);
    if (result != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFailed to allocate descriptor sets!\n", ERROR_FMT("ERROR"));
        return std::nullopt;
    }
    return sets[0];
}
