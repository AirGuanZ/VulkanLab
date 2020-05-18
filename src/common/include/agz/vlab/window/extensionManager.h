#pragma once

#include <set>
#include <string>

#include <agz/vlab/common.h>

AGZ_VULKAN_LAB_BEGIN

class InstanceExtensionManager
{
public:

    InstanceExtensionManager();

    InstanceExtensionManager(const InstanceExtensionManager &other);

    void add(const char *name);

    void add(const char *const *names, size_t nameCount);

    bool isSupported(const char *name) const;

    bool isAllSupported() const;

    const std::vector<const char *> &getExtensions() const;

private:

    // all supported extensions
    std::set<std::string> availableExts_;

    // all required extensions
    std::set<std::string> exts_;

    // c-style (required) extension names
    std::vector<const char *> cExts_;
};

class DeviceExtensionManager
{
public:

    DeviceExtensionManager() = default;

    DeviceExtensionManager(const DeviceExtensionManager &other);

    void add(const char *name);

    void add(const char *const *names, size_t nameCount);

    bool isAllSupported(vk::PhysicalDevice physicalDevice);

    const std::vector<const char *> &getExtensions() const;

private:

    std::set<std::string> exts_;
    std::vector<const char *> cExts_;
};

AGZ_VULKAN_LAB_END
