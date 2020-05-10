#pragma once

#include <vulkan/vulkan.h>

#include <agz/utility/event.h>
#include <agz/utility/misc.h>

#include <agz/vlab/common.h>

AGZ_VULKAN_LAB_BEGIN

struct DebugMessage
{
    VkDebugUtilsMessageSeverityFlagBitsEXT      severity;
    VkDebugUtilsMessageTypeFlagsEXT             type;
    const VkDebugUtilsMessengerCallbackDataEXT *data;
};

class DebugMessageManager : public event::sender_t<DebugMessage>
{
public:

    enum class Level
    {
        Verbose    = 0,
        Infomation = 1,
        Warning    = 2,
        Error      = 3
    };

    explicit DebugMessageManager(VkInstance instance);

    ~DebugMessageManager();

    void enableStdErrOutput(Level level);

    void disableStdErrOutput();

    VkDebugUtilsMessengerCreateInfoEXT _createInfo() noexcept;

    void _message(const DebugMessage &msg);

private:

    using PrintToStdErr = event::class_receiver_t<
                            DebugMessage, DebugMessageManager>;

    void printToStdErrImpl(const DebugMessage &msg);

    bool shouldPrintToStdErr(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity) const noexcept;

    Level printToStdErrLevel_ = Level::Verbose;

    PrintToStdErr printToStdErr_;

    VkInstance vkInstance_;
    VkDebugUtilsMessengerEXT vkMessenger_;
};

AGZ_VULKAN_LAB_END
