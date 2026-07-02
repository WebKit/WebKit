/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2016 The Android Open Source Project
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
 * \brief Win32 platform port.
 *//*--------------------------------------------------------------------*/

#include "tcuWin32Platform.hpp"
#include "tcuWGLContextFactory.hpp"
#include "tcuWin32EGLNativeDisplayFactory.hpp"
#include "egluGLContextFactory.hpp"

// MinGW doesn't define this in its headers, but
// still has the export in the libs.
#if defined(__MINGW32__)
extern "C" WINUSERAPI WINBOOL WINAPI SetProcessDPIAware(VOID);
#endif

namespace tcu
{
namespace win32
{

Platform::Platform(void) : m_instance(GetModuleHandle(NULL)), m_vulkanPlatform(m_instance)
{
    // Set process priority to lower.
    SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);

    // Set process to be DPI aware
    // This is requried for VK tests that manually enter exclusive mode
    // using VK_FULL_SCREEN_EXCLUSIVE_APPLICATION_CONTROLLED_EXT
    if (SetProcessDPIAware() == 0)
    {
        TCU_FAIL("SetProcessDPIAware  function failed");
    }

    {
        wgl::ContextFactory *factory = nullptr;

        try
        {
            factory = new wgl::ContextFactory(m_instance);
        }
        catch (const std::exception &e)
        {
            print("Warning: WGL not supported: %s\n", e.what());
        }

        if (factory)
        {
            try
            {
                m_contextFactoryRegistry.registerFactory(factory);
            }
            catch (...)
            {
                delete factory;
                throw;
            }
        }
    }

    m_nativeDisplayFactoryRegistry.registerFactory(new win32::EGLNativeDisplayFactory(m_instance));
    m_contextFactoryRegistry.registerFactory(new eglu::GLContextFactory(m_nativeDisplayFactoryRegistry));
}

Platform::~Platform(void)
{
}

bool Platform::processEvents(void)
{
    MSG msg;
    while (PeekMessage(&msg, (HWND)-1, 0, 0, PM_REMOVE))
    {
        DispatchMessage(&msg);
        if (msg.message == WM_QUIT)
            return false;
    }
    return true;
}

} // namespace win32
} // namespace tcu

// Create platform
tcu::Platform *createPlatform(void)
{
    return new tcu::win32::Platform();
}
