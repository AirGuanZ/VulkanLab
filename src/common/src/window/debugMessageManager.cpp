#include <iostream>

#include <agz/vlab/window/debugMessageManager.h>

AGZ_VULKAN_LAB_BEGIN

namespace
{
    VKAPI_ATTR VkBool32 VKAPI_CALL vkDebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
        VkDebugUtilsMessageTypeFlagsEXT             type,
        const VkDebugUtilsMessengerCallbackDataEXT *callbackData,
        void                                       *userData)
    {
        auto mgr = static_cast<DebugMessageManager *>(userData);
        mgr->_message({ severity, type, callbackData });
        return VK_FALSE;
    }
}

DebugMessageManager::DebugMessageManager(vk::Instance instance, Level level)
{
    instance_  = instance;

    const auto &info = _createInfo(level);

    messenger_ = instance_.createDebugUtilsMessengerEXTUnique(info);

    printToStdErrLevel_ = level;
    printToStdErr_.set_class_instance(this);
    printToStdErr_.set_mem_func(&DebugMessageManager::printToStdErrImpl);
}

void DebugMessageManager::enableStdErrOutput(Level level)
{
    printToStdErrLevel_ = level;
    attach(&printToStdErr_);
}

void DebugMessageManager::disableStdErrOutput()
{
    detach(&printToStdErr_);
}

vk::DebugUtilsMessengerCreateInfoEXT DebugMessageManager::_createInfo(
        Level level) noexcept
{
    vk::DebugUtilsMessageSeverityFlagsEXT severity;
    switch(level)
    {
    case Level::Verbose:
        severity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError   |
                   vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                   vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo    |
                   vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose;
        break;
    case Level::Infomation:
        severity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError   |
                   vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                   vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo;
        break;
    case Level::Warning:
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
        {}, severity, type, vkDebugCallback, this);

}

void DebugMessageManager::_message(const DebugMessage &msg)
{
    sender_t::send(msg);
}

void DebugMessageManager::printToStdErrImpl(const DebugMessage &msg)
{
    if(shouldPrintToStdErr(msg.severity))
        std::cerr << "validation layer: " << msg.data->pMessage << std::endl;
}

bool DebugMessageManager::shouldPrintToStdErr(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity) const noexcept
{
    const int thisLevel = static_cast<int>(printToStdErrLevel_);

    int thatLevel = 3;
    switch(severity)
    {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        thatLevel = 0;
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        thatLevel = 1;
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        thatLevel = 2;
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        thatLevel = 3;
        break;
    default:
        break;
    }

    return thatLevel >= thisLevel;
}

AGZ_VULKAN_LAB_END
