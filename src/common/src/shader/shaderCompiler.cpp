#include <agz/vlab/shader/shaderCompiler.h>

#include <shaderc/shaderc.hpp>

AGZ_VULKAN_LAB_BEGIN

std::vector<uint32_t> compileGLSLToSPIRV(
    const std::string                        &source,
    const std::string                        &sourceName,
    const std::map<std::string, std::string> &macros,
    ShaderModuleType                          moduleType,
    bool                                      optimize)
{
    shaderc_shader_kind kind = shaderc_vertex_shader;
    switch(moduleType)
    {
    case ShaderModuleType::Vertex:
        kind = shaderc_vertex_shader;
        break;
    case ShaderModuleType::Fragment:
        kind = shaderc_fragment_shader;
        break;
    }

    shaderc::Compiler compiler;
    shaderc::CompileOptions options;

    for(auto &p : macros)
        options.AddMacroDefinition(p.first, p.second);

    if(optimize)
        options.SetOptimizationLevel(shaderc_optimization_level_performance);

    auto result = compiler.CompileGlslToSpv(
        source, kind, sourceName.c_str(), options);

    if(result.GetCompilationStatus() != shaderc_compilation_status_success)
        throw std::runtime_error(result.GetErrorMessage());

    return { result.cbegin(), result.cend() };
}

AGZ_VULKAN_LAB_END
