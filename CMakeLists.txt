CMAKE_MINIMUM_REQUIRED(VERSION 3.10)

PROJECT(VulkanLab)

################ CMake Properties

IF(NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE "Release" CACHE STRING "Options: Debug Release RelWithDebInfo MinSizeRel." FORCE)
ENDIF()

################ Vulkan

FIND_PACKAGE(Vulkan REQUIRED)

################ glfw

SET(GLFW_BUILD_EXAMPLES OFF)
SET(GLFW_BUILD_TESTS    OFF)
SET(GLFW_BUILD_DOCS     OFF)
SET(GLFW_INSTALL        OFF)

ADD_SUBDIRECTORY(lib/glfw)

################ Submodules

ADD_SUBDIRECTORY(lib/agz-utils)

ADD_SUBDIRECTORY(src/common)
ADD_SUBDIRECTORY(src/00_init)
