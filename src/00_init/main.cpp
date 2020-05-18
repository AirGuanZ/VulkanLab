#include <iostream>

#include <agz/vlab/vlab.h>

int run()
{
    agz::vlab::ValidationLayerManager layers;
    layers.add("VK_LAYER_KHRONOS_validation");

    agz::vlab::Window window;
    window.Initialize(agz::vlab::WindowDesc()
        .setSize(640, 480)
        .setTitle("AirGuanZ's Vulkan Lab: 00.init")
        .setDebugMessage(true)
        .setLayers(&layers)
        .setResizable(true));

    window.getDebugMsgMgr()->enableStdErrOutput(
        agz::vlab::DebugMsgLevel::Verbose);

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
