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
 */

#include "tcuANGLEPlatform.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "egluGLContextFactory.hpp"
#include "tcuANGLENativeDisplayFactory.h"
#include "tcuNullContextFactory.hpp"
#include "util/test_utils.h"

static_assert(EGL_DONT_CARE == -1, "Unexpected value for EGL_DONT_CARE");

namespace tcu
{
ANGLEPlatform::ANGLEPlatform(angle::LogErrorFunc logErrorFunc)
{
    angle::SetLowPriorityProcess();

    mPlatformMethods.logError = logErrorFunc;

#if (DE_OS == DE_OS_WIN32)
    {
        std::vector<eglw::EGLAttrib> d3d11Attribs = initAttribs(
            EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE, EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE);

        auto *d3d11Factory = new ANGLENativeDisplayFactory("angle-d3d11", "ANGLE D3D11 Display",
                                                           d3d11Attribs, &mEvents);
        m_nativeDisplayFactoryRegistry.registerFactory(d3d11Factory);
    }

    {
        std::vector<eglw::EGLAttrib> d3d9Attribs = initAttribs(
            EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE, EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE);

        auto *d3d9Factory = new ANGLENativeDisplayFactory("angle-d3d9", "ANGLE D3D9 Display",
                                                          d3d9Attribs, &mEvents);
        m_nativeDisplayFactoryRegistry.registerFactory(d3d9Factory);
    }

    {
        std::vector<eglw::EGLAttrib> d3d1193Attribs =
            initAttribs(EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
                        EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE, 9, 3);

        auto *d3d1193Factory = new ANGLENativeDisplayFactory(
            "angle-d3d11-fl93", "ANGLE D3D11 FL9_3 Display", d3d1193Attribs, &mEvents);
        m_nativeDisplayFactoryRegistry.registerFactory(d3d1193Factory);
    }
#endif  // (DE_OS == DE_OS_WIN32)

#if defined(ANGLE_USE_OZONE) || (DE_OS == DE_OS_ANDROID) || (DE_OS == DE_OS_WIN32)
    {
        std::vector<eglw::EGLAttrib> glesAttribs =
            initAttribs(EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE);

        auto *glesFactory = new ANGLENativeDisplayFactory("angle-gles", "ANGLE OpenGL ES Display",
                                                          glesAttribs, &mEvents);
        m_nativeDisplayFactoryRegistry.registerFactory(glesFactory);
    }
#endif

    {
        std::vector<eglw::EGLAttrib> glAttribs = initAttribs(EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE);

        auto *glFactory =
            new ANGLENativeDisplayFactory("angle-gl", "ANGLE OpenGL Display", glAttribs, &mEvents);
        m_nativeDisplayFactoryRegistry.registerFactory(glFactory);
    }

#if (DE_OS == DE_OS_ANDROID) || (DE_OS == DE_OS_WIN32) || (DE_OS == DE_OS_UNIX)
    {
        std::vector<eglw::EGLAttrib> vkAttribs = initAttribs(EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE);

        auto *vkFactory = new ANGLENativeDisplayFactory("angle-vulkan", "ANGLE Vulkan Display",
                                                        vkAttribs, &mEvents);
        m_nativeDisplayFactoryRegistry.registerFactory(vkFactory);
    }
#endif

#if (DE_OS == DE_OS_WIN32) || (DE_OS == DE_OS_UNIX) || (DE_OS == DE_OS_OSX)
    {
        std::vector<eglw::EGLAttrib> swsAttribs = initAttribs(
            EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE, EGL_PLATFORM_ANGLE_DEVICE_TYPE_SWIFTSHADER_ANGLE);
        m_nativeDisplayFactoryRegistry.registerFactory(new ANGLENativeDisplayFactory(
            "angle-swiftshader", "ANGLE SwiftShader Display", swsAttribs, &mEvents));
    }
#endif

#if (DE_OS == DE_OS_OSX)
    {
        std::vector<eglw::EGLAttrib> mtlAttribs = initAttribs(EGL_PLATFORM_ANGLE_TYPE_METAL_ANGLE);

        auto *mtlFactory = new ANGLENativeDisplayFactory("angle-metal", "ANGLE Metal Display",
                                                         mtlAttribs, &mEvents);
        m_nativeDisplayFactoryRegistry.registerFactory(mtlFactory);
    }
#endif

    {
        std::vector<eglw::EGLAttrib> nullAttribs = initAttribs(EGL_PLATFORM_ANGLE_TYPE_NULL_ANGLE);

        auto *nullFactory = new ANGLENativeDisplayFactory("angle-null", "ANGLE NULL Display",
                                                          nullAttribs, &mEvents);
        m_nativeDisplayFactoryRegistry.registerFactory(nullFactory);
    }

    m_contextFactoryRegistry.registerFactory(
        new eglu::GLContextFactory(m_nativeDisplayFactoryRegistry));

    // Add Null context type for use in generating case lists
    m_contextFactoryRegistry.registerFactory(new null::NullGLContextFactory());
}

ANGLEPlatform::~ANGLEPlatform() {}

bool ANGLEPlatform::processEvents()
{
    return !mEvents.quitSignaled();
}

std::vector<eglw::EGLAttrib> ANGLEPlatform::initAttribs(eglw::EGLAttrib type,
                                                        eglw::EGLAttrib deviceType,
                                                        eglw::EGLAttrib majorVersion,
                                                        eglw::EGLAttrib minorVersion)
{
    std::vector<eglw::EGLAttrib> attribs;

    attribs.push_back(EGL_PLATFORM_ANGLE_TYPE_ANGLE);
    attribs.push_back(type);

    if (deviceType != EGL_DONT_CARE)
    {
        attribs.push_back(EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE);
        attribs.push_back(deviceType);
    }

    if (majorVersion != EGL_DONT_CARE)
    {
        attribs.push_back(EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE);
        attribs.push_back(majorVersion);
    }

    if (minorVersion != EGL_DONT_CARE)
    {
        attribs.push_back(EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE);
        attribs.push_back(minorVersion);
    }

    if (mPlatformMethods.logError)
    {
        static_assert(sizeof(eglw::EGLAttrib) == sizeof(angle::PlatformMethods *),
                      "Unexpected pointer size");
        attribs.push_back(EGL_PLATFORM_ANGLE_PLATFORM_METHODS_ANGLEX);
        attribs.push_back(reinterpret_cast<eglw::EGLAttrib>(&mPlatformMethods));
    }

    attribs.push_back(EGL_NONE);
    return attribs;
}
}  // namespace tcu

// Create platform
tcu::Platform *CreateANGLEPlatform(angle::LogErrorFunc logErrorFunc)
{
    return new tcu::ANGLEPlatform(logErrorFunc);
}

tcu::Platform *createPlatform()
{
    return CreateANGLEPlatform(nullptr);
}
