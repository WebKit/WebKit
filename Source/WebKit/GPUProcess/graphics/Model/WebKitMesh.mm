/*
 * Copyright (c) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "WebKitMesh.h"

#import "ModelTypes.h"

#import <WebCore/IOSurface.h>
#import <WebCore/ProcessIdentity.h>
#import <wtf/BlockPtr.h>
#import <wtf/CheckedArithmetic.h>
#import <wtf/CompletionHandler.h>
#import <wtf/MathExtras.h>
#import <wtf/RunLoop.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/spi/cocoa/IOSurfaceSPI.h>
#import <wtf/threads/BinarySemaphore.h>

#import "WebKitSwiftSoftLink.h"

namespace WebModel {

#if ENABLE(GPU_PROCESS_MODEL)

static WKBridgeTypedResourceId *convert(const TypedResourceId& update)
{
    return [WebKit::allocWKBridgeTypedResourceIdInstance() initWithValue:[[NSUUID alloc] initWithUUIDString:update.value.createNSString().get()] path:update.path.createNSString().get() hashValue:update.hashValue];
}

// Helper conversion functions from WebGPU types to Metal types
static WKBridgeVertexSemantic toMetal(VertexSemantic semantic)
{
    switch (semantic) {
    case VertexSemantic::Position:
        return WKBridgeVertexSemanticPosition;
    case VertexSemantic::Color:
        return WKBridgeVertexSemanticColor;
    case VertexSemantic::Normal:
        return WKBridgeVertexSemanticNormal;
    case VertexSemantic::Tangent:
        return WKBridgeVertexSemanticTangent;
    case VertexSemantic::Bitangent:
        return WKBridgeVertexSemanticBitangent;
    case VertexSemantic::UV0:
        return WKBridgeVertexSemanticUV0;
    case VertexSemantic::UV1:
        return WKBridgeVertexSemanticUV1;
    case VertexSemantic::UV2:
        return WKBridgeVertexSemanticUV2;
    case VertexSemantic::UV3:
        return WKBridgeVertexSemanticUV3;
    case VertexSemantic::UV4:
        return WKBridgeVertexSemanticUV4;
    case VertexSemantic::UV5:
        return WKBridgeVertexSemanticUV5;
    case VertexSemantic::UV6:
        return WKBridgeVertexSemanticUV6;
    case VertexSemantic::UV7:
        return WKBridgeVertexSemanticUV7;
    case VertexSemantic::Unspecified:
        return WKBridgeVertexSemanticUnspecified;
    }
}

static MTLVertexFormat toMetal(WebCore::WebGPU::VertexFormat format)
{
    switch (format) {
    case WebCore::WebGPU::VertexFormat::Uint8:
        return MTLVertexFormatUChar;
    case WebCore::WebGPU::VertexFormat::Uint8x2:
        return MTLVertexFormatUChar2;
    case WebCore::WebGPU::VertexFormat::Uint8x4:
        return MTLVertexFormatUChar4;
    case WebCore::WebGPU::VertexFormat::Sint8:
        return MTLVertexFormatChar;
    case WebCore::WebGPU::VertexFormat::Sint8x2:
        return MTLVertexFormatChar2;
    case WebCore::WebGPU::VertexFormat::Sint8x4:
        return MTLVertexFormatChar4;
    case WebCore::WebGPU::VertexFormat::Unorm8:
        return MTLVertexFormatUCharNormalized;
    case WebCore::WebGPU::VertexFormat::Unorm8x2:
        return MTLVertexFormatUChar2Normalized;
    case WebCore::WebGPU::VertexFormat::Unorm8x4:
        return MTLVertexFormatUChar4Normalized;
    case WebCore::WebGPU::VertexFormat::Snorm8:
        return MTLVertexFormatCharNormalized;
    case WebCore::WebGPU::VertexFormat::Snorm8x2:
        return MTLVertexFormatChar2Normalized;
    case WebCore::WebGPU::VertexFormat::Snorm8x4:
        return MTLVertexFormatChar4Normalized;
    case WebCore::WebGPU::VertexFormat::Uint16:
        return MTLVertexFormatUShort;
    case WebCore::WebGPU::VertexFormat::Uint16x2:
        return MTLVertexFormatUShort2;
    case WebCore::WebGPU::VertexFormat::Uint16x4:
        return MTLVertexFormatUShort4;
    case WebCore::WebGPU::VertexFormat::Sint16:
        return MTLVertexFormatShort;
    case WebCore::WebGPU::VertexFormat::Sint16x2:
        return MTLVertexFormatShort2;
    case WebCore::WebGPU::VertexFormat::Sint16x4:
        return MTLVertexFormatShort4;
    case WebCore::WebGPU::VertexFormat::Unorm16:
        return MTLVertexFormatUShortNormalized;
    case WebCore::WebGPU::VertexFormat::Unorm16x2:
        return MTLVertexFormatUShort2Normalized;
    case WebCore::WebGPU::VertexFormat::Unorm16x4:
        return MTLVertexFormatUShort4Normalized;
    case WebCore::WebGPU::VertexFormat::Snorm16:
        return MTLVertexFormatShortNormalized;
    case WebCore::WebGPU::VertexFormat::Snorm16x2:
        return MTLVertexFormatShort2Normalized;
    case WebCore::WebGPU::VertexFormat::Snorm16x4:
        return MTLVertexFormatShort4Normalized;
    case WebCore::WebGPU::VertexFormat::Float16:
        return MTLVertexFormatHalf;
    case WebCore::WebGPU::VertexFormat::Float16x2:
        return MTLVertexFormatHalf2;
    case WebCore::WebGPU::VertexFormat::Float16x4:
        return MTLVertexFormatHalf4;
    case WebCore::WebGPU::VertexFormat::Float32:
        return MTLVertexFormatFloat;
    case WebCore::WebGPU::VertexFormat::Float32x2:
        return MTLVertexFormatFloat2;
    case WebCore::WebGPU::VertexFormat::Float32x3:
        return MTLVertexFormatFloat3;
    case WebCore::WebGPU::VertexFormat::Float32x4:
        return MTLVertexFormatFloat4;
    case WebCore::WebGPU::VertexFormat::Uint32:
        return MTLVertexFormatUInt;
    case WebCore::WebGPU::VertexFormat::Uint32x2:
        return MTLVertexFormatUInt2;
    case WebCore::WebGPU::VertexFormat::Uint32x3:
        return MTLVertexFormatUInt3;
    case WebCore::WebGPU::VertexFormat::Uint32x4:
        return MTLVertexFormatUInt4;
    case WebCore::WebGPU::VertexFormat::Sint32:
        return MTLVertexFormatInt;
    case WebCore::WebGPU::VertexFormat::Sint32x2:
        return MTLVertexFormatInt2;
    case WebCore::WebGPU::VertexFormat::Sint32x3:
        return MTLVertexFormatInt3;
    case WebCore::WebGPU::VertexFormat::Sint32x4:
        return MTLVertexFormatInt4;
    case WebCore::WebGPU::VertexFormat::Unorm1010102:
        return MTLVertexFormatUInt1010102Normalized;
    case WebCore::WebGPU::VertexFormat::Unorm8x4Bgra:
        return MTLVertexFormatUChar4Normalized_BGRA;
    }
}

static MTLTextureType toMetal(WebCore::WebGPU::TextureViewDimension dimension)
{
    switch (dimension) {
    case WebCore::WebGPU::TextureViewDimension::_1d:
        return MTLTextureType1D;
    case WebCore::WebGPU::TextureViewDimension::_2d:
        return MTLTextureType2D;
    case WebCore::WebGPU::TextureViewDimension::_2dArray:
        return MTLTextureType2DArray;
    case WebCore::WebGPU::TextureViewDimension::Cube:
        return MTLTextureTypeCube;
    case WebCore::WebGPU::TextureViewDimension::CubeArray:
        return MTLTextureTypeCubeArray;
    case WebCore::WebGPU::TextureViewDimension::_3d:
        return MTLTextureType3D;
    }
}

static MTLTextureUsage toMetal(WebCore::WebGPU::TextureUsageFlags flags)
{
    MTLTextureUsage usage = 0;

    if (flags.contains(WebCore::WebGPU::TextureUsage::TextureBinding))
        usage |= MTLTextureUsageShaderRead;
    if (flags.contains(WebCore::WebGPU::TextureUsage::StorageBinding))
        usage |= MTLTextureUsageShaderWrite;
    if (flags.contains(WebCore::WebGPU::TextureUsage::RenderAttachment))
        usage |= MTLTextureUsageRenderTarget;
    if (flags.contains(WebCore::WebGPU::TextureUsage::CopySource) || flags.contains(WebCore::WebGPU::TextureUsage::CopyDestination))
        usage |= MTLTextureUsagePixelFormatView;

    return usage;
}

static MTLPrimitiveType toMetal(WebCore::WebGPU::PrimitiveTopology topology)
{
    switch (topology) {
    case WebCore::WebGPU::PrimitiveTopology::PointList:
        return MTLPrimitiveTypePoint;
    case WebCore::WebGPU::PrimitiveTopology::LineList:
        return MTLPrimitiveTypeLine;
    case WebCore::WebGPU::PrimitiveTopology::LineStrip:
        return MTLPrimitiveTypeLineStrip;
    case WebCore::WebGPU::PrimitiveTopology::TriangleList:
        return MTLPrimitiveTypeTriangle;
    case WebCore::WebGPU::PrimitiveTopology::TriangleStrip:
        return MTLPrimitiveTypeTriangleStrip;
    }
}

static MTLIndexType toMetal(WebModel::IndexType indexType)
{
    switch (indexType) {
    case WebModel::IndexType::UInt16:
        return MTLIndexTypeUInt16;
    case WebModel::IndexType::UInt32:
        return MTLIndexTypeUInt32;
    }
}

static MTLPixelFormat toMetal(WebCore::WebGPU::TextureFormat textureFormat)
{
    switch (textureFormat) {
    case WebCore::WebGPU::TextureFormat::R8unorm:
        return MTLPixelFormatR8Unorm;
    case WebCore::WebGPU::TextureFormat::R8snorm:
        return MTLPixelFormatR8Snorm;
    case WebCore::WebGPU::TextureFormat::R8uint:
        return MTLPixelFormatR8Uint;
    case WebCore::WebGPU::TextureFormat::R8sint:
        return MTLPixelFormatR8Sint;
    case WebCore::WebGPU::TextureFormat::R16unorm:
        return MTLPixelFormatR16Unorm;
    case WebCore::WebGPU::TextureFormat::R16snorm:
        return MTLPixelFormatR16Snorm;
    case WebCore::WebGPU::TextureFormat::R16uint:
        return MTLPixelFormatR16Uint;
    case WebCore::WebGPU::TextureFormat::R16sint:
        return MTLPixelFormatR16Sint;
    case WebCore::WebGPU::TextureFormat::R16float:
        return MTLPixelFormatR16Float;
    case WebCore::WebGPU::TextureFormat::Rg8unorm:
        return MTLPixelFormatRG8Unorm;
    case WebCore::WebGPU::TextureFormat::Rg8snorm:
        return MTLPixelFormatRG8Snorm;
    case WebCore::WebGPU::TextureFormat::Rg8uint:
        return MTLPixelFormatRG8Uint;
    case WebCore::WebGPU::TextureFormat::Rg8sint:
        return MTLPixelFormatRG8Sint;
    case WebCore::WebGPU::TextureFormat::R32float:
        return MTLPixelFormatR32Float;
    case WebCore::WebGPU::TextureFormat::R32uint:
        return MTLPixelFormatR32Uint;
    case WebCore::WebGPU::TextureFormat::R32sint:
        return MTLPixelFormatR32Sint;
    case WebCore::WebGPU::TextureFormat::Rg16unorm:
        return MTLPixelFormatRG16Unorm;
    case WebCore::WebGPU::TextureFormat::Rg16snorm:
        return MTLPixelFormatRG16Snorm;
    case WebCore::WebGPU::TextureFormat::Rg16uint:
        return MTLPixelFormatRG16Uint;
    case WebCore::WebGPU::TextureFormat::Rg16sint:
        return MTLPixelFormatRG16Sint;
    case WebCore::WebGPU::TextureFormat::Rg16float:
        return MTLPixelFormatRG16Float;
    case WebCore::WebGPU::TextureFormat::Rgba8unorm:
        return MTLPixelFormatRGBA8Unorm;
    case WebCore::WebGPU::TextureFormat::Rgba8unormSRGB:
        return MTLPixelFormatRGBA8Unorm_sRGB;
    case WebCore::WebGPU::TextureFormat::Rgba8snorm:
        return MTLPixelFormatRGBA8Snorm;
    case WebCore::WebGPU::TextureFormat::Rgba8uint:
        return MTLPixelFormatRGBA8Uint;
    case WebCore::WebGPU::TextureFormat::Rgba8sint:
        return MTLPixelFormatRGBA8Sint;
    case WebCore::WebGPU::TextureFormat::Bgra8unorm:
        return MTLPixelFormatBGRA8Unorm;
    case WebCore::WebGPU::TextureFormat::Bgra8unormSRGB:
        return MTLPixelFormatBGRA8Unorm_sRGB;
    case WebCore::WebGPU::TextureFormat::Rgb10a2unorm:
        return MTLPixelFormatRGB10A2Unorm;
    case WebCore::WebGPU::TextureFormat::Rg11b10ufloat:
        return MTLPixelFormatRG11B10Float;
    case WebCore::WebGPU::TextureFormat::Rgb9e5ufloat:
        return MTLPixelFormatRGB9E5Float;
    case WebCore::WebGPU::TextureFormat::Rgb10a2uint:
        return MTLPixelFormatRGB10A2Uint;
    case WebCore::WebGPU::TextureFormat::Rg32float:
        return MTLPixelFormatRG32Float;
    case WebCore::WebGPU::TextureFormat::Rg32uint:
        return MTLPixelFormatRG32Uint;
    case WebCore::WebGPU::TextureFormat::Rg32sint:
        return MTLPixelFormatRG32Sint;
    case WebCore::WebGPU::TextureFormat::Rgba16unorm:
        return MTLPixelFormatRGBA16Unorm;
    case WebCore::WebGPU::TextureFormat::Rgba16snorm:
        return MTLPixelFormatRGBA16Snorm;
    case WebCore::WebGPU::TextureFormat::Rgba16uint:
        return MTLPixelFormatRGBA16Uint;
    case WebCore::WebGPU::TextureFormat::Rgba16sint:
        return MTLPixelFormatRGBA16Sint;
    case WebCore::WebGPU::TextureFormat::Rgba16float:
        return MTLPixelFormatRGBA16Float;
    case WebCore::WebGPU::TextureFormat::Rgba32float:
        return MTLPixelFormatRGBA32Float;
    case WebCore::WebGPU::TextureFormat::Rgba32uint:
        return MTLPixelFormatRGBA32Uint;
    case WebCore::WebGPU::TextureFormat::Rgba32sint:
        return MTLPixelFormatRGBA32Sint;
    case WebCore::WebGPU::TextureFormat::Stencil8:
        return MTLPixelFormatStencil8;
    case WebCore::WebGPU::TextureFormat::Depth16unorm:
        return MTLPixelFormatDepth16Unorm;
    case WebCore::WebGPU::TextureFormat::Depth24plus:
        return MTLPixelFormatDepth32Float;
    case WebCore::WebGPU::TextureFormat::Depth24plusStencil8:
        return MTLPixelFormatDepth32Float_Stencil8;
    case WebCore::WebGPU::TextureFormat::Depth32float:
        return MTLPixelFormatDepth32Float;
    case WebCore::WebGPU::TextureFormat::Depth32floatStencil8:
        return MTLPixelFormatDepth32Float_Stencil8;
    case WebCore::WebGPU::TextureFormat::Bc1RgbaUnorm:
        return MTLPixelFormatBC1_RGBA;
    case WebCore::WebGPU::TextureFormat::Bc1RgbaUnormSRGB:
        return MTLPixelFormatBC1_RGBA_sRGB;
    case WebCore::WebGPU::TextureFormat::Bc2RgbaUnorm:
        return MTLPixelFormatBC2_RGBA;
    case WebCore::WebGPU::TextureFormat::Bc2RgbaUnormSRGB:
        return MTLPixelFormatBC2_RGBA_sRGB;
    case WebCore::WebGPU::TextureFormat::Bc3RgbaUnorm:
        return MTLPixelFormatBC3_RGBA;
    case WebCore::WebGPU::TextureFormat::Bc3RgbaUnormSRGB:
        return MTLPixelFormatBC3_RGBA_sRGB;
    case WebCore::WebGPU::TextureFormat::Bc4RUnorm:
        return MTLPixelFormatBC4_RUnorm;
    case WebCore::WebGPU::TextureFormat::Bc4RSnorm:
        return MTLPixelFormatBC4_RSnorm;
    case WebCore::WebGPU::TextureFormat::Bc5RgUnorm:
        return MTLPixelFormatBC5_RGUnorm;
    case WebCore::WebGPU::TextureFormat::Bc5RgSnorm:
        return MTLPixelFormatBC5_RGSnorm;
    case WebCore::WebGPU::TextureFormat::Bc6hRgbUfloat:
        return MTLPixelFormatBC6H_RGBUfloat;
    case WebCore::WebGPU::TextureFormat::Bc6hRgbFloat:
        return MTLPixelFormatBC6H_RGBFloat;
    case WebCore::WebGPU::TextureFormat::Bc7RgbaUnorm:
        return MTLPixelFormatBC7_RGBAUnorm;
    case WebCore::WebGPU::TextureFormat::Bc7RgbaUnormSRGB:
        return MTLPixelFormatBC7_RGBAUnorm_sRGB;
    case WebCore::WebGPU::TextureFormat::Etc2Rgb8unorm:
        return MTLPixelFormatETC2_RGB8;
    case WebCore::WebGPU::TextureFormat::Etc2Rgb8unormSRGB:
        return MTLPixelFormatETC2_RGB8_sRGB;
    case WebCore::WebGPU::TextureFormat::Etc2Rgb8a1unorm:
        return MTLPixelFormatETC2_RGB8A1;
    case WebCore::WebGPU::TextureFormat::Etc2Rgb8a1unormSRGB:
        return MTLPixelFormatETC2_RGB8A1_sRGB;
    case WebCore::WebGPU::TextureFormat::Etc2Rgba8unorm:
        return MTLPixelFormatEAC_RGBA8;
    case WebCore::WebGPU::TextureFormat::Etc2Rgba8unormSRGB:
        return MTLPixelFormatEAC_RGBA8_sRGB;
    case WebCore::WebGPU::TextureFormat::EacR11unorm:
        return MTLPixelFormatEAC_R11Unorm;
    case WebCore::WebGPU::TextureFormat::EacR11snorm:
        return MTLPixelFormatEAC_R11Snorm;
    case WebCore::WebGPU::TextureFormat::EacRg11unorm:
        return MTLPixelFormatEAC_RG11Unorm;
    case WebCore::WebGPU::TextureFormat::EacRg11snorm:
        return MTLPixelFormatEAC_RG11Snorm;
    case WebCore::WebGPU::TextureFormat::Astc4x4Unorm:
        return MTLPixelFormatASTC_4x4_LDR;
    case WebCore::WebGPU::TextureFormat::Astc4x4UnormSRGB:
        return MTLPixelFormatASTC_4x4_sRGB;
    case WebCore::WebGPU::TextureFormat::Astc5x4Unorm:
        return MTLPixelFormatASTC_5x4_LDR;
    case WebCore::WebGPU::TextureFormat::Astc5x4UnormSRGB:
        return MTLPixelFormatASTC_5x4_sRGB;
    case WebCore::WebGPU::TextureFormat::Astc5x5Unorm:
        return MTLPixelFormatASTC_5x5_LDR;
    case WebCore::WebGPU::TextureFormat::Astc5x5UnormSRGB:
        return MTLPixelFormatASTC_5x5_sRGB;
    case WebCore::WebGPU::TextureFormat::Astc6x5Unorm:
        return MTLPixelFormatASTC_6x5_LDR;
    case WebCore::WebGPU::TextureFormat::Astc6x5UnormSRGB:
        return MTLPixelFormatASTC_6x5_sRGB;
    case WebCore::WebGPU::TextureFormat::Astc6x6Unorm:
        return MTLPixelFormatASTC_6x6_LDR;
    case WebCore::WebGPU::TextureFormat::Astc6x6UnormSRGB:
        return MTLPixelFormatASTC_6x6_sRGB;
    case WebCore::WebGPU::TextureFormat::Astc8x5Unorm:
        return MTLPixelFormatASTC_8x5_LDR;
    case WebCore::WebGPU::TextureFormat::Astc8x5UnormSRGB:
        return MTLPixelFormatASTC_8x5_sRGB;
    case WebCore::WebGPU::TextureFormat::Astc8x6Unorm:
        return MTLPixelFormatASTC_8x6_LDR;
    case WebCore::WebGPU::TextureFormat::Astc8x6UnormSRGB:
        return MTLPixelFormatASTC_8x6_sRGB;
    case WebCore::WebGPU::TextureFormat::Astc8x8Unorm:
        return MTLPixelFormatASTC_8x8_LDR;
    case WebCore::WebGPU::TextureFormat::Astc8x8UnormSRGB:
        return MTLPixelFormatASTC_8x8_sRGB;
    case WebCore::WebGPU::TextureFormat::Astc10x5Unorm:
        return MTLPixelFormatASTC_10x5_LDR;
    case WebCore::WebGPU::TextureFormat::Astc10x5UnormSRGB:
        return MTLPixelFormatASTC_10x5_sRGB;
    case WebCore::WebGPU::TextureFormat::Astc10x6Unorm:
        return MTLPixelFormatASTC_10x6_LDR;
    case WebCore::WebGPU::TextureFormat::Astc10x6UnormSRGB:
        return MTLPixelFormatASTC_10x6_sRGB;
    case WebCore::WebGPU::TextureFormat::Astc10x8Unorm:
        return MTLPixelFormatASTC_10x8_LDR;
    case WebCore::WebGPU::TextureFormat::Astc10x8UnormSRGB:
        return MTLPixelFormatASTC_10x8_sRGB;
    case WebCore::WebGPU::TextureFormat::Astc10x10Unorm:
        return MTLPixelFormatASTC_10x10_LDR;
    case WebCore::WebGPU::TextureFormat::Astc10x10UnormSRGB:
        return MTLPixelFormatASTC_10x10_sRGB;
    case WebCore::WebGPU::TextureFormat::Astc12x10Unorm:
        return MTLPixelFormatASTC_12x10_LDR;
    case WebCore::WebGPU::TextureFormat::Astc12x10UnormSRGB:
        return MTLPixelFormatASTC_12x10_sRGB;
    case WebCore::WebGPU::TextureFormat::Astc12x12Unorm:
        return MTLPixelFormatASTC_12x12_LDR;
    case WebCore::WebGPU::TextureFormat::Astc12x12UnormSRGB:
        return MTLPixelFormatASTC_12x12_sRGB;
    }
}

static WKBridgeConstant convert(const Constant constant)
{
    switch (constant) {
    case Constant::kBool:
        return WKBridgeConstantBool;
    case Constant::kUchar:
        return WKBridgeConstantUchar;
    case Constant::kInt:
        return WKBridgeConstantInt;
    case Constant::kUint:
        return WKBridgeConstantUint;
    case Constant::kHalf:
        return WKBridgeConstantHalf;
    case Constant::kFloat:
        return WKBridgeConstantFloat;
    case Constant::kTimecode:
        return WKBridgeConstantTimecode;
    case Constant::kString:
        return WKBridgeConstantString;
    case Constant::kToken:
        return WKBridgeConstantToken;
    case Constant::kAsset:
        return WKBridgeConstantAsset;
    case Constant::kMatrix2f:
        return WKBridgeConstantMatrix2f;
    case Constant::kMatrix3f:
        return WKBridgeConstantMatrix3f;
    case Constant::kMatrix4f:
        return WKBridgeConstantMatrix4f;
    case Constant::kQuatf:
        return WKBridgeConstantQuatf;
    case Constant::kQuath:
        return WKBridgeConstantQuath;
    case Constant::kFloat2:
        return WKBridgeConstantFloat2;
    case Constant::kHalf2:
        return WKBridgeConstantHalf2;
    case Constant::kInt2:
        return WKBridgeConstantInt2;
    case Constant::kFloat3:
        return WKBridgeConstantFloat3;
    case Constant::kHalf3:
        return WKBridgeConstantHalf3;
    case Constant::kInt3:
        return WKBridgeConstantInt3;
    case Constant::kFloat4:
        return WKBridgeConstantFloat4;
    case Constant::kHalf4:
        return WKBridgeConstantHalf4;
    case Constant::kInt4:
        return WKBridgeConstantInt4;

    // semantic:
    case Constant::kPoint3f:
        return WKBridgeConstantPoint3f;
    case Constant::kPoint3h:
        return WKBridgeConstantPoint3h;
    case Constant::kNormal3f:
        return WKBridgeConstantNormal3f;
    case Constant::kNormal3h:
        return WKBridgeConstantNormal3h;
    case Constant::kVector3f:
        return WKBridgeConstantVector3f;
    case Constant::kVector3h:
        return WKBridgeConstantVector3h;
    case Constant::kColor3f:
        return WKBridgeConstantColor3f;
    case Constant::kColor3h:
        return WKBridgeConstantColor3h;
    case Constant::kColor4f:
        return WKBridgeConstantColor4f;
    case Constant::kColor4h:
        return WKBridgeConstantColor4h;
    case Constant::kTexCoord2h:
        return WKBridgeConstantTexCoord2h;
    case Constant::kTexCoord2f:
        return WKBridgeConstantTexCoord2f;
    case Constant::kTexCoord3h:
        return WKBridgeConstantTexCoord3h;
    case Constant::kTexCoord3f:
        return WKBridgeConstantTexCoord3f;
    }
}


static WKBridgeMeshPart *convert(const MeshPart& part)
{
    return [WebKit::allocWKBridgeMeshPartInstance() initWithIndexOffset:part.indexOffset indexCount:part.indexCount topology:toMetal(part.topology) materialIndex:part.materialIndex boundsMin:part.boundsMin boundsMax:part.boundsMax];
}

static NSArray<WKBridgeMeshPart *> *convert(const Vector<MeshPart>& parts)
{
    if (!parts.size())
        return nil;

    NSMutableArray<WKBridgeMeshPart *> *result = [NSMutableArray array];
    for (const auto& p : parts)
        [result addObject:convert(p)];

    return result;
}

static NSArray<WKBridgeTypedResourceId *> *convert(const Vector<TypedResourceId>& parts)
{
    if (!parts.size())
        return nil;

    NSMutableArray<WKBridgeTypedResourceId *> *result = [NSMutableArray array];
    for (const auto& p : parts)
        [result addObject:convert(p)];

    return result;
}

template<typename T>
static NSData *convert(const Vector<T>& data)
{
    if (!data.size())
        return nil;

    return [[NSData alloc] initWithBytes:data.span().data() length:data.sizeInBytes()];
}

template<typename T>
static NSArray<NSData*> *convert(const Vector<Vector<T>>& data)
{
    if (!data.size())
        return nil;

    NSMutableArray<NSData*> *result = [NSMutableArray array];
    for (const auto& v : data) {
        if (NSData *d = convert(v))
            [result addObject:d];
    }

    return result;
}

static NSArray<WKBridgeVertexAttributeFormat *> *convert(const Vector<VertexAttributeFormat>& formats)
{
    if (!formats.size())
        return nil;

    NSMutableArray<WKBridgeVertexAttributeFormat *> *result = [NSMutableArray array];
    for (const auto& format : formats)
        [result addObject:[WebKit::allocWKBridgeVertexAttributeFormatInstance() initWithSemantic:toMetal(format.semantic) format:toMetal(format.format) layoutIndex:format.layoutIndex offset:format.offset]];

    return result;
}

static NSArray<WKBridgeVertexLayout *> *convert(const Vector<VertexLayout>& layouts)
{
    if (!layouts.size())
        return nil;

    NSMutableArray<WKBridgeVertexLayout *> *result = [NSMutableArray array];
    for (const auto& layout : layouts)
        [result addObject:[WebKit::allocWKBridgeVertexLayoutInstance() initWithBufferIndex:layout.bufferIndex bufferOffset:layout.bufferOffset bufferStride:layout.bufferStride]];

    return result;
}

static WKBridgeMeshDescriptor *convert(const MeshDescriptor& descriptor)
{
    if (!descriptor.vertexBufferCount)
        return nil;

    return [WebKit::allocWKBridgeMeshDescriptorInstance() initWithVertexBufferCount:descriptor.vertexBufferCount
        vertexCapacity:descriptor.vertexCapacity
        vertexAttributes:convert(descriptor.vertexAttributes)
        vertexLayouts:convert(descriptor.vertexLayouts)
        indexCapacity:descriptor.indexCapacity
        indexType:toMetal(descriptor.indexType)];
}

static WKBridgeSkinningData *convert(const std::optional<SkinningData>& data)
{
    if (!data)
        return nil;

    return [WebKit::allocWKBridgeSkinningDataInstance() initWithInfluencePerVertexCount:data->influencePerVertexCount jointTransforms:convert(data->jointTransforms) inverseBindPoses:convert(data->inverseBindPoses) influenceJointIndices:convert(data->influenceJointIndices) influenceWeights:convert(data->influenceWeights) geometryBindTransform:data->geometryBindTransform];
}

static WKBridgeBlendShapeData *convert(const std::optional<BlendShapeData>& data)
{
    if (!data)
        return nil;

    return [WebKit::allocWKBridgeBlendShapeDataInstance() initWithWeights:convert(data->weights) positionOffsets:convert(data->positionOffsets) normalOffsets:convert(data->normalOffsets)];
}

static WKBridgeRenormalizationData *convert(const std::optional<RenormalizationData>& data)
{
    if (!data)
        return nil;

    return [WebKit::allocWKBridgeRenormalizationDataInstance() initWithVertexIndicesPerTriangle:convert(data->vertexIndicesPerTriangle) vertexAdjacencies:convert(data->vertexAdjacencies) vertexAdjacencyEndIndices:convert(data->vertexAdjacencyEndIndices)];
}

static WKBridgeDeformationData *convert(const std::optional<DeformationData>& data)
{
    if (!data)
        return nil;

    return [WebKit::allocWKBridgeDeformationDataInstance() initWithSkinningData:convert(data->skinningData) blendShapeData:convert(data->blendShapeData) renormalizationData:convert(data->renormalizationData)];
}

static MTLTextureSwizzleChannels convert(ImageAssetSwizzle swizzle)
{
    return MTLTextureSwizzleChannels {
        .red = static_cast<MTLTextureSwizzle>(swizzle.red),
        .green = static_cast<MTLTextureSwizzle>(swizzle.green),
        .blue = static_cast<MTLTextureSwizzle>(swizzle.blue),
        .alpha = static_cast<MTLTextureSwizzle>(swizzle.alpha)
    };
}

static WKBridgeImageAsset* convert(const ImageAsset& imageAsset)
{
    auto mtlPixelFormat = WebModel::toMetal(imageAsset.pixelFormat);
    return [WebKit::allocWKBridgeImageAssetInstance() initWithData:convert(imageAsset.data) width:imageAsset.width height:imageAsset.height depth:imageAsset.depth textureType:toMetal(imageAsset.textureType) pixelFormat:mtlPixelFormat mipmapLevelCount:imageAsset.mipmapLevelCount arrayLength:imageAsset.arrayLength textureUsage:toMetal(imageAsset.textureUsage) swizzle:convert(imageAsset.swizzle)];
}

static WKBridgeTextureLevelInfo* convert(const TextureLevelInfo& textureLevelInfo)
{
    return [WebKit::allocWKBridgeTextureLevelInfoInstance() initWithDataOffset:textureLevelInfo.dataOffset byteCountPerRow:textureLevelInfo.byteCountPerRow byteCountPerImage:textureLevelInfo.byteCountPerImage];
}

static WKBridgeDataType convert(DataType type)
{
    switch (type) {
    case DataType::kBool:
        return WKBridgeDataTypeBool;
    case DataType::kUchar:
        return WKBridgeDataTypeUchar;
    case DataType::kInt:
        return WKBridgeDataTypeInt;
    case DataType::kUint:
        return WKBridgeDataTypeUint;
    case DataType::kInt2:
        return WKBridgeDataTypeInt2;
    case DataType::kInt3:
        return WKBridgeDataTypeInt3;
    case DataType::kInt4:
        return WKBridgeDataTypeInt4;
    case DataType::kFloat:
        return WKBridgeDataTypeFloat;
    case DataType::kColor3f:
        return WKBridgeDataTypeCgColor3;
    case DataType::kColor3h:
        return WKBridgeDataTypeColor3h;
    case DataType::kColor4f:
        return WKBridgeDataTypeCgColor4;
    case DataType::kColor4h:
        return WKBridgeDataTypeColor4h;
    case DataType::kFloat2:
        return WKBridgeDataTypeFloat2;
    case DataType::kFloat3:
        return WKBridgeDataTypeFloat3;
    case DataType::kFloat4:
        return WKBridgeDataTypeFloat4;
    case DataType::kHalf:
        return WKBridgeDataTypeHalf;
    case DataType::kHalf2:
        return WKBridgeDataTypeHalf2;
    case DataType::kHalf3:
        return WKBridgeDataTypeHalf3;
    case DataType::kHalf4:
        return WKBridgeDataTypeHalf4;
    case DataType::kMatrix2f:
        return WKBridgeDataTypeMatrix2f;
    case DataType::kMatrix3f:
        return WKBridgeDataTypeMatrix3f;
    case DataType::kMatrix4f:
        return WKBridgeDataTypeMatrix4f;
    case DataType::kMatrix2h:
        return WKBridgeDataTypeMatrix2h;
    case DataType::kMatrix3h:
        return WKBridgeDataTypeMatrix3h;
    case DataType::kMatrix4h:
        return WKBridgeDataTypeMatrix4h;
    case DataType::kQuat:
        return WKBridgeDataTypeQuat;
    case DataType::kSurfaceShader:
        return WKBridgeDataTypeSurfaceShader;
    case DataType::kGeometryModifier:
        return WKBridgeDataTypeGeometryModifier;
    case DataType::kPostLightingShader:
        return WKBridgeDataTypePostLightingShader;
    case DataType::kString:
        return WKBridgeDataTypeString;
    case DataType::kToken:
        return WKBridgeDataTypeToken;
    case DataType::kAsset:
        return WKBridgeDataTypeAsset;
    default:
        RELEASE_ASSERT_NOT_REACHED("unknown data type");
    }
}

static NSArray<WKBridgeValueString *> *convert(const Vector<Variant<String, double>>& constantValues)
{
    NSMutableArray<WKBridgeValueString *> *result = [NSMutableArray array];
    for (const auto& c : constantValues) {
        [result addObject:WTF::switchOn(c, [&](const String& s) -> WKBridgeValueString * {
            return [WebKit::allocWKBridgeValueStringInstance() initWithString:s.createNSString().get()];
        }, [&] (double d) -> WKBridgeValueString * {
            return [WebKit::allocWKBridgeValueStringInstance() initWithNumber:[[NSNumber alloc] initWithDouble:d]];
        })];
    }

    return result;
}

static WKBridgeConstantContainer *convert(const ConstantContainer& constant)
{
    return [WebKit::allocWKBridgeConstantContainerInstance() initWithConstant:convert(constant.constant) constantValues:convert(constant.constantValues) name:constant.name.createNSString().get()];
}

static NSArray<WKBridgeInputOutput *> *convert(const Vector<InputOutput>& inputOutputs)
{
    NSMutableArray<WKBridgeInputOutput *> *result = [NSMutableArray array];
    for (const auto& io : inputOutputs) {
        NSString *semanticTypeName = io.semanticTypeName ? io.semanticTypeName->createNSString().get() : nil;

        WKBridgeConstantContainer *defaultValue = nil;
        if (io.defaultValue)
            defaultValue = convert(*io.defaultValue);

        [result addObject:[WebKit::allocWKBridgeInputOutputInstance() initWithType:convert(io.type)
            name:io.name.createNSString().get()
            semanticTypeName:semanticTypeName
            defaultValue:defaultValue]];
    }

    return result;
}

static NSArray<WKBridgeEdge *> *convert(const Vector<Edge>& edges)
{
    NSMutableArray<WKBridgeEdge *> *result = [NSMutableArray array];
    for (const auto& e : edges) {
        [result addObject:[WebKit::allocWKBridgeEdgeInstance() initWithOutputNode:e.outputNode.createNSString().get()
            outputPort:e.outputPort.createNSString().get()
            inputNode:e.inputNode.createNSString().get()
            inputPort:e.inputPort.createNSString().get()]];
    }

    return result;
}

static WKBridgeNodeType convert(NodeType bridgeNodeType)
{
    switch (bridgeNodeType) {
    case NodeType::Builtin:
        return WKBridgeNodeTypeBuiltin;
    case NodeType::Constant:
        return WKBridgeNodeTypeConstant;
    case NodeType::Arguments:
        return WKBridgeNodeTypeArguments;
    case NodeType::Results:
        return WKBridgeNodeTypeResults;
    }
}

static WKBridgeBuiltin *convert(const Builtin& builtin)
{
    return [WebKit::allocWKBridgeBuiltinInstance() initWithDefinition:builtin.definition.createNSString().get() name:builtin.name.createNSString().get()];
}

static WKBridgeNode *convert(const Node& node)
{
    return [WebKit::allocWKBridgeNodeInstance() initWithBridgeNodeType:convert(node.bridgeNodeType) builtin:convert(node.builtin) constant:convert(node.constant)];
}

static NSArray<WKBridgeNode *> *convert(const Vector<Node>& nodes)
{
    NSMutableArray<WKBridgeNode *> *result = [NSMutableArray array];
    for (const auto& node : nodes)
        [result addObject:convert(node)];
    return result;
}

static NSArray<NSString *> *convertStrings(const Vector<String>& strings)
{
    NSMutableArray<NSString *> *result = [NSMutableArray arrayWithCapacity:strings.size()];
    for (const auto& s : strings)
        [result addObject:s.createNSString().get()];
    return result;
}

static WKBridgeMaterialGraph *convert(const MaterialGraph& material)
{
    return [WebKit::allocWKBridgeMaterialGraphInstance()
        initWithGraphName:material.graphName.createNSString().get()
        nodes:convert(material.nodes)
        edges:convert(material.edges)
        arguments:convert(material.arguments)
        results:convert(material.results)
        inputs:convert(material.inputs)
        outputs:convert(material.outputs)
        primvarMappingPrimvarNames:convertStrings(material.primvarMappingPrimvarNames)
        primvarMappingTexcoordNames:convertStrings(material.primvarMappingTexcoordNames)
        functionConstantInputNames:convertStrings(material.functionConstantInputNames)];
}

#endif

} // namespace WebModel

namespace WebKit {

static RetainPtr<NSMutableArray> createMetalTextures(id<MTLDevice> device, const Vector<RetainPtr<IOSurfaceRef>>& ioSurfaces, unsigned width, unsigned height)
{
    MTLTextureDescriptor *textureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA16Float width:width height:height mipmapped:NO];
    RetainPtr textures = adoptNS([[NSMutableArray alloc] init]);
    for (auto& ioSurface : ioSurfaces)
        [textures addObject:[device newTextureWithDescriptor:textureDescriptor iosurface:ioSurface.get() plane:0]];
    return textures;
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebMesh);

WebMesh::WebMesh(const WebModelCreateMeshDescriptor& descriptor)
{
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    m_textures = createMetalTextures(device, descriptor.ioSurfaces, descriptor.width, descriptor.height);

#if ENABLE(GPU_PROCESS_MODEL)
    WKBridgeUSDConfiguration *configuration = [WebKit::allocWKBridgeUSDConfigurationInstance() initWithDevice:device memoryOwner:descriptor.processIdentity ? descriptor.processIdentity->taskIdToken() : 0];
    WKBridgeImageAsset *diffuseAsset = WebModel::convert(descriptor.diffuseTexture);
    WKBridgeImageAsset *specularAsset = WebModel::convert(descriptor.specularTexture);
    if (configuration) {
        BinarySemaphore completion;
        [configuration createMaterialCompiler:[&completion] mutable {
            completion.signal();
        }];
        completion.wait();
    }

    NSError *error;
    m_receiver = [WebKit::allocWKBridgeReceiverInstance() initWithConfiguration:configuration diffuseAsset:diffuseAsset specularAsset:specularAsset error:&error];
    if (error)
        WTFLogAlways("Could not initialize USD renderer"); // NOLINT

    m_meshIdentifier = [[NSUUID alloc] init];
#endif
}

bool WebMesh::isValid() const
{
    return true;
}

WebMesh::~WebMesh() = default;

void WebMesh::render(uint32_t textureIndex, Function<void(bool)>&& completionHandler) const
{
#if ENABLE(GPU_PROCESS_MODEL)
    if (!m_meshDataExists) {
        completionHandler(false);
        return;
    }

    auto texture = ^(uint32_t textureIndex) {
        return [m_textures count] > textureIndex ? [m_textures objectAtIndex:textureIndex] : nil;
    };

    if (id<MTLTexture> modelBacking = texture(textureIndex)) {
        id<MTLCommandBuffer> commandBuffer = [m_receiver commandBuffer];
        [commandBuffer addCompletedHandler:makeBlockPtr([completionHandler = WTF::move(completionHandler)] (id<MTLCommandBuffer> mtlCommandBuffer) mutable {
            completionHandler(mtlCommandBuffer.status == MTLCommandBufferStatusCompleted);
        }).get()];
        [m_receiver renderWithTexture:modelBacking commandBuffer:commandBuffer];
    } else
        completionHandler(false);
#endif
}

#if ENABLE(GPU_PROCESS_MODEL)
static WKBridgeUpdateMesh *convert(const WebModel::UpdateMeshDescriptor& input)
{
    return [WebKit::allocWKBridgeUpdateMeshInstance() initWithIdentifier:convert(input.identifier)
        updateType:static_cast<WKBridgeDataUpdateType>(input.updateType)
        descriptor:WebModel::convert(input.descriptor)
        parts:WebModel::convert(input.parts)
        indexData:WebModel::convert(input.indexData)
        vertexData:WebModel::convert(input.vertexData)
        instanceTransforms:WebModel::convert(input.instanceTransforms)
        instanceTransformsCount:input.instanceTransforms.size()
        assignedMaterials:WebModel::convert(input.assignedMaterials)
        deformationData:WebModel::convert(input.deformationData)];
}
#endif

void WebMesh::update(Vector<WebModel::UpdateMeshDescriptor>&& inputArray)
{
#if ENABLE(GPU_PROCESS_MODEL)
    if (!inputArray.size())
        return;

    RELEASE_ASSERT(m_receiver);
    BinarySemaphore completion;
    [m_receiver updateMesh:createNSArray(inputArray, [](const WebModel::UpdateMeshDescriptor& desc) {
        return convert(desc);
    }) completionHandler:[&] mutable {
        completion.signal();
    }];
    completion.wait();
    m_meshDataExists = true;
#else
    UNUSED_PARAM(inputArray);
#endif
}

#if ENABLE(GPU_PROCESS_MODEL)
static WKBridgeUpdateTexture *convert(const WebModel::UpdateTextureDescriptor& input)
{
    return [WebKit::allocWKBridgeUpdateTextureInstance() initWithImageAsset:WebModel::convert(input.imageAsset) identifier:convert(input.identifier) hashString:input.hashString.createNSString().get() layout:createNSArray(input.layout, [](const WebModel::TextureLevelInfo& desc) {
        return WebModel::convert(desc);
    })];
}
#endif

void WebMesh::updateTexture(Vector<WebModel::UpdateTextureDescriptor>&& inputArray)
{
#if ENABLE(GPU_PROCESS_MODEL)
    RELEASE_ASSERT(m_receiver);
    [m_receiver updateTexture:createNSArray(inputArray, [](const WebModel::UpdateTextureDescriptor& desc) {
        return convert(desc);
    })];
#else
    UNUSED_PARAM(inputArray);
#endif
}

#if ENABLE(GPU_PROCESS_MODEL)
static WKBridgeUpdateMaterial *convert(const WebModel::UpdateMaterialDescriptor& input)
{
    return [WebKit::allocWKBridgeUpdateMaterialInstance() initWithMaterialGraph:WebModel::convert(input.materialGraph) identifier:convert(input.identifier)];
}
#endif

void WebMesh::updateMaterial(Vector<WebModel::UpdateMaterialDescriptor>&& inputArray)
{
#if ENABLE(GPU_PROCESS_MODEL)
    RELEASE_ASSERT(m_receiver);
    BinarySemaphore completion;
    [m_receiver updateMaterial:createNSArray(inputArray, [](const WebModel::UpdateMaterialDescriptor& desc) {
        return convert(desc);
    }) completionHandler:[&] mutable {
        completion.signal();
    }];
    completion.wait();
#else
    UNUSED_PARAM(inputArray);
#endif
}

void WebMesh::setTransform(const simd_float4x4& transform)
{
#if ENABLE(GPU_PROCESS_MODEL)
    m_transform = transform;
    [m_receiver setTransform:transform];
#else
    UNUSED_PARAM(transform);
#endif
}

void WebMesh::setFOV(float fovY)
{
#if ENABLE(GPU_PROCESS_MODEL)
    [m_receiver setFOV:fovY];
#else
    UNUSED_PARAM(fovY);
#endif
}

void WebMesh::setBackgroundColor(const simd_float3& color)
{
#if ENABLE(GPU_PROCESS_MODEL)
    [m_receiver setBackgroundColor:color];
#else
    UNUSED_PARAM(color);
#endif
}

void WebMesh::setEnvironmentMap(const WebModel::UpdateTextureDescriptor& imageAsset)
{
#if ENABLE(GPU_PROCESS_MODEL)
    [m_receiver setEnvironmentMap:convert(imageAsset)];
#else
    UNUSED_PARAM(imageAsset);
#endif
}

void WebMesh::play(bool play)
{
#if ENABLE(GPU_PROCESS_MODEL)
    [m_receiver setPlaying:play];
#else
    UNUSED_PARAM(play);
#endif
}

void WebMesh::updateRenderBuffers(const WebModel::ResizeMeshDescriptor& descriptor)
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto ioSurfaces = descriptor.renderBuffers.map([](auto& buffer) -> RetainPtr<IOSurfaceRef> {
        return buffer->surface();
    });
    m_textures = createMetalTextures(MTLCreateSystemDefaultDevice(), ioSurfaces, descriptor.width, descriptor.height);
#else
    UNUSED_PARAM(descriptor);
#endif
}

void WebMesh::processRemovals(Vector<WebModel::TypedResourceId>&& meshRemovals, Vector<WebModel::TypedResourceId>&& materialRemovals, Vector<WebModel::TypedResourceId>&& textureRemovals, CompletionHandler<void(bool)>&& completionHandler)
{
#if ENABLE(GPU_PROCESS_MODEL)
    completionHandler(!![m_receiver processRemovals:WebModel::convert(meshRemovals) materialRemovals:WebModel::convert(materialRemovals) textureRemovals:WebModel::convert(textureRemovals)]);
#else
    UNUSED_PARAM(meshRemovals);
    UNUSED_PARAM(materialRemovals);
    UNUSED_PARAM(textureRemovals);
    completionHandler(false);
#endif
}

} // namespace WebKit
