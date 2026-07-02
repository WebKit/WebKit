#ifndef _TCUOSXVULKANPLATFORM_HPP
#define _TCUOSXVULKANPLATFORM_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief OSX Vulkan Platform.
 *//*--------------------------------------------------------------------*/

#include "vkWsiPlatform.hpp"
#include "vkPlatform.hpp"

namespace tcu
{
namespace osx

{

class VulkanPlatform : public vk::Platform
{
public:
    VulkanPlatform();
    vk::wsi::Display *createWsiDisplay(vk::wsi::Type wsiType) const;
    vk::Library *createLibrary(const char *libraryPath) const;
    bool hasDisplay(vk::wsi::Type wsiType) const;
    void describePlatform(std::ostream &dst) const;
};

} // namespace osx
} // namespace tcu

#endif // _TCUOSXVULKANPLATFORM_HPP
