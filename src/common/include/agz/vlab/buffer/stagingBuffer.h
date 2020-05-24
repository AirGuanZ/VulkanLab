#pragma once

#include <agz/vlab/vma/vmaAlloc.h>

AGZ_VULKAN_LAB_BEGIN

inline VMAUniqueBuffer createStagingBuffer(
    VMAAlloc &allocator, size_t byteSize, const void *initData)
{
    vk::BufferCreateInfo bufInfo;
    bufInfo
        .setSize(byteSize)
        .setUsage(vk::BufferUsageFlagBits::eTransferSrc)
        .setSharingMode(vk::SharingMode::eExclusive);

    VmaAllocationCreateInfo allocInfo;
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    return allocator.createBufferUnique(bufInfo, allocInfo);
}

AGZ_VULKAN_LAB_END
