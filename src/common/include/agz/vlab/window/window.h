#pragma once

#include <agz/vlab/window/debugMessageManager.h>
#include <agz/vlab/window/extensionManager.h>
#include <agz/vlab/window/graphicsDevice.h>
#include <agz/vlab/window/layerManager.h>
#include <agz/vlab/window/windowEvent.h>

AGZ_VULKAN_LAB_BEGIN

struct WindowDesc
{
    int width  = 640;
    int height = 480;

    std::string title;

    bool vsync      = true;
    bool resizable  = true;
    bool fullscreen = false;
    bool maximized  = false;

    bool enableDebugMessage = true;
    DebugMessageManager::Level debugMsgLevel = DebugMessageManager::Level::Warning;

    const ValidationLayerManager *layers = nullptr;

    const InstanceExtensionManager *instanceExtensions = nullptr;
    const DeviceExtensionManager   *deviceExtensions   = nullptr;

    uint32_t swapchainImageCount = 2;

    bool clipObscuredPixels = true;

    WindowDesc &setSize              (int width, int height)            noexcept;
    WindowDesc &setWidth             (int width)                        noexcept;
    WindowDesc &setHeight            (int height)                       noexcept;
    WindowDesc &setTitle             (std::string title)                noexcept;
    WindowDesc &setVSync             (bool vsync)                       noexcept;
    WindowDesc &setResizable         (bool resizable)                   noexcept;
    WindowDesc &setFullscreen        (bool fullscreen)                  noexcept;
    WindowDesc &setMaximized         (bool maximized)                   noexcept;
    WindowDesc &setDebugMessage      (bool enabled)                     noexcept;
    WindowDesc &setDebugMessageLevel (DebugMessageManager::Level level) noexcept;
    WindowDesc &setLayers            (ValidationLayerManager *layers)   noexcept;
    WindowDesc &setInstanceExtensions(InstanceExtensionManager *exts)   noexcept;
    WindowDesc &setDeviceExtensions  (DeviceExtensionManager *exts)     noexcept;
    WindowDesc &setImageCount        (uint32_t swapchainImageCount)     noexcept;
    WindowDesc &setObscuredPixels    (bool enableClipping)              noexcept;
};

struct WindowImplData;

class Window : public WindowEventManager
{
public:

    ~Window();

    void Initialize(const WindowDesc &desc);

    bool IsAvailable() const noexcept;

    void Destroy();

    void doEvents();

    bool getCloseFlag() const;

    void setCloseFlag(bool close);

    DebugMessageManager *getDebugMsgMgr() const;

    GraphicsDevice &getGraphicsDevice() const noexcept;

    vk::Queue getGraphicsQueue() const noexcept;

    vk::Queue getPresentQueue() const noexcept;

    vk::Device getDevice() const noexcept;

    vk::SwapchainKHR getSwapchain() const noexcept;

    vk::Format getSwapchainFormat() const noexcept;

    vk::Extent2D getSwapchainExtent() const noexcept;

    const std::vector<vk::UniqueImageView> &
        getSwapchainImageViews() const noexcept;

    uint32_t getSwapchainImageCount() const noexcept;

    vk::ResultValue<uint32_t> acquireNextImage(
        uint64_t timeout, vk::Semaphore semaphore, vk::Fence fence) const;

    void recreateSwapchain();

private:

    WindowImplData *data_ = nullptr;
};

AGZ_VULKAN_LAB_END
