#include <iostream>

#include <agz/vlab/window/graphicsDevice.h>
#include <agz/vlab/window/swapchain.h>
#include <agz/vlab/window/window.h>

#include <GLFW/glfw3.h>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

AGZ_VULKAN_LAB_BEGIN

struct WindowImplData
{
    // desc settings

    int swapchainImageCount = 2;
    bool clipObscuredPixels  = true;

    // others

    GLFWwindow *glfwWindow = nullptr;

    vk::UniqueInstance instance;

    std::unique_ptr<DebugMessageManager> debugMsgMgr;

    vk::UniqueSurfaceKHR surface;
    vk::PhysicalDevice physicalDevice;

    GraphicsDevice graphicsDevice;
    vk::Device     device; // not owner;

    vk::UniqueSwapchainKHR swapchain;

    vk::SurfaceFormatKHR swapchainFormat;
    vk::Extent2D         swapchainExtent;

    std::vector<vk::Image>           swapchainImages;
    std::vector<vk::UniqueImageView> swapchainImageViews;
};

namespace
{
    int glfwRefCounter = 0;

    std::unordered_map<GLFWwindow *, Window *> glfwWindowToWindow;

    void glfwFramebufferResizeCallback(GLFWwindow *window, int width, int height)
    {
        auto it = glfwWindowToWindow.find(window);
        if(it == glfwWindowToWindow.end())
            return;
        it->second->recreateSwapchain();
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL vkDebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
        VkDebugUtilsMessageTypeFlagsEXT             type,
        const VkDebugUtilsMessengerCallbackDataEXT *callbackData,
        void                                       *userData)
    {
        std::cerr << callbackData->pMessage << std::endl;
        return VK_FALSE;
    }

    vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo(
        DebugMessageManager::Level level) noexcept
    {
        vk::DebugUtilsMessageSeverityFlagsEXT severity;
        switch(level)
        {
        case DebugMessageManager::Level::Verbose:
            severity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError   |
                       vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                       vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo    |
                       vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose;
            break;
        case DebugMessageManager::Level::Infomation:
            severity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError   |
                       vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                       vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo;
            break;
        case DebugMessageManager::Level::Warning:
            severity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError   |
                       vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning;
            break;
        default:
            severity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
            break;
        }

        const auto type = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                          vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation|
                          vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;

        return vk::DebugUtilsMessengerCreateInfoEXT(
            {}, severity, type, vkDebugCallback);
    }

    vk::UniqueInstance createVkInstance(const WindowDesc &desc)
    {
        // layers

        if(desc.layers && !desc.layers->isAllSupported())
            throw std::runtime_error("validation layer(s) unsupported");

        // vk app info

        vk::ApplicationInfo appInfo(
            desc.title.c_str(),
            VK_MAKE_VERSION(1, 0, 0),
            desc.title.c_str(),
            VK_MAKE_VERSION(1, 0, 0),
            VK_API_VERSION_1_2);

        // inst info

        vk::InstanceCreateInfo instInfo;
        instInfo.flags            = {};
        instInfo.pApplicationInfo = &appInfo;

        // validation layers

        if(desc.layers)
        {
            if(!desc.layers->isAllSupported())
                throw std::runtime_error("validation layer(s) unsupported");

            const auto &layers = desc.layers->getLayers();

            instInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
            instInfo.ppEnabledLayerNames = layers.data();
        }
        else
        {
            instInfo.enabledLayerCount = 0;
            instInfo.ppEnabledLayerNames = nullptr;
        }

        // debug message

        vk::DebugUtilsMessengerCreateInfoEXT debugInfo;

        if(desc.enableDebugMessage)
        {
            debugInfo = debugCreateInfo(desc.debugMsgLevel);
            instInfo.pNext = &debugInfo;
        }
        else
            instInfo.pNext = nullptr;

        // extensions

        InstanceExtensionManager extsMgr;

        if(desc.instanceExtensions)
            extsMgr = *desc.instanceExtensions;

        if(desc.enableDebugMessage)
            extsMgr.add(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        uint32_t glfwExtCount = 0;
        const auto glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
        extsMgr.add(glfwExts, glfwExtCount);

        if(!extsMgr.isAllSupported())
            throw std::runtime_error("extension(s) unsupported");

        auto &exts = extsMgr.getExtensions();
        instInfo.enabledExtensionCount = static_cast<uint32_t>(exts.size());
        instInfo.ppEnabledExtensionNames = exts.data();

        // create instance

        auto inst = createInstanceUnique(instInfo);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(inst.get());

        return inst;
    }

    struct SwapchainDesc
    {
        vk::SurfaceCapabilitiesKHR capabilities             = {};
        vk::SurfaceFormatKHR       format                   = {};
        vk::PresentModeKHR         presentMode              = vk::PresentModeKHR::eFifo;
        uint32_t                   graphicsQueueFamilyIndex = 0;
        uint32_t                   presentQueueFamilyIndex  = 0;
        uint32_t                   imageCount               = 0;
        vk::Extent2D               extent                   = {};
        bool                       clipped                  = true;
    };

    vk::UniqueSwapchainKHR createVkSwapchain(
        vk::Device            device,
        vk::SurfaceKHR        surface,
        const SwapchainDesc &desc)
    {
        vk::SwapchainCreateInfoKHR info(
            {},
            surface,
            desc.imageCount,
            desc.format.format,
            desc.format.colorSpace,
            desc.extent,
            1,
            vk::ImageUsageFlagBits::eColorAttachment);

        const uint32_t queueFamilyIndices[2] = {
            desc.graphicsQueueFamilyIndex,
            desc.presentQueueFamilyIndex
        };

        if(desc.graphicsQueueFamilyIndex != desc.presentQueueFamilyIndex)
        {
            info.imageSharingMode      = vk::SharingMode::eConcurrent;
            info.queueFamilyIndexCount = 2;
            info.pQueueFamilyIndices   = queueFamilyIndices;
        }
        else
            info.imageSharingMode = vk::SharingMode::eExclusive;

        info.preTransform   = desc.capabilities.currentTransform;
        info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        info.presentMode    = desc.presentMode;
        info.clipped        = desc.clipped ? VK_TRUE : VK_FALSE;
        info.oldSwapchain   = nullptr;

        return device.createSwapchainKHRUnique(info);
    }

    std::vector<vk::UniqueImageView> createSwapchainImageViews(
        vk::Device device, vk::Format format,
        const std::vector<vk::Image> &imgs)
    {
        std::vector<vk::UniqueImageView> ret;

        for(auto img : imgs)
        {
            vk::ImageViewCreateInfo info = {};
            info.image                           = img;
            info.viewType                        = vk::ImageViewType::e2D;
            info.format                          = format;
            info.components.r                    = vk::ComponentSwizzle::eIdentity;
            info.components.g                    = vk::ComponentSwizzle::eIdentity;
            info.components.b                    = vk::ComponentSwizzle::eIdentity;
            info.components.a                    = vk::ComponentSwizzle::eIdentity;
            info.subresourceRange.aspectMask     = vk::ImageAspectFlagBits::eColor;
            info.subresourceRange.baseMipLevel   = 0;
            info.subresourceRange.levelCount     = 1;
            info.subresourceRange.baseArrayLayer = 0;
            info.subresourceRange.layerCount     = 1;

            ret.push_back(device.createImageViewUnique(info));
        }

        return ret;
    }
}

WindowDesc &WindowDesc::setSize(int width, int height) noexcept
{
    this->width  = width;
    this->height = height;
    return *this;
}

WindowDesc &WindowDesc::setWidth(int width) noexcept
{
    this->width = width;
    return *this;
}

WindowDesc &WindowDesc::setHeight(int height) noexcept
{
    this->height = height;
    return *this;
}

WindowDesc &WindowDesc::setTitle(std::string title) noexcept
{
    this->title = std::move(title);
    return *this;
}

WindowDesc &WindowDesc::setVSync(bool vsync) noexcept
{
    this->vsync = vsync;
    return *this;
}

WindowDesc &WindowDesc::setResizable(bool resizable) noexcept
{
    this->resizable = resizable;
    return *this;
}

WindowDesc &WindowDesc::setFullscreen(bool fullscreen) noexcept
{
    this->fullscreen = fullscreen;
    return *this;
}

WindowDesc &WindowDesc::setMaximized(bool maximized) noexcept
{
    this->maximized = maximized;
    return *this;
}

WindowDesc &WindowDesc::setDebugMessage(bool enabled) noexcept
{
    enableDebugMessage = enabled;
    return *this;
}

struct WindowDesc &WindowDesc::setDebugMessageLevel(
    DebugMessageManager::Level level) noexcept
{
    debugMsgLevel = level;
    return *this;
}

WindowDesc &WindowDesc::setLayers(ValidationLayerManager *layers) noexcept
{
    this->layers = layers;
    return *this;
}

WindowDesc &WindowDesc::setInstanceExtensions(InstanceExtensionManager *exts) noexcept
{
    instanceExtensions = exts;
    return *this;
}

WindowDesc &WindowDesc::setDeviceExtensions(DeviceExtensionManager *exts) noexcept
{
    deviceExtensions = exts;
    return *this;
}

WindowDesc &WindowDesc::setImageCount(uint32_t swapchainImageCount) noexcept
{
    this->swapchainImageCount = swapchainImageCount;
    return *this;
}

WindowDesc &WindowDesc::setObscuredPixels(bool enableClipping) noexcept
{
    clipObscuredPixels = enableClipping;
    return *this;
}

Window::~Window()
{
    Destroy();
}

void Window::Initialize(const WindowDesc &desc)
{
    // glfw initialization

    if(!glfwRefCounter)
    {
        if(glfwInit() != GLFW_TRUE)
            throw std::runtime_error("failed to initialize glfw");

        vk::DynamicLoader dl;
        PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = dl.getProcAddress
            <PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
    }

    ++glfwRefCounter;
    misc::scope_guard_t glfwGuard([&]
    {
        if(!--glfwRefCounter)
            glfwTerminate();
    });

    // create WindowImplData

    data_ = new WindowImplData;
    misc::scope_guard_t dataGuard([&]
    {
        delete data_;
        data_ = nullptr;
    });
    data_->swapchainImageCount = desc.swapchainImageCount;
    data_->clipObscuredPixels   = desc.clipObscuredPixels;

    // create glfw window

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, desc.resizable ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_MAXIMIZED, desc.maximized ? GLFW_TRUE : GLFW_FALSE);

    const auto monitor = desc.fullscreen ? glfwGetPrimaryMonitor() : nullptr;

    data_->glfwWindow = glfwCreateWindow(
        desc.width, desc.height, desc.title.c_str(), monitor, nullptr);
    if(!data_->glfwWindow)
        throw std::runtime_error("failed to create glfw window");
    glfwWindowToWindow[data_->glfwWindow] = this;
    misc::scope_guard_t windowGuard([&]
    {
        glfwWindowToWindow.erase(data_->glfwWindow);
        glfwDestroyWindow(data_->glfwWindow);
    });

    glfwSetFramebufferSizeCallback(
        data_->glfwWindow, glfwFramebufferResizeCallback);

    // instance

    data_->instance = createVkInstance(desc);

    misc::scope_guard_t instanceGuard([&]
    {
        data_->instance.reset();
    });

    // debug message manager

    if(desc.enableDebugMessage)
    {
        data_->debugMsgMgr = std::make_unique<DebugMessageManager>(
            data_->instance.get(), desc.debugMsgLevel);
    }
    misc::scope_guard_t debugGuard([&]
    {
        data_->debugMsgMgr.reset();
    });

    // framebuffer size

    int framebufferWidth, framebufferHeight;
    glfwGetFramebufferSize(
        data_->glfwWindow, &framebufferWidth, &framebufferHeight);

    // window surface

    VkSurfaceKHR surface;
    if(auto rt = glfwCreateWindowSurface(
        data_->instance.get(), data_->glfwWindow, nullptr, &surface);
        rt != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create window surface. VkResult: "
                                + std::to_string(rt));
    }

    data_->surface = vk::UniqueSurfaceKHR(surface, data_->instance.get());
    misc::scope_guard_t surfaceGuard([&]
    {
        data_->surface.reset();
    });

    // physical device

    const auto graphicsPhysicalDevices = getAllPhysicalDevices(
        data_->instance.get(), [&](vk::PhysicalDevice physicalDevice)
    {
        return IsGraphicsPhysicalDevice(
            physicalDevice, data_->surface.get(), desc.deviceExtensions);
    });

    if(graphicsPhysicalDevices.empty())
        throw std::runtime_error("graphics physical device not found");

    data_->physicalDevice = graphicsPhysicalDevices[0];

    // graphics device & queues

    data_->graphicsDevice.Initialize(
        data_->physicalDevice, data_->surface.get(), desc.deviceExtensions);
    data_->device = data_->graphicsDevice.device();
    misc::scope_guard_t deviceGuard([&]
    {
        data_->graphicsDevice.Destroy();
        data_->device = nullptr;
    });

    // swapchain

    const auto swapchainProperty = querySwapchainProperty(
        data_->physicalDevice, data_->surface.get());
    const auto scFormat = swapchainProperty.chooseFormat(
        { { vk::Format::eB8G8R8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear } });
    const auto scPresentMode = swapchainProperty.choosePresentMode(
        { vk::PresentModeKHR::eMailbox });
    const auto scExtent = swapchainProperty.chooseExtent(
        { static_cast<uint32_t>(framebufferWidth),
          static_cast<uint32_t>(framebufferHeight) });
    const uint32_t scGraphicsQueueFamilyIndex =
        data_->graphicsDevice.graphicsQueueFamilyIndex();
    const uint32_t scPresentQueueFamilyIndex =
        data_->graphicsDevice.presentQueueFamilyIndex();

    SwapchainDesc scDesc = {};
    scDesc.capabilities             = swapchainProperty.capabilities;
    scDesc.format                   = scFormat;
    scDesc.presentMode              = scPresentMode;
    scDesc.graphicsQueueFamilyIndex = scGraphicsQueueFamilyIndex;
    scDesc.presentQueueFamilyIndex  = scPresentQueueFamilyIndex;
    scDesc.imageCount               = desc.swapchainImageCount;
    scDesc.extent                   = scExtent;
    scDesc.clipped                  = desc.clipObscuredPixels;

    data_->swapchain = createVkSwapchain(
        data_->device, data_->surface.get(), scDesc);
    data_->swapchainExtent = scDesc.extent;
    data_->swapchainFormat = scDesc.format;
    misc::scope_guard_t swapchainGuard([&]
    {
        data_->swapchain.reset();
    });

    // swapchain images

    data_->swapchainImages = data_->device.getSwapchainImagesKHR(
        data_->swapchain.get());

    // swapchain image views

    data_->swapchainImageViews = createSwapchainImageViews(
        data_->device, data_->swapchainFormat.format, data_->swapchainImages);
    misc::scope_guard_t imgViewGuard([&]
    {
        data_->swapchainImageViews.clear();
    });

    imgViewGuard  .dismiss();
    swapchainGuard.dismiss();
    deviceGuard   .dismiss();
    surfaceGuard  .dismiss();
    debugGuard    .dismiss();
    instanceGuard .dismiss();
    windowGuard   .dismiss();
    dataGuard     .dismiss();
    glfwGuard     .dismiss();
}

bool Window::IsAvailable() const noexcept
{
    return data_ != nullptr;
}

void Window::Destroy()
{
    if(!data_)
        return;

    data_->swapchainImageViews.clear();

    data_->swapchain.reset();

    data_->debugMsgMgr.reset();

    data_->surface.reset();

    data_->graphicsDevice.Destroy();

    data_->instance.reset();

    glfwWindowToWindow.erase(data_->glfwWindow);
    glfwDestroyWindow(data_->glfwWindow);

    delete data_;

    if(!--glfwRefCounter)
        glfwTerminate();
}

void Window::doEvents()
{
    glfwPollEvents();
}

bool Window::getCloseFlag() const
{
    return glfwWindowShouldClose(data_->glfwWindow);
}

void Window::setCloseFlag(bool close)
{
    glfwSetWindowShouldClose(data_->glfwWindow, close);
}

vk::Instance Window::getInstance() const noexcept
{
    return data_->instance.get();
}

vk::PhysicalDevice Window::getPhysicalDevice() const noexcept
{
    return data_->physicalDevice;
}

DebugMessageManager *Window::getDebugMsgMgr() const
{
    if(!data_->debugMsgMgr)
        throw std::runtime_error("debug message is not enabled");
    return data_->debugMsgMgr.get();
}

GraphicsDevice &Window::getGraphicsDevice() const noexcept
{
    return data_->graphicsDevice;
}

vk::Queue Window::getGraphicsQueue() const noexcept
{
    return data_->graphicsDevice.graphicsQueue();
}

vk::Queue Window::getPresentQueue() const noexcept
{
    return data_->graphicsDevice.presentQueue();
}

vk::Device Window::getDevice() const noexcept
{
    return data_->graphicsDevice.device();
}

vk::SwapchainKHR Window::getSwapchain() const noexcept
{
    return data_->swapchain.get();
}

vk::Format Window::getSwapchainFormat() const noexcept
{
    return data_->swapchainFormat.format;
}

vk::Extent2D Window::getSwapchainExtent() const noexcept
{
    return data_->swapchainExtent;
}

float Window::getSwapchainAspectRatio() const noexcept
{
    return static_cast<float>(data_->swapchainExtent.width)
         / static_cast<float>(data_->swapchainExtent.height);
}

const std::vector<vk::UniqueImageView> &
    Window::getSwapchainImageViews() const noexcept
{
    return data_->swapchainImageViews;
}

uint32_t Window::getSwapchainImageCount() const noexcept
{
    return static_cast<uint32_t>(data_->swapchainImageViews.size());
}

vk::ResultValue<uint32_t> Window::acquireNextImage(
    uint64_t timeout, vk::Semaphore semaphore, vk::Fence fence) const
{
    return data_->device.acquireNextImageKHR(
        data_->swapchain.get(), timeout, semaphore, fence);
}

void Window::recreateSwapchain()
{
    int framebufferWidth, framebufferHeight;
    glfwGetFramebufferSize(
        data_->glfwWindow, &framebufferWidth, &framebufferHeight);
    while(framebufferWidth == 0 || framebufferHeight == 0)
    {
        glfwGetFramebufferSize(
            data_->glfwWindow, &framebufferWidth, &framebufferHeight);
        glfwWaitEvents();
    }

    // pre recreation

    send(WindowPreRecreateSwapchainEvent{});

    data_->device.waitIdle();

    data_->swapchainImageViews.clear();
    data_->swapchainImages.clear();
    data_->swapchain.reset();

    const auto swapchainProperty = querySwapchainProperty(
        data_->physicalDevice, data_->surface.get());
    const auto scFormat = swapchainProperty.chooseFormat(
        { { vk::Format::eB8G8R8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear } });
    const auto scPresentMode = swapchainProperty.choosePresentMode(
        { vk::PresentModeKHR::eMailbox });
    const auto scExtent = swapchainProperty.chooseExtent(
        { static_cast<uint32_t>(framebufferWidth),
          static_cast<uint32_t>(framebufferHeight) });
    const uint32_t scGraphicsQueueFamilyIndex =
        data_->graphicsDevice.graphicsQueueFamilyIndex();
    const uint32_t scPresentQueueFamilyIndex =
        data_->graphicsDevice.presentQueueFamilyIndex();
    
    SwapchainDesc scDesc;
    scDesc.capabilities             = swapchainProperty.capabilities;
    scDesc.format                   = scFormat;
    scDesc.presentMode              = scPresentMode;
    scDesc.graphicsQueueFamilyIndex = scGraphicsQueueFamilyIndex;
    scDesc.presentQueueFamilyIndex  = scPresentQueueFamilyIndex;
    scDesc.imageCount               = data_->swapchainImageCount;
    scDesc.extent                   = scExtent;
    scDesc.clipped                  = data_->clipObscuredPixels;

    data_->swapchain = createVkSwapchain(
        data_->device, data_->surface.get(), scDesc);
    data_->swapchainExtent = scDesc.extent;
    data_->swapchainFormat = scDesc.format;

    // swapchain images

    data_->swapchainImages = data_->device.getSwapchainImagesKHR(
        data_->swapchain.get());

    // swapchain image views

    data_->swapchainImageViews = createSwapchainImageViews(
        data_->device, data_->swapchainFormat.format, data_->swapchainImages);

    // post recreation

    send(WindowPostRecreateSwapchainEvent{
        static_cast<int>(scExtent.width),
        static_cast<int>(scExtent.height)
    });
}

AGZ_VULKAN_LAB_END
