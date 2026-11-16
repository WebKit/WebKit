#ifndef _TCUNULLPLATFORM_HPP
#define _TCUNULLPLATFORM_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
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
 * \brief Null GL platform.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuPlatform.hpp"
#include "gluPlatform.hpp"
#include "egluPlatform.hpp"
#include "vkPlatform.hpp"

namespace tcu
{
namespace null
{

class Platform : public tcu::Platform, private glu::Platform, private eglu::Platform, private vk::Platform
{
public:
    Platform(void);
    virtual ~Platform(void);

    virtual const glu::Platform &getGLPlatform(void) const
    {
        return static_cast<const glu::Platform &>(*this);
    }
    virtual const eglu::Platform &getEGLPlatform(void) const
    {
        return static_cast<const eglu::Platform &>(*this);
    }
    virtual const vk::Platform &getVulkanPlatform(void) const
    {
        return static_cast<const vk::Platform &>(*this);
    }
    virtual void getMemoryLimits(tcu::PlatformMemoryLimits &limits) const;

private:
    virtual vk::Library *createLibrary(vk::Platform::LibraryType libraryType = vk::Platform::LIBRARY_TYPE_VULKAN,
                                       const char *libraryPath               = nullptr) const;
};

} // namespace null
} // namespace tcu

#endif // _TCUNULLPLATFORM_HPP
