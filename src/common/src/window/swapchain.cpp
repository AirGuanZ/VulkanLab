#include <agz/vlab/window/swapchain.h>

AGZ_VULKAN_LAB_BEGIN

bool SwapchainProperty::IsAvailable() const noexcept
{
    return !formats.empty() && !presentModes.empty();
}

vk::SurfaceFormatKHR SwapchainProperty::chooseFormat(
    const std::vector<vk::SurfaceFormatKHR> &preferred) const noexcept
{
    assert(!formats.empty());

    for(auto &f : preferred)
    {
        for(auto &rf : formats)
        {
            if(f.colorSpace == rf.colorSpace && f.format == rf.format)
                return f;
        }
    }

    return formats[0];
}

vk::PresentModeKHR SwapchainProperty::choosePresentMode(
    const std::vector<vk::PresentModeKHR> &preferred) const noexcept
{
    assert(!presentModes.empty());

    for(auto &m : preferred)
    {
        for(auto &rm : presentModes)
        {
            if(m == rm)
                return m;
        }
    }

    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D SwapchainProperty::chooseExtent(
    const vk::Extent2D &preferred) const noexcept
{
    if(capabilities.currentExtent.width != UINT32_MAX)
        return capabilities.currentExtent;

    const uint32_t w =
        math::clamp(
            preferred.width,
            capabilities.minImageExtent.width,
            capabilities.maxImageExtent.width);

    const uint32_t h =
        math::clamp(
            preferred.height,
            capabilities.minImageExtent.height,
            capabilities.maxImageExtent.height);

    return VkExtent2D{ w, h };
}

SwapchainProperty querySwapchainProperty(
    vk::PhysicalDevice device, vk::SurfaceKHR surface)
{
    SwapchainProperty ret;
    ret.capabilities = device.getSurfaceCapabilitiesKHR(surface);
    ret.formats      = device.getSurfaceFormatsKHR(surface);
    ret.presentModes = device.getSurfacePresentModesKHR(surface);
    return ret;
}

AGZ_VULKAN_LAB_END
