#include <chrono>
#include <iostream>

#include <vma/vk_mem_alloc.h>

#include <agz/utility/image.h>
#include <agz/vlab/vlab.h>

using agz::vlab::Vec2;
using agz::vlab::Vec3;
using agz::vlab::Mat4;
using agz::vlab::Trans4;

const char *VERTEX_SHADER_SOURCE = R"___(
#version 450

layout(set = 0, binding = 0) uniform UniformBufferObject
{
    mat4 projViewModel;
} ubo;

layout(location = 0) in vec2 iPosition;
layout(location = 1) in vec2 iTexCoord;

layout(location = 0) out vec2 oTexCoord;

void main()
{
    gl_Position = ubo.projViewModel * vec4(iPosition, 0.0, 1.0);
    oTexCoord = iTexCoord;
}
)___";

const char *FRAGMENT_SHADER_SOURCE = R"___(
#version 450

layout(set = 0, binding = 1) uniform sampler2D Texture;

layout(location = 0) in vec2 iTexCoord;

layout(location = 0) out vec4 oColor;

void main()
{
    oColor = pow(texture(Texture, iTexCoord), vec4(2.2));
}
)___";

class TexturePipeline : public agz::misc::uncopyable_t
{
    struct Vertex
    {
        Vec2 position;
        Vec2 texCoord;

        static vk::VertexInputBindingDescription getBindingDesc() noexcept
        {
            vk::VertexInputBindingDescription ret;
            ret
                .setBinding(0)
                .setInputRate(vk::VertexInputRate::eVertex)
                .setStride(sizeof(Vertex));
            return ret;
        }

        static std::array<vk::VertexInputAttributeDescription, 2>
            getAttribDesc() noexcept
        {
            std::array<vk::VertexInputAttributeDescription, 2> ret;
            ret[0]
                .setBinding(0)
                .setFormat(vk::Format::eR32G32Sfloat)
                .setLocation(0)
                .setOffset(offsetof(Vertex, position));
            ret[1]
                .setBinding(0)
                .setFormat(vk::Format::eR32G32Sfloat)
                .setLocation(1)
                .setOffset(offsetof(Vertex, texCoord));
            return ret;
        }
    };

    struct UniformBufferObject
    {
        Mat4 projViewModel;
    };

    struct FrameResource
    {
        vk::UniqueFramebuffer framebuffer;
        vk::CommandBuffer cmdBuf;

        vk::UniqueSemaphore imageSemaphore;
        vk::UniqueSemaphore renderSemaphore;

        vk::UniqueFence frameFence;

        agz::vlab::VMAUniqueBuffer uniformBuf;
        vk::DescriptorSet descSet;
    };

    vk::Device device_;

    // shader

    vk::UniqueShaderModule vertShader_;
    vk::UniqueShaderModule fragShader_;
    vk::PipelineShaderStageCreateInfo shaderStage_[2];

    // renderpass

    vk::UniqueRenderPass renderpass_;

    // pipeline

    vk::UniqueDescriptorSetLayout descSetLayout_;
    vk::UniquePipelineLayout      pipelineLayout_;
    vk::UniquePipeline            pipeline_;

    // vertex/index buffer

    std::unique_ptr<agz::vlab::VMAAlloc> allocator_;

    agz::vlab::VMAUniqueBuffer vertexBuffer_;
    agz::vlab::VMAUniqueBuffer indexBuffer_;

    // shader resource

    agz::vlab::VMAUniqueImage image_;
    vk::UniqueImageView imageView_;

    vk::UniqueSampler sampler_;

    // frameresource

    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

    uint32_t currentFrame_ = 0;

    vk::UniqueCommandPool    cmdPool_;
    vk::UniqueDescriptorPool descPool_;

    std::vector<FrameResource> frameRscs_;

    agz::vlab::VMAUniqueBuffer createDeviceBuffer(
        size_t byteSize, vk::BufferUsageFlags usage, const void *initData,
        const agz::vlab::Window &window)
    {
        // create staging buffer

        auto stagingBuffer = allocator_->createStagingBufferUnique(
            byteSize, initData);

        // create ret buffer

        vk::BufferCreateInfo bufInfo;
        bufInfo
            .setSize(byteSize)
            .setUsage(usage | vk::BufferUsageFlagBits::eTransferDst)
            .setSharingMode(vk::SharingMode::eExclusive);

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        auto ret = allocator_->createBufferUnique(bufInfo, allocInfo);

        // copy command buffer

        vk::CommandBufferAllocateInfo cmdBufAllocInfo;
        cmdBufAllocInfo
            .setCommandPool(cmdPool_.get())
            .setCommandBufferCount(1)
            .setLevel(vk::CommandBufferLevel::ePrimary);

        auto copyCmdBuf = std::move(
            device_.allocateCommandBuffersUnique(cmdBufAllocInfo)[0]);

        vk::CommandBufferBeginInfo begInfo;
        begInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

        vk::BufferCopy copy;
        copy.setSrcOffset(0).setDstOffset(0).setSize(byteSize);

        vk::BufferMemoryBarrier barrier(
            vk::AccessFlagBits::eTransferWrite,
            vk::AccessFlagBits::eVertexAttributeRead,
            VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
            ret.get(), 0, byteSize);

        copyCmdBuf->begin(begInfo);

        copyCmdBuf->copyBuffer(stagingBuffer.get(), ret.get(), 1, &copy);
        copyCmdBuf->pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eVertexInput,
            {}, 0, nullptr, 1, &barrier, 0, nullptr);

        copyCmdBuf->end();

        vk::SubmitInfo submit;
        submit
            .setCommandBufferCount(1)
            .setPCommandBuffers(&copyCmdBuf.get());

        (void)window.getGraphicsQueue().submit(1, &submit, nullptr);
        window.getGraphicsQueue().waitIdle();

        return ret;
    }

    void initShaders(vk::Device device)
    {
        vk::ShaderModuleCreateInfo info;

        // vertex shader module

        const auto vertByteCode = compileGLSLToSPIRV(
            VERTEX_SHADER_SOURCE, "vertex shader", {},
            agz::vlab::ShaderModuleType::Vertex, false);
        info
            .setCodeSize(vertByteCode.size() *
                sizeof(decltype(vertByteCode)::value_type))
            .setPCode(vertByteCode.data());
        vertShader_ = device.createShaderModuleUnique(info);

        // vertex shader stage

        shaderStage_[0]
            .setModule(vertShader_.get())
            .setStage(vk::ShaderStageFlagBits::eVertex)
            .setPName("main");

        // fragment shader module

        const auto fragByteCode = compileGLSLToSPIRV(
            FRAGMENT_SHADER_SOURCE, "fragment shader", {},
            agz::vlab::ShaderModuleType::Fragment, false);
        info
            .setCodeSize(fragByteCode.size() *
                sizeof(decltype(fragByteCode)::value_type))
            .setPCode(fragByteCode.data());
        fragShader_ = device.createShaderModuleUnique(info);

        // fragment shader stage

        shaderStage_[1]
            .setModule(fragShader_.get())
            .setStage(vk::ShaderStageFlagBits::eFragment)
            .setPName("main");
    }

    void initCmdPool(const agz::vlab::Window &window)
    {
        vk::CommandPoolCreateInfo poolInfo;
        poolInfo
            .setQueueFamilyIndex(
                window.getGraphicsDevice().graphicsQueueFamilyIndex())
            .setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
        cmdPool_ = device_.createCommandPoolUnique(poolInfo);
    }

    vk::UniqueDescriptorSetLayout createDescSetLayout(vk::Device device)
    {
        vk::DescriptorSetLayoutBinding bindings[2];
        bindings[0]
            .setBinding(0)
            .setDescriptorCount(1)
            .setDescriptorType(vk::DescriptorType::eUniformBuffer)
            .setStageFlags(vk::ShaderStageFlagBits::eVertex);
        bindings[1]
            .setBinding(1)
            .setDescriptorCount(1)
            .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
            .setStageFlags(vk::ShaderStageFlagBits::eFragment);

        vk::DescriptorSetLayoutCreateInfo info;
        info
            .setBindingCount(2)
            .setPBindings(bindings);

        return device.createDescriptorSetLayoutUnique(info);
    }

    void initRenderpass(const agz::vlab::Window &window)
    {
        vk::AttachmentDescription colorAttachment;
        colorAttachment
            .setFormat(window.getSwapchainFormat())
            .setSamples(vk::SampleCountFlagBits::e1)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
            .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
            .setInitialLayout(vk::ImageLayout::eUndefined)
            .setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

        vk::AttachmentReference attachmentRef;
        attachmentRef
            .setAttachment(0)
            .setLayout(vk::ImageLayout::eColorAttachmentOptimal);

        vk::SubpassDescription subpass;
        subpass
            .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
            .setColorAttachmentCount(1)
            .setPColorAttachments(&attachmentRef);

        vk::SubpassDependency dependency;
        dependency
            .setSrcSubpass(VK_SUBPASS_EXTERNAL)
            .setDstSubpass(0)
            .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
            .setSrcAccessMask({})
            .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
            .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

        vk::RenderPassCreateInfo info;
        info
            .setAttachmentCount(1)
            .setPAttachments(&colorAttachment)
            .setSubpassCount(1)
            .setPSubpasses(&subpass)
            .setDependencyCount(1)
            .setPDependencies(&dependency);

        renderpass_ = device_.createRenderPassUnique(info);
    }

    void initGraphicsPipeline(const agz::vlab::Window &window)
    {
        const auto vertexBindingDesc = Vertex::getBindingDesc();
        const auto vertexAttribDesc = Vertex::getAttribDesc();
        vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
        vertexInputInfo
            .setVertexBindingDescriptionCount(1)
            .setPVertexBindingDescriptions(&vertexBindingDesc)
            .setVertexAttributeDescriptionCount(
                static_cast<uint32_t>(vertexAttribDesc.size()))
            .setPVertexAttributeDescriptions(vertexAttribDesc.data());

        vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
        inputAssembly.setTopology(vk::PrimitiveTopology::eTriangleList);

        const auto [scX, scY] = window.getSwapchainExtent();
        vk::Viewport viewport(
            0, 0, static_cast<float>(scX), static_cast<float>(scY), 0, 1);
        vk::Rect2D scissor({ 0, 0 }, { scX, scY });

        vk::PipelineViewportStateCreateInfo viewportState;
        viewportState
            .setViewportCount(1).setPViewports(&viewport)
            .setScissorCount(1).setPScissors(&scissor);

        vk::PipelineRasterizationStateCreateInfo rasterizerState;
        rasterizerState
            .setDepthBiasEnable(false)
            .setDepthClampEnable(false)
            .setRasterizerDiscardEnable(false)
            .setPolygonMode(vk::PolygonMode::eFill)
            .setLineWidth(1)
            .setCullMode(vk::CullModeFlagBits::eNone);

        vk::PipelineMultisampleStateCreateInfo multisampleState;
        multisampleState
            .setSampleShadingEnable(false)
            .setRasterizationSamples(vk::SampleCountFlagBits::e1)
            .setMinSampleShading(1);

        vk::PipelineColorBlendAttachmentState colorBlendAttachment;
        colorBlendAttachment
            .setColorWriteMask(
                vk::ColorComponentFlagBits::eR |
                vk::ColorComponentFlagBits::eG |
                vk::ColorComponentFlagBits::eB |
                vk::ColorComponentFlagBits::eA);

        vk::PipelineColorBlendStateCreateInfo blendState;
        blendState
            .setAttachmentCount(1)
            .setPAttachments(&colorBlendAttachment);

        vk::DescriptorSetLayout descSetLayoutRaw[] = { descSetLayout_.get() };

        vk::PipelineLayoutCreateInfo layoutInfo;
        layoutInfo
            .setSetLayoutCount(1)
            .setPSetLayouts(descSetLayoutRaw);
        pipelineLayout_ = device_.createPipelineLayoutUnique(layoutInfo);

        vk::GraphicsPipelineCreateInfo pipelineInfo;
        pipelineInfo
            .setStageCount(2)
            .setPStages(shaderStage_)
            .setPVertexInputState(&vertexInputInfo)
            .setPInputAssemblyState(&inputAssembly)
            .setPViewportState(&viewportState)
            .setPRasterizationState(&rasterizerState)
            .setPMultisampleState(&multisampleState)
            .setPColorBlendState(&blendState)
            .setLayout(pipelineLayout_.get())
            .setRenderPass(renderpass_.get())
            .setSubpass(0)
            .setBasePipelineIndex(-1);

        pipeline_ = device_.createGraphicsPipelineUnique(
            nullptr, pipelineInfo);
    }

    void initVertexIndexBuffer(const agz::vlab::Window &window)
    {
        allocator_ = std::make_unique<agz::vlab::VMAAlloc>(
            window.getInstance(),
            window.getPhysicalDevice(),
            device_);

        // vertex buffer

        const Vertex vertexData[] = {
            { { -0.5f, +0.5f }, { 1.0f, 0.0f } },
            { { -0.5f, -0.5f }, { 1.0f, 1.0f } },
            { { +0.5f, -0.5f }, { 0.0f, 1.0f } },
            { { +0.5f, +0.5f }, { 0.0f, 0.0f } }
        };

        vertexBuffer_ = createDeviceBuffer(
            sizeof(vertexData),
            vk::BufferUsageFlagBits::eVertexBuffer,
            vertexData, window);

        // index buffer

        const uint16_t indexData[] = { 0, 1, 2, 0, 2, 3 };

        indexBuffer_ = createDeviceBuffer(
            sizeof(indexData),
            vk::BufferUsageFlagBits::eIndexBuffer,
            indexData, window);
    }

    void initDescriptorPool()
    {
        vk::DescriptorPoolSize poolSize[2];
        poolSize[0]
            .setDescriptorCount(MAX_FRAMES_IN_FLIGHT)
            .setType(vk::DescriptorType::eUniformBuffer);
        poolSize[1]
            .setDescriptorCount(MAX_FRAMES_IN_FLIGHT)
            .setType(vk::DescriptorType::eCombinedImageSampler);

        vk::DescriptorPoolCreateInfo info;
        info
            .setMaxSets(MAX_FRAMES_IN_FLIGHT)
            .setPoolSizeCount(2)
            .setPPoolSizes(poolSize);

        descPool_ = device_.createDescriptorPoolUnique(info);
    }

    void initImage(const agz::vlab::Window &window)
    {
        // load image data

        auto imgData = agz::img::load_rgba_from_file("05_texture.png");
        if(!imgData.is_available())
            throw std::runtime_error("failed to load image data");

        // create image

        vk::ImageCreateInfo imgInfo;
        imgInfo
            .setImageType(vk::ImageType::e2D)
            .setFormat(vk::Format::eR8G8B8A8Unorm)
            .setExtent({
                uint32_t(imgData.shape()[1]),
                uint32_t(imgData.shape()[0]),
                1 })
            .setMipLevels(1)
            .setArrayLayers(1)
            .setSamples(vk::SampleCountFlagBits::e1)
            .setTiling(vk::ImageTiling::eOptimal)
            .setUsage(vk::ImageUsageFlagBits::eTransferDst |
                      vk::ImageUsageFlagBits::eSampled)
            .setSharingMode(vk::SharingMode::eExclusive)
            .setInitialLayout(vk::ImageLayout::eUndefined);

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        image_ = allocator_->createImageUnique(imgInfo, allocInfo);

        // create image view

        vk::ImageViewCreateInfo viewInfo;
        viewInfo
            .setImage(image_.get())
            .setViewType(vk::ImageViewType::e2D)
            .setFormat(vk::Format::eR8G8B8A8Unorm)
            .setComponents({
                vk::ComponentSwizzle::eIdentity,
                vk::ComponentSwizzle::eIdentity,
                vk::ComponentSwizzle::eIdentity,
                vk::ComponentSwizzle::eIdentity})
            .setSubresourceRange({
                vk::ImageAspectFlagBits::eColor,
                0, 1, 0, 1});

        imageView_ = device_.createImageViewUnique(viewInfo);

        // create staging buffer

        auto stagingBuffer = allocator_->createStagingBufferUnique(
            imgData.elem_count() * sizeof(decltype(imgData)::elem_t),
            imgData.raw_data());

        // copy texture data

        vk::CommandBufferAllocateInfo cmdBufAllocInfo;
        cmdBufAllocInfo
            .setCommandPool(cmdPool_.get())
            .setCommandBufferCount(1)
            .setLevel(vk::CommandBufferLevel::ePrimary);

        auto copyCmdBuf = std::move(
            device_.allocateCommandBuffersUnique(cmdBufAllocInfo)[0]);

        vk::CommandBufferBeginInfo cmdBegInfo;
        cmdBegInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

        copyCmdBuf->begin(cmdBegInfo);

        vk::ImageSubresourceRange imgRng(
            vk::ImageAspectFlagBits::eColor,
            0, 1, 0, 1);

        vk::ImageMemoryBarrier imgBarrier1;
        imgBarrier1
            .setSrcAccessMask({})
            .setDstAccessMask(vk::AccessFlagBits::eTransferWrite)
            .setOldLayout(vk::ImageLayout::eUndefined)
            .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
            .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setImage(image_.get())
            .setSubresourceRange(imgRng);

        copyCmdBuf->pipelineBarrier(
            vk::PipelineStageFlagBits::eTopOfPipe,
            vk::PipelineStageFlagBits::eTransfer,
            {}, 0, nullptr, 0, nullptr, 1, &imgBarrier1);

        vk::BufferImageCopy bufImgCopy;
        bufImgCopy
            .setBufferOffset(0)
            .setBufferRowLength(0)
            .setBufferImageHeight(0)
            .setImageSubresource({
                vk::ImageAspectFlagBits::eColor, 0, 0, 1 })
            .setImageOffset({ 0, 0, 0 })
            .setImageExtent({
                uint32_t(imgData.shape()[1]),
                uint32_t(imgData.shape()[0]),
                1 });

        copyCmdBuf->copyBufferToImage(
            stagingBuffer.get(), image_.get(),
            vk::ImageLayout::eTransferDstOptimal, 1,
            &bufImgCopy);

        vk::ImageMemoryBarrier imgBarrier2;
        imgBarrier2
            .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
            .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
            .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
            .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
            .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setImage(image_.get())
            .setSubresourceRange(imgRng);

        copyCmdBuf->pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eFragmentShader,
            {}, 0, nullptr, 0, nullptr,
            1, &imgBarrier2);

        copyCmdBuf->end();

        vk::SubmitInfo submit;
        submit
            .setCommandBufferCount(1)
            .setPCommandBuffers(&copyCmdBuf.get());

        (void)window.getGraphicsQueue().submit(1, &submit, nullptr);

        device_.waitIdle();
    }

    void initSampler()
    {
        vk::SamplerCreateInfo info;
        info
            .setMagFilter(vk::Filter::eLinear)
            .setMinFilter(vk::Filter::eLinear)
            .setMipmapMode(vk::SamplerMipmapMode::eNearest)
            .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
            .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
            .setAddressModeW(vk::SamplerAddressMode::eClampToEdge);

        sampler_ = device_.createSamplerUnique(info);
    }

    void initFramebufferResource(FrameResource &frame)
    {
        // semaphores

        vk::SemaphoreCreateInfo semaphoreInfo;
        frame.imageSemaphore  = device_.createSemaphoreUnique(semaphoreInfo);
        frame.renderSemaphore = device_.createSemaphoreUnique(semaphoreInfo);

        // fence

        vk::FenceCreateInfo fenceInfo;
        fenceInfo.setFlags(vk::FenceCreateFlagBits::eSignaled);
        frame.frameFence = device_.createFenceUnique(fenceInfo);

        // uniform buffer

        vk::BufferCreateInfo uniformBufInfo;
        uniformBufInfo
            .setSize(sizeof(UniformBufferObject))
            .setUsage(vk::BufferUsageFlagBits::eUniformBuffer)
            .setSharingMode(vk::SharingMode::eExclusive);

        VmaAllocationCreateInfo uniformBufAllocInfo = {};
        uniformBufAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

        frame.uniformBuf = allocator_->createBufferUnique(
            uniformBufInfo, uniformBufAllocInfo);

        // desc set

        vk::DescriptorSetAllocateInfo dsInfo;
        dsInfo
            .setDescriptorPool(descPool_.get())
            .setDescriptorSetCount(1)
            .setPSetLayouts(&descSetLayout_.get());

        frame.descSet = device_.allocateDescriptorSets(dsInfo).front();

        vk::DescriptorBufferInfo bufInfo;
        bufInfo
            .setBuffer(frame.uniformBuf.get())
            .setOffset(0)
            .setRange(sizeof(UniformBufferObject));

        vk::DescriptorImageInfo imageInfo;
        imageInfo
            .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
            .setImageView(imageView_.get())
            .setSampler(sampler_.get());

        vk::WriteDescriptorSet descWrite[2];
        descWrite[0]
            .setDescriptorCount(1)
            .setDescriptorType(vk::DescriptorType::eUniformBuffer)
            .setDstArrayElement(0)
            .setDstBinding(0)
            .setDstSet(frame.descSet)
            .setPBufferInfo(&bufInfo);
        descWrite[1]
            .setDescriptorCount(1)
            .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
            .setDstArrayElement(0)
            .setDstBinding(1)
            .setDstSet(frame.descSet)
            .setPImageInfo(&imageInfo);

        device_.updateDescriptorSets(2, descWrite, 0, nullptr);

        // command buffer

        vk::CommandBufferAllocateInfo cmdBufInfo;
        cmdBufInfo
            .setCommandPool(cmdPool_.get())
            .setLevel(vk::CommandBufferLevel::ePrimary)
            .setCommandBufferCount(1);

        frame.cmdBuf = device_.allocateCommandBuffers(cmdBufInfo).front();
    }

    void recordCommandBuffer(
        const agz::vlab::Window &window, FrameResource &frame)
    {
        auto cb = frame.cmdBuf;

        vk::CommandBufferBeginInfo beginInfo;
        beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

        vk::ClearValue clearValue(vk::ClearColorValue(
            std::array<float, 4>{ 0, 0, 0, 1 }));

        vk::RenderPassBeginInfo renderpassInfo;
        renderpassInfo
            .setRenderPass(renderpass_.get())
            .setFramebuffer(frame.framebuffer.get())
            .setRenderArea({ { 0, 0 }, window.getSwapchainExtent() })
            .setClearValueCount(1)
            .setPClearValues(&clearValue);

        cb.begin(beginInfo);
        cb.beginRenderPass(renderpassInfo, vk::SubpassContents::eInline);

        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline_.get());

        vk::Buffer vertexBuffers[] = { vertexBuffer_.get() };
        vk::DeviceSize offsets[] = { 0 };
        cb.bindVertexBuffers(0, 1, vertexBuffers, offsets);

        cb.bindIndexBuffer(indexBuffer_.get(), 0, vk::IndexType::eUint16);

        cb.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, pipelineLayout_.get(),
            0, 1, &frame.descSet, 0, nullptr);

        cb.drawIndexed(6, 1, 0, 0, 0);

        cb.endRenderPass();
        cb.end();
    }

    void initSync()
    {
        vk::SemaphoreCreateInfo semaphoreInfo;

        vk::FenceCreateInfo fenceInfo;
        fenceInfo.setFlags(vk::FenceCreateFlagBits::eSignaled);

        for(uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            frameRscs_[i].imageSemaphore = device_.createSemaphoreUnique(
                semaphoreInfo);

            frameRscs_[i].renderSemaphore = device_.createSemaphoreUnique(
                semaphoreInfo);

            frameRscs_[i].frameFence = device_.createFenceUnique(fenceInfo);
        }
    }

    void updateUniformBuffer(const float wOverH, FrameResource &frame)
    {
        using Clock = std::chrono::high_resolution_clock;

        const static auto start = Clock::now();

        const float durationS = std::chrono::duration_cast<
            std::chrono::microseconds>(Clock::now() - start).count() / 1e6f;

        Mat4 proj = Trans4::perspective(
            agz::math::deg2rad(60.0f), wOverH, 0.1f, 100.0f);
        proj[1][1] *= -1;

        const Mat4 projViewModel =
            proj
            * Trans4::look_at({ 0, 0, -1.5f }, { 0, 0, 0 }, { 0, 1, 0 })
            * Trans4::rotate_y(durationS);

        const UniformBufferObject ubData = { projViewModel };

        auto &ub = frame.uniformBuf;
        void *mappedData = ub.map();
        std::memcpy(mappedData, &ubData, sizeof(ubData));
        ub.unmap();
    }

    void preRecreateSwapchain()
    {
        device_.waitIdle();

        for(auto &f : frameRscs_)
            f.framebuffer.reset();

        pipeline_.reset();
        pipelineLayout_.reset();
        renderpass_.reset();
    }

    void postRecreateSwapchain(const agz::vlab::Window &window)
    {
        initRenderpass(window);
        initGraphicsPipeline(window);
    }

public:

    explicit TexturePipeline(agz::vlab::Window &window)
    {
        device_ = window.getDevice();

        frameRscs_.resize(MAX_FRAMES_IN_FLIGHT);

        initCmdPool(window);
        initRenderpass(window);
        initShaders(device_);
        descSetLayout_ = createDescSetLayout(device_);
        initGraphicsPipeline(window);
        initVertexIndexBuffer(window);
        initImage(window);
        initSampler();
        initDescriptorPool();

        for(auto &f : frameRscs_)
            initFramebufferResource(f);

        auto preHandler = std::make_shared<agz::event::functional_receiver_t<
            agz::vlab::WindowPreRecreateSwapchainEvent>>(
                [&](const agz::vlab::WindowPreRecreateSwapchainEvent &)
        {
            preRecreateSwapchain();
        });

        auto postHandler = std::make_shared<agz::event::functional_receiver_t<
            agz::vlab::WindowPostRecreateSwapchainEvent>>(
                [&](const agz::vlab::WindowPostRecreateSwapchainEvent &)
        {
            postRecreateSwapchain(window);
        });

        window.attach<agz::vlab::WindowPreRecreateSwapchainEvent>(preHandler);
        window.attach<agz::vlab::WindowPostRecreateSwapchainEvent>(postHandler);
    }

    ~TexturePipeline()
    {
        frameRscs_.clear();

        descPool_.reset();

        sampler_.reset();
        imageView_.reset();
        image_.reset();

        vertexBuffer_.reset();
        indexBuffer_.reset();

        allocator_.reset();

        pipeline_.reset();
        pipelineLayout_.reset();
        descSetLayout_.reset();

        vertShader_.reset();
        fragShader_.reset();

        renderpass_.reset();
        cmdPool_.reset();
    }

    void renderFrame(agz::vlab::Window &window)
    {
        auto &frame = frameRscs_[currentFrame_];

        vk::Fence frameFence[] = { frame.frameFence.get() };
        (void)device_.waitForFences(1, frameFence, true, UINT64_MAX);
        (void)device_.resetFences(1, frameFence);

        const auto nextImageResult = window.acquireNextImage(
            UINT64_MAX, frame.imageSemaphore.get(), nullptr);
        if(nextImageResult.result == vk::Result::eErrorOutOfDateKHR)
            return window.recreateSwapchain();
        const uint32_t imageIndex = nextImageResult.value;

        if(frame.framebuffer)
            frame.framebuffer.reset();

        {
            auto &iv = window.getSwapchainImageViews()[imageIndex];
            vk::ImageView attachments[] = { iv.get() };

            vk::FramebufferCreateInfo info;
            info
                .setRenderPass(renderpass_.get())
                .setAttachmentCount(1)
                .setPAttachments(attachments)
                .setWidth(window.getSwapchainExtent().width)
                .setHeight(window.getSwapchainExtent().height)
                .setLayers(1);

            frame.framebuffer = device_.createFramebufferUnique(info);
        }

        updateUniformBuffer(window.getSwapchainAspectRatio(), frame);

        vk::Semaphore waitSemaphores[] = {
            frame.imageSemaphore.get()
        };

        vk::PipelineStageFlags waitStages[] = {
            vk::PipelineStageFlagBits::eColorAttachmentOutput
        };

        vk::Semaphore signalSemaphores[] = {
            frame.renderSemaphore.get()
        };

        recordCommandBuffer(window, frame);

        vk::SubmitInfo submitInfo;
        submitInfo
            .setWaitSemaphoreCount(1)
            .setPWaitSemaphores(waitSemaphores)
            .setPWaitDstStageMask(waitStages)
            .setCommandBufferCount(1)
            .setPCommandBuffers(&frame.cmdBuf)
            .setSignalSemaphoreCount(1)
            .setPSignalSemaphores(signalSemaphores);

        (void)window.getGraphicsQueue().submit(1, &submitInfo, frameFence[0]);

        vk::SwapchainKHR swapchains[] = { window.getSwapchain() };

        vk::PresentInfoKHR presentInfo;
        presentInfo
            .setWaitSemaphoreCount(1)
            .setPWaitSemaphores(signalSemaphores)
            .setSwapchainCount(1)
            .setPSwapchains(swapchains)
            .setPImageIndices(&imageIndex);

        (void)window.getPresentQueue().presentKHR(presentInfo);

        currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
    }
};

void run()
{
    agz::vlab::ValidationLayerManager layers;
    layers.add("VK_LAYER_KHRONOS_validation");

    agz::vlab::Window window;
    window.Initialize(agz::vlab::WindowDesc()
        .setSize(640, 480)
        .setTitle("AirGuanZ's Vulkan Lab: 05.texture")
        .setDebugMessage(true)
        .setLayers(&layers)
        .setResizable(true));

    window.getDebugMsgMgr()->enableStdErrOutput(
        agz::vlab::DebugMsgLevel::Verbose);

    TexturePipeline pipeline(window);

    while(!window.getCloseFlag())
    {
        window.doEvents();
        pipeline.renderFrame(window);
    }

    window.getDevice().waitIdle();
}

int main()
{
    try
    {
        run();
    }
    catch(const std::exception &err)
    {
        std::cout << err.what() << std::endl;
        return -1;
    }
}
