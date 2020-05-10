#pragma once

#include <vulkan/vulkan.h>

#include <agz/vlab/common.h>

AGZ_VULKAN_LAB_BEGIN

class ExtensionManager
{
public:

    ExtensionManager();

    void add(const char *name);

    void add(const char *const *names, size_t nameCount);

    bool isSupported(const char *name) const;

    bool isAllSupported() const;

    const std::vector<const char *> &getExtensions() const;

private:

    std::vector<VkExtensionProperties> availableExts_;

    std::vector<std::string> exts_;
    mutable std::vector<const char *> cExts_;
};

AGZ_VULKAN_LAB_END
