#pragma once

#include <agz/utility/event.h>

#include <agz/vlab/common.h>

AGZ_VULKAN_LAB_BEGIN

struct WindowPreRecreateSwapchainEvent
{

};

struct WindowPostRecreateSwapchainEvent
{
    int width;
    int height;
};

struct WindowCloseEvent { };

class WindowEventManager :
    public event::sender_t<
                WindowPreRecreateSwapchainEvent,
                WindowPostRecreateSwapchainEvent,
                WindowCloseEvent>
{
public:

    void send(const WindowPreRecreateSwapchainEvent &e) const
    {
        sender_t::send<WindowPreRecreateSwapchainEvent>(e);
    }

    void send(const WindowPostRecreateSwapchainEvent &e) const
    {
        sender_t::send<WindowPostRecreateSwapchainEvent >(e);
    }

    void send(const WindowCloseEvent &e) const
    {
        sender_t::send<WindowCloseEvent>(e);
    }
};

AGZ_VULKAN_LAB_END
