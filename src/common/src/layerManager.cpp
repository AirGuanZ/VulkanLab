#include <agz/vlab/layerManager.h>

AGZ_VULKAN_LAB_BEGIN

void ValidationLayerManager::add(const char *name)
{
    const auto it = std::find(
        layers_.begin(), layers_.end(), std::string(name));

    if(it == layers_.end())
        layers_.push_back(name);
}

void ValidationLayerManager::add(const char *const *names, size_t nameCount)
{
    for(size_t i = 0; i < nameCount; ++i)
        add(names[i]);
}

bool ValidationLayerManager::isAllSupported() const
{
    // get all supported layers

    uint32_t allLayerCount;
    vkEnumerateInstanceLayerProperties(&allLayerCount, nullptr);

    std::vector<VkLayerProperties> allLayers(allLayerCount);
    vkEnumerateInstanceLayerProperties(&allLayerCount, allLayers.data());

    // search each layer name in allLayers

    for(auto &l : layers_)
    {
        bool found = false;
        for(auto &vkl : allLayers)
        {
            if(vkl.layerName == l)
            {
                found = true;
                break;
            }
        }

        if(!found)
            return false;
    }

    return true;
}

const std::vector<const char *> &ValidationLayerManager::getLayers() const
{
    cLayers_.clear();
    for(auto &s : layers_)
        cLayers_.push_back(s.c_str());
    return cLayers_;
}

AGZ_VULKAN_LAB_END
