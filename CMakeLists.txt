CMAKE_MINIMUM_REQUIRED(VERSION 3.10)

PROJECT(VulkanLab)

################ CMake Properties

IF(NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
ENDIF()

################ Vulkan

FIND_PACKAGE(Vulkan REQUIRED)

################ glfw

SET(GLFW_BUILD_EXAMPLES OFF)
SET(GLFW_BUILD_TESTS    OFF)
SET(GLFW_BUILD_DOCS     OFF)
SET(GLFW_INSTALL        OFF)

ADD_SUBDIRECTORY(lib/glfw)

################ shaderc

ADD_SUBDIRECTORY(lib/shaderc)

################ agz-utils

ADD_SUBDIRECTORY(lib/agz-utils)

################ samples

ADD_SUBDIRECTORY(src/common)
ADD_SUBDIRECTORY(src/00_init)
ADD_SUBDIRECTORY(src/01_triangle)
ADD_SUBDIRECTORY(src/02_vertexIndexBuffer)
ADD_SUBDIRECTORY(src/03_uniformBuffer)

SET_TARGET_PROPERTIES(glfw AGZUtils PROPERTIES FOLDER "ThirdParty")
