#pragma once

#include <agz/vlab/window/extensionManager.h>

AGZ_VULKAN_LAB_BEGIN

bool IsGraphicsPhysicalDevice(
    vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface,
    const DeviceExtensionManager *extensions);

std::vector<vk::PhysicalDevice> getAllPhysicalDevices(
    vk::Instance instance,
    const std::function<bool(vk::PhysicalDevice)> &filter = {});

class GraphicsDevice : public misc::uncopyable_t
{
public:

    ~GraphicsDevice();

    void Initialize(
        vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface,
        const DeviceExtensionManager *extensions);

    void Destroy();

    vk::Device device() const noexcept;

    uint32_t graphicsQueueFamilyIndex() const noexcept;

    uint32_t transferQueueFamilyIndex() const noexcept;

    uint32_t presentQueueFamilyIndex() const noexcept;

    vk::Queue graphicsQueue() const noexcept;

    vk::Queue transferQueue() const noexcept;

    vk::Queue presentQueue() const noexcept;

private:

    vk::UniqueDevice device_;

    uint32_t graphicsIndex_ = 0;
    uint32_t transferIndex_ = 0;
    uint32_t presentIndex_  = 0;

    vk::Queue graphicsQueue_;
    vk::Queue transferQueue_;
    vk::Queue presentationQueue_;
};

inline vk::Device GraphicsDevice::device() const noexcept
{
    return device_.get();
}

inline uint32_t GraphicsDevice::graphicsQueueFamilyIndex() const noexcept
{
    return graphicsIndex_;
}

inline ::uint32_t GraphicsDevice::transferQueueFamilyIndex() const noexcept
{
    return transferIndex_;
}

inline uint32_t GraphicsDevice::presentQueueFamilyIndex() const noexcept
{
    return presentIndex_;
}

inline vk::Queue GraphicsDevice::graphicsQueue() const noexcept
{
    return graphicsQueue_;
}

inline vk::Queue GraphicsDevice::transferQueue() const noexcept
{
    return transferQueue_;
}

inline vk::Queue GraphicsDevice::presentQueue() const noexcept
{
    return presentationQueue_;
}

AGZ_VULKAN_LAB_END
