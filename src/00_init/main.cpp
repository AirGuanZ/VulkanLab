#include <iostream>

#include <agz/vlab/debugMessageManager.h>
#include <agz/vlab/window.h>

int run()
{
    agz::vlab::ValidationLayerManager layers;
    layers.add("VK_LAYER_KHRONOS_validation");

    agz::vlab::WindowDesc windowDesc;
    windowDesc
        .setSize(640, 480)
        .setTitle("AGZ Vulkan Lab")
        .setDebugMessage(true)
        .setLayers(&layers)
        .setResizable(true);

    agz::vlab::Window window;
    window.Initialize(windowDesc);

    window.getDebugMsgMgr()->enableStdErrOutput(
        agz::vlab::DebugMessageManager::Level::Verbose);

    while(!window.getCloseFlag())
        window.doEvents();

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
