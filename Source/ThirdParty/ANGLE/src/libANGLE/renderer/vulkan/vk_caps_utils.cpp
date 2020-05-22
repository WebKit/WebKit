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
#include "libANGLE/renderer/driver_utils.h"
#include "libANGLE/renderer/vulkan/DisplayVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "vk_format_utils.h"

namespace
{
constexpr unsigned int kComponentsPerVector = 4;
}  // anonymous namespace

namespace rx
{

GLint LimitToInt(const uint32_t physicalDeviceValue)
{
    // Limit to INT_MAX / 2 instead of INT_MAX.  If the limit is queried as float, the imprecision
    // in floating point can cause the value to exceed INT_MAX.  This trips dEQP up.
    return std::min(physicalDeviceValue,
                    static_cast<uint32_t>(std::numeric_limits<int32_t>::max() / 2));
}

void RendererVk::ensureCapsInitialized() const
{
    if (mCapsInitialized)
        return;
    mCapsInitialized = true;

    ASSERT(mCurrentQueueFamilyIndex < mQueueFamilyProperties.size());
    const VkQueueFamilyProperties &queueFamilyProperties =
        mQueueFamilyProperties[mCurrentQueueFamilyIndex];
    const VkPhysicalDeviceLimits &limitsVk = mPhysicalDeviceProperties.limits;

    mNativeExtensions.setTextureExtensionSupport(mNativeTextureCaps);

    // TODO: http://anglebug.com/3609
    // Due to a dEQP bug, this extension cannot be exposed until EXT_texture_sRGB_decode is
    // implemented
    mNativeExtensions.sRGBR8EXT = false;

    // To ensure that ETC2/EAC formats are enabled only on hardware that supports them natively,
    // this flag is not set by the function above and must be set explicitly. It exposes
    // ANGLE_compressed_texture_etc extension string.
    mNativeExtensions.compressedTextureETC =
        (mPhysicalDeviceFeatures.textureCompressionETC2 == VK_TRUE) &&
        gl::DetermineCompressedTextureETCSupport(mNativeTextureCaps);

    // Vulkan doesn't support ASTC 3D block textures, which are required by
    // GL_OES_texture_compression_astc.
    mNativeExtensions.textureCompressionASTCOES = false;

    // Vulkan doesn't guarantee HDR blocks decoding without VK_EXT_texture_compression_astc_hdr.
    mNativeExtensions.textureCompressionASTCHDRKHR = false;

    // Vulkan supports sliced 3D ASTC texture uploads when ASTC is supported.
    mNativeExtensions.textureCompressionSliced3dASTCKHR =
        mNativeExtensions.textureCompressionASTCLDRKHR;

    // Enable EXT_compressed_ETC1_RGB8_sub_texture
    mNativeExtensions.compressedETC1RGB8SubTexture = mNativeExtensions.compressedETC1RGB8TextureOES;

    // Enable this for simple buffer readback testing, but some functionality is missing.
    // TODO(jmadill): Support full mapBufferRange extension.
    mNativeExtensions.mapBufferOES           = true;
    mNativeExtensions.mapBufferRange         = true;
    mNativeExtensions.textureStorage         = true;
    mNativeExtensions.drawBuffers            = true;
    mNativeExtensions.fragDepth              = true;
    mNativeExtensions.framebufferBlit        = true;
    mNativeExtensions.framebufferMultisample = true;
    mNativeExtensions.copyTexture            = true;
    mNativeExtensions.copyCompressedTexture  = true;
    mNativeExtensions.debugMarker            = true;
    mNativeExtensions.robustness =
        !IsSwiftshader(mPhysicalDeviceProperties.vendorID, mPhysicalDeviceProperties.deviceID);
    mNativeExtensions.textureBorderClampOES  = false;  // not implemented yet
    mNativeExtensions.translatedShaderSource = true;
    mNativeExtensions.discardFramebuffer     = true;

    // Enable EXT_texture_type_2_10_10_10_REV
    mNativeExtensions.textureFormat2101010REV = true;

    // Enable ANGLE_base_vertex_base_instance
    mNativeExtensions.baseVertexBaseInstance = true;

    // Enable OES/EXT_draw_elements_base_vertex
    mNativeExtensions.drawElementsBaseVertexOES = true;
    mNativeExtensions.drawElementsBaseVertexEXT = true;

    // Enable EXT_blend_minmax
    mNativeExtensions.blendMinMax = true;

    mNativeExtensions.eglImageOES                  = true;
    mNativeExtensions.eglImageExternalOES          = true;
    mNativeExtensions.eglImageExternalWrapModesEXT = true;
    mNativeExtensions.eglImageExternalEssl3OES     = true;
    mNativeExtensions.eglImageArray                = true;
    mNativeExtensions.memoryObject                 = true;
    mNativeExtensions.memoryObjectFd               = getFeatures().supportsExternalMemoryFd.enabled;
    mNativeExtensions.memoryObjectFuchsiaANGLE =
        getFeatures().supportsExternalMemoryFuchsia.enabled;

    mNativeExtensions.semaphore   = true;
    mNativeExtensions.semaphoreFd = getFeatures().supportsExternalSemaphoreFd.enabled;
    mNativeExtensions.semaphoreFuchsiaANGLE =
        getFeatures().supportsExternalSemaphoreFuchsia.enabled;

    mNativeExtensions.vertexHalfFloatOES = true;

    // Enabled in HW if VK_EXT_vertex_attribute_divisor available, otherwise emulated
    mNativeExtensions.instancedArraysANGLE = true;
    mNativeExtensions.instancedArraysEXT   = true;

    // Only expose robust buffer access if the physical device supports it.
    mNativeExtensions.robustBufferAccessBehavior =
        (mPhysicalDeviceFeatures.robustBufferAccess == VK_TRUE);

    mNativeExtensions.eglSyncOES = true;

    mNativeExtensions.vertexAttribType1010102OES = true;

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
        mPhysicalDeviceFeatures.samplerAnisotropy && limitsVk.maxSamplerAnisotropy > 1.0f;
    mNativeExtensions.maxTextureAnisotropy =
        mNativeExtensions.textureFilterAnisotropic ? limitsVk.maxSamplerAnisotropy : 0.0f;

    // Vulkan natively supports non power-of-two textures
    mNativeExtensions.textureNPOTOES = true;

    mNativeExtensions.texture3DOES = true;

    // Vulkan natively supports standard derivatives
    mNativeExtensions.standardDerivativesOES = true;

    // Vulkan natively supports noperspective interpolation
    mNativeExtensions.noperspectiveInterpolationNV = true;

    // Vulkan natively supports 32-bit indices, entry in kIndexTypeMap
    mNativeExtensions.elementIndexUintOES = true;

    mNativeExtensions.fboRenderMipmapOES = true;

    // We support getting image data for Textures and Renderbuffers.
    mNativeExtensions.getImageANGLE = true;

    // Implemented in the translator
    mNativeExtensions.shaderNonConstGlobalInitializersEXT = true;

    // Vulkan has no restrictions of the format of cubemaps, so if the proper formats are supported,
    // creating a cube of any of these formats should be implicitly supported.
    mNativeExtensions.depthTextureCubeMapOES =
        mNativeExtensions.depthTextureOES && mNativeExtensions.packedDepthStencilOES;

    // Vulkan natively supports format reinterpretation
    mNativeExtensions.textureSRGBOverride = mNativeExtensions.sRGB;

    mNativeExtensions.gpuShader5EXT = vk::CanSupportGPUShader5EXT(mPhysicalDeviceFeatures);

    mNativeExtensions.textureFilteringCHROMIUM = getFeatures().supportsFilteringPrecision.enabled;

    // https://vulkan.lunarg.com/doc/view/1.0.30.0/linux/vkspec.chunked/ch31s02.html
    mNativeCaps.maxElementIndex  = std::numeric_limits<GLuint>::max() - 1;
    mNativeCaps.max3DTextureSize = LimitToInt(limitsVk.maxImageDimension3D);
    mNativeCaps.max2DTextureSize =
        std::min(limitsVk.maxFramebufferWidth, limitsVk.maxImageDimension2D);
    mNativeCaps.maxArrayTextureLayers = LimitToInt(limitsVk.maxImageArrayLayers);
    mNativeCaps.maxLODBias            = limitsVk.maxSamplerLodBias;
    mNativeCaps.maxCubeMapTextureSize = LimitToInt(limitsVk.maxImageDimensionCube);
    mNativeCaps.maxRenderbufferSize =
        std::min({limitsVk.maxImageDimension2D, limitsVk.maxFramebufferWidth,
                  limitsVk.maxFramebufferHeight});
    mNativeCaps.minAliasedPointSize = std::max(1.0f, limitsVk.pointSizeRange[0]);
    mNativeCaps.maxAliasedPointSize = limitsVk.pointSizeRange[1];

    mNativeCaps.minAliasedLineWidth = 1.0f;
    mNativeCaps.maxAliasedLineWidth = 1.0f;

    mNativeCaps.maxDrawBuffers =
        std::min(limitsVk.maxColorAttachments, limitsVk.maxFragmentOutputAttachments);
    mNativeCaps.maxFramebufferWidth  = LimitToInt(limitsVk.maxFramebufferWidth);
    mNativeCaps.maxFramebufferHeight = LimitToInt(limitsVk.maxFramebufferHeight);
    mNativeCaps.maxColorAttachments  = LimitToInt(limitsVk.maxColorAttachments);
    mNativeCaps.maxViewportWidth     = LimitToInt(limitsVk.maxViewportDimensions[0]);
    mNativeCaps.maxViewportHeight    = LimitToInt(limitsVk.maxViewportDimensions[1]);
    mNativeCaps.maxSampleMaskWords   = LimitToInt(limitsVk.maxSampleMaskWords);
    mNativeCaps.maxColorTextureSamples =
        limitsVk.sampledImageColorSampleCounts & vk_gl::kSupportedSampleCounts;
    mNativeCaps.maxDepthTextureSamples =
        limitsVk.sampledImageDepthSampleCounts & vk_gl::kSupportedSampleCounts;
    // TODO (ianelliott): Should mask this with vk_gl::kSupportedSampleCounts, but it causes
    // end2end test failures with SwiftShader because SwiftShader returns a sample count of 1 in
    // sampledImageIntegerSampleCounts.
    // See: http://anglebug.com/4197
    mNativeCaps.maxIntegerSamples = limitsVk.sampledImageIntegerSampleCounts;

    mNativeCaps.maxVertexAttributes     = LimitToInt(limitsVk.maxVertexInputAttributes);
    mNativeCaps.maxVertexAttribBindings = LimitToInt(limitsVk.maxVertexInputBindings);
    // Offset and stride are stored as uint16_t in PackedAttribDesc.
    mNativeCaps.maxVertexAttribRelativeOffset =
        std::min(static_cast<uint32_t>(std::numeric_limits<uint16_t>::max()),
                 limitsVk.maxVertexInputAttributeOffset);
    mNativeCaps.maxVertexAttribStride =
        std::min(static_cast<uint32_t>(std::numeric_limits<uint16_t>::max()),
                 limitsVk.maxVertexInputBindingStride);

    mNativeCaps.maxElementsIndices  = std::numeric_limits<GLint>::max();
    mNativeCaps.maxElementsVertices = std::numeric_limits<GLint>::max();

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

    // Compute shader limits.
    mNativeCaps.maxComputeWorkGroupCount[0] = LimitToInt(limitsVk.maxComputeWorkGroupCount[0]);
    mNativeCaps.maxComputeWorkGroupCount[1] = LimitToInt(limitsVk.maxComputeWorkGroupCount[1]);
    mNativeCaps.maxComputeWorkGroupCount[2] = LimitToInt(limitsVk.maxComputeWorkGroupCount[2]);
    mNativeCaps.maxComputeWorkGroupSize[0]  = LimitToInt(limitsVk.maxComputeWorkGroupSize[0]);
    mNativeCaps.maxComputeWorkGroupSize[1]  = LimitToInt(limitsVk.maxComputeWorkGroupSize[1]);
    mNativeCaps.maxComputeWorkGroupSize[2]  = LimitToInt(limitsVk.maxComputeWorkGroupSize[2]);
    mNativeCaps.maxComputeWorkGroupInvocations =
        LimitToInt(limitsVk.maxComputeWorkGroupInvocations);
    mNativeCaps.maxComputeSharedMemorySize = LimitToInt(limitsVk.maxComputeSharedMemorySize);

    // TODO(lucferron): This is something we'll need to implement custom in the back-end.
    // Vulkan doesn't do any waiting for you, our back-end code is going to manage sync objects,
    // and we'll have to check that we've exceeded the max wait timeout. Also, this is ES 3.0 so
    // we'll defer the implementation until we tackle the next version.
    // mNativeCaps.maxServerWaitTimeout

    GLuint maxUniformBlockSize = limitsVk.maxUniformBufferRange;

    // Clamp the maxUniformBlockSize to 64KB (majority of devices support up to this size
    // currently), on AMD the maxUniformBufferRange is near uint32_t max.
    maxUniformBlockSize = std::min(0x10000u, maxUniformBlockSize);

    const GLuint maxUniformVectors = maxUniformBlockSize / (sizeof(GLfloat) * kComponentsPerVector);
    const GLuint maxUniformComponents = maxUniformVectors * kComponentsPerVector;

    // Uniforms are implemented using a uniform buffer, so the max number of uniforms we can
    // support is the max buffer range divided by the size of a single uniform (4X float).
    mNativeCaps.maxVertexUniformVectors    = maxUniformVectors;
    mNativeCaps.maxFragmentUniformVectors  = maxUniformVectors;
    mNativeCaps.maxFragmentInputComponents = maxUniformComponents;
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        mNativeCaps.maxShaderUniformComponents[shaderType] = maxUniformComponents;
    }
    mNativeCaps.maxUniformLocations = maxUniformVectors;

    // Every stage has 1 reserved uniform buffer for the default uniforms, and 1 for the driver
    // uniforms.
    constexpr uint32_t kTotalReservedPerStageUniformBuffers =
        kReservedDriverUniformBindingCount + kReservedPerStageDefaultUniformBindingCount;
    constexpr uint32_t kTotalReservedUniformBuffers =
        kReservedDriverUniformBindingCount + kReservedDefaultUniformBindingCount;

    const int32_t maxPerStageUniformBuffers = LimitToInt(
        limitsVk.maxPerStageDescriptorUniformBuffers - kTotalReservedPerStageUniformBuffers);
    const int32_t maxCombinedUniformBuffers =
        LimitToInt(limitsVk.maxDescriptorSetUniformBuffers - kTotalReservedUniformBuffers);
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        mNativeCaps.maxShaderUniformBlocks[shaderType] = maxPerStageUniformBuffers;
    }
    mNativeCaps.maxCombinedUniformBlocks = maxCombinedUniformBuffers;

    mNativeCaps.maxUniformBufferBindings = maxCombinedUniformBuffers;
    mNativeCaps.maxUniformBlockSize      = maxUniformBlockSize;
    mNativeCaps.uniformBufferOffsetAlignment =
        static_cast<GLint>(limitsVk.minUniformBufferOffsetAlignment);

    // Note that Vulkan currently implements textures as combined image+samplers, so the limit is
    // the minimum of supported samplers and sampled images.
    const uint32_t maxPerStageTextures = std::min(limitsVk.maxPerStageDescriptorSamplers,
                                                  limitsVk.maxPerStageDescriptorSampledImages);
    const uint32_t maxCombinedTextures =
        std::min(limitsVk.maxDescriptorSetSamplers, limitsVk.maxDescriptorSetSampledImages);
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        mNativeCaps.maxShaderTextureImageUnits[shaderType] = LimitToInt(maxPerStageTextures);
    }
    mNativeCaps.maxCombinedTextureImageUnits = LimitToInt(maxCombinedTextures);

    uint32_t maxPerStageStorageBuffers    = limitsVk.maxPerStageDescriptorStorageBuffers;
    uint32_t maxVertexStageStorageBuffers = maxPerStageStorageBuffers;
    uint32_t maxCombinedStorageBuffers    = limitsVk.maxDescriptorSetStorageBuffers;

    // A number of storage buffer slots are used in the vertex shader to emulate transform feedback.
    // Note that Vulkan requires maxPerStageDescriptorStorageBuffers to be at least 4 (i.e. the same
    // as gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS).
    // TODO(syoussefi): This should be conditioned to transform feedback extension not being
    // present.  http://anglebug.com/3206.
    // TODO(syoussefi): If geometry shader is supported, emulation will be done at that stage, and
    // so the reserved storage buffers should be accounted in that stage.  http://anglebug.com/3606
    static_assert(
        gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS == 4,
        "Limit to ES2.0 if supported SSBO count < supporting transform feedback buffer count");
    if (mPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics)
    {
        ASSERT(maxVertexStageStorageBuffers >= gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS);
        maxVertexStageStorageBuffers -= gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS;
        maxCombinedStorageBuffers -= gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS;

        // Cap the per-stage limit of the other stages to the combined limit, in case the combined
        // limit is now lower than that.
        maxPerStageStorageBuffers = std::min(maxPerStageStorageBuffers, maxCombinedStorageBuffers);
    }

    // Reserve up to IMPLEMENTATION_MAX_ATOMIC_COUNTER_BUFFERS storage buffers in the fragment and
    // compute stages for atomic counters.  This is only possible if the number of per-stage storage
    // buffers is greater than 4, which is the required GLES minimum for compute.
    //
    // For each stage, we'll either not support atomic counter buffers, or support exactly
    // IMPLEMENTATION_MAX_ATOMIC_COUNTER_BUFFERS.  This is due to restrictions in the shader
    // translator where we can't know how many atomic counter buffers we would really need after
    // linking so we can't create a packed buffer array.
    //
    // For the vertex stage, we could support atomic counters without storage buffers, but that's
    // likely not very useful, so we use the same limit (4 + MAX_ATOMIC_COUNTER_BUFFERS) for the
    // vertex stage to determine if we would want to add support for atomic counter buffers.
    constexpr uint32_t kMinimumStorageBuffersForAtomicCounterBufferSupport =
        gl::limits::kMinimumComputeStorageBuffers + gl::IMPLEMENTATION_MAX_ATOMIC_COUNTER_BUFFERS;
    uint32_t maxVertexStageAtomicCounterBuffers = 0;
    uint32_t maxPerStageAtomicCounterBuffers    = 0;
    uint32_t maxCombinedAtomicCounterBuffers    = 0;

    if (maxPerStageStorageBuffers >= kMinimumStorageBuffersForAtomicCounterBufferSupport)
    {
        maxPerStageAtomicCounterBuffers = gl::IMPLEMENTATION_MAX_ATOMIC_COUNTER_BUFFERS;
        maxCombinedAtomicCounterBuffers = gl::IMPLEMENTATION_MAX_ATOMIC_COUNTER_BUFFERS;
    }

    if (maxVertexStageStorageBuffers >= kMinimumStorageBuffersForAtomicCounterBufferSupport)
    {
        maxVertexStageAtomicCounterBuffers = gl::IMPLEMENTATION_MAX_ATOMIC_COUNTER_BUFFERS;
    }

    maxVertexStageStorageBuffers -= maxVertexStageAtomicCounterBuffers;
    maxPerStageStorageBuffers -= maxPerStageAtomicCounterBuffers;
    maxCombinedStorageBuffers -= maxCombinedAtomicCounterBuffers;

    mNativeCaps.maxShaderStorageBlocks[gl::ShaderType::Vertex] =
        mPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics
            ? LimitToInt(maxVertexStageStorageBuffers)
            : 0;
    mNativeCaps.maxShaderStorageBlocks[gl::ShaderType::Fragment] =
        mPhysicalDeviceFeatures.fragmentStoresAndAtomics ? LimitToInt(maxPerStageStorageBuffers)
                                                         : 0;
    mNativeCaps.maxShaderStorageBlocks[gl::ShaderType::Compute] =
        LimitToInt(maxPerStageStorageBuffers);
    mNativeCaps.maxCombinedShaderStorageBlocks = LimitToInt(maxCombinedStorageBuffers);

    mNativeCaps.maxShaderStorageBufferBindings = LimitToInt(maxCombinedStorageBuffers);
    mNativeCaps.maxShaderStorageBlockSize      = limitsVk.maxStorageBufferRange;
    mNativeCaps.shaderStorageBufferOffsetAlignment =
        LimitToInt(static_cast<uint32_t>(limitsVk.minStorageBufferOffsetAlignment));

    mNativeCaps.maxShaderAtomicCounterBuffers[gl::ShaderType::Vertex] =
        mPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics
            ? LimitToInt(maxVertexStageAtomicCounterBuffers)
            : 0;
    mNativeCaps.maxShaderAtomicCounterBuffers[gl::ShaderType::Fragment] =
        mPhysicalDeviceFeatures.fragmentStoresAndAtomics
            ? LimitToInt(maxPerStageAtomicCounterBuffers)
            : 0;
    mNativeCaps.maxShaderAtomicCounterBuffers[gl::ShaderType::Compute] =
        LimitToInt(maxPerStageAtomicCounterBuffers);
    mNativeCaps.maxCombinedAtomicCounterBuffers = LimitToInt(maxCombinedAtomicCounterBuffers);

    mNativeCaps.maxAtomicCounterBufferBindings = LimitToInt(maxCombinedAtomicCounterBuffers);
    // Emulated as storage buffers, atomic counter buffers have the same size limit.  However, the
    // limit is a signed integer and values above int max will end up as a negative size.
    mNativeCaps.maxAtomicCounterBufferSize = LimitToInt(limitsVk.maxStorageBufferRange);

    // There is no particular limit to how many atomic counters there can be, other than the size of
    // a storage buffer.  We nevertheless limit this to something sane (4096 arbitrarily).
    const int32_t maxAtomicCounters =
        std::min<int32_t>(4096, limitsVk.maxStorageBufferRange / sizeof(uint32_t));
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        mNativeCaps.maxShaderAtomicCounters[shaderType] = maxAtomicCounters;
    }
    mNativeCaps.maxCombinedAtomicCounters = maxAtomicCounters;

    // GL Images correspond to Vulkan Storage Images.
    const int32_t maxPerStageImages = LimitToInt(limitsVk.maxPerStageDescriptorStorageImages);
    const int32_t maxCombinedImages = LimitToInt(limitsVk.maxDescriptorSetStorageImages);

    mNativeCaps.maxShaderImageUniforms[gl::ShaderType::Vertex] =
        mPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics ? maxPerStageImages : 0;
    mNativeCaps.maxShaderImageUniforms[gl::ShaderType::Fragment] =
        mPhysicalDeviceFeatures.fragmentStoresAndAtomics ? maxPerStageImages : 0;
    mNativeCaps.maxShaderImageUniforms[gl::ShaderType::Geometry] =
        mPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics ? maxPerStageImages : 0;
    mNativeCaps.maxShaderImageUniforms[gl::ShaderType::Compute] = maxPerStageImages;

    mNativeCaps.maxCombinedImageUniforms = maxCombinedImages;
    mNativeCaps.maxImageUnits            = maxCombinedImages;

    mNativeCaps.minProgramTexelOffset         = limitsVk.minTexelOffset;
    mNativeCaps.maxProgramTexelOffset         = limitsVk.maxTexelOffset;
    mNativeCaps.minProgramTextureGatherOffset = limitsVk.minTexelGatherOffset;
    mNativeCaps.maxProgramTextureGatherOffset = limitsVk.maxTexelGatherOffset;

    // There is no additional limit to the combined number of components.  We can have up to a
    // maximum number of uniform buffers, each having the maximum number of components.  Note that
    // this limit includes both components in and out of uniform buffers.
    const uint32_t maxCombinedUniformComponents =
        (maxPerStageUniformBuffers + kReservedPerStageDefaultUniformBindingCount) *
        maxUniformComponents;
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        mNativeCaps.maxCombinedShaderUniformComponents[shaderType] = maxCombinedUniformComponents;
    }

    // Total number of resources available to the user are as many as Vulkan allows minus everything
    // that ANGLE uses internally.  That is, one dynamic uniform buffer used per stage for default
    // uniforms and a single dynamic uniform buffer for driver uniforms.  Additionally, Vulkan uses
    // up to IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS + 1 buffers for transform feedback (Note:
    // +1 is for the "counter" buffer of transform feedback, which will be necessary for transform
    // feedback extension and ES3.2 transform feedback emulation, but is not yet present).
    constexpr uint32_t kReservedPerStageUniformBufferCount = 1;
    constexpr uint32_t kReservedPerStageBindingCount =
        kReservedDriverUniformBindingCount + kReservedPerStageUniformBufferCount +
        gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS + 1;

    // Note: maxPerStageResources is required to be at least the sum of per stage UBOs, SSBOs etc
    // which total a minimum of 44 resources, so no underflow is possible here.  Limit the total
    // number of resources reported by Vulkan to 2 billion though to avoid seeing negative numbers
    // in applications that take the value as signed int (including dEQP).
    const uint32_t maxPerStageResources = limitsVk.maxPerStageResources;
    mNativeCaps.maxCombinedShaderOutputResources =
        LimitToInt(maxPerStageResources - kReservedPerStageBindingCount);

    // The max vertex output components should not include gl_Position.
    // The gles2.0 section 2.10 states that "gl_Position is not a varying variable and does
    // not count against this limit.", but the Vulkan spec has no such mention in its Built-in
    // vars section. It is implicit that we need to actually reserve it for Vulkan in that case.
    GLint reservedVaryingVectorCount = 1;

    // reserve 1 extra for ANGLEPosition when GLLineRasterization is enabled
    constexpr GLint kRservedVaryingForGLLineRasterization = 1;
    // reserve 2 extra for builtin varables when feedback is enabled
    // possible capturable out varable: gl_Position, gl_PointSize
    // https://www.khronos.org/registry/OpenGL/specs/es/3.1/GLSL_ES_Specification_3.10.withchanges.pdf
    // page 105
    constexpr GLint kReservedVaryingForTransformFeedbackExtension = 2;

    if (getFeatures().basicGLLineRasterization.enabled)
    {
        reservedVaryingVectorCount += kRservedVaryingForGLLineRasterization;
    }
    if (getFeatures().supportsTransformFeedbackExtension.enabled)
    {
        reservedVaryingVectorCount += kReservedVaryingForTransformFeedbackExtension;
    }

    const GLint maxVaryingCount =
        std::min(limitsVk.maxVertexOutputComponents, limitsVk.maxFragmentInputComponents);
    mNativeCaps.maxVaryingVectors =
        LimitToInt((maxVaryingCount / kComponentsPerVector) - reservedVaryingVectorCount);
    mNativeCaps.maxVertexOutputComponents = LimitToInt(limitsVk.maxVertexOutputComponents);

    mNativeCaps.maxTransformFeedbackInterleavedComponents =
        gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS;
    mNativeCaps.maxTransformFeedbackSeparateAttributes =
        gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS;
    mNativeCaps.maxTransformFeedbackSeparateComponents =
        gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS;

    mNativeCaps.minProgramTexelOffset = limitsVk.minTexelOffset;
    mNativeCaps.maxProgramTexelOffset = LimitToInt(limitsVk.maxTexelOffset);

    const uint32_t sampleCounts =
        limitsVk.framebufferColorSampleCounts & limitsVk.framebufferDepthSampleCounts &
        limitsVk.framebufferStencilSampleCounts & vk_gl::kSupportedSampleCounts;

    mNativeCaps.maxSamples            = LimitToInt(vk_gl::GetMaxSampleCount(sampleCounts));
    mNativeCaps.maxFramebufferSamples = mNativeCaps.maxSamples;

    mNativeCaps.subPixelBits = limitsVk.subPixelPrecisionBits;

    // Enable Program Binary extension.
    mNativeExtensions.getProgramBinaryOES = true;
    mNativeCaps.programBinaryFormats.push_back(GL_PROGRAM_BINARY_ANGLE);

    // Enable GL_NV_pixel_buffer_object extension.
    mNativeExtensions.pixelBufferObjectNV = true;

    // Enable GL_NV_fence extension.
    mNativeExtensions.fenceNV = true;

    // Geometry shader is optional.
    if (mPhysicalDeviceFeatures.geometryShader)
    {
        // TODO : Remove below comment when http://anglebug.com/3571 will be completed
        // mNativeExtensions.geometryShader = true;
        mNativeCaps.maxFramebufferLayers = LimitToInt(limitsVk.maxFramebufferLayers);
        mNativeCaps.layerProvokingVertex = GL_LAST_VERTEX_CONVENTION_EXT;

        mNativeCaps.maxGeometryInputComponents  = LimitToInt(limitsVk.maxGeometryInputComponents);
        mNativeCaps.maxGeometryOutputComponents = LimitToInt(limitsVk.maxGeometryOutputComponents);
        mNativeCaps.maxGeometryOutputVertices   = LimitToInt(limitsVk.maxGeometryOutputVertices);
        mNativeCaps.maxGeometryTotalOutputComponents =
            LimitToInt(limitsVk.maxGeometryTotalOutputComponents);
        mNativeCaps.maxShaderStorageBlocks[gl::ShaderType::Geometry] =
            mNativeCaps.maxCombinedShaderOutputResources;
        mNativeCaps.maxShaderAtomicCounterBuffers[gl::ShaderType::Geometry] =
            maxCombinedAtomicCounterBuffers;
        mNativeCaps.maxGeometryShaderInvocations =
            LimitToInt(limitsVk.maxGeometryShaderInvocations);
    }

    // GL_APPLE_clip_distance/GL_EXT_clip_cull_distance
    if (mPhysicalDeviceFeatures.shaderClipDistance && limitsVk.maxClipDistances >= 8)
    {
        mNativeExtensions.clipDistanceAPPLE = true;
        mNativeCaps.maxClipDistances =
            std::min<GLuint>(limitsVk.maxClipDistances, gl::IMPLEMENTATION_MAX_CLIP_DISTANCES);
    }
}

namespace vk
{

bool CanSupportGPUShader5EXT(const VkPhysicalDeviceFeatures &features)
{
    // We use the following Vulkan features to implement EXT_gpu_shader5:
    // - shaderImageGatherExtended: textureGatherOffset with non-constant offset and
    //   textureGatherOffsets family of functions.
    // - shaderSampledImageArrayDynamicIndexing and shaderUniformBufferArrayDynamicIndexing:
    //   dynamically uniform indices for samplers and uniform buffers.
    // - shaderStorageBufferArrayDynamicIndexing: While EXT_gpu_shader5 doesn't require dynamically
    //   uniform indices on storage buffers, we need it as we emulate atomic counter buffers with
    //   storage buffers (and atomic counter buffers *can* be indexed in that way).
    return features.shaderImageGatherExtended && features.shaderSampledImageArrayDynamicIndexing &&
           features.shaderUniformBufferArrayDynamicIndexing &&
           features.shaderStorageBufferArrayDynamicIndexing;
}

}  // namespace vk

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

    // ES3 features are required to emulate ES1
    EGLint es1Support = (maxSupportedESVersion.major >= 3 ? EGL_OPENGL_ES_BIT : 0);
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
    config.configCaveat       = GetConfigCaveat(colorFormat.internalFormat);
    config.conformant         = es1Support | es2Support | es3Support;
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
    config.nativeVisualID     = static_cast<EGLint>(GetNativeVisualID(colorFormat));
    config.nativeVisualType   = EGL_NONE;
    config.renderableType     = es1Support | es2Support | es3Support;
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
                               DisplayVk *display)
{
    ASSERT(colorFormatsCount > 0);
    ASSERT(display != nullptr);

    gl::SupportedSampleSet colorSampleCounts;
    gl::SupportedSampleSet depthStencilSampleCounts;
    gl::SupportedSampleSet sampleCounts;

    const VkPhysicalDeviceLimits &limits =
        display->getRenderer()->getPhysicalDeviceProperties().limits;
    const uint32_t depthStencilSampleCountsLimit = limits.framebufferDepthSampleCounts &
                                                   limits.framebufferStencilSampleCounts &
                                                   vk_gl::kSupportedSampleCounts;

    vk_gl::AddSampleCounts(limits.framebufferColorSampleCounts & vk_gl::kSupportedSampleCounts,
                           &colorSampleCounts);
    vk_gl::AddSampleCounts(depthStencilSampleCountsLimit, &depthStencilSampleCounts);

    // Always support 0 samples
    colorSampleCounts.insert(0);
    depthStencilSampleCounts.insert(0);

    std::set_intersection(colorSampleCounts.begin(), colorSampleCounts.end(),
                          depthStencilSampleCounts.begin(), depthStencilSampleCounts.end(),
                          std::inserter(sampleCounts, sampleCounts.begin()));

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

            const gl::SupportedSampleSet *configSampleCounts = &sampleCounts;
            // If there is no depth/stencil buffer, use the color samples set.
            if (depthStencilFormats[depthStencilFormatIdx] == GL_NONE)
            {
                configSampleCounts = &colorSampleCounts;
            }
            // If there is no color buffer, use the depth/stencil samples set.
            else if (colorFormats[colorFormatIdx] == GL_NONE)
            {
                configSampleCounts = &depthStencilSampleCounts;
            }

            for (EGLint sampleCount : *configSampleCounts)
            {
                egl::Config config = GenerateDefaultConfig(display->getRenderer(), colorFormatInfo,
                                                           depthStencilFormatInfo, sampleCount);
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
