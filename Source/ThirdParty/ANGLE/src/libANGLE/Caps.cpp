//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "libANGLE/Caps.h"
#include "common/debug.h"
#include "common/angleutils.h"

#include "angle_gl.h"

#include <algorithm>
#include <sstream>

static void InsertExtensionString(const std::string &extension, bool supported, std::vector<std::string> *extensionVector)
{
    if (supported)
    {
        extensionVector->push_back(extension);
    }
}

namespace gl
{

TextureCaps::TextureCaps()
    : texturable(false),
      filterable(false),
      renderable(false),
      sampleCounts()
{
}

GLuint TextureCaps::getMaxSamples() const
{
    return !sampleCounts.empty() ? *sampleCounts.rbegin() : 0;
}

GLuint TextureCaps::getNearestSamples(GLuint requestedSamples) const
{
    if (requestedSamples == 0)
    {
        return 0;
    }

    for (SupportedSampleSet::const_iterator i = sampleCounts.begin(); i != sampleCounts.end(); i++)
    {
        GLuint samples = *i;
        if (samples >= requestedSamples)
        {
            return samples;
        }
    }

    return 0;
}

void TextureCapsMap::insert(GLenum internalFormat, const TextureCaps &caps)
{
    mCapsMap[internalFormat] = caps;
}

void TextureCapsMap::remove(GLenum internalFormat)
{
    InternalFormatToCapsMap::iterator i = mCapsMap.find(internalFormat);
    if (i != mCapsMap.end())
    {
        mCapsMap.erase(i);
    }
}

void TextureCapsMap::clear()
{
    mCapsMap.clear();
}

const TextureCaps &TextureCapsMap::get(GLenum internalFormat) const
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
    static TextureCaps defaultUnsupportedTexture;
#pragma clang diagnostic pop
    InternalFormatToCapsMap::const_iterator iter = mCapsMap.find(internalFormat);
    return (iter != mCapsMap.end()) ? iter->second : defaultUnsupportedTexture;
}

TextureCapsMap::const_iterator TextureCapsMap::begin() const
{
    return mCapsMap.begin();
}

TextureCapsMap::const_iterator TextureCapsMap::end() const
{
    return mCapsMap.end();
}

size_t TextureCapsMap::size() const
{
    return mCapsMap.size();
}

Extensions::Extensions()
    : elementIndexUint(false),
      packedDepthStencil(false),
      getProgramBinary(false),
      rgb8rgba8(false),
      textureFormatBGRA8888(false),
      readFormatBGRA(false),
      pixelBufferObject(false),
      mapBuffer(false),
      mapBufferRange(false),
      colorBufferHalfFloat(false),
      textureHalfFloat(false),
      textureHalfFloatLinear(false),
      textureFloat(false),
      textureFloatLinear(false),
      textureRG(false),
      textureCompressionDXT1(false),
      textureCompressionDXT3(false),
      textureCompressionDXT5(false),
      textureCompressionASTCHDR(false),
      textureCompressionASTCLDR(false),
      compressedETC1RGB8Texture(false),
      depthTextures(false),
      depth32(false),
      textureStorage(false),
      textureNPOT(false),
      drawBuffers(false),
      textureFilterAnisotropic(false),
      maxTextureAnisotropy(false),
      occlusionQueryBoolean(false),
      fence(false),
      timerQuery(false),
      disjointTimerQuery(false),
      queryCounterBitsTimeElapsed(0),
      queryCounterBitsTimestamp(0),
      robustness(false),
      blendMinMax(false),
      framebufferBlit(false),
      framebufferMultisample(false),
      instancedArrays(false),
      packReverseRowOrder(false),
      standardDerivatives(false),
      shaderTextureLOD(false),
      shaderFramebufferFetch(false),
      ARMshaderFramebufferFetch(false),
      NVshaderFramebufferFetch(false),
      fragDepth(false),
      textureUsage(false),
      translatedShaderSource(false),
      fboRenderMipmap(false),
      discardFramebuffer(false),
      debugMarker(false),
      eglImage(false),
      eglImageExternal(false),
      eglImageExternalEssl3(false),
      eglStreamConsumerExternal(false),
      unpackSubimage(false),
      packSubimage(false),
      vertexArrayObject(false),
      debug(false),
      maxDebugMessageLength(0),
      maxDebugLoggedMessages(0),
      maxDebugGroupStackDepth(0),
      maxLabelLength(0),
      noError(false),
      lossyETCDecode(false),
      bindUniformLocation(false),
      syncQuery(false),
      copyTexture(false),
      copyCompressedTexture(false),
      webglCompatibility(false),
      bindGeneratesResource(false),
      robustClientMemory(false),
      colorBufferFloat(false),
      multisampleCompatibility(false),
      framebufferMixedSamples(false),
      textureNorm16(false),
      pathRendering(false)
{
}

std::vector<std::string> Extensions::getStrings() const
{
    std::vector<std::string> extensionStrings;

    for (const auto &extensionInfo : GetExtensionInfoMap())
    {
        if (this->*(extensionInfo.second.ExtensionsMember))
        {
            extensionStrings.push_back(extensionInfo.first);
        }
    }

    return extensionStrings;
}

Limitations::Limitations()
    : noFrontFacingSupport(false),
      noSampleAlphaToCoverageSupport(false),
      attributeZeroRequiresZeroDivisorInEXT(false),
      noSeparateStencilRefsAndMasks(false),
      shadersRequireIndexedLoopValidation(false),
      noSimultaneousConstantColorAndAlphaBlendFunc(false)
{
}

static bool GetFormatSupport(const TextureCapsMap &textureCaps, const std::vector<GLenum> &requiredFormats,
                             bool requiresTexturing, bool requiresFiltering, bool requiresRendering)
{
    for (size_t i = 0; i < requiredFormats.size(); i++)
    {
        const TextureCaps &cap = textureCaps.get(requiredFormats[i]);

        if (requiresTexturing && !cap.texturable)
        {
            return false;
        }

        if (requiresFiltering && !cap.filterable)
        {
            return false;
        }

        if (requiresRendering && !cap.renderable)
        {
            return false;
        }
    }

    return true;
}

// Check for GL_OES_packed_depth_stencil
static bool DeterminePackedDepthStencilSupport(const TextureCapsMap &textureCaps)
{
    std::vector<GLenum> requiredFormats;
    requiredFormats.push_back(GL_DEPTH24_STENCIL8);

    return GetFormatSupport(textureCaps, requiredFormats, false, false, true);
}

// Checks for GL_OES_rgb8_rgba8 support
static bool DetermineRGB8AndRGBA8TextureSupport(const TextureCapsMap &textureCaps)
{
    std::vector<GLenum> requiredFormats;
    requiredFormats.push_back(GL_RGB8);
    requiredFormats.push_back(GL_RGBA8);

    return GetFormatSupport(textureCaps, requiredFormats, true, true, true);
}

// Checks for GL_EXT_texture_format_BGRA8888 support
static bool DetermineBGRA8TextureSupport(const TextureCapsMap &textureCaps)
{
    std::vector<GLenum> requiredFormats;
    requiredFormats.push_back(GL_BGRA8_EXT);

    return GetFormatSupport(textureCaps, requiredFormats, true, true, true);
}

// Checks for GL_OES_color_buffer_half_float support
static bool DetermineColorBufferHalfFloatSupport(const TextureCapsMap &textureCaps)
{
    std::vector<GLenum> requiredFormats;
    requiredFormats.push_back(GL_RGBA16F);
    requiredFormats.push_back(GL_RGB16F);
    requiredFormats.push_back(GL_RG16F);
    requiredFormats.push_back(GL_R16F);

    return GetFormatSupport(textureCaps, requiredFormats, true, false, true);
}

// Checks for GL_OES_texture_half_float support
static bool DetermineHalfFloatTextureSupport(const TextureCapsMap &textureCaps)
{
    std::vector<GLenum> requiredFormats;
    requiredFormats.push_back(GL_RGB16F);
    requiredFormats.push_back(GL_RGBA16F);

    return GetFormatSupport(textureCaps, requiredFormats, true, false, true);
}

// Checks for GL_OES_texture_half_float_linear support
static bool DetermineHalfFloatTextureFilteringSupport(const TextureCapsMap &textureCaps)
{
    std::vector<GLenum> requiredFormats;
    requiredFormats.push_back(GL_RGB16F);
    requiredFormats.push_back(GL_RGBA16F);

    return DetermineHalfFloatTextureSupport(textureCaps) &&
           GetFormatSupport(textureCaps, requiredFormats, true, true, false);
}

// Checks for GL_OES_texture_float support
static bool DetermineFloatTextureSupport(const TextureCapsMap &textureCaps)
{
    std::vector<GLenum> requiredFormats;
    requiredFormats.push_back(GL_RGB32F);
    requiredFormats.push_back(GL_RGBA32F);

    return GetFormatSupport(textureCaps, requiredFormats, true, false, true);
}

// Checks for GL_OES_texture_float_linear support
static bool DetermineFloatTextureFilteringSupport(const TextureCapsMap &textureCaps)
{
    std::vector<GLenum> requiredFormats;
    requiredFormats.push_back(GL_RGB32F);
    requiredFormats.push_back(GL_RGBA32F);

    return DetermineFloatTextureSupport(textureCaps) &&
           GetFormatSupport(textureCaps, requiredFormats, true, true, false);
}

// Checks for GL_EXT_texture_rg support
static bool DetermineRGTextureSupport(const TextureCapsMap &textureCaps, bool checkHalfFloatFormats, bool checkFloatFormats)
{
    std::vector<GLenum> requiredFormats;
    requiredFormats.push_back(GL_R8);
    requiredFormats.push_back(GL_RG8);
    if (checkHalfFloatFormats)
    {
        requiredFormats.push_back(GL_R16F);
        requiredFormats.push_back(GL_RG16F);
    }
    if (checkFloatFormats)
    {
        requiredFormats.push_back(GL_R32F);
        requiredFormats.push_back(GL_RG32F);
    }

    return GetFormatSupport(textureCaps, requiredFormats, true, true, false);
}

// Check for GL_EXT_texture_compression_dxt1
static bool DetermineDXT1TextureSupport(const TextureCapsMap &textureCaps)
{
    std::vector<GLenum> requiredFormats;
    requiredFormats.push_back(GL_COMPRESSED_RGB_S3TC_DXT1_EXT);
    requiredFormats.push_back(GL_COMPRESSED_RGBA_S3TC_DXT1_EXT);

    return GetFormatSupport(textureCaps, requiredFormats, true, true, false);
}

// Check for GL_ANGLE_texture_compression_dxt3
static bool DetermineDXT3TextureSupport(const TextureCapsMap &textureCaps)
{
    std::vector<GLenum> requiredFormats;
    requiredFormats.push_back(GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE);

    return GetFormatSupport(textureCaps, requiredFormats, true, true, false);
}

// Check for GL_ANGLE_texture_compression_dxt5
static bool DetermineDXT5TextureSupport(const TextureCapsMap &textureCaps)
{
    std::vector<GLenum> requiredFormats;
    requiredFormats.push_back(GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE);

    return GetFormatSupport(textureCaps, requiredFormats, true, true, false);
}

// Check for GL_KHR_texture_compression_astc_hdr and GL_KHR_texture_compression_astc_ldr
static bool DetermineASTCTextureSupport(const TextureCapsMap &textureCaps)
{
    std::vector<GLenum> requiredFormats;
    requiredFormats.push_back(GL_COMPRESSED_RGBA_ASTC_4x4_KHR);
    requiredFormats.push_back(GL_COMPRESSED_RGBA_ASTC_5x4_KHR);
    requiredFormats.push_back(GL_COMPRESSED_RGBA_ASTC_5x5_KHR);
    requiredFormats.push_back(GL_COMPRESSED_RGBA_ASTC_6x5_KHR);
    requiredFormats.push_back(GL_COMPRESSED_RGBA_ASTC_6x6_KHR);
    requiredFormats.push_back(GL_COMPRESSED_RGBA_ASTC_8x5_KHR);
    requiredFormats.push_back(GL_COMPRESSED_RGBA_ASTC_8x6_KHR);
    requiredFormats.push_back(GL_COMPRESSED_RGBA_ASTC_8x8_KHR);
    requiredFormats.push_back(GL_COMPRESSED_RGBA_ASTC_10x5_KHR);
    requiredFormats.push_back(GL_COMPRESSED_RGBA_ASTC_10x6_KHR);
    requiredFormats.push_back(GL_COMPRESSED_RGBA_ASTC_10x8_KHR);
    requiredFormats.push_back(GL_COMPRESSED_RGBA_ASTC_10x10_KHR);
    requiredFormats.push_back(GL_COMPRESSED_RGBA_ASTC_12x10_KHR);
    requiredFormats.push_back(GL_COMPRESSED_RGBA_ASTC_12x12_KHR);
    requiredFormats.push_back(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR);
    requiredFormats.push_back(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR);
    requiredFormats.push_back(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR);
    requiredFormats.push_back(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR);
    requiredFormats.push_back(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR);
    requiredFormats.push_back(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR);
    requiredFormats.push_back(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR);
    requiredFormats.push_back(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR);
    requiredFormats.push_back(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR);
    requiredFormats.push_back(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR);
    requiredFormats.push_back(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR);
    requiredFormats.push_back(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR);
    requiredFormats.push_back(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR);
    requiredFormats.push_back(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR);

    return GetFormatSupport(textureCaps, requiredFormats, true, true, false);
}

// Check for GL_ETC1_RGB8_OES
static bool DetermineETC1RGB8TextureSupport(const TextureCapsMap &textureCaps)
{
    std::vector<GLenum> requiredFormats;
    requiredFormats.push_back(GL_ETC1_RGB8_OES);

    return GetFormatSupport(textureCaps, requiredFormats, true, true, false);
}

// Check for GL_ANGLE_texture_compression_dxt5
static bool DetermineSRGBTextureSupport(const TextureCapsMap &textureCaps)
{
    std::vector<GLenum> requiredFilterFormats;
    requiredFilterFormats.push_back(GL_SRGB8);
    requiredFilterFormats.push_back(GL_SRGB8_ALPHA8);

    std::vector<GLenum> requiredRenderFormats;
    requiredRenderFormats.push_back(GL_SRGB8_ALPHA8);

    return GetFormatSupport(textureCaps, requiredFilterFormats, true, true, false) &&
           GetFormatSupport(textureCaps, requiredRenderFormats, true, false, true);
}

// Check for GL_ANGLE_depth_texture
static bool DetermineDepthTextureSupport(const TextureCapsMap &textureCaps)
{
    std::vector<GLenum> requiredFormats;
    requiredFormats.push_back(GL_DEPTH_COMPONENT16);
    requiredFormats.push_back(GL_DEPTH_COMPONENT32_OES);
    requiredFormats.push_back(GL_DEPTH24_STENCIL8_OES);

    return GetFormatSupport(textureCaps, requiredFormats, true, true, true);
}

// Check for GL_OES_depth32
static bool DetermineDepth32Support(const TextureCapsMap &textureCaps)
{
    std::vector<GLenum> requiredFormats;
    requiredFormats.push_back(GL_DEPTH_COMPONENT32_OES);

    return GetFormatSupport(textureCaps, requiredFormats, false, false, true);
}

// Check for GL_EXT_color_buffer_float
static bool DetermineColorBufferFloatSupport(const TextureCapsMap &textureCaps)
{
    std::vector<GLenum> requiredFormats;
    requiredFormats.push_back(GL_R16F);
    requiredFormats.push_back(GL_RG16F);
    requiredFormats.push_back(GL_RGBA16F);
    requiredFormats.push_back(GL_R32F);
    requiredFormats.push_back(GL_RG32F);
    requiredFormats.push_back(GL_RGBA32F);
    requiredFormats.push_back(GL_R11F_G11F_B10F);

    return GetFormatSupport(textureCaps, requiredFormats, true, false, true);
}

// Check for GL_EXT_texture_norm16
static bool DetermineTextureNorm16Support(const TextureCapsMap &textureCaps)
{
    std::vector<GLenum> requiredFilterFormats;
    requiredFilterFormats.push_back(GL_R16_EXT);
    requiredFilterFormats.push_back(GL_RG16_EXT);
    requiredFilterFormats.push_back(GL_RGB16_EXT);
    requiredFilterFormats.push_back(GL_RGBA16_EXT);
    requiredFilterFormats.push_back(GL_R16_SNORM_EXT);
    requiredFilterFormats.push_back(GL_RG16_SNORM_EXT);
    requiredFilterFormats.push_back(GL_RGB16_SNORM_EXT);
    requiredFilterFormats.push_back(GL_RGBA16_SNORM_EXT);

    std::vector<GLenum> requiredRenderFormats;
    requiredFilterFormats.push_back(GL_R16_EXT);
    requiredFilterFormats.push_back(GL_RG16_EXT);
    requiredFilterFormats.push_back(GL_RGBA16_EXT);

    return GetFormatSupport(textureCaps, requiredFilterFormats, true, true, false) &&
           GetFormatSupport(textureCaps, requiredRenderFormats, true, false, true);
}

void Extensions::setTextureExtensionSupport(const TextureCapsMap &textureCaps)
{
    packedDepthStencil = DeterminePackedDepthStencilSupport(textureCaps);
    rgb8rgba8 = DetermineRGB8AndRGBA8TextureSupport(textureCaps);
    textureFormatBGRA8888 = DetermineBGRA8TextureSupport(textureCaps);
    colorBufferHalfFloat      = DetermineColorBufferHalfFloatSupport(textureCaps);
    textureHalfFloat = DetermineHalfFloatTextureSupport(textureCaps);
    textureHalfFloatLinear = DetermineHalfFloatTextureFilteringSupport(textureCaps);
    textureFloat = DetermineFloatTextureSupport(textureCaps);
    textureFloatLinear = DetermineFloatTextureFilteringSupport(textureCaps);
    textureRG = DetermineRGTextureSupport(textureCaps, textureHalfFloat, textureFloat);
    textureCompressionDXT1 = DetermineDXT1TextureSupport(textureCaps);
    textureCompressionDXT3 = DetermineDXT3TextureSupport(textureCaps);
    textureCompressionDXT5 = DetermineDXT5TextureSupport(textureCaps);
    textureCompressionASTCHDR = DetermineASTCTextureSupport(textureCaps);
    textureCompressionASTCLDR = textureCompressionASTCHDR;
    compressedETC1RGB8Texture = DetermineETC1RGB8TextureSupport(textureCaps);
    sRGB = DetermineSRGBTextureSupport(textureCaps);
    depthTextures = DetermineDepthTextureSupport(textureCaps);
    depth32                   = DetermineDepth32Support(textureCaps);
    colorBufferFloat = DetermineColorBufferFloatSupport(textureCaps);
    textureNorm16             = DetermineTextureNorm16Support(textureCaps);
}

const ExtensionInfoMap &GetExtensionInfoMap()
{
    auto buildExtensionInfoMap = []() {
        auto enableableExtension = [](ExtensionInfo::ExtensionBool member) {
            ExtensionInfo info;
            info.Enableable       = true;
            info.ExtensionsMember = member;
            return info;
        };

        auto esOnlyExtension = [](ExtensionInfo::ExtensionBool member) {
            ExtensionInfo info;
            info.ExtensionsMember = member;
            return info;
        };

        // clang-format off
        ExtensionInfoMap map;
        map["GL_OES_element_index_uint"] = enableableExtension(&Extensions::elementIndexUint);
        map["GL_OES_packed_depth_stencil"] = esOnlyExtension(&Extensions::packedDepthStencil);
        map["GL_OES_get_program_binary"] = esOnlyExtension(&Extensions::getProgramBinary);
        map["GL_OES_rgb8_rgba8"] = esOnlyExtension(&Extensions::rgb8rgba8);
        map["GL_EXT_texture_format_BGRA8888"] = esOnlyExtension(&Extensions::textureFormatBGRA8888);
        map["GL_EXT_read_format_bgra"] = esOnlyExtension(&Extensions::readFormatBGRA);
        map["GL_NV_pixel_buffer_object"] = esOnlyExtension(&Extensions::pixelBufferObject);
        map["GL_OES_mapbuffer"] = esOnlyExtension(&Extensions::mapBuffer);
        map["GL_EXT_map_buffer_range"] = esOnlyExtension(&Extensions::mapBufferRange);
        map["GL_EXT_color_buffer_half_float"] = esOnlyExtension(&Extensions::colorBufferHalfFloat);
        map["GL_OES_texture_half_float"] = esOnlyExtension(&Extensions::textureHalfFloat);
        map["GL_OES_texture_half_float_linear"] = esOnlyExtension(&Extensions::textureHalfFloatLinear);
        map["GL_OES_texture_float"] = esOnlyExtension(&Extensions::textureFloat);
        map["GL_OES_texture_float_linear"] = esOnlyExtension(&Extensions::textureFloatLinear);
        map["GL_EXT_texture_rg"] = esOnlyExtension(&Extensions::textureRG);
        map["GL_EXT_texture_compression_dxt1"] = esOnlyExtension(&Extensions::textureCompressionDXT1);
        map["GL_ANGLE_texture_compression_dxt3"] = esOnlyExtension(&Extensions::textureCompressionDXT3);
        map["GL_ANGLE_texture_compression_dxt5"] = esOnlyExtension(&Extensions::textureCompressionDXT5);
        map["GL_KHR_texture_compression_astc_hdr"] = esOnlyExtension(&Extensions::textureCompressionASTCHDR);
        map["GL_KHR_texture_compression_astc_ldr"] = esOnlyExtension(&Extensions::textureCompressionASTCLDR);
        map["GL_OES_compressed_ETC1_RGB8_texture"] = esOnlyExtension(&Extensions::compressedETC1RGB8Texture);
        map["GL_EXT_sRGB"] = esOnlyExtension(&Extensions::sRGB);
        map["GL_ANGLE_depth_texture"] = esOnlyExtension(&Extensions::depthTextures);
        map["GL_OES_depth32"] = esOnlyExtension(&Extensions::depth32);
        map["GL_EXT_texture_storage"] = esOnlyExtension(&Extensions::textureStorage);
        map["GL_OES_texture_npot"] = esOnlyExtension(&Extensions::textureNPOT);
        map["GL_EXT_draw_buffers"] = esOnlyExtension(&Extensions::drawBuffers);
        map["GL_EXT_texture_filter_anisotropic"] = esOnlyExtension(&Extensions::textureFilterAnisotropic);
        map["GL_EXT_occlusion_query_boolean"] = esOnlyExtension(&Extensions::occlusionQueryBoolean);
        map["GL_NV_fence"] = esOnlyExtension(&Extensions::fence);
        map["GL_ANGLE_timer_query"] = esOnlyExtension(&Extensions::timerQuery);
        map["GL_EXT_disjoint_timer_query"] = esOnlyExtension(&Extensions::disjointTimerQuery);
        map["GL_EXT_robustness"] = esOnlyExtension(&Extensions::robustness);
        map["GL_EXT_blend_minmax"] = esOnlyExtension(&Extensions::blendMinMax);
        map["GL_ANGLE_framebuffer_blit"] = esOnlyExtension(&Extensions::framebufferBlit);
        map["GL_ANGLE_framebuffer_multisample"] = esOnlyExtension(&Extensions::framebufferMultisample);
        map["GL_ANGLE_instanced_arrays"] = esOnlyExtension(&Extensions::instancedArrays);
        map["GL_ANGLE_pack_reverse_row_order"] = esOnlyExtension(&Extensions::packReverseRowOrder);
        map["GL_OES_standard_derivatives"] = esOnlyExtension(&Extensions::standardDerivatives);
        map["GL_EXT_shader_texture_lod"] = esOnlyExtension(&Extensions::shaderTextureLOD);
        map["GL_NV_shader_framebuffer_fetch"] = esOnlyExtension(&Extensions::NVshaderFramebufferFetch);
        map["GL_ARM_shader_framebuffer_fetch"] = esOnlyExtension(&Extensions::ARMshaderFramebufferFetch);
        map["GL_EXT_shader_framebuffer_fetch"] = esOnlyExtension(&Extensions::shaderFramebufferFetch);
        map["GL_EXT_frag_depth"] = esOnlyExtension(&Extensions::fragDepth);
        map["GL_ANGLE_texture_usage"] = esOnlyExtension(&Extensions::textureUsage);
        map["GL_ANGLE_translated_shader_source"] = esOnlyExtension(&Extensions::translatedShaderSource);
        map["GL_OES_fbo_render_mipmap"] = esOnlyExtension(&Extensions::fboRenderMipmap);
        map["GL_EXT_discard_framebuffer"] = esOnlyExtension(&Extensions::discardFramebuffer);
        map["GL_EXT_debug_marker"] = esOnlyExtension(&Extensions::debugMarker);
        map["GL_OES_EGL_image"] = esOnlyExtension(&Extensions::eglImage);
        map["GL_OES_EGL_image_external"] = esOnlyExtension(&Extensions::eglImageExternal);
        map["GL_OES_EGL_image_external_essl3"] = esOnlyExtension(&Extensions::eglImageExternalEssl3);
        map["GL_NV_EGL_stream_consumer_external"] = esOnlyExtension(&Extensions::eglStreamConsumerExternal);
        map["GL_EXT_unpack_subimage"] = esOnlyExtension(&Extensions::unpackSubimage);
        map["GL_NV_pack_subimage"] = esOnlyExtension(&Extensions::packSubimage);
        map["GL_EXT_color_buffer_float"] = esOnlyExtension(&Extensions::colorBufferFloat);
        map["GL_OES_vertex_array_object"] = esOnlyExtension(&Extensions::vertexArrayObject);
        map["GL_KHR_debug"] = esOnlyExtension(&Extensions::debug);
        // TODO(jmadill): Enable this when complete.
        //map["GL_KHR_no_error"] = esOnlyExtension(&Extensions::noError);
        map["GL_ANGLE_lossy_etc_decode"] = esOnlyExtension(&Extensions::lossyETCDecode);
        map["GL_CHROMIUM_bind_uniform_location"] = esOnlyExtension(&Extensions::bindUniformLocation);
        map["GL_CHROMIUM_sync_query"] = esOnlyExtension(&Extensions::syncQuery);
        map["GL_CHROMIUM_copy_texture"] = esOnlyExtension(&Extensions::copyTexture);
        map["GL_CHROMIUM_copy_compressed_texture"] = esOnlyExtension(&Extensions::copyCompressedTexture);
        map["GL_ANGLE_webgl_compatibility"] = esOnlyExtension(&Extensions::webglCompatibility);
        map["GL_CHROMIUM_bind_generates_resource"] = esOnlyExtension(&Extensions::bindGeneratesResource);
        map["GL_ANGLE_robust_client_memory"] = esOnlyExtension(&Extensions::robustClientMemory);
        map["GL_EXT_multisample_compatibility"] = esOnlyExtension(&Extensions::multisampleCompatibility);
        map["GL_CHROMIUM_framebuffer_mixed_samples"] = esOnlyExtension(&Extensions::framebufferMixedSamples);
        map["GL_EXT_texture_norm16"] = esOnlyExtension(&Extensions::textureNorm16);
        map["GL_CHROMIUM_path_rendering"] = esOnlyExtension(&Extensions::pathRendering);
        // clang-format on

        return map;
    };

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
    static const ExtensionInfoMap extensionInfo = buildExtensionInfoMap();
#pragma clang diagnostic pop
    return extensionInfo;
}

TypePrecision::TypePrecision()
{
    range[0] = 0;
    range[1] = 0;
    precision = 0;
}

void TypePrecision::setIEEEFloat()
{
    range[0] = 127;
    range[1] = 127;
    precision = 23;
}

void TypePrecision::setTwosComplementInt(unsigned int bits)
{
    range[0] = GLint(bits) - 1;
    range[1] = GLint(bits) - 2;
    precision = 0;
}

void TypePrecision::setSimulatedInt(unsigned int r)
{
    range[0] = GLint(r);
    range[1] = GLint(r);
    precision = 0;
}

void TypePrecision::get(GLint *returnRange, GLint *returnPrecision) const
{
    returnRange[0] = range[0];
    returnRange[1] = range[1];
    *returnPrecision = precision;
}

Caps::Caps()
    : maxElementIndex(0),
      max3DTextureSize(0),
      max2DTextureSize(0),
      maxArrayTextureLayers(0),
      maxLODBias(0),
      maxCubeMapTextureSize(0),
      maxRenderbufferSize(0),
      minAliasedPointSize(0),
      maxAliasedPointSize(0),
      minAliasedLineWidth(0),
      maxAliasedLineWidth(0),

      // Table 20.40
      maxDrawBuffers(0),
      maxFramebufferWidth(0),
      maxFramebufferHeight(0),
      maxFramebufferSamples(0),
      maxColorAttachments(0),
      maxViewportWidth(0),
      maxViewportHeight(0),
      maxSampleMaskWords(0),
      maxColorTextureSamples(0),
      maxDepthTextureSamples(0),
      maxIntegerSamples(0),
      maxServerWaitTimeout(0),

      // Table 20.41
      maxVertexAttribRelativeOffset(0),
      maxVertexAttribBindings(0),
      maxVertexAttribStride(0),
      maxElementsIndices(0),
      maxElementsVertices(0),

      // Table 20.43
      maxVertexAttributes(0),
      maxVertexUniformComponents(0),
      maxVertexUniformVectors(0),
      maxVertexUniformBlocks(0),
      maxVertexOutputComponents(0),
      maxVertexTextureImageUnits(0),
      maxVertexAtomicCounterBuffers(0),
      maxVertexAtomicCounters(0),
      maxVertexImageUniforms(0),
      maxVertexShaderStorageBlocks(0),

      // Table 20.44
      maxFragmentUniformComponents(0),
      maxFragmentUniformVectors(0),
      maxFragmentUniformBlocks(0),
      maxFragmentInputComponents(0),
      maxTextureImageUnits(0),
      maxFragmentAtomicCounterBuffers(0),
      maxFragmentAtomicCounters(0),
      maxFragmentImageUniforms(0),
      maxFragmentShaderStorageBlocks(0),
      minProgramTextureGatherOffset(0),
      maxProgramTextureGatherOffset(0),
      minProgramTexelOffset(0),
      maxProgramTexelOffset(0),

      // Table 20.45
      maxComputeWorkGroupInvocations(0),
      maxComputeUniformBlocks(0),
      maxComputeTextureImageUnits(0),
      maxComputeSharedMemorySize(0),
      maxComputeUniformComponents(0),
      maxComputeAtomicCounterBuffers(0),
      maxComputeAtomicCounters(0),
      maxComputeImageUniforms(0),
      maxCombinedComputeUniformComponents(0),
      maxComputeShaderStorageBlocks(0),

      // Table 20.46
      maxUniformBufferBindings(0),
      maxUniformBlockSize(0),
      uniformBufferOffsetAlignment(0),
      maxCombinedUniformBlocks(0),
      maxCombinedVertexUniformComponents(0),
      maxCombinedFragmentUniformComponents(0),
      maxVaryingComponents(0),
      maxVaryingVectors(0),
      maxCombinedTextureImageUnits(0),
      maxCombinedShaderOutputResources(0),

      // Table 20.47
      maxUniformLocations(0),
      maxAtomicCounterBufferBindings(0),
      maxAtomicCounterBufferSize(0),
      maxCombinedAtomicCounterBuffers(0),
      maxCombinedAtomicCounters(0),
      maxImageUnits(0),
      maxCombinedImageUniforms(0),
      maxShaderStorageBufferBindings(0),
      maxShaderStorageBlockSize(0),
      maxCombinedShaderStorageBlocks(0),
      shaderStorageBufferOffsetAlignment(0),

      // Table 20.48
      maxTransformFeedbackInterleavedComponents(0),
      maxTransformFeedbackSeparateAttributes(0),
      maxTransformFeedbackSeparateComponents(0),

      // Table 20.49
      maxSamples(0)

{
    for (size_t i = 0; i < 3; ++i)
    {
        maxComputeWorkGroupCount[i] = 0;
        maxComputeWorkGroupSize[i]  = 0;
    }
}

}

namespace egl
{

Caps::Caps()
    : textureNPOT(false)
{
}

DisplayExtensions::DisplayExtensions()
    : createContextRobustness(false),
      d3dShareHandleClientBuffer(false),
      surfaceD3DTexture2DShareHandle(false),
      querySurfacePointer(false),
      windowFixedSize(false),
      keyedMutex(false),
      surfaceOrientation(false),
      postSubBuffer(false),
      createContext(false),
      deviceQuery(false),
      image(false),
      imageBase(false),
      imagePixmap(false),
      glTexture2DImage(false),
      glTextureCubemapImage(false),
      glTexture3DImage(false),
      glRenderbufferImage(false),
      getAllProcAddresses(false),
      flexibleSurfaceCompatibility(false),
      directComposition(false),
      createContextNoError(false),
      stream(false),
      streamConsumerGLTexture(false),
      streamConsumerGLTextureYUV(false),
      streamProducerD3DTextureNV12(false),
      createContextWebGLCompatibility(false),
      createContextBindGeneratesResource(false),
      getSyncValues(false)
{
}

std::vector<std::string> DisplayExtensions::getStrings() const
{
    std::vector<std::string> extensionStrings;

    // clang-format off
    //                   | Extension name                                       | Supported flag                    | Output vector   |
    InsertExtensionString("EGL_EXT_create_context_robustness",                   createContextRobustness,            &extensionStrings);
    InsertExtensionString("EGL_ANGLE_d3d_share_handle_client_buffer",            d3dShareHandleClientBuffer,         &extensionStrings);
    InsertExtensionString("EGL_ANGLE_surface_d3d_texture_2d_share_handle",       surfaceD3DTexture2DShareHandle,     &extensionStrings);
    InsertExtensionString("EGL_ANGLE_query_surface_pointer",                     querySurfacePointer,                &extensionStrings);
    InsertExtensionString("EGL_ANGLE_window_fixed_size",                         windowFixedSize,                    &extensionStrings);
    InsertExtensionString("EGL_ANGLE_keyed_mutex",                               keyedMutex,                         &extensionStrings);
    InsertExtensionString("EGL_ANGLE_surface_orientation",                       surfaceOrientation,                 &extensionStrings);
    InsertExtensionString("EGL_ANGLE_direct_composition",                        directComposition,                  &extensionStrings);
    InsertExtensionString("EGL_NV_post_sub_buffer",                              postSubBuffer,                      &extensionStrings);
    InsertExtensionString("EGL_KHR_create_context",                              createContext,                      &extensionStrings);
    InsertExtensionString("EGL_EXT_device_query",                                deviceQuery,                        &extensionStrings);
    InsertExtensionString("EGL_KHR_image",                                       image,                              &extensionStrings);
    InsertExtensionString("EGL_KHR_image_base",                                  imageBase,                          &extensionStrings);
    InsertExtensionString("EGL_KHR_image_pixmap",                                imagePixmap,                        &extensionStrings);
    InsertExtensionString("EGL_KHR_gl_texture_2D_image",                         glTexture2DImage,                   &extensionStrings);
    InsertExtensionString("EGL_KHR_gl_texture_cubemap_image",                    glTextureCubemapImage,              &extensionStrings);
    InsertExtensionString("EGL_KHR_gl_texture_3D_image",                         glTexture3DImage,                   &extensionStrings);
    InsertExtensionString("EGL_KHR_gl_renderbuffer_image",                       glRenderbufferImage,                &extensionStrings);
    InsertExtensionString("EGL_KHR_get_all_proc_addresses",                      getAllProcAddresses,                &extensionStrings);
    InsertExtensionString("EGL_KHR_stream",                                      stream,                             &extensionStrings);
    InsertExtensionString("EGL_KHR_stream_consumer_gltexture",                   streamConsumerGLTexture,            &extensionStrings);
    InsertExtensionString("EGL_NV_stream_consumer_gltexture_yuv",                streamConsumerGLTextureYUV,         &extensionStrings);
    InsertExtensionString("EGL_ANGLE_flexible_surface_compatibility",            flexibleSurfaceCompatibility,       &extensionStrings);
    InsertExtensionString("EGL_ANGLE_stream_producer_d3d_texture_nv12",          streamProducerD3DTextureNV12,       &extensionStrings);
    InsertExtensionString("EGL_ANGLE_create_context_webgl_compatibility",        createContextWebGLCompatibility,    &extensionStrings);
    InsertExtensionString("EGL_CHROMIUM_create_context_bind_generates_resource", createContextBindGeneratesResource, &extensionStrings);
    InsertExtensionString("EGL_CHROMIUM_sync_control",                           getSyncValues,                      &extensionStrings);
    // TODO(jmadill): Enable this when complete.
    //InsertExtensionString("KHR_create_context_no_error",                       createContextNoError,               &extensionStrings);
    // clang-format on

    return extensionStrings;
}

DeviceExtensions::DeviceExtensions()
    : deviceD3D(false)
{
}

std::vector<std::string> DeviceExtensions::getStrings() const
{
    std::vector<std::string> extensionStrings;

    //                   | Extension name                                 | Supported flag                | Output vector   |
    InsertExtensionString("EGL_ANGLE_device_d3d",                          deviceD3D,                      &extensionStrings);

    return extensionStrings;
}

ClientExtensions::ClientExtensions()
    : clientExtensions(false),
      platformBase(false),
      platformDevice(false),
      platformANGLE(false),
      platformANGLED3D(false),
      platformANGLEOpenGL(false),
      deviceCreation(false),
      deviceCreationD3D11(false),
      x11Visual(false),
      experimentalPresentPath(false),
      clientGetAllProcAddresses(false)
{
}

std::vector<std::string> ClientExtensions::getStrings() const
{
    std::vector<std::string> extensionStrings;

    // clang-format off
    //                   | Extension name                         | Supported flag           | Output vector   |
    InsertExtensionString("EGL_EXT_client_extensions",             clientExtensions,          &extensionStrings);
    InsertExtensionString("EGL_EXT_platform_base",                 platformBase,              &extensionStrings);
    InsertExtensionString("EGL_EXT_platform_device",               platformDevice,            &extensionStrings);
    InsertExtensionString("EGL_ANGLE_platform_angle",              platformANGLE,             &extensionStrings);
    InsertExtensionString("EGL_ANGLE_platform_angle_d3d",          platformANGLED3D,          &extensionStrings);
    InsertExtensionString("EGL_ANGLE_platform_angle_opengl",       platformANGLEOpenGL,       &extensionStrings);
    InsertExtensionString("EGL_ANGLE_platform_angle_null",         platformANGLENULL,         &extensionStrings);
    InsertExtensionString("EGL_ANGLE_device_creation",             deviceCreation,            &extensionStrings);
    InsertExtensionString("EGL_ANGLE_device_creation_d3d11",       deviceCreationD3D11,       &extensionStrings);
    InsertExtensionString("EGL_ANGLE_x11_visual",                  x11Visual,                 &extensionStrings);
    InsertExtensionString("EGL_ANGLE_experimental_present_path",   experimentalPresentPath,   &extensionStrings);
    InsertExtensionString("EGL_KHR_client_get_all_proc_addresses", clientGetAllProcAddresses, &extensionStrings);
    // clang-format on

    return extensionStrings;
}

}
