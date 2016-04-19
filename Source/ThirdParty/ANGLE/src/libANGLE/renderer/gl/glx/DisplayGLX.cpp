//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// DisplayGLX.cpp: GLX implementation of egl::Display

#include "libANGLE/renderer/gl/glx/DisplayGLX.h"

#include <EGL/eglext.h>
#include <algorithm>

#include "common/debug.h"
#include "libANGLE/Config.h"
#include "libANGLE/Display.h"
#include "libANGLE/Surface.h"
#include "libANGLE/renderer/gl/glx/PbufferSurfaceGLX.h"
#include "libANGLE/renderer/gl/glx/WindowSurfaceGLX.h"
#include "libANGLE/renderer/gl/renderergl_utils.h"
#include "third_party/libXNVCtrl/NVCtrl.h"
#include "third_party/libXNVCtrl/NVCtrlLib.h"

namespace rx
{

static int IgnoreX11Errors(Display *, XErrorEvent *)
{
    return 0;
}

SwapControlData::SwapControlData()
  : targetSwapInterval(0),
    maxSwapInterval(-1),
    currentSwapInterval(-1)
{
}

class FunctionsGLGLX : public FunctionsGL
{
  public:
    FunctionsGLGLX(PFNGETPROCPROC getProc)
      : mGetProc(getProc)
    {
    }

    ~FunctionsGLGLX() override {}

  private:
    void *loadProcAddress(const std::string &function) override
    {
        return reinterpret_cast<void*>(mGetProc(function.c_str()));
    }

    PFNGETPROCPROC mGetProc;
};

DisplayGLX::DisplayGLX()
    : DisplayGL(),
      mFunctionsGL(nullptr),
      mRequestedVisual(-1),
      mContextConfig(nullptr),
      mContext(nullptr),
      mDummyPbuffer(0),
      mUsesNewXDisplay(false),
      mIsMesa(false),
      mHasMultisample(false),
      mHasARBCreateContext(false),
      mHasARBCreateContextProfile(false),
      mHasEXTCreateContextES2Profile(false),
      mSwapControl(SwapControl::Absent),
      mMinSwapInterval(0),
      mMaxSwapInterval(0),
      mCurrentSwapInterval(-1),
      mXDisplay(nullptr),
      mEGLDisplay(nullptr)
{
}

DisplayGLX::~DisplayGLX()
{
}

egl::Error DisplayGLX::initialize(egl::Display *display)
{
    mEGLDisplay = display;
    mXDisplay             = display->getNativeDisplayId();
    const auto &attribMap = display->getAttributeMap();

    // ANGLE_platform_angle allows the creation of a default display
    // using EGL_DEFAULT_DISPLAY (= nullptr). In this case just open
    // the display specified by the DISPLAY environment variable.
    if (mXDisplay == EGL_DEFAULT_DISPLAY)
    {
        mUsesNewXDisplay = true;
        mXDisplay = XOpenDisplay(NULL);
        if (!mXDisplay)
        {
            return egl::Error(EGL_NOT_INITIALIZED, "Could not open the default X display.");
        }
    }

    std::string glxInitError;
    if (!mGLX.initialize(mXDisplay, DefaultScreen(mXDisplay), &glxInitError))
    {
        return egl::Error(EGL_NOT_INITIALIZED, glxInitError.c_str());
    }

    mHasMultisample      = mGLX.minorVersion > 3 || mGLX.hasExtension("GLX_ARB_multisample");
    mHasARBCreateContext = mGLX.hasExtension("GLX_ARB_create_context");
    mHasARBCreateContextProfile    = mGLX.hasExtension("GLX_ARB_create_context_profile");
    mHasEXTCreateContextES2Profile = mGLX.hasExtension("GLX_EXT_create_context_es2_profile");

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
        mSwapControl = SwapControl::Mesa;
        mMinSwapInterval = 0;
        mMinSwapInterval = 1;
    }
    else if (mGLX.hasExtension("GLX_SGI_swap_control"))
    {
        mSwapControl = SwapControl::SGI;
        mMinSwapInterval = 0;
        mMinSwapInterval = 1;
    }
    else
    {
        mSwapControl = SwapControl::Absent;
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
            return egl::Error(EGL_NOT_INITIALIZED, "Invalid visual ID requested.");
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
        int attribList[] =
        {
            // We want RGBA8 and DEPTH24_STENCIL8
            GLX_RED_SIZE, 8,
            GLX_GREEN_SIZE, 8,
            GLX_BLUE_SIZE, 8,
            GLX_ALPHA_SIZE, 8,
            GLX_DEPTH_SIZE, 24,
            GLX_STENCIL_SIZE, 8,
            // We want RGBA rendering (vs COLOR_INDEX) and doublebuffer
            GLX_RENDER_TYPE, GLX_RGBA_BIT,
            // Double buffer is not strictly required as a non-doublebuffer
            // context can work with a doublebuffered surface, but it still
            // flickers and all applications want doublebuffer anyway.
            GLX_DOUBLEBUFFER, True,
            // All of these must be supported for full EGL support
            GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT | GLX_PBUFFER_BIT | GLX_PIXMAP_BIT,
            // This makes sure the config have an associated visual Id
            GLX_X_RENDERABLE, True,
            GLX_CONFIG_CAVEAT, GLX_NONE,
            None
        };
        glx::FBConfig *candidates = mGLX.chooseFBConfig(attribList, &nConfigs);
        if (nConfigs == 0)
        {
            XFree(candidates);
            return egl::Error(EGL_NOT_INITIALIZED, "Could not find a decent GLX FBConfig to create the context.");
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
            return egl::Error(EGL_NOT_INITIALIZED,
                              "Cannot create an OpenGL ES platform on GLX without the "
                              "GLX_ARB_create_context extension.");
        }

        XVisualInfo visualTemplate;
        visualTemplate.visualid = getGLXFBConfigAttrib(mContextConfig, GLX_VISUAL_ID);

        int numVisuals       = 0;
        XVisualInfo *visuals =
            XGetVisualInfo(mXDisplay, VisualIDMask, &visualTemplate, &numVisuals);
        if (numVisuals <= 0)
        {
            return egl::Error(EGL_NOT_INITIALIZED,
                              "Could not get the visual info from the fb config");
        }
        ASSERT(numVisuals == 1);

        mContext = mGLX.createContext(&visuals[0], nullptr, true);
        XFree(visuals);

        if (!mContext)
        {
            return egl::Error(EGL_NOT_INITIALIZED, "Could not create GL context.");
        }
    }
    ASSERT(mContext);

    // FunctionsGL and DisplayGL need to make a few GL calls, for example to
    // query the version of the context so we need to make the context current.
    // glXMakeCurrent requires a GLXDrawable so we create a temporary Pbuffer
    // (of size 1, 1) for the duration of these calls.
    // Ideally we would want to unset the current context and destroy the pbuffer
    // before going back to the application but this is TODO
    // We could use a pbuffer of size (0, 0) but it fails on the Intel Mesa driver
    // as commented on https://bugs.freedesktop.org/show_bug.cgi?id=38869 so we
    // use (1, 1) instead.

    int dummyPbufferAttribs[] =
    {
        GLX_PBUFFER_WIDTH, 1,
        GLX_PBUFFER_HEIGHT, 1,
        None,
    };
    mDummyPbuffer = mGLX.createPbuffer(mContextConfig, dummyPbufferAttribs);
    if (!mDummyPbuffer)
    {
        return egl::Error(EGL_NOT_INITIALIZED, "Could not create the dummy pbuffer.");
    }

    if (!mGLX.makeCurrent(mDummyPbuffer, mContext))
    {
        return egl::Error(EGL_NOT_INITIALIZED, "Could not make the dummy pbuffer current.");
    }

    mFunctionsGL = new FunctionsGLGLX(mGLX.getProc);
    mFunctionsGL->initialize();

    // TODO(cwallez, angleproject:1303) Disable the OpenGL ES backend on Linux NVIDIA and Intel as
    // it has problems on our automated testing. An OpenGL ES backend might not trigger this test if
    // there is no Desktop OpenGL support, but that's not the case in our automated testing.
    VendorID vendor = GetVendorID(mFunctionsGL);
    bool isOpenGLES =
        eglAttributes.get(EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE) ==
        EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE;
    if (isOpenGLES && (vendor == VENDOR_ID_INTEL || vendor == VENDOR_ID_NVIDIA))
    {
        return egl::Error(EGL_NOT_INITIALIZED, "Intel or NVIDIA OpenGL ES drivers are not supported.");
    }

    syncXCommands();

    std::string rendererString =
        reinterpret_cast<const char*>(mFunctionsGL->getString(GL_RENDERER));
    mIsMesa = rendererString.find("Mesa") != std::string::npos;

    std::string version;
    getDriverVersion(&version);

    return DisplayGL::initialize(display);
}

void DisplayGLX::terminate()
{
    DisplayGL::terminate();

    if (mDummyPbuffer)
    {
        mGLX.destroyPbuffer(mDummyPbuffer);
        mDummyPbuffer = 0;
    }

    if (mContext)
    {
        mGLX.destroyContext(mContext);
        mContext = nullptr;
    }

    mGLX.terminate();

    SafeDelete(mFunctionsGL);
}

SurfaceImpl *DisplayGLX::createWindowSurface(const egl::Config *configuration,
                                             EGLNativeWindowType window,
                                             const egl::AttributeMap &attribs)
{
    ASSERT(configIdToGLXConfig.count(configuration->configID) > 0);
    glx::FBConfig fbConfig = configIdToGLXConfig[configuration->configID];

    return new WindowSurfaceGLX(mGLX, this, this->getRenderer(), window, mGLX.getDisplay(),
                                mContext, fbConfig);
}

SurfaceImpl *DisplayGLX::createPbufferSurface(const egl::Config *configuration,
                                              const egl::AttributeMap &attribs)
{
    ASSERT(configIdToGLXConfig.count(configuration->configID) > 0);
    glx::FBConfig fbConfig = configIdToGLXConfig[configuration->configID];

    EGLint width  = static_cast<EGLint>(attribs.get(EGL_WIDTH, 0));
    EGLint height = static_cast<EGLint>(attribs.get(EGL_HEIGHT, 0));
    bool largest = (attribs.get(EGL_LARGEST_PBUFFER, EGL_FALSE) == EGL_TRUE);

    return new PbufferSurfaceGLX(this->getRenderer(), width, height, largest, mGLX, mContext,
                                 fbConfig);
}

SurfaceImpl* DisplayGLX::createPbufferFromClientBuffer(const egl::Config *configuration,
                                                       EGLClientBuffer shareHandle,
                                                       const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return nullptr;
}

SurfaceImpl *DisplayGLX::createPixmapSurface(const egl::Config *configuration,
                                             NativePixmapType nativePixmap,
                                             const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return nullptr;
}

egl::Error DisplayGLX::getDevice(DeviceImpl **device)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_DISPLAY);
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
            return egl::Error(EGL_NOT_INITIALIZED,
                              "Cannot create an OpenGL ES platform on GLX without the "
                              "GLX_EXT_create_context_es_profile extension.");
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

    // It is commonly assumed that glXCreateContextAttrib will create a context
    // of the highest version possible but it is not specified in the spec and
    // is not true on the Mesa drivers. Instead we try to create a context per
    // GL version until we succeed, starting from newer version. When the default
    // platform is selected, if no desktop context can be created, we fallback to
    // an ES context.
    // clang-format off
    const gl::Version desktopVersionsFrom3_2[] = {
        gl::Version(4, 5),
        gl::Version(4, 4),
        gl::Version(4, 3),
        gl::Version(4, 2),
        gl::Version(4, 1),
        gl::Version(4, 0),
        gl::Version(3, 3),
        gl::Version(3, 2),
    };
    const gl::Version desktopVersionsPre3_2[] = {
        gl::Version(3, 1),
        gl::Version(3, 0),
        gl::Version(2, 0),
        gl::Version(1, 5),
        gl::Version(1, 4),
        gl::Version(1, 3),
        gl::Version(1, 2),
        gl::Version(1, 1),
        gl::Version(1, 0),
    };
    const gl::Version esVersionsFrom2_0[] = {
        gl::Version(3, 2),
        gl::Version(3, 1),
        gl::Version(3, 0),
        gl::Version(2, 0),
    };
    // clang-format on

    // NOTE: below we return as soon as we're able to create a context so the
    // "error" variable is EGL_SUCCESS when returned contrary to the common idiom
    // of returning "error" when there is an actual error.
    if (requestedDisplayType != EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE)
    {
        for (auto &version : desktopVersionsFrom3_2)
        {
            egl::Error error =
                createContextAttribs(config, version, GLX_CONTEXT_CORE_PROFILE_BIT_ARB, context);
            if (!error.isError())
            {
                return error;
            }
        }
        for (auto &version : desktopVersionsPre3_2)
        {
            egl::Error error = createContextAttribs(config, version, 0, context);
            if (!error.isError())
            {
                return error;
            }
        }
    }

    if (requestedDisplayType != EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE)
    {
        for (auto &version : esVersionsFrom2_0)
        {
            egl::Error error =
                createContextAttribs(config, version, GLX_CONTEXT_ES2_PROFILE_BIT_EXT, context);
            if (!error.isError())
            {
                return error;
            }
        }
    }

    return egl::Error(EGL_NOT_INITIALIZED, "Could not create a backing OpenGL context.");
}

egl::ConfigSet DisplayGLX::generateConfigs() const
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

    int contextAccumRedSize = getGLXFBConfigAttrib(mContextConfig, GLX_ACCUM_RED_SIZE);
    int contextAccumGreenSize = getGLXFBConfigAttrib(mContextConfig, GLX_ACCUM_GREEN_SIZE);
    int contextAccumBlueSize = getGLXFBConfigAttrib(mContextConfig, GLX_ACCUM_BLUE_SIZE);
    int contextAccumAlphaSize = getGLXFBConfigAttrib(mContextConfig, GLX_ACCUM_ALPHA_SIZE);

    int attribList[] =
    {
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_X_RENDERABLE, True,
        GLX_DOUBLEBUFFER, True,
        None,
    };

    int glxConfigCount;
    glx::FBConfig *glxConfigs = mGLX.chooseFBConfig(attribList, &glxConfigCount);

    for (int i = 0; i < glxConfigCount; i++)
    {
        glx::FBConfig glxConfig = glxConfigs[i];
        egl::Config config;

        // Native stuff
        config.nativeVisualID = getGLXFBConfigAttrib(glxConfig, GLX_VISUAL_ID);
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
        config.redSize = getGLXFBConfigAttrib(glxConfig, GLX_RED_SIZE);
        config.greenSize = getGLXFBConfigAttrib(glxConfig, GLX_GREEN_SIZE);
        config.blueSize = getGLXFBConfigAttrib(glxConfig, GLX_BLUE_SIZE);
        config.alphaSize = getGLXFBConfigAttrib(glxConfig, GLX_ALPHA_SIZE);
        config.depthSize = getGLXFBConfigAttrib(glxConfig, GLX_DEPTH_SIZE);
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
        config.luminanceSize = 0;
        config.alphaMaskSize = 0;

        config.bufferSize = config.redSize + config.greenSize + config.blueSize + config.alphaSize;

        // Multisample and accumulation buffers
        int samples = mHasMultisample ? getGLXFBConfigAttrib(glxConfig, GLX_SAMPLES) : 0;
        int sampleBuffers =
            mHasMultisample ? getGLXFBConfigAttrib(glxConfig, GLX_SAMPLE_BUFFERS) : 0;

        int accumRedSize = getGLXFBConfigAttrib(glxConfig, GLX_ACCUM_RED_SIZE);
        int accumGreenSize = getGLXFBConfigAttrib(glxConfig, GLX_ACCUM_GREEN_SIZE);
        int accumBlueSize = getGLXFBConfigAttrib(glxConfig, GLX_ACCUM_BLUE_SIZE);
        int accumAlphaSize = getGLXFBConfigAttrib(glxConfig, GLX_ACCUM_ALPHA_SIZE);

        if (samples != contextSamples ||
            sampleBuffers != contextSampleBuffers ||
            accumRedSize != contextAccumRedSize ||
            accumGreenSize != contextAccumGreenSize ||
            accumBlueSize != contextAccumBlueSize ||
            accumAlphaSize != contextAccumAlphaSize)
        {
            continue;
        }

        config.samples = samples;
        config.sampleBuffers = sampleBuffers;

        // Transparency
        if (getGLXFBConfigAttrib(glxConfig, GLX_TRANSPARENT_TYPE) == GLX_TRANSPARENT_RGB)
        {
            config.transparentType = EGL_TRANSPARENT_RGB;
            config.transparentRedValue = getGLXFBConfigAttrib(glxConfig, GLX_TRANSPARENT_RED_VALUE);
            config.transparentGreenValue = getGLXFBConfigAttrib(glxConfig, GLX_TRANSPARENT_GREEN_VALUE);
            config.transparentBlueValue = getGLXFBConfigAttrib(glxConfig, GLX_TRANSPARENT_BLUE_VALUE);
        }
        else
        {
            config.transparentType = EGL_NONE;
        }

        // Pbuffer
        config.maxPBufferWidth = getGLXFBConfigAttrib(glxConfig, GLX_MAX_PBUFFER_WIDTH);
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

        config.bindToTextureRGB = EGL_FALSE;
        config.bindToTextureRGBA = EGL_FALSE;

        int glxDrawable = getGLXFBConfigAttrib(glxConfig, GLX_DRAWABLE_TYPE);
        config.surfaceType = 0 |
            (glxDrawable & GLX_WINDOW_BIT ? EGL_WINDOW_BIT : 0) |
            (glxDrawable & GLX_PBUFFER_BIT ? EGL_PBUFFER_BIT : 0) |
            (glxDrawable & GLX_PIXMAP_BIT ? EGL_PIXMAP_BIT : 0);

        config.minSwapInterval = mMinSwapInterval;
        config.maxSwapInterval = mMaxSwapInterval;

        // TODO(cwallez) wildly guessing these formats, another TODO says they should be removed anyway
        config.renderTargetFormat = GL_RGBA8;
        config.depthStencilFormat = GL_DEPTH24_STENCIL8;

        config.conformant = EGL_OPENGL_ES2_BIT | (supportsES3 ? EGL_OPENGL_ES3_BIT_KHR : 0);
        config.renderableType = config.conformant;

        // TODO(cwallez) I have no idea what this is
        config.matchNativePixmap = EGL_NONE;

        int id = configs.add(config);
        configIdToGLXConfig[id] = glxConfig;
    }

    XFree(glxConfigs);

    return configs;
}

bool DisplayGLX::isDeviceLost() const
{
    // UNIMPLEMENTED();
    return false;
}

bool DisplayGLX::testDeviceLost()
{
    // UNIMPLEMENTED();
    return false;
}

egl::Error DisplayGLX::restoreLostDevice()
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_DISPLAY);
}

bool DisplayGLX::isValidNativeWindow(EGLNativeWindowType window) const
{
    // There is no function in Xlib to check the validity of a Window directly.
    // However a small number of functions used to obtain window information
    // return a status code (0 meaning failure) and guarantee that they will
    // fail if the window doesn't exist (the rational is that these function
    // are used by window managers). Out of these function we use XQueryTree
    // as it seems to be the simplest; a drawback is that it will allocate
    // memory for the list of children, becasue we use a child window for
    // WindowSurface.
    Window root;
    Window parent;
    Window *children = nullptr;
    unsigned nChildren;
    int status = XQueryTree(mGLX.getDisplay(), window, &root, &parent, &children, &nChildren);
    if (children)
    {
        XFree(children);
    }
    return status != 0;
}

std::string DisplayGLX::getVendorString() const
{
    // UNIMPLEMENTED();
    return "";
}

egl::Error DisplayGLX::waitClient() const
{
    mGLX.waitGL();
    return egl::Error(EGL_SUCCESS);
}

egl::Error DisplayGLX::getDriverVersion(std::string *version) const
{
    VendorID vendor = GetVendorID(mFunctionsGL);

    switch (vendor)
    {
        case VENDOR_ID_NVIDIA:
            return getNVIDIADriverVersion(version);
        default:
            *version = "";
            return egl::Error(EGL_SUCCESS);
    }
}

egl::Error DisplayGLX::waitNative(EGLint engine,
                                  egl::Surface *drawSurface,
                                  egl::Surface *readSurface) const
{
    // eglWaitNative is used to notice the driver of changes in X11 for the current surface, such as
    // changes of the window size. We use this event to update the child window of WindowSurfaceGLX
    // to match its parent window's size.
    // Handling eglWaitNative this way helps the application control when resize happens. This is
    // important because drivers have a tendency to clobber the back buffer when the windows are
    // resized. See http://crbug.com/326995
    if (drawSurface != nullptr)
    {
        SurfaceGLX *glxDrawSurface = GetImplAs<SurfaceGLX>(drawSurface);
        egl::Error error = glxDrawSurface->checkForResize();
        if (error.isError())
        {
            return error;
        }
    }

    if (readSurface != drawSurface && readSurface != nullptr)
    {
        SurfaceGLX *glxReadSurface = GetImplAs<SurfaceGLX>(readSurface);
        egl::Error error = glxReadSurface->checkForResize();
        if (error.isError())
        {
            return error;
        }
    }

    // We still need to forward the resizing of the child window to the driver.
    mGLX.waitX();
    return egl::Error(EGL_SUCCESS);
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

const FunctionsGL *DisplayGLX::getFunctionsGL() const
{
    return mFunctionsGL;
}

void DisplayGLX::generateExtensions(egl::DisplayExtensions *outExtensions) const
{
    outExtensions->createContext = true;
    outExtensions->createContextNoError = true;
}

void DisplayGLX::generateCaps(egl::Caps *outCaps) const
{
    // UNIMPLEMENTED();
    outCaps->textureNPOT = true;
}

int DisplayGLX::getGLXFBConfigAttrib(glx::FBConfig config, int attrib) const
{
    int result;
    mGLX.getFBConfigAttrib(config, attrib, &result);
    return result;
}

egl::Error DisplayGLX::createContextAttribs(glx::FBConfig,
                                            gl::Version version,
                                            int profileMask,
                                            glx::Context *context) const
{
    std::vector<int> attribs;
    attribs.push_back(GLX_CONTEXT_MAJOR_VERSION_ARB);
    attribs.push_back(version.major);

    attribs.push_back(GLX_CONTEXT_MINOR_VERSION_ARB);
    attribs.push_back(version.minor);

    if (profileMask != 0 && mHasARBCreateContextProfile)
    {
        attribs.push_back(GLX_CONTEXT_PROFILE_MASK_ARB);
        attribs.push_back(profileMask);
    }

    attribs.push_back(None);

    // When creating a context with glXCreateContextAttribsARB, a variety of X11 errors can
    // be generated. To prevent these errors from crashing our process, we simply ignore
    // them and only look if GLXContext was created.
    auto oldErrorHandler = XSetErrorHandler(IgnoreX11Errors);
    *context = mGLX.createContextAttribsARB(mContextConfig, nullptr, True, attribs.data());
    XSetErrorHandler(oldErrorHandler);

    if (!*context)
    {
        return egl::Error(EGL_NOT_INITIALIZED, "Could not create GL context.");
    }
    return egl::Error(EGL_SUCCESS);
}

egl::Error DisplayGLX::getNVIDIADriverVersion(std::string *version) const
{
    *version = "";

    int eventBase = 0;
    int errorBase = 0;
    if (XNVCTRLQueryExtension(mXDisplay, &eventBase, &errorBase))
    {
        int screenCount = ScreenCount(mXDisplay);
        for (int screen = 0; screen < screenCount; ++screen)
        {
            char *buffer = nullptr;
            if (XNVCTRLIsNvScreen(mXDisplay, screen) &&
                XNVCTRLQueryStringAttribute(mXDisplay, screen, 0,
                                            NV_CTRL_STRING_NVIDIA_DRIVER_VERSION, &buffer))
            {
                *version = buffer;
                XFree(buffer);
            }
        }
    }

    return egl::Error(EGL_SUCCESS);
}
}
