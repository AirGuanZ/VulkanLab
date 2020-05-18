#include <iostream>

#include <agz/vlab/vlab.h>

const char *VERTEX_SHADER_SOURCE = R"___(
#version 450

layout(location = 0) out vec3 fragColor;

vec2 positions[3] = vec2[](
    vec2(0.0, -0.5),
    vec2(0.5, 0.5),
    vec2(-0.5, 0.5)
);

vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

void main()
{
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragColor = colors[gl_VertexIndex];
}
)___";

const char *FRAGMENT_SHADER_SOURCE = R"___(
#version 450

layout(location = 0) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fragColor, 1.0);
}
)___";

class TrianglePipeline : public agz::misc::uncopyable_t
{
    vk::Device device_;

    vk::UniqueShaderModule vertShader_;
    vk::UniqueShaderModule fragShader_;
    vk::PipelineShaderStageCreateInfo shaderStage_[2];

    vk::UniqueRenderPass renderpass_;

    vk::UniquePipelineLayout layout_;
    vk::UniquePipeline       pipeline_;

    std::vector<vk::UniqueFramebuffer> framebuffers_;

    vk::UniqueCommandPool cmdPool_;
    std::vector<vk::CommandBuffer> cmdBuffers_;

    vk::UniqueSemaphore imageSemaphore_;
    vk::UniqueSemaphore renderSemaphore_;

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
        vk::PipelineVertexInputStateCreateInfo vertexInputInfo;

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
            .setCullMode(vk::CullModeFlagBits::eBack)
            .setFrontFace(vk::FrontFace::eClockwise);

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

        vk::PipelineLayoutCreateInfo layoutInfo;
        layout_ = window.getDevice().createPipelineLayoutUnique(layoutInfo);

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
            .setLayout(layout_.get())
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

    void initCmdBuffers(const agz::vlab::Window &window)
    {
        vk::CommandPoolCreateInfo poolInfo;
        poolInfo.setQueueFamilyIndex(
            window.getGraphicsDevice().graphicsQueueFamilyIndex());

        cmdPool_ = window.getDevice().createCommandPoolUnique(poolInfo);

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
            cb.draw(3, 1, 0, 0);

            cb.endRenderPass();
            cb.end();
        }
    }

    void initSync(vk::Device device)
    {
        vk::SemaphoreCreateInfo info;
        imageSemaphore_ = device.createSemaphoreUnique(info);
        renderSemaphore_ = device.createSemaphoreUnique(info);
    }

public:

    explicit TrianglePipeline(const agz::vlab::Window &window)
    {
        device_ = window.getDevice();

        initRenderpass(window);
        initShaders(window.getDevice());
        initGraphicsPipeline(window);
        initFramebuffer(window);
        initCmdBuffers(window);
        initSync(window.getDevice());
    }

    ~TrianglePipeline()
    {
        imageSemaphore_.reset();
        renderSemaphore_.reset();

        cmdBuffers_.clear();
        cmdPool_.reset();

        framebuffers_.clear();

        pipeline_.reset();
        layout_.reset();

        vertShader_.reset();
        fragShader_.reset();

        renderpass_.reset();
    }

    void renderFrame(agz::vlab::Window &window)
    {
        const auto imageIndex = window.acquireNextImage(
            UINT64_MAX, imageSemaphore_.get(), nullptr).value;

        vk::Semaphore waitSemaphores[] = {
            imageSemaphore_.get()
        };
        vk::PipelineStageFlags waitStages[] = {
            vk::PipelineStageFlagBits::eColorAttachmentOutput
        };

        vk::Semaphore signalSemaphores[] = {
            renderSemaphore_.get()
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

        (void)window.getGraphicsQueue().submit(
            1, &submitInfo, nullptr);

        vk::SwapchainKHR swapchains[] = { window.getSwapchain() };

        vk::PresentInfoKHR presentInfo;
        presentInfo
            .setWaitSemaphoreCount(1)
            .setPWaitSemaphores(signalSemaphores)
            .setSwapchainCount(1)
            .setPSwapchains(swapchains)
            .setPImageIndices(&imageIndex);

        (void)window.getPresentQueue().presentKHR(presentInfo);

        window.getGraphicsQueue().waitIdle();
    }

    void waitIdle()
    {
        device_.waitIdle();
    }
};

int run()
{
    agz::vlab::ValidationLayerManager layers;
    layers.add("VK_LAYER_KHRONOS_validation");

    agz::vlab::Window window;
    window.Initialize(agz::vlab::WindowDesc()
        .setSize(640, 480)
        .setTitle("AirGuanZ's Vulkan Lab: 01.triangle")
        .setDebugMessage(true)
        .setLayers(&layers)
        .setResizable(false));

    window.getDebugMsgMgr()->enableStdErrOutput(
        agz::vlab::DebugMsgLevel::Verbose);

    TrianglePipeline pipeline(window);

    while(!window.getCloseFlag())
    {
        window.doEvents();
        pipeline.renderFrame(window);
    }

    pipeline.waitIdle();
    return 0;
}

int main()
{
    try
    {
        return run();
    }
    catch(const std::exception &err)
    {
        std::cout << err.what() << std::endl;
    }
}
