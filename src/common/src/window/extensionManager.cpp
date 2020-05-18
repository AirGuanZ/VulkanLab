#include <agz/vlab/window/extensionManager.h>

AGZ_VULKAN_LAB_BEGIN

InstanceExtensionManager::InstanceExtensionManager()
{
    uint32_t count;
    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);

    std::vector<VkExtensionProperties> availableExts(count);
    vkEnumerateInstanceExtensionProperties(
        nullptr, &count, availableExts.data());

    for(auto &p : availableExts)
        availableExts_.insert(p.extensionName);
}

InstanceExtensionManager::InstanceExtensionManager(
    const InstanceExtensionManager &other)
{
    availableExts_ = other.availableExts_;
    exts_ = other.exts_;
    for(auto &e : exts_)
        cExts_.push_back(e.c_str());
}

void InstanceExtensionManager::add(const char *name)
{
    auto insertRet = exts_.insert(name);
    if(insertRet.second)
        cExts_.push_back(insertRet.first->c_str());
}

void InstanceExtensionManager::add(const char *const *names, size_t nameCount)
{
    for(size_t i = 0; i < nameCount; ++i)
        add(names[i]);
}

bool InstanceExtensionManager::isSupported(const char *name) const
{
    return availableExts_.find(name) != availableExts_.end();
}

bool InstanceExtensionManager::isAllSupported() const
{
    for(auto &n : exts_)
    {
        if(!isSupported(n.c_str()))
            return false;
    }
    return true;
}

const std::vector<const char *> &InstanceExtensionManager::getExtensions() const
{
    return cExts_;
}

DeviceExtensionManager::DeviceExtensionManager(
    const DeviceExtensionManager &other)
{
    exts_ = other.exts_;
    for(auto &e : exts_)
        cExts_.push_back(e.c_str());
}

void DeviceExtensionManager::add(const char *name)
{
    auto insertRet = exts_.insert(name);
    if(insertRet.second)
        cExts_.push_back(insertRet.first->c_str());
}

void DeviceExtensionManager::add(const char *const *names, size_t nameCount)
{
    for(size_t i = 0; i < nameCount; ++i)
        add(names[i]);
}

bool DeviceExtensionManager::isAllSupported(vk::PhysicalDevice physicalDevice)
{
    const auto exts = physicalDevice.enumerateDeviceExtensionProperties();

    auto requiredExts = exts_;
    for(auto &e : exts)
        requiredExts.erase(e.extensionName);

    return requiredExts.empty();
}

const std::vector<const char *> &DeviceExtensionManager::getExtensions() const
{
    return cExts_;
}

AGZ_VULKAN_LAB_END
