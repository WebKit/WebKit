//
// Copyright 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationES2.cpp: Validation functions for OpenGL ES 2.0 entry point parameters

#include "libANGLE/validationES2_autogen.h"

#include <cstdint>

#include "common/mathutil.h"
#include "common/string_utils.h"
#include "common/utilities.h"
#include "libANGLE/Context.h"
#include "libANGLE/ErrorStrings.h"
#include "libANGLE/Fence.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/MemoryObject.h"
#include "libANGLE/Renderbuffer.h"
#include "libANGLE/Shader.h"
#include "libANGLE/Texture.h"
#include "libANGLE/Uniform.h"
#include "libANGLE/VertexArray.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/validationES.h"
#include "libANGLE/validationES2.h"
#include "libANGLE/validationES3_autogen.h"

namespace gl
{
using namespace err;

namespace
{

bool IsPartialBlit(const Context *context,
                   const FramebufferAttachment *readBuffer,
                   const FramebufferAttachment *writeBuffer,
                   GLint srcX0,
                   GLint srcY0,
                   GLint srcX1,
                   GLint srcY1,
                   GLint dstX0,
                   GLint dstY0,
                   GLint dstX1,
                   GLint dstY1)
{
    const Extents &writeSize = writeBuffer->getSize();
    const Extents &readSize  = readBuffer->getSize();

    if (srcX0 != 0 || srcY0 != 0 || dstX0 != 0 || dstY0 != 0 || dstX1 != writeSize.width ||
        dstY1 != writeSize.height || srcX1 != readSize.width || srcY1 != readSize.height)
    {
        return true;
    }

    if (context->getState().isScissorTestEnabled())
    {
        const Rectangle &scissor = context->getState().getScissor();
        return scissor.x > 0 || scissor.y > 0 || scissor.width < writeSize.width ||
               scissor.height < writeSize.height;
    }

    return false;
}

bool IsValidCopyTextureSourceInternalFormatEnum(GLenum internalFormat)
{
    // Table 1.1 from the CHROMIUM_copy_texture spec
    switch (GetUnsizedFormat(internalFormat))
    {
        case GL_RED:
        case GL_ALPHA:
        case GL_LUMINANCE:
        case GL_LUMINANCE_ALPHA:
        case GL_RGB:
        case GL_RGBA:
        case GL_RGB8:
        case GL_RGBA8:
        case GL_BGRA_EXT:
        case GL_BGRA8_EXT:
            return true;

        default:
            return false;
    }
}

bool IsValidCopySubTextureSourceInternalFormat(GLenum internalFormat)
{
    return IsValidCopyTextureSourceInternalFormatEnum(internalFormat);
}

bool IsValidCopyTextureDestinationInternalFormatEnum(GLint internalFormat)
{
    // Table 1.0 from the CHROMIUM_copy_texture spec
    switch (internalFormat)
    {
        case GL_RGB:
        case GL_RGBA:
        case GL_RGB8:
        case GL_RGBA8:
        case GL_BGRA_EXT:
        case GL_BGRA8_EXT:
        case GL_SRGB_EXT:
        case GL_SRGB_ALPHA_EXT:
        case GL_R8:
        case GL_R8UI:
        case GL_RG8:
        case GL_RG8UI:
        case GL_SRGB8:
        case GL_RGB565:
        case GL_RGB8UI:
        case GL_RGB10_A2:
        case GL_SRGB8_ALPHA8:
        case GL_RGB5_A1:
        case GL_RGBA4:
        case GL_RGBA8UI:
        case GL_RGB9_E5:
        case GL_R16F:
        case GL_R32F:
        case GL_RG16F:
        case GL_RG32F:
        case GL_RGB16F:
        case GL_RGB32F:
        case GL_RGBA16F:
        case GL_RGBA32F:
        case GL_R11F_G11F_B10F:
        case GL_LUMINANCE:
        case GL_LUMINANCE_ALPHA:
        case GL_ALPHA:
            return true;

        default:
            return false;
    }
}

bool IsValidCopySubTextureDestionationInternalFormat(GLenum internalFormat)
{
    return IsValidCopyTextureDestinationInternalFormatEnum(internalFormat);
}

bool IsValidCopyTextureDestinationFormatType(const Context *context,
                                             GLint internalFormat,
                                             GLenum type)
{
    if (!IsValidCopyTextureDestinationInternalFormatEnum(internalFormat))
    {
        context->validationError(GL_INVALID_OPERATION, kInvalidInternalFormat);
        return false;
    }

    if (!ValidES3FormatCombination(GetUnsizedFormat(internalFormat), type, internalFormat))
    {
        context->validationError(GL_INVALID_OPERATION, kMismatchedTypeAndFormat);
        return false;
    }

    const InternalFormat &internalFormatInfo = GetInternalFormatInfo(internalFormat, type);
    if (!internalFormatInfo.textureSupport(context->getClientVersion(), context->getExtensions()))
    {
        context->validationError(GL_INVALID_OPERATION, kInvalidInternalFormat);
        return false;
    }

    return true;
}

bool IsValidCopyTextureDestinationTargetEnum(const Context *context, TextureTarget target)
{
    switch (target)
    {
        case TextureTarget::_2D:
        case TextureTarget::CubeMapNegativeX:
        case TextureTarget::CubeMapNegativeY:
        case TextureTarget::CubeMapNegativeZ:
        case TextureTarget::CubeMapPositiveX:
        case TextureTarget::CubeMapPositiveY:
        case TextureTarget::CubeMapPositiveZ:
            return true;

        case TextureTarget::Rectangle:
            return context->getExtensions().textureRectangle;

        default:
            return false;
    }
}

bool IsValidCopyTextureDestinationTarget(const Context *context,
                                         TextureType textureType,
                                         TextureTarget target)
{
    return TextureTargetToType(target) == textureType;
}

bool IsValidCopyTextureSourceTarget(const Context *context, TextureType type)
{
    switch (type)
    {
        case TextureType::_2D:
            return true;
        case TextureType::Rectangle:
            return context->getExtensions().textureRectangle;
        case TextureType::External:
            return context->getExtensions().eglImageExternalOES;
        case TextureType::VideoImage:
            return context->getExtensions().webglVideoTexture;
        default:
            return false;
    }
}

bool IsValidCopyTextureSourceLevel(const Context *context, TextureType type, GLint level)
{
    if (!ValidMipLevel(context, type, level))
    {
        return false;
    }

    if (level > 0 && context->getClientVersion() < ES_3_0)
    {
        return false;
    }

    return true;
}

bool IsValidCopyTextureDestinationLevel(const Context *context,
                                        TextureType type,
                                        GLint level,
                                        GLsizei width,
                                        GLsizei height,
                                        bool isSubImage)
{
    if (!ValidMipLevel(context, type, level))
    {
        return false;
    }

    if (!ValidImageSizeParameters(context, type, level, width, height, 1, isSubImage))
    {
        return false;
    }

    const Caps &caps = context->getCaps();
    switch (type)
    {
        case TextureType::_2D:
            return width <= (caps.max2DTextureSize >> level) &&
                   height <= (caps.max2DTextureSize >> level);
        case TextureType::Rectangle:
            ASSERT(level == 0);
            return width <= (caps.max2DTextureSize >> level) &&
                   height <= (caps.max2DTextureSize >> level);

        case TextureType::CubeMap:
            return width <= (caps.maxCubeMapTextureSize >> level) &&
                   height <= (caps.maxCubeMapTextureSize >> level);
        default:
            return true;
    }
}

bool IsValidStencilFunc(GLenum func)
{
    switch (func)
    {
        case GL_NEVER:
        case GL_ALWAYS:
        case GL_LESS:
        case GL_LEQUAL:
        case GL_EQUAL:
        case GL_GEQUAL:
        case GL_GREATER:
        case GL_NOTEQUAL:
            return true;

        default:
            return false;
    }
}

bool IsValidStencilFace(GLenum face)
{
    switch (face)
    {
        case GL_FRONT:
        case GL_BACK:
        case GL_FRONT_AND_BACK:
            return true;

        default:
            return false;
    }
}

bool IsValidStencilOp(GLenum op)
{
    switch (op)
    {
        case GL_ZERO:
        case GL_KEEP:
        case GL_REPLACE:
        case GL_INCR:
        case GL_DECR:
        case GL_INVERT:
        case GL_INCR_WRAP:
        case GL_DECR_WRAP:
            return true;

        default:
            return false;
    }
}

bool ValidateES2CopyTexImageParameters(const Context *context,
                                       TextureTarget target,
                                       GLint level,
                                       GLenum internalformat,
                                       bool isSubImage,
                                       GLint xoffset,
                                       GLint yoffset,
                                       GLint x,
                                       GLint y,
                                       GLsizei width,
                                       GLsizei height,
                                       GLint border)
{
    if (!ValidTexture2DDestinationTarget(context, target))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidTextureTarget);
        return false;
    }

    TextureType texType = TextureTargetToType(target);
    if (!ValidImageSizeParameters(context, texType, level, width, height, 1, isSubImage))
    {
        // Error is already handled.
        return false;
    }

    Format textureFormat = Format::Invalid();
    if (!ValidateCopyTexImageParametersBase(context, target, level, internalformat, isSubImage,
                                            xoffset, yoffset, 0, x, y, width, height, border,
                                            &textureFormat))
    {
        return false;
    }

    const Framebuffer *framebuffer = context->getState().getReadFramebuffer();
    GLenum colorbufferFormat =
        framebuffer->getReadColorAttachment()->getFormat().info->sizedInternalFormat;
    const auto &formatInfo = *textureFormat.info;

    // [OpenGL ES 2.0.24] table 3.9
    if (isSubImage)
    {
        switch (formatInfo.format)
        {
            case GL_ALPHA:
                if (colorbufferFormat != GL_ALPHA8_EXT && colorbufferFormat != GL_RGBA4 &&
                    colorbufferFormat != GL_RGB5_A1 && colorbufferFormat != GL_RGBA8_OES &&
                    colorbufferFormat != GL_BGRA8_EXT && colorbufferFormat != GL_BGR5_A1_ANGLEX)
                {
                    context->validationError(GL_INVALID_OPERATION, kInvalidFormat);
                    return false;
                }
                break;
            case GL_LUMINANCE:
                if (colorbufferFormat != GL_R8_EXT && colorbufferFormat != GL_RG8_EXT &&
                    colorbufferFormat != GL_RGB565 && colorbufferFormat != GL_RGB8_OES &&
                    colorbufferFormat != GL_RGBA4 && colorbufferFormat != GL_RGB5_A1 &&
                    colorbufferFormat != GL_RGBA8_OES && colorbufferFormat != GL_BGRA8_EXT &&
                    colorbufferFormat != GL_BGR5_A1_ANGLEX)
                {
                    context->validationError(GL_INVALID_OPERATION, kInvalidFormat);
                    return false;
                }
                break;
            case GL_RED_EXT:
                if (colorbufferFormat != GL_R8_EXT && colorbufferFormat != GL_RG8_EXT &&
                    colorbufferFormat != GL_RGB565 && colorbufferFormat != GL_RGB8_OES &&
                    colorbufferFormat != GL_RGBA4 && colorbufferFormat != GL_RGB5_A1 &&
                    colorbufferFormat != GL_RGBA8_OES && colorbufferFormat != GL_R32F &&
                    colorbufferFormat != GL_RG32F && colorbufferFormat != GL_RGB32F &&
                    colorbufferFormat != GL_RGBA32F && colorbufferFormat != GL_BGRA8_EXT &&
                    colorbufferFormat != GL_BGR5_A1_ANGLEX)
                {
                    context->validationError(GL_INVALID_OPERATION, kInvalidFormat);
                    return false;
                }
                break;
            case GL_RG_EXT:
                if (colorbufferFormat != GL_RG8_EXT && colorbufferFormat != GL_RGB565 &&
                    colorbufferFormat != GL_RGB8_OES && colorbufferFormat != GL_RGBA4 &&
                    colorbufferFormat != GL_RGB5_A1 && colorbufferFormat != GL_RGBA8_OES &&
                    colorbufferFormat != GL_RG32F && colorbufferFormat != GL_RGB32F &&
                    colorbufferFormat != GL_RGBA32F && colorbufferFormat != GL_BGRA8_EXT &&
                    colorbufferFormat != GL_BGR5_A1_ANGLEX)
                {
                    context->validationError(GL_INVALID_OPERATION, kInvalidFormat);
                    return false;
                }
                break;
            case GL_RGB:
                if (colorbufferFormat != GL_RGB565 && colorbufferFormat != GL_RGB8_OES &&
                    colorbufferFormat != GL_RGBA4 && colorbufferFormat != GL_RGB5_A1 &&
                    colorbufferFormat != GL_RGBA8_OES && colorbufferFormat != GL_RGB32F &&
                    colorbufferFormat != GL_RGBA32F && colorbufferFormat != GL_BGRA8_EXT &&
                    colorbufferFormat != GL_BGR5_A1_ANGLEX)
                {
                    context->validationError(GL_INVALID_OPERATION, kInvalidFormat);
                    return false;
                }
                break;
            case GL_LUMINANCE_ALPHA:
            case GL_RGBA:
                if (colorbufferFormat != GL_RGBA4 && colorbufferFormat != GL_RGB5_A1 &&
                    colorbufferFormat != GL_RGBA8_OES && colorbufferFormat != GL_RGBA32F &&
                    colorbufferFormat != GL_BGRA8_EXT && colorbufferFormat != GL_BGR5_A1_ANGLEX)
                {
                    context->validationError(GL_INVALID_OPERATION, kInvalidFormat);
                    return false;
                }
                break;
            case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
            case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
            case GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE:
            case GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE:
            case GL_ETC1_RGB8_OES:
            case GL_ETC1_RGB8_LOSSY_DECODE_ANGLE:
            case GL_COMPRESSED_RGB8_LOSSY_DECODE_ETC2_ANGLE:
            case GL_COMPRESSED_SRGB8_LOSSY_DECODE_ETC2_ANGLE:
            case GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_LOSSY_DECODE_ETC2_ANGLE:
            case GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_LOSSY_DECODE_ETC2_ANGLE:
            case GL_COMPRESSED_RGBA_BPTC_UNORM_EXT:
            case GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_EXT:
            case GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_EXT:
            case GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_EXT:
                context->validationError(GL_INVALID_OPERATION, kInvalidFormat);
                return false;
            case GL_DEPTH_COMPONENT:
            case GL_DEPTH_STENCIL_OES:
                context->validationError(GL_INVALID_OPERATION, kInvalidFormat);
                return false;
            default:
                context->validationError(GL_INVALID_OPERATION, kInvalidFormat);
                return false;
        }

        if (formatInfo.type == GL_FLOAT && !context->getExtensions().textureFloatOES)
        {
            context->validationError(GL_INVALID_OPERATION, kInvalidFormat);
            return false;
        }
    }
    else
    {
        switch (internalformat)
        {
            case GL_ALPHA:
                if (colorbufferFormat != GL_ALPHA8_EXT && colorbufferFormat != GL_RGBA4 &&
                    colorbufferFormat != GL_RGB5_A1 && colorbufferFormat != GL_BGRA8_EXT &&
                    colorbufferFormat != GL_RGBA8_OES && colorbufferFormat != GL_BGR5_A1_ANGLEX)
                {
                    context->validationError(GL_INVALID_OPERATION, kInvalidFormat);
                    return false;
                }
                break;
            case GL_LUMINANCE:
                if (colorbufferFormat != GL_R8_EXT && colorbufferFormat != GL_RG8_EXT &&
                    colorbufferFormat != GL_RGB565 && colorbufferFormat != GL_RGB8_OES &&
                    colorbufferFormat != GL_RGBA4 && colorbufferFormat != GL_RGB5_A1 &&
                    colorbufferFormat != GL_BGRA8_EXT && colorbufferFormat != GL_RGBA8_OES &&
                    colorbufferFormat != GL_BGR5_A1_ANGLEX)
                {
                    context->validationError(GL_INVALID_OPERATION, kInvalidFormat);
                    return false;
                }
                break;
            case GL_RED_EXT:
                if (colorbufferFormat != GL_R8_EXT && colorbufferFormat != GL_RG8_EXT &&
                    colorbufferFormat != GL_RGB565 && colorbufferFormat != GL_RGB8_OES &&
                    colorbufferFormat != GL_RGBA4 && colorbufferFormat != GL_RGB5_A1 &&
                    colorbufferFormat != GL_BGRA8_EXT && colorbufferFormat != GL_RGBA8_OES &&
                    colorbufferFormat != GL_BGR5_A1_ANGLEX)
                {
                    context->validationError(GL_INVALID_OPERATION, kInvalidFormat);
                    return false;
                }
                break;
            case GL_RG_EXT:
                if (colorbufferFormat != GL_RG8_EXT && colorbufferFormat != GL_RGB565 &&
                    colorbufferFormat != GL_RGB8_OES && colorbufferFormat != GL_RGBA4 &&
                    colorbufferFormat != GL_RGB5_A1 && colorbufferFormat != GL_BGRA8_EXT &&
                    colorbufferFormat != GL_RGBA8_OES && colorbufferFormat != GL_BGR5_A1_ANGLEX)
                {
                    context->validationError(GL_INVALID_OPERATION, kInvalidFormat);
                    return false;
                }
                break;
            case GL_RGB:
                if (colorbufferFormat != GL_RGB565 && colorbufferFormat != GL_RGB8_OES &&
                    colorbufferFormat != GL_RGBA4 && colorbufferFormat != GL_RGB5_A1 &&
                    colorbufferFormat != GL_BGRA8_EXT && colorbufferFormat != GL_RGBA8_OES &&
                    colorbufferFormat != GL_BGR5_A1_ANGLEX)
                {
                    context->validationError(GL_INVALID_OPERATION, kInvalidFormat);
                    return false;
                }
                break;
            case GL_LUMINANCE_ALPHA:
            case GL_RGBA:
                if (colorbufferFormat != GL_RGBA4 && colorbufferFormat != GL_RGB5_A1 &&
                    colorbufferFormat != GL_BGRA8_EXT && colorbufferFormat != GL_RGBA8_OES &&
                    colorbufferFormat != GL_BGR5_A1_ANGLEX)
                {
                    context->validationError(GL_INVALID_OPERATION, kInvalidFormat);
                    return false;
                }
                break;
            case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
            case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
                if (context->getExtensions().textureCompressionDXT1)
                {
                    context->validationError(GL_INVALID_OPERATION, kInvalidFormat);
                    return false;
                }
                context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                return false;
            case GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE:
                if (context->getExtensions().textureCompressionDXT3)
                {
                    context->validationError(GL_INVALID_OPERATION, kInvalidFormat);
                    return false;
                }
                context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                return false;
            case GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE:
                if (context->getExtensions().textureCompressionDXT5)
                {
                    context->validationError(GL_INVALID_OPERATION, kInvalidFormat);
                    return false;
                }
                context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                return false;
            case GL_ETC1_RGB8_OES:
                if (context->getExtensions().compressedETC1RGB8TextureOES)
                {
                    context->validationError(GL_INVALID_OPERATION, kInvalidFormat);
                    return false;
                }
                context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                return false;
            case GL_ETC1_RGB8_LOSSY_DECODE_ANGLE:
            case GL_COMPRESSED_RGB8_LOSSY_DECODE_ETC2_ANGLE:
            case GL_COMPRESSED_SRGB8_LOSSY_DECODE_ETC2_ANGLE:
            case GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_LOSSY_DECODE_ETC2_ANGLE:
            case GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_LOSSY_DECODE_ETC2_ANGLE:
                if (context->getExtensions().lossyETCDecode)
                {
                    context->validationError(GL_INVALID_OPERATION, kInvalidFormat);
                    return false;
                }
                context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                return false;
            case GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG:
            case GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG:
            case GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG:
            case GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG:
                if (context->getExtensions().compressedTexturePVRTC)
                {
                    context->validationError(GL_INVALID_OPERATION, kInvalidFormat);
                    return false;
                }
                context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                return false;
            case GL_COMPRESSED_SRGB_PVRTC_2BPPV1_EXT:
            case GL_COMPRESSED_SRGB_PVRTC_4BPPV1_EXT:
            case GL_COMPRESSED_SRGB_ALPHA_PVRTC_2BPPV1_EXT:
            case GL_COMPRESSED_SRGB_ALPHA_PVRTC_4BPPV1_EXT:
                if (context->getExtensions().compressedTexturePVRTCsRGB)
                {
                    context->validationError(GL_INVALID_OPERATION, kInvalidFormat);
                    return false;
                }
                context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                return false;
            case GL_DEPTH_COMPONENT:
            case GL_DEPTH_COMPONENT16:
            case GL_DEPTH_COMPONENT32_OES:
                if (context->getExtensions().depthTextureAny())
                {
                    context->validationError(GL_INVALID_OPERATION, kInvalidFormat);
                    return false;
                }
                context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                return false;
            case GL_DEPTH_STENCIL_OES:
            case GL_DEPTH24_STENCIL8_OES:
                if (context->getExtensions().depthTextureAny() ||
                    context->getExtensions().packedDepthStencilOES ||
                    context->getExtensions().depthTextureCubeMapOES)
                {
                    context->validationError(GL_INVALID_OPERATION, kInvalidFormat);
                    return false;
                }
                context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                return false;
            default:
                context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                return false;
        }
    }

    // If width or height is zero, it is a no-op.  Return false without setting an error.
    return (width > 0 && height > 0);
}

bool ValidCap(const Context *context, GLenum cap, bool queryOnly)
{
    switch (cap)
    {
        // EXT_multisample_compatibility
        case GL_MULTISAMPLE_EXT:
        case GL_SAMPLE_ALPHA_TO_ONE_EXT:
            return context->getExtensions().multisampleCompatibility;

        case GL_CULL_FACE:
        case GL_POLYGON_OFFSET_FILL:
        case GL_SAMPLE_ALPHA_TO_COVERAGE:
        case GL_SAMPLE_COVERAGE:
        case GL_SCISSOR_TEST:
        case GL_STENCIL_TEST:
        case GL_DEPTH_TEST:
        case GL_BLEND:
        case GL_DITHER:
            return true;

        case GL_PRIMITIVE_RESTART_FIXED_INDEX:
        case GL_RASTERIZER_DISCARD:
            return (context->getClientMajorVersion() >= 3);

        case GL_DEBUG_OUTPUT_SYNCHRONOUS:
        case GL_DEBUG_OUTPUT:
            return context->getExtensions().debug;

        case GL_BIND_GENERATES_RESOURCE_CHROMIUM:
            return queryOnly && context->getExtensions().bindGeneratesResource;

        case GL_CLIENT_ARRAYS_ANGLE:
            return queryOnly && context->getExtensions().clientArrays;

        case GL_FRAMEBUFFER_SRGB_EXT:
            return context->getExtensions().sRGBWriteControl;

        case GL_SAMPLE_MASK:
            return context->getClientVersion() >= Version(3, 1);

        case GL_ROBUST_RESOURCE_INITIALIZATION_ANGLE:
            return queryOnly && context->getExtensions().robustResourceInitialization;

        case GL_TEXTURE_RECTANGLE_ANGLE:
            return context->getExtensions().webglCompatibility;

        // GL_APPLE_clip_distance/GL_EXT_clip_cull_distance
        case GL_CLIP_DISTANCE0_EXT:
        case GL_CLIP_DISTANCE1_EXT:
        case GL_CLIP_DISTANCE2_EXT:
        case GL_CLIP_DISTANCE3_EXT:
        case GL_CLIP_DISTANCE4_EXT:
        case GL_CLIP_DISTANCE5_EXT:
        case GL_CLIP_DISTANCE6_EXT:
        case GL_CLIP_DISTANCE7_EXT:
            if (context->getClientVersion() >= Version(2, 0) &&
                context->getExtensions().clipDistanceAPPLE)
            {
                return true;
            }
            break;
    }

    // GLES1 emulation: GLES1-specific caps after this point
    if (context->getClientVersion().major != 1)
    {
        return false;
    }

    switch (cap)
    {
        case GL_ALPHA_TEST:
        case GL_VERTEX_ARRAY:
        case GL_NORMAL_ARRAY:
        case GL_COLOR_ARRAY:
        case GL_TEXTURE_COORD_ARRAY:
        case GL_TEXTURE_2D:
        case GL_LIGHTING:
        case GL_LIGHT0:
        case GL_LIGHT1:
        case GL_LIGHT2:
        case GL_LIGHT3:
        case GL_LIGHT4:
        case GL_LIGHT5:
        case GL_LIGHT6:
        case GL_LIGHT7:
        case GL_NORMALIZE:
        case GL_RESCALE_NORMAL:
        case GL_COLOR_MATERIAL:
        case GL_CLIP_PLANE0:
        case GL_CLIP_PLANE1:
        case GL_CLIP_PLANE2:
        case GL_CLIP_PLANE3:
        case GL_CLIP_PLANE4:
        case GL_CLIP_PLANE5:
        case GL_FOG:
        case GL_POINT_SMOOTH:
        case GL_LINE_SMOOTH:
        case GL_COLOR_LOGIC_OP:
            return context->getClientVersion() < Version(2, 0);
        case GL_POINT_SIZE_ARRAY_OES:
            return context->getClientVersion() < Version(2, 0) &&
                   context->getExtensions().pointSizeArrayOES;
        case GL_TEXTURE_CUBE_MAP:
            return context->getClientVersion() < Version(2, 0) &&
                   context->getExtensions().textureCubeMapOES;
        case GL_POINT_SPRITE_OES:
            return context->getClientVersion() < Version(2, 0) &&
                   context->getExtensions().pointSpriteOES;
        default:
            return false;
    }
}

// Return true if a character belongs to the ASCII subset as defined in GLSL ES 1.0 spec section
// 3.1.
bool IsValidESSLCharacter(unsigned char c)
{
    // Printing characters are valid except " $ ` @ \ ' DEL.
    if (c >= 32 && c <= 126 && c != '"' && c != '$' && c != '`' && c != '@' && c != '\\' &&
        c != '\'')
    {
        return true;
    }

    // Horizontal tab, line feed, vertical tab, form feed, carriage return are also valid.
    if (c >= 9 && c <= 13)
    {
        return true;
    }

    return false;
}

bool IsValidESSLString(const char *str, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        if (!IsValidESSLCharacter(str[i]))
        {
            return false;
        }
    }

    return true;
}

bool IsValidESSLShaderSourceString(const char *str, size_t len, bool lineContinuationAllowed)
{
    enum class ParseState
    {
        // Have not seen an ASCII non-whitespace character yet on
        // this line. Possible that we might see a preprocessor
        // directive.
        BEGINING_OF_LINE,

        // Have seen at least one ASCII non-whitespace character
        // on this line.
        MIDDLE_OF_LINE,

        // Handling a preprocessor directive. Passes through all
        // characters up to the end of the line. Disables comment
        // processing.
        IN_PREPROCESSOR_DIRECTIVE,

        // Handling a single-line comment. The comment text is
        // replaced with a single space.
        IN_SINGLE_LINE_COMMENT,

        // Handling a multi-line comment. Newlines are passed
        // through to preserve line numbers.
        IN_MULTI_LINE_COMMENT
    };

    ParseState state = ParseState::BEGINING_OF_LINE;
    size_t pos       = 0;

    while (pos < len)
    {
        char c    = str[pos];
        char next = pos + 1 < len ? str[pos + 1] : 0;

        // Check for newlines
        if (c == '\n' || c == '\r')
        {
            if (state != ParseState::IN_MULTI_LINE_COMMENT)
            {
                state = ParseState::BEGINING_OF_LINE;
            }

            pos++;
            continue;
        }

        switch (state)
        {
            case ParseState::BEGINING_OF_LINE:
                if (c == ' ')
                {
                    // Maintain the BEGINING_OF_LINE state until a non-space is seen
                    pos++;
                }
                else if (c == '#')
                {
                    state = ParseState::IN_PREPROCESSOR_DIRECTIVE;
                    pos++;
                }
                else
                {
                    // Don't advance, re-process this character with the MIDDLE_OF_LINE state
                    state = ParseState::MIDDLE_OF_LINE;
                }
                break;

            case ParseState::MIDDLE_OF_LINE:
                if (c == '/' && next == '/')
                {
                    state = ParseState::IN_SINGLE_LINE_COMMENT;
                    pos++;
                }
                else if (c == '/' && next == '*')
                {
                    state = ParseState::IN_MULTI_LINE_COMMENT;
                    pos++;
                }
                else if (lineContinuationAllowed && c == '\\' && (next == '\n' || next == '\r'))
                {
                    // Skip line continuation characters
                }
                else if (!IsValidESSLCharacter(c))
                {
                    return false;
                }
                pos++;
                break;

            case ParseState::IN_PREPROCESSOR_DIRECTIVE:
                // Line-continuation characters may not be permitted.
                // Otherwise, just pass it through. Do not parse comments in this state.
                if (!lineContinuationAllowed && c == '\\')
                {
                    return false;
                }
                pos++;
                break;

            case ParseState::IN_SINGLE_LINE_COMMENT:
                // Line-continuation characters are processed before comment processing.
                // Advance string if a new line character is immediately behind
                // line-continuation character.
                if (c == '\\' && (next == '\n' || next == '\r'))
                {
                    pos++;
                }
                pos++;
                break;

            case ParseState::IN_MULTI_LINE_COMMENT:
                if (c == '*' && next == '/')
                {
                    state = ParseState::MIDDLE_OF_LINE;
                    pos++;
                }
                pos++;
                break;
        }
    }

    return true;
}

bool ValidateWebGLNamePrefix(const Context *context, const GLchar *name)
{
    ASSERT(context->isWebGL());

    // WebGL 1.0 [Section 6.16] GLSL Constructs
    // Identifiers starting with "webgl_" and "_webgl_" are reserved for use by WebGL.
    if (strncmp(name, "webgl_", 6) == 0 || strncmp(name, "_webgl_", 7) == 0)
    {
        context->validationError(GL_INVALID_OPERATION, kWebglBindAttribLocationReservedPrefix);
        return false;
    }

    return true;
}

bool ValidateWebGLNameLength(const Context *context, size_t length)
{
    ASSERT(context->isWebGL());

    if (context->isWebGL1() && length > 256)
    {
        // WebGL 1.0 [Section 6.21] Maxmimum Uniform and Attribute Location Lengths
        // WebGL imposes a limit of 256 characters on the lengths of uniform and attribute
        // locations.
        context->validationError(GL_INVALID_VALUE, kWebglNameLengthLimitExceeded);

        return false;
    }
    else if (length > 1024)
    {
        // WebGL 2.0 [Section 4.3.2] WebGL 2.0 imposes a limit of 1024 characters on the lengths of
        // uniform and attribute locations.
        context->validationError(GL_INVALID_VALUE, kWebgl2NameLengthLimitExceeded);
        return false;
    }

    return true;
}

bool ValidBlendFunc(const Context *context, GLenum val)
{
    const Extensions &ext = context->getExtensions();

    // these are always valid for src and dst.
    switch (val)
    {
        case GL_ZERO:
        case GL_ONE:
        case GL_SRC_COLOR:
        case GL_ONE_MINUS_SRC_COLOR:
        case GL_DST_COLOR:
        case GL_ONE_MINUS_DST_COLOR:
        case GL_SRC_ALPHA:
        case GL_ONE_MINUS_SRC_ALPHA:
        case GL_DST_ALPHA:
        case GL_ONE_MINUS_DST_ALPHA:
        case GL_CONSTANT_COLOR:
        case GL_ONE_MINUS_CONSTANT_COLOR:
        case GL_CONSTANT_ALPHA:
        case GL_ONE_MINUS_CONSTANT_ALPHA:
            return true;

        // EXT_blend_func_extended.
        case GL_SRC1_COLOR_EXT:
        case GL_SRC1_ALPHA_EXT:
        case GL_ONE_MINUS_SRC1_COLOR_EXT:
        case GL_ONE_MINUS_SRC1_ALPHA_EXT:
        case GL_SRC_ALPHA_SATURATE_EXT:
            return ext.blendFuncExtended;

        default:
            return false;
    }
}

bool ValidSrcBlendFunc(const Context *context, GLenum val)
{
    if (ValidBlendFunc(context, val))
        return true;

    if (val == GL_SRC_ALPHA_SATURATE)
        return true;

    return false;
}

bool ValidDstBlendFunc(const Context *context, GLenum val)
{
    if (ValidBlendFunc(context, val))
        return true;

    if (val == GL_SRC_ALPHA_SATURATE)
    {
        if (context->getClientMajorVersion() >= 3)
            return true;
    }

    return false;
}

bool IsValidImageLayout(ImageLayout layout)
{
    switch (layout)
    {
        case ImageLayout::Undefined:
        case ImageLayout::General:
        case ImageLayout::ColorAttachment:
        case ImageLayout::DepthStencilAttachment:
        case ImageLayout::DepthStencilReadOnlyAttachment:
        case ImageLayout::ShaderReadOnly:
        case ImageLayout::TransferSrc:
        case ImageLayout::TransferDst:
        case ImageLayout::DepthReadOnlyStencilAttachment:
        case ImageLayout::DepthAttachmentStencilReadOnly:
            return true;

        default:
            return false;
    }
}

bool ValidateES2TexImageParameters(const Context *context,
                                   TextureTarget target,
                                   GLint level,
                                   GLenum internalformat,
                                   bool isCompressed,
                                   bool isSubImage,
                                   GLint xoffset,
                                   GLint yoffset,
                                   GLsizei width,
                                   GLsizei height,
                                   GLint border,
                                   GLenum format,
                                   GLenum type,
                                   GLsizei imageSize,
                                   const void *pixels)
{
    if (!ValidTexture2DDestinationTarget(context, target))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidTextureTarget);
        return false;
    }

    return ValidateES2TexImageParametersBase(context, target, level, internalformat, isCompressed,
                                             isSubImage, xoffset, yoffset, width, height, border,
                                             format, type, imageSize, pixels);
}

bool IsValidMemoryObjectParamater(const Context *context, GLenum pname)
{
    switch (pname)
    {
        case GL_DEDICATED_MEMORY_OBJECT_EXT:
            return true;

        default:
            return false;
    }
}

}  // anonymous namespace

bool ValidateES2TexImageParametersBase(const Context *context,
                                       TextureTarget target,
                                       GLint level,
                                       GLenum internalformat,
                                       bool isCompressed,
                                       bool isSubImage,
                                       GLint xoffset,
                                       GLint yoffset,
                                       GLsizei width,
                                       GLsizei height,
                                       GLint border,
                                       GLenum format,
                                       GLenum type,
                                       GLsizei imageSize,
                                       const void *pixels)
{

    TextureType texType = TextureTargetToType(target);
    if (!ValidImageSizeParameters(context, texType, level, width, height, 1, isSubImage))
    {
        // Error already handled.
        return false;
    }

    if (!ValidMipLevel(context, texType, level))
    {
        context->validationError(GL_INVALID_VALUE, kInvalidMipLevel);
        return false;
    }

    if (xoffset < 0 || std::numeric_limits<GLsizei>::max() - xoffset < width ||
        std::numeric_limits<GLsizei>::max() - yoffset < height)
    {
        context->validationError(GL_INVALID_VALUE, kResourceMaxTextureSize);
        return false;
    }

    const Caps &caps = context->getCaps();

    switch (texType)
    {
        case TextureType::_2D:
        case TextureType::External:
        case TextureType::VideoImage:
            if (width > (caps.max2DTextureSize >> level) ||
                height > (caps.max2DTextureSize >> level))
            {
                context->validationError(GL_INVALID_VALUE, kResourceMaxTextureSize);
                return false;
            }
            break;

        case TextureType::Rectangle:
            ASSERT(level == 0);
            if (width > caps.maxRectangleTextureSize || height > caps.maxRectangleTextureSize)
            {
                context->validationError(GL_INVALID_VALUE, kResourceMaxTextureSize);
                return false;
            }
            if (isCompressed)
            {
                context->validationError(GL_INVALID_ENUM, kRectangleTextureCompressed);
                return false;
            }
            break;

        case TextureType::CubeMap:
            if (!isSubImage && width != height)
            {
                context->validationError(GL_INVALID_VALUE, kCubemapFacesEqualDimensions);
                return false;
            }

            if (width > (caps.maxCubeMapTextureSize >> level) ||
                height > (caps.maxCubeMapTextureSize >> level))
            {
                context->validationError(GL_INVALID_VALUE, kResourceMaxTextureSize);
                return false;
            }
            break;

        default:
            context->validationError(GL_INVALID_ENUM, kInvalidTextureTarget);
            return false;
    }

    Texture *texture = context->getTextureByType(texType);
    if (!texture)
    {
        context->validationError(GL_INVALID_OPERATION, kBufferNotBound);
        return false;
    }

    // Verify zero border
    if (border != 0)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidBorder);
        return false;
    }

    bool nonEqualFormatsAllowed = false;

    if (isCompressed)
    {
        GLenum actualInternalFormat =
            isSubImage ? texture->getFormat(target, level).info->sizedInternalFormat
                       : internalformat;

        const InternalFormat &internalFormatInfo = GetSizedInternalFormatInfo(actualInternalFormat);

        if (!internalFormatInfo.compressed)
        {
            context->validationError(GL_INVALID_ENUM, kInvalidInternalFormat);
            return false;
        }

        if (!internalFormatInfo.textureSupport(context->getClientVersion(),
                                               context->getExtensions()))
        {
            context->validationError(GL_INVALID_ENUM, kInvalidInternalFormat);
            return false;
        }

        if (isSubImage)
        {
            // From the OES_compressed_ETC1_RGB8_texture spec:
            //
            // INVALID_OPERATION is generated by CompressedTexSubImage2D, TexSubImage2D, or
            // CopyTexSubImage2D if the texture image <level> bound to <target> has internal format
            // ETC1_RGB8_OES.
            //
            // This is relaxed if GL_EXT_compressed_ETC1_RGB8_sub_texture is supported.
            if (actualInternalFormat == GL_ETC1_RGB8_OES &&
                !context->getExtensions().compressedETC1RGB8SubTexture)
            {
                context->validationError(GL_INVALID_OPERATION, kInvalidInternalFormat);
                return false;
            }

            if (!ValidCompressedSubImageSize(context, actualInternalFormat, xoffset, yoffset, 0,
                                             width, height, 1, texture->getWidth(target, level),
                                             texture->getHeight(target, level),
                                             texture->getDepth(target, level)))
            {
                context->validationError(GL_INVALID_OPERATION, kInvalidCompressedImageSize);
                return false;
            }

            if (format != actualInternalFormat)
            {
                context->validationError(GL_INVALID_OPERATION, kInvalidFormat);
                return false;
            }
        }
        else
        {
            if (!ValidCompressedImageSize(context, actualInternalFormat, level, width, height, 1))
            {
                context->validationError(GL_INVALID_OPERATION, kInvalidCompressedImageSize);
                return false;
            }
        }
    }
    else
    {
        // validate <type> by itself (used as secondary key below)
        switch (type)
        {
            case GL_UNSIGNED_BYTE:
            case GL_UNSIGNED_SHORT_5_6_5:
            case GL_UNSIGNED_SHORT_4_4_4_4:
            case GL_UNSIGNED_SHORT_5_5_5_1:
            case GL_UNSIGNED_SHORT:
            case GL_UNSIGNED_INT:
            case GL_UNSIGNED_INT_24_8_OES:
            case GL_HALF_FLOAT_OES:
            case GL_FLOAT:
                break;
            case GL_UNSIGNED_INT_2_10_10_10_REV_EXT:
                if (!context->getExtensions().textureFormat2101010REV)
                {
                    context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                    return false;
                }
                break;
            default:
                context->validationError(GL_INVALID_ENUM, kInvalidType);
                return false;
        }

        // validate <format> + <type> combinations
        // - invalid <format> -> sets INVALID_ENUM
        // - invalid <format>+<type> combination -> sets INVALID_OPERATION
        switch (format)
        {
            case GL_ALPHA:
            case GL_LUMINANCE:
            case GL_LUMINANCE_ALPHA:
                switch (type)
                {
                    case GL_UNSIGNED_BYTE:
                    case GL_FLOAT:
                    case GL_HALF_FLOAT_OES:
                        break;
                    default:
                        context->validationError(GL_INVALID_OPERATION, kMismatchedTypeAndFormat);
                        return false;
                }
                break;
            case GL_RED:
            case GL_RG:
                if (!context->getExtensions().textureRG)
                {
                    context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                    return false;
                }
                switch (type)
                {
                    case GL_UNSIGNED_BYTE:
                        break;
                    case GL_FLOAT:
                    case GL_HALF_FLOAT_OES:
                        if (!context->getExtensions().textureFloatOES)
                        {
                            context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                            return false;
                        }
                        break;
                    case GL_SHORT:
                    case GL_UNSIGNED_SHORT:
                        if (!context->getExtensions().textureNorm16)
                        {
                            context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                            return false;
                        }
                        break;
                    default:
                        context->validationError(GL_INVALID_OPERATION, kMismatchedTypeAndFormat);
                        return false;
                }
                break;
            case GL_RGB:
                switch (type)
                {
                    case GL_UNSIGNED_BYTE:
                    case GL_UNSIGNED_SHORT_5_6_5:
                    case GL_UNSIGNED_INT_2_10_10_10_REV_EXT:
                    case GL_FLOAT:
                    case GL_HALF_FLOAT_OES:
                        break;
                    case GL_SHORT:
                    case GL_UNSIGNED_SHORT:
                        if (!context->getExtensions().textureNorm16)
                        {
                            context->validationError(GL_INVALID_OPERATION,
                                                     kMismatchedTypeAndFormat);
                            return false;
                        }
                        break;
                    default:
                        context->validationError(GL_INVALID_OPERATION, kMismatchedTypeAndFormat);
                        return false;
                }
                break;
            case GL_RGBA:
                switch (type)
                {
                    case GL_UNSIGNED_BYTE:
                    case GL_UNSIGNED_SHORT_4_4_4_4:
                    case GL_UNSIGNED_SHORT_5_5_5_1:
                    case GL_FLOAT:
                    case GL_HALF_FLOAT_OES:
                    case GL_UNSIGNED_INT_2_10_10_10_REV_EXT:
                        break;
                    case GL_SHORT:
                    case GL_UNSIGNED_SHORT:
                        if (!context->getExtensions().textureNorm16)
                        {
                            context->validationError(GL_INVALID_OPERATION,
                                                     kMismatchedTypeAndFormat);
                            return false;
                        }
                        break;
                    default:
                        context->validationError(GL_INVALID_OPERATION, kMismatchedTypeAndFormat);
                        return false;
                }
                break;
            case GL_BGRA_EXT:
                if (!context->getExtensions().textureFormatBGRA8888)
                {
                    context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                    return false;
                }
                switch (type)
                {
                    case GL_UNSIGNED_BYTE:
                        break;
                    default:
                        context->validationError(GL_INVALID_OPERATION, kMismatchedTypeAndFormat);
                        return false;
                }
                break;
            case GL_SRGB_EXT:
            case GL_SRGB_ALPHA_EXT:
                if (!context->getExtensions().sRGB)
                {
                    context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                    return false;
                }
                switch (type)
                {
                    case GL_UNSIGNED_BYTE:
                        break;
                    default:
                        context->validationError(GL_INVALID_OPERATION, kMismatchedTypeAndFormat);
                        return false;
                }
                break;
            case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:  // error cases for compressed textures are
                                                   // handled below
            case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
            case GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE:
            case GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE:
                break;
            case GL_DEPTH_COMPONENT:
                switch (type)
                {
                    case GL_UNSIGNED_SHORT:
                    case GL_UNSIGNED_INT:
                        break;
                    case GL_FLOAT:
                        if (!context->getExtensions().depthBufferFloat2NV)
                        {
                            context->validationError(GL_INVALID_OPERATION,
                                                     kMismatchedTypeAndFormat);
                            return false;
                        }
                        break;
                    default:
                        context->validationError(GL_INVALID_OPERATION, kMismatchedTypeAndFormat);
                        return false;
                }
                break;
            case GL_DEPTH_STENCIL_OES:
                switch (type)
                {
                    case GL_UNSIGNED_INT_24_8_OES:
                        break;
                    default:
                        context->validationError(GL_INVALID_OPERATION, kMismatchedTypeAndFormat);
                        return false;
                }
                break;
            default:
                context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                return false;
        }

        switch (format)
        {
            case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
            case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
                if (context->getExtensions().textureCompressionDXT1)
                {
                    context->validationError(GL_INVALID_OPERATION, kInvalidFormat);
                    return false;
                }
                else
                {
                    context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                    return false;
                }
                break;
            case GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE:
                if (context->getExtensions().textureCompressionDXT3)
                {
                    context->validationError(GL_INVALID_OPERATION, kInvalidFormat);
                    return false;
                }
                context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                return false;
            case GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE:
                if (context->getExtensions().textureCompressionDXT5)
                {
                    context->validationError(GL_INVALID_OPERATION, kInvalidFormat);
                    return false;
                }
                context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                return false;
            case GL_ETC1_RGB8_OES:
                if (context->getExtensions().compressedETC1RGB8TextureOES)
                {
                    context->validationError(GL_INVALID_OPERATION, kInvalidFormat);
                    return false;
                }
                context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                return false;
            case GL_ETC1_RGB8_LOSSY_DECODE_ANGLE:
            case GL_COMPRESSED_RGB8_LOSSY_DECODE_ETC2_ANGLE:
            case GL_COMPRESSED_SRGB8_LOSSY_DECODE_ETC2_ANGLE:
            case GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_LOSSY_DECODE_ETC2_ANGLE:
            case GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_LOSSY_DECODE_ETC2_ANGLE:
                if (context->getExtensions().lossyETCDecode)
                {
                    context->validationError(GL_INVALID_OPERATION, kInvalidFormat);
                    return false;
                }
                context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                return false;
            case GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG:
            case GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG:
            case GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG:
            case GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG:
                if (context->getExtensions().compressedTexturePVRTC)
                {
                    context->validationError(GL_INVALID_OPERATION, kInvalidFormat);
                    return false;
                }
                context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                return false;
            case GL_COMPRESSED_SRGB_PVRTC_2BPPV1_EXT:
            case GL_COMPRESSED_SRGB_PVRTC_4BPPV1_EXT:
            case GL_COMPRESSED_SRGB_ALPHA_PVRTC_2BPPV1_EXT:
            case GL_COMPRESSED_SRGB_ALPHA_PVRTC_4BPPV1_EXT:
                if (context->getExtensions().compressedTexturePVRTCsRGB)
                {
                    context->validationError(GL_INVALID_OPERATION, kInvalidFormat);
                    return false;
                }
                context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                return false;
            case GL_DEPTH_COMPONENT:
            case GL_DEPTH_STENCIL_OES:
                if (!context->getExtensions().depthTextureANGLE &&
                    !((context->getExtensions().packedDepthStencilOES ||
                       context->getExtensions().depthTextureCubeMapOES) &&
                      context->getExtensions().depthTextureOES))
                {
                    context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                    return false;
                }

                switch (target)
                {
                    case TextureTarget::_2D:
                        break;
                    case TextureTarget::CubeMapNegativeX:
                    case TextureTarget::CubeMapNegativeY:
                    case TextureTarget::CubeMapNegativeZ:
                    case TextureTarget::CubeMapPositiveX:
                    case TextureTarget::CubeMapPositiveY:
                    case TextureTarget::CubeMapPositiveZ:
                        if (!context->getExtensions().depthTextureCubeMapOES)
                        {
                            context->validationError(GL_INVALID_OPERATION,
                                                     kMismatchedTargetAndFormat);
                            return false;
                        }
                        break;
                    default:
                        context->validationError(GL_INVALID_OPERATION, kMismatchedTargetAndFormat);
                        return false;
                }

                // OES_depth_texture supports loading depth data and multiple levels,
                // but ANGLE_depth_texture does not
                if (!context->getExtensions().depthTextureOES)
                {
                    if (pixels != nullptr)
                    {
                        context->validationError(GL_INVALID_OPERATION, kPixelDataNotNull);
                        return false;
                    }
                    if (level != 0)
                    {
                        context->validationError(GL_INVALID_OPERATION, kLevelNotZero);
                        return false;
                    }
                }
                break;
            default:
                break;
        }

        if (!isSubImage)
        {
            switch (internalformat)
            {
                // Core ES 2.0 formats
                case GL_ALPHA:
                case GL_LUMINANCE:
                case GL_LUMINANCE_ALPHA:
                case GL_RGB:
                case GL_RGBA:
                    break;

                case GL_RGBA32F:
                    if (!context->getExtensions().colorBufferFloatRGBA)
                    {
                        context->validationError(GL_INVALID_ENUM, kInvalidFormat);
                        return false;
                    }

                    nonEqualFormatsAllowed = true;

                    if (type != GL_FLOAT)
                    {
                        context->validationError(GL_INVALID_OPERATION, kMismatchedTypeAndFormat);
                        return false;
                    }
                    if (format != GL_RGBA)
                    {
                        context->validationError(GL_INVALID_OPERATION, kMismatchedTypeAndFormat);
                        return false;
                    }
                    break;

                case GL_RGB32F:
                    if (!context->getExtensions().colorBufferFloatRGB)
                    {
                        context->validationError(GL_INVALID_ENUM, kInvalidFormat);
                        return false;
                    }

                    nonEqualFormatsAllowed = true;

                    if (type != GL_FLOAT)
                    {
                        context->validationError(GL_INVALID_OPERATION, kMismatchedTypeAndFormat);
                        return false;
                    }
                    if (format != GL_RGB)
                    {
                        context->validationError(GL_INVALID_OPERATION, kMismatchedTypeAndFormat);
                        return false;
                    }
                    break;

                case GL_BGRA_EXT:
                    if (!context->getExtensions().textureFormatBGRA8888)
                    {
                        context->validationError(GL_INVALID_ENUM, kInvalidFormat);
                        return false;
                    }
                    break;

                case GL_DEPTH_COMPONENT:
                    if (!(context->getExtensions().depthTextureAny()))
                    {
                        context->validationError(GL_INVALID_ENUM, kInvalidFormat);
                        return false;
                    }
                    break;

                case GL_DEPTH_STENCIL:
                    if (!(context->getExtensions().depthTextureANGLE ||
                          context->getExtensions().packedDepthStencilOES ||
                          context->getExtensions().depthTextureCubeMapOES))
                    {
                        context->validationError(GL_INVALID_ENUM, kInvalidFormat);
                        return false;
                    }
                    break;

                case GL_RED:
                case GL_RG:
                    if (!context->getExtensions().textureRG)
                    {
                        context->validationError(GL_INVALID_ENUM, kInvalidFormat);
                        return false;
                    }
                    break;

                case GL_SRGB_EXT:
                case GL_SRGB_ALPHA_EXT:
                    if (!context->getExtensions().sRGB)
                    {
                        context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                        return false;
                    }
                    break;

                case GL_RGB10_A2_EXT:
                    if (!context->getExtensions().textureFormat2101010REV)
                    {
                        context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                        return false;
                    }

                    if (type != GL_UNSIGNED_INT_2_10_10_10_REV_EXT || format != GL_RGBA)
                    {
                        context->validationError(GL_INVALID_OPERATION, kMismatchedTypeAndFormat);
                        return false;
                    }

                    nonEqualFormatsAllowed = true;

                    break;

                case GL_RGB5_A1:
                    if (context->getExtensions().textureFormat2101010REV &&
                        type == GL_UNSIGNED_INT_2_10_10_10_REV_EXT && format == GL_RGBA)
                    {
                        nonEqualFormatsAllowed = true;
                    }

                    break;

                case GL_R16_EXT:
                case GL_RG16_EXT:
                case GL_RGB16_EXT:
                case GL_RGBA16_EXT:
                case GL_R16_SNORM_EXT:
                case GL_RG16_SNORM_EXT:
                case GL_RGB16_SNORM_EXT:
                case GL_RGBA16_SNORM_EXT:
                    if (!context->getExtensions().textureNorm16)
                    {
                        context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                        return false;
                    }
                    break;
                default:
                    context->validationError(GL_INVALID_VALUE, kInvalidInternalFormat);
                    return false;
            }
        }

        if (type == GL_FLOAT)
        {
            if (!context->getExtensions().textureFloatOES)
            {
                context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                return false;
            }
        }
        else if (type == GL_HALF_FLOAT_OES)
        {
            if (!context->getExtensions().textureHalfFloat)
            {
                context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                return false;
            }
        }
    }

    if (isSubImage)
    {
        const InternalFormat &textureInternalFormat = *texture->getFormat(target, level).info;
        if (textureInternalFormat.internalFormat == GL_NONE)
        {
            context->validationError(GL_INVALID_OPERATION, kInvalidTextureLevel);
            return false;
        }

        if (format != textureInternalFormat.format)
        {
            context->validationError(GL_INVALID_OPERATION, err::kTextureFormatMismatch);
            return false;
        }

        if (context->getExtensions().webglCompatibility)
        {
            if (GetInternalFormatInfo(format, type).sizedInternalFormat !=
                textureInternalFormat.sizedInternalFormat)
            {
                context->validationError(GL_INVALID_OPERATION, kTextureTypeMismatch);
                return false;
            }
        }

        if (static_cast<size_t>(xoffset + width) > texture->getWidth(target, level) ||
            static_cast<size_t>(yoffset + height) > texture->getHeight(target, level))
        {
            context->validationError(GL_INVALID_VALUE, kOffsetOverflow);
            return false;
        }

        if (width > 0 && height > 0 && pixels == nullptr &&
            context->getState().getTargetBuffer(BufferBinding::PixelUnpack) == nullptr)
        {
            context->validationError(GL_INVALID_VALUE, kPixelDataNull);
            return false;
        }
    }
    else
    {
        if (texture->getImmutableFormat())
        {
            context->validationError(GL_INVALID_OPERATION, kTextureIsImmutable);
            return false;
        }
    }

    // From GL_CHROMIUM_color_buffer_float_rgb[a]:
    // GL_RGB[A] / GL_RGB[A]32F becomes an allowable format / internalformat parameter pair for
    // TexImage2D. The restriction in section 3.7.1 of the OpenGL ES 2.0 spec that the
    // internalformat parameter and format parameter of TexImage2D must match is lifted for this
    // case.
    if (!isSubImage && !isCompressed && internalformat != format && !nonEqualFormatsAllowed)
    {
        context->validationError(GL_INVALID_OPERATION, kInvalidFormatCombination);
        return false;
    }

    GLenum sizeCheckFormat = isSubImage ? format : internalformat;
    return ValidImageDataSize(context, texType, width, height, 1, sizeCheckFormat, type, pixels,
                              imageSize);
}

bool ValidateES2TexStorageParameters(const Context *context,
                                     TextureType target,
                                     GLsizei levels,
                                     GLenum internalformat,
                                     GLsizei width,
                                     GLsizei height)
{
    if (target != TextureType::_2D && target != TextureType::CubeMap &&
        target != TextureType::Rectangle)
    {
        context->validationError(GL_INVALID_ENUM, kInvalidTextureTarget);
        return false;
    }

    if (width < 1 || height < 1 || levels < 1)
    {
        context->validationError(GL_INVALID_VALUE, kTextureSizeTooSmall);
        return false;
    }

    if (target == TextureType::CubeMap && width != height)
    {
        context->validationError(GL_INVALID_VALUE, kCubemapFacesEqualDimensions);
        return false;
    }

    if (levels != 1 && levels != log2(std::max(width, height)) + 1)
    {
        context->validationError(GL_INVALID_OPERATION, kInvalidMipLevels);
        return false;
    }

    const InternalFormat &formatInfo = GetSizedInternalFormatInfo(internalformat);
    if (formatInfo.format == GL_NONE || formatInfo.type == GL_NONE)
    {
        context->validationError(GL_INVALID_ENUM, kInvalidFormat);
        return false;
    }

    const Caps &caps = context->getCaps();

    switch (target)
    {
        case TextureType::_2D:
            if (width > caps.max2DTextureSize || height > caps.max2DTextureSize)
            {
                context->validationError(GL_INVALID_VALUE, kResourceMaxTextureSize);
                return false;
            }
            break;
        case TextureType::Rectangle:
            if (levels != 1)
            {
                context->validationError(GL_INVALID_VALUE, kInvalidMipLevel);
                return false;
            }

            if (width > caps.maxRectangleTextureSize || height > caps.maxRectangleTextureSize)
            {
                context->validationError(GL_INVALID_VALUE, kResourceMaxTextureSize);
                return false;
            }
            if (formatInfo.compressed)
            {
                context->validationError(GL_INVALID_ENUM, kRectangleTextureCompressed);
                return false;
            }
            break;
        case TextureType::CubeMap:
            if (width > caps.maxCubeMapTextureSize || height > caps.maxCubeMapTextureSize)
            {
                context->validationError(GL_INVALID_VALUE, kResourceMaxTextureSize);
                return false;
            }
            break;
        default:
            context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
            return false;
    }

    if (levels != 1 && !context->getExtensions().textureNPOTOES)
    {
        if (!isPow2(width) || !isPow2(height))
        {
            context->validationError(GL_INVALID_OPERATION, kDimensionsMustBePow2);
            return false;
        }
    }

    switch (internalformat)
    {
        case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
            if (!context->getExtensions().textureCompressionDXT1)
            {
                context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                return false;
            }
            break;
        case GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE:
            if (!context->getExtensions().textureCompressionDXT3)
            {
                context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                return false;
            }
            break;
        case GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE:
            if (!context->getExtensions().textureCompressionDXT5)
            {
                context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                return false;
            }
            break;
        case GL_ETC1_RGB8_OES:
            if (!context->getExtensions().compressedETC1RGB8TextureOES)
            {
                context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                return false;
            }
            break;
        case GL_ETC1_RGB8_LOSSY_DECODE_ANGLE:
        case GL_COMPRESSED_RGB8_LOSSY_DECODE_ETC2_ANGLE:
        case GL_COMPRESSED_SRGB8_LOSSY_DECODE_ETC2_ANGLE:
        case GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_LOSSY_DECODE_ETC2_ANGLE:
        case GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_LOSSY_DECODE_ETC2_ANGLE:
            if (!context->getExtensions().lossyETCDecode)
            {
                context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                return false;
            }
            break;
        case GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG:
        case GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG:
        case GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG:
        case GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG:
            if (!context->getExtensions().compressedTexturePVRTC)
            {
                context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                return false;
            }
            break;
        case GL_COMPRESSED_SRGB_PVRTC_2BPPV1_EXT:
        case GL_COMPRESSED_SRGB_PVRTC_4BPPV1_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_PVRTC_2BPPV1_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_PVRTC_4BPPV1_EXT:
            if (!context->getExtensions().compressedTexturePVRTCsRGB)
            {
                context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                return false;
            }
            break;
        case GL_RGBA32F_EXT:
        case GL_RGB32F_EXT:
        case GL_ALPHA32F_EXT:
        case GL_LUMINANCE32F_EXT:
        case GL_LUMINANCE_ALPHA32F_EXT:
            if (!context->getExtensions().textureFloatOES)
            {
                context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                return false;
            }
            break;
        case GL_RGBA16F_EXT:
        case GL_RGB16F_EXT:
        case GL_ALPHA16F_EXT:
        case GL_LUMINANCE16F_EXT:
        case GL_LUMINANCE_ALPHA16F_EXT:
            if (!context->getExtensions().textureHalfFloat)
            {
                context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                return false;
            }
            break;
        case GL_R8_EXT:
        case GL_RG8_EXT:
            if (!context->getExtensions().textureRG)
            {
                context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                return false;
            }
            break;
        case GL_R16F_EXT:
        case GL_RG16F_EXT:
            if (!context->getExtensions().textureRG || !context->getExtensions().textureHalfFloat)
            {
                context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                return false;
            }
            break;
        case GL_R32F_EXT:
        case GL_RG32F_EXT:
            if (!context->getExtensions().textureRG || !context->getExtensions().textureFloatOES)
            {
                context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                return false;
            }
            break;
        case GL_DEPTH_COMPONENT16:
        case GL_DEPTH_COMPONENT32_OES:
            if (!(context->getExtensions().depthTextureAny()))
            {
                context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                return false;
            }

            switch (target)
            {
                case TextureType::_2D:
                    break;
                case TextureType::CubeMap:
                    if (!context->getExtensions().depthTextureCubeMapOES)
                    {
                        context->validationError(GL_INVALID_OPERATION, kInvalidTextureTarget);
                        return false;
                    }
                    break;
                default:
                    context->validationError(GL_INVALID_OPERATION, kInvalidTextureTarget);
                    return false;
            }

            // ANGLE_depth_texture only supports 1-level textures
            if (!context->getExtensions().depthTextureOES)
            {
                if (levels != 1)
                {
                    context->validationError(GL_INVALID_OPERATION, kInvalidMipLevels);
                    return false;
                }
            }
            break;
        case GL_DEPTH24_STENCIL8_OES:
            if (!(context->getExtensions().depthTextureANGLE ||
                  ((context->getExtensions().packedDepthStencilOES ||
                    context->getExtensions().depthTextureCubeMapOES) &&
                   context->getExtensions().textureStorage)))
            {
                context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                return false;
            }

            switch (target)
            {
                case TextureType::_2D:
                    break;
                case TextureType::CubeMap:
                    if (!context->getExtensions().depthTextureCubeMapOES)
                    {
                        context->validationError(GL_INVALID_OPERATION, kInvalidTextureTarget);
                        return false;
                    }
                    break;
                default:
                    context->validationError(GL_INVALID_OPERATION, kInvalidTextureTarget);
                    return false;
            }

            if (!context->getExtensions().packedDepthStencilOES &&
                !context->getExtensions().depthTextureCubeMapOES)
            {
                // ANGLE_depth_texture only supports 1-level textures
                if (levels != 1)
                {
                    context->validationError(GL_INVALID_OPERATION, kInvalidMipLevels);
                    return false;
                }
            }
            break;

        default:
            break;
    }

    Texture *texture = context->getTextureByType(target);
    if (!texture || texture->id().value == 0)
    {
        context->validationError(GL_INVALID_OPERATION, kMissingTexture);
        return false;
    }

    if (texture->getImmutableFormat())
    {
        context->validationError(GL_INVALID_OPERATION, kTextureIsImmutable);
        return false;
    }

    return true;
}

bool ValidateDiscardFramebufferEXT(const Context *context,
                                   GLenum target,
                                   GLsizei numAttachments,
                                   const GLenum *attachments)
{
    if (!context->getExtensions().discardFramebuffer)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    bool defaultFramebuffer = false;

    switch (target)
    {
        case GL_FRAMEBUFFER:
            defaultFramebuffer =
                (context->getState().getTargetFramebuffer(GL_FRAMEBUFFER)->isDefault());
            break;
        default:
            context->validationError(GL_INVALID_ENUM, kInvalidFramebufferTarget);
            return false;
    }

    return ValidateDiscardFramebufferBase(context, target, numAttachments, attachments,
                                          defaultFramebuffer);
}

bool ValidateBindVertexArrayOES(const Context *context, VertexArrayID array)
{
    if (!context->getExtensions().vertexArrayObjectOES)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateBindVertexArrayBase(context, array);
}

bool ValidateDeleteVertexArraysOES(const Context *context, GLsizei n, const VertexArrayID *arrays)
{
    if (!context->getExtensions().vertexArrayObjectOES)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateGenOrDelete(context, n);
}

bool ValidateGenVertexArraysOES(const Context *context, GLsizei n, const VertexArrayID *arrays)
{
    if (!context->getExtensions().vertexArrayObjectOES)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateGenOrDelete(context, n);
}

bool ValidateIsVertexArrayOES(const Context *context, VertexArrayID array)
{
    if (!context->getExtensions().vertexArrayObjectOES)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return true;
}

bool ValidateProgramBinaryOES(const Context *context,
                              ShaderProgramID program,
                              GLenum binaryFormat,
                              const void *binary,
                              GLint length)
{
    if (!context->getExtensions().getProgramBinaryOES)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateProgramBinaryBase(context, program, binaryFormat, binary, length);
}

bool ValidateGetProgramBinaryOES(const Context *context,
                                 ShaderProgramID program,
                                 GLsizei bufSize,
                                 const GLsizei *length,
                                 const GLenum *binaryFormat,
                                 const void *binary)
{
    if (!context->getExtensions().getProgramBinaryOES)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateGetProgramBinaryBase(context, program, bufSize, length, binaryFormat, binary);
}

static bool ValidDebugSource(GLenum source, bool mustBeThirdPartyOrApplication)
{
    switch (source)
    {
        case GL_DEBUG_SOURCE_API:
        case GL_DEBUG_SOURCE_SHADER_COMPILER:
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
        case GL_DEBUG_SOURCE_OTHER:
            // Only THIRD_PARTY and APPLICATION sources are allowed to be manually inserted
            return !mustBeThirdPartyOrApplication;

        case GL_DEBUG_SOURCE_THIRD_PARTY:
        case GL_DEBUG_SOURCE_APPLICATION:
            return true;

        default:
            return false;
    }
}

static bool ValidDebugType(GLenum type)
{
    switch (type)
    {
        case GL_DEBUG_TYPE_ERROR:
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        case GL_DEBUG_TYPE_PERFORMANCE:
        case GL_DEBUG_TYPE_PORTABILITY:
        case GL_DEBUG_TYPE_OTHER:
        case GL_DEBUG_TYPE_MARKER:
        case GL_DEBUG_TYPE_PUSH_GROUP:
        case GL_DEBUG_TYPE_POP_GROUP:
            return true;

        default:
            return false;
    }
}

static bool ValidDebugSeverity(GLenum severity)
{
    switch (severity)
    {
        case GL_DEBUG_SEVERITY_HIGH:
        case GL_DEBUG_SEVERITY_MEDIUM:
        case GL_DEBUG_SEVERITY_LOW:
        case GL_DEBUG_SEVERITY_NOTIFICATION:
            return true;

        default:
            return false;
    }
}

bool ValidateDebugMessageControlKHR(const Context *context,
                                    GLenum source,
                                    GLenum type,
                                    GLenum severity,
                                    GLsizei count,
                                    const GLuint *ids,
                                    GLboolean enabled)
{
    if (!context->getExtensions().debug)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    if (!ValidDebugSource(source, false) && source != GL_DONT_CARE)
    {
        context->validationError(GL_INVALID_ENUM, kInvalidDebugSource);
        return false;
    }

    if (!ValidDebugType(type) && type != GL_DONT_CARE)
    {
        context->validationError(GL_INVALID_ENUM, kInvalidDebugType);
        return false;
    }

    if (!ValidDebugSeverity(severity) && severity != GL_DONT_CARE)
    {
        context->validationError(GL_INVALID_ENUM, kInvalidDebugSeverity);
        return false;
    }

    if (count > 0)
    {
        if (source == GL_DONT_CARE || type == GL_DONT_CARE)
        {
            context->validationError(GL_INVALID_OPERATION, kInvalidDebugSourceType);
            return false;
        }

        if (severity != GL_DONT_CARE)
        {
            context->validationError(GL_INVALID_OPERATION, kInvalidDebugSeverity);
            return false;
        }
    }

    return true;
}

bool ValidateDebugMessageInsertKHR(const Context *context,
                                   GLenum source,
                                   GLenum type,
                                   GLuint id,
                                   GLenum severity,
                                   GLsizei length,
                                   const GLchar *buf)
{
    if (!context->getExtensions().debug)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    if (!context->getState().getDebug().isOutputEnabled())
    {
        // If the DEBUG_OUTPUT state is disabled calls to DebugMessageInsert are discarded and do
        // not generate an error.
        return false;
    }

    if (!ValidDebugSeverity(severity))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidDebugSource);
        return false;
    }

    if (!ValidDebugType(type))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidDebugType);
        return false;
    }

    if (!ValidDebugSource(source, true))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidDebugSource);
        return false;
    }

    size_t messageLength = (length < 0) ? strlen(buf) : length;
    if (messageLength > context->getExtensions().maxDebugMessageLength)
    {
        context->validationError(GL_INVALID_VALUE, kExceedsMaxDebugMessageLength);
        return false;
    }

    return true;
}

bool ValidateDebugMessageCallbackKHR(const Context *context,
                                     GLDEBUGPROCKHR callback,
                                     const void *userParam)
{
    if (!context->getExtensions().debug)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return true;
}

bool ValidateGetDebugMessageLogKHR(const Context *context,
                                   GLuint count,
                                   GLsizei bufSize,
                                   const GLenum *sources,
                                   const GLenum *types,
                                   const GLuint *ids,
                                   const GLenum *severities,
                                   const GLsizei *lengths,
                                   const GLchar *messageLog)
{
    if (!context->getExtensions().debug)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    if (bufSize < 0 && messageLog != nullptr)
    {
        context->validationError(GL_INVALID_VALUE, kNegativeBufferSize);
        return false;
    }

    return true;
}

bool ValidatePushDebugGroupKHR(const Context *context,
                               GLenum source,
                               GLuint id,
                               GLsizei length,
                               const GLchar *message)
{
    if (!context->getExtensions().debug)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    if (!ValidDebugSource(source, true))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidDebugSource);
        return false;
    }

    size_t messageLength = (length < 0) ? strlen(message) : length;
    if (messageLength > context->getExtensions().maxDebugMessageLength)
    {
        context->validationError(GL_INVALID_VALUE, kExceedsMaxDebugMessageLength);
        return false;
    }

    size_t currentStackSize = context->getState().getDebug().getGroupStackDepth();
    if (currentStackSize >= context->getExtensions().maxDebugGroupStackDepth)
    {
        context->validationError(GL_STACK_OVERFLOW, kExceedsMaxDebugGroupStackDepth);
        return false;
    }

    return true;
}

bool ValidatePopDebugGroupKHR(const Context *context)
{
    if (!context->getExtensions().debug)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    size_t currentStackSize = context->getState().getDebug().getGroupStackDepth();
    if (currentStackSize <= 1)
    {
        context->validationError(GL_STACK_UNDERFLOW, kCannotPopDefaultDebugGroup);
        return false;
    }

    return true;
}

static bool ValidateObjectIdentifierAndName(const Context *context, GLenum identifier, GLuint name)
{
    switch (identifier)
    {
        case GL_BUFFER:
            if (context->getBuffer({name}) == nullptr)
            {
                context->validationError(GL_INVALID_VALUE, kInvalidBufferName);
                return false;
            }
            return true;

        case GL_SHADER:
            if (context->getShader({name}) == nullptr)
            {
                context->validationError(GL_INVALID_VALUE, kInvalidShaderName);
                return false;
            }
            return true;

        case GL_PROGRAM:
            if (context->getProgramNoResolveLink({name}) == nullptr)
            {
                context->validationError(GL_INVALID_VALUE, kInvalidProgramName);
                return false;
            }
            return true;

        case GL_VERTEX_ARRAY:
            if (context->getVertexArray({name}) == nullptr)
            {
                context->validationError(GL_INVALID_VALUE, kInvalidVertexArrayName);
                return false;
            }
            return true;

        case GL_QUERY:
            if (context->getQuery({name}) == nullptr)
            {
                context->validationError(GL_INVALID_VALUE, kInvalidQueryName);
                return false;
            }
            return true;

        case GL_TRANSFORM_FEEDBACK:
            if (context->getTransformFeedback({name}) == nullptr)
            {
                context->validationError(GL_INVALID_VALUE, kInvalidTransformFeedbackName);
                return false;
            }
            return true;

        case GL_SAMPLER:
            if (context->getSampler({name}) == nullptr)
            {
                context->validationError(GL_INVALID_VALUE, kInvalidSamplerName);
                return false;
            }
            return true;

        case GL_TEXTURE:
            if (context->getTexture({name}) == nullptr)
            {
                context->validationError(GL_INVALID_VALUE, kInvalidTextureName);
                return false;
            }
            return true;

        case GL_RENDERBUFFER:
            if (!context->isRenderbuffer({name}))
            {
                context->validationError(GL_INVALID_VALUE, kInvalidRenderbufferName);
                return false;
            }
            return true;

        case GL_FRAMEBUFFER:
            if (context->getFramebuffer({name}) == nullptr)
            {
                context->validationError(GL_INVALID_VALUE, kInvalidFramebufferName);
                return false;
            }
            return true;

        case GL_PROGRAM_PIPELINE:
            if (context->getProgramPipeline({name}) == nullptr)
            {
                context->validationError(GL_INVALID_VALUE, kInvalidProgramPipelineName);
                return false;
            }
            return true;

        default:
            context->validationError(GL_INVALID_ENUM, kInvalidIndentifier);
            return false;
    }
}

static bool ValidateLabelLength(const Context *context, GLsizei length, const GLchar *label)
{
    size_t labelLength = 0;

    if (length < 0)
    {
        if (label != nullptr)
        {
            labelLength = strlen(label);
        }
    }
    else
    {
        labelLength = static_cast<size_t>(length);
    }

    if (labelLength > context->getExtensions().maxLabelLength)
    {
        context->validationError(GL_INVALID_VALUE, kExceedsMaxLabelLength);
        return false;
    }

    return true;
}

bool ValidateObjectLabelKHR(const Context *context,
                            GLenum identifier,
                            GLuint name,
                            GLsizei length,
                            const GLchar *label)
{
    if (!context->getExtensions().debug)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    if (!ValidateObjectIdentifierAndName(context, identifier, name))
    {
        return false;
    }

    if (!ValidateLabelLength(context, length, label))
    {
        return false;
    }

    return true;
}

bool ValidateGetObjectLabelKHR(const Context *context,
                               GLenum identifier,
                               GLuint name,
                               GLsizei bufSize,
                               const GLsizei *length,
                               const GLchar *label)
{
    if (!context->getExtensions().debug)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    if (bufSize < 0)
    {
        context->validationError(GL_INVALID_VALUE, kNegativeBufferSize);
        return false;
    }

    if (!ValidateObjectIdentifierAndName(context, identifier, name))
    {
        return false;
    }

    return true;
}

static bool ValidateObjectPtrName(const Context *context, const void *ptr)
{
    if (context->getSync(reinterpret_cast<GLsync>(const_cast<void *>(ptr))) == nullptr)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidSyncPointer);
        return false;
    }

    return true;
}

bool ValidateObjectPtrLabelKHR(const Context *context,
                               const void *ptr,
                               GLsizei length,
                               const GLchar *label)
{
    if (!context->getExtensions().debug)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    if (!ValidateObjectPtrName(context, ptr))
    {
        return false;
    }

    if (!ValidateLabelLength(context, length, label))
    {
        return false;
    }

    return true;
}

bool ValidateGetObjectPtrLabelKHR(const Context *context,
                                  const void *ptr,
                                  GLsizei bufSize,
                                  const GLsizei *length,
                                  const GLchar *label)
{
    if (!context->getExtensions().debug)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    if (bufSize < 0)
    {
        context->validationError(GL_INVALID_VALUE, kNegativeBufferSize);
        return false;
    }

    if (!ValidateObjectPtrName(context, ptr))
    {
        return false;
    }

    return true;
}

bool ValidateGetPointervKHR(const Context *context, GLenum pname, void *const *params)
{
    if (!context->getExtensions().debug)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    // TODO: represent this in Context::getQueryParameterInfo.
    switch (pname)
    {
        case GL_DEBUG_CALLBACK_FUNCTION:
        case GL_DEBUG_CALLBACK_USER_PARAM:
            break;

        default:
            context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
            return false;
    }

    return true;
}

bool ValidateGetPointervRobustANGLERobustANGLE(const Context *context,
                                               GLenum pname,
                                               GLsizei bufSize,
                                               const GLsizei *length,
                                               void *const *params)
{
    UNIMPLEMENTED();
    return false;
}

bool ValidateBlitFramebufferANGLE(const Context *context,
                                  GLint srcX0,
                                  GLint srcY0,
                                  GLint srcX1,
                                  GLint srcY1,
                                  GLint dstX0,
                                  GLint dstY0,
                                  GLint dstX1,
                                  GLint dstY1,
                                  GLbitfield mask,
                                  GLenum filter)
{
    if (!context->getExtensions().framebufferBlit)
    {
        context->validationError(GL_INVALID_OPERATION, kBlitExtensionNotAvailable);
        return false;
    }

    if (srcX1 - srcX0 != dstX1 - dstX0 || srcY1 - srcY0 != dstY1 - dstY0)
    {
        // TODO(jmadill): Determine if this should be available on other implementations.
        context->validationError(GL_INVALID_OPERATION, kBlitExtensionScaleOrFlip);
        return false;
    }

    if (filter == GL_LINEAR)
    {
        context->validationError(GL_INVALID_ENUM, kBlitExtensionLinear);
        return false;
    }

    Framebuffer *readFramebuffer = context->getState().getReadFramebuffer();
    Framebuffer *drawFramebuffer = context->getState().getDrawFramebuffer();

    if (mask & GL_COLOR_BUFFER_BIT)
    {
        const FramebufferAttachment *readColorAttachment =
            readFramebuffer->getReadColorAttachment();
        const FramebufferAttachment *drawColorAttachment =
            drawFramebuffer->getFirstColorAttachment();

        if (readColorAttachment && drawColorAttachment)
        {
            if (!(readColorAttachment->type() == GL_TEXTURE &&
                  (readColorAttachment->getTextureImageIndex().getType() == TextureType::_2D ||
                   readColorAttachment->getTextureImageIndex().getType() ==
                       TextureType::Rectangle)) &&
                readColorAttachment->type() != GL_RENDERBUFFER &&
                readColorAttachment->type() != GL_FRAMEBUFFER_DEFAULT)
            {
                context->validationError(GL_INVALID_OPERATION,
                                         kBlitExtensionFromInvalidAttachmentType);
                return false;
            }

            for (size_t drawbufferIdx = 0;
                 drawbufferIdx < drawFramebuffer->getDrawbufferStateCount(); ++drawbufferIdx)
            {
                const FramebufferAttachment *attachment =
                    drawFramebuffer->getDrawBuffer(drawbufferIdx);
                if (attachment)
                {
                    if (!(attachment->type() == GL_TEXTURE &&
                          (attachment->getTextureImageIndex().getType() == TextureType::_2D ||
                           attachment->getTextureImageIndex().getType() ==
                               TextureType::Rectangle)) &&
                        attachment->type() != GL_RENDERBUFFER &&
                        attachment->type() != GL_FRAMEBUFFER_DEFAULT)
                    {
                        context->validationError(GL_INVALID_OPERATION,
                                                 kBlitExtensionToInvalidAttachmentType);
                        return false;
                    }

                    // Return an error if the destination formats do not match
                    if (!Format::EquivalentForBlit(attachment->getFormat(),
                                                   readColorAttachment->getFormat()))
                    {
                        context->validationError(GL_INVALID_OPERATION,
                                                 kBlitExtensionFormatMismatch);
                        return false;
                    }
                }
            }

            GLint samples = readFramebuffer->getSamples(context);
            if (samples != 0 &&
                IsPartialBlit(context, readColorAttachment, drawColorAttachment, srcX0, srcY0,
                              srcX1, srcY1, dstX0, dstY0, dstX1, dstY1))
            {
                context->validationError(GL_INVALID_OPERATION,
                                         kBlitExtensionMultisampledWholeBufferBlit);
                return false;
            }
        }
    }

    GLenum masks[]       = {GL_DEPTH_BUFFER_BIT, GL_STENCIL_BUFFER_BIT};
    GLenum attachments[] = {GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT};
    for (size_t i = 0; i < 2; i++)
    {
        if (mask & masks[i])
        {
            const FramebufferAttachment *readBuffer =
                readFramebuffer->getAttachment(context, attachments[i]);
            const FramebufferAttachment *drawBuffer =
                drawFramebuffer->getAttachment(context, attachments[i]);

            if (readBuffer && drawBuffer)
            {
                if (IsPartialBlit(context, readBuffer, drawBuffer, srcX0, srcY0, srcX1, srcY1,
                                  dstX0, dstY0, dstX1, dstY1))
                {
                    // only whole-buffer copies are permitted
                    context->validationError(GL_INVALID_OPERATION,
                                             kBlitExtensionDepthStencilWholeBufferBlit);
                    return false;
                }

                if (readBuffer->getSamples() != 0 || drawBuffer->getSamples() != 0)
                {
                    context->validationError(GL_INVALID_OPERATION,
                                             kBlitExtensionMultisampledDepthOrStencil);
                    return false;
                }
            }
        }
    }

    return ValidateBlitFramebufferParameters(context, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0,
                                             dstX1, dstY1, mask, filter);
}

bool ValidateClear(const Context *context, GLbitfield mask)
{
    Framebuffer *fbo             = context->getState().getDrawFramebuffer();
    const Extensions &extensions = context->getExtensions();

    if (!ValidateFramebufferComplete(context, fbo))
    {
        return false;
    }

    if ((mask & ~(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) != 0)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidClearMask);
        return false;
    }

    if (extensions.webglCompatibility && (mask & GL_COLOR_BUFFER_BIT) != 0)
    {
        constexpr GLenum validComponentTypes[] = {GL_FLOAT, GL_UNSIGNED_NORMALIZED,
                                                  GL_SIGNED_NORMALIZED};

        for (GLuint drawBufferIdx = 0; drawBufferIdx < fbo->getDrawbufferStateCount();
             drawBufferIdx++)
        {
            if (!ValidateWebGLFramebufferAttachmentClearType(
                    context, drawBufferIdx, validComponentTypes, ArraySize(validComponentTypes)))
            {
                return false;
            }
        }
    }

    if ((extensions.multiview || extensions.multiview2) && extensions.disjointTimerQuery)
    {
        const State &state       = context->getState();
        Framebuffer *framebuffer = state.getDrawFramebuffer();
        if (framebuffer->getNumViews() > 1 && state.isQueryActive(QueryType::TimeElapsed))
        {
            context->validationError(GL_INVALID_OPERATION, kMultiviewTimerQuery);
            return false;
        }
    }

    return true;
}

bool ValidateDrawBuffersEXT(const Context *context, GLsizei n, const GLenum *bufs)
{
    if (!context->getExtensions().drawBuffers)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateDrawBuffersBase(context, n, bufs);
}

bool ValidateTexImage2D(const Context *context,
                        TextureTarget target,
                        GLint level,
                        GLint internalformat,
                        GLsizei width,
                        GLsizei height,
                        GLint border,
                        GLenum format,
                        GLenum type,
                        const void *pixels)
{
    if (context->getClientMajorVersion() < 3)
    {
        return ValidateES2TexImageParameters(context, target, level, internalformat, false, false,
                                             0, 0, width, height, border, format, type, -1, pixels);
    }

    ASSERT(context->getClientMajorVersion() >= 3);
    return ValidateES3TexImage2DParameters(context, target, level, internalformat, false, false, 0,
                                           0, 0, width, height, 1, border, format, type, -1,
                                           pixels);
}

bool ValidateTexImage2DRobustANGLE(const Context *context,
                                   TextureTarget target,
                                   GLint level,
                                   GLint internalformat,
                                   GLsizei width,
                                   GLsizei height,
                                   GLint border,
                                   GLenum format,
                                   GLenum type,
                                   GLsizei bufSize,
                                   const void *pixels)
{
    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    if (context->getClientMajorVersion() < 3)
    {
        return ValidateES2TexImageParameters(context, target, level, internalformat, false, false,
                                             0, 0, width, height, border, format, type, bufSize,
                                             pixels);
    }

    ASSERT(context->getClientMajorVersion() >= 3);
    return ValidateES3TexImage2DParameters(context, target, level, internalformat, false, false, 0,
                                           0, 0, width, height, 1, border, format, type, bufSize,
                                           pixels);
}

bool ValidateTexSubImage2D(const Context *context,
                           TextureTarget target,
                           GLint level,
                           GLint xoffset,
                           GLint yoffset,
                           GLsizei width,
                           GLsizei height,
                           GLenum format,
                           GLenum type,
                           const void *pixels)
{

    if (context->getClientMajorVersion() < 3)
    {
        return ValidateES2TexImageParameters(context, target, level, GL_NONE, false, true, xoffset,
                                             yoffset, width, height, 0, format, type, -1, pixels);
    }

    ASSERT(context->getClientMajorVersion() >= 3);
    return ValidateES3TexImage2DParameters(context, target, level, GL_NONE, false, true, xoffset,
                                           yoffset, 0, width, height, 1, 0, format, type, -1,
                                           pixels);
}

bool ValidateTexSubImage2DRobustANGLE(const Context *context,
                                      TextureTarget target,
                                      GLint level,
                                      GLint xoffset,
                                      GLint yoffset,
                                      GLsizei width,
                                      GLsizei height,
                                      GLenum format,
                                      GLenum type,
                                      GLsizei bufSize,
                                      const void *pixels)
{
    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    if (context->getClientMajorVersion() < 3)
    {
        return ValidateES2TexImageParameters(context, target, level, GL_NONE, false, true, xoffset,
                                             yoffset, width, height, 0, format, type, bufSize,
                                             pixels);
    }

    ASSERT(context->getClientMajorVersion() >= 3);
    return ValidateES3TexImage2DParameters(context, target, level, GL_NONE, false, true, xoffset,
                                           yoffset, 0, width, height, 1, 0, format, type, bufSize,
                                           pixels);
}

bool ValidateTexSubImage3DOES(const Context *context,
                              TextureTarget target,
                              GLint level,
                              GLint xoffset,
                              GLint yoffset,
                              GLint zoffset,
                              GLsizei width,
                              GLsizei height,
                              GLsizei depth,
                              GLenum format,
                              GLenum type,
                              const void *pixels)
{
    return ValidateTexSubImage3D(context, target, level, xoffset, yoffset, zoffset, width, height,
                                 depth, format, type, pixels);
}

bool ValidateCompressedTexImage2D(const Context *context,
                                  TextureTarget target,
                                  GLint level,
                                  GLenum internalformat,
                                  GLsizei width,
                                  GLsizei height,
                                  GLint border,
                                  GLsizei imageSize,
                                  const void *data)
{
    if (context->getClientMajorVersion() < 3)
    {
        if (!ValidateES2TexImageParameters(context, target, level, internalformat, true, false, 0,
                                           0, width, height, border, GL_NONE, GL_NONE, -1, data))
        {
            return false;
        }
    }
    else
    {
        ASSERT(context->getClientMajorVersion() >= 3);
        if (!ValidateES3TexImage2DParameters(context, target, level, internalformat, true, false, 0,
                                             0, 0, width, height, 1, border, GL_NONE, GL_NONE, -1,
                                             data))
        {
            return false;
        }
    }

    const InternalFormat &formatInfo = GetSizedInternalFormatInfo(internalformat);

    GLuint blockSize = 0;
    if (!formatInfo.computeCompressedImageSize(Extents(width, height, 1), &blockSize))
    {
        context->validationError(GL_INVALID_OPERATION, kIntegerOverflow);
        return false;
    }

    if (imageSize < 0 || static_cast<GLuint>(imageSize) != blockSize)
    {
        context->validationError(GL_INVALID_VALUE, kCompressedTextureDimensionsMustMatchData);
        return false;
    }

    if (target == TextureTarget::Rectangle)
    {
        context->validationError(GL_INVALID_ENUM, kRectangleTextureCompressed);
        return false;
    }

    return true;
}

bool ValidateCompressedTexImage2DRobustANGLE(const Context *context,
                                             TextureTarget target,
                                             GLint level,
                                             GLenum internalformat,
                                             GLsizei width,
                                             GLsizei height,
                                             GLint border,
                                             GLsizei imageSize,
                                             GLsizei dataSize,
                                             const void *data)
{
    if (!ValidateRobustCompressedTexImageBase(context, imageSize, dataSize))
    {
        return false;
    }

    return ValidateCompressedTexImage2D(context, target, level, internalformat, width, height,
                                        border, imageSize, data);
}

bool ValidateCompressedTexImage3DOES(const Context *context,
                                     TextureTarget target,
                                     GLint level,
                                     GLenum internalformat,
                                     GLsizei width,
                                     GLsizei height,
                                     GLsizei depth,
                                     GLint border,
                                     GLsizei imageSize,
                                     const void *data)
{
    return ValidateCompressedTexImage3D(context, target, level, internalformat, width, height,
                                        depth, border, imageSize, data);
}

bool ValidateCompressedTexSubImage2DRobustANGLE(const Context *context,
                                                TextureTarget target,
                                                GLint level,
                                                GLint xoffset,
                                                GLint yoffset,
                                                GLsizei width,
                                                GLsizei height,
                                                GLenum format,
                                                GLsizei imageSize,
                                                GLsizei dataSize,
                                                const void *data)
{
    if (!ValidateRobustCompressedTexImageBase(context, imageSize, dataSize))
    {
        return false;
    }

    return ValidateCompressedTexSubImage2D(context, target, level, xoffset, yoffset, width, height,
                                           format, imageSize, data);
}

bool ValidateCompressedTexSubImage2D(const Context *context,
                                     TextureTarget target,
                                     GLint level,
                                     GLint xoffset,
                                     GLint yoffset,
                                     GLsizei width,
                                     GLsizei height,
                                     GLenum format,
                                     GLsizei imageSize,
                                     const void *data)
{
    if (context->getClientMajorVersion() < 3)
    {
        if (!ValidateES2TexImageParameters(context, target, level, GL_NONE, true, true, xoffset,
                                           yoffset, width, height, 0, format, GL_NONE, -1, data))
        {
            return false;
        }
    }
    else
    {
        ASSERT(context->getClientMajorVersion() >= 3);
        if (!ValidateES3TexImage2DParameters(context, target, level, GL_NONE, true, true, xoffset,
                                             yoffset, 0, width, height, 1, 0, format, GL_NONE, -1,
                                             data))
        {
            return false;
        }
    }

    const InternalFormat &formatInfo = GetSizedInternalFormatInfo(format);
    GLuint blockSize                 = 0;
    if (!formatInfo.computeCompressedImageSize(Extents(width, height, 1), &blockSize))
    {
        context->validationError(GL_INVALID_OPERATION, kIntegerOverflow);
        return false;
    }

    if (imageSize < 0 || static_cast<GLuint>(imageSize) != blockSize)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidCompressedImageSize);
        return false;
    }

    return true;
}

bool ValidateCompressedTexSubImage3DOES(const Context *context,
                                        TextureTarget target,
                                        GLint level,
                                        GLint xoffset,
                                        GLint yoffset,
                                        GLint zoffset,
                                        GLsizei width,
                                        GLsizei height,
                                        GLsizei depth,
                                        GLenum format,
                                        GLsizei imageSize,
                                        const void *data)
{
    return ValidateCompressedTexSubImage3D(context, target, level, xoffset, yoffset, zoffset, width,
                                           height, depth, format, imageSize, data);
}

bool ValidateGetBufferPointervOES(const Context *context,
                                  BufferBinding target,
                                  GLenum pname,
                                  void *const *params)
{
    if (!context->getExtensions().mapBufferOES)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateGetBufferPointervBase(context, target, pname, nullptr, params);
}

bool ValidateMapBufferOES(const Context *context, BufferBinding target, GLenum access)
{
    if (!context->getExtensions().mapBufferOES)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    if (!context->isValidBufferBinding(target))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidBufferTypes);
        return false;
    }

    Buffer *buffer = context->getState().getTargetBuffer(target);

    if (buffer == nullptr)
    {
        context->validationError(GL_INVALID_OPERATION, kBufferNotMappable);
        return false;
    }

    if (access != GL_WRITE_ONLY_OES)
    {
        context->validationError(GL_INVALID_ENUM, kInvalidAccessBits);
        return false;
    }

    if (buffer->isMapped())
    {
        context->validationError(GL_INVALID_OPERATION, kBufferAlreadyMapped);
        return false;
    }

    return ValidateMapBufferBase(context, target);
}

bool ValidateUnmapBufferOES(const Context *context, BufferBinding target)
{
    if (!context->getExtensions().mapBufferOES)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateUnmapBufferBase(context, target);
}

bool ValidateMapBufferRangeEXT(const Context *context,
                               BufferBinding target,
                               GLintptr offset,
                               GLsizeiptr length,
                               GLbitfield access)
{
    if (!context->getExtensions().mapBufferRange)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateMapBufferRangeBase(context, target, offset, length, access);
}

bool ValidateBufferStorageMemEXT(const Context *context,
                                 TextureType target,
                                 GLsizeiptr size,
                                 MemoryObjectID memory,
                                 GLuint64 offset)
{
    if (!context->getExtensions().memoryObject)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    UNIMPLEMENTED();
    return false;
}

bool ValidateCreateMemoryObjectsEXT(const Context *context,
                                    GLsizei n,
                                    const MemoryObjectID *memoryObjects)
{
    if (!context->getExtensions().memoryObject)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateGenOrDelete(context, n);
}

bool ValidateDeleteMemoryObjectsEXT(const Context *context,
                                    GLsizei n,
                                    const MemoryObjectID *memoryObjects)
{
    if (!context->getExtensions().memoryObject)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateGenOrDelete(context, n);
}

bool ValidateGetMemoryObjectParameterivEXT(const Context *context,
                                           MemoryObjectID memoryObject,
                                           GLenum pname,
                                           const GLint *params)
{
    if (!context->getExtensions().memoryObject)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    const MemoryObject *memory = context->getMemoryObject(memoryObject);
    if (memory == nullptr)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidMemoryObject);
    }

    if (!IsValidMemoryObjectParamater(context, pname))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidMemoryObjectParameter);
        return false;
    }

    return true;
}

bool ValidateGetUnsignedBytevEXT(const Context *context, GLenum pname, const GLubyte *data)
{
    if (!context->getExtensions().memoryObject && !context->getExtensions().semaphore)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    UNIMPLEMENTED();
    return false;
}

bool ValidateGetUnsignedBytei_vEXT(const Context *context,
                                   GLenum target,
                                   GLuint index,
                                   const GLubyte *data)
{
    if (!context->getExtensions().memoryObject && !context->getExtensions().semaphore)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    UNIMPLEMENTED();
    return false;
}

bool ValidateIsMemoryObjectEXT(const Context *context, MemoryObjectID memoryObject)
{
    if (!context->getExtensions().memoryObject)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return true;
}

bool ValidateMemoryObjectParameterivEXT(const Context *context,
                                        MemoryObjectID memoryObject,
                                        GLenum pname,
                                        const GLint *params)
{
    if (!context->getExtensions().memoryObject)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    const MemoryObject *memory = context->getMemoryObject(memoryObject);
    if (memory == nullptr)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidMemoryObject);
        return false;
    }

    if (memory->isImmutable())
    {
        context->validationError(GL_INVALID_OPERATION, kImmutableMemoryObject);
        return false;
    }

    if (!IsValidMemoryObjectParamater(context, pname))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidMemoryObjectParameter);
        return false;
    }

    return true;
}

bool ValidateTexStorageMem2DEXT(const Context *context,
                                TextureType target,
                                GLsizei levels,
                                GLenum internalFormat,
                                GLsizei width,
                                GLsizei height,
                                MemoryObjectID memory,
                                GLuint64 offset)
{
    if (!context->getExtensions().memoryObject)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    if (context->getClientMajorVersion() < 3)
    {
        return ValidateES2TexStorageParameters(context, target, levels, internalFormat, width,
                                               height);
    }

    ASSERT(context->getClientMajorVersion() >= 3);
    return ValidateES3TexStorage2DParameters(context, target, levels, internalFormat, width, height,
                                             1);
}

bool ValidateTexStorageMem3DEXT(const Context *context,
                                TextureType target,
                                GLsizei levels,
                                GLenum internalFormat,
                                GLsizei width,
                                GLsizei height,
                                GLsizei depth,
                                MemoryObjectID memory,
                                GLuint64 offset)
{
    if (!context->getExtensions().memoryObject)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    UNIMPLEMENTED();
    return false;
}

bool ValidateImportMemoryFdEXT(const Context *context,
                               MemoryObjectID memory,
                               GLuint64 size,
                               HandleType handleType,
                               GLint fd)
{
    if (!context->getExtensions().memoryObjectFd)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    switch (handleType)
    {
        case HandleType::OpaqueFd:
            break;
        default:
            context->validationError(GL_INVALID_ENUM, kInvalidHandleType);
            return false;
    }

    return true;
}

bool ValidateImportMemoryZirconHandleANGLE(const Context *context,
                                           MemoryObjectID memory,
                                           GLuint64 size,
                                           HandleType handleType,
                                           GLuint handle)
{
    if (!context->getExtensions().memoryObjectFuchsiaANGLE)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    switch (handleType)
    {
        case HandleType::ZirconVmo:
            break;
        default:
            context->validationError(GL_INVALID_ENUM, kInvalidHandleType);
            return false;
    }

    return true;
}

bool ValidateDeleteSemaphoresEXT(const Context *context, GLsizei n, const SemaphoreID *semaphores)
{
    if (!context->getExtensions().semaphore)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateGenOrDelete(context, n);
}

bool ValidateGenSemaphoresEXT(const Context *context, GLsizei n, const SemaphoreID *semaphores)
{
    if (!context->getExtensions().semaphore)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateGenOrDelete(context, n);
}

bool ValidateGetSemaphoreParameterui64vEXT(const Context *context,
                                           SemaphoreID semaphore,
                                           GLenum pname,
                                           const GLuint64 *params)
{
    if (!context->getExtensions().semaphore)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    UNIMPLEMENTED();
    return false;
}

bool ValidateIsSemaphoreEXT(const Context *context, SemaphoreID semaphore)
{
    if (!context->getExtensions().semaphore)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return true;
}

bool ValidateSemaphoreParameterui64vEXT(const Context *context,
                                        SemaphoreID semaphore,
                                        GLenum pname,
                                        const GLuint64 *params)
{
    if (!context->getExtensions().semaphore)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    UNIMPLEMENTED();
    return false;
}

bool ValidateSignalSemaphoreEXT(const Context *context,
                                SemaphoreID semaphore,
                                GLuint numBufferBarriers,
                                const BufferID *buffers,
                                GLuint numTextureBarriers,
                                const TextureID *textures,
                                const GLenum *dstLayouts)
{
    if (!context->getExtensions().semaphore)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    for (GLuint i = 0; i < numTextureBarriers; ++i)
    {
        if (!IsValidImageLayout(FromGLenum<ImageLayout>(dstLayouts[i])))
        {
            context->validationError(GL_INVALID_ENUM, kInvalidImageLayout);
            return false;
        }
    }

    return true;
}

bool ValidateWaitSemaphoreEXT(const Context *context,
                              SemaphoreID semaphore,
                              GLuint numBufferBarriers,
                              const BufferID *buffers,
                              GLuint numTextureBarriers,
                              const TextureID *textures,
                              const GLenum *srcLayouts)
{
    if (!context->getExtensions().semaphore)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    for (GLuint i = 0; i < numTextureBarriers; ++i)
    {
        if (!IsValidImageLayout(FromGLenum<ImageLayout>(srcLayouts[i])))
        {
            context->validationError(GL_INVALID_ENUM, kInvalidImageLayout);
            return false;
        }
    }

    return true;
}

bool ValidateImportSemaphoreFdEXT(const Context *context,
                                  SemaphoreID semaphore,
                                  HandleType handleType,
                                  GLint fd)
{
    if (!context->getExtensions().semaphoreFd)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    switch (handleType)
    {
        case HandleType::OpaqueFd:
            break;
        default:
            context->validationError(GL_INVALID_ENUM, kInvalidHandleType);
            return false;
    }

    return true;
}

bool ValidateImportSemaphoreZirconHandleANGLE(const Context *context,
                                              SemaphoreID semaphore,
                                              HandleType handleType,
                                              GLuint handle)
{
    if (!context->getExtensions().semaphoreFuchsiaANGLE)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    switch (handleType)
    {
        case HandleType::ZirconEvent:
            break;
        default:
            context->validationError(GL_INVALID_ENUM, kInvalidHandleType);
            return false;
    }

    return true;
}

bool ValidateMapBufferBase(const Context *context, BufferBinding target)
{
    Buffer *buffer = context->getState().getTargetBuffer(target);
    ASSERT(buffer != nullptr);

    // Check if this buffer is currently being used as a transform feedback output buffer
    if (context->getState().isTransformFeedbackActive())
    {
        TransformFeedback *transformFeedback = context->getState().getCurrentTransformFeedback();
        for (size_t i = 0; i < transformFeedback->getIndexedBufferCount(); i++)
        {
            const auto &transformFeedbackBuffer = transformFeedback->getIndexedBuffer(i);
            if (transformFeedbackBuffer.get() == buffer)
            {
                context->validationError(GL_INVALID_OPERATION, kBufferBoundForTransformFeedback);
                return false;
            }
        }
    }

    if (context->getExtensions().webglCompatibility &&
        buffer->isBoundForTransformFeedbackAndOtherUse())
    {
        context->validationError(GL_INVALID_OPERATION, kBufferBoundForTransformFeedback);
        return false;
    }

    return true;
}

bool ValidateFlushMappedBufferRangeEXT(const Context *context,
                                       BufferBinding target,
                                       GLintptr offset,
                                       GLsizeiptr length)
{
    if (!context->getExtensions().mapBufferRange)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateFlushMappedBufferRangeBase(context, target, offset, length);
}

bool ValidateBindUniformLocationCHROMIUM(const Context *context,
                                         ShaderProgramID program,
                                         UniformLocation location,
                                         const GLchar *name)
{
    if (!context->getExtensions().bindUniformLocation)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    Program *programObject = GetValidProgram(context, program);
    if (!programObject)
    {
        return false;
    }

    if (location.value < 0)
    {
        context->validationError(GL_INVALID_VALUE, kNegativeLocation);
        return false;
    }

    const Caps &caps = context->getCaps();
    if (static_cast<long>(location.value) >=
        (caps.maxVertexUniformVectors + caps.maxFragmentUniformVectors) * 4)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidBindUniformLocation);
        return false;
    }

    // The WebGL spec (section 6.20) disallows strings containing invalid ESSL characters for
    // shader-related entry points
    if (context->getExtensions().webglCompatibility && !IsValidESSLString(name, strlen(name)))
    {
        context->validationError(GL_INVALID_VALUE, kInvalidNameCharacters);
        return false;
    }

    if (strncmp(name, "gl_", 3) == 0)
    {
        context->validationError(GL_INVALID_VALUE, kNameBeginsWithGL);
        return false;
    }

    return true;
}

bool ValidateCoverageModulationCHROMIUM(const Context *context, GLenum components)
{
    if (!context->getExtensions().framebufferMixedSamples)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }
    switch (components)
    {
        case GL_RGB:
        case GL_RGBA:
        case GL_ALPHA:
        case GL_NONE:
            break;
        default:
            context->validationError(GL_INVALID_ENUM, kInvalidCoverageComponents);
            return false;
    }

    return true;
}

bool ValidateCopyTextureCHROMIUM(const Context *context,
                                 TextureID sourceId,
                                 GLint sourceLevel,
                                 TextureTarget destTarget,
                                 TextureID destId,
                                 GLint destLevel,
                                 GLint internalFormat,
                                 GLenum destType,
                                 GLboolean unpackFlipY,
                                 GLboolean unpackPremultiplyAlpha,
                                 GLboolean unpackUnmultiplyAlpha)
{
    if (!context->getExtensions().copyTexture)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    const Texture *source = context->getTexture(sourceId);
    if (source == nullptr)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidSourceTexture);
        return false;
    }

    if (!IsValidCopyTextureSourceTarget(context, source->getType()))
    {
        context->validationError(GL_INVALID_OPERATION, kInvalidInternalFormat);
        return false;
    }

    TextureType sourceType = source->getType();
    ASSERT(sourceType != TextureType::CubeMap);
    TextureTarget sourceTarget = NonCubeTextureTypeToTarget(sourceType);

    if (!IsValidCopyTextureSourceLevel(context, sourceType, sourceLevel))
    {
        context->validationError(GL_INVALID_VALUE, kInvalidSourceTextureLevel);
        return false;
    }

    GLsizei sourceWidth  = static_cast<GLsizei>(source->getWidth(sourceTarget, sourceLevel));
    GLsizei sourceHeight = static_cast<GLsizei>(source->getHeight(sourceTarget, sourceLevel));
    if (sourceWidth == 0 || sourceHeight == 0)
    {
        context->validationError(GL_INVALID_OPERATION, kInvalidInternalFormat);
        return false;
    }

    const InternalFormat &sourceFormat = *source->getFormat(sourceTarget, sourceLevel).info;
    if (!IsValidCopyTextureSourceInternalFormatEnum(sourceFormat.internalFormat))
    {
        context->validationError(GL_INVALID_OPERATION, kInvalidSourceTextureInternalFormat);
        return false;
    }

    if (!IsValidCopyTextureDestinationTargetEnum(context, destTarget))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidTextureTarget);
        return false;
    }

    const Texture *dest = context->getTexture(destId);
    if (dest == nullptr)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidDestinationTexture);
        return false;
    }

    if (!IsValidCopyTextureDestinationTarget(context, dest->getType(), destTarget))
    {
        context->validationError(GL_INVALID_VALUE, kInvalidDestinationTextureType);
        return false;
    }

    if (!IsValidCopyTextureDestinationLevel(context, dest->getType(), destLevel, sourceWidth,
                                            sourceHeight, false))
    {
        context->validationError(GL_INVALID_VALUE, kInvalidMipLevel);
        return false;
    }

    if (!IsValidCopyTextureDestinationFormatType(context, internalFormat, destType))
    {
        return false;
    }

    if (dest->getType() == TextureType::CubeMap && sourceWidth != sourceHeight)
    {
        context->validationError(GL_INVALID_VALUE, kCubemapFacesEqualDimensions);
        return false;
    }

    if (dest->getImmutableFormat())
    {
        context->validationError(GL_INVALID_OPERATION, kDestinationImmutable);
        return false;
    }

    return true;
}

bool ValidateCopySubTextureCHROMIUM(const Context *context,
                                    TextureID sourceId,
                                    GLint sourceLevel,
                                    TextureTarget destTarget,
                                    TextureID destId,
                                    GLint destLevel,
                                    GLint xoffset,
                                    GLint yoffset,
                                    GLint x,
                                    GLint y,
                                    GLsizei width,
                                    GLsizei height,
                                    GLboolean unpackFlipY,
                                    GLboolean unpackPremultiplyAlpha,
                                    GLboolean unpackUnmultiplyAlpha)
{
    if (!context->getExtensions().copyTexture)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    const Texture *source = context->getTexture(sourceId);
    if (source == nullptr)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidSourceTexture);
        return false;
    }

    if (!IsValidCopyTextureSourceTarget(context, source->getType()))
    {
        context->validationError(GL_INVALID_VALUE, kInvalidSourceTextureType);
        return false;
    }

    TextureType sourceType = source->getType();
    ASSERT(sourceType != TextureType::CubeMap);
    TextureTarget sourceTarget = NonCubeTextureTypeToTarget(sourceType);

    if (!IsValidCopyTextureSourceLevel(context, sourceType, sourceLevel))
    {
        context->validationError(GL_INVALID_VALUE, kInvalidMipLevel);
        return false;
    }

    if (source->getWidth(sourceTarget, sourceLevel) == 0 ||
        source->getHeight(sourceTarget, sourceLevel) == 0)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidSourceTextureLevel);
        return false;
    }

    if (x < 0 || y < 0)
    {
        context->validationError(GL_INVALID_VALUE, kNegativeOffset);
        return false;
    }

    if (width < 0 || height < 0)
    {
        context->validationError(GL_INVALID_VALUE, kNegativeSize);
        return false;
    }

    if (static_cast<size_t>(x + width) > source->getWidth(sourceTarget, sourceLevel) ||
        static_cast<size_t>(y + height) > source->getHeight(sourceTarget, sourceLevel))
    {
        context->validationError(GL_INVALID_VALUE, kSourceTextureTooSmall);
        return false;
    }

    const Format &sourceFormat = source->getFormat(sourceTarget, sourceLevel);
    if (!IsValidCopySubTextureSourceInternalFormat(sourceFormat.info->internalFormat))
    {
        context->validationError(GL_INVALID_OPERATION, kInvalidInternalFormat);
        return false;
    }

    if (!IsValidCopyTextureDestinationTargetEnum(context, destTarget))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidTextureTarget);
        return false;
    }

    const Texture *dest = context->getTexture(destId);
    if (dest == nullptr)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidDestinationTexture);
        return false;
    }

    if (!IsValidCopyTextureDestinationTarget(context, dest->getType(), destTarget))
    {
        context->validationError(GL_INVALID_VALUE, kInvalidDestinationTextureType);
        return false;
    }

    if (!IsValidCopyTextureDestinationLevel(context, dest->getType(), destLevel, width, height,
                                            true))
    {
        context->validationError(GL_INVALID_VALUE, kInvalidMipLevel);
        return false;
    }

    if (dest->getWidth(destTarget, destLevel) == 0 || dest->getHeight(destTarget, destLevel) == 0)
    {
        context->validationError(GL_INVALID_OPERATION, kDestinationLevelNotDefined);
        return false;
    }

    const InternalFormat &destFormat = *dest->getFormat(destTarget, destLevel).info;
    if (!IsValidCopySubTextureDestionationInternalFormat(destFormat.internalFormat))
    {
        context->validationError(GL_INVALID_OPERATION, kInvalidFormatCombination);
        return false;
    }

    if (xoffset < 0 || yoffset < 0)
    {
        context->validationError(GL_INVALID_VALUE, kNegativeOffset);
        return false;
    }

    if (static_cast<size_t>(xoffset + width) > dest->getWidth(destTarget, destLevel) ||
        static_cast<size_t>(yoffset + height) > dest->getHeight(destTarget, destLevel))
    {
        context->validationError(GL_INVALID_VALUE, kOffsetOverflow);
        return false;
    }

    return true;
}

bool ValidateCompressedCopyTextureCHROMIUM(const Context *context,
                                           TextureID sourceId,
                                           TextureID destId)
{
    if (!context->getExtensions().copyCompressedTexture)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    const Texture *source = context->getTexture(sourceId);
    if (source == nullptr)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidSourceTexture);
        return false;
    }

    if (source->getType() != TextureType::_2D)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidSourceTextureType);
        return false;
    }

    if (source->getWidth(TextureTarget::_2D, 0) == 0 ||
        source->getHeight(TextureTarget::_2D, 0) == 0)
    {
        context->validationError(GL_INVALID_VALUE, kSourceTextureLevelZeroDefined);
        return false;
    }

    const Format &sourceFormat = source->getFormat(TextureTarget::_2D, 0);
    if (!sourceFormat.info->compressed)
    {
        context->validationError(GL_INVALID_OPERATION, kSourceTextureMustBeCompressed);
        return false;
    }

    const Texture *dest = context->getTexture(destId);
    if (dest == nullptr)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidDestinationTexture);
        return false;
    }

    if (dest->getType() != TextureType::_2D)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidDestinationTextureType);
        return false;
    }

    if (dest->getImmutableFormat())
    {
        context->validationError(GL_INVALID_OPERATION, kDestinationImmutable);
        return false;
    }

    return true;
}

bool ValidateCreateShader(const Context *context, ShaderType type)
{
    switch (type)
    {
        case ShaderType::Vertex:
        case ShaderType::Fragment:
            break;

        case ShaderType::Compute:
            if (context->getClientVersion() < Version(3, 1))
            {
                context->validationError(GL_INVALID_ENUM, kES31Required);
                return false;
            }
            break;

        case ShaderType::Geometry:
            if (!context->getExtensions().geometryShader)
            {
                context->validationError(GL_INVALID_ENUM, kInvalidShaderType);
                return false;
            }
            break;
        default:
            context->validationError(GL_INVALID_ENUM, kInvalidShaderType);
            return false;
    }

    return true;
}

bool ValidateBufferData(const Context *context,
                        BufferBinding target,
                        GLsizeiptr size,
                        const void *data,
                        BufferUsage usage)
{
    if (size < 0)
    {
        context->validationError(GL_INVALID_VALUE, kNegativeSize);
        return false;
    }

    switch (usage)
    {
        case BufferUsage::StreamDraw:
        case BufferUsage::StaticDraw:
        case BufferUsage::DynamicDraw:
            break;

        case BufferUsage::StreamRead:
        case BufferUsage::StaticRead:
        case BufferUsage::DynamicRead:
        case BufferUsage::StreamCopy:
        case BufferUsage::StaticCopy:
        case BufferUsage::DynamicCopy:
            if (context->getClientMajorVersion() < 3)
            {
                context->validationError(GL_INVALID_ENUM, kInvalidBufferUsage);
                return false;
            }
            break;

        default:
            context->validationError(GL_INVALID_ENUM, kInvalidBufferUsage);
            return false;
    }

    if (!context->isValidBufferBinding(target))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidBufferTypes);
        return false;
    }

    Buffer *buffer = context->getState().getTargetBuffer(target);

    if (!buffer)
    {
        context->validationError(GL_INVALID_OPERATION, kBufferNotBound);
        return false;
    }

    if (context->getExtensions().webglCompatibility &&
        buffer->isBoundForTransformFeedbackAndOtherUse())
    {
        context->validationError(GL_INVALID_OPERATION, kBufferBoundForTransformFeedback);
        return false;
    }

    return true;
}

bool ValidateBufferSubData(const Context *context,
                           BufferBinding target,
                           GLintptr offset,
                           GLsizeiptr size,
                           const void *data)
{
    if (size < 0)
    {
        context->validationError(GL_INVALID_VALUE, kNegativeSize);
        return false;
    }

    if (offset < 0)
    {
        context->validationError(GL_INVALID_VALUE, kNegativeOffset);
        return false;
    }

    if (!context->isValidBufferBinding(target))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidBufferTypes);
        return false;
    }

    Buffer *buffer = context->getState().getTargetBuffer(target);

    if (!buffer)
    {
        context->validationError(GL_INVALID_OPERATION, kBufferNotBound);
        return false;
    }

    if (buffer->isMapped())
    {
        context->validationError(GL_INVALID_OPERATION, kBufferMapped);
        return false;
    }

    if (context->getExtensions().webglCompatibility &&
        buffer->isBoundForTransformFeedbackAndOtherUse())
    {
        context->validationError(GL_INVALID_OPERATION, kBufferBoundForTransformFeedback);
        return false;
    }

    // Check for possible overflow of size + offset
    angle::CheckedNumeric<size_t> checkedSize(size);
    checkedSize += offset;
    if (!checkedSize.IsValid())
    {
        context->validationError(GL_INVALID_VALUE, kParamOverflow);
        return false;
    }

    if (size + offset > buffer->getSize())
    {
        context->validationError(GL_INVALID_VALUE, kInsufficientBufferSize);
        return false;
    }

    return true;
}

bool ValidateRequestExtensionANGLE(const Context *context, const GLchar *name)
{
    if (!context->getExtensions().requestExtension)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    if (!context->isExtensionRequestable(name))
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotRequestable);
        return false;
    }

    return true;
}

bool ValidateDisableExtensionANGLE(const Context *context, const GLchar *name)
{
    if (!context->getExtensions().requestExtension)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    if (!context->isExtensionDisablable(name))
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotDisablable);
        return false;
    }

    return true;
}

bool ValidateActiveTexture(const Context *context, GLenum texture)
{
    if (context->getClientMajorVersion() < 2)
    {
        return ValidateMultitextureUnit(context, texture);
    }

    if (texture < GL_TEXTURE0 ||
        texture >
            GL_TEXTURE0 + static_cast<GLuint>(context->getCaps().maxCombinedTextureImageUnits) - 1)
    {
        context->validationError(GL_INVALID_ENUM, kInvalidCombinedImageUnit);
        return false;
    }

    return true;
}

bool ValidateAttachShader(const Context *context, ShaderProgramID program, ShaderProgramID shader)
{
    Program *programObject = GetValidProgram(context, program);
    if (!programObject)
    {
        return false;
    }

    Shader *shaderObject = GetValidShader(context, shader);
    if (!shaderObject)
    {
        return false;
    }

    if (programObject->getAttachedShader(shaderObject->getType()) &&
        !programObject->getState().isShaderMarkedForDetach(shaderObject->getType()))
    {
        context->validationError(GL_INVALID_OPERATION, kShaderAttachmentHasShader);
        return false;
    }

    return true;
}

bool ValidateBindAttribLocation(const Context *context,
                                ShaderProgramID program,
                                GLuint index,
                                const GLchar *name)
{
    if (index >= MAX_VERTEX_ATTRIBS)
    {
        context->validationError(GL_INVALID_VALUE, kIndexExceedsMaxVertexAttribute);
        return false;
    }

    if (strncmp(name, "gl_", 3) == 0)
    {
        context->validationError(GL_INVALID_OPERATION, kNameBeginsWithGL);
        return false;
    }

    if (context->isWebGL())
    {
        const size_t length = strlen(name);

        if (!IsValidESSLString(name, length))
        {
            // The WebGL spec (section 6.20) disallows strings containing invalid ESSL characters
            // for shader-related entry points
            context->validationError(GL_INVALID_VALUE, kInvalidNameCharacters);
            return false;
        }

        if (!ValidateWebGLNameLength(context, length) || !ValidateWebGLNamePrefix(context, name))
        {
            return false;
        }
    }

    return GetValidProgram(context, program) != nullptr;
}

bool ValidateBindFramebuffer(const Context *context, GLenum target, FramebufferID framebuffer)
{
    if (!ValidFramebufferTarget(context, target))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidFramebufferTarget);
        return false;
    }

    if (!context->getState().isBindGeneratesResourceEnabled() &&
        !context->isFramebufferGenerated(framebuffer))
    {
        context->validationError(GL_INVALID_OPERATION, kObjectNotGenerated);
        return false;
    }

    return true;
}

bool ValidateBindRenderbuffer(const Context *context, GLenum target, RenderbufferID renderbuffer)
{
    if (target != GL_RENDERBUFFER)
    {
        context->validationError(GL_INVALID_ENUM, kInvalidRenderbufferTarget);
        return false;
    }

    if (!context->getState().isBindGeneratesResourceEnabled() &&
        !context->isRenderbufferGenerated(renderbuffer))
    {
        context->validationError(GL_INVALID_OPERATION, kObjectNotGenerated);
        return false;
    }

    return true;
}

static bool ValidBlendEquationMode(const Context *context, GLenum mode)
{
    switch (mode)
    {
        case GL_FUNC_ADD:
        case GL_FUNC_SUBTRACT:
        case GL_FUNC_REVERSE_SUBTRACT:
            return true;

        case GL_MIN:
        case GL_MAX:
            return context->getClientVersion() >= ES_3_0 || context->getExtensions().blendMinMax;

        default:
            return false;
    }
}

bool ValidateBlendColor(const Context *context,
                        GLfloat red,
                        GLfloat green,
                        GLfloat blue,
                        GLfloat alpha)
{
    return true;
}

bool ValidateBlendEquation(const Context *context, GLenum mode)
{
    if (!ValidBlendEquationMode(context, mode))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidBlendEquation);
        return false;
    }

    return true;
}

bool ValidateBlendEquationSeparate(const Context *context, GLenum modeRGB, GLenum modeAlpha)
{
    if (!ValidBlendEquationMode(context, modeRGB))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidBlendEquation);
        return false;
    }

    if (!ValidBlendEquationMode(context, modeAlpha))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidBlendEquation);
        return false;
    }

    return true;
}

bool ValidateBlendFunc(const Context *context, GLenum sfactor, GLenum dfactor)
{
    return ValidateBlendFuncSeparate(context, sfactor, dfactor, sfactor, dfactor);
}

bool ValidateBlendFuncSeparate(const Context *context,
                               GLenum srcRGB,
                               GLenum dstRGB,
                               GLenum srcAlpha,
                               GLenum dstAlpha)
{
    if (!ValidSrcBlendFunc(context, srcRGB))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidBlendFunction);
        return false;
    }

    if (!ValidDstBlendFunc(context, dstRGB))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidBlendFunction);
        return false;
    }

    if (!ValidSrcBlendFunc(context, srcAlpha))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidBlendFunction);
        return false;
    }

    if (!ValidDstBlendFunc(context, dstAlpha))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidBlendFunction);
        return false;
    }

    if (context->getLimitations().noSimultaneousConstantColorAndAlphaBlendFunc ||
        context->getExtensions().webglCompatibility)
    {
        bool constantColorUsed =
            (srcRGB == GL_CONSTANT_COLOR || srcRGB == GL_ONE_MINUS_CONSTANT_COLOR ||
             dstRGB == GL_CONSTANT_COLOR || dstRGB == GL_ONE_MINUS_CONSTANT_COLOR);

        bool constantAlphaUsed =
            (srcRGB == GL_CONSTANT_ALPHA || srcRGB == GL_ONE_MINUS_CONSTANT_ALPHA ||
             dstRGB == GL_CONSTANT_ALPHA || dstRGB == GL_ONE_MINUS_CONSTANT_ALPHA);

        if (constantColorUsed && constantAlphaUsed)
        {
            if (context->getExtensions().webglCompatibility)
            {
                context->validationError(GL_INVALID_OPERATION, kInvalidConstantColor);
                return false;
            }

            WARN() << kConstantColorAlphaLimitation;
            context->validationError(GL_INVALID_OPERATION, kConstantColorAlphaLimitation);
            return false;
        }
    }

    return true;
}

bool ValidateGetString(const Context *context, GLenum name)
{
    switch (name)
    {
        case GL_VENDOR:
        case GL_RENDERER:
        case GL_VERSION:
        case GL_SHADING_LANGUAGE_VERSION:
        case GL_EXTENSIONS:
            break;

        case GL_REQUESTABLE_EXTENSIONS_ANGLE:
            if (!context->getExtensions().requestExtension)
            {
                context->validationError(GL_INVALID_ENUM, kInvalidName);
                return false;
            }
            break;

        default:
            context->validationError(GL_INVALID_ENUM, kInvalidName);
            return false;
    }

    return true;
}

bool ValidateLineWidth(const Context *context, GLfloat width)
{
    if (width <= 0.0f || isNaN(width))
    {
        context->validationError(GL_INVALID_VALUE, kInvalidWidth);
        return false;
    }

    return true;
}

bool ValidateDepthRangef(const Context *context, GLfloat zNear, GLfloat zFar)
{
    if (context->getExtensions().webglCompatibility && zNear > zFar)
    {
        context->validationError(GL_INVALID_OPERATION, kInvalidDepthRange);
        return false;
    }

    return true;
}

bool ValidateRenderbufferStorage(const Context *context,
                                 GLenum target,
                                 GLenum internalformat,
                                 GLsizei width,
                                 GLsizei height)
{
    return ValidateRenderbufferStorageParametersBase(context, target, 0, internalformat, width,
                                                     height);
}

bool ValidateRenderbufferStorageMultisampleANGLE(const Context *context,
                                                 GLenum target,
                                                 GLsizei samples,
                                                 GLenum internalformat,
                                                 GLsizei width,
                                                 GLsizei height)
{
    if (!context->getExtensions().framebufferMultisample)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    // ANGLE_framebuffer_multisample states that the value of samples must be less than or equal
    // to MAX_SAMPLES_ANGLE (Context::getCaps().maxSamples) otherwise GL_INVALID_VALUE is
    // generated.
    if (samples > context->getCaps().maxSamples)
    {
        context->validationError(GL_INVALID_VALUE, kSamplesOutOfRange);
        return false;
    }

    // ANGLE_framebuffer_multisample states GL_OUT_OF_MEMORY is generated on a failure to create
    // the specified storage. This is different than ES 3.0 in which a sample number higher
    // than the maximum sample number supported by this format generates a GL_INVALID_VALUE.
    // The TextureCaps::getMaxSamples method is only guarenteed to be valid when the context is ES3.
    if (context->getClientMajorVersion() >= 3)
    {
        const TextureCaps &formatCaps = context->getTextureCaps().get(internalformat);
        if (static_cast<GLuint>(samples) > formatCaps.getMaxSamples())
        {
            context->validationError(GL_OUT_OF_MEMORY, kSamplesOutOfRange);
            return false;
        }
    }

    return ValidateRenderbufferStorageParametersBase(context, target, samples, internalformat,
                                                     width, height);
}

bool ValidateCheckFramebufferStatus(const Context *context, GLenum target)
{
    if (!ValidFramebufferTarget(context, target))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidFramebufferTarget);
        return false;
    }

    return true;
}

bool ValidateClearColor(const Context *context,
                        GLfloat red,
                        GLfloat green,
                        GLfloat blue,
                        GLfloat alpha)
{
    return true;
}

bool ValidateClearDepthf(const Context *context, GLfloat depth)
{
    return true;
}

bool ValidateClearStencil(const Context *context, GLint s)
{
    return true;
}

bool ValidateColorMask(const Context *context,
                       GLboolean red,
                       GLboolean green,
                       GLboolean blue,
                       GLboolean alpha)
{
    return true;
}

bool ValidateCompileShader(const Context *context, ShaderProgramID shader)
{
    return true;
}

bool ValidateCreateProgram(const Context *context)
{
    return true;
}

bool ValidateCullFace(const Context *context, CullFaceMode mode)
{
    switch (mode)
    {
        case CullFaceMode::Front:
        case CullFaceMode::Back:
        case CullFaceMode::FrontAndBack:
            break;

        default:
            context->validationError(GL_INVALID_ENUM, kInvalidCullMode);
            return false;
    }

    return true;
}

bool ValidateDeleteProgram(const Context *context, ShaderProgramID program)
{
    if (program.value == 0)
    {
        return false;
    }

    if (!context->getProgramResolveLink(program))
    {
        if (context->getShader(program))
        {
            context->validationError(GL_INVALID_OPERATION, kExpectedProgramName);
            return false;
        }
        else
        {
            context->validationError(GL_INVALID_VALUE, kInvalidProgramName);
            return false;
        }
    }

    return true;
}

bool ValidateDeleteShader(const Context *context, ShaderProgramID shader)
{
    if (shader.value == 0)
    {
        return false;
    }

    if (!context->getShader(shader))
    {
        if (context->getProgramResolveLink(shader))
        {
            context->validationError(GL_INVALID_OPERATION, kInvalidShaderName);
            return false;
        }
        else
        {
            context->validationError(GL_INVALID_VALUE, kExpectedShaderName);
            return false;
        }
    }

    return true;
}

bool ValidateDepthFunc(const Context *context, GLenum func)
{
    switch (func)
    {
        case GL_NEVER:
        case GL_ALWAYS:
        case GL_LESS:
        case GL_LEQUAL:
        case GL_EQUAL:
        case GL_GREATER:
        case GL_GEQUAL:
        case GL_NOTEQUAL:
            break;

        default:
            context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
            return false;
    }

    return true;
}

bool ValidateDepthMask(const Context *context, GLboolean flag)
{
    return true;
}

bool ValidateDetachShader(const Context *context, ShaderProgramID program, ShaderProgramID shader)
{
    Program *programObject = GetValidProgram(context, program);
    if (!programObject)
    {
        return false;
    }

    Shader *shaderObject = GetValidShader(context, shader);
    if (!shaderObject)
    {
        return false;
    }

    const Shader *attachedShader = programObject->getAttachedShader(shaderObject->getType());
    if (attachedShader != shaderObject)
    {
        context->validationError(GL_INVALID_OPERATION, kShaderToDetachMustBeAttached);
        return false;
    }

    return true;
}

bool ValidateDisableVertexAttribArray(const Context *context, GLuint index)
{
    if (index >= MAX_VERTEX_ATTRIBS)
    {
        context->validationError(GL_INVALID_VALUE, kIndexExceedsMaxVertexAttribute);
        return false;
    }

    return true;
}

bool ValidateEnableVertexAttribArray(const Context *context, GLuint index)
{
    if (index >= MAX_VERTEX_ATTRIBS)
    {
        context->validationError(GL_INVALID_VALUE, kIndexExceedsMaxVertexAttribute);
        return false;
    }

    return true;
}

bool ValidateFinish(const Context *context)
{
    return true;
}

bool ValidateFlush(const Context *context)
{
    return true;
}

bool ValidateFrontFace(const Context *context, GLenum mode)
{
    switch (mode)
    {
        case GL_CW:
        case GL_CCW:
            break;
        default:
            context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
            return false;
    }

    return true;
}

bool ValidateGetActiveAttrib(const Context *context,
                             ShaderProgramID program,
                             GLuint index,
                             GLsizei bufsize,
                             const GLsizei *length,
                             const GLint *size,
                             const GLenum *type,
                             const GLchar *name)
{
    if (bufsize < 0)
    {
        context->validationError(GL_INVALID_VALUE, kNegativeBufferSize);
        return false;
    }

    Program *programObject = GetValidProgram(context, program);

    if (!programObject)
    {
        return false;
    }

    if (index >= static_cast<GLuint>(programObject->getActiveAttributeCount()))
    {
        context->validationError(GL_INVALID_VALUE, kIndexExceedsMaxActiveUniform);
        return false;
    }

    return true;
}

bool ValidateGetActiveUniform(const Context *context,
                              ShaderProgramID program,
                              GLuint index,
                              GLsizei bufsize,
                              const GLsizei *length,
                              const GLint *size,
                              const GLenum *type,
                              const GLchar *name)
{
    if (bufsize < 0)
    {
        context->validationError(GL_INVALID_VALUE, kNegativeBufferSize);
        return false;
    }

    Program *programObject = GetValidProgram(context, program);

    if (!programObject)
    {
        return false;
    }

    if (index >= static_cast<GLuint>(programObject->getActiveUniformCount()))
    {
        context->validationError(GL_INVALID_VALUE, kIndexExceedsMaxActiveUniform);
        return false;
    }

    return true;
}

bool ValidateGetAttachedShaders(const Context *context,
                                ShaderProgramID program,
                                GLsizei maxcount,
                                const GLsizei *count,
                                const ShaderProgramID *shaders)
{
    if (maxcount < 0)
    {
        context->validationError(GL_INVALID_VALUE, kNegativeMaxCount);
        return false;
    }

    Program *programObject = GetValidProgram(context, program);

    if (!programObject)
    {
        return false;
    }

    return true;
}

bool ValidateGetAttribLocation(const Context *context, ShaderProgramID program, const GLchar *name)
{
    // The WebGL spec (section 6.20) disallows strings containing invalid ESSL characters for
    // shader-related entry points
    if (context->getExtensions().webglCompatibility && !IsValidESSLString(name, strlen(name)))
    {
        context->validationError(GL_INVALID_VALUE, kInvalidNameCharacters);
        return false;
    }

    Program *programObject = GetValidProgram(context, program);

    if (!programObject)
    {
        context->validationError(GL_INVALID_OPERATION, kProgramNotBound);
        return false;
    }

    if (!programObject->isLinked())
    {
        context->validationError(GL_INVALID_OPERATION, kProgramNotLinked);
        return false;
    }

    return true;
}

bool ValidateGetBooleanv(const Context *context, GLenum pname, const GLboolean *params)
{
    GLenum nativeType;
    unsigned int numParams = 0;
    return ValidateStateQuery(context, pname, &nativeType, &numParams);
}

bool ValidateGetError(const Context *context)
{
    return true;
}

bool ValidateGetFloatv(const Context *context, GLenum pname, const GLfloat *params)
{
    GLenum nativeType;
    unsigned int numParams = 0;
    return ValidateStateQuery(context, pname, &nativeType, &numParams);
}

bool ValidateGetIntegerv(const Context *context, GLenum pname, const GLint *params)
{
    GLenum nativeType;
    unsigned int numParams = 0;
    return ValidateStateQuery(context, pname, &nativeType, &numParams);
}

bool ValidateGetProgramInfoLog(const Context *context,
                               ShaderProgramID program,
                               GLsizei bufsize,
                               const GLsizei *length,
                               const GLchar *infolog)
{
    if (bufsize < 0)
    {
        context->validationError(GL_INVALID_VALUE, kNegativeBufferSize);
        return false;
    }

    Program *programObject = GetValidProgram(context, program);
    if (!programObject)
    {
        return false;
    }

    return true;
}

bool ValidateGetShaderInfoLog(const Context *context,
                              ShaderProgramID shader,
                              GLsizei bufsize,
                              const GLsizei *length,
                              const GLchar *infolog)
{
    if (bufsize < 0)
    {
        context->validationError(GL_INVALID_VALUE, kNegativeBufferSize);
        return false;
    }

    Shader *shaderObject = GetValidShader(context, shader);
    if (!shaderObject)
    {
        return false;
    }

    return true;
}

bool ValidateGetShaderPrecisionFormat(const Context *context,
                                      GLenum shadertype,
                                      GLenum precisiontype,
                                      const GLint *range,
                                      const GLint *precision)
{
    switch (shadertype)
    {
        case GL_VERTEX_SHADER:
        case GL_FRAGMENT_SHADER:
            break;
        case GL_COMPUTE_SHADER:
            context->validationError(GL_INVALID_OPERATION, kUnimplementedComputeShaderPrecision);
            return false;
        default:
            context->validationError(GL_INVALID_ENUM, kInvalidShaderType);
            return false;
    }

    switch (precisiontype)
    {
        case GL_LOW_FLOAT:
        case GL_MEDIUM_FLOAT:
        case GL_HIGH_FLOAT:
        case GL_LOW_INT:
        case GL_MEDIUM_INT:
        case GL_HIGH_INT:
            break;

        default:
            context->validationError(GL_INVALID_ENUM, kInvalidPrecision);
            return false;
    }

    return true;
}

bool ValidateGetShaderSource(const Context *context,
                             ShaderProgramID shader,
                             GLsizei bufsize,
                             const GLsizei *length,
                             const GLchar *source)
{
    if (bufsize < 0)
    {
        context->validationError(GL_INVALID_VALUE, kNegativeBufferSize);
        return false;
    }

    Shader *shaderObject = GetValidShader(context, shader);
    if (!shaderObject)
    {
        return false;
    }

    return true;
}

bool ValidateGetUniformLocation(const Context *context, ShaderProgramID program, const GLchar *name)
{
    if (strstr(name, "gl_") == name)
    {
        return false;
    }

    // The WebGL spec (section 6.20) disallows strings containing invalid ESSL characters for
    // shader-related entry points
    if (context->getExtensions().webglCompatibility && !IsValidESSLString(name, strlen(name)))
    {
        context->validationError(GL_INVALID_VALUE, kInvalidNameCharacters);
        return false;
    }

    Program *programObject = GetValidProgram(context, program);

    if (!programObject)
    {
        return false;
    }

    if (!programObject->isLinked())
    {
        context->validationError(GL_INVALID_OPERATION, kProgramNotLinked);
        return false;
    }

    return true;
}

bool ValidateHint(const Context *context, GLenum target, GLenum mode)
{
    switch (mode)
    {
        case GL_FASTEST:
        case GL_NICEST:
        case GL_DONT_CARE:
            break;

        default:
            context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
            return false;
    }

    switch (target)
    {
        case GL_GENERATE_MIPMAP_HINT:
            break;

        case GL_TEXTURE_FILTERING_HINT_CHROMIUM:
            if (!context->getExtensions().textureFilteringCHROMIUM)
            {
                context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                return false;
            }
            break;

        case GL_FRAGMENT_SHADER_DERIVATIVE_HINT:
            if (context->getClientVersion() < ES_3_0 &&
                !context->getExtensions().standardDerivativesOES)
            {
                context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                return false;
            }
            break;

        case GL_PERSPECTIVE_CORRECTION_HINT:
        case GL_POINT_SMOOTH_HINT:
        case GL_LINE_SMOOTH_HINT:
        case GL_FOG_HINT:
            if (context->getClientMajorVersion() >= 2)
            {
                context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                return false;
            }
            break;

        default:
            context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
            return false;
    }

    return true;
}

bool ValidateIsBuffer(const Context *context, BufferID buffer)
{
    return true;
}

bool ValidateIsFramebuffer(const Context *context, FramebufferID framebuffer)
{
    return true;
}

bool ValidateIsProgram(const Context *context, ShaderProgramID program)
{
    return true;
}

bool ValidateIsRenderbuffer(const Context *context, RenderbufferID renderbuffer)
{
    return true;
}

bool ValidateIsShader(const Context *context, ShaderProgramID shader)
{
    return true;
}

bool ValidateIsTexture(const Context *context, TextureID texture)
{
    return true;
}

bool ValidatePixelStorei(const Context *context, GLenum pname, GLint param)
{
    if (context->getClientMajorVersion() < 3)
    {
        switch (pname)
        {
            case GL_UNPACK_IMAGE_HEIGHT:
            case GL_UNPACK_SKIP_IMAGES:
                context->validationError(GL_INVALID_ENUM, kInvalidPname);
                return false;

            case GL_UNPACK_ROW_LENGTH:
            case GL_UNPACK_SKIP_ROWS:
            case GL_UNPACK_SKIP_PIXELS:
                if (!context->getExtensions().unpackSubimage)
                {
                    context->validationError(GL_INVALID_ENUM, kInvalidPname);
                    return false;
                }
                break;

            case GL_PACK_ROW_LENGTH:
            case GL_PACK_SKIP_ROWS:
            case GL_PACK_SKIP_PIXELS:
                if (!context->getExtensions().packSubimage)
                {
                    context->validationError(GL_INVALID_ENUM, kInvalidPname);
                    return false;
                }
                break;
        }
    }

    if (param < 0)
    {
        context->validationError(GL_INVALID_VALUE, kNegativeParam);
        return false;
    }

    switch (pname)
    {
        case GL_UNPACK_ALIGNMENT:
            if (param != 1 && param != 2 && param != 4 && param != 8)
            {
                context->validationError(GL_INVALID_VALUE, kInvalidUnpackAlignment);
                return false;
            }
            break;

        case GL_PACK_ALIGNMENT:
            if (param != 1 && param != 2 && param != 4 && param != 8)
            {
                context->validationError(GL_INVALID_VALUE, kInvalidUnpackAlignment);
                return false;
            }
            break;

        case GL_PACK_REVERSE_ROW_ORDER_ANGLE:
            if (!context->getExtensions().packReverseRowOrder)
            {
                context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
            }
            break;

        case GL_UNPACK_ROW_LENGTH:
        case GL_UNPACK_IMAGE_HEIGHT:
        case GL_UNPACK_SKIP_IMAGES:
        case GL_UNPACK_SKIP_ROWS:
        case GL_UNPACK_SKIP_PIXELS:
        case GL_PACK_ROW_LENGTH:
        case GL_PACK_SKIP_ROWS:
        case GL_PACK_SKIP_PIXELS:
            break;

        default:
            context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
            return false;
    }

    return true;
}

bool ValidatePolygonOffset(const Context *context, GLfloat factor, GLfloat units)
{
    return true;
}

bool ValidateReleaseShaderCompiler(const Context *context)
{
    return true;
}

bool ValidateSampleCoverage(const Context *context, GLfloat value, GLboolean invert)
{
    return true;
}

bool ValidateScissor(const Context *context, GLint x, GLint y, GLsizei width, GLsizei height)
{
    if (width < 0 || height < 0)
    {
        context->validationError(GL_INVALID_VALUE, kNegativeSize);
        return false;
    }

    return true;
}

bool ValidateShaderBinary(const Context *context,
                          GLsizei n,
                          const ShaderProgramID *shaders,
                          GLenum binaryformat,
                          const void *binary,
                          GLsizei length)
{
    const std::vector<GLenum> &shaderBinaryFormats = context->getCaps().shaderBinaryFormats;
    if (std::find(shaderBinaryFormats.begin(), shaderBinaryFormats.end(), binaryformat) ==
        shaderBinaryFormats.end())
    {
        context->validationError(GL_INVALID_ENUM, kInvalidShaderBinaryFormat);
        return false;
    }

    return true;
}

bool ValidateShaderSource(const Context *context,
                          ShaderProgramID shader,
                          GLsizei count,
                          const GLchar *const *string,
                          const GLint *length)
{
    if (count < 0)
    {
        context->validationError(GL_INVALID_VALUE, kNegativeCount);
        return false;
    }

    // The WebGL spec (section 6.20) disallows strings containing invalid ESSL characters for
    // shader-related entry points
    if (context->getExtensions().webglCompatibility)
    {
        for (GLsizei i = 0; i < count; i++)
        {
            size_t len =
                (length && length[i] >= 0) ? static_cast<size_t>(length[i]) : strlen(string[i]);

            // Backslash as line-continuation is allowed in WebGL 2.0.
            if (!IsValidESSLShaderSourceString(string[i], len,
                                               context->getClientVersion() >= ES_3_0))
            {
                context->validationError(GL_INVALID_VALUE, kShaderSourceInvalidCharacters);
                return false;
            }
        }
    }

    Shader *shaderObject = GetValidShader(context, shader);
    if (!shaderObject)
    {
        return false;
    }

    return true;
}

bool ValidateStencilFunc(const Context *context, GLenum func, GLint ref, GLuint mask)
{
    if (!IsValidStencilFunc(func))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidStencil);
        return false;
    }

    return true;
}

bool ValidateStencilFuncSeparate(const Context *context,
                                 GLenum face,
                                 GLenum func,
                                 GLint ref,
                                 GLuint mask)
{
    if (!IsValidStencilFace(face))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidStencil);
        return false;
    }

    if (!IsValidStencilFunc(func))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidStencil);
        return false;
    }

    return true;
}

bool ValidateStencilMask(const Context *context, GLuint mask)
{
    return true;
}

bool ValidateStencilMaskSeparate(const Context *context, GLenum face, GLuint mask)
{
    if (!IsValidStencilFace(face))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidStencil);
        return false;
    }

    return true;
}

bool ValidateStencilOp(const Context *context, GLenum fail, GLenum zfail, GLenum zpass)
{
    if (!IsValidStencilOp(fail))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidStencil);
        return false;
    }

    if (!IsValidStencilOp(zfail))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidStencil);
        return false;
    }

    if (!IsValidStencilOp(zpass))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidStencil);
        return false;
    }

    return true;
}

bool ValidateStencilOpSeparate(const Context *context,
                               GLenum face,
                               GLenum fail,
                               GLenum zfail,
                               GLenum zpass)
{
    if (!IsValidStencilFace(face))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidStencil);
        return false;
    }

    return ValidateStencilOp(context, fail, zfail, zpass);
}

bool ValidateUniform1f(const Context *context, UniformLocation location, GLfloat x)
{
    return ValidateUniform(context, GL_FLOAT, location, 1);
}

bool ValidateUniform1fv(const Context *context,
                        UniformLocation location,
                        GLsizei count,
                        const GLfloat *v)
{
    return ValidateUniform(context, GL_FLOAT, location, count);
}

bool ValidateUniform1i(const Context *context, UniformLocation location, GLint x)
{
    return ValidateUniform1iv(context, location, 1, &x);
}

bool ValidateUniform2fv(const Context *context,
                        UniformLocation location,
                        GLsizei count,
                        const GLfloat *v)
{
    return ValidateUniform(context, GL_FLOAT_VEC2, location, count);
}

bool ValidateUniform2i(const Context *context, UniformLocation location, GLint x, GLint y)
{
    return ValidateUniform(context, GL_INT_VEC2, location, 1);
}

bool ValidateUniform2iv(const Context *context,
                        UniformLocation location,
                        GLsizei count,
                        const GLint *v)
{
    return ValidateUniform(context, GL_INT_VEC2, location, count);
}

bool ValidateUniform3f(const Context *context,
                       UniformLocation location,
                       GLfloat x,
                       GLfloat y,
                       GLfloat z)
{
    return ValidateUniform(context, GL_FLOAT_VEC3, location, 1);
}

bool ValidateUniform3fv(const Context *context,
                        UniformLocation location,
                        GLsizei count,
                        const GLfloat *v)
{
    return ValidateUniform(context, GL_FLOAT_VEC3, location, count);
}

bool ValidateUniform3i(const Context *context, UniformLocation location, GLint x, GLint y, GLint z)
{
    return ValidateUniform(context, GL_INT_VEC3, location, 1);
}

bool ValidateUniform3iv(const Context *context,
                        UniformLocation location,
                        GLsizei count,
                        const GLint *v)
{
    return ValidateUniform(context, GL_INT_VEC3, location, count);
}

bool ValidateUniform4f(const Context *context,
                       UniformLocation location,
                       GLfloat x,
                       GLfloat y,
                       GLfloat z,
                       GLfloat w)
{
    return ValidateUniform(context, GL_FLOAT_VEC4, location, 1);
}

bool ValidateUniform4fv(const Context *context,
                        UniformLocation location,
                        GLsizei count,
                        const GLfloat *v)
{
    return ValidateUniform(context, GL_FLOAT_VEC4, location, count);
}

bool ValidateUniform4i(const Context *context,
                       UniformLocation location,
                       GLint x,
                       GLint y,
                       GLint z,
                       GLint w)
{
    return ValidateUniform(context, GL_INT_VEC4, location, 1);
}

bool ValidateUniform4iv(const Context *context,
                        UniformLocation location,
                        GLsizei count,
                        const GLint *v)
{
    return ValidateUniform(context, GL_INT_VEC4, location, count);
}

bool ValidateUniformMatrix2fv(const Context *context,
                              UniformLocation location,
                              GLsizei count,
                              GLboolean transpose,
                              const GLfloat *value)
{
    return ValidateUniformMatrix(context, GL_FLOAT_MAT2, location, count, transpose);
}

bool ValidateUniformMatrix3fv(const Context *context,
                              UniformLocation location,
                              GLsizei count,
                              GLboolean transpose,
                              const GLfloat *value)
{
    return ValidateUniformMatrix(context, GL_FLOAT_MAT3, location, count, transpose);
}

bool ValidateUniformMatrix4fv(const Context *context,
                              UniformLocation location,
                              GLsizei count,
                              GLboolean transpose,
                              const GLfloat *value)
{
    return ValidateUniformMatrix(context, GL_FLOAT_MAT4, location, count, transpose);
}

bool ValidateValidateProgram(const Context *context, ShaderProgramID program)
{
    Program *programObject = GetValidProgram(context, program);

    if (!programObject)
    {
        return false;
    }

    return true;
}

bool ValidateVertexAttrib1f(const Context *context, GLuint index, GLfloat x)
{
    return ValidateVertexAttribIndex(context, index);
}

bool ValidateVertexAttrib1fv(const Context *context, GLuint index, const GLfloat *values)
{
    return ValidateVertexAttribIndex(context, index);
}

bool ValidateVertexAttrib2f(const Context *context, GLuint index, GLfloat x, GLfloat y)
{
    return ValidateVertexAttribIndex(context, index);
}

bool ValidateVertexAttrib2fv(const Context *context, GLuint index, const GLfloat *values)
{
    return ValidateVertexAttribIndex(context, index);
}

bool ValidateVertexAttrib3f(const Context *context, GLuint index, GLfloat x, GLfloat y, GLfloat z)
{
    return ValidateVertexAttribIndex(context, index);
}

bool ValidateVertexAttrib3fv(const Context *context, GLuint index, const GLfloat *values)
{
    return ValidateVertexAttribIndex(context, index);
}

bool ValidateVertexAttrib4f(const Context *context,
                            GLuint index,
                            GLfloat x,
                            GLfloat y,
                            GLfloat z,
                            GLfloat w)
{
    return ValidateVertexAttribIndex(context, index);
}

bool ValidateVertexAttrib4fv(const Context *context, GLuint index, const GLfloat *values)
{
    return ValidateVertexAttribIndex(context, index);
}

bool ValidateViewport(const Context *context, GLint x, GLint y, GLsizei width, GLsizei height)
{
    if (width < 0 || height < 0)
    {
        context->validationError(GL_INVALID_VALUE, kViewportNegativeSize);
        return false;
    }

    return true;
}

bool ValidateGetFramebufferAttachmentParameteriv(const Context *context,
                                                 GLenum target,
                                                 GLenum attachment,
                                                 GLenum pname,
                                                 const GLint *params)
{
    return ValidateGetFramebufferAttachmentParameterivBase(context, target, attachment, pname,
                                                           nullptr);
}

bool ValidateGetProgramiv(const Context *context,
                          ShaderProgramID program,
                          GLenum pname,
                          const GLint *params)
{
    return ValidateGetProgramivBase(context, program, pname, nullptr);
}

bool ValidateCopyTexImage2D(const Context *context,
                            TextureTarget target,
                            GLint level,
                            GLenum internalformat,
                            GLint x,
                            GLint y,
                            GLsizei width,
                            GLsizei height,
                            GLint border)
{
    if (context->getClientMajorVersion() < 3)
    {
        return ValidateES2CopyTexImageParameters(context, target, level, internalformat, false, 0,
                                                 0, x, y, width, height, border);
    }

    ASSERT(context->getClientMajorVersion() == 3);
    return ValidateES3CopyTexImage2DParameters(context, target, level, internalformat, false, 0, 0,
                                               0, x, y, width, height, border);
}

bool ValidateCopyTexSubImage2D(const Context *context,
                               TextureTarget target,
                               GLint level,
                               GLint xoffset,
                               GLint yoffset,
                               GLint x,
                               GLint y,
                               GLsizei width,
                               GLsizei height)
{
    if (context->getClientMajorVersion() < 3)
    {
        return ValidateES2CopyTexImageParameters(context, target, level, GL_NONE, true, xoffset,
                                                 yoffset, x, y, width, height, 0);
    }

    return ValidateES3CopyTexImage2DParameters(context, target, level, GL_NONE, true, xoffset,
                                               yoffset, 0, x, y, width, height, 0);
}

bool ValidateCopyTexSubImage3DOES(const Context *context,
                                  TextureTarget target,
                                  GLint level,
                                  GLint xoffset,
                                  GLint yoffset,
                                  GLint zoffset,
                                  GLint x,
                                  GLint y,
                                  GLsizei width,
                                  GLsizei height)
{
    return ValidateCopyTexSubImage3D(context, target, level, xoffset, yoffset, zoffset, x, y, width,
                                     height);
}

bool ValidateDeleteBuffers(const Context *context, GLint n, const BufferID *buffers)
{
    return ValidateGenOrDelete(context, n);
}

bool ValidateDeleteFramebuffers(const Context *context, GLint n, const FramebufferID *framebuffers)
{
    return ValidateGenOrDelete(context, n);
}

bool ValidateDeleteRenderbuffers(const Context *context,
                                 GLint n,
                                 const RenderbufferID *renderbuffers)
{
    return ValidateGenOrDelete(context, n);
}

bool ValidateDeleteTextures(const Context *context, GLint n, const TextureID *textures)
{
    return ValidateGenOrDelete(context, n);
}

bool ValidateDisable(const Context *context, GLenum cap)
{
    if (!ValidCap(context, cap, false))
    {
        context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
        return false;
    }

    return true;
}

bool ValidateEnable(const Context *context, GLenum cap)
{
    if (!ValidCap(context, cap, false))
    {
        context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
        return false;
    }

    if (context->getLimitations().noSampleAlphaToCoverageSupport &&
        cap == GL_SAMPLE_ALPHA_TO_COVERAGE)
    {
        context->validationError(GL_INVALID_OPERATION, kNoSampleAlphaToCoveragesLimitation);

        // We also output an error message to the debugger window if tracing is active, so that
        // developers can see the error message.
        ERR() << kNoSampleAlphaToCoveragesLimitation;
        return false;
    }

    return true;
}

bool ValidateFramebufferRenderbuffer(const Context *context,
                                     GLenum target,
                                     GLenum attachment,
                                     GLenum renderbuffertarget,
                                     RenderbufferID renderbuffer)
{
    if (!ValidFramebufferTarget(context, target))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidFramebufferTarget);
        return false;
    }

    if (renderbuffertarget != GL_RENDERBUFFER && renderbuffer.value != 0)
    {
        context->validationError(GL_INVALID_ENUM, kInvalidRenderbufferTarget);
        return false;
    }

    return ValidateFramebufferRenderbufferParameters(context, target, attachment,
                                                     renderbuffertarget, renderbuffer);
}

bool ValidateFramebufferTexture2D(const Context *context,
                                  GLenum target,
                                  GLenum attachment,
                                  TextureTarget textarget,
                                  TextureID texture,
                                  GLint level)
{
    // Attachments are required to be bound to level 0 without ES3 or the GL_OES_fbo_render_mipmap
    // extension
    if (context->getClientMajorVersion() < 3 && !context->getExtensions().fboRenderMipmapOES &&
        level != 0)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidFramebufferTextureLevel);
        return false;
    }

    if (!ValidateFramebufferTextureBase(context, target, attachment, texture, level))
    {
        return false;
    }

    if (texture.value != 0)
    {
        Texture *tex = context->getTexture(texture);
        ASSERT(tex);

        const Caps &caps = context->getCaps();

        switch (textarget)
        {
            case TextureTarget::_2D:
            {
                if (level > log2(caps.max2DTextureSize))
                {
                    context->validationError(GL_INVALID_VALUE, kInvalidMipLevel);
                    return false;
                }
                if (tex->getType() != TextureType::_2D)
                {
                    context->validationError(GL_INVALID_OPERATION, kInvalidTextureTarget);
                    return false;
                }
            }
            break;

            case TextureTarget::Rectangle:
            {
                if (level != 0)
                {
                    context->validationError(GL_INVALID_VALUE, kInvalidMipLevel);
                    return false;
                }
                if (tex->getType() != TextureType::Rectangle)
                {
                    context->validationError(GL_INVALID_OPERATION, kTextureTargetMismatch);
                    return false;
                }
            }
            break;

            case TextureTarget::CubeMapNegativeX:
            case TextureTarget::CubeMapNegativeY:
            case TextureTarget::CubeMapNegativeZ:
            case TextureTarget::CubeMapPositiveX:
            case TextureTarget::CubeMapPositiveY:
            case TextureTarget::CubeMapPositiveZ:
            {
                if (level > log2(caps.maxCubeMapTextureSize))
                {
                    context->validationError(GL_INVALID_VALUE, kInvalidMipLevel);
                    return false;
                }
                if (tex->getType() != TextureType::CubeMap)
                {
                    context->validationError(GL_INVALID_OPERATION, kTextureTargetMismatch);
                    return false;
                }
            }
            break;

            case TextureTarget::_2DMultisample:
            {
                if (context->getClientVersion() < ES_3_1 &&
                    !context->getExtensions().textureMultisample)
                {
                    context->validationError(GL_INVALID_OPERATION,
                                             kMultisampleTextureExtensionOrES31Required);
                    return false;
                }

                if (level != 0)
                {
                    context->validationError(GL_INVALID_VALUE, kLevelNotZero);
                    return false;
                }
                if (tex->getType() != TextureType::_2DMultisample)
                {
                    context->validationError(GL_INVALID_OPERATION, kTextureTargetMismatch);
                    return false;
                }
            }
            break;

            default:
                context->validationError(GL_INVALID_ENUM, kInvalidTextureTarget);
                return false;
        }
    }

    return true;
}

bool ValidateFramebufferTexture3DOES(const Context *context,
                                     GLenum target,
                                     GLenum attachment,
                                     TextureTarget textargetPacked,
                                     TextureID texture,
                                     GLint level,
                                     GLint zoffset)
{
    // We don't call into a base ValidateFramebufferTexture3D here because
    // it doesn't exist for OpenGL ES. This function is replaced by
    // FramebufferTextureLayer in ES 3.x, which has broader support.
    if (!context->getExtensions().texture3DOES)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    // Attachments are required to be bound to level 0 without ES3 or the
    // GL_OES_fbo_render_mipmap extension
    if (context->getClientMajorVersion() < 3 && !context->getExtensions().fboRenderMipmapOES &&
        level != 0)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidFramebufferTextureLevel);
        return false;
    }

    if (!ValidateFramebufferTextureBase(context, target, attachment, texture, level))
    {
        return false;
    }

    if (texture.value != 0)
    {
        Texture *tex = context->getTexture(texture);
        ASSERT(tex);

        const Caps &caps = context->getCaps();

        switch (textargetPacked)
        {
            case TextureTarget::_3D:
            {
                if (level > log2(caps.max3DTextureSize))
                {
                    context->validationError(GL_INVALID_VALUE, kInvalidMipLevel);
                    return false;
                }
                if (zoffset >= caps.max3DTextureSize)
                {
                    context->validationError(GL_INVALID_VALUE, kInvalidZOffset);
                    return false;
                }
                if (tex->getType() != TextureType::_3D)
                {
                    context->validationError(GL_INVALID_OPERATION, kInvalidTextureType);
                    return false;
                }
            }
            break;

            default:
                context->validationError(GL_INVALID_OPERATION, kInvalidTextureTarget);
                return false;
        }
    }

    return true;
}

bool ValidateGenBuffers(const Context *context, GLint n, const BufferID *buffers)
{
    return ValidateGenOrDelete(context, n);
}

bool ValidateGenFramebuffers(const Context *context, GLint n, const FramebufferID *framebuffers)
{
    return ValidateGenOrDelete(context, n);
}

bool ValidateGenRenderbuffers(const Context *context, GLint n, const RenderbufferID *renderbuffers)
{
    return ValidateGenOrDelete(context, n);
}

bool ValidateGenTextures(const Context *context, GLint n, const TextureID *textures)
{
    return ValidateGenOrDelete(context, n);
}

bool ValidateGenerateMipmap(const Context *context, TextureType target)
{
    if (!ValidTextureTarget(context, target))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidTextureTarget);
        return false;
    }

    Texture *texture = context->getTextureByType(target);

    if (texture == nullptr)
    {
        context->validationError(GL_INVALID_OPERATION, kTextureNotBound);
        return false;
    }

    const GLuint effectiveBaseLevel = texture->getTextureState().getEffectiveBaseLevel();

    // This error isn't spelled out in the spec in a very explicit way, but we interpret the spec so
    // that out-of-range base level has a non-color-renderable / non-texture-filterable format.
    if (effectiveBaseLevel >= IMPLEMENTATION_MAX_TEXTURE_LEVELS)
    {
        context->validationError(GL_INVALID_OPERATION, kBaseLevelOutOfRange);
        return false;
    }

    TextureTarget baseTarget = (target == TextureType::CubeMap)
                                   ? TextureTarget::CubeMapPositiveX
                                   : NonCubeTextureTypeToTarget(target);
    const auto &format = *(texture->getFormat(baseTarget, effectiveBaseLevel).info);
    if (format.sizedInternalFormat == GL_NONE || format.compressed || format.depthBits > 0 ||
        format.stencilBits > 0)
    {
        context->validationError(GL_INVALID_OPERATION, kGenerateMipmapNotAllowed);
        return false;
    }

    // GenerateMipmap accepts formats that are unsized or both color renderable and filterable.
    bool formatUnsized = !format.sized;
    bool formatColorRenderableAndFilterable =
        format.filterSupport(context->getClientVersion(), context->getExtensions()) &&
        format.textureAttachmentSupport(context->getClientVersion(), context->getExtensions());
    if (!formatUnsized && !formatColorRenderableAndFilterable)
    {
        context->validationError(GL_INVALID_OPERATION, kGenerateMipmapNotAllowed);
        return false;
    }

    // GL_EXT_sRGB adds an unsized SRGB (no alpha) format which has explicitly disabled mipmap
    // generation
    if (format.colorEncoding == GL_SRGB && format.format == GL_RGB)
    {
        context->validationError(GL_INVALID_OPERATION, kGenerateMipmapNotAllowed);
        return false;
    }

    // According to the OpenGL extension spec EXT_sRGB.txt, EXT_SRGB is based on ES 2.0 and
    // generateMipmap is not allowed if texture format is SRGB_EXT or SRGB_ALPHA_EXT.
    if (context->getClientVersion() < Version(3, 0) && format.colorEncoding == GL_SRGB)
    {
        context->validationError(GL_INVALID_OPERATION, kGenerateMipmapNotAllowed);
        return false;
    }

    // Non-power of 2 ES2 check
    if (context->getClientVersion() < Version(3, 0) && !context->getExtensions().textureNPOTOES &&
        (!isPow2(static_cast<int>(texture->getWidth(baseTarget, 0))) ||
         !isPow2(static_cast<int>(texture->getHeight(baseTarget, 0)))))
    {
        ASSERT(target == TextureType::_2D || target == TextureType::Rectangle ||
               target == TextureType::CubeMap);
        context->validationError(GL_INVALID_OPERATION, kTextureNotPow2);
        return false;
    }

    // Cube completeness check
    if (target == TextureType::CubeMap && !texture->getTextureState().isCubeComplete())
    {
        context->validationError(GL_INVALID_OPERATION, kCubemapIncomplete);
        return false;
    }

    if (context->getExtensions().webglCompatibility &&
        (texture->getWidth(baseTarget, effectiveBaseLevel) == 0 ||
         texture->getHeight(baseTarget, effectiveBaseLevel) == 0))
    {
        context->validationError(GL_INVALID_OPERATION, kGenerateMipmapZeroSize);
        return false;
    }

    return true;
}

bool ValidateGetBufferParameteriv(const Context *context,
                                  BufferBinding target,
                                  GLenum pname,
                                  const GLint *params)
{
    return ValidateGetBufferParameterBase(context, target, pname, false, nullptr);
}

bool ValidateGetRenderbufferParameteriv(const Context *context,
                                        GLenum target,
                                        GLenum pname,
                                        const GLint *params)
{
    return ValidateGetRenderbufferParameterivBase(context, target, pname, nullptr);
}

bool ValidateGetShaderiv(const Context *context,
                         ShaderProgramID shader,
                         GLenum pname,
                         const GLint *params)
{
    return ValidateGetShaderivBase(context, shader, pname, nullptr);
}

bool ValidateGetTexParameterfv(const Context *context,
                               TextureType target,
                               GLenum pname,
                               const GLfloat *params)
{
    return ValidateGetTexParameterBase(context, target, pname, nullptr);
}

bool ValidateGetTexParameteriv(const Context *context,
                               TextureType target,
                               GLenum pname,
                               const GLint *params)
{
    return ValidateGetTexParameterBase(context, target, pname, nullptr);
}

bool ValidateGetTexParameterIivOES(const Context *context,
                                   TextureType target,
                                   GLenum pname,
                                   const GLint *params)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }
    return ValidateGetTexParameterBase(context, target, pname, nullptr);
}

bool ValidateGetTexParameterIuivOES(const Context *context,
                                    TextureType target,
                                    GLenum pname,
                                    const GLuint *params)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }
    return ValidateGetTexParameterBase(context, target, pname, nullptr);
}

bool ValidateGetUniformfv(const Context *context,
                          ShaderProgramID program,
                          UniformLocation location,
                          const GLfloat *params)
{
    return ValidateGetUniformBase(context, program, location);
}

bool ValidateGetUniformiv(const Context *context,
                          ShaderProgramID program,
                          UniformLocation location,
                          const GLint *params)
{
    return ValidateGetUniformBase(context, program, location);
}

bool ValidateGetVertexAttribfv(const Context *context,
                               GLuint index,
                               GLenum pname,
                               const GLfloat *params)
{
    return ValidateGetVertexAttribBase(context, index, pname, nullptr, false, false);
}

bool ValidateGetVertexAttribiv(const Context *context,
                               GLuint index,
                               GLenum pname,
                               const GLint *params)
{
    return ValidateGetVertexAttribBase(context, index, pname, nullptr, false, false);
}

bool ValidateGetVertexAttribPointerv(const Context *context,
                                     GLuint index,
                                     GLenum pname,
                                     void *const *pointer)
{
    return ValidateGetVertexAttribBase(context, index, pname, nullptr, true, false);
}

bool ValidateIsEnabled(const Context *context, GLenum cap)
{
    if (!ValidCap(context, cap, true))
    {
        context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
        return false;
    }

    return true;
}

bool ValidateLinkProgram(const Context *context, ShaderProgramID program)
{
    if (context->hasActiveTransformFeedback(program))
    {
        // ES 3.0.4 section 2.15 page 91
        context->validationError(GL_INVALID_OPERATION, kTransformFeedbackActiveDuringLink);
        return false;
    }

    Program *programObject = GetValidProgram(context, program);
    if (!programObject)
    {
        return false;
    }

    return true;
}

bool ValidateReadPixels(const Context *context,
                        GLint x,
                        GLint y,
                        GLsizei width,
                        GLsizei height,
                        GLenum format,
                        GLenum type,
                        const void *pixels)
{
    return ValidateReadPixelsBase(context, x, y, width, height, format, type, -1, nullptr, nullptr,
                                  nullptr, pixels);
}

bool ValidateTexParameterf(const Context *context, TextureType target, GLenum pname, GLfloat param)
{
    return ValidateTexParameterBase(context, target, pname, -1, false, &param);
}

bool ValidateTexParameterfv(const Context *context,
                            TextureType target,
                            GLenum pname,
                            const GLfloat *params)
{
    return ValidateTexParameterBase(context, target, pname, -1, true, params);
}

bool ValidateTexParameteri(const Context *context, TextureType target, GLenum pname, GLint param)
{
    return ValidateTexParameterBase(context, target, pname, -1, false, &param);
}

bool ValidateTexParameteriv(const Context *context,
                            TextureType target,
                            GLenum pname,
                            const GLint *params)
{
    return ValidateTexParameterBase(context, target, pname, -1, true, params);
}

bool ValidateTexParameterIivOES(const Context *context,
                                TextureType target,
                                GLenum pname,
                                const GLint *params)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }
    return ValidateTexParameterBase(context, target, pname, -1, true, params);
}

bool ValidateTexParameterIuivOES(const Context *context,
                                 TextureType target,
                                 GLenum pname,
                                 const GLuint *params)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }
    return ValidateTexParameterBase(context, target, pname, -1, true, params);
}

bool ValidateUseProgram(const Context *context, ShaderProgramID program)
{
    if (program.value != 0)
    {
        Program *programObject = context->getProgramResolveLink(program);
        if (!programObject)
        {
            // ES 3.1.0 section 7.3 page 72
            if (context->getShader(program))
            {
                context->validationError(GL_INVALID_OPERATION, kExpectedProgramName);
                return false;
            }
            else
            {
                context->validationError(GL_INVALID_VALUE, kInvalidProgramName);
                return false;
            }
        }
        if (!programObject->isLinked())
        {
            context->validationError(GL_INVALID_OPERATION, kProgramNotLinked);
            return false;
        }
    }
    if (context->getState().isTransformFeedbackActiveUnpaused())
    {
        // ES 3.0.4 section 2.15 page 91
        context->validationError(GL_INVALID_OPERATION, kTransformFeedbackUseProgram);
        return false;
    }

    return true;
}

bool ValidateDeleteFencesNV(const Context *context, GLsizei n, const FenceNVID *fences)
{
    if (!context->getExtensions().fenceNV)
    {
        context->validationError(GL_INVALID_OPERATION, kNVFenceNotSupported);
        return false;
    }

    if (n < 0)
    {
        context->validationError(GL_INVALID_VALUE, kNegativeCount);
        return false;
    }

    return true;
}

bool ValidateFinishFenceNV(const Context *context, FenceNVID fence)
{
    if (!context->getExtensions().fenceNV)
    {
        context->validationError(GL_INVALID_OPERATION, kNVFenceNotSupported);
        return false;
    }

    FenceNV *fenceObject = context->getFenceNV(fence);

    if (fenceObject == nullptr)
    {
        context->validationError(GL_INVALID_OPERATION, kInvalidFence);
        return false;
    }

    if (!fenceObject->isSet())
    {
        context->validationError(GL_INVALID_OPERATION, kInvalidFenceState);
        return false;
    }

    return true;
}

bool ValidateGenFencesNV(const Context *context, GLsizei n, const FenceNVID *fences)
{
    if (!context->getExtensions().fenceNV)
    {
        context->validationError(GL_INVALID_OPERATION, kNVFenceNotSupported);
        return false;
    }

    if (n < 0)
    {
        context->validationError(GL_INVALID_VALUE, kNegativeCount);
        return false;
    }

    return true;
}

bool ValidateGetFenceivNV(const Context *context,
                          FenceNVID fence,
                          GLenum pname,
                          const GLint *params)
{
    if (!context->getExtensions().fenceNV)
    {
        context->validationError(GL_INVALID_OPERATION, kNVFenceNotSupported);
        return false;
    }

    FenceNV *fenceObject = context->getFenceNV(fence);

    if (fenceObject == nullptr)
    {
        context->validationError(GL_INVALID_OPERATION, kInvalidFence);
        return false;
    }

    if (!fenceObject->isSet())
    {
        context->validationError(GL_INVALID_OPERATION, kInvalidFenceState);
        return false;
    }

    switch (pname)
    {
        case GL_FENCE_STATUS_NV:
        case GL_FENCE_CONDITION_NV:
            break;

        default:
            context->validationError(GL_INVALID_ENUM, kInvalidPname);
            return false;
    }

    return true;
}

bool ValidateGetGraphicsResetStatusEXT(const Context *context)
{
    if (!context->getExtensions().robustness)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return true;
}

bool ValidateGetTranslatedShaderSourceANGLE(const Context *context,
                                            ShaderProgramID shader,
                                            GLsizei bufsize,
                                            const GLsizei *length,
                                            const GLchar *source)
{
    if (!context->getExtensions().translatedShaderSource)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    if (bufsize < 0)
    {
        context->validationError(GL_INVALID_VALUE, kNegativeBufferSize);
        return false;
    }

    Shader *shaderObject = context->getShader(shader);

    if (!shaderObject)
    {
        context->validationError(GL_INVALID_OPERATION, kInvalidShaderName);
        return false;
    }

    return true;
}

bool ValidateIsFenceNV(const Context *context, FenceNVID fence)
{
    if (!context->getExtensions().fenceNV)
    {
        context->validationError(GL_INVALID_OPERATION, kNVFenceNotSupported);
        return false;
    }

    return true;
}

bool ValidateSetFenceNV(const Context *context, FenceNVID fence, GLenum condition)
{
    if (!context->getExtensions().fenceNV)
    {
        context->validationError(GL_INVALID_OPERATION, kNVFenceNotSupported);
        return false;
    }

    if (condition != GL_ALL_COMPLETED_NV)
    {
        context->validationError(GL_INVALID_ENUM, kInvalidFenceCondition);
        return false;
    }

    FenceNV *fenceObject = context->getFenceNV(fence);

    if (fenceObject == nullptr)
    {
        context->validationError(GL_INVALID_OPERATION, kInvalidFence);
        return false;
    }

    return true;
}

bool ValidateTestFenceNV(const Context *context, FenceNVID fence)
{
    if (!context->getExtensions().fenceNV)
    {
        context->validationError(GL_INVALID_OPERATION, kNVFenceNotSupported);
        return false;
    }

    FenceNV *fenceObject = context->getFenceNV(fence);

    if (fenceObject == nullptr)
    {
        context->validationError(GL_INVALID_OPERATION, kInvalidFence);
        return false;
    }

    if (fenceObject->isSet() != GL_TRUE)
    {
        context->validationError(GL_INVALID_OPERATION, kInvalidFenceState);
        return false;
    }

    return true;
}

bool ValidateTexStorage2DEXT(const Context *context,
                             TextureType type,
                             GLsizei levels,
                             GLenum internalformat,
                             GLsizei width,
                             GLsizei height)
{
    if (!context->getExtensions().textureStorage)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    if (context->getClientMajorVersion() < 3)
    {
        return ValidateES2TexStorageParameters(context, type, levels, internalformat, width,
                                               height);
    }

    ASSERT(context->getClientMajorVersion() >= 3);
    return ValidateES3TexStorage2DParameters(context, type, levels, internalformat, width, height,
                                             1);
}

bool ValidateVertexAttribDivisorANGLE(const Context *context, GLuint index, GLuint divisor)
{
    if (!context->getExtensions().instancedArraysANGLE)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    if (index >= MAX_VERTEX_ATTRIBS)
    {
        context->validationError(GL_INVALID_VALUE, kIndexExceedsMaxVertexAttribute);
        return false;
    }

    if (context->getLimitations().attributeZeroRequiresZeroDivisorInEXT)
    {
        if (index == 0 && divisor != 0)
        {
            context->validationError(GL_INVALID_OPERATION, kAttributeZeroRequiresDivisorLimitation);

            // We also output an error message to the debugger window if tracing is active, so
            // that developers can see the error message.
            ERR() << kAttributeZeroRequiresDivisorLimitation;
            return false;
        }
    }

    return true;
}

bool ValidateVertexAttribDivisorEXT(const Context *context, GLuint index, GLuint divisor)
{
    if (!context->getExtensions().instancedArraysEXT)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    if (index >= MAX_VERTEX_ATTRIBS)
    {
        context->validationError(GL_INVALID_VALUE, kIndexExceedsMaxVertexAttribute);
        return false;
    }

    return true;
}

bool ValidateTexImage3DOES(const Context *context,
                           TextureTarget target,
                           GLint level,
                           GLenum internalformat,
                           GLsizei width,
                           GLsizei height,
                           GLsizei depth,
                           GLint border,
                           GLenum format,
                           GLenum type,
                           const void *pixels)
{
    return ValidateTexImage3D(context, target, level, internalformat, width, height, depth, border,
                              format, type, pixels);
}

bool ValidatePopGroupMarkerEXT(const Context *context)
{
    if (!context->getExtensions().debugMarker)
    {
        // The debug marker calls should not set error state
        // However, it seems reasonable to set an error state if the extension is not enabled
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return true;
}

bool ValidateTexStorage1DEXT(const Context *context,
                             GLenum target,
                             GLsizei levels,
                             GLenum internalformat,
                             GLsizei width)
{
    UNIMPLEMENTED();
    context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
    return false;
}

bool ValidateTexStorage3DEXT(const Context *context,
                             TextureType target,
                             GLsizei levels,
                             GLenum internalformat,
                             GLsizei width,
                             GLsizei height,
                             GLsizei depth)
{
    if (!context->getExtensions().textureStorage)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateES3TexStorage3DParameters(context, target, levels, internalformat, width, height,
                                             depth);
}

bool ValidateMaxShaderCompilerThreadsKHR(const Context *context, GLuint count)
{
    if (!context->getExtensions().parallelShaderCompile)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }
    return true;
}

bool ValidateMultiDrawArraysANGLE(const Context *context,
                                  PrimitiveMode mode,
                                  const GLint *firsts,
                                  const GLsizei *counts,
                                  GLsizei drawcount)
{
    if (!context->getExtensions().multiDraw)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }
    for (GLsizei drawID = 0; drawID < drawcount; ++drawID)
    {
        if (!ValidateDrawArrays(context, mode, firsts[drawID], counts[drawID]))
        {
            return false;
        }
    }
    return true;
}

bool ValidateMultiDrawElementsANGLE(const Context *context,
                                    PrimitiveMode mode,
                                    const GLsizei *counts,
                                    DrawElementsType type,
                                    const GLvoid *const *indices,
                                    GLsizei drawcount)
{
    if (!context->getExtensions().multiDraw)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }
    for (GLsizei drawID = 0; drawID < drawcount; ++drawID)
    {
        if (!ValidateDrawElements(context, mode, counts[drawID], type, indices[drawID]))
        {
            return false;
        }
    }
    return true;
}

bool ValidateProvokingVertexANGLE(const Context *context, ProvokingVertexConvention modePacked)
{
    if (!context->getExtensions().provokingVertex)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    switch (modePacked)
    {
        case ProvokingVertexConvention::FirstVertexConvention:
        case ProvokingVertexConvention::LastVertexConvention:
            break;
        default:
            context->validationError(GL_INVALID_ENUM, kInvalidProvokingVertex);
            return false;
    }

    return true;
}

bool ValidateFramebufferTexture2DMultisampleEXT(const Context *context,
                                                GLenum target,
                                                GLenum attachment,
                                                GLenum textarget,
                                                GLuint texture,
                                                GLint level,
                                                GLsizei samples)
{
    if (!context->getExtensions().multisampledRenderToTexture)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    if (samples < 0)
    {
        return false;
    }

    // EXT_multisampled_render_to_texture states that the value of samples
    // must be less than or equal to MAX_SAMPLES_EXT (Context::getCaps().maxSamples)
    // otherwise GL_INVALID_VALUE is generated.
    if (samples > context->getCaps().maxSamples)
    {
        context->validationError(GL_INVALID_VALUE, kSamplesOutOfRange);
        return false;
    }

    if (!ValidFramebufferTarget(context, target))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidFramebufferTarget);
        return false;
    }

    if (attachment != GL_COLOR_ATTACHMENT0)
    {
        context->validationError(GL_INVALID_ENUM, kInvalidAttachment);
        return false;
    }

    TextureTarget textargetPacked = FromGLenum<TextureTarget>(textarget);
    if (!ValidTexture2DDestinationTarget(context, textargetPacked))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidTextureTarget);
        return false;
    }

    if (texture != 0)
    {
        TextureID texturePacked = FromGL<TextureID>(texture);
        Texture *tex            = context->getTexture(texturePacked);

        if (tex == nullptr)
        {
            context->validationError(GL_INVALID_OPERATION, kMissingTexture);
            return false;
        }

        if (level < 0)
        {
            context->validationError(GL_INVALID_VALUE, kInvalidMipLevel);
            return false;
        }

        // EXT_multisampled_render_to_texture returns INVALID_OPERATION when a sample number higher
        // than the maximum sample number supported by this format is passed.
        // The TextureCaps::getMaxSamples method is only guarenteed to be valid when the context is
        // ES3.
        if (context->getClientMajorVersion() >= 3)
        {
            GLenum internalformat = tex->getFormat(textargetPacked, level).info->internalFormat;
            const TextureCaps &formatCaps = context->getTextureCaps().get(internalformat);
            if (static_cast<GLuint>(samples) > formatCaps.getMaxSamples())
            {
                context->validationError(GL_INVALID_OPERATION, kSamplesOutOfRange);
                return false;
            }
        }
    }

    return true;
}

bool ValidateRenderbufferStorageMultisampleEXT(const Context *context,
                                               GLenum target,
                                               GLsizei samples,
                                               GLenum internalformat,
                                               GLsizei width,
                                               GLsizei height)
{
    if (!context->getExtensions().multisampledRenderToTexture)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }
    if (!ValidateRenderbufferStorageParametersBase(context, target, samples, internalformat, width,
                                                   height))
    {
        return false;
    }

    // EXT_multisampled_render_to_texture states that the value of samples
    // must be less than or equal to MAX_SAMPLES_EXT (Context::getCaps().maxSamples)
    // otherwise GL_INVALID_VALUE is generated.
    if (samples > context->getCaps().maxSamples)
    {
        context->validationError(GL_INVALID_VALUE, kSamplesOutOfRange);
        return false;
    }

    // EXT_multisampled_render_to_texture returns GL_OUT_OF_MEMORY on failure to create
    // the specified storage. This is different than ES 3.0 in which a sample number higher
    // than the maximum sample number supported by this format generates a GL_INVALID_VALUE.
    // The TextureCaps::getMaxSamples method is only guarenteed to be valid when the context is ES3.
    if (context->getClientMajorVersion() >= 3)
    {
        const TextureCaps &formatCaps = context->getTextureCaps().get(internalformat);
        if (static_cast<GLuint>(samples) > formatCaps.getMaxSamples())
        {
            context->validationError(GL_OUT_OF_MEMORY, kSamplesOutOfRange);
            return false;
        }
    }

    return true;
}

void RecordBindTextureTypeError(const Context *context, TextureType target)
{
    ASSERT(!context->getStateCache().isValidBindTextureType(target));

    switch (target)
    {
        case TextureType::Rectangle:
            ASSERT(!context->getExtensions().textureRectangle);
            context->validationError(GL_INVALID_ENUM, kTextureRectangleNotSupported);
            break;

        case TextureType::_3D:
        case TextureType::_2DArray:
            ASSERT(context->getClientMajorVersion() < 3);
            context->validationError(GL_INVALID_ENUM, kES3Required);
            break;

        case TextureType::_2DMultisample:
            ASSERT(context->getClientVersion() < Version(3, 1) &&
                   !context->getExtensions().textureMultisample);
            context->validationError(GL_INVALID_ENUM, kMultisampleTextureExtensionOrES31Required);
            break;

        case TextureType::_2DMultisampleArray:
            ASSERT(!context->getExtensions().textureStorageMultisample2DArrayOES);
            context->validationError(GL_INVALID_ENUM, kMultisampleArrayExtensionRequired);
            break;

        case TextureType::External:
            ASSERT(!context->getExtensions().eglImageExternalOES &&
                   !context->getExtensions().eglStreamConsumerExternalNV);
            context->validationError(GL_INVALID_ENUM, kExternalTextureNotSupported);
            break;

        case TextureType::VideoImage:
            ASSERT(!context->getExtensions().webglVideoTexture);
            context->validationError(GL_INVALID_ENUM, kExtensionNotEnabled);
            break;

        default:
            context->validationError(GL_INVALID_ENUM, kInvalidTextureTarget);
    }
}

}  // namespace gl
