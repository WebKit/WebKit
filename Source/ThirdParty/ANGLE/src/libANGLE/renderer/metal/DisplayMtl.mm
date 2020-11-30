//
// Copyright (c) 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// DisplayMtl.mm: Metal implementation of DisplayImpl

#include "libANGLE/renderer/metal/DisplayMtl.h"

#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
#include "libANGLE/Surface.h"
#include "libANGLE/renderer/glslang_wrapper_utils.h"
#include "libANGLE/renderer/metal/ContextMtl.h"
#include "libANGLE/renderer/metal/SurfaceMtl.h"
#include "libANGLE/renderer/metal/mtl_common.h"
#include "libANGLE/renderer/metal/shaders/compiled/mtl_default_shaders_autogen.inc"
#include "platform/Platform.h"

#include "EGL/eglext.h"

namespace rx
{

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

DisplayMtl::DisplayMtl(const egl::DisplayState &state)
    : DisplayImpl(state), mUtils(this), mGlslangInitialized(false)
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
        mMetalDevice = [MTLCreateSystemDefaultDevice() ANGLE_MTL_AUTORELEASE];
        if (!mMetalDevice)
        {
            return angle::Result::Stop;
        }

        mCmdQueue.set([[mMetalDevice.get() newCommandQueue] ANGLE_MTL_AUTORELEASE]);

        mCapsInitialized = false;

        if (!mGlslangInitialized)
        {
            GlslangInitialize();
            mGlslangInitialized = true;
        }

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
    mDefaultShaders  = nil;
    mMetalDevice     = nil;
    mCapsInitialized = false;

    if (mGlslangInitialized)
    {
        GlslangRelease();
        mGlslangInitialized = false;
    }
}

bool DisplayMtl::testDeviceLost()
{
    return false;
}

egl::Error DisplayMtl::restoreLostDevice(const egl::Display *display)
{
    return egl::NoError();
}

std::string DisplayMtl::getVendorString() const
{
    ANGLE_MTL_OBJC_SCOPE
    {
        std::string vendorString = "Google Inc.";
        if (mMetalDevice)
        {
            vendorString += " Metal Renderer: ";
            vendorString += mMetalDevice.get().name.UTF8String;
        }

        return vendorString;
    }
}

DeviceImpl *DisplayMtl::createDevice()
{
    UNIMPLEMENTED();
    return nullptr;
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
    return egl::EglBadAccess();
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
    UNIMPLEMENTED();
    return static_cast<SurfaceImpl *>(0);
}

SurfaceImpl *DisplayMtl::createPbufferFromClientBuffer(const egl::SurfaceState &state,
                                                       EGLenum buftype,
                                                       EGLClientBuffer clientBuffer,
                                                       const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return static_cast<SurfaceImpl *>(0);
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
    UNIMPLEMENTED();
    return nullptr;
}

rx::ContextImpl *DisplayMtl::createContext(const gl::State &state,
                                           gl::ErrorSet *errorSet,
                                           const egl::Config *configuration,
                                           const gl::Context *shareContext,
                                           const egl::AttributeMap &attribs)
{
    return new ContextMtl(state, errorSet, this);
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

gl::Version DisplayMtl::getMaxSupportedESVersion() const
{
    return mtl::kMaxSupportedGLVersion;
}

gl::Version DisplayMtl::getMaxConformantESVersion() const
{
    return std::min(getMaxSupportedESVersion(), gl::Version(2, 0));
}

EGLSyncImpl *DisplayMtl::createSync(const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return nullptr;
}

egl::Error DisplayMtl::makeCurrent(egl::Surface *drawSurface,
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
    outExtensions->flexibleSurfaceCompatibility = true;
}

void DisplayMtl::generateCaps(egl::Caps *outCaps) const {}

void DisplayMtl::populateFeatureList(angle::FeatureList *features) {}

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

    config.surfaceType = EGL_WINDOW_BIT;

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

        // No DS
        config.depthSize   = 0;
        config.stencilSize = 0;
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

std::string DisplayMtl::getRendererDescription() const
{
    ANGLE_MTL_OBJC_SCOPE
    {
        std::string desc = "Metal Renderer";

        if (mMetalDevice)
        {
            desc += ": ";
            desc += mMetalDevice.get().name.UTF8String;
        }

        return desc;
    }
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
    mNativeCaps.max2DTextureSize          = 16384;
    mNativeCaps.maxVaryingVectors         = 31;
    mNativeCaps.maxVertexOutputComponents = 124;
#else
    if ([getMetalDevice() supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily3_v1])
    {
        mNativeCaps.max2DTextureSize          = 16384;
        mNativeCaps.maxVertexOutputComponents = 124;
        mNativeCaps.maxVaryingVectors         = mNativeCaps.maxVertexOutputComponents / 4;
    }
    else
    {
        mNativeCaps.max2DTextureSize          = 8192;
        mNativeCaps.maxVertexOutputComponents = 60;
        mNativeCaps.maxVaryingVectors         = mNativeCaps.maxVertexOutputComponents / 4;
    }
#endif

    mNativeCaps.maxArrayTextureLayers = 2048;
    mNativeCaps.maxLODBias            = 0;
    mNativeCaps.maxCubeMapTextureSize = mNativeCaps.max2DTextureSize;
    mNativeCaps.maxRenderbufferSize   = mNativeCaps.max2DTextureSize;
    mNativeCaps.minAliasedPointSize   = 1;
    // NOTE(hqle): Metal has some problems drawing big point size even though
    // Metal-Feature-Set-Tables.pdf says that max supported point size is 511. We limit it to 64 for
    // now.
    // http://anglebug.com/4816
    mNativeCaps.maxAliasedPointSize = 64;

    mNativeCaps.minAliasedLineWidth = 1.0f;
    mNativeCaps.maxAliasedLineWidth = 1.0f;

    mNativeCaps.maxDrawBuffers       = mtl::kMaxRenderTargets;
    mNativeCaps.maxFramebufferWidth  = mNativeCaps.max2DTextureSize;
    mNativeCaps.maxFramebufferHeight = mNativeCaps.max2DTextureSize;
    mNativeCaps.maxColorAttachments  = mtl::kMaxRenderTargets;
    mNativeCaps.maxViewportWidth     = mNativeCaps.max2DTextureSize;
    mNativeCaps.maxViewportHeight    = mNativeCaps.max2DTextureSize;

    // NOTE(hqle): MSAA
    mNativeCaps.maxSampleMaskWords     = 0;
    mNativeCaps.maxColorTextureSamples = 1;
    mNativeCaps.maxDepthTextureSamples = 1;
    mNativeCaps.maxIntegerSamples      = 1;

    mNativeCaps.maxVertexAttributes           = mtl::kMaxVertexAttribs;
    mNativeCaps.maxVertexAttribBindings       = mtl::kMaxVertexAttribs;
    mNativeCaps.maxVertexAttribRelativeOffset = std::numeric_limits<GLint>::max();
    mNativeCaps.maxVertexAttribStride         = std::numeric_limits<GLint>::max();

    mNativeCaps.maxElementsIndices  = std::numeric_limits<GLuint>::max();
    mNativeCaps.maxElementsVertices = std::numeric_limits<GLuint>::max();

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

    GLuint maxUniformVectors = mtl::kDefaultUniformsMaxSize / (sizeof(GLfloat) * 4);

    const GLuint maxUniformComponents = maxUniformVectors * 4;

    // Uniforms are implemented using a uniform buffer, so the max number of uniforms we can
    // support is the max buffer range divided by the size of a single uniform (4X float).
    mNativeCaps.maxVertexUniformVectors                              = maxUniformVectors;
    mNativeCaps.maxShaderUniformComponents[gl::ShaderType::Vertex]   = maxUniformComponents;
    mNativeCaps.maxFragmentUniformVectors                            = maxUniformVectors;
    mNativeCaps.maxShaderUniformComponents[gl::ShaderType::Fragment] = maxUniformComponents;

    // NOTE(hqle): support UBO (ES 3.0 feature)
    mNativeCaps.maxShaderUniformBlocks[gl::ShaderType::Vertex]   = 0;
    mNativeCaps.maxShaderUniformBlocks[gl::ShaderType::Fragment] = 0;
    mNativeCaps.maxCombinedUniformBlocks                         = 0;

    // Note that we currently implement textures as combined image+samplers, so the limit is
    // the minimum of supported samplers and sampled images.
    mNativeCaps.maxCombinedTextureImageUnits                         = mtl::kMaxGLSamplerBindings;
    mNativeCaps.maxShaderTextureImageUnits[gl::ShaderType::Fragment] = mtl::kMaxShaderSamplers;
    mNativeCaps.maxShaderTextureImageUnits[gl::ShaderType::Vertex]   = mtl::kMaxShaderSamplers;

    // NOTE(hqle): support storage buffer.
    const uint32_t maxPerStageStorageBuffers                     = 0;
    mNativeCaps.maxShaderStorageBlocks[gl::ShaderType::Vertex]   = maxPerStageStorageBuffers;
    mNativeCaps.maxShaderStorageBlocks[gl::ShaderType::Fragment] = maxPerStageStorageBuffers;
    mNativeCaps.maxCombinedShaderStorageBlocks                   = maxPerStageStorageBuffers;

    // Fill in additional limits for UBOs and SSBOs.
    mNativeCaps.maxUniformBufferBindings     = 0;
    mNativeCaps.maxUniformBlockSize          = 0;
    mNativeCaps.uniformBufferOffsetAlignment = 0;

    mNativeCaps.maxShaderStorageBufferBindings     = 0;
    mNativeCaps.maxShaderStorageBlockSize          = 0;
    mNativeCaps.shaderStorageBufferOffsetAlignment = 0;

    // NOTE(hqle): support UBO
    for (gl::ShaderType shaderType : gl::kAllGraphicsShaderTypes)
    {
        mNativeCaps.maxCombinedShaderUniformComponents[shaderType] = maxUniformComponents;
    }

    mNativeCaps.maxCombinedShaderOutputResources = 0;

    mNativeCaps.maxTransformFeedbackInterleavedComponents =
        gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS;
    mNativeCaps.maxTransformFeedbackSeparateAttributes =
        gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS;
    mNativeCaps.maxTransformFeedbackSeparateComponents =
        gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS;

    // NOTE(hqle): support MSAA.
    mNativeCaps.maxSamples = 1;

    // GL_APPLE_clip_distance
    mNativeCaps.maxClipDistances = 8;
}

void DisplayMtl::initializeExtensions() const
{
    // Reset
    mNativeExtensions = gl::Extensions();

    // Enable this for simple buffer readback testing, but some functionality is missing.
    // NOTE(hqle): Support full mapBufferRange extension.
    mNativeExtensions.mapBufferOES           = true;
    mNativeExtensions.mapBufferRange         = false;
    mNativeExtensions.textureStorage         = true;
    mNativeExtensions.drawBuffers            = false;
    mNativeExtensions.fragDepth              = true;
    mNativeExtensions.framebufferBlit        = false;
    mNativeExtensions.framebufferMultisample = false;
    mNativeExtensions.copyTexture            = false;
    mNativeExtensions.copyCompressedTexture  = false;
    mNativeExtensions.debugMarker            = false;
    mNativeExtensions.robustness             = true;
    mNativeExtensions.textureBorderClampOES  = false;  // not implemented yet
    mNativeExtensions.translatedShaderSource = true;
    mNativeExtensions.discardFramebuffer     = true;

    // Enable EXT_blend_minmax
    mNativeExtensions.blendMinMax = true;

    mNativeExtensions.eglImageOES         = false;
    mNativeExtensions.eglImageExternalOES = false;
    // NOTE(hqle): Support GL_OES_EGL_image_external_essl3.
    mNativeExtensions.eglImageExternalEssl3OES = false;

    mNativeExtensions.memoryObject   = false;
    mNativeExtensions.memoryObjectFd = false;

    mNativeExtensions.semaphore   = false;
    mNativeExtensions.semaphoreFd = false;

    mNativeExtensions.instancedArraysANGLE = mFeatures.hasBaseVertexInstancedDraw.enabled;
    mNativeExtensions.instancedArraysEXT   = mNativeExtensions.instancedArraysANGLE;

    mNativeExtensions.robustBufferAccessBehavior = false;

    mNativeExtensions.eglSyncOES = false;

    // NOTE(hqle): support occlusion query
    mNativeExtensions.occlusionQueryBoolean = false;

    mNativeExtensions.disjointTimerQuery          = false;
    mNativeExtensions.queryCounterBitsTimeElapsed = false;
    mNativeExtensions.queryCounterBitsTimestamp   = false;

    mNativeExtensions.textureFilterAnisotropic = true;
    mNativeExtensions.maxTextureAnisotropy     = 16;

    mNativeExtensions.textureNPOTOES = true;

    mNativeExtensions.texture3DOES = false;

    mNativeExtensions.standardDerivativesOES = true;

    mNativeExtensions.elementIndexUintOES = true;

    // GL_APPLE_clip_distance
    mNativeExtensions.clipDistanceAPPLE = true;
}

void DisplayMtl::initializeTextureCaps() const
{
    mNativeTextureCaps.clear();

    mFormatTable.generateTextureCaps(this, &mNativeTextureCaps,
                                     &mNativeCaps.compressedTextureFormats);

    // Re-verify texture extensions.
    mNativeExtensions.setTextureExtensionSupport(mNativeTextureCaps);

    // Disable all depth buffer and stencil buffer readback extensions until we need them
    mNativeExtensions.readDepthNV         = false;
    mNativeExtensions.readStencilNV       = false;
    mNativeExtensions.depthBufferFloat2NV = false;
}

void DisplayMtl::initializeFeatures()
{
    // default values:
    mFeatures.hasBaseVertexInstancedDraw.enabled        = true;
    mFeatures.hasDepthTextureFiltering.enabled          = false;
    mFeatures.hasNonUniformDispatch.enabled             = true;
    mFeatures.hasTextureSwizzle.enabled                 = false;
    mFeatures.allowSeparatedDepthStencilBuffers.enabled = false;

#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
    mFeatures.hasDepthTextureFiltering.enabled        = true;
    mFeatures.allowMultisampleStoreAndResolve.enabled = true;

    // Texture swizzle is only supported if macos sdk 10.15 is present
#    if defined(__MAC_10_15)
    if (ANGLE_APPLE_AVAILABLE_XC(10.15, 13.0))
    {
        // The runtime OS must be MacOS 10.15+ or Mac Catalyst for this to be supported:
        ANGLE_FEATURE_CONDITION((&mFeatures), hasTextureSwizzle,
                                [getMetalDevice() supportsFamily:MTLGPUFamilyMac2]);
    }
#    endif
#elif TARGET_OS_IOS
    // Base Vertex drawing is only supported since GPU family 3.
    ANGLE_FEATURE_CONDITION((&mFeatures), hasBaseVertexInstancedDraw,
                            [getMetalDevice() supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily3_v1]);

    ANGLE_FEATURE_CONDITION((&mFeatures), hasNonUniformDispatch,
                            [getMetalDevice() supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily4_v1]);

    ANGLE_FEATURE_CONDITION((&mFeatures), allowMultisampleStoreAndResolve,
                            [getMetalDevice() supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily3_v1]);

#    if !TARGET_OS_SIMULATOR
    mFeatures.allowSeparatedDepthStencilBuffers.enabled = true;
#    endif
#endif

    angle::PlatformMethods *platform = ANGLEPlatformCurrent();
    platform->overrideFeaturesMtl(platform, &mFeatures);

    ApplyFeatureOverrides(&mFeatures, getState());
}

angle::Result DisplayMtl::initializeShaderLibrary()
{
    mtl::AutoObjCObj<NSError> err = nil;

    const uint8_t *compiled_shader_binary;
    size_t compiled_shader_binary_len;

#if !defined(NDEBUG)
    compiled_shader_binary     = compiled_default_metallib_debug;
    compiled_shader_binary_len = compiled_default_metallib_debug_len;
#else
    compiled_shader_binary                              = compiled_default_metallib;
    compiled_shader_binary_len                          = compiled_default_metallib_len;
#endif

    mDefaultShaders = CreateShaderLibraryFromBinary(getMetalDevice(), compiled_shader_binary,
                                                    compiled_shader_binary_len, &err);

    if (err && !mDefaultShaders)
    {
        ANGLE_MTL_OBJC_SCOPE
        {
            ERR() << "Internal error: " << err.get().localizedDescription.UTF8String;
        }
        return angle::Result::Stop;
    }

    return angle::Result::Continue;
}

}  // namespace rx
