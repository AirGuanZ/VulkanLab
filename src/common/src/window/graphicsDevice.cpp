#include <optional>

#include <agz/vlab/window/graphicsDevice.h>
#include <agz/vlab/window/swapchain.h>

AGZ_VULKAN_LAB_BEGIN

namespace
{
    struct GraphicsQueueFamilyIndices
    {
        std::optional<uint32_t> graphics;
        std::optional<uint32_t> transfer;
        std::optional<uint32_t> present;

        bool isAvailable() const noexcept
        {
            return graphics.has_value() &&
                   transfer.has_value() &&
                   present.has_value();
        }
    };

    GraphicsQueueFamilyIndices getGraphicsQueueIndices(
        vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface)
    {
        const auto families = physicalDevice.getQueueFamilyProperties();

        GraphicsQueueFamilyIndices ret;
        for(uint32_t i = 0; i < families.size(); ++i)
        {
            // graphics

            if(families[i].queueFlags & vk::QueueFlagBits::eGraphics)
                ret.graphics = i;

            // transfer

            if(families[i].queueFlags & vk::QueueFlagBits::eTransfer)
                ret.transfer = i;

            // presentation

            if(physicalDevice.getSurfaceSupportKHR(i, surface))
                ret.present = i;

            if(ret.isAvailable())
                break;
        }

        return ret;
    }
}

std::vector<vk::PhysicalDevice> getAllPhysicalDevices(
    vk::Instance instance, const std::function<bool(vk::PhysicalDevice)> &filter)
{
    auto devices = instance.enumeratePhysicalDevices();

    if(filter)
    {
        devices.erase(
            std::remove_if(devices.begin(), devices.end(),
                            [&](vk::PhysicalDevice d) { return !filter(d); }),
            devices.end());
    }

    return devices;
}

bool IsGraphicsPhysicalDevice(
    vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface,
    const DeviceExtensionManager *extensions)
{
    // queues

    const auto queueFamilyIndices = getGraphicsQueueIndices(
        physicalDevice, surface);
    if(!queueFamilyIndices.isAvailable())
        return false;

    // gpu?

    vk::PhysicalDeviceProperties prop = physicalDevice.getProperties();

    if(prop.deviceType != vk::PhysicalDeviceType::eDiscreteGpu &&
       prop.deviceType != vk::PhysicalDeviceType::eIntegratedGpu)
        return false;

    // extensions

    DeviceExtensionManager exts;
    if(extensions)
        exts = *extensions;
    exts.add(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    if(!exts.isAllSupported(physicalDevice))
        return false;

    // swapchain

    const auto swapchainProperty = querySwapchainProperty(
        physicalDevice, surface);
    if(!swapchainProperty.IsAvailable())
        return false;

    return true;
}

GraphicsDevice::~GraphicsDevice()
{
    Destroy();
}

void GraphicsDevice::Initialize(
    vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface,
    const DeviceExtensionManager *extensions)
{
    Destroy();

    const auto queueFamilyIndices = getGraphicsQueueIndices(
        physicalDevice, surface);
    assert(queueFamilyIndices.isAvailable());

    const float queuePriority = 1;

    vk::DeviceQueueCreateInfo queueInfo[3] = {
        { {}, queueFamilyIndices.graphics.value(),     1, &queuePriority },
        { {}, queueFamilyIndices.transfer.value(),     1, &queuePriority },
        { {}, queueFamilyIndices.present.value(),      1, &queuePriority }
    };

    DeviceExtensionManager exts;
    if(extensions)
        exts = *extensions;
    exts.add(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    if(!exts.isAllSupported(physicalDevice))
        throw std::runtime_error("device extension(s) not supported");

    vk::PhysicalDeviceFeatures deviceFeatures = {};

    vk::DeviceCreateInfo deviceInfo(
        {}, 3, queueInfo, 0, {},
        static_cast<uint32_t>(exts.getExtensions().size()),
        exts.getExtensions().data(), &deviceFeatures);

    device_ = physicalDevice.createDeviceUnique(deviceInfo);

    graphicsIndex_ = queueFamilyIndices.graphics.value();
    transferIndex_ = queueFamilyIndices.transfer.value();
    presentIndex_  = queueFamilyIndices.present.value();

    graphicsQueue_     = device_->getQueue(graphicsIndex_, 0);
    transferQueue_     = device_->getQueue(transferIndex_, 0);
    presentationQueue_ = device_->getQueue(presentIndex_, 0);
}

void GraphicsDevice::Destroy()
{
    if(device_)
    {
        device_.reset();

        graphicsIndex_     = 0;
        transferIndex_     = 0;
        presentIndex_      = 0;

        graphicsQueue_     = nullptr;
        transferQueue_     = nullptr;
        presentationQueue_ = nullptr;
    }
}

AGZ_VULKAN_LAB_END
