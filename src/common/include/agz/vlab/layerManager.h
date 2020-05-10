#pragma once

#include <vector>

#include <vulkan/vulkan.h>

#include <agz/vlab/common.h>

AGZ_VULKAN_LAB_BEGIN

class ValidationLayerManager
{
public:

    void add(const char *name);

    void add(const char *const *names, size_t nameCount);

    bool isAllSupported() const;

    // strings in returned vector are keeped valid
    // until next calling of 'this->add' or 'this->~ValidationLayerManager()'
    const std::vector<const char *> &getLayers() const;

private:

    std::vector<std::string> layers_;
    mutable std::vector<const char*> cLayers_;
};

AGZ_VULKAN_LAB_END
