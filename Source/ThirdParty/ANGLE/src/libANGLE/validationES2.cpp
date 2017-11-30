//
// Copyright (c) 2013-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationES2.cpp: Validation functions for OpenGL ES 2.0 entry point parameters

#include "libANGLE/validationES2.h"

#include <cstdint>

#include "common/mathutil.h"
#include "common/string_utils.h"
#include "common/utilities.h"
#include "libANGLE/Context.h"
#include "libANGLE/ErrorStrings.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/Renderbuffer.h"
#include "libANGLE/Shader.h"
#include "libANGLE/Texture.h"
#include "libANGLE/Uniform.h"
#include "libANGLE/VertexArray.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/validationES.h"
#include "libANGLE/validationES3.h"

namespace gl
{

namespace
{

bool IsPartialBlit(gl::Context *context,
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

    if (context->getGLState().isScissorTestEnabled())
    {
        const Rectangle &scissor = context->getGLState().getScissor();
        return scissor.x > 0 || scissor.y > 0 || scissor.width < writeSize.width ||
               scissor.height < writeSize.height;
    }

    return false;
}

template <typename T>
bool ValidatePathInstances(gl::Context *context,
                           GLsizei numPaths,
                           const void *paths,
                           GLuint pathBase)
{
    const auto *array = static_cast<const T *>(paths);

    for (GLsizei i = 0; i < numPaths; ++i)
    {
        const GLuint pathName = array[i] + pathBase;
        if (context->hasPath(pathName) && !context->hasPathData(pathName))
        {
            ANGLE_VALIDATION_ERR(context, InvalidOperation(), NoSuchPath);
            return false;
        }
    }
    return true;
}

bool ValidateInstancedPathParameters(gl::Context *context,
                                     GLsizei numPaths,
                                     GLenum pathNameType,
                                     const void *paths,
                                     GLuint pathBase,
                                     GLenum transformType,
                                     const GLfloat *transformValues)
{
    if (!context->getExtensions().pathRendering)
    {
        context->handleError(InvalidOperation() << "GL_CHROMIUM_path_rendering is not available.");
        return false;
    }

    if (paths == nullptr)
    {
        context->handleError(InvalidValue() << "No path name array.");
        return false;
    }

    if (numPaths < 0)
    {
        context->handleError(InvalidValue() << "Invalid (negative) numPaths.");
        return false;
    }

    if (!angle::IsValueInRangeForNumericType<std::uint32_t>(numPaths))
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), IntegerOverflow);
        return false;
    }

    std::uint32_t pathNameTypeSize = 0;
    std::uint32_t componentCount   = 0;

    switch (pathNameType)
    {
        case GL_UNSIGNED_BYTE:
            pathNameTypeSize = sizeof(GLubyte);
            if (!ValidatePathInstances<GLubyte>(context, numPaths, paths, pathBase))
                return false;
            break;

        case GL_BYTE:
            pathNameTypeSize = sizeof(GLbyte);
            if (!ValidatePathInstances<GLbyte>(context, numPaths, paths, pathBase))
                return false;
            break;

        case GL_UNSIGNED_SHORT:
            pathNameTypeSize = sizeof(GLushort);
            if (!ValidatePathInstances<GLushort>(context, numPaths, paths, pathBase))
                return false;
            break;

        case GL_SHORT:
            pathNameTypeSize = sizeof(GLshort);
            if (!ValidatePathInstances<GLshort>(context, numPaths, paths, pathBase))
                return false;
            break;

        case GL_UNSIGNED_INT:
            pathNameTypeSize = sizeof(GLuint);
            if (!ValidatePathInstances<GLuint>(context, numPaths, paths, pathBase))
                return false;
            break;

        case GL_INT:
            pathNameTypeSize = sizeof(GLint);
            if (!ValidatePathInstances<GLint>(context, numPaths, paths, pathBase))
                return false;
            break;

        default:
            context->handleError(InvalidEnum() << "Invalid path name type.");
            return false;
    }

    switch (transformType)
    {
        case GL_NONE:
            componentCount = 0;
            break;
        case GL_TRANSLATE_X_CHROMIUM:
        case GL_TRANSLATE_Y_CHROMIUM:
            componentCount = 1;
            break;
        case GL_TRANSLATE_2D_CHROMIUM:
            componentCount = 2;
            break;
        case GL_TRANSLATE_3D_CHROMIUM:
            componentCount = 3;
            break;
        case GL_AFFINE_2D_CHROMIUM:
        case GL_TRANSPOSE_AFFINE_2D_CHROMIUM:
            componentCount = 6;
            break;
        case GL_AFFINE_3D_CHROMIUM:
        case GL_TRANSPOSE_AFFINE_3D_CHROMIUM:
            componentCount = 12;
            break;
        default:
            context->handleError(InvalidEnum() << "Invalid transformation.");
            return false;
    }
    if (componentCount != 0 && transformValues == nullptr)
    {
        context->handleError(InvalidValue() << "No transform array given.");
        return false;
    }

    angle::CheckedNumeric<std::uint32_t> checkedSize(0);
    checkedSize += (numPaths * pathNameTypeSize);
    checkedSize += (numPaths * sizeof(GLfloat) * componentCount);
    if (!checkedSize.IsValid())
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), IntegerOverflow);
        return false;
    }

    return true;
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

bool IsValidCopyTextureDestinationFormatType(Context *context, GLint internalFormat, GLenum type)
{
    if (!IsValidCopyTextureDestinationInternalFormatEnum(internalFormat))
    {
        return false;
    }

    if (!ValidES3FormatCombination(GetUnsizedFormat(internalFormat), type, internalFormat))
    {
        context->handleError(InvalidOperation()
                             << "Invalid combination of type and internalFormat.");
        return false;
    }

    const InternalFormat &internalFormatInfo = GetInternalFormatInfo(internalFormat, type);
    if (!internalFormatInfo.textureSupport(context->getClientVersion(), context->getExtensions()))
    {
        return false;
    }

    return true;
}

bool IsValidCopyTextureDestinationTargetEnum(Context *context, GLenum target)
{
    switch (target)
    {
        case GL_TEXTURE_2D:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
            return true;

        case GL_TEXTURE_RECTANGLE_ANGLE:
            return context->getExtensions().textureRectangle;

        default:
            return false;
    }
}

bool IsValidCopyTextureDestinationTarget(Context *context, GLenum textureType, GLenum target)
{
    if (IsCubeMapTextureTarget(target))
    {
        return textureType == GL_TEXTURE_CUBE_MAP;
    }
    else
    {
        return textureType == target;
    }
}

bool IsValidCopyTextureSourceTarget(Context *context, GLenum target)
{
    switch (target)
    {
        case GL_TEXTURE_2D:
            return true;
        case GL_TEXTURE_RECTANGLE_ANGLE:
            return context->getExtensions().textureRectangle;

        // TODO(geofflang): accept GL_TEXTURE_EXTERNAL_OES if the texture_external extension is
        // supported

        default:
            return false;
    }
}

bool IsValidCopyTextureSourceLevel(Context *context, GLenum target, GLint level)
{
    if (!ValidMipLevel(context, target, level))
    {
        return false;
    }

    if (level > 0 && context->getClientVersion() < ES_3_0)
    {
        return false;
    }

    return true;
}

bool IsValidCopyTextureDestinationLevel(Context *context,
                                        GLenum target,
                                        GLint level,
                                        GLsizei width,
                                        GLsizei height)
{
    if (!ValidMipLevel(context, target, level))
    {
        return false;
    }

    const Caps &caps = context->getCaps();
    if (target == GL_TEXTURE_2D)
    {
        if (static_cast<GLuint>(width) > (caps.max2DTextureSize >> level) ||
            static_cast<GLuint>(height) > (caps.max2DTextureSize >> level))
        {
            return false;
        }
    }
    else if (target == GL_TEXTURE_RECTANGLE_ANGLE)
    {
        ASSERT(level == 0);
        if (static_cast<GLuint>(width) > caps.maxRectangleTextureSize ||
            static_cast<GLuint>(height) > caps.maxRectangleTextureSize)
        {
            return false;
        }
    }
    else if (IsCubeMapTextureTarget(target))
    {
        if (static_cast<GLuint>(width) > (caps.maxCubeMapTextureSize >> level) ||
            static_cast<GLuint>(height) > (caps.maxCubeMapTextureSize >> level))
        {
            return false;
        }
    }

    return true;
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

bool ValidateES2CopyTexImageParameters(ValidationContext *context,
                                       GLenum target,
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
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidTextureTarget);
        return false;
    }

    if (!ValidImageSizeParameters(context, target, level, width, height, 1, isSubImage))
    {
        context->handleError(InvalidValue() << "Invalid texture dimensions.");
        return false;
    }

    Format textureFormat = Format::Invalid();
    if (!ValidateCopyTexImageParametersBase(context, target, level, internalformat, isSubImage,
                                            xoffset, yoffset, 0, x, y, width, height, border,
                                            &textureFormat))
    {
        return false;
    }

    const gl::Framebuffer *framebuffer = context->getGLState().getReadFramebuffer();
    GLenum colorbufferFormat =
        framebuffer->getReadColorbuffer()->getFormat().info->sizedInternalFormat;
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
                    ANGLE_VALIDATION_ERR(context, InvalidOperation(), InvalidFormat);
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
                    ANGLE_VALIDATION_ERR(context, InvalidOperation(), InvalidFormat);
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
                    ANGLE_VALIDATION_ERR(context, InvalidOperation(), InvalidFormat);
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
                    ANGLE_VALIDATION_ERR(context, InvalidOperation(), InvalidFormat);
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
                    ANGLE_VALIDATION_ERR(context, InvalidOperation(), InvalidFormat);
                    return false;
                }
                break;
            case GL_LUMINANCE_ALPHA:
            case GL_RGBA:
                if (colorbufferFormat != GL_RGBA4 && colorbufferFormat != GL_RGB5_A1 &&
                    colorbufferFormat != GL_RGBA8_OES && colorbufferFormat != GL_RGBA32F &&
                    colorbufferFormat != GL_BGRA8_EXT && colorbufferFormat != GL_BGR5_A1_ANGLEX)
                {
                    ANGLE_VALIDATION_ERR(context, InvalidOperation(), InvalidFormat);
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
                ANGLE_VALIDATION_ERR(context, InvalidOperation(), InvalidFormat);
                return false;
            case GL_DEPTH_COMPONENT:
            case GL_DEPTH_STENCIL_OES:
                ANGLE_VALIDATION_ERR(context, InvalidOperation(), InvalidFormat);
                return false;
            default:
                ANGLE_VALIDATION_ERR(context, InvalidOperation(), InvalidFormat);
                return false;
        }

        if (formatInfo.type == GL_FLOAT && !context->getExtensions().textureFloat)
        {
            ANGLE_VALIDATION_ERR(context, InvalidOperation(), InvalidFormat);
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
                    ANGLE_VALIDATION_ERR(context, InvalidOperation(), InvalidFormat);
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
                    ANGLE_VALIDATION_ERR(context, InvalidOperation(), InvalidFormat);
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
                    ANGLE_VALIDATION_ERR(context, InvalidOperation(), InvalidFormat);
                    return false;
                }
                break;
            case GL_RG_EXT:
                if (colorbufferFormat != GL_RG8_EXT && colorbufferFormat != GL_RGB565 &&
                    colorbufferFormat != GL_RGB8_OES && colorbufferFormat != GL_RGBA4 &&
                    colorbufferFormat != GL_RGB5_A1 && colorbufferFormat != GL_BGRA8_EXT &&
                    colorbufferFormat != GL_RGBA8_OES && colorbufferFormat != GL_BGR5_A1_ANGLEX)
                {
                    ANGLE_VALIDATION_ERR(context, InvalidOperation(), InvalidFormat);
                    return false;
                }
                break;
            case GL_RGB:
                if (colorbufferFormat != GL_RGB565 && colorbufferFormat != GL_RGB8_OES &&
                    colorbufferFormat != GL_RGBA4 && colorbufferFormat != GL_RGB5_A1 &&
                    colorbufferFormat != GL_BGRA8_EXT && colorbufferFormat != GL_RGBA8_OES &&
                    colorbufferFormat != GL_BGR5_A1_ANGLEX)
                {
                    ANGLE_VALIDATION_ERR(context, InvalidOperation(), InvalidFormat);
                    return false;
                }
                break;
            case GL_LUMINANCE_ALPHA:
            case GL_RGBA:
                if (colorbufferFormat != GL_RGBA4 && colorbufferFormat != GL_RGB5_A1 &&
                    colorbufferFormat != GL_BGRA8_EXT && colorbufferFormat != GL_RGBA8_OES &&
                    colorbufferFormat != GL_BGR5_A1_ANGLEX)
                {
                    ANGLE_VALIDATION_ERR(context, InvalidOperation(), InvalidFormat);
                    return false;
                }
                break;
            case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
            case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
                if (context->getExtensions().textureCompressionDXT1)
                {
                    ANGLE_VALIDATION_ERR(context, InvalidOperation(), InvalidFormat);
                    return false;
                }
                else
                {
                    ANGLE_VALIDATION_ERR(context, InvalidEnum(), EnumNotSupported);
                    return false;
                }
                break;
            case GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE:
                if (context->getExtensions().textureCompressionDXT3)
                {
                    ANGLE_VALIDATION_ERR(context, InvalidOperation(), InvalidFormat);
                    return false;
                }
                else
                {
                    ANGLE_VALIDATION_ERR(context, InvalidEnum(), EnumNotSupported);
                    return false;
                }
                break;
            case GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE:
                if (context->getExtensions().textureCompressionDXT5)
                {
                    ANGLE_VALIDATION_ERR(context, InvalidOperation(), InvalidFormat);
                    return false;
                }
                else
                {
                    ANGLE_VALIDATION_ERR(context, InvalidEnum(), EnumNotSupported);
                    return false;
                }
                break;
            case GL_ETC1_RGB8_OES:
                if (context->getExtensions().compressedETC1RGB8Texture)
                {
                    context->handleError(InvalidOperation());
                    return false;
                }
                else
                {
                    context->handleError(InvalidEnum());
                    return false;
                }
                break;
            case GL_ETC1_RGB8_LOSSY_DECODE_ANGLE:
            case GL_COMPRESSED_RGB8_LOSSY_DECODE_ETC2_ANGLE:
            case GL_COMPRESSED_SRGB8_LOSSY_DECODE_ETC2_ANGLE:
            case GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_LOSSY_DECODE_ETC2_ANGLE:
            case GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_LOSSY_DECODE_ETC2_ANGLE:
                if (context->getExtensions().lossyETCDecode)
                {
                    context->handleError(InvalidOperation()
                                         << "ETC lossy decode formats can't be copied to.");
                    return false;
                }
                else
                {
                    context->handleError(InvalidEnum()
                                         << "ANGLE_lossy_etc_decode extension is not supported.");
                    return false;
                }
                break;
            case GL_DEPTH_COMPONENT:
            case GL_DEPTH_COMPONENT16:
            case GL_DEPTH_COMPONENT32_OES:
            case GL_DEPTH_STENCIL_OES:
            case GL_DEPTH24_STENCIL8_OES:
                if (context->getExtensions().depthTextures)
                {
                    context->handleError(InvalidOperation());
                    return false;
                }
                else
                {
                    context->handleError(InvalidEnum());
                    return false;
                }
            default:
                context->handleError(InvalidEnum());
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

bool ValidateWebGLNamePrefix(ValidationContext *context, const GLchar *name)
{
    ASSERT(context->isWebGL());

    // WebGL 1.0 [Section 6.16] GLSL Constructs
    // Identifiers starting with "webgl_" and "_webgl_" are reserved for use by WebGL.
    if (strncmp(name, "webgl_", 6) == 0 || strncmp(name, "_webgl_", 7) == 0)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), WebglBindAttribLocationReservedPrefix);
        return false;
    }

    return true;
}

bool ValidateWebGLNameLength(ValidationContext *context, size_t length)
{
    ASSERT(context->isWebGL());

    if (context->isWebGL1() && length > 256)
    {
        // WebGL 1.0 [Section 6.21] Maxmimum Uniform and Attribute Location Lengths
        // WebGL imposes a limit of 256 characters on the lengths of uniform and attribute
        // locations.
        ANGLE_VALIDATION_ERR(context, InvalidValue(), WebglNameLengthLimitExceeded);

        return false;
    }
    else if (length > 1024)
    {
        // WebGL 2.0 [Section 4.3.2] WebGL 2.0 imposes a limit of 1024 characters on the lengths of
        // uniform and attribute locations.
        ANGLE_VALIDATION_ERR(context, InvalidValue(), Webgl2NameLengthLimitExceeded);
        return false;
    }

    return true;
}

}  // anonymous namespace

bool ValidateES2TexImageParameters(Context *context,
                                   GLenum target,
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
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidTextureTarget);
        return false;
    }

    if (!ValidImageSizeParameters(context, target, level, width, height, 1, isSubImage))
    {
        context->handleError(InvalidValue());
        return false;
    }

    if (!ValidMipLevel(context, target, level))
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), InvalidMipLevel);
        return false;
    }

    if (xoffset < 0 || std::numeric_limits<GLsizei>::max() - xoffset < width ||
        std::numeric_limits<GLsizei>::max() - yoffset < height)
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), ResourceMaxTextureSize);
        return false;
    }

    // From GL_CHROMIUM_color_buffer_float_rgb[a]:
    // GL_RGB[A] / GL_RGB[A]32F becomes an allowable format / internalformat parameter pair for
    // TexImage2D. The restriction in section 3.7.1 of the OpenGL ES 2.0 spec that the
    // internalformat parameter and format parameter of TexImage2D must match is lifted for this
    // case.
    bool nonEqualFormatsAllowed =
        (internalformat == GL_RGB32F && context->getExtensions().colorBufferFloatRGB) ||
        (internalformat == GL_RGBA32F && context->getExtensions().colorBufferFloatRGBA);

    if (!isSubImage && !isCompressed && internalformat != format && !nonEqualFormatsAllowed)
    {
        context->handleError(InvalidOperation());
        return false;
    }

    const gl::Caps &caps = context->getCaps();

    if (target == GL_TEXTURE_2D)
    {
        if (static_cast<GLuint>(width) > (caps.max2DTextureSize >> level) ||
            static_cast<GLuint>(height) > (caps.max2DTextureSize >> level))
        {
            context->handleError(InvalidValue());
            return false;
        }
    }
    else if (target == GL_TEXTURE_RECTANGLE_ANGLE)
    {
        ASSERT(level == 0);
        if (static_cast<GLuint>(width) > caps.maxRectangleTextureSize ||
            static_cast<GLuint>(height) > caps.maxRectangleTextureSize)
        {
            context->handleError(InvalidValue());
            return false;
        }
        if (isCompressed)
        {
            context->handleError(InvalidEnum()
                                 << "Rectangle texture cannot have a compressed format.");
            return false;
        }
    }
    else if (IsCubeMapTextureTarget(target))
    {
        if (!isSubImage && width != height)
        {
            ANGLE_VALIDATION_ERR(context, InvalidValue(), CubemapFacesEqualDimensions);
            return false;
        }

        if (static_cast<GLuint>(width) > (caps.maxCubeMapTextureSize >> level) ||
            static_cast<GLuint>(height) > (caps.maxCubeMapTextureSize >> level))
        {
            context->handleError(InvalidValue());
            return false;
        }
    }
    else
    {
        context->handleError(InvalidEnum());
        return false;
    }

    gl::Texture *texture =
        context->getTargetTexture(IsCubeMapTextureTarget(target) ? GL_TEXTURE_CUBE_MAP : target);
    if (!texture)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), BufferNotBound);
        return false;
    }

    if (isSubImage)
    {
        const InternalFormat &textureInternalFormat = *texture->getFormat(target, level).info;
        if (textureInternalFormat.internalFormat == GL_NONE)
        {
            context->handleError(InvalidOperation() << "Texture level does not exist.");
            return false;
        }

        if (format != GL_NONE)
        {
            if (GetInternalFormatInfo(format, type).sizedInternalFormat !=
                textureInternalFormat.sizedInternalFormat)
            {
                ANGLE_VALIDATION_ERR(context, InvalidOperation(), TypeMismatch);
                return false;
            }
        }

        if (static_cast<size_t>(xoffset + width) > texture->getWidth(target, level) ||
            static_cast<size_t>(yoffset + height) > texture->getHeight(target, level))
        {
            context->handleError(InvalidValue());
            return false;
        }

        if (width > 0 && height > 0 && pixels == nullptr &&
            context->getGLState().getTargetBuffer(gl::BufferBinding::PixelUnpack) == nullptr)
        {
            ANGLE_VALIDATION_ERR(context, InvalidValue(), PixelDataNull);
            return false;
        }
    }
    else
    {
        if (texture->getImmutableFormat())
        {
            context->handleError(InvalidOperation());
            return false;
        }
    }

    // Verify zero border
    if (border != 0)
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), InvalidBorder);
        return false;
    }

    if (isCompressed)
    {
        GLenum actualInternalFormat =
            isSubImage ? texture->getFormat(target, level).info->sizedInternalFormat
                       : internalformat;
        switch (actualInternalFormat)
        {
            case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
            case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
                if (!context->getExtensions().textureCompressionDXT1)
                {
                    ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidInternalFormat);
                    return false;
                }
                break;
            case GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE:
                if (!context->getExtensions().textureCompressionDXT3)
                {
                    ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidInternalFormat);
                    return false;
                }
                break;
            case GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE:
                if (!context->getExtensions().textureCompressionDXT5)
                {
                    ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidInternalFormat);
                    return false;
                }
                break;
            case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
            case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
            case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
            case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
                if (!context->getExtensions().textureCompressionS3TCsRGB)
                {
                    ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidInternalFormat);
                    return false;
                }
                break;
            case GL_ETC1_RGB8_OES:
                if (!context->getExtensions().compressedETC1RGB8Texture)
                {
                    ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidInternalFormat);
                    return false;
                }
                if (isSubImage)
                {
                    ANGLE_VALIDATION_ERR(context, InvalidOperation(), InvalidInternalFormat);
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
                    ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidInternalFormat);
                    return false;
                }
                break;
            default:
                ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidInternalFormat);
                return false;
        }

        if (isSubImage)
        {
            if (!ValidCompressedSubImageSize(context, actualInternalFormat, xoffset, yoffset, width,
                                             height, texture->getWidth(target, level),
                                             texture->getHeight(target, level)))
            {
                context->handleError(InvalidOperation() << "Invalid compressed format dimension.");
                return false;
            }

            if (format != actualInternalFormat)
            {
                ANGLE_VALIDATION_ERR(context, InvalidOperation(), InvalidFormat);
                return false;
            }
        }
        else
        {
            if (!ValidCompressedImageSize(context, actualInternalFormat, level, width, height))
            {
                context->handleError(InvalidOperation() << "Invalid compressed format dimension.");
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
            default:
                ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidType);
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
                        ANGLE_VALIDATION_ERR(context, InvalidOperation(), MismatchedTypeAndFormat);
                        return false;
                }
                break;
            case GL_RED:
            case GL_RG:
                if (!context->getExtensions().textureRG)
                {
                    context->handleError(InvalidEnum());
                    return false;
                }
                switch (type)
                {
                    case GL_UNSIGNED_BYTE:
                        break;
                    case GL_FLOAT:
                    case GL_HALF_FLOAT_OES:
                        if (!context->getExtensions().textureFloat)
                        {
                            context->handleError(InvalidEnum());
                            return false;
                        }
                        break;
                    default:
                        ANGLE_VALIDATION_ERR(context, InvalidOperation(), MismatchedTypeAndFormat);
                        return false;
                }
                break;
            case GL_RGB:
                switch (type)
                {
                    case GL_UNSIGNED_BYTE:
                    case GL_UNSIGNED_SHORT_5_6_5:
                    case GL_FLOAT:
                    case GL_HALF_FLOAT_OES:
                        break;
                    default:
                        ANGLE_VALIDATION_ERR(context, InvalidOperation(), MismatchedTypeAndFormat);
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
                        break;
                    default:
                        ANGLE_VALIDATION_ERR(context, InvalidOperation(), MismatchedTypeAndFormat);
                        return false;
                }
                break;
            case GL_BGRA_EXT:
                if (!context->getExtensions().textureFormatBGRA8888)
                {
                    context->handleError(InvalidEnum());
                    return false;
                }
                switch (type)
                {
                    case GL_UNSIGNED_BYTE:
                        break;
                    default:
                        ANGLE_VALIDATION_ERR(context, InvalidOperation(), MismatchedTypeAndFormat);
                        return false;
                }
                break;
            case GL_SRGB_EXT:
            case GL_SRGB_ALPHA_EXT:
                if (!context->getExtensions().sRGB)
                {
                    context->handleError(InvalidEnum());
                    return false;
                }
                switch (type)
                {
                    case GL_UNSIGNED_BYTE:
                        break;
                    default:
                        ANGLE_VALIDATION_ERR(context, InvalidOperation(), MismatchedTypeAndFormat);
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
                    default:
                        ANGLE_VALIDATION_ERR(context, InvalidOperation(), MismatchedTypeAndFormat);
                        return false;
                }
                break;
            case GL_DEPTH_STENCIL_OES:
                switch (type)
                {
                    case GL_UNSIGNED_INT_24_8_OES:
                        break;
                    default:
                        ANGLE_VALIDATION_ERR(context, InvalidOperation(), MismatchedTypeAndFormat);
                        return false;
                }
                break;
            default:
                context->handleError(InvalidEnum());
                return false;
        }

        switch (format)
        {
            case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
            case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
                if (context->getExtensions().textureCompressionDXT1)
                {
                    context->handleError(InvalidOperation());
                    return false;
                }
                else
                {
                    context->handleError(InvalidEnum());
                    return false;
                }
                break;
            case GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE:
                if (context->getExtensions().textureCompressionDXT3)
                {
                    context->handleError(InvalidOperation());
                    return false;
                }
                else
                {
                    context->handleError(InvalidEnum());
                    return false;
                }
                break;
            case GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE:
                if (context->getExtensions().textureCompressionDXT5)
                {
                    context->handleError(InvalidOperation());
                    return false;
                }
                else
                {
                    context->handleError(InvalidEnum());
                    return false;
                }
                break;
            case GL_ETC1_RGB8_OES:
                if (context->getExtensions().compressedETC1RGB8Texture)
                {
                    context->handleError(InvalidOperation());
                    return false;
                }
                else
                {
                    context->handleError(InvalidEnum());
                    return false;
                }
                break;
            case GL_ETC1_RGB8_LOSSY_DECODE_ANGLE:
            case GL_COMPRESSED_RGB8_LOSSY_DECODE_ETC2_ANGLE:
            case GL_COMPRESSED_SRGB8_LOSSY_DECODE_ETC2_ANGLE:
            case GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_LOSSY_DECODE_ETC2_ANGLE:
            case GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_LOSSY_DECODE_ETC2_ANGLE:
                if (context->getExtensions().lossyETCDecode)
                {
                    context->handleError(InvalidOperation()
                                         << "ETC lossy decode formats can't work with this type.");
                    return false;
                }
                else
                {
                    context->handleError(InvalidEnum()
                                         << "ANGLE_lossy_etc_decode extension is not supported.");
                    return false;
                }
                break;
            case GL_DEPTH_COMPONENT:
            case GL_DEPTH_STENCIL_OES:
                if (!context->getExtensions().depthTextures)
                {
                    context->handleError(InvalidValue());
                    return false;
                }
                if (target != GL_TEXTURE_2D)
                {
                    ANGLE_VALIDATION_ERR(context, InvalidOperation(), MismatchedTargetAndFormat);
                    return false;
                }
                // OES_depth_texture supports loading depth data and multiple levels,
                // but ANGLE_depth_texture does not
                if (pixels != nullptr)
                {
                    ANGLE_VALIDATION_ERR(context, InvalidOperation(), PixelDataNotNull);
                    return false;
                }
                if (level != 0)
                {
                    ANGLE_VALIDATION_ERR(context, InvalidOperation(), LevelNotZero);
                    return false;
                }
                break;
            default:
                break;
        }

        if (!isSubImage)
        {
            switch (internalformat)
            {
                case GL_RGBA32F:
                    if (!context->getExtensions().colorBufferFloatRGBA)
                    {
                        context->handleError(InvalidValue()
                                             << "Sized GL_RGBA32F internal format requires "
                                                "GL_CHROMIUM_color_buffer_float_rgba");
                        return false;
                    }
                    if (type != GL_FLOAT)
                    {
                        ANGLE_VALIDATION_ERR(context, InvalidOperation(), MismatchedTypeAndFormat);
                        return false;
                    }
                    if (format != GL_RGBA)
                    {
                        ANGLE_VALIDATION_ERR(context, InvalidOperation(), MismatchedTypeAndFormat);
                        return false;
                    }
                    break;

                case GL_RGB32F:
                    if (!context->getExtensions().colorBufferFloatRGB)
                    {
                        context->handleError(InvalidValue()
                                             << "Sized GL_RGB32F internal format requires "
                                                "GL_CHROMIUM_color_buffer_float_rgb");
                        return false;
                    }
                    if (type != GL_FLOAT)
                    {
                        ANGLE_VALIDATION_ERR(context, InvalidOperation(), MismatchedTypeAndFormat);
                        return false;
                    }
                    if (format != GL_RGB)
                    {
                        ANGLE_VALIDATION_ERR(context, InvalidOperation(), MismatchedTypeAndFormat);
                        return false;
                    }
                    break;

                default:
                    break;
            }
        }

        if (type == GL_FLOAT)
        {
            if (!context->getExtensions().textureFloat)
            {
                context->handleError(InvalidEnum());
                return false;
            }
        }
        else if (type == GL_HALF_FLOAT_OES)
        {
            if (!context->getExtensions().textureHalfFloat)
            {
                context->handleError(InvalidEnum());
                return false;
            }
        }
    }

    GLenum sizeCheckFormat = isSubImage ? format : internalformat;
    if (!ValidImageDataSize(context, target, width, height, 1, sizeCheckFormat, type, pixels,
                            imageSize))
    {
        return false;
    }

    return true;
}

bool ValidateES2TexStorageParameters(Context *context,
                                     GLenum target,
                                     GLsizei levels,
                                     GLenum internalformat,
                                     GLsizei width,
                                     GLsizei height)
{
    if (target != GL_TEXTURE_2D && target != GL_TEXTURE_CUBE_MAP &&
        target != GL_TEXTURE_RECTANGLE_ANGLE)
    {
        context->handleError(InvalidEnum());
        return false;
    }

    if (width < 1 || height < 1 || levels < 1)
    {
        context->handleError(InvalidValue());
        return false;
    }

    if (target == GL_TEXTURE_CUBE_MAP && width != height)
    {
        context->handleError(InvalidValue());
        return false;
    }

    if (levels != 1 && levels != gl::log2(std::max(width, height)) + 1)
    {
        context->handleError(InvalidOperation());
        return false;
    }

    const gl::InternalFormat &formatInfo = gl::GetSizedInternalFormatInfo(internalformat);
    if (formatInfo.format == GL_NONE || formatInfo.type == GL_NONE)
    {
        context->handleError(InvalidEnum());
        return false;
    }

    const gl::Caps &caps = context->getCaps();

    switch (target)
    {
        case GL_TEXTURE_2D:
            if (static_cast<GLuint>(width) > caps.max2DTextureSize ||
                static_cast<GLuint>(height) > caps.max2DTextureSize)
            {
                context->handleError(InvalidValue());
                return false;
            }
            break;
        case GL_TEXTURE_RECTANGLE_ANGLE:
            if (static_cast<GLuint>(width) > caps.maxRectangleTextureSize ||
                static_cast<GLuint>(height) > caps.maxRectangleTextureSize || levels != 1)
            {
                context->handleError(InvalidValue());
                return false;
            }
            if (formatInfo.compressed)
            {
                context->handleError(InvalidEnum()
                                     << "Rectangle texture cannot have a compressed format.");
                return false;
            }
            break;
        case GL_TEXTURE_CUBE_MAP:
            if (static_cast<GLuint>(width) > caps.maxCubeMapTextureSize ||
                static_cast<GLuint>(height) > caps.maxCubeMapTextureSize)
            {
                context->handleError(InvalidValue());
                return false;
            }
            break;
        default:
            context->handleError(InvalidEnum());
            return false;
    }

    if (levels != 1 && !context->getExtensions().textureNPOT)
    {
        if (!gl::isPow2(width) || !gl::isPow2(height))
        {
            context->handleError(InvalidOperation());
            return false;
        }
    }

    switch (internalformat)
    {
        case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
            if (!context->getExtensions().textureCompressionDXT1)
            {
                context->handleError(InvalidEnum());
                return false;
            }
            break;
        case GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE:
            if (!context->getExtensions().textureCompressionDXT3)
            {
                context->handleError(InvalidEnum());
                return false;
            }
            break;
        case GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE:
            if (!context->getExtensions().textureCompressionDXT5)
            {
                context->handleError(InvalidEnum());
                return false;
            }
            break;
        case GL_ETC1_RGB8_OES:
            if (!context->getExtensions().compressedETC1RGB8Texture)
            {
                context->handleError(InvalidEnum());
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
                context->handleError(InvalidEnum()
                                     << "ANGLE_lossy_etc_decode extension is not supported.");
                return false;
            }
            break;
        case GL_RGBA32F_EXT:
        case GL_RGB32F_EXT:
        case GL_ALPHA32F_EXT:
        case GL_LUMINANCE32F_EXT:
        case GL_LUMINANCE_ALPHA32F_EXT:
            if (!context->getExtensions().textureFloat)
            {
                context->handleError(InvalidEnum());
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
                context->handleError(InvalidEnum());
                return false;
            }
            break;
        case GL_R8_EXT:
        case GL_RG8_EXT:
            if (!context->getExtensions().textureRG)
            {
                context->handleError(InvalidEnum());
                return false;
            }
            break;
        case GL_R16F_EXT:
        case GL_RG16F_EXT:
            if (!context->getExtensions().textureRG || !context->getExtensions().textureHalfFloat)
            {
                context->handleError(InvalidEnum());
                return false;
            }
            break;
        case GL_R32F_EXT:
        case GL_RG32F_EXT:
            if (!context->getExtensions().textureRG || !context->getExtensions().textureFloat)
            {
                context->handleError(InvalidEnum());
                return false;
            }
            break;
        case GL_DEPTH_COMPONENT16:
        case GL_DEPTH_COMPONENT32_OES:
        case GL_DEPTH24_STENCIL8_OES:
            if (!context->getExtensions().depthTextures)
            {
                context->handleError(InvalidEnum());
                return false;
            }
            if (target != GL_TEXTURE_2D)
            {
                context->handleError(InvalidOperation());
                return false;
            }
            // ANGLE_depth_texture only supports 1-level textures
            if (levels != 1)
            {
                context->handleError(InvalidOperation());
                return false;
            }
            break;
        default:
            break;
    }

    gl::Texture *texture = context->getTargetTexture(target);
    if (!texture || texture->id() == 0)
    {
        context->handleError(InvalidOperation());
        return false;
    }

    if (texture->getImmutableFormat())
    {
        context->handleError(InvalidOperation());
        return false;
    }

    return true;
}

bool ValidateDiscardFramebufferEXT(Context *context,
                                   GLenum target,
                                   GLsizei numAttachments,
                                   const GLenum *attachments)
{
    if (!context->getExtensions().discardFramebuffer)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ExtensionNotEnabled);
        return false;
    }

    bool defaultFramebuffer = false;

    switch (target)
    {
        case GL_FRAMEBUFFER:
            defaultFramebuffer =
                (context->getGLState().getTargetFramebuffer(GL_FRAMEBUFFER)->id() == 0);
            break;
        default:
            ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidFramebufferTarget);
            return false;
    }

    return ValidateDiscardFramebufferBase(context, target, numAttachments, attachments,
                                          defaultFramebuffer);
}

bool ValidateBindVertexArrayOES(Context *context, GLuint array)
{
    if (!context->getExtensions().vertexArrayObject)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ExtensionNotEnabled);
        return false;
    }

    return ValidateBindVertexArrayBase(context, array);
}

bool ValidateDeleteVertexArraysOES(Context *context, GLsizei n, const GLuint *arrays)
{
    if (!context->getExtensions().vertexArrayObject)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ExtensionNotEnabled);
        return false;
    }

    return ValidateGenOrDelete(context, n);
}

bool ValidateGenVertexArraysOES(Context *context, GLsizei n, GLuint *arrays)
{
    if (!context->getExtensions().vertexArrayObject)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ExtensionNotEnabled);
        return false;
    }

    return ValidateGenOrDelete(context, n);
}

bool ValidateIsVertexArrayOES(Context *context, GLuint array)
{
    if (!context->getExtensions().vertexArrayObject)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ExtensionNotEnabled);
        return false;
    }

    return true;
}

bool ValidateProgramBinaryOES(Context *context,
                              GLuint program,
                              GLenum binaryFormat,
                              const void *binary,
                              GLint length)
{
    if (!context->getExtensions().getProgramBinary)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ExtensionNotEnabled);
        return false;
    }

    return ValidateProgramBinaryBase(context, program, binaryFormat, binary, length);
}

bool ValidateGetProgramBinaryOES(Context *context,
                                 GLuint program,
                                 GLsizei bufSize,
                                 GLsizei *length,
                                 GLenum *binaryFormat,
                                 void *binary)
{
    if (!context->getExtensions().getProgramBinary)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ExtensionNotEnabled);
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

bool ValidateDebugMessageControlKHR(Context *context,
                                    GLenum source,
                                    GLenum type,
                                    GLenum severity,
                                    GLsizei count,
                                    const GLuint *ids,
                                    GLboolean enabled)
{
    if (!context->getExtensions().debug)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ExtensionNotEnabled);
        return false;
    }

    if (!ValidDebugSource(source, false) && source != GL_DONT_CARE)
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidDebugSource);
        return false;
    }

    if (!ValidDebugType(type) && type != GL_DONT_CARE)
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidDebugType);
        return false;
    }

    if (!ValidDebugSeverity(severity) && severity != GL_DONT_CARE)
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidDebugSeverity);
        return false;
    }

    if (count > 0)
    {
        if (source == GL_DONT_CARE || type == GL_DONT_CARE)
        {
            context->handleError(
                InvalidOperation()
                << "If count is greater than zero, source and severity cannot be GL_DONT_CARE.");
            return false;
        }

        if (severity != GL_DONT_CARE)
        {
            context->handleError(
                InvalidOperation()
                << "If count is greater than zero, severity must be GL_DONT_CARE.");
            return false;
        }
    }

    return true;
}

bool ValidateDebugMessageInsertKHR(Context *context,
                                   GLenum source,
                                   GLenum type,
                                   GLuint id,
                                   GLenum severity,
                                   GLsizei length,
                                   const GLchar *buf)
{
    if (!context->getExtensions().debug)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ExtensionNotEnabled);
        return false;
    }

    if (!context->getGLState().getDebug().isOutputEnabled())
    {
        // If the DEBUG_OUTPUT state is disabled calls to DebugMessageInsert are discarded and do
        // not generate an error.
        return false;
    }

    if (!ValidDebugSeverity(severity))
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidDebugSource);
        return false;
    }

    if (!ValidDebugType(type))
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidDebugType);
        return false;
    }

    if (!ValidDebugSource(source, true))
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidDebugSource);
        return false;
    }

    size_t messageLength = (length < 0) ? strlen(buf) : length;
    if (messageLength > context->getExtensions().maxDebugMessageLength)
    {
        context->handleError(InvalidValue()
                             << "Message length is larger than GL_MAX_DEBUG_MESSAGE_LENGTH.");
        return false;
    }

    return true;
}

bool ValidateDebugMessageCallbackKHR(Context *context,
                                     GLDEBUGPROCKHR callback,
                                     const void *userParam)
{
    if (!context->getExtensions().debug)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ExtensionNotEnabled);
        return false;
    }

    return true;
}

bool ValidateGetDebugMessageLogKHR(Context *context,
                                   GLuint count,
                                   GLsizei bufSize,
                                   GLenum *sources,
                                   GLenum *types,
                                   GLuint *ids,
                                   GLenum *severities,
                                   GLsizei *lengths,
                                   GLchar *messageLog)
{
    if (!context->getExtensions().debug)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ExtensionNotEnabled);
        return false;
    }

    if (bufSize < 0 && messageLog != nullptr)
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), NegativeBufferSize);
        return false;
    }

    return true;
}

bool ValidatePushDebugGroupKHR(Context *context,
                               GLenum source,
                               GLuint id,
                               GLsizei length,
                               const GLchar *message)
{
    if (!context->getExtensions().debug)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ExtensionNotEnabled);
        return false;
    }

    if (!ValidDebugSource(source, true))
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidDebugSource);
        return false;
    }

    size_t messageLength = (length < 0) ? strlen(message) : length;
    if (messageLength > context->getExtensions().maxDebugMessageLength)
    {
        context->handleError(InvalidValue()
                             << "Message length is larger than GL_MAX_DEBUG_MESSAGE_LENGTH.");
        return false;
    }

    size_t currentStackSize = context->getGLState().getDebug().getGroupStackDepth();
    if (currentStackSize >= context->getExtensions().maxDebugGroupStackDepth)
    {
        context
            ->handleError(StackOverflow()
                          << "Cannot push more than GL_MAX_DEBUG_GROUP_STACK_DEPTH debug groups.");
        return false;
    }

    return true;
}

bool ValidatePopDebugGroupKHR(Context *context)
{
    if (!context->getExtensions().debug)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ExtensionNotEnabled);
        return false;
    }

    size_t currentStackSize = context->getGLState().getDebug().getGroupStackDepth();
    if (currentStackSize <= 1)
    {
        context->handleError(StackUnderflow() << "Cannot pop the default debug group.");
        return false;
    }

    return true;
}

static bool ValidateObjectIdentifierAndName(Context *context, GLenum identifier, GLuint name)
{
    switch (identifier)
    {
        case GL_BUFFER:
            if (context->getBuffer(name) == nullptr)
            {
                context->handleError(InvalidValue() << "name is not a valid buffer.");
                return false;
            }
            return true;

        case GL_SHADER:
            if (context->getShader(name) == nullptr)
            {
                context->handleError(InvalidValue() << "name is not a valid shader.");
                return false;
            }
            return true;

        case GL_PROGRAM:
            if (context->getProgram(name) == nullptr)
            {
                context->handleError(InvalidValue() << "name is not a valid program.");
                return false;
            }
            return true;

        case GL_VERTEX_ARRAY:
            if (context->getVertexArray(name) == nullptr)
            {
                context->handleError(InvalidValue() << "name is not a valid vertex array.");
                return false;
            }
            return true;

        case GL_QUERY:
            if (context->getQuery(name) == nullptr)
            {
                context->handleError(InvalidValue() << "name is not a valid query.");
                return false;
            }
            return true;

        case GL_TRANSFORM_FEEDBACK:
            if (context->getTransformFeedback(name) == nullptr)
            {
                context->handleError(InvalidValue() << "name is not a valid transform feedback.");
                return false;
            }
            return true;

        case GL_SAMPLER:
            if (context->getSampler(name) == nullptr)
            {
                context->handleError(InvalidValue() << "name is not a valid sampler.");
                return false;
            }
            return true;

        case GL_TEXTURE:
            if (context->getTexture(name) == nullptr)
            {
                context->handleError(InvalidValue() << "name is not a valid texture.");
                return false;
            }
            return true;

        case GL_RENDERBUFFER:
            if (context->getRenderbuffer(name) == nullptr)
            {
                context->handleError(InvalidValue() << "name is not a valid renderbuffer.");
                return false;
            }
            return true;

        case GL_FRAMEBUFFER:
            if (context->getFramebuffer(name) == nullptr)
            {
                context->handleError(InvalidValue() << "name is not a valid framebuffer.");
                return false;
            }
            return true;

        default:
            context->handleError(InvalidEnum() << "Invalid identifier.");
            return false;
    }
}

static bool ValidateLabelLength(Context *context, GLsizei length, const GLchar *label)
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
        context->handleError(InvalidValue() << "Label length is larger than GL_MAX_LABEL_LENGTH.");
        return false;
    }

    return true;
}

bool ValidateObjectLabelKHR(Context *context,
                            GLenum identifier,
                            GLuint name,
                            GLsizei length,
                            const GLchar *label)
{
    if (!context->getExtensions().debug)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ExtensionNotEnabled);
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

bool ValidateGetObjectLabelKHR(Context *context,
                               GLenum identifier,
                               GLuint name,
                               GLsizei bufSize,
                               GLsizei *length,
                               GLchar *label)
{
    if (!context->getExtensions().debug)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ExtensionNotEnabled);
        return false;
    }

    if (bufSize < 0)
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), NegativeBufferSize);
        return false;
    }

    if (!ValidateObjectIdentifierAndName(context, identifier, name))
    {
        return false;
    }

    return true;
}

static bool ValidateObjectPtrName(Context *context, const void *ptr)
{
    if (context->getSync(reinterpret_cast<GLsync>(const_cast<void *>(ptr))) == nullptr)
    {
        context->handleError(InvalidValue() << "name is not a valid sync.");
        return false;
    }

    return true;
}

bool ValidateObjectPtrLabelKHR(Context *context,
                               const void *ptr,
                               GLsizei length,
                               const GLchar *label)
{
    if (!context->getExtensions().debug)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ExtensionNotEnabled);
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

bool ValidateGetObjectPtrLabelKHR(Context *context,
                                  const void *ptr,
                                  GLsizei bufSize,
                                  GLsizei *length,
                                  GLchar *label)
{
    if (!context->getExtensions().debug)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ExtensionNotEnabled);
        return false;
    }

    if (bufSize < 0)
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), NegativeBufferSize);
        return false;
    }

    if (!ValidateObjectPtrName(context, ptr))
    {
        return false;
    }

    return true;
}

bool ValidateGetPointervKHR(Context *context, GLenum pname, void **params)
{
    if (!context->getExtensions().debug)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ExtensionNotEnabled);
        return false;
    }

    // TODO: represent this in Context::getQueryParameterInfo.
    switch (pname)
    {
        case GL_DEBUG_CALLBACK_FUNCTION:
        case GL_DEBUG_CALLBACK_USER_PARAM:
            break;

        default:
            ANGLE_VALIDATION_ERR(context, InvalidEnum(), EnumNotSupported);
            return false;
    }

    return true;
}

bool ValidateBlitFramebufferANGLE(Context *context,
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
        context->handleError(InvalidOperation() << "Blit extension not available.");
        return false;
    }

    if (srcX1 - srcX0 != dstX1 - dstX0 || srcY1 - srcY0 != dstY1 - dstY0)
    {
        // TODO(jmadill): Determine if this should be available on other implementations.
        context->handleError(InvalidOperation() << "Scaling and flipping in "
                                                   "BlitFramebufferANGLE not supported by this "
                                                   "implementation.");
        return false;
    }

    if (filter == GL_LINEAR)
    {
        context->handleError(InvalidEnum() << "Linear blit not supported in this extension");
        return false;
    }

    Framebuffer *readFramebuffer = context->getGLState().getReadFramebuffer();
    Framebuffer *drawFramebuffer = context->getGLState().getDrawFramebuffer();

    if (mask & GL_COLOR_BUFFER_BIT)
    {
        const FramebufferAttachment *readColorAttachment = readFramebuffer->getReadColorbuffer();
        const FramebufferAttachment *drawColorAttachment = drawFramebuffer->getFirstColorbuffer();

        if (readColorAttachment && drawColorAttachment)
        {
            if (!(readColorAttachment->type() == GL_TEXTURE &&
                  readColorAttachment->getTextureImageIndex().type == GL_TEXTURE_2D) &&
                readColorAttachment->type() != GL_RENDERBUFFER &&
                readColorAttachment->type() != GL_FRAMEBUFFER_DEFAULT)
            {
                context->handleError(InvalidOperation());
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
                          attachment->getTextureImageIndex().type == GL_TEXTURE_2D) &&
                        attachment->type() != GL_RENDERBUFFER &&
                        attachment->type() != GL_FRAMEBUFFER_DEFAULT)
                    {
                        context->handleError(InvalidOperation());
                        return false;
                    }

                    // Return an error if the destination formats do not match
                    if (!Format::EquivalentForBlit(attachment->getFormat(),
                                                   readColorAttachment->getFormat()))
                    {
                        context->handleError(InvalidOperation());
                        return false;
                    }
                }
            }

            if (readFramebuffer->getSamples(context) != 0 &&
                IsPartialBlit(context, readColorAttachment, drawColorAttachment, srcX0, srcY0,
                              srcX1, srcY1, dstX0, dstY0, dstX1, dstY1))
            {
                context->handleError(InvalidOperation());
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
                readFramebuffer->getAttachment(attachments[i]);
            const FramebufferAttachment *drawBuffer =
                drawFramebuffer->getAttachment(attachments[i]);

            if (readBuffer && drawBuffer)
            {
                if (IsPartialBlit(context, readBuffer, drawBuffer, srcX0, srcY0, srcX1, srcY1,
                                  dstX0, dstY0, dstX1, dstY1))
                {
                    // only whole-buffer copies are permitted
                    context->handleError(InvalidOperation() << "Only whole-buffer depth and "
                                                               "stencil blits are supported by "
                                                               "this extension.");
                    return false;
                }

                if (readBuffer->getSamples() != 0 || drawBuffer->getSamples() != 0)
                {
                    context->handleError(InvalidOperation());
                    return false;
                }
            }
        }
    }

    return ValidateBlitFramebufferParameters(context, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0,
                                             dstX1, dstY1, mask, filter);
}

bool ValidateClear(ValidationContext *context, GLbitfield mask)
{
    Framebuffer *fbo = context->getGLState().getDrawFramebuffer();
    if (fbo->checkStatus(context) != GL_FRAMEBUFFER_COMPLETE)
    {
        context->handleError(InvalidFramebufferOperation());
        return false;
    }

    if ((mask & ~(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) != 0)
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), InvalidClearMask);
        return false;
    }

    if (context->getExtensions().webglCompatibility && (mask & GL_COLOR_BUFFER_BIT) != 0)
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

    return true;
}

bool ValidateDrawBuffersEXT(ValidationContext *context, GLsizei n, const GLenum *bufs)
{
    if (!context->getExtensions().drawBuffers)
    {
        context->handleError(InvalidOperation() << "Extension not supported.");
        return false;
    }

    return ValidateDrawBuffersBase(context, n, bufs);
}

bool ValidateTexImage2D(Context *context,
                        GLenum target,
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

bool ValidateTexImage2DRobust(Context *context,
                              GLenum target,
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

bool ValidateTexSubImage2D(Context *context,
                           GLenum target,
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

bool ValidateTexSubImage2DRobustANGLE(Context *context,
                                      GLenum target,
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

bool ValidateCompressedTexImage2D(Context *context,
                                  GLenum target,
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
    auto blockSizeOrErr = formatInfo.computeCompressedImageSize(gl::Extents(width, height, 1));
    if (blockSizeOrErr.isError())
    {
        context->handleError(blockSizeOrErr.getError());
        return false;
    }

    if (imageSize < 0 || static_cast<GLuint>(imageSize) != blockSizeOrErr.getResult())
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), CompressedTextureDimensionsMustMatchData);
        return false;
    }

    if (target == GL_TEXTURE_RECTANGLE_ANGLE)
    {
        context->handleError(InvalidEnum() << "Rectangle texture cannot have a compressed format.");
        return false;
    }

    return true;
}

bool ValidateCompressedTexImage2DRobustANGLE(Context *context,
                                             GLenum target,
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
bool ValidateCompressedTexSubImage2DRobustANGLE(Context *context,
                                                GLenum target,
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

bool ValidateCompressedTexSubImage2D(Context *context,
                                     GLenum target,
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
    auto blockSizeOrErr = formatInfo.computeCompressedImageSize(gl::Extents(width, height, 1));
    if (blockSizeOrErr.isError())
    {
        context->handleError(blockSizeOrErr.getError());
        return false;
    }

    if (imageSize < 0 || static_cast<GLuint>(imageSize) != blockSizeOrErr.getResult())
    {
        context->handleError(InvalidValue());
        return false;
    }

    return true;
}

bool ValidateGetBufferPointervOES(Context *context,
                                  BufferBinding target,
                                  GLenum pname,
                                  void **params)
{
    return ValidateGetBufferPointervBase(context, target, pname, nullptr, params);
}

bool ValidateMapBufferOES(Context *context, BufferBinding target, GLenum access)
{
    if (!context->getExtensions().mapBuffer)
    {
        context->handleError(InvalidOperation() << "Map buffer extension not available.");
        return false;
    }

    if (!ValidBufferType(context, target))
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidBufferTypes);
        return false;
    }

    Buffer *buffer = context->getGLState().getTargetBuffer(target);

    if (buffer == nullptr)
    {
        context->handleError(InvalidOperation() << "Attempted to map buffer object zero.");
        return false;
    }

    if (access != GL_WRITE_ONLY_OES)
    {
        context->handleError(InvalidEnum() << "Non-write buffer mapping not supported.");
        return false;
    }

    if (buffer->isMapped())
    {
        context->handleError(InvalidOperation() << "Buffer is already mapped.");
        return false;
    }

    return ValidateMapBufferBase(context, target);
}

bool ValidateUnmapBufferOES(Context *context, BufferBinding target)
{
    if (!context->getExtensions().mapBuffer)
    {
        context->handleError(InvalidOperation() << "Map buffer extension not available.");
        return false;
    }

    return ValidateUnmapBufferBase(context, target);
}

bool ValidateMapBufferRangeEXT(Context *context,
                               BufferBinding target,
                               GLintptr offset,
                               GLsizeiptr length,
                               GLbitfield access)
{
    if (!context->getExtensions().mapBufferRange)
    {
        context->handleError(InvalidOperation() << "Map buffer range extension not available.");
        return false;
    }

    return ValidateMapBufferRangeBase(context, target, offset, length, access);
}

bool ValidateMapBufferBase(Context *context, BufferBinding target)
{
    Buffer *buffer = context->getGLState().getTargetBuffer(target);
    ASSERT(buffer != nullptr);

    // Check if this buffer is currently being used as a transform feedback output buffer
    TransformFeedback *transformFeedback = context->getGLState().getCurrentTransformFeedback();
    if (transformFeedback != nullptr && transformFeedback->isActive())
    {
        for (size_t i = 0; i < transformFeedback->getIndexedBufferCount(); i++)
        {
            const auto &transformFeedbackBuffer = transformFeedback->getIndexedBuffer(i);
            if (transformFeedbackBuffer.get() == buffer)
            {
                context->handleError(InvalidOperation()
                                     << "Buffer is currently bound for transform feedback.");
                return false;
            }
        }
    }

    return true;
}

bool ValidateFlushMappedBufferRangeEXT(Context *context,
                                       BufferBinding target,
                                       GLintptr offset,
                                       GLsizeiptr length)
{
    if (!context->getExtensions().mapBufferRange)
    {
        context->handleError(InvalidOperation() << "Map buffer range extension not available.");
        return false;
    }

    return ValidateFlushMappedBufferRangeBase(context, target, offset, length);
}

bool ValidateBindTexture(Context *context, GLenum target, GLuint texture)
{
    Texture *textureObject = context->getTexture(texture);
    if (textureObject && textureObject->getTarget() != target && texture != 0)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), TypeMismatch);
        return false;
    }

    if (!context->getGLState().isBindGeneratesResourceEnabled() &&
        !context->isTextureGenerated(texture))
    {
        context->handleError(InvalidOperation() << "Texture was not generated");
        return false;
    }

    switch (target)
    {
        case GL_TEXTURE_2D:
        case GL_TEXTURE_CUBE_MAP:
            break;

        case GL_TEXTURE_RECTANGLE_ANGLE:
            if (!context->getExtensions().textureRectangle)
            {
                context->handleError(InvalidEnum()
                                     << "Context does not support GL_ANGLE_texture_rectangle");
                return false;
            }
            break;

        case GL_TEXTURE_3D:
        case GL_TEXTURE_2D_ARRAY:
            if (context->getClientMajorVersion() < 3)
            {
                ANGLE_VALIDATION_ERR(context, InvalidEnum(), ES3Required);
                return false;
            }
            break;

        case GL_TEXTURE_2D_MULTISAMPLE:
            if (context->getClientVersion() < Version(3, 1))
            {
                ANGLE_VALIDATION_ERR(context, InvalidEnum(), ES31Required);
                return false;
            }
            break;

        case GL_TEXTURE_EXTERNAL_OES:
            if (!context->getExtensions().eglImageExternal &&
                !context->getExtensions().eglStreamConsumerExternal)
            {
                context->handleError(InvalidEnum() << "External texture extension not enabled");
                return false;
            }
            break;
        default:
            ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidTextureTarget);
            return false;
    }

    return true;
}

bool ValidateBindUniformLocationCHROMIUM(Context *context,
                                         GLuint program,
                                         GLint location,
                                         const GLchar *name)
{
    if (!context->getExtensions().bindUniformLocation)
    {
        context->handleError(InvalidOperation()
                             << "GL_CHROMIUM_bind_uniform_location is not available.");
        return false;
    }

    Program *programObject = GetValidProgram(context, program);
    if (!programObject)
    {
        return false;
    }

    if (location < 0)
    {
        context->handleError(InvalidValue() << "Location cannot be less than 0.");
        return false;
    }

    const Caps &caps = context->getCaps();
    if (static_cast<size_t>(location) >=
        (caps.maxVertexUniformVectors + caps.maxFragmentUniformVectors) * 4)
    {
        context->handleError(InvalidValue() << "Location must be less than "
                                               "(MAX_VERTEX_UNIFORM_VECTORS + "
                                               "MAX_FRAGMENT_UNIFORM_VECTORS) * 4");
        return false;
    }

    // The WebGL spec (section 6.20) disallows strings containing invalid ESSL characters for
    // shader-related entry points
    if (context->getExtensions().webglCompatibility && !IsValidESSLString(name, strlen(name)))
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), InvalidNameCharacters);
        return false;
    }

    if (strncmp(name, "gl_", 3) == 0)
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), NameBeginsWithGL);
        return false;
    }

    return true;
}

bool ValidateCoverageModulationCHROMIUM(Context *context, GLenum components)
{
    if (!context->getExtensions().framebufferMixedSamples)
    {
        context->handleError(InvalidOperation()
                             << "GL_CHROMIUM_framebuffer_mixed_samples is not available.");
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
            context->handleError(
                InvalidEnum()
                << "GLenum components is not one of GL_RGB, GL_RGBA, GL_ALPHA or GL_NONE.");
            return false;
    }

    return true;
}

// CHROMIUM_path_rendering

bool ValidateMatrix(Context *context, GLenum matrixMode, const GLfloat *matrix)
{
    if (!context->getExtensions().pathRendering)
    {
        context->handleError(InvalidOperation() << "GL_CHROMIUM_path_rendering is not available.");
        return false;
    }
    if (matrixMode != GL_PATH_MODELVIEW_CHROMIUM && matrixMode != GL_PATH_PROJECTION_CHROMIUM)
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidMatrixMode);
        return false;
    }
    if (matrix == nullptr)
    {
        context->handleError(InvalidOperation() << "Invalid matrix.");
        return false;
    }
    return true;
}

bool ValidateMatrixMode(Context *context, GLenum matrixMode)
{
    if (!context->getExtensions().pathRendering)
    {
        context->handleError(InvalidOperation() << "GL_CHROMIUM_path_rendering is not available.");
        return false;
    }
    if (matrixMode != GL_PATH_MODELVIEW_CHROMIUM && matrixMode != GL_PATH_PROJECTION_CHROMIUM)
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidMatrixMode);
        return false;
    }
    return true;
}

bool ValidateGenPaths(Context *context, GLsizei range)
{
    if (!context->getExtensions().pathRendering)
    {
        context->handleError(InvalidOperation() << "GL_CHROMIUM_path_rendering is not available.");
        return false;
    }

    // range = 0 is undefined in NV_path_rendering.
    // we add stricter semantic check here and require a non zero positive range.
    if (range <= 0)
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), InvalidRange);
        return false;
    }

    if (!angle::IsValueInRangeForNumericType<std::uint32_t>(range))
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), IntegerOverflow);
        return false;
    }

    return true;
}

bool ValidateDeletePaths(Context *context, GLuint path, GLsizei range)
{
    if (!context->getExtensions().pathRendering)
    {
        context->handleError(InvalidOperation() << "GL_CHROMIUM_path_rendering is not available.");
        return false;
    }

    // range = 0 is undefined in NV_path_rendering.
    // we add stricter semantic check here and require a non zero positive range.
    if (range <= 0)
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), InvalidRange);
        return false;
    }

    angle::CheckedNumeric<std::uint32_t> checkedRange(path);
    checkedRange += range;

    if (!angle::IsValueInRangeForNumericType<std::uint32_t>(range) || !checkedRange.IsValid())
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), IntegerOverflow);
        return false;
    }
    return true;
}

bool ValidatePathCommands(Context *context,
                          GLuint path,
                          GLsizei numCommands,
                          const GLubyte *commands,
                          GLsizei numCoords,
                          GLenum coordType,
                          const void *coords)
{
    if (!context->getExtensions().pathRendering)
    {
        context->handleError(InvalidOperation() << "GL_CHROMIUM_path_rendering is not available.");
        return false;
    }
    if (!context->hasPath(path))
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), NoSuchPath);
        return false;
    }

    if (numCommands < 0)
    {
        context->handleError(InvalidValue() << "Invalid number of commands.");
        return false;
    }
    else if (numCommands > 0)
    {
        if (!commands)
        {
            context->handleError(InvalidValue() << "No commands array given.");
            return false;
        }
    }

    if (numCoords < 0)
    {
        context->handleError(InvalidValue() << "Invalid number of coordinates.");
        return false;
    }
    else if (numCoords > 0)
    {
        if (!coords)
        {
            context->handleError(InvalidValue() << "No coordinate array given.");
            return false;
        }
    }

    std::uint32_t coordTypeSize = 0;
    switch (coordType)
    {
        case GL_BYTE:
            coordTypeSize = sizeof(GLbyte);
            break;

        case GL_UNSIGNED_BYTE:
            coordTypeSize = sizeof(GLubyte);
            break;

        case GL_SHORT:
            coordTypeSize = sizeof(GLshort);
            break;

        case GL_UNSIGNED_SHORT:
            coordTypeSize = sizeof(GLushort);
            break;

        case GL_FLOAT:
            coordTypeSize = sizeof(GLfloat);
            break;

        default:
            context->handleError(InvalidEnum() << "Invalid coordinate type.");
            return false;
    }

    angle::CheckedNumeric<std::uint32_t> checkedSize(numCommands);
    checkedSize += (coordTypeSize * numCoords);
    if (!checkedSize.IsValid())
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), IntegerOverflow);
        return false;
    }

    // early return skips command data validation when it doesn't exist.
    if (!commands)
        return true;

    GLsizei expectedNumCoords = 0;
    for (GLsizei i = 0; i < numCommands; ++i)
    {
        switch (commands[i])
        {
            case GL_CLOSE_PATH_CHROMIUM:  // no coordinates.
                break;
            case GL_MOVE_TO_CHROMIUM:
            case GL_LINE_TO_CHROMIUM:
                expectedNumCoords += 2;
                break;
            case GL_QUADRATIC_CURVE_TO_CHROMIUM:
                expectedNumCoords += 4;
                break;
            case GL_CUBIC_CURVE_TO_CHROMIUM:
                expectedNumCoords += 6;
                break;
            case GL_CONIC_CURVE_TO_CHROMIUM:
                expectedNumCoords += 5;
                break;
            default:
                context->handleError(InvalidEnum() << "Invalid command.");
                return false;
        }
    }
    if (expectedNumCoords != numCoords)
    {
        context->handleError(InvalidValue() << "Invalid number of coordinates.");
        return false;
    }

    return true;
}

bool ValidateSetPathParameter(Context *context, GLuint path, GLenum pname, GLfloat value)
{
    if (!context->getExtensions().pathRendering)
    {
        context->handleError(InvalidOperation() << "GL_CHROMIUM_path_rendering is not available.");
        return false;
    }
    if (!context->hasPath(path))
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), NoSuchPath);
        return false;
    }

    switch (pname)
    {
        case GL_PATH_STROKE_WIDTH_CHROMIUM:
            if (value < 0.0f)
            {
                context->handleError(InvalidValue() << "Invalid stroke width.");
                return false;
            }
            break;
        case GL_PATH_END_CAPS_CHROMIUM:
            switch (static_cast<GLenum>(value))
            {
                case GL_FLAT_CHROMIUM:
                case GL_SQUARE_CHROMIUM:
                case GL_ROUND_CHROMIUM:
                    break;
                default:
                    context->handleError(InvalidEnum() << "Invalid end caps.");
                    return false;
            }
            break;
        case GL_PATH_JOIN_STYLE_CHROMIUM:
            switch (static_cast<GLenum>(value))
            {
                case GL_MITER_REVERT_CHROMIUM:
                case GL_BEVEL_CHROMIUM:
                case GL_ROUND_CHROMIUM:
                    break;
                default:
                    context->handleError(InvalidEnum() << "Invalid join style.");
                    return false;
            }
        case GL_PATH_MITER_LIMIT_CHROMIUM:
            if (value < 0.0f)
            {
                context->handleError(InvalidValue() << "Invalid miter limit.");
                return false;
            }
            break;

        case GL_PATH_STROKE_BOUND_CHROMIUM:
            // no errors, only clamping.
            break;

        default:
            context->handleError(InvalidEnum() << "Invalid path parameter.");
            return false;
    }
    return true;
}

bool ValidateGetPathParameter(Context *context, GLuint path, GLenum pname, GLfloat *value)
{
    if (!context->getExtensions().pathRendering)
    {
        context->handleError(InvalidOperation() << "GL_CHROMIUM_path_rendering is not available.");
        return false;
    }

    if (!context->hasPath(path))
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), NoSuchPath);
        return false;
    }
    if (!value)
    {
        context->handleError(InvalidValue() << "No value array.");
        return false;
    }

    switch (pname)
    {
        case GL_PATH_STROKE_WIDTH_CHROMIUM:
        case GL_PATH_END_CAPS_CHROMIUM:
        case GL_PATH_JOIN_STYLE_CHROMIUM:
        case GL_PATH_MITER_LIMIT_CHROMIUM:
        case GL_PATH_STROKE_BOUND_CHROMIUM:
            break;

        default:
            context->handleError(InvalidEnum() << "Invalid path parameter.");
            return false;
    }

    return true;
}

bool ValidatePathStencilFunc(Context *context, GLenum func, GLint ref, GLuint mask)
{
    if (!context->getExtensions().pathRendering)
    {
        context->handleError(InvalidOperation() << "GL_CHROMIUM_path_rendering is not available.");
        return false;
    }

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
            break;
        default:
            ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidStencil);
            return false;
    }

    return true;
}

// Note that the spec specifies that for the path drawing commands
// if the path object is not an existing path object the command
// does nothing and no error is generated.
// However if the path object exists but has not been specified any
// commands then an error is generated.

bool ValidateStencilFillPath(Context *context, GLuint path, GLenum fillMode, GLuint mask)
{
    if (!context->getExtensions().pathRendering)
    {
        context->handleError(InvalidOperation() << "GL_CHROMIUM_path_rendering is not available.");
        return false;
    }
    if (context->hasPath(path) && !context->hasPathData(path))
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), NoSuchPath);
        return false;
    }

    switch (fillMode)
    {
        case GL_COUNT_UP_CHROMIUM:
        case GL_COUNT_DOWN_CHROMIUM:
            break;
        default:
            ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidFillMode);
            return false;
    }

    if (!isPow2(mask + 1))
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), InvalidStencilBitMask);
        return false;
    }

    return true;
}

bool ValidateStencilStrokePath(Context *context, GLuint path, GLint reference, GLuint mask)
{
    if (!context->getExtensions().pathRendering)
    {
        context->handleError(InvalidOperation() << "GL_CHROMIUM_path_rendering is not available.");
        return false;
    }
    if (context->hasPath(path) && !context->hasPathData(path))
    {
        context->handleError(InvalidOperation() << "No such path or path has no data.");
        return false;
    }

    return true;
}

bool ValidateCoverPath(Context *context, GLuint path, GLenum coverMode)
{
    if (!context->getExtensions().pathRendering)
    {
        context->handleError(InvalidOperation() << "GL_CHROMIUM_path_rendering is not available.");
        return false;
    }
    if (context->hasPath(path) && !context->hasPathData(path))
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), NoSuchPath);
        return false;
    }

    switch (coverMode)
    {
        case GL_CONVEX_HULL_CHROMIUM:
        case GL_BOUNDING_BOX_CHROMIUM:
            break;
        default:
            ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidCoverMode);
            return false;
    }
    return true;
}

bool ValidateStencilThenCoverFillPath(Context *context,
                                      GLuint path,
                                      GLenum fillMode,
                                      GLuint mask,
                                      GLenum coverMode)
{
    return ValidateStencilFillPath(context, path, fillMode, mask) &&
           ValidateCoverPath(context, path, coverMode);
}

bool ValidateStencilThenCoverStrokePath(Context *context,
                                        GLuint path,
                                        GLint reference,
                                        GLuint mask,
                                        GLenum coverMode)
{
    return ValidateStencilStrokePath(context, path, reference, mask) &&
           ValidateCoverPath(context, path, coverMode);
}

bool ValidateIsPath(Context *context)
{
    if (!context->getExtensions().pathRendering)
    {
        context->handleError(InvalidOperation() << "GL_CHROMIUM_path_rendering is not available.");
        return false;
    }
    return true;
}

bool ValidateCoverFillPathInstanced(Context *context,
                                    GLsizei numPaths,
                                    GLenum pathNameType,
                                    const void *paths,
                                    GLuint pathBase,
                                    GLenum coverMode,
                                    GLenum transformType,
                                    const GLfloat *transformValues)
{
    if (!ValidateInstancedPathParameters(context, numPaths, pathNameType, paths, pathBase,
                                         transformType, transformValues))
        return false;

    switch (coverMode)
    {
        case GL_CONVEX_HULL_CHROMIUM:
        case GL_BOUNDING_BOX_CHROMIUM:
        case GL_BOUNDING_BOX_OF_BOUNDING_BOXES_CHROMIUM:
            break;
        default:
            ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidCoverMode);
            return false;
    }

    return true;
}

bool ValidateCoverStrokePathInstanced(Context *context,
                                      GLsizei numPaths,
                                      GLenum pathNameType,
                                      const void *paths,
                                      GLuint pathBase,
                                      GLenum coverMode,
                                      GLenum transformType,
                                      const GLfloat *transformValues)
{
    if (!ValidateInstancedPathParameters(context, numPaths, pathNameType, paths, pathBase,
                                         transformType, transformValues))
        return false;

    switch (coverMode)
    {
        case GL_CONVEX_HULL_CHROMIUM:
        case GL_BOUNDING_BOX_CHROMIUM:
        case GL_BOUNDING_BOX_OF_BOUNDING_BOXES_CHROMIUM:
            break;
        default:
            ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidCoverMode);
            return false;
    }

    return true;
}

bool ValidateStencilFillPathInstanced(Context *context,
                                      GLsizei numPaths,
                                      GLenum pathNameType,
                                      const void *paths,
                                      GLuint pathBase,
                                      GLenum fillMode,
                                      GLuint mask,
                                      GLenum transformType,
                                      const GLfloat *transformValues)
{

    if (!ValidateInstancedPathParameters(context, numPaths, pathNameType, paths, pathBase,
                                         transformType, transformValues))
        return false;

    switch (fillMode)
    {
        case GL_COUNT_UP_CHROMIUM:
        case GL_COUNT_DOWN_CHROMIUM:
            break;
        default:
            ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidFillMode);
            return false;
    }
    if (!isPow2(mask + 1))
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), InvalidStencilBitMask);
        return false;
    }
    return true;
}

bool ValidateStencilStrokePathInstanced(Context *context,
                                        GLsizei numPaths,
                                        GLenum pathNameType,
                                        const void *paths,
                                        GLuint pathBase,
                                        GLint reference,
                                        GLuint mask,
                                        GLenum transformType,
                                        const GLfloat *transformValues)
{
    if (!ValidateInstancedPathParameters(context, numPaths, pathNameType, paths, pathBase,
                                         transformType, transformValues))
        return false;

    // no more validation here.

    return true;
}

bool ValidateStencilThenCoverFillPathInstanced(Context *context,
                                               GLsizei numPaths,
                                               GLenum pathNameType,
                                               const void *paths,
                                               GLuint pathBase,
                                               GLenum fillMode,
                                               GLuint mask,
                                               GLenum coverMode,
                                               GLenum transformType,
                                               const GLfloat *transformValues)
{
    if (!ValidateInstancedPathParameters(context, numPaths, pathNameType, paths, pathBase,
                                         transformType, transformValues))
        return false;

    switch (coverMode)
    {
        case GL_CONVEX_HULL_CHROMIUM:
        case GL_BOUNDING_BOX_CHROMIUM:
        case GL_BOUNDING_BOX_OF_BOUNDING_BOXES_CHROMIUM:
            break;
        default:
            ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidCoverMode);
            return false;
    }

    switch (fillMode)
    {
        case GL_COUNT_UP_CHROMIUM:
        case GL_COUNT_DOWN_CHROMIUM:
            break;
        default:
            ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidFillMode);
            return false;
    }
    if (!isPow2(mask + 1))
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), InvalidStencilBitMask);
        return false;
    }

    return true;
}

bool ValidateStencilThenCoverStrokePathInstanced(Context *context,
                                                 GLsizei numPaths,
                                                 GLenum pathNameType,
                                                 const void *paths,
                                                 GLuint pathBase,
                                                 GLint reference,
                                                 GLuint mask,
                                                 GLenum coverMode,
                                                 GLenum transformType,
                                                 const GLfloat *transformValues)
{
    if (!ValidateInstancedPathParameters(context, numPaths, pathNameType, paths, pathBase,
                                         transformType, transformValues))
        return false;

    switch (coverMode)
    {
        case GL_CONVEX_HULL_CHROMIUM:
        case GL_BOUNDING_BOX_CHROMIUM:
        case GL_BOUNDING_BOX_OF_BOUNDING_BOXES_CHROMIUM:
            break;
        default:
            ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidCoverMode);
            return false;
    }

    return true;
}

bool ValidateBindFragmentInputLocation(Context *context,
                                       GLuint program,
                                       GLint location,
                                       const GLchar *name)
{
    if (!context->getExtensions().pathRendering)
    {
        context->handleError(InvalidOperation() << "GL_CHROMIUM_path_rendering is not available.");
        return false;
    }

    const GLint MaxLocation = context->getCaps().maxVaryingVectors * 4;
    if (location >= MaxLocation)
    {
        context->handleError(InvalidValue() << "Location exceeds max varying.");
        return false;
    }

    const auto *programObject = context->getProgram(program);
    if (!programObject)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ProgramNotBound);
        return false;
    }

    if (!name)
    {
        context->handleError(InvalidValue() << "No name given.");
        return false;
    }

    if (angle::BeginsWith(name, "gl_"))
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), NameBeginsWithGL);
        return false;
    }

    return true;
}

bool ValidateProgramPathFragmentInputGen(Context *context,
                                         GLuint program,
                                         GLint location,
                                         GLenum genMode,
                                         GLint components,
                                         const GLfloat *coeffs)
{
    if (!context->getExtensions().pathRendering)
    {
        context->handleError(InvalidOperation() << "GL_CHROMIUM_path_rendering is not available.");
        return false;
    }

    const auto *programObject = context->getProgram(program);
    if (!programObject || programObject->isFlaggedForDeletion())
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ProgramDoesNotExist);
        return false;
    }

    if (!programObject->isLinked())
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ProgramNotLinked);
        return false;
    }

    switch (genMode)
    {
        case GL_NONE:
            if (components != 0)
            {
                context->handleError(InvalidValue() << "Invalid components.");
                return false;
            }
            break;

        case GL_OBJECT_LINEAR_CHROMIUM:
        case GL_EYE_LINEAR_CHROMIUM:
        case GL_CONSTANT_CHROMIUM:
            if (components < 1 || components > 4)
            {
                context->handleError(InvalidValue() << "Invalid components.");
                return false;
            }
            if (!coeffs)
            {
                context->handleError(InvalidValue() << "No coefficients array given.");
                return false;
            }
            break;

        default:
            context->handleError(InvalidEnum() << "Invalid gen mode.");
            return false;
    }

    // If the location is -1 then the command is silently ignored
    // and no further validation is needed.
    if (location == -1)
        return true;

    const auto &binding = programObject->getFragmentInputBindingInfo(context, location);

    if (!binding.valid)
    {
        context->handleError(InvalidOperation() << "No such binding.");
        return false;
    }

    if (binding.type != GL_NONE)
    {
        GLint expectedComponents = 0;
        switch (binding.type)
        {
            case GL_FLOAT:
                expectedComponents = 1;
                break;
            case GL_FLOAT_VEC2:
                expectedComponents = 2;
                break;
            case GL_FLOAT_VEC3:
                expectedComponents = 3;
                break;
            case GL_FLOAT_VEC4:
                expectedComponents = 4;
                break;
            default:
                context->handleError(
                    InvalidOperation()
                    << "Fragment input type is not a floating point scalar or vector.");
                return false;
        }
        if (expectedComponents != components && genMode != GL_NONE)
        {
            context->handleError(InvalidOperation() << "Unexpected number of components");
            return false;
        }
    }
    return true;
}

bool ValidateCopyTextureCHROMIUM(Context *context,
                                 GLuint sourceId,
                                 GLint sourceLevel,
                                 GLenum destTarget,
                                 GLuint destId,
                                 GLint destLevel,
                                 GLint internalFormat,
                                 GLenum destType,
                                 GLboolean unpackFlipY,
                                 GLboolean unpackPremultiplyAlpha,
                                 GLboolean unpackUnmultiplyAlpha)
{
    if (!context->getExtensions().copyTexture)
    {
        context->handleError(InvalidOperation()
                             << "GL_CHROMIUM_copy_texture extension not available.");
        return false;
    }

    const Texture *source = context->getTexture(sourceId);
    if (source == nullptr)
    {
        context->handleError(InvalidValue() << "Source texture is not a valid texture object.");
        return false;
    }

    if (!IsValidCopyTextureSourceTarget(context, source->getTarget()))
    {
        context->handleError(InvalidValue() << "Source texture a valid texture type.");
        return false;
    }

    GLenum sourceTarget = source->getTarget();
    ASSERT(sourceTarget != GL_TEXTURE_CUBE_MAP);

    if (!IsValidCopyTextureSourceLevel(context, source->getTarget(), sourceLevel))
    {
        context->handleError(InvalidValue() << "Source texture level is not valid.");
        return false;
    }

    GLsizei sourceWidth  = static_cast<GLsizei>(source->getWidth(sourceTarget, sourceLevel));
    GLsizei sourceHeight = static_cast<GLsizei>(source->getHeight(sourceTarget, sourceLevel));
    if (sourceWidth == 0 || sourceHeight == 0)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), InvalidInternalFormat);
        return false;
    }

    const InternalFormat &sourceFormat = *source->getFormat(sourceTarget, sourceLevel).info;
    if (!IsValidCopyTextureSourceInternalFormatEnum(sourceFormat.internalFormat))
    {
        context->handleError(InvalidOperation() << "Source texture internal format is invalid.");
        return false;
    }

    if (!IsValidCopyTextureDestinationTargetEnum(context, destTarget))
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidTextureTarget);
        return false;
    }

    const Texture *dest = context->getTexture(destId);
    if (dest == nullptr)
    {
        context->handleError(InvalidValue()
                             << "Destination texture is not a valid texture object.");
        return false;
    }

    if (!IsValidCopyTextureDestinationTarget(context, dest->getTarget(), destTarget))
    {
        context->handleError(InvalidValue() << "Destination texture a valid texture type.");
        return false;
    }

    if (!IsValidCopyTextureDestinationLevel(context, destTarget, destLevel, sourceWidth,
                                            sourceHeight))
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), InvalidMipLevel);
        return false;
    }

    if (!IsValidCopyTextureDestinationFormatType(context, internalFormat, destType))
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), MismatchedTypeAndFormat);
        return false;
    }

    if (IsCubeMapTextureTarget(destTarget) && sourceWidth != sourceHeight)
    {
        context->handleError(
            InvalidValue() << "Destination width and height must be equal for cube map textures.");
        return false;
    }

    if (dest->getImmutableFormat())
    {
        context->handleError(InvalidOperation() << "Destination texture is immutable.");
        return false;
    }

    return true;
}

bool ValidateCopySubTextureCHROMIUM(Context *context,
                                    GLuint sourceId,
                                    GLint sourceLevel,
                                    GLenum destTarget,
                                    GLuint destId,
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
        context->handleError(InvalidOperation()
                             << "GL_CHROMIUM_copy_texture extension not available.");
        return false;
    }

    const Texture *source = context->getTexture(sourceId);
    if (source == nullptr)
    {
        context->handleError(InvalidValue() << "Source texture is not a valid texture object.");
        return false;
    }

    if (!IsValidCopyTextureSourceTarget(context, source->getTarget()))
    {
        context->handleError(InvalidValue() << "Source texture a valid texture type.");
        return false;
    }

    GLenum sourceTarget = source->getTarget();
    ASSERT(sourceTarget != GL_TEXTURE_CUBE_MAP);

    if (!IsValidCopyTextureSourceLevel(context, source->getTarget(), sourceLevel))
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), InvalidMipLevel);
        return false;
    }

    if (source->getWidth(sourceTarget, sourceLevel) == 0 ||
        source->getHeight(sourceTarget, sourceLevel) == 0)
    {
        context->handleError(InvalidValue()
                             << "The source level of the source texture must be defined.");
        return false;
    }

    if (x < 0 || y < 0)
    {
        context->handleError(InvalidValue() << "x and y cannot be negative.");
        return false;
    }

    if (width < 0 || height < 0)
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), NegativeSize);
        return false;
    }

    if (static_cast<size_t>(x + width) > source->getWidth(sourceTarget, sourceLevel) ||
        static_cast<size_t>(y + height) > source->getHeight(sourceTarget, sourceLevel))
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), SourceTextureTooSmall);
        return false;
    }

    const Format &sourceFormat = source->getFormat(sourceTarget, sourceLevel);
    if (!IsValidCopySubTextureSourceInternalFormat(sourceFormat.info->internalFormat))
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), InvalidInternalFormat);
        return false;
    }

    if (!IsValidCopyTextureDestinationTargetEnum(context, destTarget))
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidTextureTarget);
        return false;
    }

    const Texture *dest = context->getTexture(destId);
    if (dest == nullptr)
    {
        context->handleError(InvalidValue()
                             << "Destination texture is not a valid texture object.");
        return false;
    }

    if (!IsValidCopyTextureDestinationTarget(context, dest->getTarget(), destTarget))
    {
        context->handleError(InvalidValue() << "Destination texture a valid texture type.");
        return false;
    }

    if (!IsValidCopyTextureDestinationLevel(context, destTarget, destLevel, width, height))
    {
        context->handleError(InvalidValue() << "Destination texture level is not valid.");
        return false;
    }

    if (dest->getWidth(destTarget, destLevel) == 0 || dest->getHeight(destTarget, destLevel) == 0)
    {
        context
            ->handleError(InvalidOperation()
                          << "The destination level of the destination texture must be defined.");
        return false;
    }

    const InternalFormat &destFormat = *dest->getFormat(destTarget, destLevel).info;
    if (!IsValidCopySubTextureDestionationInternalFormat(destFormat.internalFormat))
    {
        context->handleError(InvalidOperation()
                             << "Destination internal format and type combination is not valid.");
        return false;
    }

    if (xoffset < 0 || yoffset < 0)
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), NegativeOffset);
        return false;
    }

    if (static_cast<size_t>(xoffset + width) > dest->getWidth(destTarget, destLevel) ||
        static_cast<size_t>(yoffset + height) > dest->getHeight(destTarget, destLevel))
    {
        context->handleError(InvalidValue() << "Destination texture not large enough to copy to.");
        return false;
    }

    return true;
}

bool ValidateCompressedCopyTextureCHROMIUM(Context *context, GLuint sourceId, GLuint destId)
{
    if (!context->getExtensions().copyCompressedTexture)
    {
        context->handleError(InvalidOperation()
                             << "GL_CHROMIUM_copy_compressed_texture extension not available.");
        return false;
    }

    const gl::Texture *source = context->getTexture(sourceId);
    if (source == nullptr)
    {
        context->handleError(InvalidValue() << "Source texture is not a valid texture object.");
        return false;
    }

    if (source->getTarget() != GL_TEXTURE_2D)
    {
        context->handleError(InvalidValue() << "Source texture must be of type GL_TEXTURE_2D.");
        return false;
    }

    if (source->getWidth(GL_TEXTURE_2D, 0) == 0 || source->getHeight(GL_TEXTURE_2D, 0) == 0)
    {
        context->handleError(InvalidValue() << "Source texture must level 0 defined.");
        return false;
    }

    const gl::Format &sourceFormat = source->getFormat(GL_TEXTURE_2D, 0);
    if (!sourceFormat.info->compressed)
    {
        context->handleError(InvalidOperation()
                             << "Source texture must have a compressed internal format.");
        return false;
    }

    const gl::Texture *dest = context->getTexture(destId);
    if (dest == nullptr)
    {
        context->handleError(InvalidValue()
                             << "Destination texture is not a valid texture object.");
        return false;
    }

    if (dest->getTarget() != GL_TEXTURE_2D)
    {
        context->handleError(InvalidValue()
                             << "Destination texture must be of type GL_TEXTURE_2D.");
        return false;
    }

    if (dest->getImmutableFormat())
    {
        context->handleError(InvalidOperation() << "Destination cannot be immutable.");
        return false;
    }

    return true;
}

bool ValidateCreateShader(Context *context, GLenum type)
{
    switch (type)
    {
        case GL_VERTEX_SHADER:
        case GL_FRAGMENT_SHADER:
            break;

        case GL_COMPUTE_SHADER:
            if (context->getClientVersion() < Version(3, 1))
            {
                ANGLE_VALIDATION_ERR(context, InvalidEnum(), ES31Required);
                return false;
            }
            break;

        case GL_GEOMETRY_SHADER_EXT:
            if (!context->getExtensions().geometryShader)
            {
                ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidShaderType);
                return false;
            }
            break;
        default:
            ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidShaderType);
            return false;
    }

    return true;
}

bool ValidateBufferData(ValidationContext *context,
                        BufferBinding target,
                        GLsizeiptr size,
                        const void *data,
                        BufferUsage usage)
{
    if (size < 0)
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), NegativeSize);
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
                ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidBufferUsage);
                return false;
            }
            break;

        default:
            ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidBufferUsage);
            return false;
    }

    if (!ValidBufferType(context, target))
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidBufferTypes);
        return false;
    }

    Buffer *buffer = context->getGLState().getTargetBuffer(target);

    if (!buffer)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), BufferNotBound);
        return false;
    }

    return true;
}

bool ValidateBufferSubData(ValidationContext *context,
                           BufferBinding target,
                           GLintptr offset,
                           GLsizeiptr size,
                           const void *data)
{
    if (size < 0)
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), NegativeSize);
        return false;
    }

    if (offset < 0)
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), NegativeOffset);
        return false;
    }

    if (!ValidBufferType(context, target))
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidBufferTypes);
        return false;
    }

    Buffer *buffer = context->getGLState().getTargetBuffer(target);

    if (!buffer)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), BufferNotBound);
        return false;
    }

    if (buffer->isMapped())
    {
        context->handleError(InvalidOperation());
        return false;
    }

    // Check for possible overflow of size + offset
    angle::CheckedNumeric<size_t> checkedSize(size);
    checkedSize += offset;
    if (!checkedSize.IsValid())
    {
        context->handleError(OutOfMemory());
        return false;
    }

    if (size + offset > buffer->getSize())
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), InsufficientBufferSize);
        return false;
    }

    return true;
}

bool ValidateRequestExtensionANGLE(Context *context, const GLchar *name)
{
    if (!context->getExtensions().requestExtension)
    {
        context->handleError(InvalidOperation() << "GL_ANGLE_request_extension is not available.");
        return false;
    }

    if (!context->isExtensionRequestable(name))
    {
        context->handleError(InvalidOperation() << "Extension " << name << " is not requestable.");
        return false;
    }

    return true;
}

bool ValidateActiveTexture(ValidationContext *context, GLenum texture)
{
    if (texture < GL_TEXTURE0 ||
        texture > GL_TEXTURE0 + context->getCaps().maxCombinedTextureImageUnits - 1)
    {
        context->handleError(InvalidEnum());
        return false;
    }

    return true;
}

bool ValidateAttachShader(ValidationContext *context, GLuint program, GLuint shader)
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

    switch (shaderObject->getType())
    {
        case GL_VERTEX_SHADER:
        {
            if (programObject->getAttachedVertexShader())
            {
                ANGLE_VALIDATION_ERR(context, InvalidOperation(), ShaderAttachmentHasShader);
                return false;
            }
            break;
        }
        case GL_FRAGMENT_SHADER:
        {
            if (programObject->getAttachedFragmentShader())
            {
                ANGLE_VALIDATION_ERR(context, InvalidOperation(), ShaderAttachmentHasShader);
                return false;
            }
            break;
        }
        case GL_COMPUTE_SHADER:
        {
            if (programObject->getAttachedComputeShader())
            {
                ANGLE_VALIDATION_ERR(context, InvalidOperation(), ShaderAttachmentHasShader);
                return false;
            }
            break;
        }
        case GL_GEOMETRY_SHADER_EXT:
        {
            if (programObject->getAttachedGeometryShader())
            {
                ANGLE_VALIDATION_ERR(context, InvalidOperation(), ShaderAttachmentHasShader);
                return false;
            }
            break;
        }
        default:
            UNREACHABLE();
            break;
    }

    return true;
}

bool ValidateBindAttribLocation(ValidationContext *context,
                                GLuint program,
                                GLuint index,
                                const GLchar *name)
{
    if (index >= MAX_VERTEX_ATTRIBS)
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), IndexExceedsMaxVertexAttribute);
        return false;
    }

    if (strncmp(name, "gl_", 3) == 0)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), NameBeginsWithGL);
        return false;
    }

    if (context->isWebGL())
    {
        const size_t length = strlen(name);

        if (!IsValidESSLString(name, length))
        {
            // The WebGL spec (section 6.20) disallows strings containing invalid ESSL characters
            // for shader-related entry points
            ANGLE_VALIDATION_ERR(context, InvalidValue(), InvalidNameCharacters);
            return false;
        }

        if (!ValidateWebGLNameLength(context, length) || !ValidateWebGLNamePrefix(context, name))
        {
            return false;
        }
    }

    return GetValidProgram(context, program) != nullptr;
}

bool ValidateBindBuffer(ValidationContext *context, BufferBinding target, GLuint buffer)
{
    if (!ValidBufferType(context, target))
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidBufferTypes);
        return false;
    }

    if (!context->getGLState().isBindGeneratesResourceEnabled() &&
        !context->isBufferGenerated(buffer))
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ObjectNotGenerated);
        return false;
    }

    return true;
}

bool ValidateBindFramebuffer(ValidationContext *context, GLenum target, GLuint framebuffer)
{
    if (!ValidFramebufferTarget(context, target))
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidFramebufferTarget);
        return false;
    }

    if (!context->getGLState().isBindGeneratesResourceEnabled() &&
        !context->isFramebufferGenerated(framebuffer))
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ObjectNotGenerated);
        return false;
    }

    return true;
}

bool ValidateBindRenderbuffer(ValidationContext *context, GLenum target, GLuint renderbuffer)
{
    if (target != GL_RENDERBUFFER)
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidRenderbufferTarget);
        return false;
    }

    if (!context->getGLState().isBindGeneratesResourceEnabled() &&
        !context->isRenderbufferGenerated(renderbuffer))
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ObjectNotGenerated);
        return false;
    }

    return true;
}

static bool ValidBlendEquationMode(const ValidationContext *context, GLenum mode)
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

bool ValidateBlendColor(ValidationContext *context,
                        GLfloat red,
                        GLfloat green,
                        GLfloat blue,
                        GLfloat alpha)
{
    return true;
}

bool ValidateBlendEquation(ValidationContext *context, GLenum mode)
{
    if (!ValidBlendEquationMode(context, mode))
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidBlendEquation);
        return false;
    }

    return true;
}

bool ValidateBlendEquationSeparate(ValidationContext *context, GLenum modeRGB, GLenum modeAlpha)
{
    if (!ValidBlendEquationMode(context, modeRGB))
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidBlendEquation);
        return false;
    }

    if (!ValidBlendEquationMode(context, modeAlpha))
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidBlendEquation);
        return false;
    }

    return true;
}

bool ValidateBlendFunc(ValidationContext *context, GLenum sfactor, GLenum dfactor)
{
    return ValidateBlendFuncSeparate(context, sfactor, dfactor, sfactor, dfactor);
}

static bool ValidSrcBlendFunc(GLenum srcBlend)
{
    switch (srcBlend)
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
        case GL_SRC_ALPHA_SATURATE:
            return true;

        default:
            return false;
    }
}

static bool ValidDstBlendFunc(GLenum dstBlend, GLint contextMajorVersion)
{
    switch (dstBlend)
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

        case GL_SRC_ALPHA_SATURATE:
            return (contextMajorVersion >= 3);

        default:
            return false;
    }
}

bool ValidateBlendFuncSeparate(ValidationContext *context,
                               GLenum srcRGB,
                               GLenum dstRGB,
                               GLenum srcAlpha,
                               GLenum dstAlpha)
{
    if (!ValidSrcBlendFunc(srcRGB))
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidBlendFunction);
        return false;
    }

    if (!ValidDstBlendFunc(dstRGB, context->getClientMajorVersion()))
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidBlendFunction);
        return false;
    }

    if (!ValidSrcBlendFunc(srcAlpha))
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidBlendFunction);
        return false;
    }

    if (!ValidDstBlendFunc(dstAlpha, context->getClientMajorVersion()))
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidBlendFunction);
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
            const char *msg;
            if (context->getExtensions().webglCompatibility)
            {
                msg = kErrorInvalidConstantColor;
            }
            else
            {
                msg =
                    "Simultaneous use of GL_CONSTANT_ALPHA/GL_ONE_MINUS_CONSTANT_ALPHA and "
                    "GL_CONSTANT_COLOR/GL_ONE_MINUS_CONSTANT_COLOR not supported by this "
                    "implementation.";
                ERR() << msg;
            }
            context->handleError(InvalidOperation() << msg);
            return false;
        }
    }

    return true;
}

bool ValidateGetString(Context *context, GLenum name)
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
                ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidName);
                return false;
            }
            break;

        default:
            ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidName);
            return false;
    }

    return true;
}

bool ValidateLineWidth(ValidationContext *context, GLfloat width)
{
    if (width <= 0.0f || isNaN(width))
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), InvalidWidth);
        return false;
    }

    return true;
}

bool ValidateVertexAttribPointer(ValidationContext *context,
                                 GLuint index,
                                 GLint size,
                                 GLenum type,
                                 GLboolean normalized,
                                 GLsizei stride,
                                 const void *ptr)
{
    if (!ValidateVertexFormatBase(context, index, size, type, false))
    {
        return false;
    }

    if (stride < 0)
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), NegativeStride);
        return false;
    }

    const Caps &caps = context->getCaps();
    if (context->getClientVersion() >= ES_3_1)
    {
        if (stride > caps.maxVertexAttribStride)
        {
            context->handleError(InvalidValue()
                                 << "stride cannot be greater than MAX_VERTEX_ATTRIB_STRIDE.");
            return false;
        }

        if (index >= caps.maxVertexAttribBindings)
        {
            context->handleError(InvalidValue()
                                 << "index must be smaller than MAX_VERTEX_ATTRIB_BINDINGS.");
            return false;
        }
    }

    // [OpenGL ES 3.0.2] Section 2.8 page 24:
    // An INVALID_OPERATION error is generated when a non-zero vertex array object
    // is bound, zero is bound to the ARRAY_BUFFER buffer object binding point,
    // and the pointer argument is not NULL.
    bool nullBufferAllowed = context->getGLState().areClientArraysEnabled() &&
                             context->getGLState().getVertexArray()->id() == 0;
    if (!nullBufferAllowed && context->getGLState().getTargetBuffer(BufferBinding::Array) == 0 &&
        ptr != nullptr)
    {
        context
            ->handleError(InvalidOperation()
                          << "Client data cannot be used with a non-default vertex array object.");
        return false;
    }

    if (context->getExtensions().webglCompatibility)
    {
        // WebGL 1.0 [Section 6.14] Fixed point support
        // The WebGL API does not support the GL_FIXED data type.
        if (type == GL_FIXED)
        {
            context->handleError(InvalidEnum() << "GL_FIXED is not supported in WebGL.");
            return false;
        }

        if (!ValidateWebGLVertexAttribPointer(context, type, normalized, stride, ptr, false))
        {
            return false;
        }
    }

    return true;
}

bool ValidateDepthRangef(ValidationContext *context, GLfloat zNear, GLfloat zFar)
{
    if (context->getExtensions().webglCompatibility && zNear > zFar)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), InvalidDepthRange);
        return false;
    }

    return true;
}

bool ValidateRenderbufferStorage(ValidationContext *context,
                                 GLenum target,
                                 GLenum internalformat,
                                 GLsizei width,
                                 GLsizei height)
{
    return ValidateRenderbufferStorageParametersBase(context, target, 0, internalformat, width,
                                                     height);
}

bool ValidateRenderbufferStorageMultisampleANGLE(ValidationContext *context,
                                                 GLenum target,
                                                 GLsizei samples,
                                                 GLenum internalformat,
                                                 GLsizei width,
                                                 GLsizei height)
{
    if (!context->getExtensions().framebufferMultisample)
    {
        context->handleError(InvalidOperation()
                             << "GL_ANGLE_framebuffer_multisample not available");
        return false;
    }

    // ANGLE_framebuffer_multisample states that the value of samples must be less than or equal
    // to MAX_SAMPLES_ANGLE (Context::getCaps().maxSamples) otherwise GL_INVALID_OPERATION is
    // generated.
    if (static_cast<GLuint>(samples) > context->getCaps().maxSamples)
    {
        context->handleError(InvalidValue());
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
            context->handleError(OutOfMemory());
            return false;
        }
    }

    return ValidateRenderbufferStorageParametersBase(context, target, samples, internalformat,
                                                     width, height);
}

bool ValidateCheckFramebufferStatus(ValidationContext *context, GLenum target)
{
    if (!ValidFramebufferTarget(context, target))
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidFramebufferTarget);
        return false;
    }

    return true;
}

bool ValidateClearColor(ValidationContext *context,
                        GLfloat red,
                        GLfloat green,
                        GLfloat blue,
                        GLfloat alpha)
{
    return true;
}

bool ValidateClearDepthf(ValidationContext *context, GLfloat depth)
{
    return true;
}

bool ValidateClearStencil(ValidationContext *context, GLint s)
{
    return true;
}

bool ValidateColorMask(ValidationContext *context,
                       GLboolean red,
                       GLboolean green,
                       GLboolean blue,
                       GLboolean alpha)
{
    return true;
}

bool ValidateCompileShader(ValidationContext *context, GLuint shader)
{
    return true;
}

bool ValidateCreateProgram(ValidationContext *context)
{
    return true;
}

bool ValidateCullFace(ValidationContext *context, CullFaceMode mode)
{
    switch (mode)
    {
        case CullFaceMode::Front:
        case CullFaceMode::Back:
        case CullFaceMode::FrontAndBack:
            break;

        default:
            ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidCullMode);
            return false;
    }

    return true;
}

bool ValidateDeleteProgram(ValidationContext *context, GLuint program)
{
    if (program == 0)
    {
        return false;
    }

    if (!context->getProgram(program))
    {
        if (context->getShader(program))
        {
            ANGLE_VALIDATION_ERR(context, InvalidOperation(), ExpectedProgramName);
            return false;
        }
        else
        {
            ANGLE_VALIDATION_ERR(context, InvalidValue(), InvalidProgramName);
            return false;
        }
    }

    return true;
}

bool ValidateDeleteShader(ValidationContext *context, GLuint shader)
{
    if (shader == 0)
    {
        return false;
    }

    if (!context->getShader(shader))
    {
        if (context->getProgram(shader))
        {
            ANGLE_VALIDATION_ERR(context, InvalidOperation(), InvalidShaderName);
            return false;
        }
        else
        {
            ANGLE_VALIDATION_ERR(context, InvalidValue(), ExpectedShaderName);
            return false;
        }
    }

    return true;
}

bool ValidateDepthFunc(ValidationContext *context, GLenum func)
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
            ANGLE_VALIDATION_ERR(context, InvalidEnum(), EnumNotSupported);
            return false;
    }

    return true;
}

bool ValidateDepthMask(ValidationContext *context, GLboolean flag)
{
    return true;
}

bool ValidateDetachShader(ValidationContext *context, GLuint program, GLuint shader)
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

    const Shader *attachedShader = nullptr;

    switch (shaderObject->getType())
    {
        case GL_VERTEX_SHADER:
        {
            attachedShader = programObject->getAttachedVertexShader();
            break;
        }
        case GL_FRAGMENT_SHADER:
        {
            attachedShader = programObject->getAttachedFragmentShader();
            break;
        }
        case GL_COMPUTE_SHADER:
        {
            attachedShader = programObject->getAttachedComputeShader();
            break;
        }
        case GL_GEOMETRY_SHADER_EXT:
        {
            attachedShader = programObject->getAttachedGeometryShader();
            break;
        }
        default:
            UNREACHABLE();
            return false;
    }

    if (attachedShader != shaderObject)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ShaderToDetachMustBeAttached);
        return false;
    }

    return true;
}

bool ValidateDisableVertexAttribArray(ValidationContext *context, GLuint index)
{
    if (index >= MAX_VERTEX_ATTRIBS)
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), IndexExceedsMaxVertexAttribute);
        return false;
    }

    return true;
}

bool ValidateEnableVertexAttribArray(ValidationContext *context, GLuint index)
{
    if (index >= MAX_VERTEX_ATTRIBS)
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), IndexExceedsMaxVertexAttribute);
        return false;
    }

    return true;
}

bool ValidateFinish(ValidationContext *context)
{
    return true;
}

bool ValidateFlush(ValidationContext *context)
{
    return true;
}

bool ValidateFrontFace(ValidationContext *context, GLenum mode)
{
    switch (mode)
    {
        case GL_CW:
        case GL_CCW:
            break;
        default:
            ANGLE_VALIDATION_ERR(context, InvalidEnum(), EnumNotSupported);
            return false;
    }

    return true;
}

bool ValidateGetActiveAttrib(ValidationContext *context,
                             GLuint program,
                             GLuint index,
                             GLsizei bufsize,
                             GLsizei *length,
                             GLint *size,
                             GLenum *type,
                             GLchar *name)
{
    if (bufsize < 0)
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), NegativeBufferSize);
        return false;
    }

    Program *programObject = GetValidProgram(context, program);

    if (!programObject)
    {
        return false;
    }

    if (index >= static_cast<GLuint>(programObject->getActiveAttributeCount()))
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), IndexExceedsMaxActiveUniform);
        return false;
    }

    return true;
}

bool ValidateGetActiveUniform(ValidationContext *context,
                              GLuint program,
                              GLuint index,
                              GLsizei bufsize,
                              GLsizei *length,
                              GLint *size,
                              GLenum *type,
                              GLchar *name)
{
    if (bufsize < 0)
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), NegativeBufferSize);
        return false;
    }

    Program *programObject = GetValidProgram(context, program);

    if (!programObject)
    {
        return false;
    }

    if (index >= static_cast<GLuint>(programObject->getActiveUniformCount()))
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), IndexExceedsMaxActiveUniform);
        return false;
    }

    return true;
}

bool ValidateGetAttachedShaders(ValidationContext *context,
                                GLuint program,
                                GLsizei maxcount,
                                GLsizei *count,
                                GLuint *shaders)
{
    if (maxcount < 0)
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), NegativeMaxCount);
        return false;
    }

    Program *programObject = GetValidProgram(context, program);

    if (!programObject)
    {
        return false;
    }

    return true;
}

bool ValidateGetAttribLocation(ValidationContext *context, GLuint program, const GLchar *name)
{
    // The WebGL spec (section 6.20) disallows strings containing invalid ESSL characters for
    // shader-related entry points
    if (context->getExtensions().webglCompatibility && !IsValidESSLString(name, strlen(name)))
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), InvalidNameCharacters);
        return false;
    }

    Program *programObject = GetValidProgram(context, program);

    if (!programObject)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ProgramNotBound);
        return false;
    }

    if (!programObject->isLinked())
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ProgramNotLinked);
        return false;
    }

    return true;
}

bool ValidateGetBooleanv(ValidationContext *context, GLenum pname, GLboolean *params)
{
    GLenum nativeType;
    unsigned int numParams = 0;
    return ValidateStateQuery(context, pname, &nativeType, &numParams);
}

bool ValidateGetError(ValidationContext *context)
{
    return true;
}

bool ValidateGetFloatv(ValidationContext *context, GLenum pname, GLfloat *params)
{
    GLenum nativeType;
    unsigned int numParams = 0;
    return ValidateStateQuery(context, pname, &nativeType, &numParams);
}

bool ValidateGetIntegerv(ValidationContext *context, GLenum pname, GLint *params)
{
    GLenum nativeType;
    unsigned int numParams = 0;
    return ValidateStateQuery(context, pname, &nativeType, &numParams);
}

bool ValidateGetProgramInfoLog(ValidationContext *context,
                               GLuint program,
                               GLsizei bufsize,
                               GLsizei *length,
                               GLchar *infolog)
{
    if (bufsize < 0)
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), NegativeBufferSize);
        return false;
    }

    Program *programObject = GetValidProgram(context, program);
    if (!programObject)
    {
        return false;
    }

    return true;
}

bool ValidateGetShaderInfoLog(ValidationContext *context,
                              GLuint shader,
                              GLsizei bufsize,
                              GLsizei *length,
                              GLchar *infolog)
{
    if (bufsize < 0)
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), NegativeBufferSize);
        return false;
    }

    Shader *shaderObject = GetValidShader(context, shader);
    if (!shaderObject)
    {
        return false;
    }

    return true;
}

bool ValidateGetShaderPrecisionFormat(ValidationContext *context,
                                      GLenum shadertype,
                                      GLenum precisiontype,
                                      GLint *range,
                                      GLint *precision)
{
    switch (shadertype)
    {
        case GL_VERTEX_SHADER:
        case GL_FRAGMENT_SHADER:
            break;
        case GL_COMPUTE_SHADER:
            context->handleError(InvalidOperation()
                                 << "compute shader precision not yet implemented.");
            return false;
        default:
            ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidShaderType);
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
            ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidPrecision);
            return false;
    }

    return true;
}

bool ValidateGetShaderSource(ValidationContext *context,
                             GLuint shader,
                             GLsizei bufsize,
                             GLsizei *length,
                             GLchar *source)
{
    if (bufsize < 0)
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), NegativeBufferSize);
        return false;
    }

    Shader *shaderObject = GetValidShader(context, shader);
    if (!shaderObject)
    {
        return false;
    }

    return true;
}

bool ValidateGetUniformLocation(ValidationContext *context, GLuint program, const GLchar *name)
{
    if (strstr(name, "gl_") == name)
    {
        return false;
    }

    // The WebGL spec (section 6.20) disallows strings containing invalid ESSL characters for
    // shader-related entry points
    if (context->getExtensions().webglCompatibility && !IsValidESSLString(name, strlen(name)))
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), InvalidNameCharacters);
        return false;
    }

    Program *programObject = GetValidProgram(context, program);

    if (!programObject)
    {
        return false;
    }

    if (!programObject->isLinked())
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ProgramNotLinked);
        return false;
    }

    return true;
}

bool ValidateHint(ValidationContext *context, GLenum target, GLenum mode)
{
    switch (mode)
    {
        case GL_FASTEST:
        case GL_NICEST:
        case GL_DONT_CARE:
            break;

        default:
            ANGLE_VALIDATION_ERR(context, InvalidEnum(), EnumNotSupported);
            return false;
    }

    switch (target)
    {
        case GL_GENERATE_MIPMAP_HINT:
            break;

        case GL_FRAGMENT_SHADER_DERIVATIVE_HINT:
            if (context->getClientVersion() < ES_3_0 &&
                !context->getExtensions().standardDerivatives)
            {
                context->handleError(InvalidEnum() << "hint requires OES_standard_derivatives.");
                return false;
            }
            break;

        default:
            ANGLE_VALIDATION_ERR(context, InvalidEnum(), EnumNotSupported);
            return false;
    }

    return true;
}

bool ValidateIsBuffer(ValidationContext *context, GLuint buffer)
{
    return true;
}

bool ValidateIsFramebuffer(ValidationContext *context, GLuint framebuffer)
{
    return true;
}

bool ValidateIsProgram(ValidationContext *context, GLuint program)
{
    return true;
}

bool ValidateIsRenderbuffer(ValidationContext *context, GLuint renderbuffer)
{
    return true;
}

bool ValidateIsShader(ValidationContext *context, GLuint shader)
{
    return true;
}

bool ValidateIsTexture(ValidationContext *context, GLuint texture)
{
    return true;
}

bool ValidatePixelStorei(ValidationContext *context, GLenum pname, GLint param)
{
    if (context->getClientMajorVersion() < 3)
    {
        switch (pname)
        {
            case GL_UNPACK_IMAGE_HEIGHT:
            case GL_UNPACK_SKIP_IMAGES:
                context->handleError(InvalidEnum());
                return false;

            case GL_UNPACK_ROW_LENGTH:
            case GL_UNPACK_SKIP_ROWS:
            case GL_UNPACK_SKIP_PIXELS:
                if (!context->getExtensions().unpackSubimage)
                {
                    context->handleError(InvalidEnum());
                    return false;
                }
                break;

            case GL_PACK_ROW_LENGTH:
            case GL_PACK_SKIP_ROWS:
            case GL_PACK_SKIP_PIXELS:
                if (!context->getExtensions().packSubimage)
                {
                    context->handleError(InvalidEnum());
                    return false;
                }
                break;
        }
    }

    if (param < 0)
    {
        context->handleError(InvalidValue() << "Cannot use negative values in PixelStorei");
        return false;
    }

    switch (pname)
    {
        case GL_UNPACK_ALIGNMENT:
            if (param != 1 && param != 2 && param != 4 && param != 8)
            {
                ANGLE_VALIDATION_ERR(context, InvalidValue(), InvalidUnpackAlignment);
                return false;
            }
            break;

        case GL_PACK_ALIGNMENT:
            if (param != 1 && param != 2 && param != 4 && param != 8)
            {
                ANGLE_VALIDATION_ERR(context, InvalidValue(), InvalidUnpackAlignment);
                return false;
            }
            break;

        case GL_PACK_REVERSE_ROW_ORDER_ANGLE:
            if (!context->getExtensions().packReverseRowOrder)
            {
                ANGLE_VALIDATION_ERR(context, InvalidEnum(), EnumNotSupported);
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
            ANGLE_VALIDATION_ERR(context, InvalidEnum(), EnumNotSupported);
            return false;
    }

    return true;
}

bool ValidatePolygonOffset(ValidationContext *context, GLfloat factor, GLfloat units)
{
    return true;
}

bool ValidateReleaseShaderCompiler(ValidationContext *context)
{
    return true;
}

bool ValidateSampleCoverage(ValidationContext *context, GLfloat value, GLboolean invert)
{
    return true;
}

bool ValidateScissor(ValidationContext *context, GLint x, GLint y, GLsizei width, GLsizei height)
{
    if (width < 0 || height < 0)
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), NegativeSize);
        return false;
    }

    return true;
}

bool ValidateShaderBinary(ValidationContext *context,
                          GLsizei n,
                          const GLuint *shaders,
                          GLenum binaryformat,
                          const void *binary,
                          GLsizei length)
{
    const std::vector<GLenum> &shaderBinaryFormats = context->getCaps().shaderBinaryFormats;
    if (std::find(shaderBinaryFormats.begin(), shaderBinaryFormats.end(), binaryformat) ==
        shaderBinaryFormats.end())
    {
        context->handleError(InvalidEnum() << "Invalid shader binary format.");
        return false;
    }

    return true;
}

bool ValidateShaderSource(ValidationContext *context,
                          GLuint shader,
                          GLsizei count,
                          const GLchar *const *string,
                          const GLint *length)
{
    if (count < 0)
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), NegativeCount);
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
                ANGLE_VALIDATION_ERR(context, InvalidValue(), ShaderSourceInvalidCharacters);
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

bool ValidateStencilFunc(ValidationContext *context, GLenum func, GLint ref, GLuint mask)
{
    if (!IsValidStencilFunc(func))
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidStencil);
        return false;
    }

    return true;
}

bool ValidateStencilFuncSeparate(ValidationContext *context,
                                 GLenum face,
                                 GLenum func,
                                 GLint ref,
                                 GLuint mask)
{
    if (!IsValidStencilFace(face))
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidStencil);
        return false;
    }

    if (!IsValidStencilFunc(func))
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidStencil);
        return false;
    }

    return true;
}

bool ValidateStencilMask(ValidationContext *context, GLuint mask)
{
    return true;
}

bool ValidateStencilMaskSeparate(ValidationContext *context, GLenum face, GLuint mask)
{
    if (!IsValidStencilFace(face))
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidStencil);
        return false;
    }

    return true;
}

bool ValidateStencilOp(ValidationContext *context, GLenum fail, GLenum zfail, GLenum zpass)
{
    if (!IsValidStencilOp(fail))
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidStencil);
        return false;
    }

    if (!IsValidStencilOp(zfail))
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidStencil);
        return false;
    }

    if (!IsValidStencilOp(zpass))
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidStencil);
        return false;
    }

    return true;
}

bool ValidateStencilOpSeparate(ValidationContext *context,
                               GLenum face,
                               GLenum fail,
                               GLenum zfail,
                               GLenum zpass)
{
    if (!IsValidStencilFace(face))
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidStencil);
        return false;
    }

    return ValidateStencilOp(context, fail, zfail, zpass);
}

bool ValidateUniform1f(ValidationContext *context, GLint location, GLfloat x)
{
    return ValidateUniform(context, GL_FLOAT, location, 1);
}

bool ValidateUniform1fv(ValidationContext *context, GLint location, GLsizei count, const GLfloat *v)
{
    return ValidateUniform(context, GL_FLOAT, location, count);
}

bool ValidateUniform1i(ValidationContext *context, GLint location, GLint x)
{
    return ValidateUniform1iv(context, location, 1, &x);
}

bool ValidateUniform2f(ValidationContext *context, GLint location, GLfloat x, GLfloat y)
{
    return ValidateUniform(context, GL_FLOAT_VEC2, location, 1);
}

bool ValidateUniform2fv(ValidationContext *context, GLint location, GLsizei count, const GLfloat *v)
{
    return ValidateUniform(context, GL_FLOAT_VEC2, location, count);
}

bool ValidateUniform2i(ValidationContext *context, GLint location, GLint x, GLint y)
{
    return ValidateUniform(context, GL_INT_VEC2, location, 1);
}

bool ValidateUniform2iv(ValidationContext *context, GLint location, GLsizei count, const GLint *v)
{
    return ValidateUniform(context, GL_INT_VEC2, location, count);
}

bool ValidateUniform3f(ValidationContext *context, GLint location, GLfloat x, GLfloat y, GLfloat z)
{
    return ValidateUniform(context, GL_FLOAT_VEC3, location, 1);
}

bool ValidateUniform3fv(ValidationContext *context, GLint location, GLsizei count, const GLfloat *v)
{
    return ValidateUniform(context, GL_FLOAT_VEC3, location, count);
}

bool ValidateUniform3i(ValidationContext *context, GLint location, GLint x, GLint y, GLint z)
{
    return ValidateUniform(context, GL_INT_VEC3, location, 1);
}

bool ValidateUniform3iv(ValidationContext *context, GLint location, GLsizei count, const GLint *v)
{
    return ValidateUniform(context, GL_INT_VEC3, location, count);
}

bool ValidateUniform4f(ValidationContext *context,
                       GLint location,
                       GLfloat x,
                       GLfloat y,
                       GLfloat z,
                       GLfloat w)
{
    return ValidateUniform(context, GL_FLOAT_VEC4, location, 1);
}

bool ValidateUniform4fv(ValidationContext *context, GLint location, GLsizei count, const GLfloat *v)
{
    return ValidateUniform(context, GL_FLOAT_VEC4, location, count);
}

bool ValidateUniform4i(ValidationContext *context,
                       GLint location,
                       GLint x,
                       GLint y,
                       GLint z,
                       GLint w)
{
    return ValidateUniform(context, GL_INT_VEC4, location, 1);
}

bool ValidateUniform4iv(ValidationContext *context, GLint location, GLsizei count, const GLint *v)
{
    return ValidateUniform(context, GL_INT_VEC4, location, count);
}

bool ValidateUniformMatrix2fv(ValidationContext *context,
                              GLint location,
                              GLsizei count,
                              GLboolean transpose,
                              const GLfloat *value)
{
    return ValidateUniformMatrix(context, GL_FLOAT_MAT2, location, count, transpose);
}

bool ValidateUniformMatrix3fv(ValidationContext *context,
                              GLint location,
                              GLsizei count,
                              GLboolean transpose,
                              const GLfloat *value)
{
    return ValidateUniformMatrix(context, GL_FLOAT_MAT3, location, count, transpose);
}

bool ValidateUniformMatrix4fv(ValidationContext *context,
                              GLint location,
                              GLsizei count,
                              GLboolean transpose,
                              const GLfloat *value)
{
    return ValidateUniformMatrix(context, GL_FLOAT_MAT4, location, count, transpose);
}

bool ValidateValidateProgram(ValidationContext *context, GLuint program)
{
    Program *programObject = GetValidProgram(context, program);

    if (!programObject)
    {
        return false;
    }

    return true;
}

bool ValidateVertexAttrib1f(ValidationContext *context, GLuint index, GLfloat x)
{
    return ValidateVertexAttribIndex(context, index);
}

bool ValidateVertexAttrib1fv(ValidationContext *context, GLuint index, const GLfloat *values)
{
    return ValidateVertexAttribIndex(context, index);
}

bool ValidateVertexAttrib2f(ValidationContext *context, GLuint index, GLfloat x, GLfloat y)
{
    return ValidateVertexAttribIndex(context, index);
}

bool ValidateVertexAttrib2fv(ValidationContext *context, GLuint index, const GLfloat *values)
{
    return ValidateVertexAttribIndex(context, index);
}

bool ValidateVertexAttrib3f(ValidationContext *context,
                            GLuint index,
                            GLfloat x,
                            GLfloat y,
                            GLfloat z)
{
    return ValidateVertexAttribIndex(context, index);
}

bool ValidateVertexAttrib3fv(ValidationContext *context, GLuint index, const GLfloat *values)
{
    return ValidateVertexAttribIndex(context, index);
}

bool ValidateVertexAttrib4f(ValidationContext *context,
                            GLuint index,
                            GLfloat x,
                            GLfloat y,
                            GLfloat z,
                            GLfloat w)
{
    return ValidateVertexAttribIndex(context, index);
}

bool ValidateVertexAttrib4fv(ValidationContext *context, GLuint index, const GLfloat *values)
{
    return ValidateVertexAttribIndex(context, index);
}

bool ValidateViewport(ValidationContext *context, GLint x, GLint y, GLsizei width, GLsizei height)
{
    if (width < 0 || height < 0)
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), ViewportNegativeSize);
        return false;
    }

    return true;
}

bool ValidateDrawArrays(ValidationContext *context, GLenum mode, GLint first, GLsizei count)
{
    return ValidateDrawArraysCommon(context, mode, first, count, 1);
}

bool ValidateDrawElements(ValidationContext *context,
                          GLenum mode,
                          GLsizei count,
                          GLenum type,
                          const void *indices)
{
    return ValidateDrawElementsCommon(context, mode, count, type, indices, 1);
}

bool ValidateGetFramebufferAttachmentParameteriv(ValidationContext *context,
                                                 GLenum target,
                                                 GLenum attachment,
                                                 GLenum pname,
                                                 GLint *params)
{
    return ValidateGetFramebufferAttachmentParameterivBase(context, target, attachment, pname,
                                                           nullptr);
}

bool ValidateGetProgramiv(ValidationContext *context, GLuint program, GLenum pname, GLint *params)
{
    return ValidateGetProgramivBase(context, program, pname, nullptr);
}

bool ValidateCopyTexImage2D(ValidationContext *context,
                            GLenum target,
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

bool ValidateCopyTexSubImage2D(Context *context,
                               GLenum target,
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

bool ValidateDeleteBuffers(Context *context, GLint n, const GLuint *)
{
    return ValidateGenOrDelete(context, n);
}

bool ValidateDeleteFramebuffers(Context *context, GLint n, const GLuint *)
{
    return ValidateGenOrDelete(context, n);
}

bool ValidateDeleteRenderbuffers(Context *context, GLint n, const GLuint *)
{
    return ValidateGenOrDelete(context, n);
}

bool ValidateDeleteTextures(Context *context, GLint n, const GLuint *)
{
    return ValidateGenOrDelete(context, n);
}

bool ValidateDisable(Context *context, GLenum cap)
{
    if (!ValidCap(context, cap, false))
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), EnumNotSupported);
        return false;
    }

    return true;
}

bool ValidateEnable(Context *context, GLenum cap)
{
    if (!ValidCap(context, cap, false))
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), EnumNotSupported);
        return false;
    }

    if (context->getLimitations().noSampleAlphaToCoverageSupport &&
        cap == GL_SAMPLE_ALPHA_TO_COVERAGE)
    {
        const char *errorMessage = "Current renderer doesn't support alpha-to-coverage";
        context->handleError(InvalidOperation() << errorMessage);

        // We also output an error message to the debugger window if tracing is active, so that
        // developers can see the error message.
        ERR() << errorMessage;
        return false;
    }

    return true;
}

bool ValidateFramebufferRenderbuffer(Context *context,
                                     GLenum target,
                                     GLenum attachment,
                                     GLenum renderbuffertarget,
                                     GLuint renderbuffer)
{
    if (!ValidFramebufferTarget(context, target))
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidFramebufferTarget);
        return false;
    }

    if (renderbuffertarget != GL_RENDERBUFFER && renderbuffer != 0)
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidRenderbufferTarget);
        return false;
    }

    return ValidateFramebufferRenderbufferParameters(context, target, attachment,
                                                     renderbuffertarget, renderbuffer);
}

bool ValidateFramebufferTexture2D(Context *context,
                                  GLenum target,
                                  GLenum attachment,
                                  GLenum textarget,
                                  GLuint texture,
                                  GLint level)
{
    // Attachments are required to be bound to level 0 without ES3 or the GL_OES_fbo_render_mipmap
    // extension
    if (context->getClientMajorVersion() < 3 && !context->getExtensions().fboRenderMipmap &&
        level != 0)
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), InvalidFramebufferTextureLevel);
        return false;
    }

    if (!ValidateFramebufferTextureBase(context, target, attachment, texture, level))
    {
        return false;
    }

    if (texture != 0)
    {
        gl::Texture *tex = context->getTexture(texture);
        ASSERT(tex);

        const gl::Caps &caps = context->getCaps();

        switch (textarget)
        {
            case GL_TEXTURE_2D:
            {
                if (level > gl::log2(caps.max2DTextureSize))
                {
                    context->handleError(InvalidValue());
                    return false;
                }
                if (tex->getTarget() != GL_TEXTURE_2D)
                {
                    ANGLE_VALIDATION_ERR(context, InvalidOperation(), InvalidTextureTarget);
                    return false;
                }
            }
            break;

            case GL_TEXTURE_RECTANGLE_ANGLE:
            {
                if (level != 0)
                {
                    context->handleError(InvalidValue());
                    return false;
                }
                if (tex->getTarget() != GL_TEXTURE_RECTANGLE_ANGLE)
                {
                    context->handleError(InvalidOperation()
                                         << "Textarget must match the texture target type.");
                    return false;
                }
            }
            break;

            case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
            case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
            case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
            case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
            case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
            case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
            {
                if (level > gl::log2(caps.maxCubeMapTextureSize))
                {
                    context->handleError(InvalidValue());
                    return false;
                }
                if (tex->getTarget() != GL_TEXTURE_CUBE_MAP)
                {
                    context->handleError(InvalidOperation()
                                         << "Textarget must match the texture target type.");
                    return false;
                }
            }
            break;

            case GL_TEXTURE_2D_MULTISAMPLE:
            {
                if (context->getClientVersion() < ES_3_1)
                {
                    ANGLE_VALIDATION_ERR(context, InvalidOperation(), ES31Required);
                    return false;
                }

                if (level != 0)
                {
                    ANGLE_VALIDATION_ERR(context, InvalidValue(), LevelNotZero);
                    return false;
                }
                if (tex->getTarget() != GL_TEXTURE_2D_MULTISAMPLE)
                {
                    context->handleError(InvalidOperation()
                                         << "Textarget must match the texture target type.");
                    return false;
                }
            }
            break;

            default:
                ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidTextureTarget);
                return false;
        }

        const Format &format = tex->getFormat(textarget, level);
        if (format.info->compressed)
        {
            context->handleError(InvalidOperation());
            return false;
        }
    }

    return true;
}

bool ValidateGenBuffers(Context *context, GLint n, GLuint *)
{
    return ValidateGenOrDelete(context, n);
}

bool ValidateGenFramebuffers(Context *context, GLint n, GLuint *)
{
    return ValidateGenOrDelete(context, n);
}

bool ValidateGenRenderbuffers(Context *context, GLint n, GLuint *)
{
    return ValidateGenOrDelete(context, n);
}

bool ValidateGenTextures(Context *context, GLint n, GLuint *)
{
    return ValidateGenOrDelete(context, n);
}

bool ValidateGenerateMipmap(Context *context, GLenum target)
{
    if (!ValidTextureTarget(context, target))
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidTextureTarget);
        return false;
    }

    Texture *texture = context->getTargetTexture(target);

    if (texture == nullptr)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), TextureNotBound);
        return false;
    }

    const GLuint effectiveBaseLevel = texture->getTextureState().getEffectiveBaseLevel();

    // This error isn't spelled out in the spec in a very explicit way, but we interpret the spec so
    // that out-of-range base level has a non-color-renderable / non-texture-filterable format.
    if (effectiveBaseLevel >= gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS)
    {
        context->handleError(InvalidOperation());
        return false;
    }

    GLenum baseTarget  = (target == GL_TEXTURE_CUBE_MAP) ? GL_TEXTURE_CUBE_MAP_POSITIVE_X : target;
    const auto &format = *(texture->getFormat(baseTarget, effectiveBaseLevel).info);
    if (format.sizedInternalFormat == GL_NONE || format.compressed || format.depthBits > 0 ||
        format.stencilBits > 0)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), GenerateMipmapNotAllowed);
        return false;
    }

    // GenerateMipmap accepts formats that are unsized or both color renderable and filterable.
    bool formatUnsized = !format.sized;
    bool formatColorRenderableAndFilterable =
        format.filterSupport(context->getClientVersion(), context->getExtensions()) &&
        format.renderSupport(context->getClientVersion(), context->getExtensions());
    if (!formatUnsized && !formatColorRenderableAndFilterable)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), GenerateMipmapNotAllowed);
        return false;
    }

    // GL_EXT_sRGB adds an unsized SRGB (no alpha) format which has explicitly disabled mipmap
    // generation
    if (format.colorEncoding == GL_SRGB && format.format == GL_RGB)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), GenerateMipmapNotAllowed);
        return false;
    }

    // ES3 and WebGL grant mipmap generation for sRGBA (with alpha) textures but GL_EXT_sRGB does
    // not.
    bool supportsSRGBMipmapGeneration =
        context->getClientVersion() >= ES_3_0 || context->getExtensions().webglCompatibility;
    if (!supportsSRGBMipmapGeneration && format.colorEncoding == GL_SRGB)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), GenerateMipmapNotAllowed);
        return false;
    }

    // Non-power of 2 ES2 check
    if (context->getClientVersion() < Version(3, 0) && !context->getExtensions().textureNPOT &&
        (!isPow2(static_cast<int>(texture->getWidth(baseTarget, 0))) ||
         !isPow2(static_cast<int>(texture->getHeight(baseTarget, 0)))))
    {
        ASSERT(target == GL_TEXTURE_2D || target == GL_TEXTURE_RECTANGLE_ANGLE ||
               target == GL_TEXTURE_CUBE_MAP);
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), TextureNotPow2);
        return false;
    }

    // Cube completeness check
    if (target == GL_TEXTURE_CUBE_MAP && !texture->getTextureState().isCubeComplete())
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), CubemapIncomplete);
        return false;
    }

    return true;
}

bool ValidateGetBufferParameteriv(ValidationContext *context,
                                  BufferBinding target,
                                  GLenum pname,
                                  GLint *params)
{
    return ValidateGetBufferParameterBase(context, target, pname, false, nullptr);
}

bool ValidateGetRenderbufferParameteriv(Context *context,
                                        GLenum target,
                                        GLenum pname,
                                        GLint *params)
{
    return ValidateGetRenderbufferParameterivBase(context, target, pname, nullptr);
}

bool ValidateGetShaderiv(Context *context, GLuint shader, GLenum pname, GLint *params)
{
    return ValidateGetShaderivBase(context, shader, pname, nullptr);
}

bool ValidateGetTexParameterfv(Context *context, GLenum target, GLenum pname, GLfloat *params)
{
    return ValidateGetTexParameterBase(context, target, pname, nullptr);
}

bool ValidateGetTexParameteriv(Context *context, GLenum target, GLenum pname, GLint *params)
{
    return ValidateGetTexParameterBase(context, target, pname, nullptr);
}

bool ValidateGetUniformfv(Context *context, GLuint program, GLint location, GLfloat *params)
{
    return ValidateGetUniformBase(context, program, location);
}

bool ValidateGetUniformiv(Context *context, GLuint program, GLint location, GLint *params)
{
    return ValidateGetUniformBase(context, program, location);
}

bool ValidateGetVertexAttribfv(Context *context, GLuint index, GLenum pname, GLfloat *params)
{
    return ValidateGetVertexAttribBase(context, index, pname, nullptr, false, false);
}

bool ValidateGetVertexAttribiv(Context *context, GLuint index, GLenum pname, GLint *params)
{
    return ValidateGetVertexAttribBase(context, index, pname, nullptr, false, false);
}

bool ValidateGetVertexAttribPointerv(Context *context, GLuint index, GLenum pname, void **pointer)
{
    return ValidateGetVertexAttribBase(context, index, pname, nullptr, true, false);
}

bool ValidateIsEnabled(Context *context, GLenum cap)
{
    if (!ValidCap(context, cap, true))
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), EnumNotSupported);
        return false;
    }

    return true;
}

bool ValidateLinkProgram(Context *context, GLuint program)
{
    if (context->hasActiveTransformFeedback(program))
    {
        // ES 3.0.4 section 2.15 page 91
        context->handleError(InvalidOperation() << "Cannot link program while program is "
                                                   "associated with an active transform "
                                                   "feedback object.");
        return false;
    }

    Program *programObject = GetValidProgram(context, program);
    if (!programObject)
    {
        return false;
    }

    return true;
}

bool ValidateReadPixels(Context *context,
                        GLint x,
                        GLint y,
                        GLsizei width,
                        GLsizei height,
                        GLenum format,
                        GLenum type,
                        void *pixels)
{
    return ValidateReadPixelsBase(context, x, y, width, height, format, type, -1, nullptr, nullptr,
                                  nullptr, pixels);
}

bool ValidateTexParameterf(Context *context, GLenum target, GLenum pname, GLfloat param)
{
    return ValidateTexParameterBase(context, target, pname, -1, &param);
}

bool ValidateTexParameterfv(Context *context, GLenum target, GLenum pname, const GLfloat *params)
{
    return ValidateTexParameterBase(context, target, pname, -1, params);
}

bool ValidateTexParameteri(Context *context, GLenum target, GLenum pname, GLint param)
{
    return ValidateTexParameterBase(context, target, pname, -1, &param);
}

bool ValidateTexParameteriv(Context *context, GLenum target, GLenum pname, const GLint *params)
{
    return ValidateTexParameterBase(context, target, pname, -1, params);
}

bool ValidateUseProgram(Context *context, GLuint program)
{
    if (program != 0)
    {
        Program *programObject = context->getProgram(program);
        if (!programObject)
        {
            // ES 3.1.0 section 7.3 page 72
            if (context->getShader(program))
            {
                ANGLE_VALIDATION_ERR(context, InvalidOperation(), ExpectedProgramName);
                return false;
            }
            else
            {
                ANGLE_VALIDATION_ERR(context, InvalidValue(), InvalidProgramName);
                return false;
            }
        }
        if (!programObject->isLinked())
        {
            ANGLE_VALIDATION_ERR(context, InvalidOperation(), ProgramNotLinked);
            return false;
        }
    }
    if (context->getGLState().isTransformFeedbackActiveUnpaused())
    {
        // ES 3.0.4 section 2.15 page 91
        context
            ->handleError(InvalidOperation()
                          << "Cannot change active program while transform feedback is unpaused.");
        return false;
    }

    return true;
}

}  // namespace gl
