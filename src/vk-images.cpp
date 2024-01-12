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
                0, VK_REMAINING_MIP_LEVELS,
                0, VK_REMAINING_ARRAY_LAYERS
                )
            );
    vk::DependencyInfo dep_info({}, {}, {}, {}, {}, 1, &img_barrier);
    cmd.pipelineBarrier2(dep_info);
}

void vkutil::copy_image_to_image(vk::CommandBuffer cmd, vk::Image src, vk::Image dst, vk::Extent2D src_size, vk::Extent2D dst_size)
{
    vk::ImageBlit2 blit_region(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
            std::array<vk::Offset3D, 2>{vk::Offset3D(0, 0, 0), vk::Offset3D(src_size.width, src_size.height, 1)},
            vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
            std::array<vk::Offset3D, 2>{vk::Offset3D(0, 0, 0), vk::Offset3D(dst_size.width, dst_size.height, 1)});
    vk::BlitImageInfo2 blit_info(src, vk::ImageLayout::eTransferSrcOptimal, dst, vk::ImageLayout::eTransferDstOptimal, blit_region, vk::Filter::eLinear);
    cmd.blitImage2(blit_info);
}
