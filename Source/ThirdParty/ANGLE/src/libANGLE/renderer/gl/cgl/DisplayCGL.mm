//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// DisplayCGL.mm: CGL implementation of egl::Display

#include "common/platform.h"

#if defined(ANGLE_PLATFORM_MACOS) || defined(ANGLE_PLATFORM_MACCATALYST)

#    include "libANGLE/renderer/gl/cgl/DisplayCGL.h"

#    import <Cocoa/Cocoa.h>
#    include <EGL/eglext.h>
#    include <dlfcn.h>

#    include "common/debug.h"
#    include "gpu_info_util/SystemInfo.h"
#    include "libANGLE/Display.h"
#    include "libANGLE/Error.h"
#    include "libANGLE/renderer/gl/cgl/ContextCGL.h"
#    include "libANGLE/renderer/gl/cgl/DeviceCGL.h"
#    include "libANGLE/renderer/gl/cgl/IOSurfaceSurfaceCGL.h"
#    include "libANGLE/renderer/gl/cgl/PbufferSurfaceCGL.h"
#    include "libANGLE/renderer/gl/cgl/RendererCGL.h"
#    include "libANGLE/renderer/gl/cgl/WindowSurfaceCGL.h"
#    include "platform/PlatformMethods.h"

#    include "libANGLE/renderer/gl/cgl/CGLFunctions.h"

namespace
{

const char *kDefaultOpenGLDylibName =
    "/System/Library/Frameworks/OpenGL.framework/Libraries/libGL.dylib";
const char *kFallbackOpenGLDylibName = "GL";

}

namespace rx
{

namespace
{

// Global IOKit I/O registryID that can match a GPU across process boundaries.
using IORegistryGPUID = uint64_t;

// Code from WebKit to set an OpenGL context to use a particular GPU by ID.
// https://trac.webkit.org/browser/webkit/trunk/Source/WebCore/platform/graphics/cocoa/GraphicsContextGLOpenGLCocoa.mm
// Used with permission.
static void SetGPUByRegistryID(CGLContextObj contextObj,
                               CGLPixelFormatObj pixelFormatObj,
                               IORegistryGPUID preferredGPUID)
{
    if (@available(macOS 10.13, *))
    {
        // When a process does not have access to the WindowServer (as with Chromium's GPU process
        // and WebKit's WebProcess), there is no way for OpenGL to tell which GPU is connected to a
        // display. On 10.13+, find the virtual screen that corresponds to the preferred GPU by its
        // registryID. CGLSetVirtualScreen can then be used to tell OpenGL which GPU it should be
        // using.

        if (!contextObj || !preferredGPUID)
            return;

        GLint virtualScreenCount = 0;
        CGLError error = CGLDescribePixelFormat(pixelFormatObj, 0, kCGLPFAVirtualScreenCount,
                                                &virtualScreenCount);
        ASSERT(error == kCGLNoError);

        GLint firstAcceleratedScreen = -1;

        for (GLint virtualScreen = 0; virtualScreen < virtualScreenCount; ++virtualScreen)
        {
            GLint displayMask = 0;
            error = CGLDescribePixelFormat(pixelFormatObj, virtualScreen, kCGLPFADisplayMask,
                                           &displayMask);
            ASSERT(error == kCGLNoError);

            auto gpuID = angle::GetGpuIDFromOpenGLDisplayMask(displayMask);

            if (gpuID == preferredGPUID)
            {
                error = CGLSetVirtualScreen(contextObj, virtualScreen);
                ASSERT(error == kCGLNoError);
                fprintf(stderr, "Context (%p) set to GPU with ID: (%lld).", contextObj, gpuID);
                return;
            }

            if (firstAcceleratedScreen < 0)
            {
                GLint isAccelerated = 0;
                error = CGLDescribePixelFormat(pixelFormatObj, virtualScreen, kCGLPFAAccelerated,
                                               &isAccelerated);
                ASSERT(error == kCGLNoError);
                if (isAccelerated)
                    firstAcceleratedScreen = virtualScreen;
            }
        }

        // No registryID match found; set to first hardware-accelerated virtual screen.
        if (firstAcceleratedScreen >= 0)
        {
            error = CGLSetVirtualScreen(contextObj, firstAcceleratedScreen);
            ASSERT(error == kCGLNoError);
        }
    }
}

}  // anonymous namespace

class FunctionsGLCGL : public FunctionsGL
{
  public:
    FunctionsGLCGL(void *dylibHandle) : mDylibHandle(dylibHandle) {}

    ~FunctionsGLCGL() override { dlclose(mDylibHandle); }

  private:
    void *loadProcAddress(const std::string &function) const override
    {
        return dlsym(mDylibHandle, function.c_str());
    }

    void *mDylibHandle;
};

DisplayCGL::DisplayCGL(const egl::DisplayState &state)
    : DisplayGL(state),
      mEGLDisplay(nullptr),
      mContext(nullptr),
      mPixelFormat(nullptr),
      mSupportsGPUSwitching(false),
      mCurrentGPUID(0),
      mDiscreteGPUPixelFormat(nullptr),
      mDiscreteGPURefs(0),
      mLastDiscreteGPUUnrefTime(0.0)
{}

DisplayCGL::~DisplayCGL() {}

egl::Error DisplayCGL::initialize(egl::Display *display)
{
    mEGLDisplay = display;

    angle::SystemInfo info;
    if (!angle::GetSystemInfo(&info))
    {
        return egl::EglNotInitialized() << "Unable to query ANGLE's SystemInfo.";
    }

    // This code implements the effect of the
    // disableGPUSwitchingSupport workaround in FeaturesGL.
    mSupportsGPUSwitching = info.isMacSwitchable && !info.hasNVIDIAGPU();

    {
        // TODO(cwallez) investigate which pixel format we want
        std::vector<CGLPixelFormatAttribute> attribs;
        attribs.push_back(kCGLPFAOpenGLProfile);
        attribs.push_back(static_cast<CGLPixelFormatAttribute>(kCGLOGLPVersion_3_2_Core));
        attribs.push_back(kCGLPFAAllowOfflineRenderers);
        attribs.push_back(static_cast<CGLPixelFormatAttribute>(0));
        GLint nVirtualScreens = 0;
        CGLChoosePixelFormat(attribs.data(), &mPixelFormat, &nVirtualScreens);

        if (mPixelFormat == nullptr)
        {
            return egl::EglNotInitialized() << "Could not create the context's pixel format.";
        }
    }

    CGLCreateContext(mPixelFormat, nullptr, &mContext);
    if (mContext == nullptr)
    {
        return egl::EglNotInitialized() << "Could not create the CGL context.";
    }

    if (mSupportsGPUSwitching)
    {
        // Determine the currently active GPU on the system.
        mCurrentGPUID = angle::GetGpuIDFromDisplayID(kCGDirectMainDisplay);
    }

    CGLSetCurrentContext(mContext);

    mCurrentContexts[std::this_thread::get_id()] = mContext;

    // There is no equivalent getProcAddress in CGL so we open the dylib directly
    void *handle = dlopen(kDefaultOpenGLDylibName, RTLD_NOW);
    if (!handle)
    {
        handle = dlopen(kFallbackOpenGLDylibName, RTLD_NOW);
    }
    if (!handle)
    {
        return egl::EglNotInitialized() << "Could not open the OpenGL Framework.";
    }

    std::unique_ptr<FunctionsGL> functionsGL(new FunctionsGLCGL(handle));
    functionsGL->initialize(display->getAttributeMap());

    mRenderer.reset(new RendererCGL(std::move(functionsGL), display->getAttributeMap(), this));

    const gl::Version &maxVersion = mRenderer->getMaxSupportedESVersion();
    if (maxVersion < gl::Version(2, 0))
    {
        return egl::EglNotInitialized() << "OpenGL ES 2.0 is not supportable.";
    }

    return DisplayGL::initialize(display);
}

void DisplayCGL::terminate()
{
    DisplayGL::terminate();

    mRenderer.reset();
    if (mPixelFormat != nullptr)
    {
        CGLDestroyPixelFormat(mPixelFormat);
        mPixelFormat = nullptr;
    }
    mCurrentContexts.clear();
    if (mContext != nullptr)
    {
        CGLSetCurrentContext(nullptr);
        CGLDestroyContext(mContext);
        mContext = nullptr;
    }
    if (mDiscreteGPUPixelFormat != nullptr)
    {
        CGLDestroyPixelFormat(mDiscreteGPUPixelFormat);
        mDiscreteGPUPixelFormat   = nullptr;
        mLastDiscreteGPUUnrefTime = 0.0;
    }
}

egl::Error DisplayCGL::makeCurrent(egl::Surface *drawSurface,
                                   egl::Surface *readSurface,
                                   gl::Context *context)
{
    checkDiscreteGPUStatus();
    // If the thread that's calling makeCurrent does not have the correct
    // context current (either mContext or 0), we need to set it current.
    CGLContextObj newContext = 0;
    if (context)
    {
        newContext = mContext;
    }
    if (newContext != mCurrentContexts[std::this_thread::get_id()])
    {
        CGLSetCurrentContext(newContext);
        mCurrentContexts[std::this_thread::get_id()] = newContext;
    }
    return DisplayGL::makeCurrent(drawSurface, readSurface, context);
}

SurfaceImpl *DisplayCGL::createWindowSurface(const egl::SurfaceState &state,
                                             EGLNativeWindowType window,
                                             const egl::AttributeMap &attribs)
{
    return new WindowSurfaceCGL(state, mRenderer.get(), window, mContext);
}

SurfaceImpl *DisplayCGL::createPbufferSurface(const egl::SurfaceState &state,
                                              const egl::AttributeMap &attribs)
{
    EGLint width  = static_cast<EGLint>(attribs.get(EGL_WIDTH, 0));
    EGLint height = static_cast<EGLint>(attribs.get(EGL_HEIGHT, 0));
    return new PbufferSurfaceCGL(state, mRenderer.get(), width, height);
}

SurfaceImpl *DisplayCGL::createPbufferFromClientBuffer(const egl::SurfaceState &state,
                                                       EGLenum buftype,
                                                       EGLClientBuffer clientBuffer,
                                                       const egl::AttributeMap &attribs)
{
    ASSERT(buftype == EGL_IOSURFACE_ANGLE);

    return new IOSurfaceSurfaceCGL(state, mContext, clientBuffer, attribs);
}

SurfaceImpl *DisplayCGL::createPixmapSurface(const egl::SurfaceState &state,
                                             NativePixmapType nativePixmap,
                                             const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return nullptr;
}

ContextImpl *DisplayCGL::createContext(const gl::State &state,
                                       gl::ErrorSet *errorSet,
                                       const egl::Config *configuration,
                                       const gl::Context *shareContext,
                                       const egl::AttributeMap &attribs)
{
    bool usesDiscreteGPU = false;

    if (attribs.get(EGL_POWER_PREFERENCE_ANGLE, EGL_LOW_POWER_ANGLE) == EGL_HIGH_POWER_ANGLE)
    {
        // Should have been rejected by validation if not supported.
        ASSERT(mSupportsGPUSwitching);
        usesDiscreteGPU = true;
    }

    return new ContextCGL(this, state, errorSet, mRenderer, usesDiscreteGPU);
}

DeviceImpl *DisplayCGL::createDevice()
{
    return new DeviceCGL();
}

egl::ConfigSet DisplayCGL::generateConfigs()
{
    // TODO(cwallez): generate more config permutations
    egl::ConfigSet configs;

    const gl::Version &maxVersion = getMaxSupportedESVersion();
    ASSERT(maxVersion >= gl::Version(2, 0));
    bool supportsES3 = maxVersion >= gl::Version(3, 0);

    egl::Config config;

    // Native stuff
    config.nativeVisualID   = 0;
    config.nativeVisualType = 0;
    config.nativeRenderable = EGL_TRUE;

    // Buffer sizes
    config.redSize     = 8;
    config.greenSize   = 8;
    config.blueSize    = 8;
    config.alphaSize   = 8;
    config.depthSize   = 24;
    config.stencilSize = 8;

    config.colorBufferType = EGL_RGB_BUFFER;
    config.luminanceSize   = 0;
    config.alphaMaskSize   = 0;

    config.bufferSize = config.redSize + config.greenSize + config.blueSize + config.alphaSize;

    config.transparentType = EGL_NONE;

    // Pbuffer
    config.maxPBufferWidth  = 4096;
    config.maxPBufferHeight = 4096;
    config.maxPBufferPixels = 4096 * 4096;

    // Caveat
    config.configCaveat = EGL_NONE;

    // Misc
    config.sampleBuffers     = 0;
    config.samples           = 0;
    config.level             = 0;
    config.bindToTextureRGB  = EGL_FALSE;
    config.bindToTextureRGBA = EGL_FALSE;

    config.bindToTextureTarget = EGL_TEXTURE_RECTANGLE_ANGLE;

    config.surfaceType = EGL_WINDOW_BIT | EGL_PBUFFER_BIT;

    config.minSwapInterval = 1;
    config.maxSwapInterval = 1;

    config.renderTargetFormat = GL_RGBA8;
    config.depthStencilFormat = GL_DEPTH24_STENCIL8;

    config.conformant     = EGL_OPENGL_ES2_BIT | (supportsES3 ? EGL_OPENGL_ES3_BIT_KHR : 0);
    config.renderableType = config.conformant;

    config.matchNativePixmap = EGL_NONE;

    config.colorComponentType = EGL_COLOR_COMPONENT_TYPE_FIXED_EXT;

    configs.add(config);
    return configs;
}

bool DisplayCGL::testDeviceLost()
{
    // TODO(cwallez) investigate implementing this
    return false;
}

egl::Error DisplayCGL::restoreLostDevice(const egl::Display *display)
{
    UNIMPLEMENTED();
    return egl::EglBadDisplay();
}

bool DisplayCGL::isValidNativeWindow(EGLNativeWindowType window) const
{
    NSObject *layer = reinterpret_cast<NSObject *>(window);
    return [layer isKindOfClass:[CALayer class]];
}

egl::Error DisplayCGL::validateClientBuffer(const egl::Config *configuration,
                                            EGLenum buftype,
                                            EGLClientBuffer clientBuffer,
                                            const egl::AttributeMap &attribs) const
{
    ASSERT(buftype == EGL_IOSURFACE_ANGLE);

    if (!IOSurfaceSurfaceCGL::validateAttributes(clientBuffer, attribs))
    {
        return egl::EglBadAttribute();
    }

    return egl::NoError();
}

std::string DisplayCGL::getVendorString() const
{
    // TODO(cwallez) find a useful vendor string
    return "";
}

CGLContextObj DisplayCGL::getCGLContext() const
{
    return mContext;
}

CGLPixelFormatObj DisplayCGL::getCGLPixelFormat() const
{
    return mPixelFormat;
}

void DisplayCGL::generateExtensions(egl::DisplayExtensions *outExtensions) const
{
    outExtensions->iosurfaceClientBuffer = true;
    outExtensions->surfacelessContext    = true;
    outExtensions->deviceQuery           = true;

    // Contexts are virtualized so textures can be shared globally
    outExtensions->displayTextureShareGroup = true;

    if (mSupportsGPUSwitching)
    {
        outExtensions->powerPreference = true;
    }

    DisplayGL::generateExtensions(outExtensions);
}

void DisplayCGL::generateCaps(egl::Caps *outCaps) const
{
    outCaps->textureNPOT = true;
}

egl::Error DisplayCGL::waitClient(const gl::Context *context)
{
    // TODO(cwallez) UNIMPLEMENTED()
    return egl::NoError();
}

egl::Error DisplayCGL::waitNative(const gl::Context *context, EGLint engine)
{
    // TODO(cwallez) UNIMPLEMENTED()
    return egl::NoError();
}

gl::Version DisplayCGL::getMaxSupportedESVersion() const
{
    return mRenderer->getMaxSupportedESVersion();
}

egl::Error DisplayCGL::makeCurrentSurfaceless(gl::Context *context)
{
    // We have nothing to do as mContext is always current, and that CGL is surfaceless by
    // default.
    return egl::NoError();
}

class WorkerContextCGL final : public WorkerContext
{
  public:
    WorkerContextCGL(CGLContextObj context);
    ~WorkerContextCGL() override;

    bool makeCurrent() override;
    void unmakeCurrent() override;

  private:
    CGLContextObj mContext;
};

WorkerContextCGL::WorkerContextCGL(CGLContextObj context) : mContext(context) {}

WorkerContextCGL::~WorkerContextCGL()
{
    CGLSetCurrentContext(nullptr);
    CGLReleaseContext(mContext);
    mContext = nullptr;
}

bool WorkerContextCGL::makeCurrent()
{
    CGLError error = CGLSetCurrentContext(mContext);
    if (error != kCGLNoError)
    {
        ERR() << "Unable to make gl context current.\n";
        return false;
    }
    return true;
}

void WorkerContextCGL::unmakeCurrent()
{
    CGLSetCurrentContext(nullptr);
}

WorkerContext *DisplayCGL::createWorkerContext(std::string *infoLog)
{
    CGLContextObj context = nullptr;
    CGLCreateContext(mPixelFormat, mContext, &context);
    if (context == nullptr)
    {
        *infoLog += "Could not create the CGL context.";
        return nullptr;
    }

    return new WorkerContextCGL(context);
}

void DisplayCGL::initializeFrontendFeatures(angle::FrontendFeatures *features) const
{
    mRenderer->initializeFrontendFeatures(features);
}

void DisplayCGL::populateFeatureList(angle::FeatureList *features)
{
    mRenderer->getFeatures().populateFeatureList(features);
}

egl::Error DisplayCGL::referenceDiscreteGPU()
{
    // Should have been rejected by validation if not supported.
    ASSERT(mSupportsGPUSwitching);
    // Create discrete pixel format if necessary.
    if (mDiscreteGPUPixelFormat)
    {
        // Clear this out if necessary.
        mLastDiscreteGPUUnrefTime = 0.0;
    }
    else
    {
        ASSERT(mLastDiscreteGPUUnrefTime == 0.0);
        CGLPixelFormatAttribute discreteAttribs[] = {static_cast<CGLPixelFormatAttribute>(0)};
        GLint numPixelFormats                     = 0;
        if (CGLChoosePixelFormat(discreteAttribs, &mDiscreteGPUPixelFormat, &numPixelFormats) !=
            kCGLNoError)
        {
            return egl::EglBadAlloc() << "Error choosing discrete pixel format.";
        }
    }
    ++mDiscreteGPURefs;

    return egl::NoError();
}

egl::Error DisplayCGL::unreferenceDiscreteGPU()
{
    // Should have been rejected by validation if not supported.
    ASSERT(mSupportsGPUSwitching);
    ASSERT(mDiscreteGPURefs > 0);
    if (--mDiscreteGPURefs == 0)
    {
        auto *platform            = ANGLEPlatformCurrent();
        mLastDiscreteGPUUnrefTime = platform->monotonicallyIncreasingTime(platform);
    }

    return egl::NoError();
}

void DisplayCGL::checkDiscreteGPUStatus()
{
    const double kDiscreteGPUTimeoutInSeconds = 10.0;

    if (mLastDiscreteGPUUnrefTime != 0.0)
    {
        ASSERT(mSupportsGPUSwitching);
        // A non-zero value implies that the timer is ticking on deleting the discrete GPU pixel
        // format.
        auto *platform = ANGLEPlatformCurrent();
        ASSERT(platform);
        double currentTime = platform->monotonicallyIncreasingTime(platform);
        if (currentTime > mLastDiscreteGPUUnrefTime + kDiscreteGPUTimeoutInSeconds)
        {
            CGLDestroyPixelFormat(mDiscreteGPUPixelFormat);
            mDiscreteGPUPixelFormat   = nullptr;
            mLastDiscreteGPUUnrefTime = 0.0;
        }
    }
}

egl::Error DisplayCGL::handleGPUSwitch()
{
    if (mSupportsGPUSwitching)
    {
        uint64_t gpuID = angle::GetGpuIDFromDisplayID(kCGDirectMainDisplay);
        if (gpuID != mCurrentGPUID)
        {
            SetGPUByRegistryID(mContext, mPixelFormat, gpuID);
            // Performing the above operation seems to need a call to CGLSetCurrentContext to make
            // the context work properly again. Failing to do this returns null strings for
            // GL_VENDOR and GL_RENDERER.
            CGLUpdateContext(mContext);
            CGLSetCurrentContext(mContext);
            onStateChange(angle::SubjectMessage::SubjectChanged);
            mCurrentGPUID = gpuID;
        }
    }

    return egl::NoError();
}

}  // namespace rx

#endif  // defined(ANGLE_PLATFORM_MACOS) || defined(ANGLE_PLATFORM_MACCATALYST)
