//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// vk_utils:
//    Helper functions for the Vulkan Caps.
//

#include "libANGLE/renderer/vulkan/vk_caps_utils.h"

#include <type_traits>

#include "common/utilities.h"
#include "libANGLE/Caps.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/vulkan/DisplayVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "vk_format_utils.h"

namespace
{
constexpr unsigned int kComponentsPerVector = 4;
}  // anonymous namespace

namespace rx
{

void RendererVk::ensureCapsInitialized() const
{
    if (mCapsInitialized)
        return;
    mCapsInitialized = true;

    ASSERT(mCurrentQueueFamilyIndex < mQueueFamilyProperties.size());
    const VkQueueFamilyProperties &queueFamilyProperties =
        mQueueFamilyProperties[mCurrentQueueFamilyIndex];

    mNativeExtensions.setTextureExtensionSupport(mNativeTextureCaps);

    // Enable this for simple buffer readback testing, but some functionality is missing.
    // TODO(jmadill): Support full mapBufferRange extension.
    mNativeExtensions.mapBuffer              = true;
    mNativeExtensions.mapBufferRange         = true;
    mNativeExtensions.textureStorage         = true;
    mNativeExtensions.framebufferBlit        = true;
    mNativeExtensions.copyTexture            = true;
    mNativeExtensions.copyCompressedTexture  = true;
    mNativeExtensions.debugMarker            = true;
    mNativeExtensions.robustness             = true;
    mNativeExtensions.textureBorderClamp     = false;  // not implemented yet
    mNativeExtensions.translatedShaderSource = true;

    mNativeExtensions.eglImage         = true;
    mNativeExtensions.eglImageExternal = true;
    // TODO(geofflang): Support GL_OES_EGL_image_external_essl3. http://anglebug.com/2668
    mNativeExtensions.eglImageExternalEssl3 = false;

    mNativeExtensions.memoryObject   = true;
    mNativeExtensions.memoryObjectFd = getFeatures().supportsExternalMemoryFd;

    mNativeExtensions.semaphore   = true;
    mNativeExtensions.semaphoreFd = getFeatures().supportsExternalSemaphoreFd;

    // TODO: Enable this always and emulate instanced draws if any divisor exceeds the maximum
    // supported.  http://anglebug.com/2672
    mNativeExtensions.instancedArraysANGLE = mMaxVertexAttribDivisor > 1;

    // Only expose robust buffer access if the physical device supports it.
    mNativeExtensions.robustBufferAccessBehavior = mPhysicalDeviceFeatures.robustBufferAccess;

    mNativeExtensions.eglSync = true;

    // We use secondary command buffers almost everywhere and they require a feature to be
    // able to execute in the presence of queries.  As a result, we won't support queries
    // unless that feature is available.
    mNativeExtensions.occlusionQueryBoolean =
        vk::CommandBuffer::SupportsQueries(mPhysicalDeviceFeatures);

    // From the Vulkan specs:
    // > The number of valid bits in a timestamp value is determined by the
    // > VkQueueFamilyProperties::timestampValidBits property of the queue on which the timestamp is
    // > written. Timestamps are supported on any queue which reports a non-zero value for
    // > timestampValidBits via vkGetPhysicalDeviceQueueFamilyProperties.
    mNativeExtensions.disjointTimerQuery          = queueFamilyProperties.timestampValidBits > 0;
    mNativeExtensions.queryCounterBitsTimeElapsed = queueFamilyProperties.timestampValidBits;
    mNativeExtensions.queryCounterBitsTimestamp   = queueFamilyProperties.timestampValidBits;

    mNativeExtensions.textureFilterAnisotropic =
        mPhysicalDeviceFeatures.samplerAnisotropy &&
        mPhysicalDeviceProperties.limits.maxSamplerAnisotropy > 1.0f;
    mNativeExtensions.maxTextureAnisotropy =
        mNativeExtensions.textureFilterAnisotropic
            ? mPhysicalDeviceProperties.limits.maxSamplerAnisotropy
            : 0.0f;

    // Vulkan natively supports non power-of-two textures
    mNativeExtensions.textureNPOT = true;

    // TODO(lucferron): Eventually remove everything above this line in this function as the caps
    // get implemented.
    // https://vulkan.lunarg.com/doc/view/1.0.30.0/linux/vkspec.chunked/ch31s02.html
    mNativeCaps.maxElementIndex       = std::numeric_limits<GLuint>::max() - 1;
    mNativeCaps.max3DTextureSize      = mPhysicalDeviceProperties.limits.maxImageDimension3D;
    mNativeCaps.max2DTextureSize      = mPhysicalDeviceProperties.limits.maxImageDimension2D;
    mNativeCaps.maxArrayTextureLayers = mPhysicalDeviceProperties.limits.maxImageArrayLayers;
    mNativeCaps.maxLODBias            = mPhysicalDeviceProperties.limits.maxSamplerLodBias;
    mNativeCaps.maxCubeMapTextureSize = mPhysicalDeviceProperties.limits.maxImageDimensionCube;
    mNativeCaps.maxRenderbufferSize   = mNativeCaps.max2DTextureSize;
    mNativeCaps.minAliasedPointSize =
        std::max(1.0f, mPhysicalDeviceProperties.limits.pointSizeRange[0]);
    mNativeCaps.maxAliasedPointSize = mPhysicalDeviceProperties.limits.pointSizeRange[1];

    mNativeCaps.minAliasedLineWidth = 1.0f;
    mNativeCaps.maxAliasedLineWidth = 1.0f;

    mNativeCaps.maxDrawBuffers =
        std::min<uint32_t>(mPhysicalDeviceProperties.limits.maxColorAttachments,
                           mPhysicalDeviceProperties.limits.maxFragmentOutputAttachments);
    mNativeCaps.maxFramebufferWidth  = mPhysicalDeviceProperties.limits.maxFramebufferWidth;
    mNativeCaps.maxFramebufferHeight = mPhysicalDeviceProperties.limits.maxFramebufferHeight;
    mNativeCaps.maxColorAttachments  = mPhysicalDeviceProperties.limits.maxColorAttachments;
    mNativeCaps.maxViewportWidth     = mPhysicalDeviceProperties.limits.maxViewportDimensions[0];
    mNativeCaps.maxViewportHeight    = mPhysicalDeviceProperties.limits.maxViewportDimensions[1];
    mNativeCaps.maxSampleMaskWords   = mPhysicalDeviceProperties.limits.maxSampleMaskWords;
    mNativeCaps.maxColorTextureSamples =
        mPhysicalDeviceProperties.limits.sampledImageColorSampleCounts;
    mNativeCaps.maxDepthTextureSamples =
        mPhysicalDeviceProperties.limits.sampledImageDepthSampleCounts;
    mNativeCaps.maxIntegerSamples =
        mPhysicalDeviceProperties.limits.sampledImageIntegerSampleCounts;

    mNativeCaps.maxVertexAttributes     = mPhysicalDeviceProperties.limits.maxVertexInputAttributes;
    mNativeCaps.maxVertexAttribBindings = mPhysicalDeviceProperties.limits.maxVertexInputBindings;
    mNativeCaps.maxVertexAttribRelativeOffset =
        mPhysicalDeviceProperties.limits.maxVertexInputAttributeOffset;
    mNativeCaps.maxVertexAttribStride =
        mPhysicalDeviceProperties.limits.maxVertexInputBindingStride;

    mNativeCaps.maxElementsIndices  = std::numeric_limits<GLuint>::max();
    mNativeCaps.maxElementsVertices = std::numeric_limits<GLuint>::max();

    // Looks like all floats are IEEE according to the docs here:
    // https://www.khronos.org/registry/vulkan/specs/1.0-wsi_extensions/html/vkspec.html#spirvenv-precision-operation
    mNativeCaps.vertexHighpFloat.setIEEEFloat();
    mNativeCaps.vertexMediumpFloat.setIEEEFloat();
    mNativeCaps.vertexLowpFloat.setIEEEFloat();
    mNativeCaps.fragmentHighpFloat.setIEEEFloat();
    mNativeCaps.fragmentMediumpFloat.setIEEEFloat();
    mNativeCaps.fragmentLowpFloat.setIEEEFloat();

    // Can't find documentation on the int precision in Vulkan.
    mNativeCaps.vertexHighpInt.setTwosComplementInt(32);
    mNativeCaps.vertexMediumpInt.setTwosComplementInt(32);
    mNativeCaps.vertexLowpInt.setTwosComplementInt(32);
    mNativeCaps.fragmentHighpInt.setTwosComplementInt(32);
    mNativeCaps.fragmentMediumpInt.setTwosComplementInt(32);
    mNativeCaps.fragmentLowpInt.setTwosComplementInt(32);

    // TODO(lucferron): This is something we'll need to implement custom in the back-end.
    // Vulkan doesn't do any waiting for you, our back-end code is going to manage sync objects,
    // and we'll have to check that we've exceeded the max wait timeout. Also, this is ES 3.0 so
    // we'll defer the implementation until we tackle the next version.
    // mNativeCaps.maxServerWaitTimeout

    GLuint maxUniformVectors = mPhysicalDeviceProperties.limits.maxUniformBufferRange /
                               (sizeof(GLfloat) * kComponentsPerVector);

    // Clamp the maxUniformVectors to 1024u, on AMD the maxUniformBufferRange is way too high.
    maxUniformVectors = std::min(1024u, maxUniformVectors);

    const GLuint maxUniformComponents = maxUniformVectors * kComponentsPerVector;

    // Uniforms are implemented using a uniform buffer, so the max number of uniforms we can
    // support is the max buffer range divided by the size of a single uniform (4X float).
    mNativeCaps.maxVertexUniformVectors                              = maxUniformVectors;
    mNativeCaps.maxShaderUniformComponents[gl::ShaderType::Vertex]   = maxUniformComponents;
    mNativeCaps.maxFragmentUniformVectors                            = maxUniformVectors;
    mNativeCaps.maxShaderUniformComponents[gl::ShaderType::Fragment] = maxUniformComponents;

    // TODO(jmadill): this is an ES 3.0 property and we can skip implementing it for now.
    // This is maxDescriptorSetUniformBuffers minus the number of uniform buffers we
    // reserve for internal variables. We reserve one per shader stage for default uniforms
    // and likely one per shader stage for ANGLE internal variables.
    // mNativeCaps.maxShaderUniformBlocks[gl::ShaderType::Vertex] = ...

    // we use the same bindings on each stage, so the limitation is the same combined or not.
    mNativeCaps.maxCombinedTextureImageUnits =
        mPhysicalDeviceProperties.limits.maxPerStageDescriptorSamplers;
    mNativeCaps.maxShaderTextureImageUnits[gl::ShaderType::Fragment] =
        mPhysicalDeviceProperties.limits.maxPerStageDescriptorSamplers;
    mNativeCaps.maxShaderTextureImageUnits[gl::ShaderType::Vertex] =
        mPhysicalDeviceProperties.limits.maxPerStageDescriptorSamplers;

    // The max vertex output components should not include gl_Position.
    // The gles2.0 section 2.10 states that "gl_Position is not a varying variable and does
    // not count against this limit.", but the Vulkan spec has no such mention in its Built-in
    // vars section. It is implicit that we need to actually reserve it for Vulkan in that case.
    // TODO(lucferron): AMD has a weird behavior when we edge toward the maximum number of varyings
    // and can often crash. Reserving an additional varying just for them bringing the total to 2.
    // http://anglebug.com/2483
    constexpr GLint kReservedVaryingCount = 2;
    mNativeCaps.maxVaryingVectors =
        (mPhysicalDeviceProperties.limits.maxVertexOutputComponents / 4) - kReservedVaryingCount;
    mNativeCaps.maxVertexOutputComponents = mNativeCaps.maxVaryingVectors * 4;

    mNativeCaps.subPixelBits = mPhysicalDeviceProperties.limits.subPixelPrecisionBits;
}

namespace egl_vk
{

namespace
{

EGLint ComputeMaximumPBufferPixels(const VkPhysicalDeviceProperties &physicalDeviceProperties)
{
    // EGLints are signed 32-bit integers, it's fairly easy to overflow them, especially since
    // Vulkan's minimum guaranteed VkImageFormatProperties::maxResourceSize is 2^31 bytes.
    constexpr uint64_t kMaxValueForEGLint =
        static_cast<uint64_t>(std::numeric_limits<EGLint>::max());

    // TODO(geofflang): Compute the maximum size of a pbuffer by using the maxResourceSize result
    // from vkGetPhysicalDeviceImageFormatProperties for both the color and depth stencil format and
    // the exact image creation parameters that would be used to create the pbuffer. Because it is
    // always safe to return out-of-memory errors on pbuffer allocation, it's fine to simply return
    // the number of pixels in a max width by max height pbuffer for now. http://anglebug.com/2622

    // Storing the result of squaring a 32-bit unsigned int in a 64-bit unsigned int is safe.
    static_assert(std::is_same<decltype(physicalDeviceProperties.limits.maxImageDimension2D),
                               uint32_t>::value,
                  "physicalDeviceProperties.limits.maxImageDimension2D expected to be a uint32_t.");
    const uint64_t maxDimensionsSquared =
        static_cast<uint64_t>(physicalDeviceProperties.limits.maxImageDimension2D) *
        static_cast<uint64_t>(physicalDeviceProperties.limits.maxImageDimension2D);

    return static_cast<EGLint>(std::min(maxDimensionsSquared, kMaxValueForEGLint));
}

// Generates a basic config for a combination of color format, depth stencil format and sample
// count.
egl::Config GenerateDefaultConfig(const RendererVk *renderer,
                                  const gl::InternalFormat &colorFormat,
                                  const gl::InternalFormat &depthStencilFormat,
                                  EGLint sampleCount)
{
    const VkPhysicalDeviceProperties &physicalDeviceProperties =
        renderer->getPhysicalDeviceProperties();
    gl::Version maxSupportedESVersion = renderer->getMaxSupportedESVersion();

    EGLint es2Support = (maxSupportedESVersion.major >= 2 ? EGL_OPENGL_ES2_BIT : 0);
    EGLint es3Support = (maxSupportedESVersion.major >= 3 ? EGL_OPENGL_ES3_BIT : 0);

    egl::Config config;

    config.renderTargetFormat = colorFormat.internalFormat;
    config.depthStencilFormat = depthStencilFormat.internalFormat;
    config.bufferSize         = colorFormat.pixelBytes * 8;
    config.redSize            = colorFormat.redBits;
    config.greenSize          = colorFormat.greenBits;
    config.blueSize           = colorFormat.blueBits;
    config.alphaSize          = colorFormat.alphaBits;
    config.alphaMaskSize      = 0;
    config.bindToTextureRGB   = colorFormat.format == GL_RGB;
    config.bindToTextureRGBA  = colorFormat.format == GL_RGBA || colorFormat.format == GL_BGRA_EXT;
    config.colorBufferType    = EGL_RGB_BUFFER;
    config.configCaveat       = EGL_NONE;
    config.conformant         = es2Support | es3Support;
    config.depthSize          = depthStencilFormat.depthBits;
    config.stencilSize        = depthStencilFormat.stencilBits;
    config.level              = 0;
    config.matchNativePixmap  = EGL_NONE;
    config.maxPBufferWidth    = physicalDeviceProperties.limits.maxImageDimension2D;
    config.maxPBufferHeight   = physicalDeviceProperties.limits.maxImageDimension2D;
    config.maxPBufferPixels   = ComputeMaximumPBufferPixels(physicalDeviceProperties);
    config.maxSwapInterval    = 1;
    config.minSwapInterval    = 0;
    config.nativeRenderable   = EGL_TRUE;
    config.nativeVisualID     = 0;
    config.nativeVisualType   = EGL_NONE;
    config.renderableType     = es2Support | es3Support;
    config.sampleBuffers      = (sampleCount > 0) ? 1 : 0;
    config.samples            = sampleCount;
    config.surfaceType        = EGL_WINDOW_BIT | EGL_PBUFFER_BIT;
    // Vulkan surfaces use a different origin than OpenGL, always prefer to be flipped vertically if
    // possible.
    config.optimalOrientation    = EGL_SURFACE_ORIENTATION_INVERT_Y_ANGLE;
    config.transparentType       = EGL_NONE;
    config.transparentRedValue   = 0;
    config.transparentGreenValue = 0;
    config.transparentBlueValue  = 0;
    config.colorComponentType =
        gl_egl::GLComponentTypeToEGLColorComponentType(colorFormat.componentType);

    return config;
}

}  // anonymous namespace

egl::ConfigSet GenerateConfigs(const GLenum *colorFormats,
                               size_t colorFormatsCount,
                               const GLenum *depthStencilFormats,
                               size_t depthStencilFormatCount,
                               const EGLint *sampleCounts,
                               size_t sampleCountsCount,
                               DisplayVk *display)
{
    ASSERT(colorFormatsCount > 0);
    ASSERT(display != nullptr);

    egl::ConfigSet configSet;

    for (size_t colorFormatIdx = 0; colorFormatIdx < colorFormatsCount; colorFormatIdx++)
    {
        const gl::InternalFormat &colorFormatInfo =
            gl::GetSizedInternalFormatInfo(colorFormats[colorFormatIdx]);
        ASSERT(colorFormatInfo.sized);

        for (size_t depthStencilFormatIdx = 0; depthStencilFormatIdx < depthStencilFormatCount;
             depthStencilFormatIdx++)
        {
            const gl::InternalFormat &depthStencilFormatInfo =
                gl::GetSizedInternalFormatInfo(depthStencilFormats[depthStencilFormatIdx]);
            ASSERT(depthStencilFormats[depthStencilFormatIdx] == GL_NONE ||
                   depthStencilFormatInfo.sized);

            for (size_t sampleCountIndex = 0; sampleCountIndex < sampleCountsCount;
                 sampleCountIndex++)
            {
                egl::Config config =
                    GenerateDefaultConfig(display->getRenderer(), colorFormatInfo,
                                          depthStencilFormatInfo, sampleCounts[sampleCountIndex]);
                if (display->checkConfigSupport(&config))
                {
                    configSet.add(config);
                }
            }
        }
    }

    return configSet;
}

}  // namespace egl_vk

}  // namespace rx
