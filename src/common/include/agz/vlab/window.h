#pragma once

#include <agz/vlab/extensionManager.h>
#include <agz/vlab/layerManager.h>
#include <agz/vlab/windowEvent.h>

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

    const ValidationLayerManager *layers = nullptr;

    const ExtensionManager *exts = nullptr;

    WindowDesc &setSize(int width, int height) noexcept;

    WindowDesc &setWidth(int width) noexcept;

    WindowDesc &setHeight(int height) noexcept;

    WindowDesc &setTitle(std::string title) noexcept;

    WindowDesc &setVSync(bool vsync) noexcept;

    WindowDesc &setResizable(bool resizable) noexcept;

    WindowDesc &setFullscreen(bool fullscreen) noexcept;

    WindowDesc &setMaximized(bool maximized) noexcept;

    WindowDesc &setDebugMessage(bool enabled) noexcept;

    WindowDesc &setLayers(ValidationLayerManager *layers) noexcept;

    WindowDesc &setExtensions(ExtensionManager *exts) noexcept;
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

    DebugMessageManager *getDebugMsgMgr() const noexcept;

    Vec2i getFramebufferSize() const noexcept;

private:

    WindowImplData *data_ = nullptr;
};

AGZ_VULKAN_LAB_END
