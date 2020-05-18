#pragma once

#include <agz/utility/event.h>

#include <agz/vlab/common.h>

AGZ_VULKAN_LAB_BEGIN

struct FramebufferResizeEvent
{
    int newWidth;
    int newHeight;
};

struct WindowCloseEvent { };

class WindowEventManager :
    public event::sender_t<FramebufferResizeEvent, WindowCloseEvent>
{
public:

    void send(const FramebufferResizeEvent &e) const
    {
        sender_t::send<FramebufferResizeEvent>(e);
    }

    void send(const WindowCloseEvent &e) const
    {
        sender_t::send<WindowCloseEvent>(e);
    }
};

AGZ_VULKAN_LAB_END
