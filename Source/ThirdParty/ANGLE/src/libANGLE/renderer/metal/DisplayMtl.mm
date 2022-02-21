//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// DisplayMtl.mm: Metal implementation of DisplayImpl

#include "libANGLE/renderer/metal/DisplayMtl.h"

#include "common/system_utils.h"
#include "gpu_info_util/SystemInfo.h"
#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
#include "libANGLE/Surface.h"
#include "libANGLE/renderer/driver_utils.h"
#include "libANGLE/renderer/glslang_wrapper_utils.h"
#include "libANGLE/renderer/metal/CompilerMtl.h"
#include "libANGLE/renderer/metal/ContextMtl.h"
#include "libANGLE/renderer/metal/DeviceMtl.h"
#include "libANGLE/renderer/metal/IOSurfaceSurfaceMtl.h"
#include "libANGLE/renderer/metal/ImageMtl.h"
#include "libANGLE/renderer/metal/SurfaceMtl.h"
#include "libANGLE/renderer/metal/SyncMtl.h"
#include "libANGLE/renderer/metal/mtl_common.h"
#include "libANGLE/renderer/metal/shaders/mtl_default_shaders_src_autogen.inc"
#include "libANGLE/trace.h"
#include "platform/Platform.h"

#ifdef ANGLE_METAL_XCODE_BUILDS_SHADERS
#    include "libANGLE/renderer/metal/mtl_default_shaders_compiled.inc"
#endif

#include "EGL/eglext.h"

#if defined(ANGLE_PLATFORM_MACOS) || defined(ANGLE_PLATFORM_MACCATALYST)
constexpr char kANGLEPreferredDeviceEnv[] = "ANGLE_PREFERRED_DEVICE";
#endif

namespace rx
{

static EGLint GetDepthSize(GLint internalformat)
{
    switch (internalformat)
    {
        case GL_STENCIL_INDEX8:
            return 0;
        case GL_DEPTH_COMPONENT16:
            return 16;
        case GL_DEPTH_COMPONENT24:
            return 24;
        case GL_DEPTH_COMPONENT32_OES:
            return 32;
        case GL_DEPTH_COMPONENT32F:
            return 32;
        case GL_DEPTH24_STENCIL8:
            return 24;
        case GL_DEPTH32F_STENCIL8:
            return 32;
        default:
            //    UNREACHABLE(internalformat);
            return 0;
    }
}

static EGLint GetStencilSize(GLint internalformat)
{
    switch (internalformat)
    {
        case GL_STENCIL_INDEX8:
            return 8;
        case GL_DEPTH_COMPONENT16:
            return 0;
        case GL_DEPTH_COMPONENT24:
            return 0;
        case GL_DEPTH_COMPONENT32_OES:
            return 0;
        case GL_DEPTH_COMPONENT32F:
            return 0;
        case GL_DEPTH24_STENCIL8:
            return 8;
        case GL_DEPTH32F_STENCIL8:
            return 8;
        default:
            //    UNREACHABLE(internalformat);
            return 0;
    }
}

bool IsMetalDisplayAvailable()
{
    // We only support macos 10.13+ and 11 for now. Since they are requirements for Metal 2.0.
#if TARGET_OS_SIMULATOR
    if (ANGLE_APPLE_AVAILABLE_XCI(10.13, 13.0, 13))
#else
    if (ANGLE_APPLE_AVAILABLE_XCI(10.13, 13.0, 11))
#endif
    {
        return true;
    }
    return false;
}

DisplayImpl *CreateMetalDisplay(const egl::DisplayState &state)
{
    return new DisplayMtl(state);
}

struct DefaultShaderAsyncInfoMtl
{
    mtl::AutoObjCPtr<id<MTLLibrary>> defaultShaders;
    mtl::AutoObjCPtr<NSError *> defaultShadersCompileError;

    // Synchronization primitives for compiling default shaders in back-ground
    std::condition_variable cv;
    std::mutex lock;

    bool compiled = false;
};

DisplayMtl::DisplayMtl(const egl::DisplayState &state)
    : DisplayImpl(state), mStateCache(mFeatures), mUtils(this)
{}

DisplayMtl::~DisplayMtl() {}

egl::Error DisplayMtl::initialize(egl::Display *display)
{
    ASSERT(IsMetalDisplayAvailable());

    angle::Result result = initializeImpl(display);
    if (result != angle::Result::Continue)
    {
        return egl::EglNotInitialized();
    }
    return egl::NoError();
}

angle::Result DisplayMtl::initializeImpl(egl::Display *display)
{
    ANGLE_MTL_OBJC_SCOPE
    {
        mMetalDevice = getMetalDeviceMatchingAttribute(display->getAttributeMap());
        // If we can't create a device, fail initialization.
        if (!mMetalDevice.get())
        {
            return angle::Result::Stop;
        }

        mMetalDeviceVendorId = mtl::GetDeviceVendorId(mMetalDevice);

        mCmdQueue.set([[mMetalDevice newCommandQueue] ANGLE_MTL_AUTORELEASE]);

        mCapsInitialized = false;
#if ANGLE_ENABLE_METAL_SPIRV
        ANGLE_TRACE_EVENT0("gpu.angle,startup", "GlslangWarmup");
        sh::InitializeGlslang();
#endif

        if (!mState.featuresAllDisabled)
        {
            initializeFeatures();
        }

        ANGLE_TRY(mFormatTable.initialize(this));
        ANGLE_TRY(initializeShaderLibrary());

        return mUtils.initialize();
    }
}

void DisplayMtl::terminate()
{
    mUtils.onDestroy();
    mCmdQueue.reset();
    mDefaultShadersAsyncInfo = nullptr;
    mMetalDevice             = nil;
#if ANGLE_MTL_EVENT_AVAILABLE
    mSharedEventListener = nil;
#endif
    mCapsInitialized = false;

    mMetalDeviceVendorId = 0;
#if ANGLE_ENABLE_METAL_SPIRV
    sh::FinalizeGlslang();
#endif
}

bool DisplayMtl::testDeviceLost()
{
    return false;
}

egl::Error DisplayMtl::restoreLostDevice(const egl::Display *display)
{
    return egl::NoError();
}

std::string DisplayMtl::getRendererDescription()
{
    ANGLE_MTL_OBJC_SCOPE
    {
        std::string desc = "ANGLE Metal Renderer";

        if (mMetalDevice)
        {
            desc += ": ";
            desc += mMetalDevice.get().name.UTF8String;
        }

        return desc;
    }
}

std::string DisplayMtl::getVendorString()
{
    return GetVendorString(mMetalDeviceVendorId);
}

std::string DisplayMtl::getVersionString()
{
    ANGLE_MTL_OBJC_SCOPE
    {
        NSProcessInfo *procInfo = [NSProcessInfo processInfo];
        return procInfo.operatingSystemVersionString.UTF8String;
    }
}

DeviceImpl *DisplayMtl::createDevice()
{
    return new DeviceMtl();
}

mtl::AutoObjCPtr<id<MTLDevice>> DisplayMtl::getMetalDeviceMatchingAttribute(
    const egl::AttributeMap &attribs)
{
#if defined(ANGLE_PLATFORM_MACOS) || defined(ANGLE_PLATFORM_MACCATALYST)
    auto deviceList = mtl::adoptObjCObj(MTLCopyAllDevices());

    auto externalGPUs = mtl::adoptObjCObj<NSMutableArray<id<MTLDevice>>>([[NSMutableArray alloc] init]);
    auto integratedGPUs = mtl::adoptObjCObj<NSMutableArray<id<MTLDevice>>>([[NSMutableArray alloc] init]);
    auto discreteGPUs = mtl::adoptObjCObj<NSMutableArray<id<MTLDevice>>>([[NSMutableArray alloc] init]);
    for (id<MTLDevice> device in deviceList.get())
    {
        if (device.removable)
        {
            [externalGPUs addObject:device];
        }
        else if (device.lowPower)
        {
            [integratedGPUs addObject:device];
        }
        else
        {
            [discreteGPUs addObject:device];
        }
    }
    // TODO(kpiddington: External GPU support. Do we prefer high power / low bandwidth for general
    // WebGL applications?
    //      Can we support hot-swapping in GPU's?
    if (attribs.get(EGL_POWER_PREFERENCE_ANGLE, 0) == EGL_HIGH_POWER_ANGLE)
    {
        // Search for a discrete GPU first.
        for (id<MTLDevice> device in discreteGPUs.get())
        {
            if (![device isHeadless])
                return device;
        }
    }
    else if (attribs.get(EGL_POWER_PREFERENCE_ANGLE, 0) == EGL_LOW_POWER_ANGLE)
    {
        // If we've selected a low power device, look through integrated devices.
        for (id<MTLDevice> device in integratedGPUs.get())
        {
            if (![device isHeadless])
                return device;
        }
    }

    // Check the ANGLE_PREFERRED_DEVICE environment variable for device preference
    const std::string anglePreferredDevice = angle::GetEnvironmentVar(kANGLEPreferredDeviceEnv);
    if (anglePreferredDevice != "")
    {
        for (id<MTLDevice> device in deviceList.get())
        {
            if ([device.name.lowercaseString
                    containsString:[NSString stringWithUTF8String:anglePreferredDevice.c_str()]
                                       .lowercaseString])
            {
                NSLog(@"Using Metal Device: %@", [device name]);
                return device;
            }
        }
    }

    // Default to use a low power device, look through integrated devices.
    for (id<MTLDevice> device in integratedGPUs.get())
    {
        if (![device isHeadless])
            return device;
    }

    // If we selected a low power device and there's no low-power devices avaialble, return the
    // first (default) device.
    if (deviceList.get().count > 0)
        return deviceList[0];
#endif
    // If we can't find anything, or are on a platform that doesn't support power options, create a
    // default device.
    return mtl::adoptObjCObj(MTLCreateSystemDefaultDevice());
}

egl::Error DisplayMtl::waitClient(const gl::Context *context)
{
    auto contextMtl      = GetImplAs<ContextMtl>(context);
    angle::Result result = contextMtl->finishCommandBuffer();

    if (result != angle::Result::Continue)
    {
        return egl::EglBadAccess();
    }
    return egl::NoError();
}

egl::Error DisplayMtl::waitNative(const gl::Context *context, EGLint engine)
{
    UNIMPLEMENTED();
    return egl::NoError();
}

SurfaceImpl *DisplayMtl::createWindowSurface(const egl::SurfaceState &state,
                                             EGLNativeWindowType window,
                                             const egl::AttributeMap &attribs)
{
    return new WindowSurfaceMtl(this, state, window, attribs);
}

SurfaceImpl *DisplayMtl::createPbufferSurface(const egl::SurfaceState &state,
                                              const egl::AttributeMap &attribs)
{
    return new PBufferSurfaceMtl(this, state, attribs);
}

SurfaceImpl *DisplayMtl::createPbufferFromClientBuffer(const egl::SurfaceState &state,
                                                       EGLenum buftype,
                                                       EGLClientBuffer clientBuffer,
                                                       const egl::AttributeMap &attribs)
{
    switch (buftype)
    {
        case EGL_IOSURFACE_ANGLE:
            return new IOSurfaceSurfaceMtl(this, state, clientBuffer, attribs);
        default:
            UNREACHABLE();
    }
    return nullptr;
}

SurfaceImpl *DisplayMtl::createPixmapSurface(const egl::SurfaceState &state,
                                             NativePixmapType nativePixmap,
                                             const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return static_cast<SurfaceImpl *>(0);
}

ImageImpl *DisplayMtl::createImage(const egl::ImageState &state,
                                   const gl::Context *context,
                                   EGLenum target,
                                   const egl::AttributeMap &attribs)
{
    return new ImageMtl(state, context);
}

rx::ContextImpl *DisplayMtl::createContext(const gl::State &state,
                                           gl::ErrorSet *errorSet,
                                           const egl::Config *configuration,
                                           const gl::Context *shareContext,
                                           const egl::AttributeMap &attribs)
{
    return new ContextMtl(state, errorSet, attribs, this);
}

StreamProducerImpl *DisplayMtl::createStreamProducerD3DTexture(
    egl::Stream::ConsumerType consumerType,
    const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return nullptr;
}

ShareGroupImpl *DisplayMtl::createShareGroup()
{
    return new ShareGroupMtl();
}

ExternalImageSiblingImpl *DisplayMtl::createExternalImageSibling(const gl::Context *context,
                                                                 EGLenum target,
                                                                 EGLClientBuffer buffer,
                                                                 const egl::AttributeMap &attribs)
{
    switch (target)
    {
        case EGL_METAL_TEXTURE_ANGLE:
            return new TextureImageSiblingMtl(buffer);

        default:
            UNREACHABLE();
            return nullptr;
    }
}

gl::Version DisplayMtl::getMaxSupportedESVersion() const
{
#if TARGET_OS_SIMULATOR
    // Simulator should be able to support ES3, despite not supporting iOS GPU
    // Family 3 in its entirety.
    // FIXME: None of the feature conditions are checked for simulator support.
    return gl::Version(3, 0);
#else
    if (supportsEitherGPUFamily(3, 1))
    {
        return mtl::kMaxSupportedGLVersion;
    }
    return gl::Version(2, 0);
#endif
}

gl::Version DisplayMtl::getMaxConformantESVersion() const
{
    return std::min(getMaxSupportedESVersion(), gl::Version(3, 0));
}

EGLSyncImpl *DisplayMtl::createSync(const egl::AttributeMap &attribs)
{
    return new EGLSyncMtl(attribs);
}

egl::Error DisplayMtl::makeCurrent(egl::Display *display,
                                   egl::Surface *drawSurface,
                                   egl::Surface *readSurface,
                                   gl::Context *context)
{
    if (!context)
    {
        return egl::NoError();
    }

    return egl::NoError();
}

void DisplayMtl::generateExtensions(egl::DisplayExtensions *outExtensions) const
{
    outExtensions->iosurfaceClientBuffer      = true;
    outExtensions->surfacelessContext         = true;
    outExtensions->noConfigContext            = true;
    outExtensions->displayTextureShareGroup   = true;
    outExtensions->displaySemaphoreShareGroup = true;
    outExtensions->mtlTextureClientBuffer     = true;

    if (mFeatures.hasEvents.enabled)
    {
        // MTLSharedEvent is only available since Metal 2.1
        outExtensions->fenceSync = true;
        outExtensions->waitSync  = true;
    }

    // Note that robust resource initialization is not yet implemented. We only expose
    // this extension so that ANGLE can be initialized in Chrome. WebGL will fail to use
    // this extension (anglebug.com/4929)
    outExtensions->robustResourceInitializationANGLE = true;

    // EGL_KHR_image
    outExtensions->image     = true;
    outExtensions->imageBase = true;

    // EGL_ANGLE_metal_create_context_ownership_identity
    outExtensions->metalCreateContextOwnershipIdentityANGLE = true;
}

void DisplayMtl::generateCaps(egl::Caps *outCaps) const {}

void DisplayMtl::populateFeatureList(angle::FeatureList *features)
{
    mFeatures.populateFeatureList(features);
}

EGLenum DisplayMtl::EGLDrawingBufferTextureTarget()
{
    // TODO(anglebug.com/6395): Apple's implementation conditionalized this on
    // MacCatalyst and whether it was running on ARM64 or X64, preferring
    // EGL_TEXTURE_RECTANGLE_ANGLE. Metal can bind IOSurfaces to regular 2D
    // textures, and rectangular textures don't work in the SPIR-V Metal
    // backend, so for the time being use EGL_TEXTURE_2D on all platforms.
    return EGL_TEXTURE_2D;
}

egl::ConfigSet DisplayMtl::generateConfigs()
{
    // NOTE(hqle): generate more config permutations
    egl::ConfigSet configs;

    const gl::Version &maxVersion = getMaxSupportedESVersion();
    ASSERT(maxVersion >= gl::Version(2, 0));
    bool supportsES3 = maxVersion >= gl::Version(3, 0);

    egl::Config config;

    // Native stuff
    config.nativeVisualID   = 0;
    config.nativeVisualType = 0;
    config.nativeRenderable = EGL_TRUE;

    config.colorBufferType = EGL_RGB_BUFFER;
    config.luminanceSize   = 0;
    config.alphaMaskSize   = 0;

    config.transparentType = EGL_NONE;

    // Pbuffer
    config.bindToTextureTarget = EGLDrawingBufferTextureTarget();
    config.maxPBufferWidth     = 4096;
    config.maxPBufferHeight    = 4096;
    config.maxPBufferPixels    = 4096 * 4096;

    // Caveat
    config.configCaveat = EGL_NONE;

    // Misc
    config.sampleBuffers     = 0;
    config.samples           = 0;
    config.level             = 0;
    config.bindToTextureRGB  = EGL_FALSE;
    config.bindToTextureRGBA = EGL_TRUE;

    config.surfaceType = EGL_WINDOW_BIT | EGL_PBUFFER_BIT;

#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
    config.minSwapInterval = 0;
    config.maxSwapInterval = 1;
#else
    config.minSwapInterval = 1;
    config.maxSwapInterval = 1;
#endif

    config.renderTargetFormat = GL_RGBA8;
    config.depthStencilFormat = GL_DEPTH24_STENCIL8;

    config.conformant     = EGL_OPENGL_ES2_BIT | (supportsES3 ? EGL_OPENGL_ES3_BIT_KHR : 0);
    config.renderableType = config.conformant;

    config.matchNativePixmap = EGL_NONE;

    config.colorComponentType = EGL_COLOR_COMPONENT_TYPE_FIXED_EXT;

    constexpr int samplesSupported[] = {0, 4};

    for (int samples : samplesSupported)
    {
        config.samples       = samples;
        config.sampleBuffers = (samples == 0) ? 0 : 1;

        // Buffer sizes
        config.redSize    = 8;
        config.greenSize  = 8;
        config.blueSize   = 8;
        config.alphaSize  = 8;
        config.bufferSize = config.redSize + config.greenSize + config.blueSize + config.alphaSize;

        // With DS
        config.depthSize   = 24;
        config.stencilSize = 8;

        configs.add(config);

        // With D
        config.depthSize   = 24;
        config.stencilSize = 0;
        configs.add(config);

        // With S
        config.depthSize   = 0;
        config.stencilSize = 8;
        configs.add(config);

        // Tests like dEQP-GLES2.functional.depth_range.* assume EGL_DEPTH_SIZE is properly set even
        // if renderConfig attributes are set to glu::RenderConfig::DONT_CARE
        config.depthSize   = GetDepthSize(config.depthStencilFormat);
        config.stencilSize = GetStencilSize(config.depthStencilFormat);
        configs.add(config);
    }

    return configs;
}

bool DisplayMtl::isValidNativeWindow(EGLNativeWindowType window) const
{
    ANGLE_MTL_OBJC_SCOPE
    {
        NSObject *layer = (__bridge NSObject *)(window);
        return [layer isKindOfClass:[CALayer class]];
    }
}

egl::Error DisplayMtl::validateClientBuffer(const egl::Config *configuration,
                                            EGLenum buftype,
                                            EGLClientBuffer clientBuffer,
                                            const egl::AttributeMap &attribs) const
{
    switch (buftype)
    {
        case EGL_IOSURFACE_ANGLE:
            if (!IOSurfaceSurfaceMtl::ValidateAttributes(clientBuffer, attribs))
            {
                return egl::EglBadAttribute();
            }
            break;
        default:
            UNREACHABLE();
            return egl::EglBadAttribute();
    }
    return egl::NoError();
}

egl::Error DisplayMtl::validateImageClientBuffer(const gl::Context *context,
                                                 EGLenum target,
                                                 EGLClientBuffer clientBuffer,
                                                 const egl::AttributeMap &attribs) const
{
    switch (target)
    {
        case EGL_METAL_TEXTURE_ANGLE:
            if (!TextureImageSiblingMtl::ValidateClientBuffer(this, clientBuffer))
            {
                return egl::EglBadAttribute();
            }
            break;
        default:
            UNREACHABLE();
            return egl::EglBadAttribute();
    }
    return egl::NoError();
}

gl::Caps DisplayMtl::getNativeCaps() const
{
    ensureCapsInitialized();
    return mNativeCaps;
}
const gl::TextureCapsMap &DisplayMtl::getNativeTextureCaps() const
{
    ensureCapsInitialized();
    return mNativeTextureCaps;
}
const gl::Extensions &DisplayMtl::getNativeExtensions() const
{
    ensureCapsInitialized();
    return mNativeExtensions;
}

const gl::Limitations &DisplayMtl::getNativeLimitations() const
{
    ensureCapsInitialized();
    return mNativeLimitations;
}

void DisplayMtl::ensureCapsInitialized() const
{
    if (mCapsInitialized)
    {
        return;
    }

    mCapsInitialized = true;

    // Reset
    mNativeCaps = gl::Caps();

    // Fill extension and texture caps
    initializeExtensions();
    initializeTextureCaps();

    // https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf
    mNativeCaps.maxElementIndex  = std::numeric_limits<GLuint>::max() - 1;
    mNativeCaps.max3DTextureSize = 2048;
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
    mNativeCaps.max2DTextureSize = 16384;
    // On macOS exclude [[position]] from maxVaryingVectors.
    mNativeCaps.maxVaryingVectors         = 31 - 1;
    mNativeCaps.maxVertexOutputComponents = mNativeCaps.maxFragmentInputComponents = 124 - 4;
#else
    if (supportsAppleGPUFamily(3))
    {
        mNativeCaps.max2DTextureSize          = 16384;
        mNativeCaps.maxVertexOutputComponents = mNativeCaps.maxFragmentInputComponents = 124;
        mNativeCaps.maxVaryingVectors = mNativeCaps.maxVertexOutputComponents / 4;
    }
    else
    {
        mNativeCaps.max2DTextureSize          = 8192;
        mNativeCaps.maxVertexOutputComponents = mNativeCaps.maxFragmentInputComponents = 60;
        mNativeCaps.maxVaryingVectors = mNativeCaps.maxVertexOutputComponents / 4;
    }
#endif

    mNativeCaps.maxArrayTextureLayers = 2048;
    mNativeCaps.maxLODBias            = 2.0;  // default GLES3 limit
    mNativeCaps.maxCubeMapTextureSize = mNativeCaps.max2DTextureSize;
    mNativeCaps.maxRenderbufferSize   = mNativeCaps.max2DTextureSize;
    mNativeCaps.minAliasedPointSize   = 1;
    // NOTE(hqle): Metal has some problems drawing big point size even though
    // Metal-Feature-Set-Tables.pdf says that max supported point size is 511. We limit it to 64
    // for now. http://anglebug.com/4816

    // NOTE(kpiddington): This seems to be fixed in macOS Monterey
    if (ANGLE_APPLE_AVAILABLE_XCI(12.0, 15.0, 15.0))
    {
        mNativeCaps.maxAliasedPointSize = 511;
    }
    else
    {
        mNativeCaps.maxAliasedPointSize = 64;
    }
    mNativeCaps.minAliasedLineWidth = 1.0f;
    mNativeCaps.maxAliasedLineWidth = 1.0f;

    mNativeCaps.maxDrawBuffers       = mtl::kMaxRenderTargets;
    mNativeCaps.maxFramebufferWidth  = mNativeCaps.max2DTextureSize;
    mNativeCaps.maxFramebufferHeight = mNativeCaps.max2DTextureSize;
    mNativeCaps.maxColorAttachments  = mtl::kMaxRenderTargets;
    mNativeCaps.maxViewportWidth     = mNativeCaps.max2DTextureSize;
    mNativeCaps.maxViewportHeight    = mNativeCaps.max2DTextureSize;

    // MSAA
    mNativeCaps.maxSamples             = mFormatTable.getMaxSamples();
    mNativeCaps.maxSampleMaskWords     = 0;
    mNativeCaps.maxColorTextureSamples = mNativeCaps.maxSamples;
    mNativeCaps.maxDepthTextureSamples = mNativeCaps.maxSamples;
    mNativeCaps.maxIntegerSamples      = 1;

    mNativeCaps.maxVertexAttributes           = mtl::kMaxVertexAttribs;
    mNativeCaps.maxVertexAttribBindings       = mtl::kMaxVertexAttribs;
    mNativeCaps.maxVertexAttribRelativeOffset = std::numeric_limits<GLint>::max();
    mNativeCaps.maxVertexAttribStride         = std::numeric_limits<GLint>::max();

    // glGet() use signed integer as parameter so we have to use GLint's max here, not GLuint.
    mNativeCaps.maxElementsIndices  = std::numeric_limits<GLint>::max();
    mNativeCaps.maxElementsVertices = std::numeric_limits<GLint>::max();

    // Looks like all floats are IEEE according to the docs here:
    mNativeCaps.vertexHighpFloat.setIEEEFloat();
    mNativeCaps.vertexMediumpFloat.setIEEEFloat();
    mNativeCaps.vertexLowpFloat.setIEEEFloat();
    mNativeCaps.fragmentHighpFloat.setIEEEFloat();
    mNativeCaps.fragmentMediumpFloat.setIEEEFloat();
    mNativeCaps.fragmentLowpFloat.setIEEEFloat();

    mNativeCaps.vertexHighpInt.setTwosComplementInt(32);
    mNativeCaps.vertexMediumpInt.setTwosComplementInt(32);
    mNativeCaps.vertexLowpInt.setTwosComplementInt(32);
    mNativeCaps.fragmentHighpInt.setTwosComplementInt(32);
    mNativeCaps.fragmentMediumpInt.setTwosComplementInt(32);
    mNativeCaps.fragmentLowpInt.setTwosComplementInt(32);

    GLuint maxDefaultUniformVectors = mtl::kDefaultUniformsMaxSize / (sizeof(GLfloat) * 4);

    const GLuint maxDefaultUniformComponents = maxDefaultUniformVectors * 4;

    // Uniforms are implemented using a uniform buffer, so the max number of uniforms we can
    // support is the max buffer range divided by the size of a single uniform (4X float).
    mNativeCaps.maxVertexUniformVectors                              = maxDefaultUniformVectors;
    mNativeCaps.maxShaderUniformComponents[gl::ShaderType::Vertex]   = maxDefaultUniformComponents;
    mNativeCaps.maxFragmentUniformVectors                            = maxDefaultUniformVectors;
    mNativeCaps.maxShaderUniformComponents[gl::ShaderType::Fragment] = maxDefaultUniformComponents;

    mNativeCaps.maxShaderUniformBlocks[gl::ShaderType::Vertex]   = mtl::kMaxShaderUBOs;
    mNativeCaps.maxShaderUniformBlocks[gl::ShaderType::Fragment] = mtl::kMaxShaderUBOs;
    mNativeCaps.maxCombinedUniformBlocks                         = mtl::kMaxGLUBOBindings;

    // Note that we currently implement textures as combined image+samplers, so the limit is
    // the minimum of supported samplers and sampled images.
    mNativeCaps.maxCombinedTextureImageUnits                         = mtl::kMaxGLSamplerBindings;
    mNativeCaps.maxShaderTextureImageUnits[gl::ShaderType::Fragment] = mtl::kMaxShaderSamplers;
    mNativeCaps.maxShaderTextureImageUnits[gl::ShaderType::Vertex]   = mtl::kMaxShaderSamplers;

    // No info from Metal given, use default GLES3 spec values:
    mNativeCaps.minProgramTexelOffset = -8;
    mNativeCaps.maxProgramTexelOffset = 7;

    // NOTE(hqle): support storage buffer.
    const uint32_t maxPerStageStorageBuffers                     = 0;
    mNativeCaps.maxShaderStorageBlocks[gl::ShaderType::Vertex]   = maxPerStageStorageBuffers;
    mNativeCaps.maxShaderStorageBlocks[gl::ShaderType::Fragment] = maxPerStageStorageBuffers;
    mNativeCaps.maxCombinedShaderStorageBlocks                   = maxPerStageStorageBuffers;

    // Fill in additional limits for UBOs and SSBOs.
    mNativeCaps.maxUniformBufferBindings = mNativeCaps.maxCombinedUniformBlocks;
    mNativeCaps.maxUniformBlockSize      = mtl::kMaxUBOSize;  // Default according to GLES 3.0 spec.
    if (supportsAppleGPUFamily(1))
    {
        mNativeCaps.uniformBufferOffsetAlignment =
            16;  // on Apple based GPU's We can ignore data types when setting constant buffer
                 // alignment at 16.
    }
    else
    {
        mNativeCaps.uniformBufferOffsetAlignment =
            256;  // constant buffers on all other GPUs must be aligned to 256.
    }

    mNativeCaps.maxShaderStorageBufferBindings     = 0;
    mNativeCaps.maxShaderStorageBlockSize          = 0;
    mNativeCaps.shaderStorageBufferOffsetAlignment = 0;

    // UBO plus default uniform limits
    const uint32_t maxCombinedUniformComponents =
        maxDefaultUniformComponents + mtl::kMaxUBOSize * mtl::kMaxShaderUBOs / 4;
    for (gl::ShaderType shaderType : gl::kAllGraphicsShaderTypes)
    {
        mNativeCaps.maxCombinedShaderUniformComponents[shaderType] = maxCombinedUniformComponents;
    }

    mNativeCaps.maxCombinedShaderOutputResources = 0;

    mNativeCaps.maxTransformFeedbackInterleavedComponents =
        gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS;
    mNativeCaps.maxTransformFeedbackSeparateAttributes =
        gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS;
    mNativeCaps.maxTransformFeedbackSeparateComponents =
        gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS;

    // GL_OES_get_program_binary
    mNativeCaps.programBinaryFormats.push_back(GL_PROGRAM_BINARY_ANGLE);

    // GL_APPLE_clip_distance
    mNativeCaps.maxClipDistances = mFeatures.directMetalGeneration.enabled ? 0 : 8;

    // Metal doesn't support GL_TEXTURE_COMPARE_MODE=GL_NONE for shadow samplers
    mNativeLimitations.noShadowSamplerCompareModeNone = true;

    // Apple platforms require PVRTC1 textures to be squares.
    mNativeLimitations.squarePvrtc1 = true;

    // Older Metal does not support compressed formats for TEXTURE_3D target.
    if (ANGLE_APPLE_AVAILABLE_XCI(10.15, 13.0, 13.0))
    {
        mNativeLimitations.noCompressedTexture3D = !supportsEitherGPUFamily(3, 1);
    }
    else
    {
        mNativeLimitations.noCompressedTexture3D = true;
    }

    // Direct-to-metal constants:
    mNativeCaps.driverUniformsBindingIndex    = mtl::kDriverUniformsBindingIndex;
    mNativeCaps.defaultUniformsBindingIndex   = mtl::kDefaultUniformsBindingIndex;
    mNativeCaps.UBOArgumentBufferBindingIndex = mtl::kUBOArgumentBufferBindingIndex;
}

void DisplayMtl::initializeExtensions() const
{
    // Reset
    mNativeExtensions = gl::Extensions();

    // Enable this for simple buffer readback testing, but some functionality is missing.
    // NOTE(hqle): Support full mapBufferRangeEXT extension.
    mNativeExtensions.mapbufferOES                  = true;
    mNativeExtensions.mapBufferRangeEXT             = true;
    mNativeExtensions.textureStorageEXT             = true;
    mNativeExtensions.drawBuffersEXT                = true;
    mNativeExtensions.drawBuffersIndexedEXT         = true;
    mNativeExtensions.drawBuffersIndexedOES         = true;
    mNativeExtensions.fboRenderMipmapOES            = true;
    mNativeExtensions.fragDepthEXT                  = true;
    mNativeExtensions.framebufferBlitANGLE          = true;
    mNativeExtensions.framebufferBlitNV             = true;
    mNativeExtensions.framebufferMultisampleANGLE   = true;
    mNativeExtensions.copyTextureCHROMIUM           = true;
    mNativeExtensions.copyCompressedTextureCHROMIUM = false;

    // EXT_debug_marker is not implemented yet, but the entry points must be exposed for the
    // Metal backend to be used in Chrome (http://anglebug.com/4946)
    mNativeExtensions.debugMarkerEXT = true;

    mNativeExtensions.robustnessEXT               = true;
    mNativeExtensions.textureBorderClampOES       = false;  // not implemented yet
    mNativeExtensions.multiDrawIndirectEXT        = true;
    mNativeExtensions.translatedShaderSourceANGLE = true;
    mNativeExtensions.discardFramebufferEXT       = true;
    // TODO(anglebug.com/6395): Apple's implementation exposed
    // mNativeExtensions.textureRectangle = true here and
    // EGL_TEXTURE_RECTANGLE_ANGLE as the eglBindTexImage texture target on
    // macOS. This no longer seems necessary as IOSurfaces can be bound to
    // regular 2D textures with Metal, and causes other problems such as
    // breaking the SPIR-V Metal compiler.

    // TODO(anglebug.com/6395): figure out why WebGL drawing buffer
    // creation fails on macOS when the Metal backend advertises the
    // EXT_multisampled_render_to_texture extension.
#if !defined(ANGLE_PLATFORM_MACOS)
    // EXT_multisampled_render_to_texture
    if (mFeatures.allowMultisampleStoreAndResolve.enabled &&
        mFeatures.hasDepthAutoResolve.enabled && mFeatures.hasStencilAutoResolve.enabled)
    {
        mNativeExtensions.multisampledRenderToTextureEXT = true;
    }
#endif

    // Enable EXT_blend_minmax
    mNativeExtensions.blendMinmaxEXT = true;

    mNativeExtensions.EGLImageOES         = true;
    mNativeExtensions.EGLImageExternalOES = false;
    // NOTE(hqle): Support GL_OES_EGL_image_external_essl3.
    mNativeExtensions.EGLImageExternalEssl3OES = false;

    mNativeExtensions.memoryObjectEXT   = false;
    mNativeExtensions.memoryObjectFdEXT = false;

    mNativeExtensions.semaphoreEXT   = false;
    mNativeExtensions.semaphoreFdEXT = false;

    mNativeExtensions.instancedArraysANGLE = true;
    mNativeExtensions.instancedArraysEXT   = mNativeExtensions.instancedArraysANGLE;

    mNativeExtensions.robustBufferAccessBehaviorKHR = false;

    mNativeExtensions.EGLSyncOES = false;

    mNativeExtensions.occlusionQueryBooleanEXT = true;

    mNativeExtensions.disjointTimerQueryEXT = false;
    mNativeCaps.queryCounterBitsTimeElapsed = 0;
    mNativeCaps.queryCounterBitsTimestamp   = 0;

    mNativeExtensions.textureFilterAnisotropicEXT = true;
    mNativeCaps.maxTextureAnisotropy              = 16;

    mNativeExtensions.textureNpotOES = true;

    mNativeExtensions.texture3DOES = true;

    mNativeExtensions.standardDerivativesOES = true;

    mNativeExtensions.elementIndexUintOES = true;

    // GL_OES_get_program_binary
    mNativeExtensions.getProgramBinaryOES = true;

    // GL_APPLE_clip_distance
    mNativeExtensions.clipDistanceAPPLE = !mFeatures.directMetalGeneration.enabled;

    // GL_NV_pixel_buffer_object
    mNativeExtensions.pixelBufferObjectNV = true;

    if (mFeatures.hasEvents.enabled)
    {
        // MTLSharedEvent is only available since Metal 2.1

        // GL_NV_fence
        mNativeExtensions.fenceNV = true;

        // GL_OES_EGL_sync
        mNativeExtensions.EGLSyncOES = true;

        // GL_ARB_sync
        mNativeExtensions.syncARB = true;
    }
}

void DisplayMtl::initializeTextureCaps() const
{
    mNativeTextureCaps.clear();

    mFormatTable.generateTextureCaps(this, &mNativeTextureCaps,
                                     &mNativeCaps.compressedTextureFormats);

    // Re-verify texture extensions.
    mNativeExtensions.setTextureExtensionSupport(mNativeTextureCaps);

    // When ETC2/EAC formats are natively supported, enable ANGLE-specific extension string to
    // expose them to WebGL. In other case, mark potentially-available ETC1 extension as
    // emulated.
    if (supportsAppleGPUFamily(1) && gl::DetermineCompressedTextureETCSupport(mNativeTextureCaps))
    {
        mNativeExtensions.compressedTextureEtcANGLE = true;
    }
    else
    {
        mNativeLimitations.emulatedEtc1 = true;
    }

    // Enable ASTC sliced 3D, requires MTLGPUFamilyApple3
    if (supportsAppleGPUFamily(3) && mNativeExtensions.textureCompressionAstcLdrKHR)
    {
        mNativeExtensions.textureCompressionAstcSliced3dKHR = true;
    }

    // Enable ASTC HDR, requires MTLGPUFamilyApple6
    if (supportsAppleGPUFamily(6) && mNativeExtensions.textureCompressionAstcLdrKHR)
    {
        mNativeExtensions.textureCompressionAstcHdrKHR = true;
    }

    // Disable all depth buffer and stencil buffer readback extensions until we need them
    mNativeExtensions.readDepthNV         = false;
    mNativeExtensions.readStencilNV       = false;
    mNativeExtensions.depthBufferFloat2NV = false;
    mNativeExtensions.textureCompressionAstcLdrKHR &= supportsAppleGPUFamily(2);
}

void DisplayMtl::initializeLimitations()
{
    mNativeLimitations.noVertexAttributeAliasing = true;
}

void DisplayMtl::initializeFeatures()
{
    bool isMetal2_1 = false;
    bool isMetal2_2 = false;
    if (ANGLE_APPLE_AVAILABLE_XCI(10.14, 13.0, 12.0))
    {
        isMetal2_1 = true;
    }

    if (ANGLE_APPLE_AVAILABLE_XCI(10.15, 13.0, 13.0))
    {
        isMetal2_2 = true;
    }

    bool isOSX       = TARGET_OS_OSX;
    bool isCatalyst  = TARGET_OS_MACCATALYST;
    bool isSimulator = TARGET_OS_SIMULATOR;
    bool isARM       = ANGLE_MTL_ARM;

    ANGLE_FEATURE_CONDITION((&mFeatures), allowGenMultipleMipsPerPass, true);
    ANGLE_FEATURE_CONDITION((&mFeatures), forceBufferGPUStorage, false);
    ANGLE_FEATURE_CONDITION((&mFeatures), hasExplicitMemBarrier,
                            isMetal2_1 && (isOSX || isCatalyst) && !isARM);
    ANGLE_FEATURE_CONDITION((&mFeatures), hasDepthAutoResolve, supportsEitherGPUFamily(3, 2));
    ANGLE_FEATURE_CONDITION((&mFeatures), hasStencilAutoResolve, supportsEitherGPUFamily(5, 2));
    ANGLE_FEATURE_CONDITION((&mFeatures), allowMultisampleStoreAndResolve,
                            supportsEitherGPUFamily(3, 1));

    ANGLE_FEATURE_CONDITION((&mFeatures), allowRuntimeSamplerCompareMode,
                            supportsEitherGPUFamily(3, 1));
    // AMD does not support sample_compare_grad
    ANGLE_FEATURE_CONDITION((&mFeatures), allowSamplerCompareGradient,
                            supportsEitherGPUFamily(3, 1) && !isAMD());
    ANGLE_FEATURE_CONDITION((&mFeatures), allowSamplerCompareLod, supportsEitherGPUFamily(3, 1));

    // http://anglebug.com/4919
    // Stencil blit shader is not compiled on Intel & NVIDIA, need investigation.
    ANGLE_FEATURE_CONDITION((&mFeatures), hasStencilOutput,
                            isMetal2_1 && !isIntel() && !isNVIDIA());

    ANGLE_FEATURE_CONDITION((&mFeatures), hasTextureSwizzle,
                            isMetal2_2 && supportsEitherGPUFamily(3, 2) && !isSimulator);

    // http://crbug.com/1136673
    // Fence sync is flaky on Nvidia
    ANGLE_FEATURE_CONDITION((&mFeatures), hasEvents, isMetal2_1 && !isNVIDIA());

    ANGLE_FEATURE_CONDITION((&mFeatures), hasCheapRenderPass, (isOSX || isCatalyst) && !isARM);

    // http://anglebug.com/5235
    // D24S8 is unreliable on AMD.
    ANGLE_FEATURE_CONDITION((&mFeatures), forceD24S8AsUnsupported, isAMD());

    // Base Vertex drawing is only supported since GPU family 3.
    ANGLE_FEATURE_CONDITION((&mFeatures), hasBaseVertexInstancedDraw,
                            isOSX || isCatalyst || supportsAppleGPUFamily(3));

    ANGLE_FEATURE_CONDITION((&mFeatures), hasNonUniformDispatch,
                            isOSX || isCatalyst || supportsAppleGPUFamily(4));

    ANGLE_FEATURE_CONDITION((&mFeatures), allowSeparatedDepthStencilBuffers,
                            !isOSX && !isCatalyst && !isSimulator);
    ANGLE_FEATURE_CONDITION((&mFeatures), rewriteRowMajorMatrices, true);
    ANGLE_FEATURE_CONDITION((&mFeatures), emulateTransformFeedback, true);

    ANGLE_FEATURE_CONDITION((&mFeatures), intelExplicitBoolCastWorkaround,
                            isIntel() && GetMacOSVersion() < OSVersion(11, 0, 0));
    ANGLE_FEATURE_CONDITION((&mFeatures), intelDisableFastMath,
                            isIntel() && GetMacOSVersion() < OSVersion(12, 0, 0));

    ANGLE_FEATURE_CONDITION((&mFeatures), forceNonCSBaseMipmapGeneration, isIntel());

    bool defaultDirectToMetal = true;

    ANGLE_FEATURE_CONDITION((&mFeatures), directMetalGeneration, defaultDirectToMetal);

    angle::PlatformMethods *platform = ANGLEPlatformCurrent();
    platform->overrideFeaturesMtl(platform, &mFeatures);

    ApplyFeatureOverrides(&mFeatures, getState());
#ifdef ANGLE_ENABLE_ASSERTS
    fprintf(stderr, "Shader compiler output: %s\n",
            mFeatures.directMetalGeneration.enabled ? "Metal" : "SPIR-V");
#endif
}

angle::Result DisplayMtl::initializeShaderLibrary()
{
#ifdef ANGLE_METAL_XCODE_BUILDS_SHADERS
    mDefaultShadersAsyncInfo.reset(new DefaultShaderAsyncInfoMtl);

    const uint8_t *compiled_shader_binary;
    size_t compiled_shader_binary_len;
    compiled_shader_binary                           = gMetalBinaryShaders;
    compiled_shader_binary_len                       = gMetalBinaryShaders_len;
    mtl::AutoObjCPtr<NSError *> err                  = nil;
    mtl::AutoObjCPtr<id<MTLLibrary>> mDefaultShaders = mtl::CreateShaderLibraryFromBinary(
        getMetalDevice(), compiled_shader_binary, compiled_shader_binary_len, &err);
    mDefaultShadersAsyncInfo->defaultShaders             = std::move(mDefaultShaders.get());
    mDefaultShadersAsyncInfo->defaultShadersCompileError = std::move(err.get());
    mDefaultShadersAsyncInfo->compiled                   = true;

#else
    mDefaultShadersAsyncInfo.reset(new DefaultShaderAsyncInfoMtl);

    // Create references to async info struct since it might be released in terminate(), but the
    // callback might still not be fired yet.
    std::shared_ptr<DefaultShaderAsyncInfoMtl> asyncRef = mDefaultShadersAsyncInfo;

    // Compile the default shaders asynchronously
    ANGLE_MTL_OBJC_SCOPE
    {
        auto nsSource = [[NSString alloc] initWithBytesNoCopy:gDefaultMetallibSrc
                                                       length:sizeof(gDefaultMetallibSrc)
                                                     encoding:NSUTF8StringEncoding
                                                 freeWhenDone:NO];
        auto options  = [[[MTLCompileOptions alloc] init] ANGLE_MTL_AUTORELEASE];
        [getMetalDevice() newLibraryWithSource:nsSource
                                       options:options
                             completionHandler:^(id<MTLLibrary> library, NSError *error) {
                               std::unique_lock<std::mutex> lg(asyncRef->lock);

                               asyncRef->defaultShaders             = std::move(library);
                               asyncRef->defaultShadersCompileError = std::move(error);

                               asyncRef->compiled = true;
                               asyncRef->cv.notify_one();
                             }];

        [nsSource ANGLE_MTL_AUTORELEASE];
    }
#endif
    return angle::Result::Continue;
}

id<MTLLibrary> DisplayMtl::getDefaultShadersLib()
{
    std::unique_lock<std::mutex> lg(mDefaultShadersAsyncInfo->lock);
    if (!mDefaultShadersAsyncInfo->compiled)
    {
        // Wait for async compilation
        mDefaultShadersAsyncInfo->cv.wait(lg,
                                          [this] { return mDefaultShadersAsyncInfo->compiled; });
    }

    if (mDefaultShadersAsyncInfo->defaultShadersCompileError &&
        !mDefaultShadersAsyncInfo->defaultShaders)
    {
        ANGLE_MTL_OBJC_SCOPE
        {
            ERR() << "Internal error: "
                  << mDefaultShadersAsyncInfo->defaultShadersCompileError.get()
                         .localizedDescription.UTF8String;
        }
        // This is not supposed to happen
        UNREACHABLE();
    }

    return mDefaultShadersAsyncInfo->defaultShaders;
}

bool DisplayMtl::supportsAppleGPUFamily(uint8_t iOSFamily) const
{
    return mtl::SupportsAppleGPUFamily(getMetalDevice(), iOSFamily);
}

bool DisplayMtl::supportsMacGPUFamily(uint8_t macFamily) const
{
    return mtl::SupportsMacGPUFamily(getMetalDevice(), macFamily);
}

bool DisplayMtl::supportsEitherGPUFamily(uint8_t iOSFamily, uint8_t macFamily) const
{
    return supportsAppleGPUFamily(iOSFamily) || supportsMacGPUFamily(macFamily);
}

bool DisplayMtl::supports32BitFloatFiltering() const
{
#if (defined(__MAC_11_0) && __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_11_0) ||        \
    (defined(__IPHONE_14_0) && __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_14_0)
    if (@available(ios 14.0, macOS 11.0, *))
    {
        return [mMetalDevice supports32BitFloatFiltering];
    }
    else
#endif
    {
        return supportsMacGPUFamily(1);
    }
}

bool DisplayMtl::supportsDepth24Stencil8PixelFormat() const
{
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
    return [mMetalDevice isDepth24Stencil8PixelFormatSupported];
#else
    return false;
#endif
}
bool DisplayMtl::isAMD() const
{
    return angle::IsAMD(mMetalDeviceVendorId);
}

bool DisplayMtl::isIntel() const
{
    return angle::IsIntel(mMetalDeviceVendorId);
}

bool DisplayMtl::isNVIDIA() const
{
    return angle::IsNVIDIA(mMetalDeviceVendorId);
}

#if ANGLE_MTL_EVENT_AVAILABLE
mtl::AutoObjCObj<MTLSharedEventListener> DisplayMtl::getOrCreateSharedEventListener()
{
    if (!mSharedEventListener)
    {
        ANGLE_MTL_OBJC_SCOPE
        {
            mSharedEventListener = [[[MTLSharedEventListener alloc] init] ANGLE_MTL_AUTORELEASE];
            ASSERT(mSharedEventListener);  // Failure here most probably means a sandbox issue.
        }
    }
    return mSharedEventListener;
}
#endif

bool DisplayMtl::useDirectToMetalCompiler() const
{
    return mFeatures.directMetalGeneration.enabled;
}

}  // namespace rx
