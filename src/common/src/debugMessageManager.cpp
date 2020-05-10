#include <iostream>

#include <agz/vlab/debugMessageManager.h>

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

DebugMessageManager::DebugMessageManager(VkInstance instance)
{
    vkInstance_  = instance;
    vkMessenger_ = nullptr;

    VkDebugUtilsMessengerCreateInfoEXT info = _createInfo();

    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkCreateDebugUtilsMessengerEXT");
    if(!func)
    {
        throw std::runtime_error(
            "failed to get proc addr: vkCreateDebugUtilsMessengerEXT");
    }

    if(func(instance, &info, nullptr, &vkMessenger_) != VK_SUCCESS)
        throw std::runtime_error("failed to create debug utils messenger");

    printToStdErrLevel_ = Level::Infomation;
    printToStdErr_.set_class_instance(this);
    printToStdErr_.set_mem_func(&DebugMessageManager::printToStdErrImpl);
}

DebugMessageManager::~DebugMessageManager()
{
    if(vkInstance_ && vkMessenger_)
    {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            vkInstance_, "vkDestroyDebugUtilsMessengerEXT");
        if(func)
            func(vkInstance_, vkMessenger_, nullptr);
    }
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

VkDebugUtilsMessengerCreateInfoEXT DebugMessageManager::_createInfo() noexcept
{
    VkDebugUtilsMessengerCreateInfoEXT info = {};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                         | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                         | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                     | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                     | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.pUserData       = this;
    info.pfnUserCallback = vkDebugCallback;
    return info;
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
