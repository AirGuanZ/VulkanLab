#pragma once

#include <map>

#include <agz/vlab/common.h>

AGZ_VULKAN_LAB_BEGIN

enum class ShaderModuleType
{
    Vertex,
    Fragment
};

std::vector<uint32_t> compileGLSLToSPIRV(
    const std::string                        &source,
    const std::string                        &sourceName,
    const std::map<std::string, std::string> &macros,
    ShaderModuleType                          moduleType,
    bool                                      optimize);

AGZ_VULKAN_LAB_END
