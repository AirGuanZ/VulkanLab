#include <agz/vlab/extensionManager.h>

AGZ_VULKAN_LAB_BEGIN

ExtensionManager::ExtensionManager()
{
    uint32_t count;
    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);

    availableExts_.resize(count);
    vkEnumerateInstanceExtensionProperties(
        nullptr, &count, availableExts_.data());
}

void ExtensionManager::add(const char *name)
{
    for(auto &s : exts_)
    {
        if(s == name)
            return;
    }
    exts_.push_back(name);
}

void ExtensionManager::add(const char *const *names, size_t nameCount)
{
    for(size_t i = 0; i < nameCount; ++i)
        add(names[i]);
}

bool ExtensionManager::isSupported(const char *name) const
{
    for(auto &a : availableExts_)
    {
        if(std::strcmp(a.extensionName, name) == 0)
            return true;
    }
    return false;
}

bool ExtensionManager::isAllSupported() const
{
    for(auto &n : exts_)
    {
        if(!isSupported(n.c_str()))
            return false;
    }
    return true;
}

const std::vector<const char *> &ExtensionManager::getExtensions() const
{
    cExts_.clear();
    for(auto &n : exts_)
        cExts_.push_back(n.c_str());
    return cExts_;
}

AGZ_VULKAN_LAB_END
