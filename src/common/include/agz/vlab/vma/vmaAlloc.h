#pragma once

#include <agz/vlab/common.h>

#include <vma/vk_mem_alloc.h>

AGZ_VULKAN_LAB_BEGIN

class VMAUniqueBuffer : public misc::uncopyable_t
{
public:

    VMAUniqueBuffer();

    VMAUniqueBuffer(
        vk::Buffer buffer, VmaAllocation alloc, VmaAllocator allocator);

    VMAUniqueBuffer(VMAUniqueBuffer &&other) noexcept;

    VMAUniqueBuffer &operator=(VMAUniqueBuffer &&other) noexcept;

    ~VMAUniqueBuffer();

    void reset(
        vk::Buffer buffer      = nullptr,
        VmaAllocation alloc    = nullptr,
        VmaAllocator allocator = nullptr);

    vk::Buffer get() const noexcept;

    vk::DeviceMemory getDeviceMemory() const noexcept;

    VmaAllocation getAlloc() const noexcept;

    void swap(VMAUniqueBuffer &other) noexcept;

    void *map() const;

    void unmap() const;

private:

    vk::Buffer    buffer_;
    VmaAllocation alloc_;
    VmaAllocator  allocator_;

    VmaAllocationInfo allocInfo_;
};

class VMAAlloc : public misc::uncopyable_t
{
public:

    VMAAlloc(
        vk::Instance       instance,
        vk::PhysicalDevice physicalDevice,
        vk::Device         device);

    ~VMAAlloc();

    std::pair<vk::Buffer, VmaAllocation> createBuffer(
        const vk::BufferCreateInfo    &bufferCreateInfo,
        const VmaAllocationCreateInfo &allocCreateInfo);

    VMAUniqueBuffer createBufferUnique(
        const vk::BufferCreateInfo    &bufferCreateInfo,
        const VmaAllocationCreateInfo &allocCreateInfo);

private:

    VmaAllocator alloc_ = nullptr;
};

inline VMAUniqueBuffer::VMAUniqueBuffer()
    : VMAUniqueBuffer(nullptr, nullptr, nullptr)
{
    
}

inline VMAUniqueBuffer::VMAUniqueBuffer(
    vk::Buffer buffer, VmaAllocation alloc, VmaAllocator allocator)
    : buffer_(buffer), alloc_(alloc), allocator_(allocator)
{
    if(buffer_)
        vmaGetAllocationInfo(allocator_, alloc_, &allocInfo_);
}

inline VMAUniqueBuffer::VMAUniqueBuffer(VMAUniqueBuffer &&other) noexcept
    : VMAUniqueBuffer()
{
    swap(other);
}

inline VMAUniqueBuffer &VMAUniqueBuffer::operator=(
    VMAUniqueBuffer &&other) noexcept
{
    reset();
    swap(other);
    return *this;
}

inline VMAUniqueBuffer::~VMAUniqueBuffer()
{
    reset();
}

inline void VMAUniqueBuffer::reset(
    vk::Buffer buffer, VmaAllocation alloc, VmaAllocator allocator)
{
    if(buffer_)
    {
        vmaDestroyBuffer(allocator_, buffer_, alloc_);
        allocInfo_ = {};
    }

    buffer_    = buffer;
    alloc_     = alloc;
    allocator_ = allocator;

    if(buffer_)
        vmaGetAllocationInfo(allocator_, alloc_, &allocInfo_);
}

inline vk::Buffer VMAUniqueBuffer::get() const noexcept
{
    return buffer_;
}

inline vk::DeviceMemory VMAUniqueBuffer::getDeviceMemory() const noexcept
{
    return allocInfo_.deviceMemory;
}

inline VmaAllocation VMAUniqueBuffer::getAlloc() const noexcept
{
    return alloc_;
}

inline void VMAUniqueBuffer::swap(VMAUniqueBuffer &other) noexcept
{
    std::swap(buffer_,    other.buffer_);
    std::swap(alloc_,     other.alloc_);
    std::swap(allocator_, other.allocator_);
    std::swap(allocInfo_, other.allocInfo_);
}

inline void *VMAUniqueBuffer::map() const
{
    void *ret;
    if(auto rt = vmaMapMemory(allocator_, alloc_, &ret); rt != VK_SUCCESS)
    {
        throw std::runtime_error(
            "failed to map vma buffer. err code = " +
            std::to_string(rt));
    }
    return ret;
}

inline void VMAUniqueBuffer::unmap() const
{
    vmaUnmapMemory(allocator_, alloc_);
}

inline VMAAlloc::VMAAlloc(
    vk::Instance       instance,
    vk::PhysicalDevice physicalDevice,
    vk::Device         device)
{
    VmaAllocatorCreateInfo info = {};
    info.instance       = instance;
    info.physicalDevice = physicalDevice;
    info.device         = device;

    if(const auto rt = vmaCreateAllocator(&info, &alloc_); rt != VK_SUCCESS)
    {
        throw std::runtime_error(
            "failed to create vma allocator. err code = " + 
            std::to_string(rt));
    }
}

inline VMAAlloc::~VMAAlloc()
{
    vmaDestroyAllocator(alloc_);
}

inline std::pair<vk::Buffer, VmaAllocation> VMAAlloc::createBuffer(
    const vk::BufferCreateInfo    &bufferCreateInfo,
    const VmaAllocationCreateInfo &allocCreateInfo)
{
    const VkBufferCreateInfo &vkInfo = bufferCreateInfo;
    VkBuffer buffer; VmaAllocation alloc;
    const auto rt = vmaCreateBuffer(
        alloc_, &vkInfo, &allocCreateInfo, &buffer, &alloc, nullptr);
    if(rt != VK_SUCCESS)
    {
        throw std::runtime_error(
            "failed to create vma buffer. err code = " +
            std::to_string(rt));
    }
    return { buffer, alloc };
}

inline VMAUniqueBuffer VMAAlloc::createBufferUnique(
    const vk::BufferCreateInfo    &bufferCreateInfo,
    const VmaAllocationCreateInfo &allocCreateInfo)
{
    auto [buffer, alloc] = createBuffer(bufferCreateInfo, allocCreateInfo);
    return VMAUniqueBuffer(buffer, alloc, alloc_);
}

AGZ_VULKAN_LAB_END
