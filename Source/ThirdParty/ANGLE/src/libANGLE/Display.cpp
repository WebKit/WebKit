//
// Copyright 2002 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Display.cpp: Implements the egl::Display class, representing the abstract
// display on which graphics are drawn. Implements EGLDisplay.
// [EGL 1.4] section 2.1.2 page 3.

#include "libANGLE/Display.h"

#include <algorithm>
#include <iterator>
#include <sstream>
#include <vector>

#include <EGL/eglext.h>
#include <platform/Platform.h>

#include "anglebase/no_destructor.h"
#include "common/android_util.h"
#include "common/debug.h"
#include "common/mathutil.h"
#include "common/platform.h"
#include "common/string_utils.h"
#include "common/system_utils.h"
#include "common/tls.h"
#include "common/utilities.h"
#include "gpu_info_util/SystemInfo.h"
#include "libANGLE/Context.h"
#include "libANGLE/Device.h"
#include "libANGLE/EGLSync.h"
#include "libANGLE/Image.h"
#include "libANGLE/ResourceManager.h"
#include "libANGLE/Stream.h"
#include "libANGLE/Surface.h"
#include "libANGLE/Thread.h"
#include "libANGLE/capture/FrameCapture.h"
#include "libANGLE/histogram_macros.h"
#include "libANGLE/renderer/DeviceImpl.h"
#include "libANGLE/renderer/DisplayImpl.h"
#include "libANGLE/renderer/ImageImpl.h"
#include "libANGLE/trace.h"

#if defined(ANGLE_ENABLE_D3D9) || defined(ANGLE_ENABLE_D3D11)
#    include <versionhelpers.h>

#    include "libANGLE/renderer/d3d/DisplayD3D.h"
#endif

#if defined(ANGLE_ENABLE_OPENGL)
#    if defined(ANGLE_PLATFORM_WINDOWS)
#        include "libANGLE/renderer/gl/wgl/DisplayWGL.h"
#    elif defined(ANGLE_PLATFORM_MACOS) || defined(ANGLE_PLATFORM_IOS)
#        include "libANGLE/renderer/gl/apple/DisplayApple_api.h"
#    elif defined(ANGLE_PLATFORM_LINUX)
#        include "libANGLE/renderer/gl/egl/DisplayEGL.h"
#        if defined(ANGLE_USE_GBM)
#            include "libANGLE/renderer/gl/egl/gbm/DisplayGbm.h"
#        endif
#        if defined(ANGLE_USE_X11)
#            include "libANGLE/renderer/gl/glx/DisplayGLX.h"
#        endif
#    elif defined(ANGLE_PLATFORM_ANDROID)
#        include "libANGLE/renderer/gl/egl/android/DisplayAndroid.h"
#    else
#        error Unsupported OpenGL platform.
#    endif
#endif

#if defined(ANGLE_ENABLE_NULL)
#    include "libANGLE/renderer/null/DisplayNULL.h"
#endif  // defined(ANGLE_ENABLE_NULL)

#if defined(ANGLE_ENABLE_VULKAN)
#    include "libANGLE/renderer/vulkan/DisplayVk_api.h"
#endif  // defined(ANGLE_ENABLE_VULKAN)

#if defined(ANGLE_ENABLE_METAL)
#    include "libANGLE/renderer/metal/DisplayMtl_api.h"
#endif  // defined(ANGLE_ENABLE_METAL)

namespace egl
{

namespace
{

constexpr angle::SubjectIndex kGPUSwitchedSubjectIndex = 0;

static constexpr size_t kWindowSurfaceMapSize = 32;
typedef angle::FlatUnorderedMap<EGLNativeWindowType, Surface *, kWindowSurfaceMapSize>
    WindowSurfaceMap;
// Get a map of all EGL window surfaces to validate that no window has more than one EGL surface
// associated with it.
static WindowSurfaceMap *GetWindowSurfaces()
{
    static angle::base::NoDestructor<WindowSurfaceMap> windowSurfaces;
    return windowSurfaces.get();
}

struct ANGLEPlatformDisplay
{
    ANGLEPlatformDisplay() = default;

    ANGLEPlatformDisplay(EGLNativeDisplayType nativeDisplayType)
        : nativeDisplayType(nativeDisplayType)
    {}

    ANGLEPlatformDisplay(EGLNativeDisplayType nativeDisplayType,
                         EGLAttrib powerPreference,
                         EGLAttrib platformANGLEType,
                         EGLAttrib deviceIdHigh,
                         EGLAttrib deviceIdLow)
        : nativeDisplayType(nativeDisplayType),
          powerPreference(powerPreference),
          platformANGLEType(platformANGLEType),
          deviceIdHigh(deviceIdHigh),
          deviceIdLow(deviceIdLow)
    {}

    auto tie() const
    {
        return std::tie(nativeDisplayType, powerPreference, platformANGLEType, deviceIdHigh,
                        deviceIdLow);
    }

    EGLNativeDisplayType nativeDisplayType{EGL_DEFAULT_DISPLAY};
    EGLAttrib powerPreference{EGL_LOW_POWER_ANGLE};
    EGLAttrib platformANGLEType{EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE};
    EGLAttrib deviceIdHigh{0};
    EGLAttrib deviceIdLow{0};
};

inline bool operator==(const ANGLEPlatformDisplay &a, const ANGLEPlatformDisplay &b)
{
    return a.tie() == b.tie();
}

static constexpr size_t kANGLEPlatformDisplayMapSize = 9;
typedef angle::FlatUnorderedMap<ANGLEPlatformDisplay, Display *, kANGLEPlatformDisplayMapSize>
    ANGLEPlatformDisplayMap;
static ANGLEPlatformDisplayMap *GetANGLEPlatformDisplayMap()
{
    static angle::base::NoDestructor<ANGLEPlatformDisplayMap> displays;
    return displays.get();
}

static constexpr size_t kDevicePlatformDisplayMapSize = 8;
typedef angle::FlatUnorderedMap<Device *, Display *, kDevicePlatformDisplayMapSize>
    DevicePlatformDisplayMap;
static DevicePlatformDisplayMap *GetDevicePlatformDisplayMap()
{
    static angle::base::NoDestructor<DevicePlatformDisplayMap> displays;
    return displays.get();
}

rx::DisplayImpl *CreateDisplayFromDevice(Device *eglDevice, const DisplayState &state)
{
    rx::DisplayImpl *impl = nullptr;

    switch (eglDevice->getType())
    {
#if defined(ANGLE_ENABLE_D3D11)
        case EGL_D3D11_DEVICE_ANGLE:
            impl = new rx::DisplayD3D(state);
            break;
#endif
#if defined(ANGLE_ENABLE_D3D9)
        case EGL_D3D9_DEVICE_ANGLE:
            // Currently the only way to get EGLDeviceEXT representing a D3D9 device
            // is to retrieve one from an already-existing EGLDisplay.
            // When eglGetPlatformDisplayEXT is called with a D3D9 EGLDeviceEXT,
            // the already-existing display should be returned.
            // Therefore this codepath to create a new display from the device
            // should never be hit.
            UNREACHABLE();
            break;
#endif
        default:
            UNREACHABLE();
            break;
    }

    ASSERT(impl != nullptr);
    return impl;
}

// On platforms with support for multiple back-ends, allow an environment variable to control
// the default.  This is useful to run angle with benchmarks without having to modify the
// benchmark source.  Possible values for this environment variable (ANGLE_DEFAULT_PLATFORM)
// are: vulkan, gl, d3d11, null.
EGLAttrib GetDisplayTypeFromEnvironment()
{
    std::string angleDefaultEnv = angle::GetEnvironmentVar("ANGLE_DEFAULT_PLATFORM");
    angle::ToLower(&angleDefaultEnv);

#if defined(ANGLE_ENABLE_VULKAN)
    if ((angleDefaultEnv == "vulkan") || (angleDefaultEnv == "vulkan-null") ||
        (angleDefaultEnv == "swiftshader"))
    {
        return EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE;
    }
#endif

#if defined(ANGLE_ENABLE_OPENGL)
    if (angleDefaultEnv == "gl")
    {
        return EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE;
    }
#endif

#if defined(ANGLE_ENABLE_D3D11)
    if (angleDefaultEnv == "d3d11")
    {
        return EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE;
    }
#endif

#if defined(ANGLE_ENABLE_METAL)
    if (angleDefaultEnv == "metal")
    {
        return EGL_PLATFORM_ANGLE_TYPE_METAL_ANGLE;
    }
#endif

#if defined(ANGLE_ENABLE_NULL)
    if (angleDefaultEnv == "null")
    {
        return EGL_PLATFORM_ANGLE_TYPE_NULL_ANGLE;
    }
#endif
#if defined(ANGLE_ENABLE_METAL)
    if (angleDefaultEnv == "metal")
    {
        return EGL_PLATFORM_ANGLE_TYPE_METAL_ANGLE;
    }

#endif
#if defined(ANGLE_ENABLE_D3D11)
    return EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE;
#elif defined(ANGLE_ENABLE_D3D9)
    return EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE;
#elif defined(ANGLE_ENABLE_VULKAN) && defined(ANGLE_PLATFORM_ANDROID)
    return EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE;
#elif defined(ANGLE_ENABLE_OPENGL)
#    if defined(ANGLE_PLATFORM_ANDROID) || defined(ANGLE_USE_GBM)
    return EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE;
#    else
    return EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE;
#    endif
#elif defined(ANGLE_ENABLE_METAL)
    return EGL_PLATFORM_ANGLE_TYPE_METAL_ANGLE;
#elif defined(ANGLE_ENABLE_VULKAN)
    return EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE;
#elif defined(ANGLE_ENABLE_NULL)
    return EGL_PLATFORM_ANGLE_TYPE_NULL_ANGLE;
#else
#    error No default ANGLE platform type
#endif
}

EGLAttrib GetDeviceTypeFromEnvironment()
{
    std::string angleDefaultEnv = angle::GetEnvironmentVar("ANGLE_DEFAULT_PLATFORM");
    angle::ToLower(&angleDefaultEnv);

#if defined(ANGLE_ENABLE_VULKAN)
    if (angleDefaultEnv == "vulkan-null")
    {
        return EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE;
    }
    else if (angleDefaultEnv == "swiftshader")
    {
        return EGL_PLATFORM_ANGLE_DEVICE_TYPE_SWIFTSHADER_ANGLE;
    }
#endif
    return EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE;
}

EGLAttrib GetPlatformTypeFromEnvironment()
{
#if defined(ANGLE_USE_OZONE)
    return 0;
#elif defined(ANGLE_USE_X11)
    return EGL_PLATFORM_X11_EXT;
#elif defined(ANGLE_USE_WAYLAND)
    return EGL_PLATFORM_WAYLAND_EXT;
#elif defined(ANGLE_USE_VULKAN_DISPLAY) && defined(ANGLE_VULKAN_DISPLAY_MODE_SIMPLE)
    return EGL_PLATFORM_VULKAN_DISPLAY_MODE_SIMPLE_ANGLE;
#elif defined(ANGLE_USE_VULKAN_DISPLAY) && defined(ANGLE_VULKAN_DISPLAY_MODE_HEADLESS)
    return EGL_PLATFORM_VULKAN_DISPLAY_MODE_HEADLESS_ANGLE;
#else
    return 0;
#endif  // defined(ANGLE_USE_OZONE)
}

rx::DisplayImpl *CreateDisplayFromAttribs(EGLAttrib displayType,
                                          EGLAttrib deviceType,
                                          EGLAttrib platformType,
                                          const DisplayState &state)
{
    ASSERT(displayType != EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE);
    rx::DisplayImpl *impl = nullptr;

    switch (displayType)
    {
        case EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE:
            UNREACHABLE();
            break;

        case EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE:
        case EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE:
#if defined(ANGLE_ENABLE_D3D9) || defined(ANGLE_ENABLE_D3D11)
            impl = new rx::DisplayD3D(state);
            break;
#else
            // A D3D display was requested on a platform that doesn't support it
            UNREACHABLE();
            break;
#endif

        case EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE:
#if defined(ANGLE_ENABLE_OPENGL)
#    if defined(ANGLE_PLATFORM_WINDOWS)
            impl = new rx::DisplayWGL(state);
            break;

#    elif defined(ANGLE_PLATFORM_MACOS) || defined(ANGLE_PLATFORM_IOS)
            impl = rx::CreateDisplayCGLOrEAGL(state);
            break;

#    elif defined(ANGLE_PLATFORM_LINUX)
#        if defined(ANGLE_USE_GBM)
            if (platformType == 0)
            {
                // If platformType is unknown, use DisplayGbm now. In the future, it should use
                // DisplayEGL letting native EGL decide what display to use.
                impl = new rx::DisplayGbm(state);
                break;
            }
#        endif
            if (deviceType == EGL_PLATFORM_ANGLE_DEVICE_TYPE_EGL_ANGLE)
            {
                impl = new rx::DisplayEGL(state);
                break;
            }
#        if defined(ANGLE_USE_X11)
            if (platformType == EGL_PLATFORM_X11_EXT)
            {
                impl = new rx::DisplayGLX(state);
                break;
            }
#        endif
            break;

#    elif defined(ANGLE_PLATFORM_ANDROID)
            // No GL support on this platform, fail display creation.
            impl = nullptr;
            break;

#    else
#        error Unsupported OpenGL platform.
#    endif
#else
            // No display available
            UNREACHABLE();
            break;

#endif  // defined(ANGLE_ENABLE_OPENGL)

        case EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE:
#if defined(ANGLE_ENABLE_OPENGL)
#    if defined(ANGLE_PLATFORM_WINDOWS)
            impl = new rx::DisplayWGL(state);
#    elif defined(ANGLE_PLATFORM_LINUX)
#        if defined(ANGLE_USE_GBM)
            if (platformType == 0 ||
                platformType == EGL_PLATFORM_VULKAN_DISPLAY_MODE_HEADLESS_ANGLE)
            {
                // If platformType is unknown, use DisplayGbm now. In the future, it should use
                // DisplayEGL letting native EGL decide what display to use.
                // platformType == EGL_PLATFORM_VULKAN_DISPLAY_MODE_HEADLESS_ANGLE is a hack,
                // to allow ChromeOS GLES backend to continue functioning when Vulkan is enabled.
                impl = new rx::DisplayGbm(state);
                break;
            }
#        endif
            if (deviceType == EGL_PLATFORM_ANGLE_DEVICE_TYPE_EGL_ANGLE)
            {
                impl = new rx::DisplayEGL(state);
                break;
            }
            else
            {
#        if defined(ANGLE_USE_X11)
                if (platformType == EGL_PLATFORM_X11_EXT)
                {
                    impl = new rx::DisplayGLX(state);
                    break;
                }
#        endif
            }
#    elif defined(ANGLE_PLATFORM_ANDROID)
            impl = new rx::DisplayAndroid(state);
#    else
            // No GLES support on this platform, fail display creation.
            impl = nullptr;
#    endif
#endif  // defined(ANGLE_ENABLE_OPENGL)
            break;

        case EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE:
#if defined(ANGLE_ENABLE_VULKAN)
#    if defined(ANGLE_USE_VULKAN_NULL_DISPLAY)
            if (rx::IsVulkanNullDisplayAvailable())
            {
                impl = rx::CreateVulkanNullDisplay(state);
            }
            break;
#    elif defined(ANGLE_PLATFORM_WINDOWS)
            if (rx::IsVulkanWin32DisplayAvailable())
            {
                impl = rx::CreateVulkanWin32Display(state);
            }
            break;
#    elif defined(ANGLE_PLATFORM_LINUX)
#        if defined(ANGLE_USE_GBM)
            if (platformType == EGL_PLATFORM_GBM_KHR && rx::IsVulkanGbmDisplayAvailable())
            {
                impl = rx::CreateVulkanGbmDisplay(state);
                break;
            }
#        endif
#        if defined(ANGLE_USE_X11)
            if (platformType == EGL_PLATFORM_X11_EXT && rx::IsVulkanXcbDisplayAvailable())
            {
                impl = rx::CreateVulkanXcbDisplay(state);
                break;
            }
#        endif
#        if defined(ANGLE_USE_WAYLAND)
            if (platformType == EGL_PLATFORM_WAYLAND_EXT && rx::IsVulkanWaylandDisplayAvailable())
            {
                impl = rx::CreateVulkanWaylandDisplay(state);
                break;
            }
#        endif
#        if defined(ANGLE_USE_VULKAN_DISPLAY)
            if (platformType == EGL_PLATFORM_VULKAN_DISPLAY_MODE_SIMPLE_ANGLE &&
                rx::IsVulkanSimpleDisplayAvailable())
            {
                impl = rx::CreateVulkanSimpleDisplay(state);
            }
            else if (platformType == EGL_PLATFORM_VULKAN_DISPLAY_MODE_HEADLESS_ANGLE &&
                     rx::IsVulkanHeadlessDisplayAvailable())
            {
                impl = rx::CreateVulkanHeadlessDisplay(state);
            }
            else
            {
                // Not supported creation type on vulkan display, fail display creation.
                impl = nullptr;
            }
#        endif
            break;
#    elif defined(ANGLE_PLATFORM_ANDROID)
            if (rx::IsVulkanAndroidDisplayAvailable())
            {
                impl = rx::CreateVulkanAndroidDisplay(state);
            }
            break;
#    elif defined(ANGLE_PLATFORM_FUCHSIA)
            if (rx::IsVulkanFuchsiaDisplayAvailable())
            {
                impl = rx::CreateVulkanFuchsiaDisplay(state);
            }
            break;
#    elif defined(ANGLE_PLATFORM_GGP)
            if (rx::IsVulkanGGPDisplayAvailable())
            {
                impl = rx::CreateVulkanGGPDisplay(state);
            }
            break;
#    elif defined(ANGLE_PLATFORM_APPLE)
            if (rx::IsVulkanMacDisplayAvailable())
            {
                impl = rx::CreateVulkanMacDisplay(state);
            }
            break;
#    else
#        error Unsupported Vulkan platform.
#    endif
#else
            // No display available
            UNREACHABLE();
            break;
#endif  // defined(ANGLE_ENABLE_VULKAN)

        case EGL_PLATFORM_ANGLE_TYPE_METAL_ANGLE:
#if defined(ANGLE_ENABLE_METAL)
            if (rx::IsMetalDisplayAvailable())
            {
                impl = rx::CreateMetalDisplay(state);
                break;
            }
#endif
            // No display available
            UNREACHABLE();
            break;

        case EGL_PLATFORM_ANGLE_TYPE_NULL_ANGLE:
#if defined(ANGLE_ENABLE_NULL)
            impl = new rx::DisplayNULL(state);
            break;
#else
            // No display available
            UNREACHABLE();
            break;
#endif  // defined(ANGLE_ENABLE_NULL)

        default:
            UNREACHABLE();
            break;
    }

    return impl;
}

void Display_logError(angle::PlatformMethods *platform, const char *errorMessage)
{
    gl::Trace(gl::LOG_ERR, errorMessage);
}

void Display_logWarning(angle::PlatformMethods *platform, const char *warningMessage)
{
    gl::Trace(gl::LOG_WARN, warningMessage);
}

void Display_logInfo(angle::PlatformMethods *platform, const char *infoMessage)
{
    // Uncomment to get info spam
#if defined(ANGLE_ENABLE_DEBUG_TRACE)
    gl::Trace(gl::LOG_INFO, infoMessage);
#endif
}

const std::vector<std::string> EGLStringArrayToStringVector(const char **ary)
{
    std::vector<std::string> vec;
    if (ary != nullptr)
    {
        for (; *ary != nullptr; ary++)
        {
            vec.push_back(std::string(*ary));
        }
    }
    return vec;
}

void ANGLESetDefaultDisplayPlatform(angle::EGLDisplayType display)
{
    angle::PlatformMethods *platformMethods = ANGLEPlatformCurrent();

    ANGLEResetDisplayPlatform(display);
    platformMethods->logError   = Display_logError;
    platformMethods->logWarning = Display_logWarning;
    platformMethods->logInfo    = Display_logInfo;
}

void UpdateAttribsFromEnvironment(AttributeMap &attribMap)
{
    EGLAttrib displayType =
        attribMap.get(EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE);
    if (displayType == EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE)
    {
        displayType = GetDisplayTypeFromEnvironment();
        attribMap.insert(EGL_PLATFORM_ANGLE_TYPE_ANGLE, displayType);
    }
    EGLAttrib deviceType = attribMap.get(EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE, 0);
    if (deviceType == 0)
    {
        deviceType = GetDeviceTypeFromEnvironment();
        attribMap.insert(EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE, deviceType);
    }
    EGLAttrib platformType = attribMap.get(EGL_PLATFORM_ANGLE_NATIVE_PLATFORM_TYPE_ANGLE, 0);
    if (platformType == 0)
    {
        platformType = GetPlatformTypeFromEnvironment();
        attribMap.insert(EGL_PLATFORM_ANGLE_NATIVE_PLATFORM_TYPE_ANGLE, platformType);
    }
}

static constexpr uint32_t kScratchBufferLifetime = 64u;

}  // anonymous namespace

// ShareGroup
ShareGroup::ShareGroup(rx::EGLImplFactory *factory)
    : mRefCount(1),
      mImplementation(factory->createShareGroup()),
      mFrameCaptureShared(new angle::FrameCaptureShared)
{}

void ShareGroup::finishAllContexts()
{
    for (gl::Context *shareContext : mContexts)
    {
        if (shareContext->hasBeenCurrent() && !shareContext->isDestroyed())
        {
            shareContext->finish();
        }
    }
}

void ShareGroup::addSharedContext(gl::Context *context)
{
    mContexts.insert(context);
}

void ShareGroup::removeSharedContext(gl::Context *context)
{
    mContexts.erase(context);
}

ShareGroup::~ShareGroup()
{
    SafeDelete(mImplementation);
}

void ShareGroup::addRef()
{
    // This is protected by global lock, so no atomic is required
    mRefCount++;
}

void ShareGroup::release(const Display *display)
{
    if (--mRefCount == 0)
    {
        if (mImplementation)
        {
            mImplementation->onDestroy(display);
        }
        delete this;
    }
}

// DisplayState
DisplayState::DisplayState(EGLNativeDisplayType nativeDisplayId)
    : label(nullptr), featuresAllDisabled(false), displayId(nativeDisplayId)
{}

DisplayState::~DisplayState() {}

// Note that ANGLE support on Ozone platform is limited. Our preferred support Matrix for
// EGL_ANGLE_platform_angle on Linux and Ozone/Linux/Fuchsia platforms should be the following:
//
// |--------------------------------------------------------|
// | ANGLE type | DEVICE type |  PLATFORM type   | Display  |
// |--------------------------------------------------------|
// |   OPENGL   |     EGL     |       ANY        |   EGL    |
// |   OPENGL   |   HARDWARE  |     X11_EXT      |   GLX    |
// |  OPENGLES  |   HARDWARE  |     X11_EXT      |   GLX    |
// |  OPENGLES  |     EGL     |       ANY        |   EGL    |
// |   VULKAN   |   HARDWARE  |     X11_EXT      |  VkXcb   |
// |   VULKAN   | SWIFTSHADER |     X11_EXT      |  VkXcb   |
// |  OPENGLES  |   HARDWARE  | SURFACELESS_MESA |   EGL*   |
// |  OPENGLES  |   HARDWARE  |    DEVICE_EXT    |   EGL    |
// |   VULKAN   |   HARDWARE  | SURFACELESS_MESA | VkBase** |
// |   VULKAN   | SWIFTSHADER | SURFACELESS_MESA | VkBase** |
// |--------------------------------------------------------|
//
// * No surfaceless support yet.
// ** Not implemented yet.
//
// |-----------------------------------------------|
// |   OS    | BUILD type |  Default PLATFORM type |
// |-----------------------------------------------|
// |  Linux  |    X11     |        X11_EXT         |
// |  Linux  |   Ozone    |    SURFACELESS_MESA    |
// | Fuchsia |   Ozone    |        FUCHSIA***      |
// |-----------------------------------------------|
//
// *** Chosen implicitly. No EGLAttrib available.
//
// For more details, please refer to
// https://docs.google.com/document/d/1XjHiDZQISq1AMrg_l1TX1_kIKvDpU76hidn9i4cAjl8/edit?disco=AAAAJl9V_YY
//
// static
Display *Display::GetDisplayFromNativeDisplay(EGLenum platform,
                                              EGLNativeDisplayType nativeDisplay,
                                              const AttributeMap &attribMap)
{
    Display *display = nullptr;

    AttributeMap updatedAttribMap(attribMap);
    UpdateAttribsFromEnvironment(updatedAttribMap);

    EGLAttrib powerPreference =
        updatedAttribMap.get(EGL_POWER_PREFERENCE_ANGLE, EGL_LOW_POWER_ANGLE);
    EGLAttrib platformANGLEType =
        updatedAttribMap.get(EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE);
    EGLAttrib deviceIdHigh = updatedAttribMap.get(EGL_PLATFORM_ANGLE_DEVICE_ID_HIGH_ANGLE, 0);
    EGLAttrib deviceIdLow  = updatedAttribMap.get(EGL_PLATFORM_ANGLE_DEVICE_ID_LOW_ANGLE, 0);
    ANGLEPlatformDisplayMap *displays = GetANGLEPlatformDisplayMap();
    ANGLEPlatformDisplay displayKey(nativeDisplay, powerPreference, platformANGLEType, deviceIdHigh,
                                    deviceIdLow);
    const auto &iter = displays->find(displayKey);
    if (iter != displays->end())
    {
        display = iter->second;
    }

    if (display == nullptr)
    {
        // Validate the native display
        if (!Display::isValidNativeDisplay(nativeDisplay))
        {
            return nullptr;
        }

        display = new Display(platform, nativeDisplay, nullptr);
        displays->insert(std::make_pair(displayKey, display));
    }
    // Apply new attributes if the display is not initialized yet.
    if (!display->isInitialized())
    {
        display->setAttributes(updatedAttribMap);

        EGLAttrib displayType  = display->mAttributeMap.get(EGL_PLATFORM_ANGLE_TYPE_ANGLE);
        EGLAttrib deviceType   = display->mAttributeMap.get(EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE);
        EGLAttrib platformType = platform;
        if (platform == EGL_PLATFORM_ANGLE_ANGLE)
        {
            platformType =
                display->mAttributeMap.get(EGL_PLATFORM_ANGLE_NATIVE_PLATFORM_TYPE_ANGLE);
        }
        rx::DisplayImpl *impl =
            CreateDisplayFromAttribs(displayType, deviceType, platformType, display->getState());
        if (impl == nullptr)
        {
            // No valid display implementation for these attributes
            return nullptr;
        }

#if defined(ANGLE_USE_ANDROID_TLS_SLOT)
        angle::gUseAndroidOpenGLTlsSlot = displayType == EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE;
#endif  // defined(ANGLE_PLATFORM_ANDROID)

        display->setupDisplayPlatform(impl);
    }

    return display;
}

// static
Display *Display::GetExistingDisplayFromNativeDisplay(EGLNativeDisplayType nativeDisplay)
{
    ANGLEPlatformDisplayMap *displays = GetANGLEPlatformDisplayMap();
    const auto &iter                  = displays->find(nativeDisplay);

    // Check that there is a matching display
    if (iter == displays->end())
    {
        return nullptr;
    }

    return iter->second;
}

// static
Display *Display::GetDisplayFromDevice(Device *device, const AttributeMap &attribMap)
{
    Display *display = nullptr;

    ASSERT(Device::IsValidDevice(device));

    ANGLEPlatformDisplayMap *anglePlatformDisplays   = GetANGLEPlatformDisplayMap();
    DevicePlatformDisplayMap *devicePlatformDisplays = GetDevicePlatformDisplayMap();

    // First see if this eglDevice is in use by a Display created using ANGLE platform
    for (auto &displayMapEntry : *anglePlatformDisplays)
    {
        egl::Display *iterDisplay = displayMapEntry.second;
        if (iterDisplay->getDevice() == device)
        {
            display = iterDisplay;
        }
    }

    if (display == nullptr)
    {
        // See if the eglDevice is in use by a Display created using the DEVICE platform
        const auto &iter = devicePlatformDisplays->find(device);
        if (iter != devicePlatformDisplays->end())
        {
            display = iter->second;
        }
    }

    if (display == nullptr)
    {
        // Otherwise create a new Display
        display = new Display(EGL_PLATFORM_DEVICE_EXT, 0, device);
        devicePlatformDisplays->insert(std::make_pair(device, display));
    }

    // Apply new attributes if the display is not initialized yet.
    if (!display->isInitialized())
    {
        display->setAttributes(attribMap);
        rx::DisplayImpl *impl = CreateDisplayFromDevice(device, display->getState());
        display->setupDisplayPlatform(impl);
    }

    return display;
}

// static
Display::EglDisplaySet Display::GetEglDisplaySet()
{
    Display::EglDisplaySet displays;

    ANGLEPlatformDisplayMap *anglePlatformDisplays   = GetANGLEPlatformDisplayMap();
    DevicePlatformDisplayMap *devicePlatformDisplays = GetDevicePlatformDisplayMap();

    for (auto anglePlatformDisplayMapEntry : *anglePlatformDisplays)
    {
        displays.insert(anglePlatformDisplayMapEntry.second);
    }

    for (auto devicePlatformDisplayMapEntry : *devicePlatformDisplays)
    {
        displays.insert(devicePlatformDisplayMapEntry.second);
    }

    return displays;
}

Display::Display(EGLenum platform, EGLNativeDisplayType displayId, Device *eglDevice)
    : mState(displayId),
      mImplementation(nullptr),
      mGPUSwitchedBinding(this, kGPUSwitchedSubjectIndex),
      mAttributeMap(),
      mConfigSet(),
      mStreamSet(),
      mInvalidImageSet(),
      mInvalidStreamSet(),
      mInvalidSurfaceSet(),
      mInvalidSyncSet(),
      mInitialized(false),
      mDeviceLost(false),
      mCaps(),
      mDisplayExtensions(),
      mDisplayExtensionString(),
      mVendorString(),
      mVersionString(),
      mDevice(eglDevice),
      mSurface(nullptr),
      mPlatform(platform),
      mTextureManager(nullptr),
      mSemaphoreManager(nullptr),
      mBlobCache(gl::kDefaultMaxProgramCacheMemoryBytes),
      mMemoryProgramCache(mBlobCache),
      mGlobalTextureShareGroupUsers(0),
      mGlobalSemaphoreShareGroupUsers(0),
      mTerminatedByApi(false),
      mActiveThreads()
{}

Display::~Display()
{
    switch (mPlatform)
    {
        case EGL_PLATFORM_ANGLE_ANGLE:
        case EGL_PLATFORM_GBM_KHR:
        case EGL_PLATFORM_WAYLAND_EXT:
        {
            ANGLEPlatformDisplayMap *displays      = GetANGLEPlatformDisplayMap();
            ANGLEPlatformDisplayMap::iterator iter = displays->find(ANGLEPlatformDisplay(
                mState.displayId,
                mAttributeMap.get(EGL_POWER_PREFERENCE_ANGLE, EGL_LOW_POWER_ANGLE),
                mAttributeMap.get(EGL_PLATFORM_ANGLE_TYPE_ANGLE,
                                  EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE),
                mAttributeMap.get(EGL_PLATFORM_ANGLE_DEVICE_ID_HIGH_ANGLE, 0),
                mAttributeMap.get(EGL_PLATFORM_ANGLE_DEVICE_ID_LOW_ANGLE, 0)));
            if (iter != displays->end())
            {
                displays->erase(iter);
            }
            break;
        }
        case EGL_PLATFORM_DEVICE_EXT:
        {
            DevicePlatformDisplayMap *displays      = GetDevicePlatformDisplayMap();
            DevicePlatformDisplayMap::iterator iter = displays->find(mDevice);
            if (iter != displays->end())
            {
                displays->erase(iter);
            }
            break;
        }
        default:
        {
            UNREACHABLE();
        }
    }

    SafeDelete(mDevice);
    SafeDelete(mImplementation);
}

void Display::setLabel(EGLLabelKHR label)
{
    mState.label = label;
}

EGLLabelKHR Display::getLabel() const
{
    return mState.label;
}

void Display::onSubjectStateChange(angle::SubjectIndex index, angle::SubjectMessage message)
{
    ASSERT(index == kGPUSwitchedSubjectIndex);
    ASSERT(message == angle::SubjectMessage::SubjectChanged);
    for (gl::Context *context : mState.contextSet)
    {
        context->onGPUSwitch();
    }
}

void Display::setupDisplayPlatform(rx::DisplayImpl *impl)
{
    ASSERT(!mInitialized);

    ASSERT(impl != nullptr);
    SafeDelete(mImplementation);
    mImplementation = impl;

    // TODO(anglebug.com/7365): Remove PlatformMethods.
    const angle::PlatformMethods *platformMethods =
        reinterpret_cast<const angle::PlatformMethods *>(
            mAttributeMap.get(EGL_PLATFORM_ANGLE_PLATFORM_METHODS_ANGLEX, 0));
    if (platformMethods != nullptr)
    {
        *ANGLEPlatformCurrent() = *platformMethods;
    }
    else
    {
        ANGLESetDefaultDisplayPlatform(this);
    }

    const char **featuresForceEnabled =
        reinterpret_cast<const char **>(mAttributeMap.get(EGL_FEATURE_OVERRIDES_ENABLED_ANGLE, 0));
    const char **featuresForceDisabled =
        reinterpret_cast<const char **>(mAttributeMap.get(EGL_FEATURE_OVERRIDES_DISABLED_ANGLE, 0));
    mState.featureOverridesEnabled  = EGLStringArrayToStringVector(featuresForceEnabled);
    mState.featureOverridesDisabled = EGLStringArrayToStringVector(featuresForceDisabled);
    mState.featuresAllDisabled =
        static_cast<bool>(mAttributeMap.get(EGL_FEATURE_ALL_DISABLED_ANGLE, 0));
    mImplementation->addObserver(&mGPUSwitchedBinding);
}

Error Display::initialize()
{
    mTerminatedByApi = false;

    ASSERT(mImplementation != nullptr);
    mImplementation->setBlobCache(&mBlobCache);

    // Enable shader caching if debug layers are turned on. This allows us to test that shaders are
    // properly saved & restored on all platforms. The cache won't allocate space until it's used
    // and will be ignored entirely if the application / system sets it's own cache functions.
    if (rx::ShouldUseDebugLayers(mAttributeMap))
    {
        mBlobCache.resize(1024 * 1024);
    }

    setGlobalDebugAnnotator();

    gl::InitializeDebugMutexIfNeeded();

    ANGLE_TRACE_EVENT0("gpu.angle", "egl::Display::initialize");

    if (isInitialized())
    {
        return NoError();
    }

    Error error = mImplementation->initialize(this);
    if (error.isError())
    {
        // Log extended error message here
        ERR() << "ANGLE Display::initialize error " << error.getID() << ": " << error.getMessage();
        return error;
    }

    mCaps = mImplementation->getCaps();

    mConfigSet = mImplementation->generateConfigs();
    if (mConfigSet.size() == 0)
    {
        mImplementation->terminate();
        return EglNotInitialized() << "No configs were generated.";
    }

    // OpenGL ES1 is implemented in the frontend, explicitly add ES1 support to all configs
    for (auto &config : mConfigSet)
    {
        // TODO(geofflang): Enable the conformant bit once we pass enough tests
        // config.second.conformant |= EGL_OPENGL_ES_BIT;

        config.second.renderableType |= EGL_OPENGL_ES_BIT;

        // If we aren't using desktop GL entry points, remove desktop GL support from all configs
#if !defined(ANGLE_ENABLE_GL_DESKTOP_FRONTEND)
        config.second.renderableType &= ~EGL_OPENGL_BIT;
#endif
    }

    if (!mState.featuresAllDisabled)
    {
        initializeFrontendFeatures();
    }

    mFeatures.clear();
    mFrontendFeatures.populateFeatureList(&mFeatures);
    mImplementation->populateFeatureList(&mFeatures);

    initDisplayExtensions();
    initVendorString();
    initVersionString();

    // Populate the Display's EGLDeviceEXT if the Display wasn't created using one
    if (mPlatform == EGL_PLATFORM_DEVICE_EXT)
    {
        // For EGL_PLATFORM_DEVICE_EXT, mDevice should always be populated using
        // an external device
        ASSERT(mDevice != nullptr);
    }
    else if (GetClientExtensions().deviceQueryEXT)
    {
        std::unique_ptr<rx::DeviceImpl> impl(mImplementation->createDevice());
        ASSERT(impl);
        error = impl->initialize();
        if (error.isError())
        {
            ERR() << "Failed to initialize display because device creation failed: "
                  << error.getMessage();
            mImplementation->terminate();
            return error;
        }
        // Don't leak Device memory.
        ASSERT(mDevice == nullptr);
        mDevice = new Device(this, impl.release());
    }
    else
    {
        mDevice = nullptr;
    }

    mInitialized = true;

    return NoError();
}

Error Display::destroyInvalidEglObjects()
{
    // Destroy invalid EGL objects

    ImageSet images     = {};
    StreamSet streams   = {};
    SurfaceSet surfaces = {};
    SyncSet syncs       = {};
    {
        // Retrieve objects to be destroyed
        std::lock_guard<std::mutex> lock(mInvalidEglObjectsMutex);
        images   = std::move(mInvalidImageSet);
        streams  = std::move(mInvalidStreamSet);
        surfaces = std::move(mInvalidSurfaceSet);
        syncs    = std::move(mInvalidSyncSet);

        // Update invalid object sets
        mInvalidImageSet.clear();
        mInvalidStreamSet.clear();
        mInvalidSurfaceSet.clear();
        mInvalidSyncSet.clear();
    }

    while (!images.empty())
    {
        destroyImageImpl(*images.begin(), &images);
    }

    while (!streams.empty())
    {
        destroyStreamImpl(*streams.begin(), &streams);
    }

    while (!surfaces.empty())
    {
        ANGLE_TRY(destroySurfaceImpl(*surfaces.begin(), &surfaces));
    }

    while (!syncs.empty())
    {
        destroySyncImpl(*syncs.begin(), &syncs);
    }

    return NoError();
}

Error Display::terminate(Thread *thread, TerminateReason terminateReason)
{
    if (terminateReason == TerminateReason::Api)
    {
        mTerminatedByApi = true;
    }

    if (!mInitialized)
    {
        return NoError();
    }

    // EGL 1.5 Specification
    // 3.2 Initialization
    // Termination marks all EGL-specific resources, such as contexts and surfaces, associated
    // with the specified display for deletion. Handles to all such resources are invalid as soon
    // as eglTerminate returns
    // Cache EGL objects that are no longer valid
    // TODO (http://www.anglebug.com/6798): Invalidate context handles as well.
    if (mTerminatedByApi)
    {
        std::lock_guard<std::mutex> lock(mInvalidEglObjectsMutex);

        mInvalidImageSet.insert(mImageSet.begin(), mImageSet.end());
        mImageSet.clear();

        mInvalidStreamSet.insert(mStreamSet.begin(), mStreamSet.end());
        mStreamSet.clear();

        mInvalidSurfaceSet.insert(mState.surfaceSet.begin(), mState.surfaceSet.end());
        mState.surfaceSet.clear();

        mInvalidSyncSet.insert(mSyncSet.begin(), mSyncSet.end());
        mSyncSet.clear();
    }

    // All EGL objects, except contexts, have been marked invalid by the block above and will be
    // cleaned up by "destroyInvalidEglObjects". If app called eglTerminate and no active threads
    // remain, perform the cleanup right away.
    for (gl::Context *context : mState.contextSet)
    {
        if (context->getRefCount() > 0)
        {
            if (terminateReason == TerminateReason::NoActiveThreads)
            {
                ASSERT(mTerminatedByApi);
                context->release();
                (void)context->unMakeCurrent(this);
            }
            else
            {
                return NoError();
            }
        }
    }

    // Destroy all of the Contexts for this Display, since none of them are current anymore.
    while (!mState.contextSet.empty())
    {
        gl::Context *context = *mState.contextSet.begin();
        context->setIsDestroyed();
        ANGLE_TRY(releaseContext(context, thread));
    }

    mMemoryProgramCache.clear();
    mBlobCache.setBlobCacheFuncs(nullptr, nullptr);

    // The global texture and semaphore managers should be deleted with the last context that uses
    // it.
    ASSERT(mGlobalTextureShareGroupUsers == 0 && mTextureManager == nullptr);
    ASSERT(mGlobalSemaphoreShareGroupUsers == 0 && mSemaphoreManager == nullptr);

    // Now that contexts have been destroyed, clean up any remaining invalid objects
    ANGLE_TRY(destroyInvalidEglObjects());

    mConfigSet.clear();

    if (mDevice != nullptr && mDevice->getOwningDisplay() != nullptr)
    {
        // Don't delete the device if it was created externally using eglCreateDeviceANGLE
        // We also shouldn't set it to null in case eglInitialize() is called again later
        SafeDelete(mDevice);
    }

    mImplementation->terminate();

    mDeviceLost = false;

    mInitialized = false;

    gl::UninitializeDebugAnnotations();

    // TODO(jmadill): Store Platform in Display and deinit here.
    ANGLEResetDisplayPlatform(this);

    return NoError();
}

Error Display::prepareForCall()
{
    return mImplementation->prepareForCall();
}

Error Display::releaseThread()
{
    ANGLE_TRY(mImplementation->releaseThread());
    return destroyInvalidEglObjects();
}

void Display::addActiveThread(Thread *thread)
{
    std::lock_guard<std::mutex> lock(mActiveThreadsMutex);
    mActiveThreads.insert(thread);
}

void Display::removeActiveThreadAndPerformCleanup(Thread *thread)
{
    std::lock_guard<std::mutex> lock(mActiveThreadsMutex);
    mActiveThreads.erase(thread);
    if (mTerminatedByApi && mActiveThreads.size() == 0)
    {
        (void)terminate(thread, TerminateReason::NoActiveThreads);
    }
}

std::vector<const Config *> Display::getConfigs(const egl::AttributeMap &attribs) const
{
    return mConfigSet.filter(attribs);
}

std::vector<const Config *> Display::chooseConfig(const egl::AttributeMap &attribs) const
{
    egl::AttributeMap attribsWithDefaults = AttributeMap();

    // Insert default values for attributes that have either an Exact or Mask selection criteria,
    // and a default value that matters (e.g. isn't EGL_DONT_CARE):
    attribsWithDefaults.insert(EGL_COLOR_BUFFER_TYPE, EGL_RGB_BUFFER);
    attribsWithDefaults.insert(EGL_LEVEL, 0);
    attribsWithDefaults.insert(EGL_RENDERABLE_TYPE, EGL_OPENGL_ES_BIT);
    attribsWithDefaults.insert(EGL_SURFACE_TYPE, EGL_WINDOW_BIT);
    attribsWithDefaults.insert(EGL_TRANSPARENT_TYPE, EGL_NONE);
    if (getExtensions().pixelFormatFloat)
    {
        attribsWithDefaults.insert(EGL_COLOR_COMPONENT_TYPE_EXT,
                                   EGL_COLOR_COMPONENT_TYPE_FIXED_EXT);
    }

    // Add the caller-specified values (Note: the poorly-named insert() method will replace any
    // of the default values from above):
    for (auto attribIter = attribs.begin(); attribIter != attribs.end(); attribIter++)
    {
        attribsWithDefaults.insert(attribIter->first, attribIter->second);
    }

    return mConfigSet.filter(attribsWithDefaults);
}

Error Display::createWindowSurface(const Config *configuration,
                                   EGLNativeWindowType window,
                                   const AttributeMap &attribs,
                                   Surface **outSurface)
{
    if (mImplementation->testDeviceLost())
    {
        ANGLE_TRY(restoreLostDevice());
    }

    SurfacePointer surface(new WindowSurface(mImplementation, configuration, window, attribs,
                                             mFrontendFeatures.forceRobustResourceInit.enabled),
                           this);
    ANGLE_TRY(surface->initialize(this));

    ASSERT(outSurface != nullptr);
    *outSurface = surface.release();
    mState.surfaceSet.insert(*outSurface);

    WindowSurfaceMap *windowSurfaces = GetWindowSurfaces();
    ASSERT(windowSurfaces && windowSurfaces->find(window) == windowSurfaces->end());
    windowSurfaces->insert(std::make_pair(window, *outSurface));

    mSurface = *outSurface;

    return NoError();
}

Error Display::createPbufferSurface(const Config *configuration,
                                    const AttributeMap &attribs,
                                    Surface **outSurface)
{
    ASSERT(isInitialized());

    if (mImplementation->testDeviceLost())
    {
        ANGLE_TRY(restoreLostDevice());
    }

    SurfacePointer surface(new PbufferSurface(mImplementation, configuration, attribs,
                                              mFrontendFeatures.forceRobustResourceInit.enabled),
                           this);
    ANGLE_TRY(surface->initialize(this));

    ASSERT(outSurface != nullptr);
    *outSurface = surface.release();
    mState.surfaceSet.insert(*outSurface);

    return NoError();
}

Error Display::createPbufferFromClientBuffer(const Config *configuration,
                                             EGLenum buftype,
                                             EGLClientBuffer clientBuffer,
                                             const AttributeMap &attribs,
                                             Surface **outSurface)
{
    ASSERT(isInitialized());

    if (mImplementation->testDeviceLost())
    {
        ANGLE_TRY(restoreLostDevice());
    }

    SurfacePointer surface(
        new PbufferSurface(mImplementation, configuration, buftype, clientBuffer, attribs,
                           mFrontendFeatures.forceRobustResourceInit.enabled),
        this);
    ANGLE_TRY(surface->initialize(this));

    ASSERT(outSurface != nullptr);
    *outSurface = surface.release();
    mState.surfaceSet.insert(*outSurface);

    return NoError();
}

Error Display::createPixmapSurface(const Config *configuration,
                                   NativePixmapType nativePixmap,
                                   const AttributeMap &attribs,
                                   Surface **outSurface)
{
    ASSERT(isInitialized());

    if (mImplementation->testDeviceLost())
    {
        ANGLE_TRY(restoreLostDevice());
    }

    SurfacePointer surface(new PixmapSurface(mImplementation, configuration, nativePixmap, attribs,
                                             mFrontendFeatures.forceRobustResourceInit.enabled),
                           this);
    ANGLE_TRY(surface->initialize(this));

    ASSERT(outSurface != nullptr);
    *outSurface = surface.release();
    mState.surfaceSet.insert(*outSurface);

    return NoError();
}

Error Display::createImage(const gl::Context *context,
                           EGLenum target,
                           EGLClientBuffer buffer,
                           const AttributeMap &attribs,
                           Image **outImage)
{
    ASSERT(isInitialized());

    if (mImplementation->testDeviceLost())
    {
        ANGLE_TRY(restoreLostDevice());
    }

    egl::ImageSibling *sibling = nullptr;
    if (IsTextureTarget(target))
    {
        sibling = context->getTexture({egl_gl::EGLClientBufferToGLObjectHandle(buffer)});
    }
    else if (IsRenderbufferTarget(target))
    {
        sibling = context->getRenderbuffer({egl_gl::EGLClientBufferToGLObjectHandle(buffer)});
    }
    else if (IsExternalImageTarget(target))
    {
        sibling = new ExternalImageSibling(mImplementation, context, target, buffer, attribs);
    }
    else
    {
        UNREACHABLE();
    }
    ASSERT(sibling != nullptr);

    angle::UniqueObjectPointer<Image, Display> imagePtr(
        new Image(mImplementation, context, target, sibling, attribs), this);
    ANGLE_TRY(imagePtr->initialize(this));

    Image *image = imagePtr.release();

    ASSERT(outImage != nullptr);
    *outImage = image;

    // Add this image to the list of all images and hold a ref to it.
    image->addRef();
    mImageSet.insert(image);

    return NoError();
}

Error Display::createStream(const AttributeMap &attribs, Stream **outStream)
{
    ASSERT(isInitialized());

    Stream *stream = new Stream(this, attribs);

    ASSERT(stream != nullptr);
    mStreamSet.insert(stream);

    ASSERT(outStream != nullptr);
    *outStream = stream;

    return NoError();
}

Error Display::createContext(const Config *configuration,
                             gl::Context *shareContext,
                             EGLenum clientType,
                             const AttributeMap &attribs,
                             gl::Context **outContext)
{
    ASSERT(!mTerminatedByApi);
    ASSERT(isInitialized());

    if (mImplementation->testDeviceLost())
    {
        ANGLE_TRY(restoreLostDevice());
    }

    // This display texture sharing will allow the first context to create the texture share group.
    bool usingDisplayTextureShareGroup =
        attribs.get(EGL_DISPLAY_TEXTURE_SHARE_GROUP_ANGLE, EGL_FALSE) == EGL_TRUE;
    gl::TextureManager *shareTextures = nullptr;

    if (usingDisplayTextureShareGroup)
    {
        ASSERT((mTextureManager == nullptr) == (mGlobalTextureShareGroupUsers == 0));
        if (mTextureManager == nullptr)
        {
            mTextureManager = new gl::TextureManager();
        }

        mGlobalTextureShareGroupUsers++;
        shareTextures = mTextureManager;
    }

    // This display semaphore sharing will allow the first context to create the semaphore share
    // group.
    bool usingDisplaySemaphoreShareGroup =
        attribs.get(EGL_DISPLAY_SEMAPHORE_SHARE_GROUP_ANGLE, EGL_FALSE) == EGL_TRUE;
    gl::SemaphoreManager *shareSemaphores = nullptr;
    if (usingDisplaySemaphoreShareGroup)
    {
        ASSERT((mSemaphoreManager == nullptr) == (mGlobalSemaphoreShareGroupUsers == 0));
        if (mSemaphoreManager == nullptr)
        {
            mSemaphoreManager = new gl::SemaphoreManager();
        }

        mGlobalSemaphoreShareGroupUsers++;
        shareSemaphores = mSemaphoreManager;
    }

    gl::MemoryProgramCache *cachePointer = &mMemoryProgramCache;

    // Check context creation attributes to see if we are using EGL_ANGLE_program_cache_control.
    // If not, keep caching enabled for EGL_ANDROID_blob_cache, which can have its callbacks set
    // at any time.
    bool usesProgramCacheControl = attribs.contains(EGL_CONTEXT_PROGRAM_BINARY_CACHE_ENABLED_ANGLE);
    if (usesProgramCacheControl)
    {
        bool programCacheControlEnabled =
            (attribs.get(EGL_CONTEXT_PROGRAM_BINARY_CACHE_ENABLED_ANGLE, GL_FALSE) == GL_TRUE);
        // A program cache size of zero indicates it should be disabled.
        if (!programCacheControlEnabled || mMemoryProgramCache.maxSize() == 0)
        {
            cachePointer = nullptr;
        }
    }

    gl::Context *context = new gl::Context(this, configuration, shareContext, shareTextures,
                                           shareSemaphores, cachePointer, clientType, attribs,
                                           mDisplayExtensions, GetClientExtensions());
    Error error          = context->initialize();
    if (error.isError())
    {
        delete context;
        return error;
    }

    if (shareContext != nullptr)
    {
        shareContext->setShared();
    }

    ASSERT(context != nullptr);
    mState.contextSet.insert(context);

    ASSERT(outContext != nullptr);
    *outContext = context;
    return NoError();
}

Error Display::createSync(const gl::Context *currentContext,
                          EGLenum type,
                          const AttributeMap &attribs,
                          Sync **outSync)
{
    ASSERT(isInitialized());

    if (mImplementation->testDeviceLost())
    {
        ANGLE_TRY(restoreLostDevice());
    }

    angle::UniqueObjectPointer<egl::Sync, Display> syncPtr(new Sync(mImplementation, type, attribs),
                                                           this);

    ANGLE_TRY(syncPtr->initialize(this, currentContext));

    Sync *sync = syncPtr.release();

    sync->addRef();
    mSyncSet.insert(sync);

    *outSync = sync;
    return NoError();
}

Error Display::makeCurrent(Thread *thread,
                           gl::Context *previousContext,
                           egl::Surface *drawSurface,
                           egl::Surface *readSurface,
                           gl::Context *context)
{
    if (!mInitialized)
    {
        return NoError();
    }

    bool contextChanged = context != previousContext;
    if (previousContext != nullptr && contextChanged)
    {
        previousContext->release();
        thread->setCurrent(nullptr);

        auto error = previousContext->unMakeCurrent(this);
        if (previousContext->getRefCount() == 0 && previousContext->isDestroyed())
        {
            // The previous Context may have been created with a different Display.
            Display *previousDisplay = previousContext->getDisplay();
            ANGLE_TRY(previousDisplay->releaseContext(previousContext, thread));
        }
        ANGLE_TRY(error);
    }

    thread->setCurrent(context);

    ANGLE_TRY(mImplementation->makeCurrent(this, drawSurface, readSurface, context));

    if (context != nullptr)
    {
        ANGLE_TRY(context->makeCurrent(this, drawSurface, readSurface));
        if (contextChanged)
        {
            context->addRef();
        }
    }

    // Tick all the scratch buffers to make sure they get cleaned up eventually if they stop being
    // used.
    {
        std::lock_guard<std::mutex> lock(mScratchBufferMutex);

        for (angle::ScratchBuffer &scatchBuffer : mScratchBuffers)
        {
            scatchBuffer.tick();
        }
        for (angle::ScratchBuffer &zeroFilledBuffer : mZeroFilledBuffers)
        {
            zeroFilledBuffer.tick();
        }
    }

    return NoError();
}

Error Display::restoreLostDevice()
{
    for (ContextSet::iterator ctx = mState.contextSet.begin(); ctx != mState.contextSet.end();
         ctx++)
    {
        if ((*ctx)->isResetNotificationEnabled())
        {
            // If reset notifications have been requested, application must delete all contexts
            // first
            return EglContextLost();
        }
    }

    return mImplementation->restoreLostDevice(this);
}

Error Display::destroySurfaceImpl(Surface *surface, SurfaceSet *surfaces)
{
    if (surface->getType() == EGL_WINDOW_BIT)
    {
        WindowSurfaceMap *windowSurfaces = GetWindowSurfaces();
        ASSERT(windowSurfaces);

        bool surfaceRemoved = false;
        for (WindowSurfaceMap::iterator iter = windowSurfaces->begin();
             iter != windowSurfaces->end(); iter++)
        {
            if (iter->second == surface)
            {
                windowSurfaces->erase(iter);
                surfaceRemoved = true;
                break;
            }
        }

        ASSERT(surfaceRemoved);
    }

    auto iter = surfaces->find(surface);
    ASSERT(iter != surfaces->end());
    surfaces->erase(iter);
    ANGLE_TRY(surface->onDestroy(this));
    return NoError();
}

void Display::destroyImageImpl(Image *image, ImageSet *images)
{
    auto iter = images->find(image);
    ASSERT(iter != images->end());
    (*iter)->release(this);
    images->erase(iter);
}

void Display::destroyStreamImpl(Stream *stream, StreamSet *streams)
{
    streams->erase(stream);
    SafeDelete(stream);
}

// releaseContext must be called with the context being deleted as current.
// To do that we can only call this in two places, Display::makeCurrent at the point where this
// context is being made uncurrent and in Display::destroyContext where we make the context current
// as part of destruction.
Error Display::releaseContext(gl::Context *context, Thread *thread)
{
    ASSERT(context->getRefCount() == 0);

    // Use scoped_ptr to make sure the context is always freed.
    std::unique_ptr<gl::Context> unique_context(context);
    ASSERT(mState.contextSet.find(context) != mState.contextSet.end());
    mState.contextSet.erase(context);

    if (context->usingDisplayTextureShareGroup())
    {
        ASSERT(mGlobalTextureShareGroupUsers >= 1 && mTextureManager != nullptr);
        if (mGlobalTextureShareGroupUsers == 1)
        {
            // If this is the last context using the global share group, destroy the global
            // texture manager so that the textures can be destroyed while a context still
            // exists
            mTextureManager->release(context);
            mTextureManager = nullptr;
        }
        mGlobalTextureShareGroupUsers--;
    }

    if (context->usingDisplaySemaphoreShareGroup())
    {
        ASSERT(mGlobalSemaphoreShareGroupUsers >= 1 && mSemaphoreManager != nullptr);
        if (mGlobalSemaphoreShareGroupUsers == 1)
        {
            // If this is the last context using the global share group, destroy the global
            // semaphore manager so that the semaphores can be destroyed while a context still
            // exists
            mSemaphoreManager->release(context);
            mSemaphoreManager = nullptr;
        }
        mGlobalSemaphoreShareGroupUsers--;
    }

    ANGLE_TRY(context->onDestroy(this));

    return NoError();
}

Error Display::destroyContext(Thread *thread, gl::Context *context)
{
    auto *currentContext     = thread->getContext();
    auto *currentDrawSurface = thread->getCurrentDrawSurface();
    auto *currentReadSurface = thread->getCurrentReadSurface();

    context->setIsDestroyed();

    // If the context is still current on at least 1 thread, just return since it'll be released
    // once no threads have it current anymore.
    if (context->getRefCount() > 0)
    {
        return NoError();
    }

    // For external context, we cannot change the current native context, and the API user should
    // make sure the native context is current.
    if (context->isExternal())
    {
        ANGLE_TRY(releaseContext(context, thread));
    }
    else
    {
        // Keep |currentContext| alive, while releasing |context|.
        gl::ScopedContextRef scopedContextRef(currentContext);

        // keep |currentDrawSurface| and |currentReadSurface| alive as well
        // while releasing |context|.
        ScopedSurfaceRef drawSurfaceRef(currentDrawSurface);
        ScopedSurfaceRef readSurfaceRef(
            currentReadSurface == currentDrawSurface ? nullptr : currentReadSurface);

        // Make the context current, so we can release resources belong to the context, and then
        // when context is released from the current, it will be destroyed.
        // TODO(http://www.anglebug.com/6322): Don't require a Context to be current in order to
        // destroy it.
        ANGLE_TRY(makeCurrent(thread, currentContext, nullptr, nullptr, context));
        ANGLE_TRY(
            makeCurrent(thread, context, currentDrawSurface, currentReadSurface, currentContext));
    }

    // If eglTerminate() has previously been called and this is the last Context the Display owns,
    // we can now fully terminate the display and release all of its resources.
    if (mTerminatedByApi)
    {
        for (const gl::Context *ctx : mState.contextSet)
        {
            if (ctx->getRefCount() > 0)
            {
                return NoError();
            }
        }

        return terminate(thread, TerminateReason::InternalCleanup);
    }

    return NoError();
}

void Display::destroySyncImpl(Sync *sync, SyncSet *syncs)
{
    auto iter = syncs->find(sync);
    ASSERT(iter != syncs->end());
    (*iter)->release(this);
    syncs->erase(iter);
}

void Display::destroyImage(Image *image)
{
    return destroyImageImpl(image, &mImageSet);
}

void Display::destroyStream(Stream *stream)
{
    return destroyStreamImpl(stream, &mStreamSet);
}

Error Display::destroySurface(Surface *surface)
{
    return destroySurfaceImpl(surface, &mState.surfaceSet);
}

void Display::destroySync(Sync *sync)
{
    return destroySyncImpl(sync, &mSyncSet);
}

bool Display::isDeviceLost() const
{
    ASSERT(isInitialized());
    return mDeviceLost;
}

bool Display::testDeviceLost()
{
    ASSERT(isInitialized());

    if (!mDeviceLost && mImplementation->testDeviceLost())
    {
        notifyDeviceLost();
    }

    return mDeviceLost;
}

void Display::notifyDeviceLost()
{
    if (mDeviceLost)
    {
        return;
    }

    for (ContextSet::iterator context = mState.contextSet.begin();
         context != mState.contextSet.end(); context++)
    {
        (*context)->markContextLost(gl::GraphicsResetStatus::UnknownContextReset);
    }

    mDeviceLost = true;
}

void Display::setBlobCacheFuncs(EGLSetBlobFuncANDROID set, EGLGetBlobFuncANDROID get)
{
    mBlobCache.setBlobCacheFuncs(set, get);
    mImplementation->setBlobCacheFuncs(set, get);
}

// static
EGLClientBuffer Display::GetNativeClientBuffer(const AHardwareBuffer *buffer)
{
    return angle::android::AHardwareBufferToClientBuffer(buffer);
}

// static
Error Display::CreateNativeClientBuffer(const egl::AttributeMap &attribMap,
                                        EGLClientBuffer *eglClientBuffer)
{
    int androidHardwareBufferFormat = gl::GetAndroidHardwareBufferFormatFromChannelSizes(attribMap);
    int width                       = attribMap.getAsInt(EGL_WIDTH, 0);
    int height                      = attribMap.getAsInt(EGL_HEIGHT, 0);
    int usage                       = attribMap.getAsInt(EGL_NATIVE_BUFFER_USAGE_ANDROID, 0);

    // https://developer.android.com/ndk/reference/group/a-hardware-buffer#ahardwarebuffer_lock
    // for AHardwareBuffer_lock()
    // The passed AHardwareBuffer must have one layer, otherwise the call will fail.
    constexpr int kLayerCount = 1;

    *eglClientBuffer = angle::android::CreateEGLClientBufferFromAHardwareBuffer(
        width, height, kLayerCount, androidHardwareBufferFormat, usage);

    return (*eglClientBuffer == nullptr)
               ? egl::EglBadParameter() << "native client buffer allocation failed."
               : NoError();
}

Error Display::waitClient(const gl::Context *context)
{
    return mImplementation->waitClient(context);
}

Error Display::waitNative(const gl::Context *context, EGLint engine)
{
    return mImplementation->waitNative(context, engine);
}

const Caps &Display::getCaps() const
{
    return mCaps;
}

bool Display::isInitialized() const
{
    return mInitialized;
}

bool Display::isValidConfig(const Config *config) const
{
    return mConfigSet.contains(config);
}

bool Display::isValidContext(const gl::Context *context) const
{
    return mState.contextSet.find(const_cast<gl::Context *>(context)) != mState.contextSet.end();
}

bool Display::isValidSurface(const Surface *surface) const
{
    return mState.surfaceSet.find(const_cast<Surface *>(surface)) != mState.surfaceSet.end();
}

bool Display::isValidImage(const Image *image) const
{
    return mImageSet.find(const_cast<Image *>(image)) != mImageSet.end();
}

bool Display::isValidStream(const Stream *stream) const
{
    return mStreamSet.find(const_cast<Stream *>(stream)) != mStreamSet.end();
}

bool Display::isValidSync(const Sync *sync) const
{
    return mSyncSet.find(const_cast<Sync *>(sync)) != mSyncSet.end();
}

bool Display::hasExistingWindowSurface(EGLNativeWindowType window)
{
    WindowSurfaceMap *windowSurfaces = GetWindowSurfaces();
    ASSERT(windowSurfaces);

    return windowSurfaces->find(window) != windowSurfaces->end();
}

static ClientExtensions GenerateClientExtensions()
{
    ClientExtensions extensions;

    extensions.clientExtensions = true;
    extensions.platformBase     = true;
    extensions.platformANGLE    = true;

#if defined(ANGLE_ENABLE_D3D9) || defined(ANGLE_ENABLE_D3D11)
    extensions.platformANGLED3D = true;
    extensions.platformDevice   = true;
#endif

#if defined(ANGLE_USE_GBM)
    extensions.platformGbmKHR = true;
#endif

#if defined(ANGLE_USE_WAYLAND)
    extensions.platformWaylandEXT = true;
#endif

#if defined(ANGLE_ENABLE_D3D11)
#    if defined(ANGLE_ENABLE_WINDOWS_UWP)
    extensions.platformANGLED3D11ON12 = true;
#    else
    extensions.platformANGLED3D11ON12 = IsWindows10OrGreater();
#    endif
    extensions.platformANGLEDeviceId = true;
#endif

#if defined(ANGLE_ENABLE_OPENGL)
    extensions.platformANGLEOpenGL = true;
#endif

#if defined(ANGLE_ENABLE_NULL)
    extensions.platformANGLENULL = true;
#endif

#if defined(ANGLE_ENABLE_D3D11)
    extensions.deviceCreation          = true;
    extensions.deviceCreationD3D11     = true;
    extensions.experimentalPresentPath = true;
#endif

#if defined(ANGLE_ENABLE_VULKAN)
    extensions.platformANGLEVulkan   = true;
    extensions.platformANGLEDeviceId = true;
#endif

#if defined(ANGLE_ENABLE_SWIFTSHADER)
    extensions.platformANGLEDeviceTypeSwiftShader = true;
#endif

#if defined(ANGLE_ENABLE_METAL)
    extensions.platformANGLEMetal    = true;
    extensions.platformANGLEDeviceId = true;
#endif

#if defined(ANGLE_USE_X11)
    extensions.x11Visual = true;
#endif

#if defined(ANGLE_PLATFORM_LINUX)
    extensions.platformANGLEDeviceTypeEGLANGLE = true;
#endif

#if (defined(ANGLE_PLATFORM_IOS) && !defined(ANGLE_PLATFORM_MACCATALYST)) || \
    (defined(ANGLE_PLATFORM_MACCATALYST) && defined(ANGLE_CPU_ARM64))
    extensions.platformANGLEDeviceContextVolatileEagl = true;
#endif

#if defined(ANGLE_PLATFORM_MACOS) || defined(ANGLE_PLATFORM_MACCATALYST)
    extensions.platformANGLEDeviceContextVolatileCgl = true;
#endif

#if defined(ANGLE_ENABLE_METAL)
    extensions.displayPowerPreferenceANGLE = true;
#endif

    extensions.clientGetAllProcAddresses = true;
    extensions.debug                     = true;
    extensions.featureControlANGLE       = true;
    extensions.deviceQueryEXT            = true;

    return extensions;
}

template <typename T>
static std::string GenerateExtensionsString(const T &extensions)
{
    std::vector<std::string> extensionsVector = extensions.getStrings();

    std::ostringstream stream;
    std::copy(extensionsVector.begin(), extensionsVector.end(),
              std::ostream_iterator<std::string>(stream, " "));
    return stream.str();
}

// static
const ClientExtensions &Display::GetClientExtensions()
{
    static const ClientExtensions clientExtensions = GenerateClientExtensions();
    return clientExtensions;
}

// static
const std::string &Display::GetClientExtensionString()
{
    static const angle::base::NoDestructor<std::string> clientExtensionsString(
        GenerateExtensionsString(GetClientExtensions()));
    return *clientExtensionsString;
}

void Display::initDisplayExtensions()
{
    mDisplayExtensions = mImplementation->getExtensions();

    // Some extensions are always available because they are implemented in the EGL layer.
    mDisplayExtensions.createContext        = true;
    mDisplayExtensions.createContextNoError = !mFrontendFeatures.forceGlErrorChecking.enabled;
    mDisplayExtensions.createContextWebGLCompatibility    = true;
    mDisplayExtensions.createContextBindGeneratesResource = true;
    mDisplayExtensions.createContextClientArrays          = true;
    mDisplayExtensions.pixelFormatFloat                   = true;
    mDisplayExtensions.reusableSyncKHR                    = true;

    // Force EGL_KHR_get_all_proc_addresses on.
    mDisplayExtensions.getAllProcAddresses = true;

    // Enable program cache control since it is not back-end dependent.
    mDisplayExtensions.programCacheControlANGLE = true;

    // Request extension is implemented in the ANGLE frontend
    mDisplayExtensions.createContextExtensionsEnabled = true;

    // Blob cache extension is provided by the ANGLE frontend
    mDisplayExtensions.blobCache = true;

    // The EGL_ANDROID_recordable extension is provided by the ANGLE frontend, and will always
    // say that ANativeWindow is not recordable.
    mDisplayExtensions.recordable = true;

    // All backends support specific context versions
    mDisplayExtensions.createContextBackwardsCompatible = true;

    mDisplayExtensionString = GenerateExtensionsString(mDisplayExtensions);
}

bool Display::isValidNativeWindow(EGLNativeWindowType window) const
{
    return mImplementation->isValidNativeWindow(window);
}

Error Display::validateClientBuffer(const Config *configuration,
                                    EGLenum buftype,
                                    EGLClientBuffer clientBuffer,
                                    const AttributeMap &attribs) const
{
    return mImplementation->validateClientBuffer(configuration, buftype, clientBuffer, attribs);
}

Error Display::validateImageClientBuffer(const gl::Context *context,
                                         EGLenum target,
                                         EGLClientBuffer clientBuffer,
                                         const egl::AttributeMap &attribs) const
{
    return mImplementation->validateImageClientBuffer(context, target, clientBuffer, attribs);
}

Error Display::valdiatePixmap(const Config *config,
                              EGLNativePixmapType pixmap,
                              const AttributeMap &attributes) const
{
    return mImplementation->validatePixmap(config, pixmap, attributes);
}

bool Display::isValidDisplay(const egl::Display *display)
{
    const ANGLEPlatformDisplayMap *anglePlatformDisplayMap = GetANGLEPlatformDisplayMap();
    for (const auto &displayPair : *anglePlatformDisplayMap)
    {
        if (displayPair.second == display)
        {
            return true;
        }
    }

    const DevicePlatformDisplayMap *devicePlatformDisplayMap = GetDevicePlatformDisplayMap();
    for (const auto &displayPair : *devicePlatformDisplayMap)
    {
        if (displayPair.second == display)
        {
            return true;
        }
    }

    return false;
}

bool Display::isValidNativeDisplay(EGLNativeDisplayType display)
{
    // TODO(jmadill): handle this properly
    if (display == EGL_DEFAULT_DISPLAY)
    {
        return true;
    }

#if defined(ANGLE_PLATFORM_WINDOWS) && !defined(ANGLE_ENABLE_WINDOWS_UWP)
    if (display == EGL_SOFTWARE_DISPLAY_ANGLE || display == EGL_D3D11_ELSE_D3D9_DISPLAY_ANGLE ||
        display == EGL_D3D11_ONLY_DISPLAY_ANGLE)
    {
        return true;
    }
    return (WindowFromDC(display) != nullptr);
#else
    return true;
#endif
}

void Display::initVendorString()
{
    mVendorString                = "Google Inc.";
    std::string vendorStringImpl = mImplementation->getVendorString();
    if (!vendorStringImpl.empty())
    {
        mVendorString += " (" + vendorStringImpl + ")";
    }
}

void Display::initVersionString()
{
    mVersionString = mImplementation->getVersionString(true);
}

void Display::initializeFrontendFeatures()
{
    // Enable on all Impls
    ANGLE_FEATURE_CONDITION((&mFrontendFeatures), loseContextOnOutOfMemory, true);
    ANGLE_FEATURE_CONDITION((&mFrontendFeatures), allowCompressedFormats, true);

    // No longer enable this on any Impl - crbug.com/1165751
    ANGLE_FEATURE_CONDITION((&mFrontendFeatures), scalarizeVecAndMatConstructorArgs, false);

    // Disabled by default. To reduce the risk, create a feature to enable
    // compressing pipeline cache in multi-thread pool.
    ANGLE_FEATURE_CONDITION(&mFrontendFeatures, enableCompressingPipelineCacheInThreadPool, false);

    // Disabled by default until work on the extension is complete - anglebug.com/7279.
    ANGLE_FEATURE_CONDITION(&mFrontendFeatures, emulatePixelLocalStorage, false);

    mImplementation->initializeFrontendFeatures(&mFrontendFeatures);

    rx::ApplyFeatureOverrides(&mFrontendFeatures, mState);
}

const DisplayExtensions &Display::getExtensions() const
{
    return mDisplayExtensions;
}

const std::string &Display::getExtensionString() const
{
    return mDisplayExtensionString;
}

const std::string &Display::getVendorString() const
{
    return mVendorString;
}

const std::string &Display::getVersionString() const
{
    return mVersionString;
}

std::string Display::getBackendRendererDescription() const
{
    return mImplementation->getRendererDescription();
}

std::string Display::getBackendVendorString() const
{
    return mImplementation->getVendorString();
}

std::string Display::getBackendVersionString(bool includeFullVersion) const
{
    return mImplementation->getVersionString(includeFullVersion);
}

Device *Display::getDevice() const
{
    return mDevice;
}

Surface *Display::getWGLSurface() const
{
    return mSurface;
}

gl::Version Display::getMaxSupportedESVersion() const
{
    return mImplementation->getMaxSupportedESVersion();
}

EGLint Display::programCacheGetAttrib(EGLenum attrib) const
{
    switch (attrib)
    {
        case EGL_PROGRAM_CACHE_KEY_LENGTH_ANGLE:
            return static_cast<EGLint>(BlobCache::kKeyLength);

        case EGL_PROGRAM_CACHE_SIZE_ANGLE:
            return static_cast<EGLint>(mMemoryProgramCache.entryCount());

        default:
            UNREACHABLE();
            return 0;
    }
}

Error Display::programCacheQuery(EGLint index,
                                 void *key,
                                 EGLint *keysize,
                                 void *binary,
                                 EGLint *binarysize)
{
    ASSERT(index >= 0 && index < static_cast<EGLint>(mMemoryProgramCache.entryCount()));

    const BlobCache::Key *programHash = nullptr;
    BlobCache::Value programBinary;
    // TODO(jmadill): Make this thread-safe.
    bool result =
        mMemoryProgramCache.getAt(static_cast<size_t>(index), &programHash, &programBinary);
    if (!result)
    {
        return EglBadAccess() << "Program binary not accessible.";
    }

    ASSERT(keysize && binarysize);

    if (key)
    {
        ASSERT(*keysize == static_cast<EGLint>(BlobCache::kKeyLength));
        memcpy(key, programHash->data(), BlobCache::kKeyLength);
    }

    if (binary)
    {
        // Note: we check the size here instead of in the validation code, since we need to
        // access the cache as atomically as possible. It's possible that the cache contents
        // could change between the validation size check and the retrieval.
        if (programBinary.size() > static_cast<size_t>(*binarysize))
        {
            return EglBadAccess() << "Program binary too large or changed during access.";
        }

        memcpy(binary, programBinary.data(), programBinary.size());
    }

    *binarysize = static_cast<EGLint>(programBinary.size());
    *keysize    = static_cast<EGLint>(BlobCache::kKeyLength);

    return NoError();
}

Error Display::programCachePopulate(const void *key,
                                    EGLint keysize,
                                    const void *binary,
                                    EGLint binarysize)
{
    ASSERT(keysize == static_cast<EGLint>(BlobCache::kKeyLength));

    BlobCache::Key programHash;
    memcpy(programHash.data(), key, BlobCache::kKeyLength);

    if (!mMemoryProgramCache.putBinary(programHash, reinterpret_cast<const uint8_t *>(binary),
                                       static_cast<size_t>(binarysize)))
    {
        return EglBadAccess() << "Failed to copy program binary into the cache.";
    }

    return NoError();
}

EGLint Display::programCacheResize(EGLint limit, EGLenum mode)
{
    switch (mode)
    {
        case EGL_PROGRAM_CACHE_RESIZE_ANGLE:
        {
            size_t initialSize = mMemoryProgramCache.size();
            mMemoryProgramCache.resize(static_cast<size_t>(limit));
            return static_cast<EGLint>(initialSize);
        }

        case EGL_PROGRAM_CACHE_TRIM_ANGLE:
            return static_cast<EGLint>(mMemoryProgramCache.trim(static_cast<size_t>(limit)));

        default:
            UNREACHABLE();
            return 0;
    }
}

void Display::overrideFrontendFeatures(const std::vector<std::string> &featureNames, bool enabled)
{
    mFrontendFeatures.overrideFeatures(featureNames, enabled);
}

const char *Display::queryStringi(const EGLint name, const EGLint index)
{
    const char *result = nullptr;
    switch (name)
    {
        case EGL_FEATURE_NAME_ANGLE:
            result = mFeatures[index]->name;
            break;
        case EGL_FEATURE_CATEGORY_ANGLE:
            result = angle::FeatureCategoryToString(mFeatures[index]->category);
            break;
        case EGL_FEATURE_DESCRIPTION_ANGLE:
            result = mFeatures[index]->description;
            break;
        case EGL_FEATURE_BUG_ANGLE:
            result = mFeatures[index]->bug;
            break;
        case EGL_FEATURE_STATUS_ANGLE:
            result = angle::FeatureStatusToString(mFeatures[index]->enabled);
            break;
        case EGL_FEATURE_CONDITION_ANGLE:
            result = mFeatures[index]->condition;
            break;
        default:
            UNREACHABLE();
            return nullptr;
    }
    return result;
}

EGLAttrib Display::queryAttrib(const EGLint attribute)
{
    EGLAttrib value = 0;
    switch (attribute)
    {
        case EGL_DEVICE_EXT:
            value = reinterpret_cast<EGLAttrib>(mDevice);
            break;

        case EGL_FEATURE_COUNT_ANGLE:
            value = mFeatures.size();
            break;

        default:
            UNREACHABLE();
    }
    return value;
}

angle::ScratchBuffer Display::requestScratchBuffer()
{
    return requestScratchBufferImpl(&mScratchBuffers);
}

void Display::returnScratchBuffer(angle::ScratchBuffer scratchBuffer)
{
    returnScratchBufferImpl(std::move(scratchBuffer), &mScratchBuffers);
}

angle::ScratchBuffer Display::requestZeroFilledBuffer()
{
    return requestScratchBufferImpl(&mZeroFilledBuffers);
}

void Display::returnZeroFilledBuffer(angle::ScratchBuffer zeroFilledBuffer)
{
    returnScratchBufferImpl(std::move(zeroFilledBuffer), &mZeroFilledBuffers);
}

angle::ScratchBuffer Display::requestScratchBufferImpl(
    std::vector<angle::ScratchBuffer> *bufferVector)
{
    std::lock_guard<std::mutex> lock(mScratchBufferMutex);
    if (!bufferVector->empty())
    {
        angle::ScratchBuffer buffer = std::move(bufferVector->back());
        bufferVector->pop_back();
        return buffer;
    }

    return angle::ScratchBuffer(kScratchBufferLifetime);
}

void Display::returnScratchBufferImpl(angle::ScratchBuffer scratchBuffer,
                                      std::vector<angle::ScratchBuffer> *bufferVector)
{
    std::lock_guard<std::mutex> lock(mScratchBufferMutex);
    bufferVector->push_back(std::move(scratchBuffer));
}

Error Display::handleGPUSwitch()
{
    ANGLE_TRY(mImplementation->handleGPUSwitch());
    initVendorString();
    return NoError();
}

Error Display::forceGPUSwitch(EGLint gpuIDHigh, EGLint gpuIDLow)
{
    ANGLE_TRY(mImplementation->forceGPUSwitch(gpuIDHigh, gpuIDLow));
    initVendorString();
    return NoError();
}

bool Display::supportsDmaBufFormat(EGLint format) const
{
    return mImplementation->supportsDmaBufFormat(format);
}

Error Display::queryDmaBufFormats(EGLint max_formats, EGLint *formats, EGLint *num_formats)
{
    ANGLE_TRY(mImplementation->queryDmaBufFormats(max_formats, formats, num_formats));
    return NoError();
}

Error Display::queryDmaBufModifiers(EGLint format,
                                    EGLint max_modifiers,
                                    EGLuint64KHR *modifiers,
                                    EGLBoolean *external_only,
                                    EGLint *num_modifiers)
{
    ANGLE_TRY(mImplementation->queryDmaBufModifiers(format, max_modifiers, modifiers, external_only,
                                                    num_modifiers));
    return NoError();
}

}  // namespace egl
