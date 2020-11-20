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
            textureCaps.blendable         = mtlFormat.getCaps().blendable;
            textureCaps.textureAttachment = textureCaps.renderbuffer;
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
    if (srcFormatId == angle::FormatID::ETC1_R8G8B8_UNORM_BLOCK &&
        actualFormatId == angle::FormatID::ETC2_R8G8B8_UNORM_BLOCK)
    {
        // ETC1 RGB & ETC2 RGB are technically the same.
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

        if (!mPixelFormatTable[i].caps->depthRenderable &&
            mPixelFormatTable[i].actualFormatId != mPixelFormatTable[i].intendedFormatId)
        {
            mPixelFormatTable[i].textureLoadFunctions = angle::GetLoadFunctionsMap(
                mPixelFormatTable[i].intendedAngleFormat().glInternalFormat,
                mPixelFormatTable[i].actualFormatId);
        }

        mVertexFormatTables[0][i].init(formatId, false);
        mVertexFormatTables[1][i].init(formatId, true);
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
                  false);
}

void FormatTable::setFormatCaps(MTLPixelFormat id,
                                bool filterable,
                                bool writable,
                                bool blendable,
                                bool multisample,
                                bool resolve,
                                bool colorRenderable,
                                bool depthRenderable)
{
    mNativePixelFormatCapsTable[id].filterable      = filterable;
    mNativePixelFormatCapsTable[id].writable        = writable;
    mNativePixelFormatCapsTable[id].colorRenderable = colorRenderable;
    mNativePixelFormatCapsTable[id].depthRenderable = depthRenderable;
    mNativePixelFormatCapsTable[id].blendable       = blendable;
    mNativePixelFormatCapsTable[id].multisample     = multisample;
    mNativePixelFormatCapsTable[id].resolve         = resolve;
}

void FormatTable::setCompressedFormatCaps(MTLPixelFormat formatId, bool filterable)
{
    setFormatCaps(formatId, filterable, false, false, false, false, false, false);
}

void FormatTable::initNativeFormatCaps(const DisplayMtl *display)
{
    initNativeFormatCapsAutogen(display);
}

}  // namespace mtl
}  // namespace rx
