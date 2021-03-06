#pragma once

#include <vulkan/vulkan.hpp>

#include <agz/utility/math.h>
#include <agz/utility/misc.h>

#define AGZ_VULKAN_LAB_BEGIN namespace agz::vlab {
#define AGZ_VULKAN_LAB_END   }

AGZ_VULKAN_LAB_BEGIN

using Vec2 = math::vec2f;
using Vec3 = math::vec3f;
using Vec4 = math::vec4f;

using Vec2i = math::vec2i;

using Mat3 = math::mat3f_c;
using Mat4 = math::mat4f_c;

using Trans4 = Mat4::left_transform;

AGZ_VULKAN_LAB_END
