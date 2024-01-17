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

bool descriptor_allocator_growable_t::init(vk::Device device, std::uint32_t max_sets, std::span<pool_size_ratio_t> pool_ratios)
{
    this->ratios.clear();
    for (auto r : pool_ratios)
        this->ratios.push_back(r);

    auto ret = this->create_pool(device, max_sets, pool_ratios);
    if (!ret.has_value()) return false;
    this->sets_per_pool = max_sets * 1.5f;
    this->ready_pools.push_back(ret.value());

    return true;
}

void descriptor_allocator_growable_t::clear_pools(vk::Device device)
{
    for (auto p : this->ready_pools)
    {
        device.resetDescriptorPool(p);
    }
    for (auto p : this->full_pools)
    {
        device.resetDescriptorPool(p);
        this->ready_pools.push_back(p);
    }
    this->full_pools.clear();
}

void descriptor_allocator_growable_t::destroy_pools(vk::Device device)
{
    for (auto p : this->ready_pools)
    {
        device.destroyDescriptorPool(p);
    }
    this->ready_pools.clear();
    for (auto p : this->full_pools)
    {
        device.destroyDescriptorPool(p);
    }
    this->full_pools.clear();
}

std::optional<vk::DescriptorSet> descriptor_allocator_growable_t::allocate(vk::Device device, vk::DescriptorSetLayout layout)
{
    auto ret = this->get_pool(device);
    if (!ret.has_value()) return std::nullopt;
    vk::DescriptorPool pool = ret.value();
    vk::DescriptorSetAllocateInfo alloc_info(pool, 1, &layout);

    auto [result, set] = device.allocateDescriptorSets(alloc_info);
    if (result == vk::Result::eErrorOutOfPoolMemory || result == vk::Result::eErrorFragmentedPool)
    {
        this->full_pools.push_back(pool);
        auto ret = this->get_pool(device);
        if (!ret.has_value()) return std::nullopt;
        pool = ret.value();
        alloc_info.descriptorPool = pool;

        std::tie(result, set) = device.allocateDescriptorSets(alloc_info);
        if (result != vk::Result::eSuccess)
        {
            fmt::print(stderr, "[ {} ]\tFailed to allocate descriptor set!\n", ERROR_FMT("ERROR"));
            return std::nullopt;
        }
    }

    this->ready_pools.push_back(pool);
    return set[0];
}

std::optional<vk::DescriptorPool> descriptor_allocator_growable_t::get_pool(vk::Device device)
{
    vk::DescriptorPool new_pool;
    if (this->ready_pools.size() != 0)
    {
        new_pool = this->ready_pools.back();
        this->ready_pools.pop_back();
    }
    else
    {
        auto ret = this->create_pool(device, this->sets_per_pool, this->ratios);
        if (!ret.has_value()) return std::nullopt;
        new_pool = ret.value();
        this->sets_per_pool = this->sets_per_pool * 1.5f;
        if (this->sets_per_pool > 4092)
            this->sets_per_pool = 4092;
    }
    return new_pool;
}

std::optional<vk::DescriptorPool> descriptor_allocator_growable_t::create_pool(vk::Device device, std::uint32_t set_count, std::span<pool_size_ratio_t> pool_ratios)
{
    std::vector<vk::DescriptorPoolSize> pool_sizes;
    for (pool_size_ratio_t ratio : pool_ratios)
    {
        pool_sizes.push_back(vk::DescriptorPoolSize(ratio.type, std::uint32_t(ratio.ratio * set_count)));
    }
    vk::DescriptorPoolCreateInfo pool_info({}, set_count, pool_sizes);
    auto [result, pool] = device.createDescriptorPool(pool_info);
    if (result != vk::Result::eSuccess)
    {
        fmt::print(stderr, "[ {} ]\tFailed to create descriptor pool!\n", ERROR_FMT("ERROR"));
        return std::nullopt;
    }
    return pool;
}
