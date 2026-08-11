#ifndef _TCULNXVULKANPLATFORM_HPP
#define _TCULNXVULKANPLATFORM_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \brief Linux Vulkan Platform.
 *//*--------------------------------------------------------------------*/

#include "vkWsiPlatform.hpp"
#include "vkPlatform.hpp"
#include "tcuLnx.hpp"

namespace tcu
{
namespace lnx

{

class VulkanPlatform : public vk::Platform
{
public:
    VulkanPlatform(EventState &eventState);
    vk::wsi::Display *createWsiDisplay(vk::wsi::Type wsiType) const;
    vk::Library *createLibrary(LibraryType libraryType = LIBRARY_TYPE_VULKAN, const char *libraryPath = nullptr) const;
    bool hasDisplay(vk::wsi::Type wsiType) const;
    void describePlatform(std::ostream &dst) const;

private:
    EventState &m_eventState;
};

} // namespace lnx
} // namespace tcu

#endif // _TCULNXVULKANPLATFORM_HPP
