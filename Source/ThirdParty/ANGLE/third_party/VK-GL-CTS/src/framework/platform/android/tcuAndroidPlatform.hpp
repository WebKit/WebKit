#ifndef _TCUANDROIDPLATFORM_HPP
#define _TCUANDROIDPLATFORM_HPP
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
 * \brief Android EGL platform.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuPlatform.hpp"
#include "egluPlatform.hpp"
#include "gluPlatform.hpp"
#include "vkPlatform.hpp"
#include "tcuAndroidWindow.hpp"
#include "tcuAndroidNativeActivity.hpp"

namespace tcu
{
namespace Android
{

class Platform : public tcu::Platform, private eglu::Platform, private glu::Platform, private vk::Platform
{
public:
    Platform(NativeActivity &activity);
    virtual ~Platform(void);

    virtual bool processEvents(void);

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

    WindowRegistry &getWindowRegistry(void)
    {
        return m_windowRegistry;
    }

    // Vulkan Platform API
    vk::Library *createLibrary(const char *libraryPath) const;
    void describePlatform(std::ostream &dst) const;
    vk::wsi::Display *createWsiDisplay(vk::wsi::Type wsiType) const;
    bool hasDisplay(vk::wsi::Type wsiType) const;
    void setCustomScreenOrientation(bool enable) const;
    void requestPixelCopy(const char *filename) const;
    void rotateScreen(int rotation) const;

private:
    NativeActivity &m_activity;
    WindowRegistry m_windowRegistry;
    const size_t m_totalSystemMemory;
};

} // namespace Android
} // namespace tcu

#endif // _TCUANDROIDPLATFORM_HPP
