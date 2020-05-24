#include <chrono>
#include <iostream>

#include <vma/vk_mem_alloc.h>

#include <agz/vlab/vlab.h>

using agz::vlab::Vec2;
using agz::vlab::Vec3;
using agz::vlab::Mat4;
using agz::vlab::Trans4;

const char *VERTEX_SHADER_SOURCE = R"___(
#version 450

layout(binding = 0) uniform UniformBufferObject
{
    mat4 projViewModel;
} ubo;

layout(location = 0) in vec2 iPosition;
layout(location = 1) in vec3 iColor;

layout(location = 0) out vec3 oColor;

void main()
{
    gl_Position = ubo.projViewModel * vec4(iPosition, 0.0, 1.0);
    oColor = iColor;
}
)___";

const char *FRAGMENT_SHADER_SOURCE = R"___(
#version 450

layout(location = 0) in vec3 iColor;

layout(location = 0) out vec4 oColor;

void main()
{
    oColor = vec4(iColor, 1.0);
}
)___";

class StagingBufferPipeline : public agz::misc::uncopyable_t
{
    struct Vertex
    {
        Vec2 position;
        Vec3 color;

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
                .setFormat(vk::Format::eR32G32B32Sfloat)
                .setLocation(1)
                .setOffset(offsetof(Vertex, color));
            return ret;
        }
    };

    struct UniformBufferObject
    {
        Mat4 projViewModel;

        static vk::UniqueDescriptorSetLayout createDescSetLayout(vk::Device device)
        {
            vk::DescriptorSetLayoutBinding binding;
            binding
                .setBinding(0)
                .setDescriptorCount(1)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setStageFlags(vk::ShaderStageFlagBits::eVertex);

            vk::DescriptorSetLayoutCreateInfo info;
            info
                .setBindingCount(1)
                .setPBindings(&binding);

            return device.createDescriptorSetLayoutUnique(info);
        }
    };

    vk::Device device_;

    vk::UniqueShaderModule vertShader_;
    vk::UniqueShaderModule fragShader_;
    vk::PipelineShaderStageCreateInfo shaderStage_[2];

    vk::UniqueRenderPass renderpass_;

    vk::UniqueDescriptorSetLayout descSetLayout_;
    vk::UniquePipelineLayout      pipelineLayout_;
    vk::UniquePipeline            pipeline_;

    std::vector<vk::UniqueFramebuffer> framebuffers_;

    vk::UniqueCommandPool cmdPool_;
    std::vector<vk::CommandBuffer> cmdBuffers_;

    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    std::vector<vk::UniqueSemaphore> imageSemaphores_;
    std::vector<vk::UniqueSemaphore> renderSemaphores_;

    std::vector<vk::UniqueFence> inFlightFrames_;
    std::vector<vk::Fence>       imageInFlight_;

    uint32_t currentFrame_ = 0;

    std::unique_ptr<agz::vlab::VMAAlloc> allocator_;

    agz::vlab::VMAUniqueBuffer vertexBuffer_;
    agz::vlab::VMAUniqueBuffer indexBuffer_;

    std::vector<agz::vlab::VMAUniqueBuffer> uniformBuffers_;
    vk::UniqueDescriptorPool descPool_;
    std::vector<vk::DescriptorSet> descSets_;

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

        copyCmdBuf->begin(begInfo);
        copyCmdBuf->copyBuffer(stagingBuffer.get(), ret.get(), 1, &copy);
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
            VERTEX_SHADER_SOURCE, "", {},
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
            FRAGMENT_SHADER_SOURCE, "", {},
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
        poolInfo.setQueueFamilyIndex(
            window.getGraphicsDevice().graphicsQueueFamilyIndex());

        cmdPool_ = window.getDevice().createCommandPoolUnique(poolInfo);
    }

    void initRenderpass(const agz::vlab::Window &window)
    {
        vk::AttachmentDescription colorAttachment;
        colorAttachment
            .setFormat        (window.getSwapchainFormat())
            .setSamples       (vk::SampleCountFlagBits::e1)
            .setLoadOp        (vk::AttachmentLoadOp::eClear)
            .setStoreOp       (vk::AttachmentStoreOp::eStore)
            .setStencilLoadOp (vk::AttachmentLoadOp::eDontCare)
            .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
            .setInitialLayout (vk::ImageLayout::eUndefined)
            .setFinalLayout   (vk::ImageLayout::ePresentSrcKHR);

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
            .setDstAccessMask({});

        vk::RenderPassCreateInfo info;
        info
            .setAttachmentCount(1)
            .setPAttachments(&colorAttachment)
            .setSubpassCount(1)
            .setPSubpasses(&subpass)
            .setDependencyCount(1)
            .setPDependencies(&dependency);

        renderpass_ = window.getDevice().createRenderPassUnique(info);
    }

    void initGraphicsPipeline(const agz::vlab::Window &window)
    {
        const auto vertexBindingDesc = Vertex::getBindingDesc();
        const auto vertexAttribDesc  = Vertex::getAttribDesc();
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
            .setScissorCount(1) .setPScissors(&scissor);

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

        descSetLayout_ = UniformBufferObject::createDescSetLayout(device_);
        vk::DescriptorSetLayout descSetLayoutRaw[] = { descSetLayout_.get() };

        vk::PipelineLayoutCreateInfo layoutInfo;
        layoutInfo
            .setSetLayoutCount(1)
            .setPSetLayouts(descSetLayoutRaw);
        pipelineLayout_ = window.getDevice().createPipelineLayoutUnique(layoutInfo);

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

        pipeline_ = window.getDevice().createGraphicsPipelineUnique(
            nullptr, pipelineInfo);
    }

    void initFramebuffer(const agz::vlab::Window &window)
    {
        const auto &imageViews = window.getSwapchainImageViews();
        for(auto &iv : imageViews)
        {
            vk::ImageView attachments[] = { iv.get() };

            vk::FramebufferCreateInfo info;
            info
                .setRenderPass(renderpass_.get())
                .setAttachmentCount(1)
                .setPAttachments(attachments)
                .setWidth(window.getSwapchainExtent().width)
                .setHeight(window.getSwapchainExtent().height)
                .setLayers(1);

            framebuffers_.push_back(
                window.getDevice().createFramebufferUnique(info));
        }
    }

    void initVertexIndexBuffer(const agz::vlab::Window &window)
    {
        allocator_ = std::make_unique<agz::vlab::VMAAlloc>(
            window.getInstance(),
            window.getPhysicalDevice(),
            window.getDevice());

        // vertex buffer

        const Vertex vertexData[] = {
            { { -0.5f, +0.5f }, { 1.0f, 0.0f, 0.0f } },
            { { -0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f } },
            { { +0.5f, -0.5f }, { 0.0f, 0.0f, 1.0f } },
            { { +0.5f, +0.5f }, { 0.0f, 1.0f, 1.0f } }
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

    void initUniformBuffers(const agz::vlab::Window &window)
    {
        vk::BufferCreateInfo bufInfo;
        bufInfo
            .setSize(sizeof(UniformBufferObject))
            .setUsage(vk::BufferUsageFlagBits::eUniformBuffer)
            .setSharingMode(vk::SharingMode::eExclusive);

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

        for(uint32_t i = 0; i < window.getSwapchainImageCount(); ++i)
        {
            uniformBuffers_.push_back(
                allocator_->createBufferUnique(bufInfo, allocInfo));
        }
    }

    void initDescriptorPool(const agz::vlab::Window &window)
    {
        vk::DescriptorPoolSize poolSize;
        poolSize
            .setDescriptorCount(window.getSwapchainImageCount())
            .setType(vk::DescriptorType::eUniformBuffer);

        vk::DescriptorPoolCreateInfo info;
        info
            .setMaxSets(window.getSwapchainImageCount())
            .setPoolSizeCount(1)
            .setPPoolSizes(&poolSize);

        descPool_ = device_.createDescriptorPoolUnique(info);
    }

    void initDescriptorSets(const agz::vlab::Window &window)
    {
        std::vector<vk::DescriptorSetLayout> layout(
            window.getSwapchainImageCount(), descSetLayout_.get());

        vk::DescriptorSetAllocateInfo allocInfo;
        allocInfo
            .setDescriptorPool(descPool_.get())
            .setDescriptorSetCount(window.getSwapchainImageCount())
            .setPSetLayouts(layout.data());

        descSets_ = device_.allocateDescriptorSets(allocInfo);

        for(uint32_t i = 0; i < window.getSwapchainImageCount(); ++i)
        {
            vk::DescriptorBufferInfo bufInfo;
            bufInfo
                .setBuffer(uniformBuffers_[i].get())
                .setOffset(0)
                .setRange(sizeof(UniformBufferObject));

            vk::WriteDescriptorSet descWrite;
            descWrite
                .setDescriptorCount(1)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setDstArrayElement(0)
                .setDstBinding(0)
                .setDstSet(descSets_[i])
                .setPBufferInfo(&bufInfo);

            device_.updateDescriptorSets(1, &descWrite, 0, nullptr);
        }
    }

    void initCmdBuffers(const agz::vlab::Window &window)
    {
        vk::CommandBufferAllocateInfo allocInfo;
        allocInfo
            .setCommandPool(cmdPool_.get())
            .setLevel(vk::CommandBufferLevel::ePrimary)
            .setCommandBufferCount(window.getSwapchainImageCount());

        cmdBuffers_ = window.getDevice().allocateCommandBuffers(allocInfo);

        for(size_t i = 0; i < cmdBuffers_.size(); ++i)
        {
            auto cb = cmdBuffers_[i];

            vk::CommandBufferBeginInfo beginInfo;

            vk::ClearValue clearValue(vk::ClearColorValue(
                std::array<float, 4>{ 0, 0, 0, 1 }));
            
            vk::RenderPassBeginInfo renderpassInfo;
            renderpassInfo
                .setRenderPass(renderpass_.get())
                .setFramebuffer(framebuffers_[i].get())
                .setRenderArea({ { 0, 0 }, window.getSwapchainExtent() })
                .setClearValueCount(1)
                .setPClearValues(&clearValue);

            cb.begin(beginInfo);
            cb.beginRenderPass(renderpassInfo, vk::SubpassContents::eInline);

            cb.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline_.get());

            vk::Buffer vertexBuffers[] = { vertexBuffer_.get() };
            vk::DeviceSize offsets[]   = { 0 };
            cb.bindVertexBuffers(0, 1, vertexBuffers, offsets);

            cb.bindIndexBuffer(indexBuffer_.get(), 0, vk::IndexType::eUint16);

            cb.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics, pipelineLayout_.get(),
                0, 1, &descSets_[i], 0, nullptr);
            
            cb.drawIndexed(6, 1, 0, 0, 0);

            cb.endRenderPass();
            cb.end();
        }
    }

    void initSync(const agz::vlab::Window &window)
    {
        auto device = window.getDevice();

        imageInFlight_.resize(window.getSwapchainImageCount());

        vk::SemaphoreCreateInfo semaphoreInfo;

        vk::FenceCreateInfo fenceInfo;
        fenceInfo.setFlags(vk::FenceCreateFlagBits::eSignaled);

        for(uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            imageSemaphores_.push_back(device.createSemaphoreUnique(semaphoreInfo));
            renderSemaphores_.push_back(device.createSemaphoreUnique(semaphoreInfo));
            inFlightFrames_.push_back(device.createFenceUnique(fenceInfo));
        }
    }

    void updateUniformBuffer(const float wOverH, uint32_t imageIndex)
    {
        using Clock = std::chrono::high_resolution_clock;

        const static auto start = Clock::now();

        const float durationS = std::chrono::duration_cast<std::chrono::microseconds>(
            Clock::now() - start).count() / 1e6f;

        Mat4 proj = Trans4::perspective(
            agz::math::deg2rad(60.0f), wOverH, 0.1f, 100.0f);
        proj[1][1] *= -1;

        const Mat4 projViewModel =
            proj
          * Trans4::look_at({ 0, 0, -1.5f }, { 0, 0, 0 }, { 0, 1, 0 })
          * Trans4::rotate_y(durationS);

        const UniformBufferObject ubData = { projViewModel };

        auto &ub = uniformBuffers_[imageIndex];
        void *mappedData = ub.map();
        std::memcpy(mappedData, &ubData, sizeof(ubData));
        ub.unmap();
    }

    void preRecreateSwapchain()
    {
        device_.waitIdle();

        cmdBuffers_.clear();
        descSets_.clear();
        descPool_.reset();
        uniformBuffers_.clear();
        framebuffers_.clear();
        pipeline_.reset();
        pipelineLayout_.reset();
        descSetLayout_.reset();
        renderpass_.reset();
        cmdPool_.reset();
    }

    void postRecreateSwapchain(const agz::vlab::Window &window)
    {
        initCmdPool(window);
        initRenderpass(window);
        initGraphicsPipeline(window);
        initFramebuffer(window);
        initUniformBuffers(window);
        initDescriptorPool(window);
        initDescriptorSets(window);
        initCmdBuffers(window);
    }

public:

    explicit StagingBufferPipeline(agz::vlab::Window &window)
    {
        device_ = window.getDevice();

        initCmdPool(window);
        initRenderpass(window);
        initShaders(window.getDevice());
        initGraphicsPipeline(window);
        initFramebuffer(window);
        initVertexIndexBuffer(window);
        initUniformBuffers(window);
        initDescriptorPool(window);
        initDescriptorSets(window);
        initCmdBuffers(window);
        initSync(window);

        auto preHandler = std::make_shared<agz::event::functional_receiver_t<
            agz::vlab::WindowPreRecreateSwapchainEvent>>(
                [&](const agz::vlab::WindowPreRecreateSwapchainEvent&)
        {
            preRecreateSwapchain();
        });

        auto postHandler = std::make_shared<agz::event::functional_receiver_t<
            agz::vlab::WindowPostRecreateSwapchainEvent>>(
                [&](const agz::vlab::WindowPostRecreateSwapchainEvent&)
        {
            postRecreateSwapchain(window);
        });

        window.attach<agz::vlab::WindowPreRecreateSwapchainEvent>(preHandler);
        window.attach<agz::vlab::WindowPostRecreateSwapchainEvent>(postHandler);
    }

    ~StagingBufferPipeline()
    {
        imageSemaphores_.clear();
        renderSemaphores_.clear();
        inFlightFrames_.clear();
        imageInFlight_.clear();

        cmdBuffers_.clear();

        descSets_.clear();
        descPool_.reset();
        uniformBuffers_.clear();

        vertexBuffer_.reset();
        indexBuffer_.reset();

        allocator_.reset();

        framebuffers_.clear();

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
        vk::Fence frameFence[] = { inFlightFrames_[currentFrame_].get() };
        (void)device_.waitForFences(1, frameFence, true, UINT64_MAX);

        const auto nextImageResult = window.acquireNextImage(
            UINT64_MAX, imageSemaphores_[currentFrame_].get(), nullptr);
        if(nextImageResult.result == vk::Result::eErrorOutOfDateKHR)
            window.recreateSwapchain();
        const uint32_t imageIndex = nextImageResult.value;

        if(imageInFlight_[imageIndex])
        {
            device_.waitForFences(
                1, &imageInFlight_[imageIndex], true, UINT64_MAX);
        }
        imageInFlight_[imageIndex] = inFlightFrames_[currentFrame_].get();

        updateUniformBuffer(window.getSwapchainAspectRatio(), imageIndex);

        vk::Semaphore waitSemaphores[] = {
            imageSemaphores_[currentFrame_].get()
        };

        vk::PipelineStageFlags waitStages[] = {
            vk::PipelineStageFlagBits::eColorAttachmentOutput
        };

        vk::Semaphore signalSemaphores[] = {
            renderSemaphores_[currentFrame_].get()
        };

        vk::SubmitInfo submitInfo;
        submitInfo
            .setWaitSemaphoreCount(1)
            .setPWaitSemaphores(waitSemaphores)
            .setPWaitDstStageMask(waitStages)
            .setCommandBufferCount(1)
            .setPCommandBuffers(&cmdBuffers_[imageIndex])
            .setSignalSemaphoreCount(1)
            .setPSignalSemaphores(signalSemaphores);

        (void)device_.resetFences(1, frameFence);
        (void)window.getGraphicsQueue().submit(
            1, &submitInfo, inFlightFrames_[currentFrame_].get());

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

#include <agz/utility/time.h>

void run()
{
    agz::vlab::ValidationLayerManager layers;
    layers.add("VK_LAYER_KHRONOS_validation");

    agz::vlab::Window window;
    window.Initialize(agz::vlab::WindowDesc()
        .setSize(640, 480)
        .setTitle("AirGuanZ's Vulkan Lab: 04.staging buffer")
        .setDebugMessage(true)
        .setLayers(&layers)
        .setResizable(true));

    window.getDebugMsgMgr()->enableStdErrOutput(
        agz::vlab::DebugMsgLevel::Verbose);

    StagingBufferPipeline pipeline(window);

    agz::time::fps_counter_t fps;
    fps.restart();

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
