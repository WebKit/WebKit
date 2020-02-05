//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// mtl_format_utils.mm:
//      Implements Format conversion utilities classes that convert from angle formats
//      to respective MTLPixelFormat and MTLVertexFormat.
//

#include "libANGLE/renderer/metal/mtl_format_utils.h"

#include "common/debug.h"
#include "libANGLE/renderer/Format.h"
#include "libANGLE/renderer/metal/DisplayMtl.h"

namespace rx
{
namespace mtl
{

namespace
{

bool OverrideTextureCaps(const DisplayMtl *display, angle::FormatID formatId, gl::TextureCaps *caps)
{
    // NOTE(hqle): Auto generate this.
    switch (formatId)
    {
        case angle::FormatID::R8G8_UNORM:
        case angle::FormatID::R8G8B8_UNORM:
        case angle::FormatID::R8G8B8_UNORM_SRGB:
        case angle::FormatID::R8G8B8A8_UNORM:
        case angle::FormatID::R8G8B8A8_UNORM_SRGB:
        case angle::FormatID::B8G8R8A8_UNORM:
        case angle::FormatID::B8G8R8A8_UNORM_SRGB:
        // NOTE: even though iOS devices don't support filtering depth textures, we still report as
        // supported here in order for the OES_depth_texture extension to be enabled.
        // During draw call, the filter modes will be converted to nearest.
        case angle::FormatID::D16_UNORM:
        case angle::FormatID::D24_UNORM_S8_UINT:
        case angle::FormatID::D32_FLOAT_S8X24_UINT:
        case angle::FormatID::D32_FLOAT:
        case angle::FormatID::D32_UNORM:
            caps->texturable = caps->filterable = caps->textureAttachment = caps->renderbuffer =
                true;
            return true;
        default:
            // NOTE(hqle): Handle more cases
            return false;
    }
}

void GenerateTextureCapsMap(const FormatTable &formatTable,
                            const DisplayMtl *display,
                            gl::TextureCapsMap *capsMapOut,
                            std::vector<GLenum> *compressedFormatsOut)
{
    auto &textureCapsMap    = *capsMapOut;
    auto &compressedFormats = *compressedFormatsOut;

    compressedFormats.clear();

    // Metal doesn't have programmatical way to determine texture format support.
    // What is available is the online documents from Apple. What we can do here
    // is manually set certain extension flag to true then let angle decide the supported formats.
    //
    // TODO(hqle): The proper way of doing this is creating a detailed "format support table" json
    // file with info parsed from https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf.
    // Then using that json file to generate a table in C++ file.
    gl::Extensions tmpTextureExtensions;

#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
    // https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf
    // Requires depth24Stencil8PixelFormatSupported=YES for these extensions
    bool packedDepthStencil24Support =
        display->getMetalDevice().depth24Stencil8PixelFormatSupported;
    tmpTextureExtensions.packedDepthStencilOES  = true;  // We support this reguardless
    tmpTextureExtensions.colorBufferHalfFloat   = packedDepthStencil24Support;
    tmpTextureExtensions.colorBufferFloat       = packedDepthStencil24Support;
    tmpTextureExtensions.colorBufferFloatRGB    = packedDepthStencil24Support;
    tmpTextureExtensions.colorBufferFloatRGBA   = packedDepthStencil24Support;
    tmpTextureExtensions.textureHalfFloat       = packedDepthStencil24Support;
    tmpTextureExtensions.textureFloatOES        = packedDepthStencil24Support;
    tmpTextureExtensions.textureHalfFloatLinear = packedDepthStencil24Support;
    tmpTextureExtensions.textureFloatLinearOES  = packedDepthStencil24Support;
    tmpTextureExtensions.textureRG              = packedDepthStencil24Support;
    tmpTextureExtensions.textureFormatBGRA8888  = packedDepthStencil24Support;

    tmpTextureExtensions.textureCompressionDXT3 = true;
    tmpTextureExtensions.textureCompressionDXT5 = true;

    // We can only fully support DXT1 without alpha using texture swizzle support from MacOs 10.15
    tmpTextureExtensions.textureCompressionDXT1 = display->getFeatures().hasTextureSwizzle.enabled;

    tmpTextureExtensions.textureCompressionS3TCsRGB = tmpTextureExtensions.textureCompressionDXT1;
#else
    tmpTextureExtensions.packedDepthStencilOES  = true;  // override to D32_FLOAT_S8X24_UINT
    tmpTextureExtensions.colorBufferHalfFloat   = true;
    tmpTextureExtensions.colorBufferFloat       = true;
    tmpTextureExtensions.colorBufferFloatRGB    = true;
    tmpTextureExtensions.colorBufferFloatRGBA   = true;
    tmpTextureExtensions.textureHalfFloat       = true;
    tmpTextureExtensions.textureHalfFloatLinear = true;
    tmpTextureExtensions.textureFloatOES        = true;
    tmpTextureExtensions.textureRG              = true;
    tmpTextureExtensions.textureFormatBGRA8888  = true;
    if ([display->getMetalDevice() supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily1_v1])
    {
        tmpTextureExtensions.compressedETC1RGB8Texture        = true;
        tmpTextureExtensions.compressedETC2RGB8Texture        = true;
        tmpTextureExtensions.compressedETC2sRGB8Texture       = true;
        tmpTextureExtensions.compressedETC2RGBA8Texture       = true;
        tmpTextureExtensions.compressedETC2sRGB8Alpha8Texture = true;
        tmpTextureExtensions.compressedEACR11UnsignedTexture  = true;
        tmpTextureExtensions.compressedEACR11SignedTexture    = true;
        tmpTextureExtensions.compressedEACRG11UnsignedTexture = true;
        tmpTextureExtensions.compressedEACRG11SignedTexture   = true;
        tmpTextureExtensions.compressedTexturePVRTC           = true;
        tmpTextureExtensions.compressedTexturePVRTCsRGB       = true;
    }
#endif
    tmpTextureExtensions.sRGB              = true;
    tmpTextureExtensions.depth32OES        = true;
    tmpTextureExtensions.depth24OES        = true;
    tmpTextureExtensions.rgb8rgba8OES      = true;
    tmpTextureExtensions.textureStorage    = true;
    tmpTextureExtensions.depthTextureOES   = true;
    tmpTextureExtensions.depthTextureANGLE = true;

    auto formatVerifier = [&](const gl::InternalFormat &internalFormatInfo) {
        angle::FormatID angleFormatId =
            angle::Format::InternalFormatToID(internalFormatInfo.sizedInternalFormat);
        const Format &mtlFormat = formatTable.getPixelFormat(angleFormatId);

        if (!mtlFormat.valid())
        {
            return;
        }

        const angle::Format &intendedAngleFormat = mtlFormat.intendedAngleFormat();
        gl::TextureCaps textureCaps;

        const auto &clientVersion = kMaxSupportedGLVersion;

        // First let check whether we can determine programmatically.
        if (!OverrideTextureCaps(display, mtlFormat.intendedFormatId, &textureCaps))
        {
            // Let angle decide based on extensions we enabled above.
            textureCaps = gl::GenerateMinimumTextureCaps(internalFormatInfo.sizedInternalFormat,
                                                         clientVersion, tmpTextureExtensions);
        }

        // NOTE(hqle): Support MSAA.
        textureCaps.sampleCounts.clear();
        textureCaps.sampleCounts.insert(0);
        textureCaps.sampleCounts.insert(1);

        textureCapsMap.set(mtlFormat.intendedFormatId, textureCaps);

        if (intendedAngleFormat.isBlock)
        {
            compressedFormats.push_back(intendedAngleFormat.glInternalFormat);
        }

        // Verify implementation mismatch
        ASSERT(!textureCaps.renderbuffer || mtl::Format::FormatRenderable(mtlFormat.metalFormat));
        ASSERT(!textureCaps.textureAttachment ||
               mtl::Format::FormatRenderable(mtlFormat.metalFormat));
    };

    // Texture caps map.
    const gl::FormatSet &internalFormats = gl::GetAllSizedInternalFormats();
    for (const auto internalFormat : internalFormats)
    {
        const gl::InternalFormat &internalFormatInfo =
            gl::GetSizedInternalFormatInfo(internalFormat);

        formatVerifier(internalFormatInfo);
    }
}

}  // namespace

// FormatBase implementation
const angle::Format &FormatBase::actualAngleFormat() const
{
    return angle::Format::Get(actualFormatId);
}

const angle::Format &FormatBase::intendedAngleFormat() const
{
    return angle::Format::Get(intendedFormatId);
}

// Format implementation
/** static */
bool Format::FormatRenderable(MTLPixelFormat format)
{
    switch (format)
    {
        case MTLPixelFormatR8Unorm:
        case MTLPixelFormatRG8Unorm:
        case MTLPixelFormatR16Float:
        case MTLPixelFormatRG16Float:
        case MTLPixelFormatRGBA16Float:
        case MTLPixelFormatR32Float:
        case MTLPixelFormatRG32Float:
        case MTLPixelFormatRGBA32Float:
        case MTLPixelFormatBGRA8Unorm:
        case MTLPixelFormatBGRA8Unorm_sRGB:
        case MTLPixelFormatRGBA8Unorm:
        case MTLPixelFormatRGBA8Unorm_sRGB:
        case MTLPixelFormatDepth32Float:
        case MTLPixelFormatStencil8:
        case MTLPixelFormatDepth32Float_Stencil8:
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
        case MTLPixelFormatDepth16Unorm:
        case MTLPixelFormatDepth24Unorm_Stencil8:
#else
        case MTLPixelFormatR8Unorm_sRGB:
        case MTLPixelFormatRG8Unorm_sRGB:
        case MTLPixelFormatB5G6R5Unorm:
        case MTLPixelFormatA1BGR5Unorm:
        case MTLPixelFormatABGR4Unorm:
        case MTLPixelFormatBGR5A1Unorm:
#endif
            // NOTE(hqle): we may add more formats support here in future.
            return true;
        default:
            return false;
    }
    return false;
}

/** static */
bool Format::FormatCPUReadable(MTLPixelFormat format)
{
    switch (format)
    {
        case MTLPixelFormatDepth32Float:
        case MTLPixelFormatStencil8:
        case MTLPixelFormatDepth32Float_Stencil8:
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
        case MTLPixelFormatDepth16Unorm:
        case MTLPixelFormatDepth24Unorm_Stencil8:
#endif
            // NOTE(hqle): we may add more formats support here in future.
            return false;
        default:
            return true;
    }
}

const gl::InternalFormat &Format::intendedInternalFormat() const
{
    return gl::GetSizedInternalFormatInfo(intendedAngleFormat().glInternalFormat);
}

// FormatTable implementation
angle::Result FormatTable::initialize(const DisplayMtl *display)
{
    for (size_t i = 0; i < angle::kNumANGLEFormats; ++i)
    {
        const auto formatId = static_cast<angle::FormatID>(i);

        mPixelFormatTable[i].init(display, formatId);
        mVertexFormatTables[0][i].init(formatId, false);
        mVertexFormatTables[1][i].init(formatId, true);
    }

    return angle::Result::Continue;
}

void FormatTable::generateTextureCaps(const DisplayMtl *display,
                                      gl::TextureCapsMap *capsMapOut,
                                      std::vector<GLenum> *compressedFormatsOut) const
{
    GenerateTextureCapsMap(*this, display, capsMapOut, compressedFormatsOut);
}

const Format &FormatTable::getPixelFormat(angle::FormatID angleFormatId) const
{
    return mPixelFormatTable[static_cast<size_t>(angleFormatId)];
}
const VertexFormat &FormatTable::getVertexFormat(angle::FormatID angleFormatId,
                                                 bool tightlyPacked) const
{
    auto tableIdx = tightlyPacked ? 1 : 0;
    return mVertexFormatTables[tableIdx][static_cast<size_t>(angleFormatId)];
}

}  // namespace mtl
}  // namespace rx
