/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2017 The Android Open Source Project
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
 * \brief Linux Platform.
 *//*--------------------------------------------------------------------*/

#include "tcuLnxPlatform.hpp"

#include "tcuLnxVulkanPlatform.hpp"
#include "tcuLnxEglPlatform.hpp"

#include "deUniquePtr.hpp"
#include "gluPlatform.hpp"
#include "vkPlatform.hpp"

#if defined(DEQP_SUPPORT_X11)
#include <X11/Xlib.h>
#endif // DEQP_SUPPORT_X11

#if defined(DEQP_SUPPORT_GLX)
#include "tcuLnxX11GlxPlatform.hpp"
#endif // DEQP_SUPPORT_GLX

using de::MovePtr;
using de::UniquePtr;

namespace tcu
{
namespace lnx
{

class LinuxGLPlatform : public glu::Platform
{
public:
    void registerFactory(de::MovePtr<glu::ContextFactory> factory)
    {
        m_contextFactoryRegistry.registerFactory(factory.release());
    }
};

class LinuxPlatform : public tcu::Platform
{
public:
    LinuxPlatform(void);
    bool processEvents(void)
    {
        return !m_eventState.getQuitFlag();
    }

    const vk::Platform &getVulkanPlatform(void) const
    {
        return m_vkPlatform;
    }
    const eglu::Platform &getEGLPlatform(void) const
    {
        return m_eglPlatform;
    }
    const glu::Platform &getGLPlatform(void) const
    {
        return m_glPlatform;
    }

private:
    EventState m_eventState;
    VulkanPlatform m_vkPlatform;
    egl::Platform m_eglPlatform;
    LinuxGLPlatform m_glPlatform;
};

LinuxPlatform::LinuxPlatform(void) : m_vkPlatform(m_eventState), m_eglPlatform(m_eventState)
{
#if defined(DEQP_SUPPORT_GLX)
    m_glPlatform.registerFactory(x11::glx::createContextFactory(m_eventState));
#endif // DEQP_SUPPORT_GLX

    m_glPlatform.registerFactory(m_eglPlatform.createContextFactory());
}

} // namespace lnx
} // namespace tcu

tcu::Platform *createPlatform(void)
{
#if defined(DEQP_SUPPORT_X11)
    // From man:XinitThreads(3):
    //
    //     The XInitThreads function initializes Xlib support for concurrent
    //     threads.  This function must be the first Xlib function
    //     a multi-threaded program calls, and it must complete before any other
    //     Xlib call is made.
    DE_CHECK_RUNTIME_ERR(XInitThreads() != 0);
#endif // DEQP_SUPPORT_X11

    return new tcu::lnx::LinuxPlatform();
}
