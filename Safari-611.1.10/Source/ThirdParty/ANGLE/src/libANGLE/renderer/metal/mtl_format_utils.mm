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
#include "libANGLE/renderer/load_functions_table.h"
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
                            std::vector<GLenum> *compressedFormatsOut,
                            uint32_t *maxSamplesOut)
{
    auto &textureCapsMap    = *capsMapOut;
    auto &compressedFormats = *compressedFormatsOut;

    compressedFormats.clear();

    auto formatVerifier = [&](const gl::InternalFormat &internalFormatInfo) {
        angle::FormatID angleFormatId =
            angle::Format::InternalFormatToID(internalFormatInfo.sizedInternalFormat);
        const Format &mtlFormat = formatTable.getPixelFormat(angleFormatId);
        if (!mtlFormat.valid())
        {
            return;
        }
        const FormatCaps &formatCaps = mtlFormat.getCaps();

        const angle::Format &intendedAngleFormat = mtlFormat.intendedAngleFormat();
        gl::TextureCaps textureCaps;

        // First let check whether we can override certain special cases.
        if (!OverrideTextureCaps(display, mtlFormat.intendedFormatId, &textureCaps))
        {
            // Fill the texture caps using pixel format's caps
            textureCaps.filterable = mtlFormat.getCaps().filterable;
            textureCaps.renderbuffer =
                mtlFormat.getCaps().colorRenderable || mtlFormat.getCaps().depthRenderable;
            textureCaps.texturable        = true;
            textureCaps.textureAttachment = textureCaps.renderbuffer;
            textureCaps.blendable         = mtlFormat.getCaps().blendable;
        }

        if (formatCaps.multisample)
        {
            constexpr uint32_t sampleCounts[] = {2, 4, 8};
            for (auto sampleCount : sampleCounts)
            {
                if ([display->getMetalDevice() supportsTextureSampleCount:sampleCount])
                {
                    textureCaps.sampleCounts.insert(sampleCount);
                    *maxSamplesOut = std::max(*maxSamplesOut, sampleCount);
                }
            }
        }

        textureCapsMap.set(mtlFormat.intendedFormatId, textureCaps);

        if (intendedAngleFormat.isBlock)
        {
            compressedFormats.push_back(intendedAngleFormat.glInternalFormat);
        }
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
const gl::InternalFormat &Format::intendedInternalFormat() const
{
    return gl::GetSizedInternalFormatInfo(intendedAngleFormat().glInternalFormat);
}

const gl::InternalFormat &Format::actualInternalFormat() const
{
    return gl::GetSizedInternalFormatInfo(actualAngleFormat().glInternalFormat);
}

bool Format::needConversion(angle::FormatID srcFormatId) const
{
    if ((srcFormatId == angle::FormatID::BC1_RGB_UNORM_BLOCK &&
         actualFormatId == angle::FormatID::BC1_RGBA_UNORM_BLOCK) ||
        (srcFormatId == angle::FormatID::BC1_RGB_UNORM_SRGB_BLOCK &&
         actualFormatId == angle::FormatID::BC1_RGBA_UNORM_SRGB_BLOCK))
    {
        // When texture swizzling is available, DXT1 RGB format will be swizzled with RGB1.
        // WebGL allows unswizzled mapping when swizzling is not available. No need to convert.
        return false;
    }
    return srcFormatId != actualFormatId;
}

bool Format::isPVRTC() const
{
    switch (metalFormat)
    {
#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
        case MTLPixelFormatPVRTC_RGB_2BPP:
        case MTLPixelFormatPVRTC_RGB_2BPP_sRGB:
        case MTLPixelFormatPVRTC_RGB_4BPP:
        case MTLPixelFormatPVRTC_RGB_4BPP_sRGB:
        case MTLPixelFormatPVRTC_RGBA_2BPP:
        case MTLPixelFormatPVRTC_RGBA_2BPP_sRGB:
        case MTLPixelFormatPVRTC_RGBA_4BPP:
        case MTLPixelFormatPVRTC_RGBA_4BPP_sRGB:
            return true;
#endif
        default:
            return false;
    }
}

// FormatTable implementation
angle::Result FormatTable::initialize(const DisplayMtl *display)
{
    mMaxSamples = 0;

    // Initialize native format caps
    initNativeFormatCaps(display);

    for (size_t i = 0; i < angle::kNumANGLEFormats; ++i)
    {
        const auto formatId = static_cast<angle::FormatID>(i);

        mPixelFormatTable[i].init(display, formatId);
        mPixelFormatTable[i].caps = &mNativePixelFormatCapsTable[mPixelFormatTable[i].metalFormat];

        if (mPixelFormatTable[i].actualFormatId != mPixelFormatTable[i].intendedFormatId)
        {
            mPixelFormatTable[i].textureLoadFunctions = angle::GetLoadFunctionsMap(
                mPixelFormatTable[i].intendedAngleFormat().glInternalFormat,
                mPixelFormatTable[i].actualFormatId);
        }

        mVertexFormatTables[0][i].init(formatId, false);
        mVertexFormatTables[1][i].init(formatId, true);
    }

    // NOTE(hqle): Work-around AMD's issue that D24S8 format sometimes returns zero during sampling:
    if (display->getRendererDescription().find("AMD") != std::string::npos)
    {
        // Fallback to D32_FLOAT_S8X24_UINT.
        Format &format =
            mPixelFormatTable[static_cast<uint32_t>(angle::FormatID::D24_UNORM_S8_UINT)];
        format.actualFormatId       = angle::FormatID::D32_FLOAT_S8X24_UINT;
        format.metalFormat          = MTLPixelFormatDepth32Float_Stencil8;
        format.initFunction         = nullptr;
        format.textureLoadFunctions = nullptr;
        format.caps = &mNativePixelFormatCapsTable[MTLPixelFormatDepth32Float_Stencil8];
    }

    return angle::Result::Continue;
}

void FormatTable::generateTextureCaps(const DisplayMtl *display,
                                      gl::TextureCapsMap *capsMapOut,
                                      std::vector<GLenum> *compressedFormatsOut)
{
    GenerateTextureCapsMap(*this, display, capsMapOut, compressedFormatsOut, &mMaxSamples);
}

const Format &FormatTable::getPixelFormat(angle::FormatID angleFormatId) const
{
    return mPixelFormatTable[static_cast<size_t>(angleFormatId)];
}
const FormatCaps &FormatTable::getNativeFormatCaps(MTLPixelFormat mtlFormat) const
{
    ASSERT(mNativePixelFormatCapsTable.count(mtlFormat));
    return mNativePixelFormatCapsTable.at(mtlFormat);
}
const VertexFormat &FormatTable::getVertexFormat(angle::FormatID angleFormatId,
                                                 bool tightlyPacked) const
{
    auto tableIdx = tightlyPacked ? 1 : 0;
    return mVertexFormatTables[tableIdx][static_cast<size_t>(angleFormatId)];
}

void FormatTable::setFormatCaps(MTLPixelFormat formatId,
                                bool filterable,
                                bool writable,
                                bool blendable,
                                bool multisample,
                                bool resolve,
                                bool colorRenderable)
{
    setFormatCaps(formatId, filterable, writable, blendable, multisample, resolve, colorRenderable,
                  false, 0);
}
void FormatTable::setFormatCaps(MTLPixelFormat formatId,
                                bool filterable,
                                bool writable,
                                bool blendable,
                                bool multisample,
                                bool resolve,
                                bool colorRenderable,
                                NSUInteger pixelBytes,
                                NSUInteger channels)
{
    setFormatCaps(formatId, filterable, writable, blendable, multisample, resolve, colorRenderable,
                  false, pixelBytes, channels);
}

void FormatTable::setFormatCaps(MTLPixelFormat formatId,
                                bool filterable,
                                bool writable,
                                bool blendable,
                                bool multisample,
                                bool resolve,
                                bool colorRenderable,
                                bool depthRenderable)
{
    setFormatCaps(formatId, filterable, writable, blendable, multisample, resolve, colorRenderable,
                  depthRenderable, 0, 0);
}
void FormatTable::setFormatCaps(MTLPixelFormat id,
                                bool filterable,
                                bool writable,
                                bool blendable,
                                bool multisample,
                                bool resolve,
                                bool colorRenderable,
                                bool depthRenderable,
                                NSUInteger pixelBytes,
                                NSUInteger channels)
{
    mNativePixelFormatCapsTable[id].filterable      = filterable;
    mNativePixelFormatCapsTable[id].writable        = writable;
    mNativePixelFormatCapsTable[id].colorRenderable = colorRenderable;
    mNativePixelFormatCapsTable[id].depthRenderable = depthRenderable;
    mNativePixelFormatCapsTable[id].blendable       = blendable;
    mNativePixelFormatCapsTable[id].multisample     = multisample;
    mNativePixelFormatCapsTable[id].resolve         = resolve;
    mNativePixelFormatCapsTable[id].pixelBytes      = pixelBytes;
    mNativePixelFormatCapsTable[id].pixelBytesMSAA  = pixelBytes;
    mNativePixelFormatCapsTable[id].channels        = channels;
    if (channels != 0)
        mNativePixelFormatCapsTable[id].alignment = MAX(pixelBytes / channels, 1U);
}

void FormatTable::setCompressedFormatCaps(MTLPixelFormat formatId, bool filterable)
{
    setFormatCaps(formatId, filterable, false, false, false, false, false, false);
}

void FormatTable::adjustFormatCapsForDevice(id<MTLDevice> device,
                                            MTLPixelFormat id,
                                            bool supportsiOS2,
                                            bool supportsiOS4)
{
#if !(TARGET_OS_OSX || TARGET_OS_MACCATALYST)

    NSUInteger pixelBytesRender     = mNativePixelFormatCapsTable[id].pixelBytes;
    NSUInteger pixelBytesRenderMSAA = mNativePixelFormatCapsTable[id].pixelBytesMSAA;
    NSUInteger alignment            = mNativePixelFormatCapsTable[id].alignment;

// Override the current pixelBytesRender
#    define SPECIFIC(_pixelFormat, _pixelBytesRender)                                            \
        case _pixelFormat:                                                                       \
            pixelBytesRender     = _pixelBytesRender;                                            \
            pixelBytesRenderMSAA = _pixelBytesRender;                                            \
            alignment =                                                                          \
                supportsiOS4 ? _pixelBytesRender / mNativePixelFormatCapsTable[id].channels : 4; \
            break
// Override the current pixel bytes render, and MSAA
#    define SPECIFIC_MSAA(_pixelFormat, _pixelBytesRender, _pixelBytesRenderMSAA)                \
        case _pixelFormat:                                                                       \
            pixelBytesRender     = _pixelBytesRender;                                            \
            pixelBytesRenderMSAA = _pixelBytesRenderMSAA;                                        \
            alignment =                                                                          \
                supportsiOS4 ? _pixelBytesRender / mNativePixelFormatCapsTable[id].channels : 4; \
            break
// Override the current pixelBytesRender, and alignment
#    define SPECIFIC_ALIGN(_pixelFormat, _pixelBytesRender, _alignment) \
        case _pixelFormat:                                              \
            pixelBytesRender     = _pixelBytesRender;                   \
            pixelBytesRenderMSAA = _pixelBytesRender;                   \
            alignment            = _alignment;                          \
            break

    if (!mNativePixelFormatCapsTable[id].compressed)
    {
        // On AppleGPUFamily4+, there is no 4byte minimum requirement for render targets
        uint32_t minSize     = supportsiOS4 ? 1U : 4U;
        pixelBytesRender     = MAX(mNativePixelFormatCapsTable[id].pixelBytes, minSize);
        pixelBytesRenderMSAA = pixelBytesRender;
        alignment =
            supportsiOS4 ? MAX(pixelBytesRender / mNativePixelFormatCapsTable[id].channels, 1U) : 4;
    }

    // This list of tables starts from a general multi-platform table,
    // to specific platforms (i.e. ios2, ios4) inheriting from the previous tables

    // Start off with the general case
    switch ((NSUInteger)id)
    {
        SPECIFIC(MTLPixelFormatB5G6R5Unorm, 4U);
        SPECIFIC(MTLPixelFormatA1BGR5Unorm, 4U);
        SPECIFIC(MTLPixelFormatABGR4Unorm, 4U);
        SPECIFIC(MTLPixelFormatBGR5A1Unorm, 4U);

        SPECIFIC(MTLPixelFormatRGBA8Unorm, 4U);
        SPECIFIC(MTLPixelFormatBGRA8Unorm, 4U);

        SPECIFIC_MSAA(MTLPixelFormatRGBA8Unorm_sRGB, 4U, 8U);
        SPECIFIC_MSAA(MTLPixelFormatBGRA8Unorm_sRGB, 4U, 8U);
        SPECIFIC_MSAA(MTLPixelFormatRGBA8Snorm, 4U, 8U);
        SPECIFIC_MSAA(MTLPixelFormatRGB10A2Uint, 4U, 8U);

        SPECIFIC(MTLPixelFormatRGB10A2Unorm, 8U);
        SPECIFIC(MTLPixelFormatBGR10A2Unorm, 8U);

        SPECIFIC(MTLPixelFormatRG11B10Float, 8U);

        SPECIFIC(MTLPixelFormatRGB9E5Float, 8U);

        SPECIFIC(MTLPixelFormatStencil8, 1U);
    }

    // Override based ios2
    if (supportsiOS2)
    {
        switch ((NSUInteger)id)
        {
            SPECIFIC(MTLPixelFormatB5G6R5Unorm, 8U);
            SPECIFIC(MTLPixelFormatA1BGR5Unorm, 8U);
            SPECIFIC(MTLPixelFormatABGR4Unorm, 8U);
            SPECIFIC(MTLPixelFormatBGR5A1Unorm, 8U);
            SPECIFIC_MSAA(MTLPixelFormatRGBA8Unorm, 4U, 8U);
            SPECIFIC_MSAA(MTLPixelFormatBGRA8Unorm, 4U, 8U);
        }
    }

    // Override based on ios4
    if (supportsiOS4)
    {
        switch ((NSUInteger)id)
        {
            SPECIFIC_ALIGN(MTLPixelFormatB5G6R5Unorm, 6U, 2U);
            SPECIFIC(MTLPixelFormatRGBA8Unorm, 4U);
            SPECIFIC(MTLPixelFormatBGRA8Unorm, 4U);

            SPECIFIC(MTLPixelFormatRGBA8Unorm_sRGB, 4U);
            SPECIFIC(MTLPixelFormatBGRA8Unorm_sRGB, 4U);

            SPECIFIC(MTLPixelFormatRGBA8Snorm, 4U);

            SPECIFIC_ALIGN(MTLPixelFormatRGB10A2Unorm, 4U, 4U);
            SPECIFIC_ALIGN(MTLPixelFormatBGR10A2Unorm, 4U, 4U);
            SPECIFIC(MTLPixelFormatRGB10A2Uint, 8U);

            SPECIFIC_ALIGN(MTLPixelFormatRG11B10Float, 4U, 4U);

            SPECIFIC_ALIGN(MTLPixelFormatRGB9E5Float, 4U, 4U);
        }
    }
    mNativePixelFormatCapsTable[id].pixelBytes     = pixelBytesRender;
    mNativePixelFormatCapsTable[id].pixelBytesMSAA = pixelBytesRenderMSAA;
    mNativePixelFormatCapsTable[id].alignment      = alignment;

#    undef SPECIFIC
#    undef SPECIFIC_ALIGN
#    undef SPECIFIC_MSAA
#endif
    // macOS does not need to perform any additoinal adjustment. These values are only used to check
    // valid MRT sizes on iOS.
}

void FormatTable::initNativeFormatCaps(const DisplayMtl *display)
{
    const angle::FeaturesMtl &featuresMtl = display->getFeatures();
    // Skip auto resolve if either hasDepth/StencilAutoResolve or allowMultisampleStoreAndResolve
    // feature are disabled.
    bool supportDepthAutoResolve = featuresMtl.hasDepthAutoResolve.enabled &&
                                   featuresMtl.allowMultisampleStoreAndResolve.enabled;
    bool supportStencilAutoResolve = featuresMtl.hasStencilAutoResolve.enabled &&
                                     featuresMtl.allowMultisampleStoreAndResolve.enabled;
    bool supportDepthStencilAutoResolve = supportDepthAutoResolve && supportStencilAutoResolve;

    // Source: https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf
    // clang-format off

    //            |  formatId                  | filterable    |  writable  |  blendable |  multisample |  resolve                              | colorRenderable | bytesPerChannel | channel
    setFormatCaps(MTLPixelFormatA8Unorm,        true,            false,       false,          false,       false,                                false, 1U, 1U);
    setFormatCaps(MTLPixelFormatR8Unorm,        true,            true,        true,           true,        true,                                 true,  1U, 1U);
    setFormatCaps(MTLPixelFormatR8Snorm,        true,            true,        true,           true,      display->supportsEitherGPUFamily(2, 1),  true,  1U, 1U);
    setFormatCaps(MTLPixelFormatR16Unorm,       true,            true,        true,           true,      display->supportsEitherGPUFamily(1, 1),        true,  2U, 1U);
    setFormatCaps(MTLPixelFormatR16Snorm,       true,            true,        true,           true,      display->supportsEitherGPUFamily(1, 1),        true,  2U, 1U);
    setFormatCaps(MTLPixelFormatRG8Unorm,       true,            true,        true,           true,        true,                                 true,  1U, 1U);
    setFormatCaps(MTLPixelFormatRG8Snorm,       true,            true,        true,           true,      display->supportsEitherGPUFamily(2, 1),  true,  2U, 2U);
    setFormatCaps(MTLPixelFormatRG16Unorm,      true,            true,        true,           true,      display->supportsEitherGPUFamily(1, 1),        true,  4U, 2U);
    setFormatCaps(MTLPixelFormatRG16Snorm,      true,            true,        true,           true,      display->supportsEitherGPUFamily(1, 1),        true,  4U, 2U);
    setFormatCaps(MTLPixelFormatRGBA16Unorm,    true,            true,        true,           true,      display->supportsEitherGPUFamily(1, 1),        true,  8U, 4U);
    setFormatCaps(MTLPixelFormatRGBA16Snorm,    true,            true,        true,           true,      display->supportsEitherGPUFamily(1, 1),        true,  8U, 4U);
    setFormatCaps(MTLPixelFormatRGBA16Float,    true,            true,        true,           true,        true,                                 true,  8U, 4U);

    //            |  formatId                      | filterable    |  writable                         |  blendable |  multisample |  resolve                              | colorRenderable |
    setFormatCaps(MTLPixelFormatRGBA8Unorm,          true,            true,                               true,           true,        true,                                    true,   4U, 4U);
    setFormatCaps(MTLPixelFormatRGBA8Unorm_sRGB,     true,          display->supportsIOSGPUFamily(2),      true,           true,        true,                                    true,   4U, 4U);
    setFormatCaps(MTLPixelFormatRGBA8Snorm,          true,            true,                               true,           true,     display->supportsEitherGPUFamily(2, 1),      true,   4U, 4U);
    setFormatCaps(MTLPixelFormatBGRA8Unorm,          true,            true,                               true,           true,        true,                                    true,   4U, 4U);
    setFormatCaps(MTLPixelFormatBGRA8Unorm_sRGB,     true,          display->supportsIOSGPUFamily(2),      true,           true,        true,                                    true,   4U, 4U);

    //            |  formatId              | filterable                    |  writable  |  blendable |  multisample |  resolve                              | colorRenderable |
    setFormatCaps(MTLPixelFormatR16Float,       true,                          true,        true,           true,        true,                                 true,    2U, 1U);
    setFormatCaps(MTLPixelFormatRG16Float,      true,                          true,        true,           true,        true,                                 true,    4U, 2U);
    setFormatCaps(MTLPixelFormatR32Float,    display->supportsEitherGPUFamily(1,1),  true,        true,           true,      display->supportsEitherGPUFamily(1,1),        true,    4U, 1U);

#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
    //            |  formatId                  | filterable    |  writable  |  blendable |  multisample |  resolve   | colorRenderable |
    setFormatCaps(MTLPixelFormatB5G6R5Unorm,      true,            false,        true,           true,        true,      true,  2U, 3U);
    setFormatCaps(MTLPixelFormatABGR4Unorm,       true,            false,        true,           true,        true,      true,  2U, 4U);
    setFormatCaps(MTLPixelFormatBGR5A1Unorm,      true,            false,        true,           true,        true,      true,  2U, 4U);
    setFormatCaps(MTLPixelFormatA1BGR5Unorm,      true,            false,        true,           true,        true,      true,  2U, 4U);
#endif

    //            |  formatId                  | filterable    |  writable                                 |  blendable |  multisample |  resolve   | colorRenderable |
    setFormatCaps(MTLPixelFormatBGR10A2Unorm,     true,         display->supportsEitherGPUFamily(3, 1),       true,           true,        true,      true,  4U, 4U);
    setFormatCaps(MTLPixelFormatRGB10A2Unorm,     true,         display->supportsEitherGPUFamily(3, 1),       true,           true,        true,      true,  4U, 4U);
    setFormatCaps(MTLPixelFormatRGB10A2Uint,      false,        display->supportsEitherGPUFamily(3, 1),       false,          true,        false,     true,  4U, 4U);
    setFormatCaps(MTLPixelFormatRG11B10Float,     true,         display->supportsEitherGPUFamily(3, 1),       true,           true,        true,      true,  4U, 3U);

    //            |  formatId                  | filterable    |  writable                         |  blendable                     |  multisample                    |  resolve                       | colorRenderable                 |
    setFormatCaps(MTLPixelFormatRGB9E5Float,       true,          display->supportsIOSGPUFamily(3),  display->supportsIOSGPUFamily(1),  display->supportsIOSGPUFamily(1), display->supportsIOSGPUFamily(1), display->supportsIOSGPUFamily(1), 4U, 3U);

    //            |  formatId               | filterable    |  writable  |  blendable  |  multisample                        |  resolve      | colorRenderable |
    setFormatCaps(MTLPixelFormatR8Uint,        false,           true,        false,          true,                             false,         true, 1U, 1U);
    setFormatCaps(MTLPixelFormatR8Sint,        false,           true,        false,          true,                             false,         true, 1U, 1U);
    setFormatCaps(MTLPixelFormatR16Uint,       false,           true,        false,          true,                             false,         true, 2U, 1U);
    setFormatCaps(MTLPixelFormatR16Sint,       false,           true,        false,          true,                             false,         true, 4U, 1U);
    setFormatCaps(MTLPixelFormatRG8Uint,       false,           true,        false,          true,                             false,         true, 2U, 2U);
    setFormatCaps(MTLPixelFormatRG8Sint,       false,           true,        false,          true,                             false,         true, 2U, 2U);
    setFormatCaps(MTLPixelFormatR32Uint,       false,           true,        false,          display->supportsEitherGPUFamily(1,1),  false,         true, 4U, 1U);
    setFormatCaps(MTLPixelFormatR32Sint,       false,           true,        false,          display->supportsEitherGPUFamily(1,1),  false,         true, 4U, 1U);
    setFormatCaps(MTLPixelFormatRG16Uint,      false,           true,        false,          true,                             false,         true, 4U, 2U);
    setFormatCaps(MTLPixelFormatRG16Sint,      false,           true,        false,          true,                             false,         true, 4U, 2U);
    setFormatCaps(MTLPixelFormatRGBA8Uint,     false,           true,        false,          true,                             false,         true, 4U, 1U);
    setFormatCaps(MTLPixelFormatRGBA8Sint,     false,           true,        false,          true,                             false,         true, 4U, 1U);
    setFormatCaps(MTLPixelFormatRG32Uint,      false,           true,        false,          display->supportsEitherGPUFamily(1,1),  false,         true, 8U, 2U);
    setFormatCaps(MTLPixelFormatRG32Sint,      false,           true,        false,          display->supportsEitherGPUFamily(1,1),  false,         true, 8U, 2U);
    setFormatCaps(MTLPixelFormatRGBA16Uint,    false,           true,        false,          true,                             false,         true, 8U, 4U);
    setFormatCaps(MTLPixelFormatRGBA16Sint,    false,           true,        false,          true,                             false,         true, 8U, 4U);
    setFormatCaps(MTLPixelFormatRGBA32Uint,    false,           true,        false,          display->supportsEitherGPUFamily(1,1),  false,         true, 16U, 4U);
    setFormatCaps(MTLPixelFormatRGBA32Sint,    false,           true,        false,          display->supportsEitherGPUFamily(1,1),  false,         true, 16U, 4U);

    //            |  formatId                   | filterable                      |  writable  |  blendable                     |  multisample                     |  resolve                         | colorRenderable |
    setFormatCaps(MTLPixelFormatRG32Float,       display->supportsEitherGPUFamily(1,1),   true,        true,                            display->supportsEitherGPUFamily(1,1),  display->supportsEitherGPUFamily(1,1),         true,  8U, 2U);
    setFormatCaps(MTLPixelFormatRGBA32Float,     display->supportsEitherGPUFamily(1,1),   true,        display->supportsEitherGPUFamily(1,1), display->supportsEitherGPUFamily(1,1),  display->supportsEitherGPUFamily(1,1),         true,  16U, 4U);

    //            |  formatId                           | filterable                       |  writable  |  blendable |  multisample |  resolve                                | colorRenderable | depthRenderable                    |
    setFormatCaps(MTLPixelFormatDepth32Float,               display->supportsEitherGPUFamily(1,1),   false,        false,           true,    supportDepthAutoResolve,                    false,            true,  4U, 1U);
    setFormatCaps(MTLPixelFormatStencil8,                   false,                             false,        false,           true,    false,                                      false,            true,  1U, 1U);
    setFormatCaps(MTLPixelFormatDepth32Float_Stencil8,      display->supportsEitherGPUFamily(1,1),   false,        false,           true,    supportDepthStencilAutoResolve,             false,            true,  8U, 2U);
//ToDo: @available on 13.0
    setFormatCaps(MTLPixelFormatDepth16Unorm,               true,                              false,        false,           true,    supportDepthAutoResolve,                    false,            true,  2U, 1U);
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
    setFormatCaps(MTLPixelFormatDepth24Unorm_Stencil8,      display->supportsEitherGPUFamily(1,1),   false,        false,           true,    supportDepthStencilAutoResolve,             false,            display->supportsEitherGPUFamily(1,1), 4U, 2U);

    setCompressedFormatCaps(MTLPixelFormatBC1_RGBA, true);
    setCompressedFormatCaps(MTLPixelFormatBC1_RGBA_sRGB, true);
    setCompressedFormatCaps(MTLPixelFormatBC2_RGBA, true);
    setCompressedFormatCaps(MTLPixelFormatBC2_RGBA_sRGB, true);
    setCompressedFormatCaps(MTLPixelFormatBC3_RGBA, true);
    setCompressedFormatCaps(MTLPixelFormatBC3_RGBA_sRGB, true);
#else
    setCompressedFormatCaps(MTLPixelFormatPVRTC_RGB_2BPP, true);
    setCompressedFormatCaps(MTLPixelFormatPVRTC_RGB_2BPP_sRGB, true);
    setCompressedFormatCaps(MTLPixelFormatPVRTC_RGB_4BPP, true);
    setCompressedFormatCaps(MTLPixelFormatPVRTC_RGB_4BPP_sRGB, true);
    setCompressedFormatCaps(MTLPixelFormatPVRTC_RGBA_2BPP, true);
    setCompressedFormatCaps(MTLPixelFormatPVRTC_RGBA_2BPP_sRGB, true);
    setCompressedFormatCaps(MTLPixelFormatPVRTC_RGBA_4BPP, true);
    setCompressedFormatCaps(MTLPixelFormatPVRTC_RGBA_4BPP_sRGB, true);
    setCompressedFormatCaps(MTLPixelFormatEAC_R11Unorm, true);
    setCompressedFormatCaps(MTLPixelFormatEAC_R11Snorm, true);
    setCompressedFormatCaps(MTLPixelFormatEAC_RG11Unorm, true);
    setCompressedFormatCaps(MTLPixelFormatEAC_RG11Snorm, true);
    setCompressedFormatCaps(MTLPixelFormatEAC_RGBA8, true);
    setCompressedFormatCaps(MTLPixelFormatEAC_RGBA8_sRGB, true);
    setCompressedFormatCaps(MTLPixelFormatETC2_RGB8, true);
    setCompressedFormatCaps(MTLPixelFormatETC2_RGB8_sRGB, true);
    setCompressedFormatCaps(MTLPixelFormatETC2_RGB8A1, true);
    setCompressedFormatCaps(MTLPixelFormatETC2_RGB8A1_sRGB, true);
    setCompressedFormatCaps(MTLPixelFormatASTC_4x4_sRGB, true);
    setCompressedFormatCaps(MTLPixelFormatASTC_5x4_sRGB, true);
    setCompressedFormatCaps(MTLPixelFormatASTC_5x5_sRGB, true);
    setCompressedFormatCaps(MTLPixelFormatASTC_6x5_sRGB, true);
    setCompressedFormatCaps(MTLPixelFormatASTC_6x6_sRGB, true);
    setCompressedFormatCaps(MTLPixelFormatASTC_8x5_sRGB, true);
    setCompressedFormatCaps(MTLPixelFormatASTC_8x6_sRGB, true);
    setCompressedFormatCaps(MTLPixelFormatASTC_8x8_sRGB, true);
    setCompressedFormatCaps(MTLPixelFormatASTC_10x5_sRGB, true);
    setCompressedFormatCaps(MTLPixelFormatASTC_10x6_sRGB, true);
    setCompressedFormatCaps(MTLPixelFormatASTC_10x8_sRGB, true);
    setCompressedFormatCaps(MTLPixelFormatASTC_10x10_sRGB, true);
    setCompressedFormatCaps(MTLPixelFormatASTC_12x10_sRGB, true);
    setCompressedFormatCaps(MTLPixelFormatASTC_12x12_sRGB, true);
    setCompressedFormatCaps(MTLPixelFormatASTC_4x4_LDR, true);
    setCompressedFormatCaps(MTLPixelFormatASTC_5x4_LDR, true);
    setCompressedFormatCaps(MTLPixelFormatASTC_5x5_LDR, true);
    setCompressedFormatCaps(MTLPixelFormatASTC_6x5_LDR, true);
    setCompressedFormatCaps(MTLPixelFormatASTC_6x6_LDR, true);
    setCompressedFormatCaps(MTLPixelFormatASTC_8x5_LDR, true);
    setCompressedFormatCaps(MTLPixelFormatASTC_8x6_LDR, true);
    setCompressedFormatCaps(MTLPixelFormatASTC_8x8_LDR, true);
    setCompressedFormatCaps(MTLPixelFormatASTC_10x5_LDR, true);
    setCompressedFormatCaps(MTLPixelFormatASTC_10x6_LDR, true);
    setCompressedFormatCaps(MTLPixelFormatASTC_10x8_LDR, true);
    setCompressedFormatCaps(MTLPixelFormatASTC_10x10_LDR, true);
    setCompressedFormatCaps(MTLPixelFormatASTC_12x10_LDR, true);
    setCompressedFormatCaps(MTLPixelFormatASTC_12x12_LDR, true);
#endif
    // clang-format on
}

}  // namespace mtl
}  // namespace rx
