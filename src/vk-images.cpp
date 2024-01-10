#include <vk-images.h>

void vkutil::transition_image(vk::CommandBuffer cmd, vk::Image img, vk::ImageLayout current, vk::ImageLayout target)
{
    vk::ImageMemoryBarrier2 img_barrier(
            vk::PipelineStageFlagBits2::eAllCommands,
            vk::AccessFlagBits2::eMemoryWrite,
            vk::PipelineStageFlagBits2::eAllCommands,
            vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead,
            current, target, {}, {}, img, vk::ImageSubresourceRange(
                (target == vk::ImageLayout::eDepthAttachmentOptimal) ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor,
                0, vk::RemainingMipLevels,
                0, vk::RemainingArrayLayers
                )
            );
    vk::DependencyInfo dep_info({}, {}, {}, {}, {}, 1, &img_barrier);
    cmd.pipelineBarrier2(dep_info);
}
