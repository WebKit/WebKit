//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// DisplayGLX.cpp: GLX implementation of egl::Display

#include "libANGLE/renderer/gl/glx/DisplayGLX.h"

#include <EGL/eglext.h>
#include <algorithm>
#include <cstring>
#include <fstream>

#include "common/debug.h"
#include "libANGLE/Config.h"
#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
#include "libANGLE/Surface.h"
#include "libANGLE/renderer/gl/ContextGL.h"
#include "libANGLE/renderer/gl/glx/PbufferSurfaceGLX.h"
#include "libANGLE/renderer/gl/glx/PixmapSurfaceGLX.h"
#include "libANGLE/renderer/gl/glx/RendererGLX.h"
#include "libANGLE/renderer/gl/glx/WindowSurfaceGLX.h"
#include "libANGLE/renderer/gl/glx/glx_utils.h"
#include "libANGLE/renderer/gl/renderergl_utils.h"

namespace
{

bool HasParallelShaderCompileExtension(const rx::FunctionsGL *functions)
{
    return functions->maxShaderCompilerThreadsKHR != nullptr ||
           functions->maxShaderCompilerThreadsARB != nullptr;
}

}  // anonymous namespace

namespace rx
{

static int IgnoreX11Errors(Display *, XErrorEvent *)
{
    return 0;
}

class FunctionsGLGLX : public FunctionsGL
{
  public:
    FunctionsGLGLX(PFNGETPROCPROC getProc) : mGetProc(getProc) {}

    ~FunctionsGLGLX() override {}

  private:
    void *loadProcAddress(const std::string &function) const override
    {
        return reinterpret_cast<void *>(mGetProc(function.c_str()));
    }

    PFNGETPROCPROC mGetProc;
};

DisplayGLX::DisplayGLX(const egl::DisplayState &state)
    : DisplayGL(state),
      mRequestedVisual(-1),
      mContextConfig(nullptr),
      mVisuals(nullptr),
      mContext(nullptr),
      mSharedContext(nullptr),
      mDummyPbuffer(0),
      mUsesNewXDisplay(false),
      mIsMesa(false),
      mHasMultisample(false),
      mHasARBCreateContext(false),
      mHasARBCreateContextProfile(false),
      mHasARBCreateContextRobustness(false),
      mHasEXTCreateContextES2Profile(false),
      mSwapControl(SwapControl::Absent),
      mMinSwapInterval(0),
      mMaxSwapInterval(0),
      mCurrentSwapInterval(-1),
      mCurrentDrawable(0),
      mXDisplay(nullptr),
      mEGLDisplay(nullptr)
{}

DisplayGLX::~DisplayGLX() {}

egl::Error DisplayGLX::initialize(egl::Display *display)
{
    mEGLDisplay           = display;
    mXDisplay             = reinterpret_cast<Display *>(display->getNativeDisplayId());
    const auto &attribMap = display->getAttributeMap();

    // ANGLE_platform_angle allows the creation of a default display
    // using EGL_DEFAULT_DISPLAY (= nullptr). In this case just open
    // the display specified by the DISPLAY environment variable.
    if (mXDisplay == reinterpret_cast<Display *>(EGL_DEFAULT_DISPLAY))
    {
        mUsesNewXDisplay = true;
        mXDisplay        = XOpenDisplay(nullptr);
        if (!mXDisplay)
        {
            return egl::EglNotInitialized() << "Could not open the default X display.";
        }
    }

    std::string glxInitError;
    if (!mGLX.initialize(mXDisplay, DefaultScreen(mXDisplay), &glxInitError))
    {
        return egl::EglNotInitialized() << glxInitError;
    }

    mHasMultisample             = mGLX.minorVersion > 3 || mGLX.hasExtension("GLX_ARB_multisample");
    mHasARBCreateContext        = mGLX.hasExtension("GLX_ARB_create_context");
    mHasARBCreateContextProfile = mGLX.hasExtension("GLX_ARB_create_context_profile");
    mHasARBCreateContextRobustness = mGLX.hasExtension("GLX_ARB_create_context_robustness");
    mHasEXTCreateContextES2Profile = mGLX.hasExtension("GLX_EXT_create_context_es2_profile");

    std::string clientVendor = mGLX.getClientString(GLX_VENDOR);
    mIsMesa                  = clientVendor.find("Mesa") != std::string::npos;

    // Choose the swap_control extension to use, if any.
    // The EXT version is better as it allows glXSwapInterval to be called per
    // window, while we'll potentially need to change the swap interval on each
    // swap buffers when using the SGI or MESA versions.
    if (mGLX.hasExtension("GLX_EXT_swap_control"))
    {
        mSwapControl = SwapControl::EXT;

        // In GLX_EXT_swap_control querying these is done on a GLXWindow so we just
        // set default values.
        mMinSwapInterval = 0;
        mMaxSwapInterval = 4;
    }
    else if (mGLX.hasExtension("GLX_MESA_swap_control"))
    {
        // If we have the Mesa or SGI extension, assume that you can at least set
        // a swap interval of 0 or 1.
        mSwapControl     = SwapControl::Mesa;
        mMinSwapInterval = 0;
        mMinSwapInterval = 1;
    }
    else if (mGLX.hasExtension("GLX_SGI_swap_control"))
    {
        mSwapControl     = SwapControl::SGI;
        mMinSwapInterval = 0;
        mMinSwapInterval = 1;
    }
    else
    {
        mSwapControl     = SwapControl::Absent;
        mMinSwapInterval = 1;
        mMinSwapInterval = 1;
    }

    if (attribMap.contains(EGL_X11_VISUAL_ID_ANGLE))
    {
        mRequestedVisual = static_cast<EGLint>(attribMap.get(EGL_X11_VISUAL_ID_ANGLE, -1));

        // There is no direct way to get the GLXFBConfig matching an X11 visual ID
        // so we have to iterate over all the GLXFBConfigs to find the right one.
        int nConfigs;
        int attribList[] = {
            None,
        };
        glx::FBConfig *allConfigs = mGLX.chooseFBConfig(attribList, &nConfigs);

        for (int i = 0; i < nConfigs; ++i)
        {
            if (getGLXFBConfigAttrib(allConfigs[i], GLX_VISUAL_ID) == mRequestedVisual)
            {
                mContextConfig = allConfigs[i];
                break;
            }
        }
        XFree(allConfigs);

        if (mContextConfig == nullptr)
        {
            return egl::EglNotInitialized() << "Invalid visual ID requested.";
        }
    }
    else
    {
        // When glXMakeCurrent is called, the context and the surface must be
        // compatible which in glX-speak means that their config have the same
        // color buffer type, are both RGBA or ColorIndex, and their buffers have
        // the same depth, if they exist.
        // Since our whole EGL implementation is backed by only one GL context, this
        // context must be compatible with all the GLXFBConfig corresponding to the
        // EGLconfigs that we will be exposing.
        int nConfigs;
        int attribList[]          = {// We want RGBA8 and DEPTH24_STENCIL8
                            GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8, GLX_ALPHA_SIZE, 8,
                            GLX_DEPTH_SIZE, 24, GLX_STENCIL_SIZE, 8,
                            // We want RGBA rendering (vs COLOR_INDEX) and doublebuffer
                            GLX_RENDER_TYPE, GLX_RGBA_BIT,
                            // Double buffer is not strictly required as a non-doublebuffer
                            // context can work with a doublebuffered surface, but it still
                            // flickers and all applications want doublebuffer anyway.
                            GLX_DOUBLEBUFFER, True,
                            // All of these must be supported for full EGL support
                            GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT | GLX_PBUFFER_BIT | GLX_PIXMAP_BIT,
                            // This makes sure the config have an associated visual Id
                            GLX_X_RENDERABLE, True, GLX_CONFIG_CAVEAT, GLX_NONE, None};
        glx::FBConfig *candidates = mGLX.chooseFBConfig(attribList, &nConfigs);
        if (nConfigs == 0)
        {
            XFree(candidates);
            return egl::EglNotInitialized()
                   << "Could not find a decent GLX FBConfig to create the context.";
        }
        mContextConfig = candidates[0];
        XFree(candidates);
    }

    const auto &eglAttributes = display->getAttributeMap();
    if (mHasARBCreateContext)
    {
        egl::Error error = initializeContext(mContextConfig, eglAttributes, &mContext);
        if (error.isError())
        {
            return error;
        }
    }
    else
    {
        if (eglAttributes.get(EGL_PLATFORM_ANGLE_TYPE_ANGLE,
                              EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE) ==
            EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE)
        {
            return egl::EglNotInitialized() << "Cannot create an OpenGL ES platform on GLX without "
                                               "the GLX_ARB_create_context extension.";
        }

        XVisualInfo visualTemplate;
        visualTemplate.visualid = getGLXFBConfigAttrib(mContextConfig, GLX_VISUAL_ID);

        int numVisuals = 0;
        mVisuals       = XGetVisualInfo(mXDisplay, VisualIDMask, &visualTemplate, &numVisuals);
        if (numVisuals <= 0)
        {
            return egl::EglNotInitialized() << "Could not get the visual info from the fb config";
        }
        ASSERT(numVisuals == 1);

        mContext = mGLX.createContext(&mVisuals[0], nullptr, true);

        if (!mContext)
        {
            return egl::EglNotInitialized() << "Could not create GL context.";
        }

        mSharedContext = mGLX.createContext(&mVisuals[0], mContext, True);
    }
    ASSERT(mContext);

    mCurrentContexts[std::this_thread::get_id()] = mContext;

    // FunctionsGL and DisplayGL need to make a few GL calls, for example to
    // query the version of the context so we need to make the context current.
    // glXMakeCurrent requires a GLXDrawable so we create a temporary Pbuffer
    // (of size 1, 1) for the duration of these calls.
    // Ideally we would want to unset the current context and destroy the pbuffer
    // before going back to the application but this is TODO
    // We could use a pbuffer of size (0, 0) but it fails on the Intel Mesa driver
    // as commented on https://bugs.freedesktop.org/show_bug.cgi?id=38869 so we
    // use (1, 1) instead.

    int dummyPbufferAttribs[] = {
        GLX_PBUFFER_WIDTH, 1, GLX_PBUFFER_HEIGHT, 1, None,
    };
    mDummyPbuffer = mGLX.createPbuffer(mContextConfig, dummyPbufferAttribs);
    if (!mDummyPbuffer)
    {
        return egl::EglNotInitialized() << "Could not create the dummy pbuffer.";
    }

    if (!mGLX.makeCurrent(mDummyPbuffer, mContext))
    {
        return egl::EglNotInitialized() << "Could not make the dummy pbuffer current.";
    }

    std::unique_ptr<FunctionsGL> functionsGL(new FunctionsGLGLX(mGLX.getProc));
    functionsGL->initialize(eglAttributes);

    // TODO(cwallez, angleproject:1303) Disable the OpenGL ES backend on Linux NVIDIA and Intel as
    // it has problems on our automated testing. An OpenGL ES backend might not trigger this test if
    // there is no Desktop OpenGL support, but that's not the case in our automated testing.
    VendorID vendor = GetVendorID(functionsGL.get());
    bool isOpenGLES =
        eglAttributes.get(EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE) ==
        EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE;
    if (isOpenGLES && (IsIntel(vendor) || IsNvidia(vendor)))
    {
        return egl::EglNotInitialized() << "Intel or NVIDIA OpenGL ES drivers are not supported.";
    }

    if (mSharedContext)
    {
        if (HasParallelShaderCompileExtension(functionsGL.get()))
        {
            mGLX.destroyContext(mSharedContext);
            mSharedContext = nullptr;
        }
        else
        {
            for (unsigned int i = 0; i < RendererGL::getMaxWorkerContexts(); ++i)
            {
                glx::Pbuffer workerPbuffer =
                    mGLX.createPbuffer(mContextConfig, dummyPbufferAttribs);
                if (!workerPbuffer)
                {
                    return egl::EglNotInitialized() << "Could not create the worker pbuffers.";
                }
                mWorkerPbufferPool.push_back(workerPbuffer);
            }
        }
    }

    syncXCommands();

    mRenderer.reset(new RendererGLX(std::move(functionsGL), eglAttributes, this));
    const gl::Version &maxVersion = mRenderer->getMaxSupportedESVersion();
    if (maxVersion < gl::Version(2, 0))
    {
        return egl::EglNotInitialized() << "OpenGL ES 2.0 is not supportable.";
    }

    return DisplayGL::initialize(display);
}

void DisplayGLX::terminate()
{
    DisplayGL::terminate();

    if (mVisuals)
    {
        XFree(mVisuals);
        mVisuals = 0;
    }

    if (mDummyPbuffer)
    {
        mGLX.destroyPbuffer(mDummyPbuffer);
        mDummyPbuffer = 0;
    }

    for (auto &workerPbuffer : mWorkerPbufferPool)
    {
        mGLX.destroyPbuffer(workerPbuffer);
    }
    mWorkerPbufferPool.clear();

    mCurrentContexts.clear();

    if (mContext)
    {
        mGLX.destroyContext(mContext);
        mContext = nullptr;
    }

    if (mSharedContext)
    {
        mGLX.destroyContext(mSharedContext);
        mSharedContext = nullptr;
    }

    mGLX.terminate();

    mRenderer.reset();

    if (mUsesNewXDisplay)
    {
        XCloseDisplay(mXDisplay);
    }
}

egl::Error DisplayGLX::makeCurrent(egl::Surface *drawSurface,
                                   egl::Surface *readSurface,
                                   gl::Context *context)
{
    glx::Drawable newDrawable =
        (drawSurface ? GetImplAs<SurfaceGLX>(drawSurface)->getDrawable() : mDummyPbuffer);
    glx::Context newContext = mContext;
    // If the thread calling makeCurrent does not have the correct context current (either mContext
    // or 0), we need to set it current.
    if (!context)
    {
        newDrawable = 0;
        newContext  = 0;
    }
    if (newDrawable != mCurrentDrawable ||
        newContext != mCurrentContexts[std::this_thread::get_id()])
    {
        if (mGLX.makeCurrent(newDrawable, newContext) != True)
        {
            return egl::EglContextLost() << "Failed to make the GLX context current";
        }
        mCurrentContexts[std::this_thread::get_id()] = newContext;
        mCurrentDrawable                             = newDrawable;
    }

    return DisplayGL::makeCurrent(drawSurface, readSurface, context);
}

SurfaceImpl *DisplayGLX::createWindowSurface(const egl::SurfaceState &state,
                                             EGLNativeWindowType window,
                                             const egl::AttributeMap &attribs)
{
    ASSERT(configIdToGLXConfig.count(state.config->configID) > 0);
    glx::FBConfig fbConfig = configIdToGLXConfig[state.config->configID];

    return new WindowSurfaceGLX(state, mGLX, this, window, mGLX.getDisplay(), fbConfig);
}

SurfaceImpl *DisplayGLX::createPbufferSurface(const egl::SurfaceState &state,
                                              const egl::AttributeMap &attribs)
{
    ASSERT(configIdToGLXConfig.count(state.config->configID) > 0);
    glx::FBConfig fbConfig = configIdToGLXConfig[state.config->configID];

    EGLint width  = static_cast<EGLint>(attribs.get(EGL_WIDTH, 0));
    EGLint height = static_cast<EGLint>(attribs.get(EGL_HEIGHT, 0));
    bool largest  = (attribs.get(EGL_LARGEST_PBUFFER, EGL_FALSE) == EGL_TRUE);

    return new PbufferSurfaceGLX(state, width, height, largest, mGLX, fbConfig);
}

SurfaceImpl *DisplayGLX::createPbufferFromClientBuffer(const egl::SurfaceState &state,
                                                       EGLenum buftype,
                                                       EGLClientBuffer clientBuffer,
                                                       const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return nullptr;
}

SurfaceImpl *DisplayGLX::createPixmapSurface(const egl::SurfaceState &state,
                                             NativePixmapType nativePixmap,
                                             const egl::AttributeMap &attribs)
{
    ASSERT(configIdToGLXConfig.count(state.config->configID) > 0);
    glx::FBConfig fbConfig = configIdToGLXConfig[state.config->configID];
    return new PixmapSurfaceGLX(state, nativePixmap, mGLX.getDisplay(), mGLX, fbConfig);
}

egl::Error DisplayGLX::validatePixmap(egl::Config *config,
                                      EGLNativePixmapType pixmap,
                                      const egl::AttributeMap &attributes) const
{
    Window rootWindow;
    int x                    = 0;
    int y                    = 0;
    unsigned int width       = 0;
    unsigned int height      = 0;
    unsigned int borderWidth = 0;
    unsigned int depth       = 0;
    int status = XGetGeometry(mGLX.getDisplay(), pixmap, &rootWindow, &x, &y, &width, &height,
                              &borderWidth, &depth);
    if (!status)
    {
        return egl::EglBadNativePixmap() << "Invalid native pixmap, XGetGeometry failed: "
                                         << x11::XErrorToString(mXDisplay, status);
    }

    return egl::NoError();
}

ContextImpl *DisplayGLX::createContext(const gl::State &state,
                                       gl::ErrorSet *errorSet,
                                       const egl::Config *configuration,
                                       const gl::Context *shareContext,
                                       const egl::AttributeMap &attribs)
{
    return new ContextGL(state, errorSet, mRenderer);
}

DeviceImpl *DisplayGLX::createDevice()
{
    UNIMPLEMENTED();
    return nullptr;
}

egl::Error DisplayGLX::initializeContext(glx::FBConfig config,
                                         const egl::AttributeMap &eglAttributes,
                                         glx::Context *context)
{
    int profileMask = 0;

    EGLint requestedDisplayType = static_cast<EGLint>(
        eglAttributes.get(EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE));
    if (requestedDisplayType == EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE)
    {
        if (!mHasEXTCreateContextES2Profile)
        {
            return egl::EglNotInitialized() << "Cannot create an OpenGL ES platform on GLX without "
                                               "the GLX_EXT_create_context_es_profile extension.";
        }

        ASSERT(mHasARBCreateContextProfile);
        profileMask |= GLX_CONTEXT_ES2_PROFILE_BIT_EXT;
    }

    // Create a context of the requested version, if any.
    gl::Version requestedVersion(static_cast<EGLint>(eglAttributes.get(
                                     EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE, EGL_DONT_CARE)),
                                 static_cast<EGLint>(eglAttributes.get(
                                     EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE, EGL_DONT_CARE)));
    if (static_cast<EGLint>(requestedVersion.major) != EGL_DONT_CARE &&
        static_cast<EGLint>(requestedVersion.minor) != EGL_DONT_CARE)
    {
        if (!(profileMask & GLX_CONTEXT_ES2_PROFILE_BIT_EXT) &&
            requestedVersion >= gl::Version(3, 2))
        {
            profileMask |= GLX_CONTEXT_CORE_PROFILE_BIT_ARB;
        }
        return createContextAttribs(config, requestedVersion, profileMask, context);
    }

    // The only way to get a core profile context of the highest version using
    // glXCreateContextAttrib is to try creationg contexts in decreasing version
    // numbers. It might look that asking for a core context of version (0, 0)
    // works on some driver but it create a _compatibility_ context of the highest
    // version instead. The cost of failing a context creation is small (< 0.1 ms)
    // on Mesa but is unfortunately a bit expensive on the Nvidia driver (~3ms).
    // Also try to get any Desktop GL context, but if that fails fallback to
    // asking for OpenGL ES contexts.

    // NOTE: below we return as soon as we're able to create a context so the
    // "error" variable is EGL_SUCCESS when returned contrary to the common idiom
    // of returning "error" when there is an actual error.
    for (const auto &info : GenerateContextCreationToTry(requestedDisplayType, mIsMesa))
    {
        int profileFlag = 0;
        if (info.type == ContextCreationTry::Type::DESKTOP_CORE)
        {
            profileFlag |= GLX_CONTEXT_CORE_PROFILE_BIT_ARB;
        }
        else if (info.type == ContextCreationTry::Type::ES)
        {
            profileFlag |= GLX_CONTEXT_ES2_PROFILE_BIT_EXT;
        }

        egl::Error error = createContextAttribs(config, info.version, profileFlag, context);
        if (!error.isError())
        {
            return error;
        }
    }

    return egl::EglNotInitialized() << "Could not create a backing OpenGL context.";
}

egl::ConfigSet DisplayGLX::generateConfigs()
{
    egl::ConfigSet configs;
    configIdToGLXConfig.clear();

    const gl::Version &maxVersion = getMaxSupportedESVersion();
    ASSERT(maxVersion >= gl::Version(2, 0));
    bool supportsES3 = maxVersion >= gl::Version(3, 0);

    int contextRedSize   = getGLXFBConfigAttrib(mContextConfig, GLX_RED_SIZE);
    int contextGreenSize = getGLXFBConfigAttrib(mContextConfig, GLX_GREEN_SIZE);
    int contextBlueSize  = getGLXFBConfigAttrib(mContextConfig, GLX_BLUE_SIZE);
    int contextAlphaSize = getGLXFBConfigAttrib(mContextConfig, GLX_ALPHA_SIZE);

    int contextDepthSize   = getGLXFBConfigAttrib(mContextConfig, GLX_DEPTH_SIZE);
    int contextStencilSize = getGLXFBConfigAttrib(mContextConfig, GLX_STENCIL_SIZE);

    int contextSamples = mHasMultisample ? getGLXFBConfigAttrib(mContextConfig, GLX_SAMPLES) : 0;
    int contextSampleBuffers =
        mHasMultisample ? getGLXFBConfigAttrib(mContextConfig, GLX_SAMPLE_BUFFERS) : 0;

    int contextAccumRedSize   = getGLXFBConfigAttrib(mContextConfig, GLX_ACCUM_RED_SIZE);
    int contextAccumGreenSize = getGLXFBConfigAttrib(mContextConfig, GLX_ACCUM_GREEN_SIZE);
    int contextAccumBlueSize  = getGLXFBConfigAttrib(mContextConfig, GLX_ACCUM_BLUE_SIZE);
    int contextAccumAlphaSize = getGLXFBConfigAttrib(mContextConfig, GLX_ACCUM_ALPHA_SIZE);

    int attribList[] = {
        GLX_RENDER_TYPE, GLX_RGBA_BIT, GLX_X_RENDERABLE, True, GLX_DOUBLEBUFFER, True, None,
    };

    int glxConfigCount;
    glx::FBConfig *glxConfigs = mGLX.chooseFBConfig(attribList, &glxConfigCount);

    for (int i = 0; i < glxConfigCount; i++)
    {
        glx::FBConfig glxConfig = glxConfigs[i];
        egl::Config config;

        // Native stuff
        config.nativeVisualID   = getGLXFBConfigAttrib(glxConfig, GLX_VISUAL_ID);
        config.nativeVisualType = getGLXFBConfigAttrib(glxConfig, GLX_X_VISUAL_TYPE);
        config.nativeRenderable = EGL_TRUE;

        // When a visual ID has been specified with EGL_ANGLE_x11_visual we should
        // only return configs with this visual: it will maximize performance by avoid
        // blits in the driver when showing the window on the screen.
        if (mRequestedVisual != -1 && config.nativeVisualID != mRequestedVisual)
        {
            continue;
        }

        // Buffer sizes
        config.redSize     = getGLXFBConfigAttrib(glxConfig, GLX_RED_SIZE);
        config.greenSize   = getGLXFBConfigAttrib(glxConfig, GLX_GREEN_SIZE);
        config.blueSize    = getGLXFBConfigAttrib(glxConfig, GLX_BLUE_SIZE);
        config.alphaSize   = getGLXFBConfigAttrib(glxConfig, GLX_ALPHA_SIZE);
        config.depthSize   = getGLXFBConfigAttrib(glxConfig, GLX_DEPTH_SIZE);
        config.stencilSize = getGLXFBConfigAttrib(glxConfig, GLX_STENCIL_SIZE);

        // We require RGBA8 and the D24S8 (or no DS buffer)
        if (config.redSize != contextRedSize || config.greenSize != contextGreenSize ||
            config.blueSize != contextBlueSize || config.alphaSize != contextAlphaSize)
        {
            continue;
        }
        // The GLX spec says that it is ok for a whole buffer to not be present
        // however the Mesa Intel driver (and probably on other Mesa drivers)
        // fails to make current when the Depth stencil doesn't exactly match the
        // configuration.
        bool hasSameDepthStencil =
            config.depthSize == contextDepthSize && config.stencilSize == contextStencilSize;
        bool hasNoDepthStencil = config.depthSize == 0 && config.stencilSize == 0;
        if (!hasSameDepthStencil && (mIsMesa || !hasNoDepthStencil))
        {
            continue;
        }

        config.colorBufferType = EGL_RGB_BUFFER;
        config.luminanceSize   = 0;
        config.alphaMaskSize   = 0;

        config.bufferSize = config.redSize + config.greenSize + config.blueSize + config.alphaSize;

        // Multisample and accumulation buffers
        int samples = mHasMultisample ? getGLXFBConfigAttrib(glxConfig, GLX_SAMPLES) : 0;
        int sampleBuffers =
            mHasMultisample ? getGLXFBConfigAttrib(glxConfig, GLX_SAMPLE_BUFFERS) : 0;

        int accumRedSize   = getGLXFBConfigAttrib(glxConfig, GLX_ACCUM_RED_SIZE);
        int accumGreenSize = getGLXFBConfigAttrib(glxConfig, GLX_ACCUM_GREEN_SIZE);
        int accumBlueSize  = getGLXFBConfigAttrib(glxConfig, GLX_ACCUM_BLUE_SIZE);
        int accumAlphaSize = getGLXFBConfigAttrib(glxConfig, GLX_ACCUM_ALPHA_SIZE);

        if (samples != contextSamples || sampleBuffers != contextSampleBuffers ||
            accumRedSize != contextAccumRedSize || accumGreenSize != contextAccumGreenSize ||
            accumBlueSize != contextAccumBlueSize || accumAlphaSize != contextAccumAlphaSize)
        {
            continue;
        }

        config.samples       = samples;
        config.sampleBuffers = sampleBuffers;

        // Transparency
        if (getGLXFBConfigAttrib(glxConfig, GLX_TRANSPARENT_TYPE) == GLX_TRANSPARENT_RGB)
        {
            config.transparentType     = EGL_TRANSPARENT_RGB;
            config.transparentRedValue = getGLXFBConfigAttrib(glxConfig, GLX_TRANSPARENT_RED_VALUE);
            config.transparentGreenValue =
                getGLXFBConfigAttrib(glxConfig, GLX_TRANSPARENT_GREEN_VALUE);
            config.transparentBlueValue =
                getGLXFBConfigAttrib(glxConfig, GLX_TRANSPARENT_BLUE_VALUE);
        }
        else
        {
            config.transparentType = EGL_NONE;
        }

        // Pbuffer
        config.maxPBufferWidth  = getGLXFBConfigAttrib(glxConfig, GLX_MAX_PBUFFER_WIDTH);
        config.maxPBufferHeight = getGLXFBConfigAttrib(glxConfig, GLX_MAX_PBUFFER_HEIGHT);
        config.maxPBufferPixels = getGLXFBConfigAttrib(glxConfig, GLX_MAX_PBUFFER_PIXELS);

        // Caveat
        config.configCaveat = EGL_NONE;

        int caveat = getGLXFBConfigAttrib(glxConfig, GLX_CONFIG_CAVEAT);
        if (caveat == GLX_SLOW_CONFIG)
        {
            config.configCaveat = EGL_SLOW_CONFIG;
        }
        else if (caveat == GLX_NON_CONFORMANT_CONFIG)
        {
            continue;
        }

        // Misc
        config.level = getGLXFBConfigAttrib(glxConfig, GLX_LEVEL);

        config.minSwapInterval = mMinSwapInterval;
        config.maxSwapInterval = mMaxSwapInterval;

        // TODO(cwallez) wildly guessing these formats, another TODO says they should be removed
        // anyway
        config.renderTargetFormat = GL_RGBA8;
        config.depthStencilFormat = GL_DEPTH24_STENCIL8;

        config.conformant     = EGL_OPENGL_ES2_BIT | (supportsES3 ? EGL_OPENGL_ES3_BIT_KHR : 0);
        config.renderableType = config.conformant;

        // TODO(cwallez) I have no idea what this is
        config.matchNativePixmap = EGL_NONE;

        config.colorComponentType = EGL_COLOR_COMPONENT_TYPE_FIXED_EXT;

        // GLX doesn't support binding pbuffers to textures and there is no way to differentiate in
        // EGL that pixmaps can be bound but pbuffers cannot.  If both pixmaps and pbuffers are
        // supported, generate extra configs with either pbuffer or pixmap support.
        int glxDrawable     = getGLXFBConfigAttrib(glxConfig, GLX_DRAWABLE_TYPE);
        bool pbufferSupport = (glxDrawable & EGL_PBUFFER_BIT) != 0;
        bool pixmapSupport  = (glxDrawable & GLX_PIXMAP_BIT) != 0;
        bool pixmapBindToTextureSupport =
            pixmapSupport && mGLX.hasExtension("GLX_EXT_texture_from_pixmap");

        if (pbufferSupport && pixmapBindToTextureSupport)
        {
            // Generate the pixmap-only config
            config.surfaceType = (glxDrawable & GLX_WINDOW_BIT ? EGL_WINDOW_BIT : 0) |
                                 (pixmapSupport ? EGL_PIXMAP_BIT : 0);

            config.bindToTextureRGB = getGLXFBConfigAttrib(glxConfig, GLX_BIND_TO_TEXTURE_RGB_EXT);
            config.bindToTextureRGBA =
                getGLXFBConfigAttrib(glxConfig, GLX_BIND_TO_TEXTURE_RGBA_EXT);
            config.yInverted = getGLXFBConfigAttrib(glxConfig, GLX_Y_INVERTED_EXT);

            int id                  = configs.add(config);
            configIdToGLXConfig[id] = glxConfig;
        }

        // Generate the pbuffer config. It can support pixmaps but not bind-to-texture.
        config.surfaceType = (glxDrawable & GLX_WINDOW_BIT ? EGL_WINDOW_BIT : 0) |
                             (pbufferSupport ? EGL_PBUFFER_BIT : 0) |
                             (pixmapSupport ? EGL_PIXMAP_BIT : 0);

        config.bindToTextureRGB  = false;
        config.bindToTextureRGBA = false;
        config.yInverted         = false;

        int id                  = configs.add(config);
        configIdToGLXConfig[id] = glxConfig;
    }

    XFree(glxConfigs);

    return configs;
}

bool DisplayGLX::testDeviceLost()
{
    return false;
}

egl::Error DisplayGLX::restoreLostDevice(const egl::Display *display)
{
    return egl::EglBadDisplay();
}

bool DisplayGLX::isValidNativeWindow(EGLNativeWindowType window) const
{

    // Check the validity of the window by calling a getter function on the window that
    // returns a status code. If the window is bad the call return a status of zero. We
    // need to set a temporary X11 error handler while doing this because the default
    // X11 error handler exits the program on any error.
    auto oldErrorHandler = XSetErrorHandler(IgnoreX11Errors);
    XWindowAttributes attributes;
    int status = XGetWindowAttributes(mXDisplay, window, &attributes);
    XSetErrorHandler(oldErrorHandler);

    return status != 0;
}

std::string DisplayGLX::getVendorString() const
{
    // UNIMPLEMENTED();
    return "";
}

egl::Error DisplayGLX::waitClient(const gl::Context *context)
{
    mGLX.waitGL();
    return egl::NoError();
}

egl::Error DisplayGLX::waitNative(const gl::Context *context, EGLint engine)
{
    // eglWaitNative is used to notice the driver of changes in X11 for the current surface, such as
    // changes of the window size. We use this event to update the child window of WindowSurfaceGLX
    // to match its parent window's size.
    // Handling eglWaitNative this way helps the application control when resize happens. This is
    // important because drivers have a tendency to clobber the back buffer when the windows are
    // resized. See http://crbug.com/326995
    egl::Surface *drawSurface = context->getCurrentDrawSurface();
    egl::Surface *readSurface = context->getCurrentReadSurface();
    if (drawSurface != nullptr)
    {
        SurfaceGLX *glxDrawSurface = GetImplAs<SurfaceGLX>(drawSurface);
        ANGLE_TRY(glxDrawSurface->checkForResize());
    }

    if (readSurface != drawSurface && readSurface != nullptr)
    {
        SurfaceGLX *glxReadSurface = GetImplAs<SurfaceGLX>(readSurface);
        ANGLE_TRY(glxReadSurface->checkForResize());
    }

    // We still need to forward the resizing of the child window to the driver.
    mGLX.waitX();
    return egl::NoError();
}

gl::Version DisplayGLX::getMaxSupportedESVersion() const
{
    return mRenderer->getMaxSupportedESVersion();
}

void DisplayGLX::syncXCommands() const
{
    if (mUsesNewXDisplay)
    {
        XSync(mGLX.getDisplay(), False);
    }
}

void DisplayGLX::setSwapInterval(glx::Drawable drawable, SwapControlData *data)
{
    ASSERT(data != nullptr);

    // TODO(cwallez) error checking?
    if (mSwapControl == SwapControl::EXT)
    {
        // Prefer the EXT extension, it gives per-drawable swap intervals, which will
        // minimize the number of driver calls.
        if (data->maxSwapInterval < 0)
        {
            unsigned int maxSwapInterval = 0;
            mGLX.queryDrawable(drawable, GLX_MAX_SWAP_INTERVAL_EXT, &maxSwapInterval);
            data->maxSwapInterval = static_cast<int>(maxSwapInterval);
        }

        // When the egl configs were generated we had to guess what the max swap interval
        // was because we didn't have a window to query it one (and that this max could
        // depend on the monitor). This means that the target interval might be higher
        // than the max interval and needs to be clamped.
        const int realInterval = std::min(data->targetSwapInterval, data->maxSwapInterval);
        if (data->currentSwapInterval != realInterval)
        {
            mGLX.swapIntervalEXT(drawable, realInterval);
            data->currentSwapInterval = realInterval;
        }
    }
    else if (mCurrentSwapInterval != data->targetSwapInterval)
    {
        // With the Mesa or SGI extensions we can still do per-drawable swap control
        // manually but it is more expensive in number of driver calls.
        if (mSwapControl == SwapControl::Mesa)
        {
            mGLX.swapIntervalMESA(data->targetSwapInterval);
        }
        else if (mSwapControl == SwapControl::SGI)
        {
            mGLX.swapIntervalSGI(data->targetSwapInterval);
        }
        mCurrentSwapInterval = data->targetSwapInterval;
    }
}

bool DisplayGLX::isValidWindowVisualId(unsigned long visualId) const
{
    return mRequestedVisual == -1 || static_cast<unsigned long>(mRequestedVisual) == visualId;
}

void DisplayGLX::generateExtensions(egl::DisplayExtensions *outExtensions) const
{
    outExtensions->createContextRobustness = mHasARBCreateContextRobustness;

    // Contexts are virtualized so textures can be shared globally
    outExtensions->displayTextureShareGroup = true;

    outExtensions->surfacelessContext = true;

    const bool hasSyncControlOML        = mGLX.hasExtension("GLX_OML_sync_control");
    outExtensions->syncControlCHROMIUM  = hasSyncControlOML;
    outExtensions->syncControlRateANGLE = hasSyncControlOML;

    outExtensions->textureFromPixmapNOK = mGLX.hasExtension("GLX_EXT_texture_from_pixmap");

    DisplayGL::generateExtensions(outExtensions);
}

void DisplayGLX::generateCaps(egl::Caps *outCaps) const
{
    outCaps->textureNPOT = true;
}

egl::Error DisplayGLX::makeCurrentSurfaceless(gl::Context *context)
{
    // Nothing to do because GLX always uses the same context and the previous surface can be left
    // current.
    return egl::NoError();
}

int DisplayGLX::getGLXFBConfigAttrib(glx::FBConfig config, int attrib) const
{
    int result;
    mGLX.getFBConfigAttrib(config, attrib, &result);
    return result;
}

egl::Error DisplayGLX::createContextAttribs(glx::FBConfig,
                                            const Optional<gl::Version> &version,
                                            int profileMask,
                                            glx::Context *context)
{
    mAttribs.clear();

    if (mHasARBCreateContextRobustness)
    {
        mAttribs.push_back(GLX_CONTEXT_FLAGS_ARB);
        mAttribs.push_back(GLX_CONTEXT_ROBUST_ACCESS_BIT_ARB);
        mAttribs.push_back(GLX_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB);
        mAttribs.push_back(GLX_LOSE_CONTEXT_ON_RESET_ARB);
    }

    if (version.valid())
    {
        mAttribs.push_back(GLX_CONTEXT_MAJOR_VERSION_ARB);
        mAttribs.push_back(version.value().major);

        mAttribs.push_back(GLX_CONTEXT_MINOR_VERSION_ARB);
        mAttribs.push_back(version.value().minor);
    }

    if (profileMask != 0 && mHasARBCreateContextProfile)
    {
        mAttribs.push_back(GLX_CONTEXT_PROFILE_MASK_ARB);
        mAttribs.push_back(profileMask);
    }

    mAttribs.push_back(None);

    // When creating a context with glXCreateContextAttribsARB, a variety of X11 errors can
    // be generated. To prevent these errors from crashing our process, we simply ignore
    // them and only look if GLXContext was created.
    // Process all events before setting the error handler to avoid desynchronizing XCB instances
    // (the error handler is NOT per-display).
    XSync(mXDisplay, False);
    auto oldErrorHandler = XSetErrorHandler(IgnoreX11Errors);
    *context = mGLX.createContextAttribsARB(mContextConfig, nullptr, True, mAttribs.data());
    XSetErrorHandler(oldErrorHandler);

    if (!*context)
    {
        return egl::EglNotInitialized() << "Could not create GL context.";
    }

    mSharedContext = mGLX.createContextAttribsARB(mContextConfig, mContext, True, mAttribs.data());

    return egl::NoError();
}

class WorkerContextGLX final : public WorkerContext
{
  public:
    WorkerContextGLX(glx::Context context, FunctionsGLX *functions, glx::Pbuffer buffer);
    ~WorkerContextGLX() override;

    bool makeCurrent() override;
    void unmakeCurrent() override;

  private:
    glx::Context mContext;
    FunctionsGLX *mFunctions;
    glx::Pbuffer mBuffer;
};

WorkerContextGLX::WorkerContextGLX(glx::Context context,
                                   FunctionsGLX *functions,
                                   glx::Pbuffer buffer)
    : mContext(context), mFunctions(functions), mBuffer(buffer)
{}

WorkerContextGLX::~WorkerContextGLX()
{
    mFunctions->destroyContext(mContext);
    mFunctions->destroyPbuffer(mBuffer);
}

bool WorkerContextGLX::makeCurrent()
{
    Bool result = mFunctions->makeCurrent(mBuffer, mContext);
    if (result != True)
    {
        ERR() << "Unable to make the GLX context current.";
        return false;
    }
    return true;
}

void WorkerContextGLX::unmakeCurrent()
{
    mFunctions->makeCurrent(0, nullptr);
}

WorkerContext *DisplayGLX::createWorkerContext(std::string *infoLog)
{
    if (!mSharedContext)
    {
        *infoLog += "No shared context.";
        return nullptr;
    }
    if (mWorkerPbufferPool.empty())
    {
        *infoLog += "No worker pbuffers.";
        return nullptr;
    }
    glx::Context context = nullptr;
    if (mHasARBCreateContext)
    {
        context =
            mGLX.createContextAttribsARB(mContextConfig, mSharedContext, True, mAttribs.data());
    }
    else
    {
        context = mGLX.createContext(&mVisuals[0], mSharedContext, True);
    }

    if (!context)
    {
        *infoLog += "Unable to create the glx context.";
        return nullptr;
    }

    glx::Pbuffer workerPbuffer = mWorkerPbufferPool.back();
    mWorkerPbufferPool.pop_back();

    return new WorkerContextGLX(context, &mGLX, workerPbuffer);
}

void DisplayGLX::initializeFrontendFeatures(angle::FrontendFeatures *features) const
{
    mRenderer->initializeFrontendFeatures(features);
}

void DisplayGLX::populateFeatureList(angle::FeatureList *features)
{
    mRenderer->getFeatures().populateFeatureList(features);
}

}  // namespace rx
