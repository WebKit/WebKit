//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// mtl_utils.mm:
//    Implements utilities functions that create Metal shaders, convert from angle enums
//    to Metal enums and so on.
//

#include "libANGLE/renderer/metal/mtl_utils.h"

#include <TargetConditionals.h>

#include "common/MemoryBuffer.h"
#include "libANGLE/renderer/metal/ContextMtl.h"

namespace rx
{
namespace mtl
{

angle::Result InitializeTextureContents(const gl::Context *context,
                                        const TextureRef &texture,
                                        const Format &textureObjFormat,
                                        const gl::ImageIndex &index)
{
    ASSERT(texture && texture->valid());
    ASSERT(texture->textureType() == MTLTextureType2D ||
           texture->textureType() == MTLTextureTypeCube);
    ContextMtl *contextMtl = mtl::GetImpl(context);

    const gl::InternalFormat &intendedInternalFormat = textureObjFormat.intendedInternalFormat();

    // This function is called in many places to initialize the content of a texture.
    // So it's better we do the sanity check here instead of let the callers do it themselves:
    if (!textureObjFormat.valid() || intendedInternalFormat.compressed ||
        intendedInternalFormat.depthBits > 0 || intendedInternalFormat.stencilBits > 0)
    {
        return angle::Result::Continue;
    }

    gl::Extents size = texture->size(index);

    // Intialize the content to black
    const angle::Format &srcFormat =
        angle::Format::Get(intendedInternalFormat.alphaBits > 0 ? angle::FormatID::R8G8B8A8_UNORM
                                                                : angle::FormatID::R8G8B8_UNORM);
    const size_t srcRowPitch = srcFormat.pixelBytes * size.width;
    angle::MemoryBuffer srcRow;
    ANGLE_CHECK_GL_ALLOC(contextMtl, srcRow.resize(srcRowPitch));
    memset(srcRow.data(), 0, srcRowPitch);

    const angle::Format &dstFormat = angle::Format::Get(textureObjFormat.actualFormatId);
    const size_t dstRowPitch       = dstFormat.pixelBytes * size.width;
    angle::MemoryBuffer conversionRow;
    ANGLE_CHECK_GL_ALLOC(contextMtl, conversionRow.resize(dstRowPitch));

    CopyImageCHROMIUM(srcRow.data(), srcRowPitch, srcFormat.pixelBytes, 0,
                      srcFormat.pixelReadFunction, conversionRow.data(), dstRowPitch,
                      dstFormat.pixelBytes, 0, dstFormat.pixelWriteFunction,
                      intendedInternalFormat.format, dstFormat.componentType, size.width, 1, 1,
                      false, false, false);

    auto mtlRowRegion = MTLRegionMake2D(0, 0, size.width, 1);

    for (NSUInteger r = 0; r < static_cast<NSUInteger>(size.height); ++r)
    {
        mtlRowRegion.origin.y = r;

        // Upload to texture
        texture->replaceRegion(contextMtl, mtlRowRegion, index.getLevelIndex(),
                               index.hasLayer() ? index.cubeMapFaceIndex() : 0,
                               conversionRow.data(), dstRowPitch);
    }

    return angle::Result::Continue;
}

MTLViewport GetViewport(const gl::Rectangle &rect, double znear, double zfar)
{
    MTLViewport re;

    re.originX = rect.x;
    re.originY = rect.y;
    re.width   = rect.width;
    re.height  = rect.height;
    re.znear   = znear;
    re.zfar    = zfar;

    return re;
}

MTLViewport GetViewportFlipY(const gl::Rectangle &rect,
                             NSUInteger screenHeight,
                             double znear,
                             double zfar)
{
    MTLViewport re;

    re.originX = rect.x;
    re.originY = static_cast<double>(screenHeight) - rect.y1();
    re.width   = rect.width;
    re.height  = rect.height;
    re.znear   = znear;
    re.zfar    = zfar;

    return re;
}

MTLViewport GetViewport(const gl::Rectangle &rect,
                        NSUInteger screenHeight,
                        bool flipY,
                        double znear,
                        double zfar)
{
    if (flipY)
    {
        return GetViewportFlipY(rect, screenHeight, znear, zfar);
    }

    return GetViewport(rect, znear, zfar);
}

MTLScissorRect GetScissorRect(const gl::Rectangle &rect, NSUInteger screenHeight, bool flipY)
{
    MTLScissorRect re;

    re.x      = rect.x;
    re.y      = flipY ? (screenHeight - rect.y1()) : rect.y;
    re.width  = rect.width;
    re.height = rect.height;

    return re;
}

AutoObjCPtr<id<MTLLibrary>> CreateShaderLibrary(id<MTLDevice> metalDevice,
                                                const std::string &source,
                                                AutoObjCPtr<NSError *> *error)
{
    return CreateShaderLibrary(metalDevice, source.c_str(), source.size(), error);
}

AutoObjCPtr<id<MTLLibrary>> CreateShaderLibrary(id<MTLDevice> metalDevice,
                                                const char *source,
                                                size_t sourceLen,
                                                AutoObjCPtr<NSError *> *errorOut)
{
    ANGLE_MTL_OBJC_SCOPE
    {
        NSError *nsError = nil;
        auto nsSource    = [[NSString alloc] initWithBytesNoCopy:const_cast<char *>(source)
                                                       length:sourceLen
                                                     encoding:NSUTF8StringEncoding
                                                 freeWhenDone:NO];
        auto options     = [[[MTLCompileOptions alloc] init] ANGLE_MTL_AUTORELEASE];
        auto library = [metalDevice newLibraryWithSource:nsSource options:options error:&nsError];

        [nsSource ANGLE_MTL_AUTORELEASE];

        *errorOut = std::move(nsError);

        return [library ANGLE_MTL_AUTORELEASE];
    }
}

AutoObjCPtr<id<MTLLibrary>> CreateShaderLibraryFromBinary(id<MTLDevice> metalDevice,
                                                          const uint8_t *binarySource,
                                                          size_t binarySourceLen,
                                                          AutoObjCPtr<NSError *> *errorOut)
{
    ANGLE_MTL_OBJC_SCOPE
    {
        NSError *nsError = nil;
        auto shaderSourceData =
            dispatch_data_create(binarySource, binarySourceLen, dispatch_get_main_queue(),
                                 ^{
                                 });

        auto library = [metalDevice newLibraryWithData:shaderSourceData error:&nsError];

        [shaderSourceData ANGLE_MTL_AUTORELEASE];

        *errorOut = std::move(nsError);

        return [library ANGLE_MTL_AUTORELEASE];
    }
}

MTLTextureType GetTextureType(gl::TextureType glType)
{
    switch (glType)
    {
        case gl::TextureType::_2D:
            return MTLTextureType2D;
        case gl::TextureType::CubeMap:
            return MTLTextureTypeCube;
        default:
            return MTLTextureTypeInvalid;
    }
}

MTLSamplerMinMagFilter GetFilter(GLenum filter)
{
    switch (filter)
    {
        case GL_LINEAR_MIPMAP_LINEAR:
        case GL_LINEAR_MIPMAP_NEAREST:
        case GL_LINEAR:
            return MTLSamplerMinMagFilterLinear;
        case GL_NEAREST_MIPMAP_LINEAR:
        case GL_NEAREST_MIPMAP_NEAREST:
        case GL_NEAREST:
            return MTLSamplerMinMagFilterNearest;
        default:
            UNIMPLEMENTED();
            return MTLSamplerMinMagFilterNearest;
    }
}

MTLSamplerMipFilter GetMipmapFilter(GLenum filter)
{
    switch (filter)
    {
        case GL_LINEAR:
        case GL_NEAREST:
            return MTLSamplerMipFilterNotMipmapped;
        case GL_LINEAR_MIPMAP_LINEAR:
        case GL_NEAREST_MIPMAP_LINEAR:
            return MTLSamplerMipFilterLinear;
        case GL_NEAREST_MIPMAP_NEAREST:
        case GL_LINEAR_MIPMAP_NEAREST:
            return MTLSamplerMipFilterNearest;
        default:
            UNIMPLEMENTED();
            return MTLSamplerMipFilterNotMipmapped;
    }
}

MTLSamplerAddressMode GetSamplerAddressMode(GLenum wrap)
{
    switch (wrap)
    {
        case GL_REPEAT:
            return MTLSamplerAddressModeRepeat;
        case GL_MIRRORED_REPEAT:
            return MTLSamplerAddressModeMirrorRepeat;
        case GL_CLAMP_TO_BORDER:
            // ES doesn't have border support
            return MTLSamplerAddressModeClampToEdge;
        case GL_CLAMP_TO_EDGE:
            return MTLSamplerAddressModeClampToEdge;
        default:
            UNIMPLEMENTED();
            return MTLSamplerAddressModeClampToEdge;
    }
}

MTLBlendFactor GetBlendFactor(GLenum factor)
{
    switch (factor)
    {
        case GL_ZERO:
            return MTLBlendFactorZero;
        case GL_ONE:
            return MTLBlendFactorOne;
        case GL_SRC_COLOR:
            return MTLBlendFactorSourceColor;
        case GL_DST_COLOR:
            return MTLBlendFactorDestinationColor;
        case GL_ONE_MINUS_SRC_COLOR:
            return MTLBlendFactorOneMinusSourceColor;
        case GL_SRC_ALPHA:
            return MTLBlendFactorSourceAlpha;
        case GL_ONE_MINUS_SRC_ALPHA:
            return MTLBlendFactorOneMinusSourceAlpha;
        case GL_DST_ALPHA:
            return MTLBlendFactorDestinationAlpha;
        case GL_ONE_MINUS_DST_ALPHA:
            return MTLBlendFactorOneMinusDestinationAlpha;
        case GL_ONE_MINUS_DST_COLOR:
            return MTLBlendFactorOneMinusDestinationColor;
        case GL_SRC_ALPHA_SATURATE:
            return MTLBlendFactorSourceAlphaSaturated;
        case GL_CONSTANT_COLOR:
            return MTLBlendFactorBlendColor;
        case GL_CONSTANT_ALPHA:
            return MTLBlendFactorBlendAlpha;
        case GL_ONE_MINUS_CONSTANT_COLOR:
            return MTLBlendFactorOneMinusBlendColor;
        case GL_ONE_MINUS_CONSTANT_ALPHA:
            return MTLBlendFactorOneMinusBlendAlpha;
        default:
            UNREACHABLE();
            return MTLBlendFactorZero;
    }
}

MTLBlendOperation GetBlendOp(GLenum op)
{
    switch (op)
    {
        case GL_FUNC_ADD:
            return MTLBlendOperationAdd;
        case GL_FUNC_SUBTRACT:
            return MTLBlendOperationSubtract;
        case GL_FUNC_REVERSE_SUBTRACT:
            return MTLBlendOperationReverseSubtract;
        case GL_MIN:
            return MTLBlendOperationMin;
        case GL_MAX:
            return MTLBlendOperationMax;
        default:
            UNREACHABLE();
            return MTLBlendOperationAdd;
    }
}

MTLCompareFunction GetCompareFunc(GLenum func)
{
    switch (func)
    {
        case GL_NEVER:
            return MTLCompareFunctionNever;
        case GL_ALWAYS:
            return MTLCompareFunctionAlways;
        case GL_LESS:
            return MTLCompareFunctionLess;
        case GL_LEQUAL:
            return MTLCompareFunctionLessEqual;
        case GL_EQUAL:
            return MTLCompareFunctionEqual;
        case GL_GREATER:
            return MTLCompareFunctionGreater;
        case GL_GEQUAL:
            return MTLCompareFunctionGreaterEqual;
        case GL_NOTEQUAL:
            return MTLCompareFunctionNotEqual;
        default:
            UNREACHABLE();
            return MTLCompareFunctionAlways;
    }
}

MTLStencilOperation GetStencilOp(GLenum op)
{
    switch (op)
    {
        case GL_KEEP:
            return MTLStencilOperationKeep;
        case GL_ZERO:
            return MTLStencilOperationZero;
        case GL_REPLACE:
            return MTLStencilOperationReplace;
        case GL_INCR:
            return MTLStencilOperationIncrementClamp;
        case GL_DECR:
            return MTLStencilOperationDecrementClamp;
        case GL_INCR_WRAP:
            return MTLStencilOperationIncrementWrap;
        case GL_DECR_WRAP:
            return MTLStencilOperationDecrementWrap;
        case GL_INVERT:
            return MTLStencilOperationInvert;
        default:
            UNREACHABLE();
            return MTLStencilOperationKeep;
    }
}

MTLWinding GetFontfaceWinding(GLenum frontFaceMode, bool invert)
{
    switch (frontFaceMode)
    {
        case GL_CW:
            return invert ? MTLWindingCounterClockwise : MTLWindingClockwise;
        case GL_CCW:
            return invert ? MTLWindingClockwise : MTLWindingCounterClockwise;
        default:
            UNREACHABLE();
            return MTLWindingClockwise;
    }
}

#if ANGLE_MTL_PRIMITIVE_TOPOLOGY_CLASS_AVAILABLE
PrimitiveTopologyClass GetPrimitiveTopologyClass(gl::PrimitiveMode mode)
{
    // NOTE(hqle): Support layered renderring in future.
    // In non-layered rendering mode, unspecified is enough.
    return MTLPrimitiveTopologyClassUnspecified;
}
#else  // ANGLE_MTL_PRIMITIVE_TOPOLOGY_CLASS_AVAILABLE
PrimitiveTopologyClass GetPrimitiveTopologyClass(gl::PrimitiveMode mode)
{
    return kPrimitiveTopologyClassTriangle;
}
#endif

MTLPrimitiveType GetPrimitiveType(gl::PrimitiveMode mode)
{
    switch (mode)
    {
        case gl::PrimitiveMode::Triangles:
            return MTLPrimitiveTypeTriangle;
        case gl::PrimitiveMode::Points:
            return MTLPrimitiveTypePoint;
        case gl::PrimitiveMode::Lines:
            return MTLPrimitiveTypeLine;
        case gl::PrimitiveMode::LineStrip:
        case gl::PrimitiveMode::LineLoop:
            return MTLPrimitiveTypeLineStrip;
        case gl::PrimitiveMode::TriangleStrip:
            return MTLPrimitiveTypeTriangleStrip;
        case gl::PrimitiveMode::TriangleFan:
            // NOTE(hqle): Emulate triangle fan.
        default:
            return MTLPrimitiveTypeInvalid;
    }
}

MTLIndexType GetIndexType(gl::DrawElementsType type)
{
    switch (type)
    {
        case gl::DrawElementsType::UnsignedShort:
            return MTLIndexTypeUInt16;
        case gl::DrawElementsType::UnsignedInt:
            return MTLIndexTypeUInt32;
        case gl::DrawElementsType::UnsignedByte:
            // NOTE(hqle): Convert to supported type
        default:
            return MTLIndexTypeInvalid;
    }
}

MTLClearColor EmulatedAlphaClearColor(MTLClearColor color, MTLColorWriteMask colorMask)
{
    MTLClearColor re = color;

    if (!(colorMask & MTLColorWriteMaskAlpha))
    {
        re.alpha = kEmulatedAlphaValue;
    }

    return re;
}

}  // namespace mtl
}  // namespace rx
