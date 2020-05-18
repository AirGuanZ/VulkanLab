#pragma once

#include <agz/vlab/common.h>

AGZ_VULKAN_LAB_BEGIN

struct SwapchainProperty
{
    vk::SurfaceCapabilitiesKHR capabilities = {};
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> presentModes;

    bool IsAvailable() const noexcept;

    vk::SurfaceFormatKHR chooseFormat(
        const std::vector<vk::SurfaceFormatKHR> &preferred) const noexcept;

    vk::PresentModeKHR choosePresentMode(
        const std::vector<vk::PresentModeKHR> &preferred) const noexcept;

    vk::Extent2D chooseExtent(const vk::Extent2D &preferred) const noexcept;
};

SwapchainProperty querySwapchainProperty(
    vk::PhysicalDevice device, vk::SurfaceKHR surface);

AGZ_VULKAN_LAB_END
