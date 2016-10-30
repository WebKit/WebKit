//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Display.cpp: Implements the egl::Display class, representing the abstract
// display on which graphics are drawn. Implements EGLDisplay.
// [EGL 1.4] section 2.1.2 page 3.

#include "libANGLE/Display.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <sstream>
#include <vector>

#include <platform/Platform.h>
#include <EGL/eglext.h>

#include "common/debug.h"
#include "common/mathutil.h"
#include "common/platform.h"
#include "common/utilities.h"
#include "libANGLE/Context.h"
#include "libANGLE/Device.h"
#include "libANGLE/histogram_macros.h"
#include "libANGLE/Image.h"
#include "libANGLE/Surface.h"
#include "libANGLE/Stream.h"
#include "libANGLE/renderer/DisplayImpl.h"
#include "libANGLE/renderer/ImageImpl.h"
#include "third_party/trace_event/trace_event.h"

#if defined(ANGLE_ENABLE_D3D9) || defined(ANGLE_ENABLE_D3D11)
#   include "libANGLE/renderer/d3d/DisplayD3D.h"
#endif

#if defined(ANGLE_ENABLE_OPENGL)
#   if defined(ANGLE_PLATFORM_WINDOWS)
#       include "libANGLE/renderer/gl/wgl/DisplayWGL.h"
#   elif defined(ANGLE_USE_X11)
#       include "libANGLE/renderer/gl/glx/DisplayGLX.h"
#   elif defined(ANGLE_PLATFORM_APPLE)
#       include "libANGLE/renderer/gl/cgl/DisplayCGL.h"
#   elif defined(ANGLE_USE_OZONE)
#       include "libANGLE/renderer/gl/egl/ozone/DisplayOzone.h"
#   elif defined(ANGLE_PLATFORM_ANDROID)
#       include "libANGLE/renderer/gl/egl/android/DisplayAndroid.h"
#   else
#       error Unsupported OpenGL platform.
#   endif
#endif

#if defined(ANGLE_ENABLE_NULL)
#include "libANGLE/renderer/null/DisplayNULL.h"
#endif

namespace egl
{

namespace
{

class DefaultPlatform : public angle::Platform
{
public:
    DefaultPlatform() {}
    ~DefaultPlatform() override {}
};

DefaultPlatform *defaultPlatform = nullptr;

void InitDefaultPlatformImpl()
{
    if (ANGLEPlatformCurrent() == nullptr)
    {
        if (defaultPlatform == nullptr)
        {
            defaultPlatform = new DefaultPlatform();
        }

        ANGLEPlatformInitialize(defaultPlatform);
    }
}

typedef std::map<EGLNativeWindowType, Surface*> WindowSurfaceMap;
// Get a map of all EGL window surfaces to validate that no window has more than one EGL surface
// associated with it.
static WindowSurfaceMap *GetWindowSurfaces()
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
    static WindowSurfaceMap windowSurfaces;
#pragma clang diagnostic pop
    return &windowSurfaces;
}

typedef std::map<EGLNativeDisplayType, Display *> ANGLEPlatformDisplayMap;
static ANGLEPlatformDisplayMap *GetANGLEPlatformDisplayMap()
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
    static ANGLEPlatformDisplayMap displays;
#pragma clang diagnostic pop
    return &displays;
}

typedef std::map<Device *, Display *> DevicePlatformDisplayMap;
static DevicePlatformDisplayMap *GetDevicePlatformDisplayMap()
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
    static DevicePlatformDisplayMap displays;
#pragma clang diagnostic pop
    return &displays;
}

rx::DisplayImpl *CreateDisplayFromDevice(Device *eglDevice)
{
    rx::DisplayImpl *impl = nullptr;

    switch (eglDevice->getType())
    {
#if defined(ANGLE_ENABLE_D3D11)
        case EGL_D3D11_DEVICE_ANGLE:
            impl = new rx::DisplayD3D();
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

rx::DisplayImpl *CreateDisplayFromAttribs(const AttributeMap &attribMap)
{
    rx::DisplayImpl *impl = nullptr;
    EGLAttrib displayType =
        attribMap.get(EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE);
    switch (displayType)
    {
      case EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE:
#if defined(ANGLE_ENABLE_D3D9) || defined(ANGLE_ENABLE_D3D11)
        // Default to D3D displays
        impl = new rx::DisplayD3D();
#elif defined(ANGLE_USE_X11)
        impl = new rx::DisplayGLX();
#elif defined(ANGLE_PLATFORM_APPLE)
        impl = new rx::DisplayCGL();
#elif defined(ANGLE_USE_OZONE)
        impl = new rx::DisplayOzone();
#elif defined(ANGLE_PLATFORM_ANDROID)
        impl = new rx::DisplayAndroid();
#else
        // No display available
        UNREACHABLE();
#endif
        break;

      case EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE:
      case EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE:
#if defined(ANGLE_ENABLE_D3D9) || defined(ANGLE_ENABLE_D3D11)
        impl = new rx::DisplayD3D();
#else
        // A D3D display was requested on a platform that doesn't support it
        UNREACHABLE();
#endif
        break;

      case EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE:
#if defined(ANGLE_ENABLE_OPENGL)
#if defined(ANGLE_PLATFORM_WINDOWS)
        impl = new rx::DisplayWGL();
#elif defined(ANGLE_USE_X11)
        impl = new rx::DisplayGLX();
#elif defined(ANGLE_PLATFORM_APPLE)
        impl = new rx::DisplayCGL();
#elif defined(ANGLE_USE_OZONE)
        // This might work but has never been tried, so disallow for now.
        impl = nullptr;
#elif defined(ANGLE_PLATFORM_ANDROID)
        // No GL support on this platform, fail display creation.
        impl = nullptr;
#else
#error Unsupported OpenGL platform.
#endif
#else
        UNREACHABLE();
#endif
        break;

#if defined(ANGLE_ENABLE_OPENGL)
      case EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE:
#if defined(ANGLE_PLATFORM_WINDOWS)
        impl = new rx::DisplayWGL();
#elif defined(ANGLE_USE_X11)
        impl = new rx::DisplayGLX();
#elif defined(ANGLE_USE_OZONE)
        impl = new rx::DisplayOzone();
#elif defined(ANGLE_PLATFORM_ANDROID)
        impl = new rx::DisplayAndroid();
#else
        // No GLES support on this platform, fail display creation.
        impl = nullptr;
#endif
        break;
#endif

#if defined(ANGLE_ENABLE_NULL)
      case EGL_PLATFORM_ANGLE_TYPE_NULL_ANGLE:
          impl = new rx::DisplayNULL();
          break;
#endif

      default:
        UNREACHABLE();
        break;
    }

    return impl;
}

}

Display *Display::GetDisplayFromAttribs(void *native_display, const AttributeMap &attribMap)
{
    // Initialize the global platform if not already
    InitDefaultPlatformImpl();

    Display *display = nullptr;

    EGLNativeDisplayType displayId = reinterpret_cast<EGLNativeDisplayType>(native_display);

    ANGLEPlatformDisplayMap *displays            = GetANGLEPlatformDisplayMap();
    ANGLEPlatformDisplayMap::const_iterator iter = displays->find(displayId);
    if (iter != displays->end())
    {
        display = iter->second;
    }

    if (display == nullptr)
    {
        // Validate the native display
        if (!Display::isValidNativeDisplay(displayId))
        {
            return NULL;
        }

        display = new Display(EGL_PLATFORM_ANGLE_ANGLE, displayId, nullptr);
        displays->insert(std::make_pair(displayId, display));
    }

    // Apply new attributes if the display is not initialized yet.
    if (!display->isInitialized())
    {
        rx::DisplayImpl *impl = CreateDisplayFromAttribs(attribMap);
        if (impl == nullptr)
        {
            // No valid display implementation for these attributes
            return nullptr;
        }

        display->setAttributes(impl, attribMap);
    }

    return display;
}

Display *Display::GetDisplayFromDevice(void *native_display)
{
    // Initialize the global platform if not already
    InitDefaultPlatformImpl();

    Display *display = nullptr;

    Device *eglDevice = reinterpret_cast<Device *>(native_display);
    ASSERT(Device::IsValidDevice(eglDevice));

    ANGLEPlatformDisplayMap *anglePlatformDisplays   = GetANGLEPlatformDisplayMap();
    DevicePlatformDisplayMap *devicePlatformDisplays = GetDevicePlatformDisplayMap();

    // First see if this eglDevice is in use by a Display created using ANGLE platform
    for (auto &displayMapEntry : *anglePlatformDisplays)
    {
        egl::Display *iterDisplay = displayMapEntry.second;
        if (iterDisplay->getDevice() == eglDevice)
        {
            display = iterDisplay;
        }
    }

    if (display == nullptr)
    {
        // See if the eglDevice is in use by a Display created using the DEVICE platform
        DevicePlatformDisplayMap::const_iterator iter = devicePlatformDisplays->find(eglDevice);
        if (iter != devicePlatformDisplays->end())
        {
            display = iter->second;
        }
    }

    if (display == nullptr)
    {
        // Otherwise create a new Display
        display = new Display(EGL_PLATFORM_DEVICE_EXT, 0, eglDevice);
        devicePlatformDisplays->insert(std::make_pair(eglDevice, display));
    }

    // Apply new attributes if the display is not initialized yet.
    if (!display->isInitialized())
    {
        rx::DisplayImpl *impl = CreateDisplayFromDevice(eglDevice);
        display->setAttributes(impl, egl::AttributeMap());
    }

    return display;
}

Display::Display(EGLenum platform, EGLNativeDisplayType displayId, Device *eglDevice)
    : mImplementation(nullptr),
      mDisplayId(displayId),
      mAttributeMap(),
      mConfigSet(),
      mContextSet(),
      mStreamSet(),
      mInitialized(false),
      mDeviceLost(false),
      mCaps(),
      mDisplayExtensions(),
      mDisplayExtensionString(),
      mVendorString(),
      mDevice(eglDevice),
      mPlatform(platform)
{
}

Display::~Display()
{
    terminate();

    if (mPlatform == EGL_PLATFORM_ANGLE_ANGLE)
    {
        ANGLEPlatformDisplayMap *displays      = GetANGLEPlatformDisplayMap();
        ANGLEPlatformDisplayMap::iterator iter = displays->find(mDisplayId);
        if (iter != displays->end())
        {
            displays->erase(iter);
        }
    }
    else if (mPlatform == EGL_PLATFORM_DEVICE_EXT)
    {
        DevicePlatformDisplayMap *displays      = GetDevicePlatformDisplayMap();
        DevicePlatformDisplayMap::iterator iter = displays->find(mDevice);
        if (iter != displays->end())
        {
            displays->erase(iter);
        }
    }
    else
    {
        UNREACHABLE();
    }

    SafeDelete(mDevice);
    SafeDelete(mImplementation);
}

void Display::setAttributes(rx::DisplayImpl *impl, const AttributeMap &attribMap)
{
    ASSERT(!mInitialized);

    ASSERT(impl != nullptr);
    SafeDelete(mImplementation);
    mImplementation = impl;

    mAttributeMap = attribMap;
}

Error Display::initialize()
{
    // Re-initialize default platform if it's needed
    InitDefaultPlatformImpl();

    SCOPED_ANGLE_HISTOGRAM_TIMER("GPU.ANGLE.DisplayInitializeMS");
    TRACE_EVENT0("gpu.angle", "egl::Display::initialize");

    ASSERT(mImplementation != nullptr);

    if (isInitialized())
    {
        return egl::Error(EGL_SUCCESS);
    }

    Error error = mImplementation->initialize(this);
    if (error.isError())
    {
        // Log extended error message here
        std::stringstream errorStream;
        errorStream << "ANGLE Display::initialize error " << error.getID() << ": "
                    << error.getMessage();
        ANGLEPlatformCurrent()->logError(errorStream.str().c_str());
        return error;
    }

    mCaps = mImplementation->getCaps();

    mConfigSet = mImplementation->generateConfigs();
    if (mConfigSet.size() == 0)
    {
        mImplementation->terminate();
        return Error(EGL_NOT_INITIALIZED);
    }

    initDisplayExtensions();
    initVendorString();

    // Populate the Display's EGLDeviceEXT if the Display wasn't created using one
    if (mPlatform != EGL_PLATFORM_DEVICE_EXT)
    {
        if (mDisplayExtensions.deviceQuery)
        {
            rx::DeviceImpl *impl = nullptr;
            ANGLE_TRY(mImplementation->getDevice(&impl));
            ANGLE_TRY(Device::CreateDevice(this, impl, &mDevice));
        }
        else
        {
            mDevice = nullptr;
        }
    }
    else
    {
        // For EGL_PLATFORM_DEVICE_EXT, mDevice should always be populated using
        // an external device
        ASSERT(mDevice != nullptr);
    }

    mInitialized = true;

    return egl::Error(EGL_SUCCESS);
}

void Display::terminate()
{
    makeCurrent(nullptr, nullptr, nullptr);

    while (!mContextSet.empty())
    {
        destroyContext(*mContextSet.begin());
    }

    while (!mImageSet.empty())
    {
        destroyImage(*mImageSet.begin());
    }

    while (!mStreamSet.empty())
    {
        destroyStream(*mStreamSet.begin());
    }

    while (!mImplementation->getSurfaceSet().empty())
    {
        destroySurface(*mImplementation->getSurfaceSet().begin());
    }

    mConfigSet.clear();

    if (mDevice != nullptr && mDevice->getOwningDisplay() != nullptr)
    {
        // Don't delete the device if it was created externally using eglCreateDeviceANGLE
        // We also shouldn't set it to null in case eglInitialize() is called again later
        SafeDelete(mDevice);
    }

    mImplementation->terminate();

    mInitialized = false;

    // Never de-init default platform.. terminate is not that final.
}

std::vector<const Config*> Display::getConfigs(const egl::AttributeMap &attribs) const
{
    return mConfigSet.filter(attribs);
}

bool Display::getConfigAttrib(const Config *configuration, EGLint attribute, EGLint *value)
{
    switch (attribute)
    {
      case EGL_BUFFER_SIZE:               *value = configuration->bufferSize;             break;
      case EGL_ALPHA_SIZE:                *value = configuration->alphaSize;              break;
      case EGL_BLUE_SIZE:                 *value = configuration->blueSize;               break;
      case EGL_GREEN_SIZE:                *value = configuration->greenSize;              break;
      case EGL_RED_SIZE:                  *value = configuration->redSize;                break;
      case EGL_DEPTH_SIZE:                *value = configuration->depthSize;              break;
      case EGL_STENCIL_SIZE:              *value = configuration->stencilSize;            break;
      case EGL_CONFIG_CAVEAT:             *value = configuration->configCaveat;           break;
      case EGL_CONFIG_ID:                 *value = configuration->configID;               break;
      case EGL_LEVEL:                     *value = configuration->level;                  break;
      case EGL_NATIVE_RENDERABLE:         *value = configuration->nativeRenderable;       break;
      case EGL_NATIVE_VISUAL_ID:          *value = configuration->nativeVisualID;         break;
      case EGL_NATIVE_VISUAL_TYPE:        *value = configuration->nativeVisualType;       break;
      case EGL_SAMPLES:                   *value = configuration->samples;                break;
      case EGL_SAMPLE_BUFFERS:            *value = configuration->sampleBuffers;          break;
      case EGL_SURFACE_TYPE:              *value = configuration->surfaceType;            break;
      case EGL_TRANSPARENT_TYPE:          *value = configuration->transparentType;        break;
      case EGL_TRANSPARENT_BLUE_VALUE:    *value = configuration->transparentBlueValue;   break;
      case EGL_TRANSPARENT_GREEN_VALUE:   *value = configuration->transparentGreenValue;  break;
      case EGL_TRANSPARENT_RED_VALUE:     *value = configuration->transparentRedValue;    break;
      case EGL_BIND_TO_TEXTURE_RGB:       *value = configuration->bindToTextureRGB;       break;
      case EGL_BIND_TO_TEXTURE_RGBA:      *value = configuration->bindToTextureRGBA;      break;
      case EGL_MIN_SWAP_INTERVAL:         *value = configuration->minSwapInterval;        break;
      case EGL_MAX_SWAP_INTERVAL:         *value = configuration->maxSwapInterval;        break;
      case EGL_LUMINANCE_SIZE:            *value = configuration->luminanceSize;          break;
      case EGL_ALPHA_MASK_SIZE:           *value = configuration->alphaMaskSize;          break;
      case EGL_COLOR_BUFFER_TYPE:         *value = configuration->colorBufferType;        break;
      case EGL_RENDERABLE_TYPE:           *value = configuration->renderableType;         break;
      case EGL_MATCH_NATIVE_PIXMAP:       *value = false; UNIMPLEMENTED();                break;
      case EGL_CONFORMANT:                *value = configuration->conformant;             break;
      case EGL_MAX_PBUFFER_WIDTH:         *value = configuration->maxPBufferWidth;        break;
      case EGL_MAX_PBUFFER_HEIGHT:        *value = configuration->maxPBufferHeight;       break;
      case EGL_MAX_PBUFFER_PIXELS:        *value = configuration->maxPBufferPixels;       break;

      case EGL_OPTIMAL_SURFACE_ORIENTATION_ANGLE:
          if (!getExtensions().surfaceOrientation)
          {
              return false;
          }
          *value = configuration->optimalOrientation;
          break;

      default:
        return false;
    }

    return true;
}

Error Display::createWindowSurface(const Config *configuration, EGLNativeWindowType window, const AttributeMap &attribs,
                                   Surface **outSurface)
{
    if (mImplementation->testDeviceLost())
    {
        ANGLE_TRY(restoreLostDevice());
    }

    std::unique_ptr<Surface> surface(
        new WindowSurface(mImplementation, configuration, window, attribs));
    ANGLE_TRY(surface->initialize());

    ASSERT(outSurface != nullptr);
    *outSurface = surface.release();
    mImplementation->getSurfaceSet().insert(*outSurface);

    WindowSurfaceMap *windowSurfaces = GetWindowSurfaces();
    ASSERT(windowSurfaces && windowSurfaces->find(window) == windowSurfaces->end());
    windowSurfaces->insert(std::make_pair(window, *outSurface));

    return egl::Error(EGL_SUCCESS);
}

Error Display::createPbufferSurface(const Config *configuration, const AttributeMap &attribs, Surface **outSurface)
{
    ASSERT(isInitialized());

    if (mImplementation->testDeviceLost())
    {
        ANGLE_TRY(restoreLostDevice());
    }

    std::unique_ptr<Surface> surface(new PbufferSurface(mImplementation, configuration, attribs));
    ANGLE_TRY(surface->initialize());

    ASSERT(outSurface != nullptr);
    *outSurface = surface.release();
    mImplementation->getSurfaceSet().insert(*outSurface);

    return egl::Error(EGL_SUCCESS);
}

Error Display::createPbufferFromClientBuffer(const Config *configuration, EGLClientBuffer shareHandle,
                                             const AttributeMap &attribs, Surface **outSurface)
{
    ASSERT(isInitialized());

    if (mImplementation->testDeviceLost())
    {
        ANGLE_TRY(restoreLostDevice());
    }

    std::unique_ptr<Surface> surface(
        new PbufferSurface(mImplementation, configuration, shareHandle, attribs));
    ANGLE_TRY(surface->initialize());

    ASSERT(outSurface != nullptr);
    *outSurface = surface.release();
    mImplementation->getSurfaceSet().insert(*outSurface);

    return egl::Error(EGL_SUCCESS);
}

Error Display::createPixmapSurface(const Config *configuration, NativePixmapType nativePixmap, const AttributeMap &attribs,
                                   Surface **outSurface)
{
    ASSERT(isInitialized());

    if (mImplementation->testDeviceLost())
    {
        ANGLE_TRY(restoreLostDevice());
    }

    std::unique_ptr<Surface> surface(
        new PixmapSurface(mImplementation, configuration, nativePixmap, attribs));
    ANGLE_TRY(surface->initialize());

    ASSERT(outSurface != nullptr);
    *outSurface = surface.release();
    mImplementation->getSurfaceSet().insert(*outSurface);

    return egl::Error(EGL_SUCCESS);
}

Error Display::createImage(gl::Context *context,
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
        sibling = context->getTexture(egl_gl::EGLClientBufferToGLObjectHandle(buffer));
    }
    else if (IsRenderbufferTarget(target))
    {
        sibling = context->getRenderbuffer(egl_gl::EGLClientBufferToGLObjectHandle(buffer));
    }
    else
    {
        UNREACHABLE();
    }
    ASSERT(sibling != nullptr);

    rx::ImageImpl *imageImpl = mImplementation->createImage(target, sibling, attribs);
    ASSERT(imageImpl != nullptr);

    ANGLE_TRY(imageImpl->initialize());

    Image *image = new Image(imageImpl, target, sibling, attribs);

    ASSERT(outImage != nullptr);
    *outImage = image;

    // Add this image to the list of all images and hold a ref to it.
    image->addRef();
    mImageSet.insert(image);

    return egl::Error(EGL_SUCCESS);
}

Error Display::createStream(const AttributeMap &attribs, Stream **outStream)
{
    ASSERT(isInitialized());

    Stream *stream = new Stream(this, attribs);

    ASSERT(stream != nullptr);
    mStreamSet.insert(stream);

    ASSERT(outStream != nullptr);
    *outStream = stream;

    return Error(EGL_SUCCESS);
}

Error Display::createContext(const Config *configuration, gl::Context *shareContext, const AttributeMap &attribs,
                             gl::Context **outContext)
{
    ASSERT(isInitialized());

    if (mImplementation->testDeviceLost())
    {
        ANGLE_TRY(restoreLostDevice());
    }

    gl::Context *context = new gl::Context(mImplementation, configuration, shareContext, attribs);

    ASSERT(context != nullptr);
    mContextSet.insert(context);

    ASSERT(outContext != nullptr);
    *outContext = context;
    return egl::Error(EGL_SUCCESS);
}

Error Display::makeCurrent(egl::Surface *drawSurface, egl::Surface *readSurface, gl::Context *context)
{
    ANGLE_TRY(mImplementation->makeCurrent(drawSurface, readSurface, context));

    if (context != nullptr && drawSurface != nullptr)
    {
        ASSERT(readSurface == drawSurface);
        context->makeCurrent(drawSurface);
    }

    return egl::Error(EGL_SUCCESS);
}

Error Display::restoreLostDevice()
{
    for (ContextSet::iterator ctx = mContextSet.begin(); ctx != mContextSet.end(); ctx++)
    {
        if ((*ctx)->isResetNotificationEnabled())
        {
            // If reset notifications have been requested, application must delete all contexts first
            return Error(EGL_CONTEXT_LOST);
        }
    }

    return mImplementation->restoreLostDevice();
}

void Display::destroySurface(Surface *surface)
{
    if (surface->getType() == EGL_WINDOW_BIT)
    {
        WindowSurfaceMap *windowSurfaces = GetWindowSurfaces();
        ASSERT(windowSurfaces);

        bool surfaceRemoved = false;
        for (WindowSurfaceMap::iterator iter = windowSurfaces->begin(); iter != windowSurfaces->end(); iter++)
        {
            if (iter->second == surface)
            {
                windowSurfaces->erase(iter);
                surfaceRemoved = true;
                break;
            }
        }

        ASSERT(surfaceRemoved);
        UNUSED_ASSERTION_VARIABLE(surfaceRemoved);
    }

    mImplementation->destroySurface(surface);
}

void Display::destroyImage(egl::Image *image)
{
    auto iter = mImageSet.find(image);
    ASSERT(iter != mImageSet.end());
    (*iter)->release();
    mImageSet.erase(iter);
}

void Display::destroyStream(egl::Stream *stream)
{
    mStreamSet.erase(stream);
    SafeDelete(stream);
}

void Display::destroyContext(gl::Context *context)
{
    mContextSet.erase(context);
    SafeDelete(context);
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

    for (ContextSet::iterator context = mContextSet.begin(); context != mContextSet.end(); context++)
    {
        (*context)->markContextLost();
    }

    mDeviceLost = true;
}

Error Display::waitClient() const
{
    return mImplementation->waitClient();
}

Error Display::waitNative(EGLint engine, egl::Surface *drawSurface, egl::Surface *readSurface) const
{
    return mImplementation->waitNative(engine, drawSurface, readSurface);
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

bool Display::isValidContext(gl::Context *context) const
{
    return mContextSet.find(context) != mContextSet.end();
}

bool Display::isValidSurface(Surface *surface) const
{
    return mImplementation->getSurfaceSet().find(surface) != mImplementation->getSurfaceSet().end();
}

bool Display::isValidImage(const Image *image) const
{
    return mImageSet.find(const_cast<Image *>(image)) != mImageSet.end();
}

bool Display::isValidStream(const Stream *stream) const
{
    return mStreamSet.find(const_cast<Stream *>(stream)) != mStreamSet.end();
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
    extensions.platformBase = true;
    extensions.platformANGLE = true;

#if defined(ANGLE_ENABLE_D3D9) || defined(ANGLE_ENABLE_D3D11)
    extensions.platformANGLED3D = true;
    extensions.platformDevice   = true;
#endif

#if defined(ANGLE_ENABLE_OPENGL)
    extensions.platformANGLEOpenGL = true;
#endif

#if defined(ANGLE_ENABLE_NULL)
    extensions.platformANGLENULL = true;
#endif

#if defined(ANGLE_ENABLE_D3D11)
    extensions.deviceCreation      = true;
    extensions.deviceCreationD3D11 = true;
    extensions.experimentalPresentPath = true;
#endif

#if defined(ANGLE_USE_X11)
    extensions.x11Visual = true;
#endif

    extensions.clientGetAllProcAddresses = true;

    return extensions;
}

template <typename T>
static std::string GenerateExtensionsString(const T &extensions)
{
    std::vector<std::string> extensionsVector = extensions.getStrings();

    std::ostringstream stream;
    std::copy(extensionsVector.begin(), extensionsVector.end(), std::ostream_iterator<std::string>(stream, " "));
    return stream.str();
}

const ClientExtensions &Display::getClientExtensions()
{
    static const ClientExtensions clientExtensions = GenerateClientExtensions();
    return clientExtensions;
}

const std::string &Display::getClientExtensionString()
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
    static const std::string clientExtensionsString = GenerateExtensionsString(getClientExtensions());
#pragma clang diagnostic pop
    return clientExtensionsString;
}

void Display::initDisplayExtensions()
{
    mDisplayExtensions = mImplementation->getExtensions();

    // Some extensions are always available because they are implemented in the EGL layer.
    mDisplayExtensions.createContext        = true;
    mDisplayExtensions.createContextNoError = true;
    mDisplayExtensions.createContextWebGLCompatibility = true;
    mDisplayExtensions.createContextBindGeneratesResource = true;

    // Force EGL_KHR_get_all_proc_addresses on.
    mDisplayExtensions.getAllProcAddresses = true;

    mDisplayExtensionString = GenerateExtensionsString(mDisplayExtensions);
}

bool Display::isValidNativeWindow(EGLNativeWindowType window) const
{
    return mImplementation->isValidNativeWindow(window);
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

#if defined(ANGLE_PLATFORM_WINDOWS) && !defined(ANGLE_ENABLE_WINDOWS_STORE)
    if (display == EGL_SOFTWARE_DISPLAY_ANGLE ||
        display == EGL_D3D11_ELSE_D3D9_DISPLAY_ANGLE ||
        display == EGL_D3D11_ONLY_DISPLAY_ANGLE)
    {
        return true;
    }
    return (WindowFromDC(display) != NULL);
#else
    return true;
#endif
}

void Display::initVendorString()
{
    mVendorString = mImplementation->getVendorString();
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

Device *Display::getDevice() const
{
    return mDevice;
}

gl::Version Display::getMaxSupportedESVersion() const
{
    return mImplementation->getMaxSupportedESVersion();
}
}
