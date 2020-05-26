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

    VmaAllocation getAlloc() const noexcept;

    void swap(VMAUniqueBuffer &other) noexcept;

    void *map() const;

    void unmap() const;

private:

    vk::Buffer    buffer_;
    VmaAllocation alloc_;
    VmaAllocator  allocator_;
};

class VMAUniqueImage : public misc::uncopyable_t
{
public:

    VMAUniqueImage();

    VMAUniqueImage(vk::Image image, VmaAllocation alloc, VmaAllocator allocator);

    VMAUniqueImage(VMAUniqueImage &&other) noexcept;

    VMAUniqueImage &operator=(VMAUniqueImage &&other) noexcept;

    ~VMAUniqueImage();

    void reset(
        vk::Image     buffer    = nullptr,
        VmaAllocation alloc     = nullptr,
        VmaAllocator  allocator = nullptr);

    vk::Image get() const noexcept;

    VmaAllocation getAlloc() const noexcept;

    void swap(VMAUniqueImage &other) noexcept;

private:

    vk::Image     image_;
    VmaAllocation alloc_;
    VmaAllocator  allocator_;
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

    std::pair<vk::Image, VmaAllocation> createImage(
        const vk::ImageCreateInfo     &imageCreateInfo,
        const VmaAllocationCreateInfo &allocCreateInfo);

    VMAUniqueBuffer createBufferUnique(
        const vk::BufferCreateInfo    &bufferCreateInfo,
        const VmaAllocationCreateInfo &allocCreateInfo);
    
    VMAUniqueImage createImageUnique(
        const vk::ImageCreateInfo     &imageCreateInfo,
        const VmaAllocationCreateInfo &allocCreateInfo);

    VMAUniqueBuffer createStagingBufferUnique(
        size_t byteSize, const void *initData);

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
        vmaDestroyBuffer(allocator_, buffer_, alloc_);

    buffer_    = buffer;
    alloc_     = alloc;
    allocator_ = allocator;
}

inline vk::Buffer VMAUniqueBuffer::get() const noexcept
{
    return buffer_;
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

inline VMAUniqueImage::VMAUniqueImage()
    : VMAUniqueImage(nullptr, nullptr, nullptr)
{
    
}

inline VMAUniqueImage::VMAUniqueImage(
    vk::Image image, VmaAllocation alloc, VmaAllocator allocator)
    : image_(image), alloc_(alloc), allocator_(allocator)
{

}

inline VMAUniqueImage::VMAUniqueImage(VMAUniqueImage &&other) noexcept
    : VMAUniqueImage()
{
    swap(other);
}

inline VMAUniqueImage &VMAUniqueImage::operator=(
    VMAUniqueImage &&other) noexcept
{
    reset();
    swap(other);
    return *this;
}

inline VMAUniqueImage::~VMAUniqueImage()
{
    reset();
}

inline void VMAUniqueImage::reset(
    vk::Image image, VmaAllocation alloc, VmaAllocator allocator)
{
    if(image_)
        vmaDestroyImage(allocator_, image_, alloc_);

    image_     = image;
    alloc_     = alloc;
    allocator_ = allocator;
}

inline vk::Image VMAUniqueImage::get() const noexcept
{
    return image_;
}

inline VmaAllocation VMAUniqueImage::getAlloc() const noexcept
{
    return alloc_;
}

inline void VMAUniqueImage::swap(VMAUniqueImage &other) noexcept
{
    std::swap(image_ ,    other.image_);
    std::swap(alloc_,     other.alloc_);
    std::swap(allocator_, other.allocator_);
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

inline std::pair<vk::Image, VmaAllocation> VMAAlloc::createImage(
    const vk::ImageCreateInfo     &imageCreateInfo,
    const VmaAllocationCreateInfo &allocCreateInfo)
{
    const VkImageCreateInfo vkInfo = imageCreateInfo;
    VkImage image; VmaAllocation alloc;
    const auto rt = vmaCreateImage(
        alloc_, &vkInfo, &allocCreateInfo, &image, &alloc, nullptr);
    if(rt != VK_SUCCESS)
    {
        throw std::runtime_error(
            "failed to create vma image. err code = " +
            std::to_string(rt));
    }
    return { image, alloc };
}

inline VMAUniqueBuffer VMAAlloc::createBufferUnique(
    const vk::BufferCreateInfo    &bufferCreateInfo,
    const VmaAllocationCreateInfo &allocCreateInfo)
{
    auto [buffer, alloc] = createBuffer(bufferCreateInfo, allocCreateInfo);
    return VMAUniqueBuffer(buffer, alloc, alloc_);
}

inline VMAUniqueImage VMAAlloc::createImageUnique(
    const vk::ImageCreateInfo     &imageCreateInfo,
    const VmaAllocationCreateInfo &allocCreateInfo)
{
    auto [image, alloc] = createImage(imageCreateInfo, allocCreateInfo);
    return VMAUniqueImage(image, alloc, alloc_);
}

inline VMAUniqueBuffer VMAAlloc::createStagingBufferUnique(
    size_t byteSize, const void *initData)
{
    vk::BufferCreateInfo bufInfo;
    bufInfo
        .setSize(byteSize)
        .setUsage(vk::BufferUsageFlagBits::eTransferSrc)
        .setSharingMode(vk::SharingMode::eExclusive);

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    auto ret = createBufferUnique(bufInfo, allocInfo);

    if(initData)
    {
        void *mappedData = ret.map();
        std::memcpy(mappedData, initData, byteSize);
        vmaFlushAllocation(alloc_, ret.getAlloc(), 0, byteSize);
        ret.unmap();
    }

    return ret;
}

AGZ_VULKAN_LAB_END
