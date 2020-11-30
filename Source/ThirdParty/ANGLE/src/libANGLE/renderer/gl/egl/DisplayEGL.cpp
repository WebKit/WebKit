//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// DisplayEGL.cpp: Common across EGL parts of platform specific egl::Display implementations

#include "libANGLE/renderer/gl/egl/DisplayEGL.h"

#include "common/debug.h"
#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
#include "libANGLE/Surface.h"
#include "libANGLE/renderer/gl/ContextGL.h"
#include "libANGLE/renderer/gl/RendererGL.h"
#include "libANGLE/renderer/gl/egl/ContextEGL.h"
#include "libANGLE/renderer/gl/egl/DmaBufImageSiblingEGL.h"
#include "libANGLE/renderer/gl/egl/FunctionsEGLDL.h"
#include "libANGLE/renderer/gl/egl/ImageEGL.h"
#include "libANGLE/renderer/gl/egl/PbufferSurfaceEGL.h"
#include "libANGLE/renderer/gl/egl/RendererEGL.h"
#include "libANGLE/renderer/gl/egl/SyncEGL.h"
#include "libANGLE/renderer/gl/egl/WindowSurfaceEGL.h"
#include "libANGLE/renderer/gl/renderergl_utils.h"

namespace
{

EGLint ESBitFromPlatformAttrib(const rx::FunctionsEGL *egl, const EGLAttrib platformAttrib)
{
    EGLint esBit = EGL_NONE;
    switch (platformAttrib)
    {
        case EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE:
        {
            esBit = EGL_OPENGL_BIT;
            break;
        }

        case EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE:
        {
            static_assert(EGL_OPENGL_ES3_BIT == EGL_OPENGL_ES3_BIT_KHR,
                          "Extension define must match core");

            gl::Version eglVersion(egl->majorVersion, egl->minorVersion);
            esBit = (eglVersion >= gl::Version(1, 5) || egl->hasExtension("EGL_KHR_create_context"))
                        ? EGL_OPENGL_ES3_BIT
                        : EGL_OPENGL_ES2_BIT;
            break;
        }

        default:
            break;
    }
    return esBit;
}

class WorkerContextEGL final : public rx::WorkerContext
{
  public:
    WorkerContextEGL(EGLContext context, rx::FunctionsEGL *functions, EGLSurface pbuffer);
    ~WorkerContextEGL() override;

    bool makeCurrent() override;
    void unmakeCurrent() override;

  private:
    EGLContext mContext;
    rx::FunctionsEGL *mFunctions;
    EGLSurface mPbuffer;
};

WorkerContextEGL::WorkerContextEGL(EGLContext context,
                                   rx::FunctionsEGL *functions,
                                   EGLSurface pbuffer)
    : mContext(context), mFunctions(functions), mPbuffer(pbuffer)
{}

WorkerContextEGL::~WorkerContextEGL()
{
    mFunctions->destroyContext(mContext);
}

bool WorkerContextEGL::makeCurrent()
{
    if (mFunctions->makeCurrent(mPbuffer, mContext) == EGL_FALSE)
    {
        ERR() << "Unable to make the EGL context current.";
        return false;
    }
    return true;
}

void WorkerContextEGL::unmakeCurrent()
{
    mFunctions->makeCurrent(EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

}  // namespace

namespace rx
{

DisplayEGL::DisplayEGL(const egl::DisplayState &state)
    : DisplayGL(state), mRenderer(nullptr), mEGL(nullptr), mConfig(EGL_NO_CONFIG_KHR)
{}

DisplayEGL::~DisplayEGL() {}

ImageImpl *DisplayEGL::createImage(const egl::ImageState &state,
                                   const gl::Context *context,
                                   EGLenum target,
                                   const egl::AttributeMap &attribs)
{
    return new ImageEGL(state, context, target, attribs, mEGL);
}

EGLSyncImpl *DisplayEGL::createSync(const egl::AttributeMap &attribs)
{
    return new SyncEGL(attribs, mEGL);
}

std::string DisplayEGL::getVendorString() const
{
    const char *vendor = mEGL->queryString(EGL_VENDOR);
    ASSERT(vendor);
    return vendor;
}

egl::Error DisplayEGL::initializeContext(EGLContext shareContext,
                                         const egl::AttributeMap &eglAttributes,
                                         EGLContext *outContext,
                                         native_egl::AttributeVector *outAttribs) const
{
    gl::Version eglVersion(mEGL->majorVersion, mEGL->minorVersion);

    EGLint requestedMajor =
        eglAttributes.getAsInt(EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE, EGL_DONT_CARE);
    EGLint requestedMinor =
        eglAttributes.getAsInt(EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE, EGL_DONT_CARE);
    bool initializeRequested = requestedMajor != EGL_DONT_CARE && requestedMinor != EGL_DONT_CARE;

    static_assert(EGL_CONTEXT_MAJOR_VERSION == EGL_CONTEXT_MAJOR_VERSION_KHR,
                  "Major Version define should match");
    static_assert(EGL_CONTEXT_MINOR_VERSION == EGL_CONTEXT_MINOR_VERSION_KHR,
                  "Minor Version define should match");

    std::vector<native_egl::AttributeVector> contextAttribLists;
    if (eglVersion >= gl::Version(1, 5) || mEGL->hasExtension("EGL_KHR_create_context"))
    {
        if (initializeRequested)
        {
            contextAttribLists.push_back({EGL_CONTEXT_MAJOR_VERSION, requestedMajor,
                                          EGL_CONTEXT_MINOR_VERSION, requestedMinor, EGL_NONE});
        }
        else
        {
            // clang-format off
            const gl::Version esVersionsFrom2_0[] = {
                gl::Version(3, 2),
                gl::Version(3, 1),
                gl::Version(3, 0),
                gl::Version(2, 0),
            };
            // clang-format on

            for (const auto &version : esVersionsFrom2_0)
            {
                contextAttribLists.push_back(
                    {EGL_CONTEXT_MAJOR_VERSION, static_cast<EGLint>(version.major),
                     EGL_CONTEXT_MINOR_VERSION, static_cast<EGLint>(version.minor), EGL_NONE});
            }
        }
    }
    else
    {
        if (initializeRequested && (requestedMajor != 2 || requestedMinor != 0))
        {
            return egl::EglBadAttribute() << "Unsupported requested context version";
        }
        contextAttribLists.push_back({EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE});
    }

    EGLContext context = EGL_NO_CONTEXT;
    for (const auto &attribList : contextAttribLists)
    {
        context = mEGL->createContext(mConfig, shareContext, attribList.data());
        if (context != EGL_NO_CONTEXT)
        {
            *outContext = context;
            *outAttribs = attribList;
            return egl::NoError();
        }
    }

    return egl::Error(mEGL->getError(), "eglCreateContext failed");
}

egl::Error DisplayEGL::initialize(egl::Display *display)
{
    mDisplayAttributes = display->getAttributeMap();
    mEGL               = new FunctionsEGLDL();

    void *eglHandle =
        reinterpret_cast<void *>(mDisplayAttributes.get(EGL_PLATFORM_ANGLE_EGL_HANDLE_ANGLE, 0));
    ANGLE_TRY(mEGL->initialize(display->getNativeDisplayId(), "libEGL.so.1", eglHandle));

    gl::Version eglVersion(mEGL->majorVersion, mEGL->minorVersion);
    if (eglVersion < gl::Version(1, 4))
    {
        return egl::EglNotInitialized() << "EGL >= 1.4 is required";
    }

    // Only support modern EGL implementation to keep default implementation
    // simple.
    const char *necessaryExtensions[] = {
        "EGL_KHR_no_config_context",
        "EGL_KHR_surfaceless_context",
    };

    for (const char *ext : necessaryExtensions)
    {
        if (!mEGL->hasExtension(ext))
        {
            return egl::EglNotInitialized() << "need " << ext;
        }
    }

    const EGLAttrib platformAttrib = mDisplayAttributes.get(EGL_PLATFORM_ANGLE_TYPE_ANGLE, 0);
    EGLint esBit                   = ESBitFromPlatformAttrib(mEGL, platformAttrib);
    if (esBit == EGL_NONE)
    {
        return egl::EglNotInitialized() << "No matching ES Bit";
    }

    std::vector<EGLint> configAttribListBase = {
        EGL_COLOR_BUFFER_TYPE, EGL_RGB_BUFFER, EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
        EGL_CONFIG_CAVEAT,     EGL_NONE,       EGL_CONFORMANT,   esBit,
        EGL_RENDERABLE_TYPE,   esBit,          EGL_NONE};

    mConfigAttribList = configAttribListBase;

    EGLContext context = EGL_NO_CONTEXT;
    native_egl::AttributeVector attribs;
    ANGLE_TRY(initializeContext(EGL_NO_CONTEXT, mDisplayAttributes, &context, &attribs));

    if (!mEGL->makeCurrent(EGL_NO_SURFACE, context))
    {
        return egl::EglNotInitialized() << "Could not make context current.";
    }

    std::unique_ptr<FunctionsGL> functionsGL(mEGL->makeFunctionsGL());
    functionsGL->initialize(mDisplayAttributes);

    mRenderer.reset(
        new RendererEGL(std::move(functionsGL), mDisplayAttributes, this, context, attribs));
    const gl::Version &maxVersion = mRenderer->getMaxSupportedESVersion();
    if (maxVersion < gl::Version(2, 0))
    {
        return egl::EglNotInitialized() << "OpenGL ES 2.0 is not supportable.";
    }

    ANGLE_TRY(DisplayGL::initialize(display));

    return egl::NoError();
}

void DisplayEGL::terminate()
{
    DisplayGL::terminate();

    EGLBoolean success = mEGL->makeCurrent(EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (success == EGL_FALSE)
    {
        ERR() << "eglMakeCurrent error " << egl::Error(mEGL->getError());
    }

    mRenderer.reset();

    mCurrentNativeContexts.clear();

    egl::Error result = mEGL->terminate();
    if (result.isError())
    {
        ERR() << "eglTerminate error " << result;
    }

    SafeDelete(mEGL);
}

SurfaceImpl *DisplayEGL::createWindowSurface(const egl::SurfaceState &state,
                                             EGLNativeWindowType window,
                                             const egl::AttributeMap &attribs)
{
    EGLConfig config;
    EGLint numConfig;
    EGLBoolean success;

    const EGLint configAttribList[] = {EGL_CONFIG_ID, mConfigIds[state.config->configID], EGL_NONE};
    success                         = mEGL->chooseConfig(configAttribList, &config, 1, &numConfig);
    ASSERT(success && numConfig == 1);

    return new WindowSurfaceEGL(state, mEGL, config, window);
}

SurfaceImpl *DisplayEGL::createPbufferSurface(const egl::SurfaceState &state,
                                              const egl::AttributeMap &attribs)
{
    EGLConfig config;
    EGLint numConfig;
    EGLBoolean success;

    const EGLint configAttribList[] = {EGL_CONFIG_ID, mConfigIds[state.config->configID], EGL_NONE};
    success                         = mEGL->chooseConfig(configAttribList, &config, 1, &numConfig);
    ASSERT(success && numConfig == 1);

    return new PbufferSurfaceEGL(state, mEGL, config);
}

SurfaceImpl *DisplayEGL::createPbufferFromClientBuffer(const egl::SurfaceState &state,
                                                       EGLenum buftype,
                                                       EGLClientBuffer clientBuffer,
                                                       const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return nullptr;
}

SurfaceImpl *DisplayEGL::createPixmapSurface(const egl::SurfaceState &state,
                                             NativePixmapType nativePixmap,
                                             const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return nullptr;
}

ContextImpl *DisplayEGL::createContext(const gl::State &state,
                                       gl::ErrorSet *errorSet,
                                       const egl::Config *configuration,
                                       const gl::Context *shareContext,
                                       const egl::AttributeMap &attribs)
{
    std::shared_ptr<RendererEGL> renderer;
    EGLContext nativeShareContext = EGL_NO_CONTEXT;
    if (shareContext)
    {
        ContextEGL *shareContextEGL = GetImplAs<ContextEGL>(shareContext);
        nativeShareContext          = shareContextEGL->getContext();
    }

    egl::Error error = createRenderer(nativeShareContext, &renderer);
    if (error.isError())
    {
        ERR() << "Failed to create a shared renderer: " << error.getMessage();
        return nullptr;
    }

    return new ContextEGL(state, errorSet, renderer);
}

template <typename T>
void DisplayEGL::getConfigAttrib(EGLConfig config, EGLint attribute, T *value) const
{
    EGLint tmp;
    EGLBoolean success = mEGL->getConfigAttrib(config, attribute, &tmp);
    ASSERT(success == EGL_TRUE);
    *value = tmp;
}

template <typename T, typename U>
void DisplayEGL::getConfigAttribIfExtension(EGLConfig config,
                                            EGLint attribute,
                                            T *value,
                                            const char *extension,
                                            const U &defaultValue) const
{
    if (mEGL->hasExtension(extension))
    {
        getConfigAttrib(config, attribute, value);
    }
    else
    {
        *value = static_cast<T>(defaultValue);
    }
}

egl::ConfigSet DisplayEGL::generateConfigs()
{
    egl::ConfigSet configSet;
    mConfigIds.clear();

    EGLint numConfigs;
    EGLBoolean success = mEGL->chooseConfig(mConfigAttribList.data(), nullptr, 0, &numConfigs);
    ASSERT(success == EGL_TRUE && numConfigs > 0);

    std::vector<EGLConfig> configs(numConfigs);
    EGLint numConfigs2;
    success =
        mEGL->chooseConfig(mConfigAttribList.data(), configs.data(), numConfigs, &numConfigs2);
    ASSERT(success == EGL_TRUE && numConfigs2 == numConfigs);

    for (int i = 0; i < numConfigs; i++)
    {
        egl::Config config;

        getConfigAttrib(configs[i], EGL_BUFFER_SIZE, &config.bufferSize);
        getConfigAttrib(configs[i], EGL_RED_SIZE, &config.redSize);
        getConfigAttrib(configs[i], EGL_GREEN_SIZE, &config.greenSize);
        getConfigAttrib(configs[i], EGL_BLUE_SIZE, &config.blueSize);
        getConfigAttrib(configs[i], EGL_LUMINANCE_SIZE, &config.luminanceSize);
        getConfigAttrib(configs[i], EGL_ALPHA_SIZE, &config.alphaSize);
        getConfigAttrib(configs[i], EGL_ALPHA_MASK_SIZE, &config.alphaMaskSize);
        getConfigAttrib(configs[i], EGL_BIND_TO_TEXTURE_RGB, &config.bindToTextureRGB);
        getConfigAttrib(configs[i], EGL_BIND_TO_TEXTURE_RGBA, &config.bindToTextureRGBA);
        getConfigAttrib(configs[i], EGL_COLOR_BUFFER_TYPE, &config.colorBufferType);
        getConfigAttrib(configs[i], EGL_CONFIG_CAVEAT, &config.configCaveat);
        getConfigAttrib(configs[i], EGL_CONFIG_ID, &config.configID);
        getConfigAttrib(configs[i], EGL_CONFORMANT, &config.conformant);
        getConfigAttrib(configs[i], EGL_DEPTH_SIZE, &config.depthSize);
        getConfigAttrib(configs[i], EGL_LEVEL, &config.level);
        getConfigAttrib(configs[i], EGL_MAX_PBUFFER_WIDTH, &config.maxPBufferWidth);
        getConfigAttrib(configs[i], EGL_MAX_PBUFFER_HEIGHT, &config.maxPBufferHeight);
        getConfigAttrib(configs[i], EGL_MAX_PBUFFER_PIXELS, &config.maxPBufferPixels);
        getConfigAttrib(configs[i], EGL_MAX_SWAP_INTERVAL, &config.maxSwapInterval);
        getConfigAttrib(configs[i], EGL_MIN_SWAP_INTERVAL, &config.minSwapInterval);
        getConfigAttrib(configs[i], EGL_NATIVE_RENDERABLE, &config.nativeRenderable);
        getConfigAttrib(configs[i], EGL_NATIVE_VISUAL_ID, &config.nativeVisualID);
        getConfigAttrib(configs[i], EGL_NATIVE_VISUAL_TYPE, &config.nativeVisualType);
        getConfigAttrib(configs[i], EGL_RENDERABLE_TYPE, &config.renderableType);
        getConfigAttrib(configs[i], EGL_SAMPLE_BUFFERS, &config.sampleBuffers);
        getConfigAttrib(configs[i], EGL_SAMPLES, &config.samples);
        getConfigAttrib(configs[i], EGL_STENCIL_SIZE, &config.stencilSize);
        getConfigAttrib(configs[i], EGL_SURFACE_TYPE, &config.surfaceType);
        getConfigAttrib(configs[i], EGL_TRANSPARENT_TYPE, &config.transparentType);
        getConfigAttrib(configs[i], EGL_TRANSPARENT_RED_VALUE, &config.transparentRedValue);
        getConfigAttrib(configs[i], EGL_TRANSPARENT_GREEN_VALUE, &config.transparentGreenValue);
        getConfigAttrib(configs[i], EGL_TRANSPARENT_BLUE_VALUE, &config.transparentBlueValue);
        getConfigAttribIfExtension(configs[i], EGL_COLOR_COMPONENT_TYPE_EXT,
                                   &config.colorComponentType, "EGL_EXT_pixel_format_float",
                                   EGL_COLOR_COMPONENT_TYPE_FIXED_EXT);

        // Pixmaps are not supported on EGL, make sure the config doesn't expose them.
        config.surfaceType &= ~EGL_PIXMAP_BIT;

        if (config.colorBufferType == EGL_RGB_BUFFER)
        {
            ASSERT(config.colorComponentType == EGL_COLOR_COMPONENT_TYPE_FIXED_EXT);
            if (config.redSize == 8 && config.greenSize == 8 && config.blueSize == 8 &&
                config.alphaSize == 8)
            {
                config.renderTargetFormat = GL_RGBA8;
            }
            else if (config.redSize == 8 && config.greenSize == 8 && config.blueSize == 8 &&
                     config.alphaSize == 0)
            {
                config.renderTargetFormat = GL_RGB8;
            }
            else if (config.redSize == 5 && config.greenSize == 6 && config.blueSize == 5 &&
                     config.alphaSize == 0)
            {
                config.renderTargetFormat = GL_RGB565;
            }
            else if (config.redSize == 5 && config.greenSize == 5 && config.blueSize == 5 &&
                     config.alphaSize == 1)
            {
                config.renderTargetFormat = GL_RGB5_A1;
            }
            else if (config.redSize == 4 && config.greenSize == 4 && config.blueSize == 4 &&
                     config.alphaSize == 4)
            {
                config.renderTargetFormat = GL_RGBA4;
            }
            else
            {
                ERR() << "RGBA(" << config.redSize << "," << config.greenSize << ","
                      << config.blueSize << "," << config.alphaSize << ") not handled";
                UNREACHABLE();
            }
        }
        else
        {
            UNREACHABLE();
        }

        if (config.depthSize == 0 && config.stencilSize == 0)
        {
            config.depthStencilFormat = GL_ZERO;
        }
        else if (config.depthSize == 16 && config.stencilSize == 0)
        {
            config.depthStencilFormat = GL_DEPTH_COMPONENT16;
        }
        else if (config.depthSize == 24 && config.stencilSize == 0)
        {
            config.depthStencilFormat = GL_DEPTH_COMPONENT24;
        }
        else if (config.depthSize == 24 && config.stencilSize == 8)
        {
            config.depthStencilFormat = GL_DEPTH24_STENCIL8;
        }
        else if (config.depthSize == 0 && config.stencilSize == 8)
        {
            config.depthStencilFormat = GL_STENCIL_INDEX8;
        }
        else
        {
            UNREACHABLE();
        }

        config.matchNativePixmap  = EGL_NONE;
        config.optimalOrientation = 0;

        int internalId         = configSet.add(config);
        mConfigIds[internalId] = config.configID;
    }

    return configSet;
}

bool DisplayEGL::testDeviceLost()
{
    return false;
}

egl::Error DisplayEGL::restoreLostDevice(const egl::Display *display)
{
    UNIMPLEMENTED();
    return egl::NoError();
}

bool DisplayEGL::isValidNativeWindow(EGLNativeWindowType window) const
{
    return true;
}

DeviceImpl *DisplayEGL::createDevice()
{
    UNIMPLEMENTED();
    return nullptr;
}

egl::Error DisplayEGL::waitClient(const gl::Context *context)
{
    UNIMPLEMENTED();
    return egl::NoError();
}

egl::Error DisplayEGL::waitNative(const gl::Context *context, EGLint engine)
{
    UNIMPLEMENTED();
    return egl::NoError();
}

egl::Error DisplayEGL::makeCurrent(egl::Surface *drawSurface,
                                   egl::Surface *readSurface,
                                   gl::Context *context)
{
    CurrentNativeContext &currentContext = mCurrentNativeContexts[std::this_thread::get_id()];

    EGLSurface newSurface = EGL_NO_SURFACE;
    if (drawSurface)
    {
        SurfaceEGL *drawSurfaceEGL = GetImplAs<SurfaceEGL>(drawSurface);
        newSurface                 = drawSurfaceEGL->getSurface();
    }

    EGLContext newContext = EGL_NO_CONTEXT;
    if (context)
    {
        ContextEGL *contextEGL = GetImplAs<ContextEGL>(context);
        newContext             = contextEGL->getContext();
    }

    if (newSurface != currentContext.surface || newContext != currentContext.context)
    {
        if (mEGL->makeCurrent(newSurface, newContext) == EGL_FALSE)
        {
            return egl::Error(mEGL->getError(), "eglMakeCurrent failed");
        }
        currentContext.surface = newSurface;
        currentContext.context = newContext;
    }

    return DisplayGL::makeCurrent(drawSurface, readSurface, context);
}

gl::Version DisplayEGL::getMaxSupportedESVersion() const
{
    return mRenderer->getMaxSupportedESVersion();
}

void DisplayEGL::destroyNativeContext(EGLContext context)
{
    // If this context is current, remove it from the tracking of current contexts to make sure we
    // don't try to make it current again.
    for (auto &currentContext : mCurrentNativeContexts)
    {
        if (currentContext.second.context == context)
        {
            currentContext.second.surface = EGL_NO_SURFACE;
            currentContext.second.context = EGL_NO_CONTEXT;
        }
    }

    mEGL->destroyContext(context);
}

void DisplayEGL::generateExtensions(egl::DisplayExtensions *outExtensions) const
{
    gl::Version eglVersion(mEGL->majorVersion, mEGL->minorVersion);

    outExtensions->createContextRobustness =
        mEGL->hasExtension("EGL_EXT_create_context_robustness");

    outExtensions->postSubBuffer    = false;  // Since SurfaceEGL::postSubBuffer is not implemented
    outExtensions->presentationTime = mEGL->hasExtension("EGL_ANDROID_presentation_time");

    // Contexts are virtualized so textures can be shared globally
    outExtensions->displayTextureShareGroup = true;

    // We will fallback to regular swap if swapBuffersWithDamage isn't
    // supported, so indicate support here to keep validation happy.
    outExtensions->swapBuffersWithDamage = true;

    outExtensions->image     = mEGL->hasExtension("EGL_KHR_image");
    outExtensions->imageBase = mEGL->hasExtension("EGL_KHR_image_base");
    // Pixmaps are not supported in ANGLE's EGL implementation.
    // outExtensions->imagePixmap = mEGL->hasExtension("EGL_KHR_image_pixmap");
    outExtensions->glTexture2DImage      = mEGL->hasExtension("EGL_KHR_gl_texture_2D_image");
    outExtensions->glTextureCubemapImage = mEGL->hasExtension("EGL_KHR_gl_texture_cubemap_image");
    outExtensions->glTexture3DImage      = mEGL->hasExtension("EGL_KHR_gl_texture_3D_image");
    outExtensions->glRenderbufferImage   = mEGL->hasExtension("EGL_KHR_gl_renderbuffer_image");
    outExtensions->pixelFormatFloat      = mEGL->hasExtension("EGL_EXT_pixel_format_float");

    outExtensions->glColorspace = mEGL->hasExtension("EGL_KHR_gl_colorspace");
    if (outExtensions->glColorspace)
    {
        outExtensions->glColorspaceDisplayP3Linear =
            mEGL->hasExtension("EGL_EXT_gl_colorspace_display_p3_linear");
        outExtensions->glColorspaceDisplayP3 =
            mEGL->hasExtension("EGL_EXT_gl_colorspace_display_p3");
        outExtensions->glColorspaceScrgb = mEGL->hasExtension("EGL_EXT_gl_colorspace_scrgb");
        outExtensions->glColorspaceScrgbLinear =
            mEGL->hasExtension("EGL_EXT_gl_colorspace_scrgb_linear");
        outExtensions->glColorspaceDisplayP3Passthrough =
            mEGL->hasExtension("EGL_EXT_gl_colorspace_display_p3_passthrough");
        outExtensions->imageGlColorspace = mEGL->hasExtension("EGL_EXT_image_gl_colorspace");
    }

    outExtensions->imageNativeBuffer = mEGL->hasExtension("EGL_ANDROID_image_native_buffer");

    outExtensions->getFrameTimestamps = mEGL->hasExtension("EGL_ANDROID_get_frame_timestamps");

    outExtensions->fenceSync =
        eglVersion >= gl::Version(1, 5) || mEGL->hasExtension("EGL_KHR_fence_sync");
    outExtensions->waitSync =
        eglVersion >= gl::Version(1, 5) || mEGL->hasExtension("EGL_KHR_wait_sync");

    outExtensions->getNativeClientBufferANDROID =
        mEGL->hasExtension("EGL_ANDROID_get_native_client_buffer");

    outExtensions->nativeFenceSyncANDROID = mEGL->hasExtension("EGL_ANDROID_native_fence_sync");

    outExtensions->noConfigContext = mEGL->hasExtension("EGL_KHR_no_config_context");

    outExtensions->surfacelessContext = mEGL->hasExtension("EGL_KHR_surfaceless_context");

    outExtensions->framebufferTargetANDROID = mEGL->hasExtension("EGL_ANDROID_framebuffer_target");

    outExtensions->imageDmaBufImportEXT = mEGL->hasExtension("EGL_EXT_image_dma_buf_import");

    outExtensions->imageDmaBufImportModifiersEXT =
        mEGL->hasExtension("EGL_EXT_image_dma_buf_import_modifiers");

    DisplayGL::generateExtensions(outExtensions);
}

void DisplayEGL::generateCaps(egl::Caps *outCaps) const
{
    outCaps->textureNPOT = true;  // Since we request GLES >= 2
}

void DisplayEGL::setBlobCacheFuncs(EGLSetBlobFuncANDROID set, EGLGetBlobFuncANDROID get)
{
    if (mEGL->hasExtension("EGL_ANDROID_blob_cache"))
    {
        mEGL->setBlobCacheFuncsANDROID(set, get);
    }
}

egl::Error DisplayEGL::makeCurrentSurfaceless(gl::Context *context)
{
    // Nothing to do because EGL always uses the same context and the previous surface can be left
    // current.
    return egl::NoError();
}

egl::Error DisplayEGL::createRenderer(EGLContext shareContext,
                                      std::shared_ptr<RendererEGL> *outRenderer)
{
    EGLContext context = EGL_NO_CONTEXT;
    native_egl::AttributeVector attribs;
    ANGLE_TRY(initializeContext(shareContext, mDisplayAttributes, &context, &attribs));

    if (mEGL->makeCurrent(EGL_NO_SURFACE, context) == EGL_FALSE)
    {
        return egl::EglNotInitialized()
               << "eglMakeCurrent failed with " << egl::Error(mEGL->getError());
    }

    CurrentNativeContext &currentContext = mCurrentNativeContexts[std::this_thread::get_id()];
    currentContext.surface               = EGL_NO_SURFACE;
    currentContext.context               = context;

    std::unique_ptr<FunctionsGL> functionsGL(mEGL->makeFunctionsGL());
    functionsGL->initialize(mDisplayAttributes);

    outRenderer->reset(
        new RendererEGL(std::move(functionsGL), mDisplayAttributes, this, context, attribs));

    return egl::NoError();
}

WorkerContext *DisplayEGL::createWorkerContext(std::string *infoLog,
                                               EGLContext sharedContext,
                                               const native_egl::AttributeVector workerAttribs)
{
    EGLContext context = mEGL->createContext(mConfig, sharedContext, workerAttribs.data());
    if (context == EGL_NO_CONTEXT)
    {
        *infoLog += "Unable to create the EGL context.";
        return nullptr;
    }
    return new WorkerContextEGL(context, mEGL, EGL_NO_SURFACE);
}

void DisplayEGL::initializeFrontendFeatures(angle::FrontendFeatures *features) const
{
    mRenderer->initializeFrontendFeatures(features);
}

void DisplayEGL::populateFeatureList(angle::FeatureList *features)
{
    mRenderer->getFeatures().populateFeatureList(features);
}

egl::Error DisplayEGL::validateImageClientBuffer(const gl::Context *context,
                                                 EGLenum target,
                                                 EGLClientBuffer clientBuffer,
                                                 const egl::AttributeMap &attribs) const
{
    switch (target)
    {
        case EGL_LINUX_DMA_BUF_EXT:
            return egl::NoError();

        default:
            return DisplayGL::validateImageClientBuffer(context, target, clientBuffer, attribs);
    }
}

ExternalImageSiblingImpl *DisplayEGL::createExternalImageSibling(const gl::Context *context,
                                                                 EGLenum target,
                                                                 EGLClientBuffer buffer,
                                                                 const egl::AttributeMap &attribs)
{
    switch (target)
    {
        case EGL_LINUX_DMA_BUF_EXT:
            ASSERT(context == nullptr);
            ASSERT(buffer == nullptr);
            return new DmaBufImageSiblingEGL(attribs);

        default:
            return DisplayGL::createExternalImageSibling(context, target, buffer, attribs);
    }
}

}  // namespace rx
