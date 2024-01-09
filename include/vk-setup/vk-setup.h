#pragma once
#include <vulkan/vulkan.hpp>

struct instance_builder_t
{
    vk::InstanceCreateInfo instance_info;
    vk::ApplicationInfo app_info;

    instance_builder_t& enable_validation_layers();
    vk::ResultValue<vk::Instance> build();
};
