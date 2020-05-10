#include <iostream>

#include <GLFW/glfw3.h>

#include <agz/vlab/debugMessageManager.h>
#include <agz/vlab/window.h>

AGZ_VULKAN_LAB_BEGIN

struct WindowImplData
{
    GLFWwindow *glfwWindow = nullptr;

    int framebufferWidth  = 0;
    int framebufferHeight = 0;

    VkInstance instance = nullptr;

    std::unique_ptr<DebugMessageManager> debugMsgMgr;

    VkPhysicalDevice physicalDevice = nullptr;

    VkDevice logicalDevice = nullptr;
};

namespace
{
    int glfwRefCounter = 0;

    VKAPI_ATTR VkBool32 VKAPI_CALL vkDebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
        VkDebugUtilsMessageTypeFlagsEXT             type,
        const VkDebugUtilsMessengerCallbackDataEXT *callbackData,
        void *userData)
    {
        std::cerr << callbackData->pMessage << std::endl;
        return VK_FALSE;
    }

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo() noexcept
    {
        VkDebugUtilsMessengerCreateInfoEXT info = {};
        info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                             | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                             | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                         | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                         | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        info.pfnUserCallback = vkDebugCallback;
        return info;
    }

    VkInstance createVkInstance(const WindowDesc &desc)
    {
        // layers

        if(desc.layers && !desc.layers->isAllSupported())
            throw std::runtime_error("validation layer(s) unsupported");

        // vk app info

        VkApplicationInfo appInfo;
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.apiVersion = VK_API_VERSION_1_1;
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pApplicationName = desc.title.c_str();
        appInfo.pEngineName = desc.title.c_str();
        appInfo.pNext = nullptr;

        // inst info

        VkInstanceCreateInfo instInfo;
        instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instInfo.flags = 0;
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

        VkDebugUtilsMessengerCreateInfoEXT debugInfo;

        if(desc.enableDebugMessage)
        {
            debugInfo = debugCreateInfo();
            instInfo.pNext = &debugInfo;
        }
        else
            instInfo.pNext = nullptr;

        // extensions

        ExtensionManager extsMgr;

        if(desc.exts)
            extsMgr = *desc.exts;

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

        VkInstance inst;
        if(vkCreateInstance(&instInfo, nullptr, &inst) != VK_SUCCESS)
            throw std::runtime_error("failed to create vk instance");
        return inst;
    }

    struct QueueFamilyIndices
    {
        std::optional<uint32_t> graphics;

        bool isAvailable() const noexcept
        {
            return graphics.has_value();
        }
    };

    QueueFamilyIndices getQueueFamilyIndices(VkPhysicalDevice physicalDevice)
    {
        uint32_t familyCount;
        vkGetPhysicalDeviceQueueFamilyProperties(
            physicalDevice, &familyCount, nullptr);

        std::vector<VkQueueFamilyProperties> families(familyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(
            physicalDevice, &familyCount, families.data());

        QueueFamilyIndices ret;
        for(uint32_t i = 0; i < familyCount; ++i)
        {
            if(families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                ret.graphics = i;

                if(ret.isAvailable())
                    break;
            }
        }

        return ret;
    }

    VkPhysicalDevice pickVkPhysicalDevice(VkInstance instance)
    {
        uint32_t deviceCount;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if(!deviceCount)
            throw std::runtime_error("failed to find gpu with vulkan support");

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        for(auto device : devices)
        {
            VkPhysicalDeviceProperties property;
            vkGetPhysicalDeviceProperties(device, &property);

            if(!getQueueFamilyIndices(device).isAvailable())
                continue;

            if(property.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ||
               property.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
                return device;
        }

        throw std::runtime_error("no gpu found in physical devices");
    }

    VkDevice createLogicalDevice(VkPhysicalDevice physicalDevice)
    {
        const auto queueFamilyIndices = getQueueFamilyIndices(physicalDevice);
        assert(queueFamilyIndices.isAvailable());

        // TODO

        return nullptr;
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

WindowDesc &WindowDesc::setLayers(ValidationLayerManager *layers) noexcept
{
    this->layers = layers;
    return *this;
}

WindowDesc &WindowDesc::setExtensions(ExtensionManager *exts) noexcept
{
    this->exts = exts;
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

    // create glfw window

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, desc.resizable ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_MAXIMIZED, desc.maximized ? GLFW_TRUE : GLFW_FALSE);

    const auto monitor = desc.fullscreen ? glfwGetPrimaryMonitor() : nullptr;

    data_->glfwWindow = glfwCreateWindow(
        desc.width, desc.height, desc.title.c_str(), monitor, nullptr);
    if(!data_->glfwWindow)
        throw std::runtime_error("failed to create glfw window");
    misc::scope_guard_t windowGuard([&]
    {
        glfwDestroyWindow(data_->glfwWindow);
    });

    // instance

    data_->instance = createVkInstance(desc);

    misc::scope_guard_t instanceGuard([&]
    {
        vkDestroyInstance(data_->instance, nullptr);
    });

    // debug message manager

    if(desc.enableDebugMessage)
        data_->debugMsgMgr = std::make_unique<DebugMessageManager>(data_->instance);

    // framebuffer size

    glfwGetFramebufferSize(
        data_->glfwWindow, &data_->framebufferWidth, &data_->framebufferHeight);

    // physical device

    data_->physicalDevice = pickVkPhysicalDevice(data_->instance);

    instanceGuard.dismiss();
    windowGuard  .dismiss();
    dataGuard    .dismiss();
    glfwGuard    .dismiss();
}

bool Window::IsAvailable() const noexcept
{
    return data_ != nullptr;
}

void Window::Destroy()
{
    if(!data_)
        return;

    data_->debugMsgMgr.reset();

    vkDestroyInstance(data_->instance, nullptr);

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

DebugMessageManager *Window::getDebugMsgMgr() const noexcept
{
    return data_->debugMsgMgr.get();
}

Vec2i Window::getFramebufferSize() const noexcept
{
    return { data_->framebufferWidth, data_->framebufferHeight };
}

AGZ_VULKAN_LAB_END
