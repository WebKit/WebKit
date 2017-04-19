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
#include "libANGLE/Texture.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/Renderbuffer.h"
#include "libANGLE/Shader.h"
#include "libANGLE/Uniform.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/validationES.h"
#include "libANGLE/validationES3.h"
#include "libANGLE/VertexArray.h"

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
            context->handleError(gl::Error(GL_INVALID_OPERATION, "No such path object."));
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
        context->handleError(
            gl::Error(GL_INVALID_OPERATION, "GL_CHROMIUM_path_rendering is not available."));
        return false;
    }

    if (paths == nullptr)
    {
        context->handleError(gl::Error(GL_INVALID_VALUE, "No path name array."));
        return false;
    }

    if (numPaths < 0)
    {
        context->handleError(gl::Error(GL_INVALID_VALUE, "Invalid (negative) numPaths."));
        return false;
    }

    if (!angle::IsValueInRangeForNumericType<std::uint32_t>(numPaths))
    {
        context->handleError(gl::Error(GL_INVALID_OPERATION, "Overflow in numPaths."));
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
            context->handleError(gl::Error(GL_INVALID_ENUM, "Invalid path name type."));
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
            context->handleError(gl::Error(GL_INVALID_ENUM, "Invalid transformation."));
            return false;
    }
    if (componentCount != 0 && transformValues == nullptr)
    {
        context->handleError(gl::Error(GL_INVALID_VALUE, "No transform array given."));
        return false;
    }

    angle::CheckedNumeric<std::uint32_t> checkedSize(0);
    checkedSize += (numPaths * pathNameTypeSize);
    checkedSize += (numPaths * sizeof(GLfloat) * componentCount);
    if (!checkedSize.IsValid())
    {
        context->handleError(gl::Error(GL_INVALID_OPERATION, "Overflow in num paths."));
        return false;
    }

    return true;
}

bool IsValidCopyTextureFormat(Context *context, GLenum internalFormat)
{
    const InternalFormat &internalFormatInfo = GetInternalFormatInfo(internalFormat);
    switch (internalFormatInfo.format)
    {
        case GL_ALPHA:
        case GL_LUMINANCE:
        case GL_LUMINANCE_ALPHA:
        case GL_RGB:
        case GL_RGBA:
            return true;

        case GL_RED:
            return context->getClientMajorVersion() >= 3 || context->getExtensions().textureRG;

        case GL_BGRA_EXT:
            return context->getExtensions().textureFormatBGRA8888;

        default:
            return false;
    }
}

bool IsValidCopyTextureDestinationFormatType(Context *context, GLint internalFormat, GLenum type)
{
    switch (internalFormat)
    {
        case GL_RGB:
        case GL_RGBA:
            break;

        case GL_BGRA_EXT:
            return context->getExtensions().textureFormatBGRA8888;

        default:
            return false;
    }

    switch (type)
    {
        case GL_UNSIGNED_BYTE:
            break;

        default:
            return false;
    }

    return true;
}

bool IsValidCopyTextureDestinationTarget(Context *context, GLenum target)
{
    switch (target)
    {
        case GL_TEXTURE_2D:
            return true;

        // TODO(geofflang): accept GL_TEXTURE_RECTANGLE_ARB if the texture_rectangle extension is
        // supported

        default:
            return false;
    }
}

bool IsValidCopyTextureSourceTarget(Context *context, GLenum target)
{
    if (IsValidCopyTextureDestinationTarget(context, target))
    {
        return true;
    }

    // TODO(geofflang): accept GL_TEXTURE_EXTERNAL_OES if the texture_external extension is
    // supported

    return false;
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
                                   const GLvoid *pixels)
{
    if (!ValidTexture2DDestinationTarget(context, target))
    {
        context->handleError(Error(GL_INVALID_ENUM));
        return false;
    }

    if (!ValidImageSizeParameters(context, target, level, width, height, 1, isSubImage))
    {
        context->handleError(Error(GL_INVALID_VALUE));
        return false;
    }

    if (level < 0 || xoffset < 0 || std::numeric_limits<GLsizei>::max() - xoffset < width ||
        std::numeric_limits<GLsizei>::max() - yoffset < height)
    {
        context->handleError(Error(GL_INVALID_VALUE));
        return false;
    }

    if (!isSubImage && !isCompressed && internalformat != format)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    const gl::Caps &caps = context->getCaps();

    if (target == GL_TEXTURE_2D)
    {
        if (static_cast<GLuint>(width) > (caps.max2DTextureSize >> level) ||
            static_cast<GLuint>(height) > (caps.max2DTextureSize >> level))
        {
            context->handleError(Error(GL_INVALID_VALUE));
            return false;
        }
    }
    else if (IsCubeMapTextureTarget(target))
    {
        if (!isSubImage && width != height)
        {
            context->handleError(Error(GL_INVALID_VALUE));
            return false;
        }

        if (static_cast<GLuint>(width) > (caps.maxCubeMapTextureSize >> level) ||
            static_cast<GLuint>(height) > (caps.maxCubeMapTextureSize >> level))
        {
            context->handleError(Error(GL_INVALID_VALUE));
            return false;
        }
    }
    else
    {
        context->handleError(Error(GL_INVALID_ENUM));
        return false;
    }

    gl::Texture *texture =
        context->getTargetTexture(IsCubeMapTextureTarget(target) ? GL_TEXTURE_CUBE_MAP : target);
    if (!texture)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    if (isSubImage)
    {
        GLenum textureFormat = texture->getFormat(target, level).asSized();
        if (textureFormat == GL_NONE)
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Texture level does not exist."));
            return false;
        }

        if (format != GL_NONE)
        {
            if (gl::GetSizedInternalFormat(format, type) != textureFormat)
            {
                context->handleError(Error(GL_INVALID_OPERATION));
                return false;
            }
        }

        if (static_cast<size_t>(xoffset + width) > texture->getWidth(target, level) ||
            static_cast<size_t>(yoffset + height) > texture->getHeight(target, level))
        {
            context->handleError(Error(GL_INVALID_VALUE));
            return false;
        }
    }
    else
    {
        if (texture->getImmutableFormat())
        {
            context->handleError(Error(GL_INVALID_OPERATION));
            return false;
        }
    }

    // Verify zero border
    if (border != 0)
    {
        context->handleError(Error(GL_INVALID_VALUE));
        return false;
    }

    if (isCompressed)
    {
        GLenum actualInternalFormat =
            isSubImage ? texture->getFormat(target, level).asSized() : internalformat;
        switch (actualInternalFormat)
        {
            case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
            case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
                if (!context->getExtensions().textureCompressionDXT1)
                {
                    context->handleError(Error(GL_INVALID_ENUM));
                    return false;
                }
                break;
            case GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE:
                if (!context->getExtensions().textureCompressionDXT1)
                {
                    context->handleError(Error(GL_INVALID_ENUM));
                    return false;
                }
                break;
            case GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE:
                if (!context->getExtensions().textureCompressionDXT5)
                {
                    context->handleError(Error(GL_INVALID_ENUM));
                    return false;
                }
                break;
            case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
            case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
            case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
            case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
                if (!context->getExtensions().textureCompressionS3TCsRGB)
                {
                    context->handleError(Error(GL_INVALID_ENUM));
                    return false;
                }
                break;
            case GL_ETC1_RGB8_OES:
                if (!context->getExtensions().compressedETC1RGB8Texture)
                {
                    context->handleError(Error(GL_INVALID_ENUM));
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
                    context->handleError(Error(
                        GL_INVALID_ENUM, "ANGLE_lossy_etc_decode extension is not supported"));
                    return false;
                }
                break;
            default:
                context->handleError(
                    Error(GL_INVALID_ENUM,
                          "internalformat is not a supported compressed internal format"));
                return false;
        }
        if (!ValidCompressedImageSize(context, actualInternalFormat, xoffset, yoffset, width,
                                      height))
        {
            context->handleError(Error(GL_INVALID_OPERATION));
            return false;
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
                context->handleError(Error(GL_INVALID_ENUM));
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
                        context->handleError(Error(GL_INVALID_OPERATION));
                        return false;
                }
                break;
            case GL_RED:
            case GL_RG:
                if (!context->getExtensions().textureRG)
                {
                    context->handleError(Error(GL_INVALID_ENUM));
                    return false;
                }
                switch (type)
                {
                    case GL_UNSIGNED_BYTE:
                    case GL_FLOAT:
                    case GL_HALF_FLOAT_OES:
                        break;
                    default:
                        context->handleError(Error(GL_INVALID_OPERATION));
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
                        context->handleError(Error(GL_INVALID_OPERATION));
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
                        context->handleError(Error(GL_INVALID_OPERATION));
                        return false;
                }
                break;
            case GL_BGRA_EXT:
                switch (type)
                {
                    case GL_UNSIGNED_BYTE:
                        break;
                    default:
                        context->handleError(Error(GL_INVALID_OPERATION));
                        return false;
                }
                break;
            case GL_SRGB_EXT:
            case GL_SRGB_ALPHA_EXT:
                if (!context->getExtensions().sRGB)
                {
                    context->handleError(Error(GL_INVALID_ENUM));
                    return false;
                }
                switch (type)
                {
                    case GL_UNSIGNED_BYTE:
                        break;
                    default:
                        context->handleError(Error(GL_INVALID_OPERATION));
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
                        context->handleError(Error(GL_INVALID_OPERATION));
                        return false;
                }
                break;
            case GL_DEPTH_STENCIL_OES:
                switch (type)
                {
                    case GL_UNSIGNED_INT_24_8_OES:
                        break;
                    default:
                        context->handleError(Error(GL_INVALID_OPERATION));
                        return false;
                }
                break;
            default:
                context->handleError(Error(GL_INVALID_ENUM));
                return false;
        }

        switch (format)
        {
            case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
            case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
                if (context->getExtensions().textureCompressionDXT1)
                {
                    context->handleError(Error(GL_INVALID_OPERATION));
                    return false;
                }
                else
                {
                    context->handleError(Error(GL_INVALID_ENUM));
                    return false;
                }
                break;
            case GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE:
                if (context->getExtensions().textureCompressionDXT3)
                {
                    context->handleError(Error(GL_INVALID_OPERATION));
                    return false;
                }
                else
                {
                    context->handleError(Error(GL_INVALID_ENUM));
                    return false;
                }
                break;
            case GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE:
                if (context->getExtensions().textureCompressionDXT5)
                {
                    context->handleError(Error(GL_INVALID_OPERATION));
                    return false;
                }
                else
                {
                    context->handleError(Error(GL_INVALID_ENUM));
                    return false;
                }
                break;
            case GL_ETC1_RGB8_OES:
                if (context->getExtensions().compressedETC1RGB8Texture)
                {
                    context->handleError(Error(GL_INVALID_OPERATION));
                    return false;
                }
                else
                {
                    context->handleError(Error(GL_INVALID_ENUM));
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
                    context->handleError(
                        Error(GL_INVALID_OPERATION,
                              "ETC lossy decode formats can't work with this type."));
                    return false;
                }
                else
                {
                    context->handleError(Error(
                        GL_INVALID_ENUM, "ANGLE_lossy_etc_decode extension is not supported."));
                    return false;
                }
                break;
            case GL_DEPTH_COMPONENT:
            case GL_DEPTH_STENCIL_OES:
                if (!context->getExtensions().depthTextures)
                {
                    context->handleError(Error(GL_INVALID_VALUE));
                    return false;
                }
                if (target != GL_TEXTURE_2D)
                {
                    context->handleError(Error(GL_INVALID_OPERATION));
                    return false;
                }
                // OES_depth_texture supports loading depth data and multiple levels,
                // but ANGLE_depth_texture does not
                if (pixels != NULL || level != 0)
                {
                    context->handleError(Error(GL_INVALID_OPERATION));
                    return false;
                }
                break;
            default:
                break;
        }

        if (type == GL_FLOAT)
        {
            if (!context->getExtensions().textureFloat)
            {
                context->handleError(Error(GL_INVALID_ENUM));
                return false;
            }
        }
        else if (type == GL_HALF_FLOAT_OES)
        {
            if (!context->getExtensions().textureHalfFloat)
            {
                context->handleError(Error(GL_INVALID_ENUM));
                return false;
            }
        }
    }

    if (!ValidImageDataSize(context, target, width, height, 1, internalformat, type, pixels,
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
    if (target != GL_TEXTURE_2D && target != GL_TEXTURE_CUBE_MAP)
    {
        context->handleError(Error(GL_INVALID_ENUM));
        return false;
    }

    if (width < 1 || height < 1 || levels < 1)
    {
        context->handleError(Error(GL_INVALID_VALUE));
        return false;
    }

    if (target == GL_TEXTURE_CUBE_MAP && width != height)
    {
        context->handleError(Error(GL_INVALID_VALUE));
        return false;
    }

    if (levels != 1 && levels != gl::log2(std::max(width, height)) + 1)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    const gl::InternalFormat &formatInfo = gl::GetInternalFormatInfo(internalformat);
    if (formatInfo.format == GL_NONE || formatInfo.type == GL_NONE)
    {
        context->handleError(Error(GL_INVALID_ENUM));
        return false;
    }

    const gl::Caps &caps = context->getCaps();

    switch (target)
    {
        case GL_TEXTURE_2D:
            if (static_cast<GLuint>(width) > caps.max2DTextureSize ||
                static_cast<GLuint>(height) > caps.max2DTextureSize)
            {
                context->handleError(Error(GL_INVALID_VALUE));
                return false;
            }
            break;
        case GL_TEXTURE_CUBE_MAP:
            if (static_cast<GLuint>(width) > caps.maxCubeMapTextureSize ||
                static_cast<GLuint>(height) > caps.maxCubeMapTextureSize)
            {
                context->handleError(Error(GL_INVALID_VALUE));
                return false;
            }
            break;
        default:
            context->handleError(Error(GL_INVALID_ENUM));
            return false;
    }

    if (levels != 1 && !context->getExtensions().textureNPOT)
    {
        if (!gl::isPow2(width) || !gl::isPow2(height))
        {
            context->handleError(Error(GL_INVALID_OPERATION));
            return false;
        }
    }

    switch (internalformat)
    {
        case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
            if (!context->getExtensions().textureCompressionDXT1)
            {
                context->handleError(Error(GL_INVALID_ENUM));
                return false;
            }
            break;
        case GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE:
            if (!context->getExtensions().textureCompressionDXT3)
            {
                context->handleError(Error(GL_INVALID_ENUM));
                return false;
            }
            break;
        case GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE:
            if (!context->getExtensions().textureCompressionDXT5)
            {
                context->handleError(Error(GL_INVALID_ENUM));
                return false;
            }
            break;
        case GL_ETC1_RGB8_OES:
            if (!context->getExtensions().compressedETC1RGB8Texture)
            {
                context->handleError(Error(GL_INVALID_ENUM));
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
                context->handleError(
                    Error(GL_INVALID_ENUM, "ANGLE_lossy_etc_decode extension is not supported."));
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
                context->handleError(Error(GL_INVALID_ENUM));
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
                context->handleError(Error(GL_INVALID_ENUM));
                return false;
            }
            break;
        case GL_R8_EXT:
        case GL_RG8_EXT:
        case GL_R16F_EXT:
        case GL_RG16F_EXT:
        case GL_R32F_EXT:
        case GL_RG32F_EXT:
            if (!context->getExtensions().textureRG)
            {
                context->handleError(Error(GL_INVALID_ENUM));
                return false;
            }
            break;
        case GL_DEPTH_COMPONENT16:
        case GL_DEPTH_COMPONENT32_OES:
        case GL_DEPTH24_STENCIL8_OES:
            if (!context->getExtensions().depthTextures)
            {
                context->handleError(Error(GL_INVALID_ENUM));
                return false;
            }
            if (target != GL_TEXTURE_2D)
            {
                context->handleError(Error(GL_INVALID_OPERATION));
                return false;
            }
            // ANGLE_depth_texture only supports 1-level textures
            if (levels != 1)
            {
                context->handleError(Error(GL_INVALID_OPERATION));
                return false;
            }
            break;
        default:
            break;
    }

    gl::Texture *texture = context->getTargetTexture(target);
    if (!texture || texture->id() == 0)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    if (texture->getImmutableFormat())
    {
        context->handleError(Error(GL_INVALID_OPERATION));
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
        context->handleError(Error(GL_INVALID_OPERATION, "Extension not enabled"));
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
            context->handleError(Error(GL_INVALID_ENUM, "Invalid framebuffer target"));
            return false;
    }

    return ValidateDiscardFramebufferBase(context, target, numAttachments, attachments,
                                          defaultFramebuffer);
}

bool ValidateBindVertexArrayOES(Context *context, GLuint array)
{
    if (!context->getExtensions().vertexArrayObject)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Extension not enabled"));
        return false;
    }

    return ValidateBindVertexArrayBase(context, array);
}

bool ValidateDeleteVertexArraysOES(Context *context, GLsizei n)
{
    if (!context->getExtensions().vertexArrayObject)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Extension not enabled"));
        return false;
    }

    return ValidateGenOrDelete(context, n);
}

bool ValidateGenVertexArraysOES(Context *context, GLsizei n)
{
    if (!context->getExtensions().vertexArrayObject)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Extension not enabled"));
        return false;
    }

    return ValidateGenOrDelete(context, n);
}

bool ValidateIsVertexArrayOES(Context *context)
{
    if (!context->getExtensions().vertexArrayObject)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Extension not enabled"));
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
        context->handleError(Error(GL_INVALID_OPERATION, "Extension not enabled"));
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
        context->handleError(Error(GL_INVALID_OPERATION, "Extension not enabled"));
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
        context->handleError(Error(GL_INVALID_OPERATION, "Extension not enabled"));
        return false;
    }

    if (!ValidDebugSource(source, false) && source != GL_DONT_CARE)
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid debug source."));
        return false;
    }

    if (!ValidDebugType(type) && type != GL_DONT_CARE)
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid debug type."));
        return false;
    }

    if (!ValidDebugSeverity(severity) && severity != GL_DONT_CARE)
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid debug severity."));
        return false;
    }

    if (count > 0)
    {
        if (source == GL_DONT_CARE || type == GL_DONT_CARE)
        {
            context->handleError(Error(
                GL_INVALID_OPERATION,
                "If count is greater than zero, source and severity cannot be GL_DONT_CARE."));
            return false;
        }

        if (severity != GL_DONT_CARE)
        {
            context->handleError(
                Error(GL_INVALID_OPERATION,
                      "If count is greater than zero, severity must be GL_DONT_CARE."));
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
        context->handleError(Error(GL_INVALID_OPERATION, "Extension not enabled"));
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
        context->handleError(Error(GL_INVALID_ENUM, "Invalid debug severity."));
        return false;
    }

    if (!ValidDebugType(type))
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid debug type."));
        return false;
    }

    if (!ValidDebugSource(source, true))
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid debug source."));
        return false;
    }

    size_t messageLength = (length < 0) ? strlen(buf) : length;
    if (messageLength > context->getExtensions().maxDebugMessageLength)
    {
        context->handleError(
            Error(GL_INVALID_VALUE, "Message length is larger than GL_MAX_DEBUG_MESSAGE_LENGTH."));
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
        context->handleError(Error(GL_INVALID_OPERATION, "Extension not enabled"));
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
        context->handleError(Error(GL_INVALID_OPERATION, "Extension not enabled"));
        return false;
    }

    if (bufSize < 0 && messageLog != nullptr)
    {
        context->handleError(
            Error(GL_INVALID_VALUE, "bufSize must be positive if messageLog is not null."));
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
        context->handleError(Error(GL_INVALID_OPERATION, "Extension not enabled"));
        return false;
    }

    if (!ValidDebugSource(source, true))
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid debug source."));
        return false;
    }

    size_t messageLength = (length < 0) ? strlen(message) : length;
    if (messageLength > context->getExtensions().maxDebugMessageLength)
    {
        context->handleError(
            Error(GL_INVALID_VALUE, "Message length is larger than GL_MAX_DEBUG_MESSAGE_LENGTH."));
        return false;
    }

    size_t currentStackSize = context->getGLState().getDebug().getGroupStackDepth();
    if (currentStackSize >= context->getExtensions().maxDebugGroupStackDepth)
    {
        context->handleError(
            Error(GL_STACK_OVERFLOW,
                  "Cannot push more than GL_MAX_DEBUG_GROUP_STACK_DEPTH debug groups."));
        return false;
    }

    return true;
}

bool ValidatePopDebugGroupKHR(Context *context)
{
    if (!context->getExtensions().debug)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Extension not enabled"));
        return false;
    }

    size_t currentStackSize = context->getGLState().getDebug().getGroupStackDepth();
    if (currentStackSize <= 1)
    {
        context->handleError(Error(GL_STACK_UNDERFLOW, "Cannot pop the default debug group."));
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
                context->handleError(Error(GL_INVALID_VALUE, "name is not a valid buffer."));
                return false;
            }
            return true;

        case GL_SHADER:
            if (context->getShader(name) == nullptr)
            {
                context->handleError(Error(GL_INVALID_VALUE, "name is not a valid shader."));
                return false;
            }
            return true;

        case GL_PROGRAM:
            if (context->getProgram(name) == nullptr)
            {
                context->handleError(Error(GL_INVALID_VALUE, "name is not a valid program."));
                return false;
            }
            return true;

        case GL_VERTEX_ARRAY:
            if (context->getVertexArray(name) == nullptr)
            {
                context->handleError(Error(GL_INVALID_VALUE, "name is not a valid vertex array."));
                return false;
            }
            return true;

        case GL_QUERY:
            if (context->getQuery(name) == nullptr)
            {
                context->handleError(Error(GL_INVALID_VALUE, "name is not a valid query."));
                return false;
            }
            return true;

        case GL_TRANSFORM_FEEDBACK:
            if (context->getTransformFeedback(name) == nullptr)
            {
                context->handleError(
                    Error(GL_INVALID_VALUE, "name is not a valid transform feedback."));
                return false;
            }
            return true;

        case GL_SAMPLER:
            if (context->getSampler(name) == nullptr)
            {
                context->handleError(Error(GL_INVALID_VALUE, "name is not a valid sampler."));
                return false;
            }
            return true;

        case GL_TEXTURE:
            if (context->getTexture(name) == nullptr)
            {
                context->handleError(Error(GL_INVALID_VALUE, "name is not a valid texture."));
                return false;
            }
            return true;

        case GL_RENDERBUFFER:
            if (context->getRenderbuffer(name) == nullptr)
            {
                context->handleError(Error(GL_INVALID_VALUE, "name is not a valid renderbuffer."));
                return false;
            }
            return true;

        case GL_FRAMEBUFFER:
            if (context->getFramebuffer(name) == nullptr)
            {
                context->handleError(Error(GL_INVALID_VALUE, "name is not a valid framebuffer."));
                return false;
            }
            return true;

        default:
            context->handleError(Error(GL_INVALID_ENUM, "Invalid identifier."));
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
        context->handleError(
            Error(GL_INVALID_VALUE, "Label length is larger than GL_MAX_LABEL_LENGTH."));
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
        context->handleError(Error(GL_INVALID_OPERATION, "Extension not enabled"));
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
        context->handleError(Error(GL_INVALID_OPERATION, "Extension not enabled"));
        return false;
    }

    if (bufSize < 0)
    {
        context->handleError(Error(GL_INVALID_VALUE, "bufSize cannot be negative."));
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
    if (context->getFenceSync(reinterpret_cast<GLsync>(const_cast<void *>(ptr))) == nullptr)
    {
        context->handleError(Error(GL_INVALID_VALUE, "name is not a valid sync."));
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
        context->handleError(Error(GL_INVALID_OPERATION, "Extension not enabled"));
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
        context->handleError(Error(GL_INVALID_OPERATION, "Extension not enabled"));
        return false;
    }

    if (bufSize < 0)
    {
        context->handleError(Error(GL_INVALID_VALUE, "bufSize cannot be negative."));
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
        context->handleError(Error(GL_INVALID_OPERATION, "Extension not enabled"));
        return false;
    }

    // TODO: represent this in Context::getQueryParameterInfo.
    switch (pname)
    {
        case GL_DEBUG_CALLBACK_FUNCTION:
        case GL_DEBUG_CALLBACK_USER_PARAM:
            break;

        default:
            context->handleError(Error(GL_INVALID_ENUM, "Invalid pname."));
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
        context->handleError(Error(GL_INVALID_OPERATION, "Blit extension not available."));
        return false;
    }

    if (srcX1 - srcX0 != dstX1 - dstX0 || srcY1 - srcY0 != dstY1 - dstY0)
    {
        // TODO(jmadill): Determine if this should be available on other implementations.
        context->handleError(Error(
            GL_INVALID_OPERATION,
            "Scaling and flipping in BlitFramebufferANGLE not supported by this implementation."));
        return false;
    }

    if (filter == GL_LINEAR)
    {
        context->handleError(Error(GL_INVALID_ENUM, "Linear blit not supported in this extension"));
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
                context->handleError(Error(GL_INVALID_OPERATION));
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
                        context->handleError(Error(GL_INVALID_OPERATION));
                        return false;
                    }

                    // Return an error if the destination formats do not match
                    if (!Format::SameSized(attachment->getFormat(),
                                           readColorAttachment->getFormat()))
                    {
                        context->handleError(Error(GL_INVALID_OPERATION));
                        return false;
                    }
                }
            }

            if (readFramebuffer->getSamples(context) != 0 &&
                IsPartialBlit(context, readColorAttachment, drawColorAttachment, srcX0, srcY0,
                              srcX1, srcY1, dstX0, dstY0, dstX1, dstY1))
            {
                context->handleError(Error(GL_INVALID_OPERATION));
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
                    context->handleError(Error(GL_INVALID_OPERATION,
                                               "Only whole-buffer depth and stencil blits are "
                                               "supported by this extension."));
                    return false;
                }

                if (readBuffer->getSamples() != 0 || drawBuffer->getSamples() != 0)
                {
                    context->handleError(Error(GL_INVALID_OPERATION));
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
    auto fbo = context->getGLState().getDrawFramebuffer();
    if (fbo->checkStatus(context) != GL_FRAMEBUFFER_COMPLETE)
    {
        context->handleError(Error(GL_INVALID_FRAMEBUFFER_OPERATION));
        return false;
    }

    if ((mask & ~(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) != 0)
    {
        context->handleError(Error(GL_INVALID_VALUE));
        return false;
    }

    if (context->getExtensions().webglCompatibility && (mask & GL_COLOR_BUFFER_BIT) != 0)
    {
        constexpr GLenum validComponentTypes[] = {GL_FLOAT, GL_UNSIGNED_NORMALIZED,
                                                  GL_SIGNED_NORMALIZED};

        for (GLuint drawBufferIdx = 0; drawBufferIdx < context->getCaps().maxDrawBuffers;
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
        context->handleError(Error(GL_INVALID_OPERATION, "Extension not supported."));
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
                        const GLvoid *pixels)
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
                              const GLvoid *pixels)
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
                           const GLvoid *pixels)
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
                                      const GLvoid *pixels)
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
                                  const GLvoid *data)
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

    const InternalFormat &formatInfo = GetInternalFormatInfo(internalformat);
    auto blockSizeOrErr =
        formatInfo.computeCompressedImageSize(GL_UNSIGNED_BYTE, gl::Extents(width, height, 1));
    if (blockSizeOrErr.isError())
    {
        context->handleError(blockSizeOrErr.getError());
        return false;
    }

    if (imageSize < 0 || static_cast<GLuint>(imageSize) != blockSizeOrErr.getResult())
    {
        context->handleError(Error(GL_INVALID_VALUE));
        return false;
    }

    return true;
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
                                     const GLvoid *data)
{
    if (context->getClientMajorVersion() < 3)
    {
        if (!ValidateES2TexImageParameters(context, target, level, GL_NONE, true, true, xoffset,
                                           yoffset, width, height, 0, GL_NONE, GL_NONE, -1, data))
        {
            return false;
        }
    }
    else
    {
        ASSERT(context->getClientMajorVersion() >= 3);
        if (!ValidateES3TexImage2DParameters(context, target, level, GL_NONE, true, true, xoffset,
                                             yoffset, 0, width, height, 1, 0, GL_NONE, GL_NONE, -1,
                                             data))
        {
            return false;
        }
    }

    const InternalFormat &formatInfo = GetInternalFormatInfo(format);
    auto blockSizeOrErr =
        formatInfo.computeCompressedImageSize(GL_UNSIGNED_BYTE, gl::Extents(width, height, 1));
    if (blockSizeOrErr.isError())
    {
        context->handleError(blockSizeOrErr.getError());
        return false;
    }

    if (imageSize < 0 || static_cast<GLuint>(imageSize) != blockSizeOrErr.getResult())
    {
        context->handleError(Error(GL_INVALID_VALUE));
        return false;
    }

    return true;
}

bool ValidateGetBufferPointervOES(Context *context, GLenum target, GLenum pname, void **params)
{
    return ValidateGetBufferPointervBase(context, target, pname, nullptr, params);
}

bool ValidateMapBufferOES(Context *context, GLenum target, GLenum access)
{
    if (!context->getExtensions().mapBuffer)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Map buffer extension not available."));
        return false;
    }

    if (!ValidBufferTarget(context, target))
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid buffer target."));
        return false;
    }

    Buffer *buffer = context->getGLState().getTargetBuffer(target);

    if (buffer == nullptr)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Attempted to map buffer object zero."));
        return false;
    }

    if (access != GL_WRITE_ONLY_OES)
    {
        context->handleError(Error(GL_INVALID_ENUM, "Non-write buffer mapping not supported."));
        return false;
    }

    if (buffer->isMapped())
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Buffer is already mapped."));
        return false;
    }

    return true;
}

bool ValidateUnmapBufferOES(Context *context, GLenum target)
{
    if (!context->getExtensions().mapBuffer)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Map buffer extension not available."));
        return false;
    }

    return ValidateUnmapBufferBase(context, target);
}

bool ValidateMapBufferRangeEXT(Context *context,
                               GLenum target,
                               GLintptr offset,
                               GLsizeiptr length,
                               GLbitfield access)
{
    if (!context->getExtensions().mapBufferRange)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "Map buffer range extension not available."));
        return false;
    }

    return ValidateMapBufferRangeBase(context, target, offset, length, access);
}

bool ValidateFlushMappedBufferRangeEXT(Context *context,
                                       GLenum target,
                                       GLintptr offset,
                                       GLsizeiptr length)
{
    if (!context->getExtensions().mapBufferRange)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "Map buffer range extension not available."));
        return false;
    }

    return ValidateFlushMappedBufferRangeBase(context, target, offset, length);
}

bool ValidateBindTexture(Context *context, GLenum target, GLuint texture)
{
    Texture *textureObject = context->getTexture(texture);
    if (textureObject && textureObject->getTarget() != target && texture != 0)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Invalid texture"));
        return false;
    }

    if (!context->getGLState().isBindGeneratesResourceEnabled() &&
        !context->isTextureGenerated(texture))
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Texture was not generated"));
        return false;
    }

    switch (target)
    {
        case GL_TEXTURE_2D:
        case GL_TEXTURE_CUBE_MAP:
            break;

        case GL_TEXTURE_3D:
        case GL_TEXTURE_2D_ARRAY:
            if (context->getClientMajorVersion() < 3)
            {
                context->handleError(Error(GL_INVALID_ENUM, "GLES 3.0 disabled"));
                return false;
            }
            break;

        case GL_TEXTURE_2D_MULTISAMPLE:
            if (context->getClientVersion() < Version(3, 1))
            {
                context->handleError(Error(GL_INVALID_ENUM, "Context does not support GLES3.1"));
                return false;
            }
            break;

        case GL_TEXTURE_EXTERNAL_OES:
            if (!context->getExtensions().eglImageExternal &&
                !context->getExtensions().eglStreamConsumerExternal)
            {
                context->handleError(
                    Error(GL_INVALID_ENUM, "External texture extension not enabled"));
                return false;
            }
            break;
        default:
            context->handleError(Error(GL_INVALID_ENUM, "Invalid target"));
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
        context->handleError(
            Error(GL_INVALID_OPERATION, "GL_CHROMIUM_bind_uniform_location is not available."));
        return false;
    }

    Program *programObject = GetValidProgram(context, program);
    if (!programObject)
    {
        return false;
    }

    if (location < 0)
    {
        context->handleError(Error(GL_INVALID_VALUE, "Location cannot be less than 0."));
        return false;
    }

    const Caps &caps = context->getCaps();
    if (static_cast<size_t>(location) >=
        (caps.maxVertexUniformVectors + caps.maxFragmentUniformVectors) * 4)
    {
        context->handleError(Error(GL_INVALID_VALUE,
                                   "Location must be less than (MAX_VERTEX_UNIFORM_VECTORS + "
                                   "MAX_FRAGMENT_UNIFORM_VECTORS) * 4"));
        return false;
    }

    if (strncmp(name, "gl_", 3) == 0)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "Name cannot start with the reserved \"gl_\" prefix."));
        return false;
    }

    return true;
}

bool ValidateCoverageModulationCHROMIUM(Context *context, GLenum components)
{
    if (!context->getExtensions().framebufferMixedSamples)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "GL_CHROMIUM_framebuffer_mixed_samples is not available."));
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
                Error(GL_INVALID_ENUM,
                      "GLenum components is not one of GL_RGB, GL_RGBA, GL_ALPHA or GL_NONE."));
            return false;
    }

    return true;
}

// CHROMIUM_path_rendering

bool ValidateMatrix(Context *context, GLenum matrixMode, const GLfloat *matrix)
{
    if (!context->getExtensions().pathRendering)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "GL_CHROMIUM_path_rendering is not available."));
        return false;
    }
    if (matrixMode != GL_PATH_MODELVIEW_CHROMIUM && matrixMode != GL_PATH_PROJECTION_CHROMIUM)
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid matrix mode."));
        return false;
    }
    if (matrix == nullptr)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Invalid matrix."));
        return false;
    }
    return true;
}

bool ValidateMatrixMode(Context *context, GLenum matrixMode)
{
    if (!context->getExtensions().pathRendering)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "GL_CHROMIUM_path_rendering is not available."));
        return false;
    }
    if (matrixMode != GL_PATH_MODELVIEW_CHROMIUM && matrixMode != GL_PATH_PROJECTION_CHROMIUM)
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid matrix mode."));
        return false;
    }
    return true;
}

bool ValidateGenPaths(Context *context, GLsizei range)
{
    if (!context->getExtensions().pathRendering)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "GL_CHROMIUM_path_rendering is not available."));
        return false;
    }

    // range = 0 is undefined in NV_path_rendering.
    // we add stricter semantic check here and require a non zero positive range.
    if (range <= 0)
    {
        context->handleError(Error(GL_INVALID_VALUE, "Invalid range."));
        return false;
    }

    if (!angle::IsValueInRangeForNumericType<std::uint32_t>(range))
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Range overflow."));
        return false;
    }

    return true;
}

bool ValidateDeletePaths(Context *context, GLuint path, GLsizei range)
{
    if (!context->getExtensions().pathRendering)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "GL_CHROMIUM_path_rendering is not available."));
        return false;
    }

    // range = 0 is undefined in NV_path_rendering.
    // we add stricter semantic check here and require a non zero positive range.
    if (range <= 0)
    {
        context->handleError(Error(GL_INVALID_VALUE, "Invalid range."));
        return false;
    }

    angle::CheckedNumeric<std::uint32_t> checkedRange(path);
    checkedRange += range;

    if (!angle::IsValueInRangeForNumericType<std::uint32_t>(range) || !checkedRange.IsValid())
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Range overflow."));
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
        context->handleError(
            Error(GL_INVALID_OPERATION, "GL_CHROMIUM_path_rendering is not available."));
        return false;
    }
    if (!context->hasPath(path))
    {
        context->handleError(Error(GL_INVALID_OPERATION, "No such path object."));
        return false;
    }

    if (numCommands < 0)
    {
        context->handleError(Error(GL_INVALID_VALUE, "Invalid number of commands."));
        return false;
    }
    else if (numCommands > 0)
    {
        if (!commands)
        {
            context->handleError(Error(GL_INVALID_VALUE, "No commands array given."));
            return false;
        }
    }

    if (numCoords < 0)
    {
        context->handleError(Error(GL_INVALID_VALUE, "Invalid number of coordinates."));
        return false;
    }
    else if (numCoords > 0)
    {
        if (!coords)
        {
            context->handleError(Error(GL_INVALID_VALUE, "No coordinate array given."));
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
            context->handleError(Error(GL_INVALID_ENUM, "Invalid coordinate type."));
            return false;
    }

    angle::CheckedNumeric<std::uint32_t> checkedSize(numCommands);
    checkedSize += (coordTypeSize * numCoords);
    if (!checkedSize.IsValid())
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Coord size overflow."));
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
                context->handleError(Error(GL_INVALID_ENUM, "Invalid command."));
                return false;
        }
    }
    if (expectedNumCoords != numCoords)
    {
        context->handleError(Error(GL_INVALID_VALUE, "Invalid number of coordinates."));
        return false;
    }

    return true;
}

bool ValidateSetPathParameter(Context *context, GLuint path, GLenum pname, GLfloat value)
{
    if (!context->getExtensions().pathRendering)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "GL_CHROMIUM_path_rendering is not available."));
        return false;
    }
    if (!context->hasPath(path))
    {
        context->handleError(Error(GL_INVALID_OPERATION, "No such path object."));
        return false;
    }

    switch (pname)
    {
        case GL_PATH_STROKE_WIDTH_CHROMIUM:
            if (value < 0.0f)
            {
                context->handleError(Error(GL_INVALID_VALUE, "Invalid stroke width."));
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
                    context->handleError(Error(GL_INVALID_ENUM, "Invalid end caps."));
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
                    context->handleError(Error(GL_INVALID_ENUM, "Invalid join style."));
                    return false;
            }
        case GL_PATH_MITER_LIMIT_CHROMIUM:
            if (value < 0.0f)
            {
                context->handleError(Error(GL_INVALID_VALUE, "Invalid miter limit."));
                return false;
            }
            break;

        case GL_PATH_STROKE_BOUND_CHROMIUM:
            // no errors, only clamping.
            break;

        default:
            context->handleError(Error(GL_INVALID_ENUM, "Invalid path parameter."));
            return false;
    }
    return true;
}

bool ValidateGetPathParameter(Context *context, GLuint path, GLenum pname, GLfloat *value)
{
    if (!context->getExtensions().pathRendering)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "GL_CHROMIUM_path_rendering is not available."));
        return false;
    }

    if (!context->hasPath(path))
    {
        context->handleError(Error(GL_INVALID_OPERATION, "No such path object."));
        return false;
    }
    if (!value)
    {
        context->handleError(Error(GL_INVALID_VALUE, "No value array."));
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
            context->handleError(Error(GL_INVALID_ENUM, "Invalid path parameter."));
            return false;
    }

    return true;
}

bool ValidatePathStencilFunc(Context *context, GLenum func, GLint ref, GLuint mask)
{
    if (!context->getExtensions().pathRendering)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "GL_CHROMIUM_path_rendering is not available."));
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
            context->handleError(Error(GL_INVALID_ENUM, "Invalid stencil function."));
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
        context->handleError(
            Error(GL_INVALID_OPERATION, "GL_CHROMIUM_path_rendering is not available."));
        return false;
    }
    if (context->hasPath(path) && !context->hasPathData(path))
    {
        context->handleError(Error(GL_INVALID_OPERATION, "No such path object."));
        return false;
    }

    switch (fillMode)
    {
        case GL_COUNT_UP_CHROMIUM:
        case GL_COUNT_DOWN_CHROMIUM:
            break;
        default:
            context->handleError(Error(GL_INVALID_ENUM, "Invalid fill mode."));
            return false;
    }

    if (!isPow2(mask + 1))
    {
        context->handleError(Error(GL_INVALID_VALUE, "Invalid stencil bit mask."));
        return false;
    }

    return true;
}

bool ValidateStencilStrokePath(Context *context, GLuint path, GLint reference, GLuint mask)
{
    if (!context->getExtensions().pathRendering)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "GL_CHROMIUM_path_rendering is not available."));
        return false;
    }
    if (context->hasPath(path) && !context->hasPathData(path))
    {
        context->handleError(Error(GL_INVALID_OPERATION, "No such path or path has no data."));
        return false;
    }

    return true;
}

bool ValidateCoverPath(Context *context, GLuint path, GLenum coverMode)
{
    if (!context->getExtensions().pathRendering)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "GL_CHROMIUM_path_rendering is not available."));
        return false;
    }
    if (context->hasPath(path) && !context->hasPathData(path))
    {
        context->handleError(Error(GL_INVALID_OPERATION, "No such path object."));
        return false;
    }

    switch (coverMode)
    {
        case GL_CONVEX_HULL_CHROMIUM:
        case GL_BOUNDING_BOX_CHROMIUM:
            break;
        default:
            context->handleError(Error(GL_INVALID_ENUM, "Invalid cover mode."));
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
        context->handleError(
            Error(GL_INVALID_OPERATION, "GL_CHROMIUM_path_rendering is not available."));
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
            context->handleError(Error(GL_INVALID_ENUM, "Invalid cover mode."));
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
            context->handleError(Error(GL_INVALID_ENUM, "Invalid cover mode."));
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
            context->handleError(Error(GL_INVALID_ENUM, "Invalid fill mode."));
            return false;
    }
    if (!isPow2(mask + 1))
    {
        context->handleError(Error(GL_INVALID_VALUE, "Invalid stencil bit mask."));
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
            context->handleError(Error(GL_INVALID_ENUM, "Invalid cover mode."));
            return false;
    }

    switch (fillMode)
    {
        case GL_COUNT_UP_CHROMIUM:
        case GL_COUNT_DOWN_CHROMIUM:
            break;
        default:
            context->handleError(Error(GL_INVALID_ENUM, "Invalid fill mode."));
            return false;
    }
    if (!isPow2(mask + 1))
    {
        context->handleError(Error(GL_INVALID_VALUE, "Invalid stencil bit mask."));
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
            context->handleError(Error(GL_INVALID_ENUM, "Invalid cover mode."));
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
        context->handleError(
            Error(GL_INVALID_OPERATION, "GL_CHROMIUM_path_rendering is not available."));
        return false;
    }

    const GLint MaxLocation = context->getCaps().maxVaryingVectors * 4;
    if (location >= MaxLocation)
    {
        context->handleError(Error(GL_INVALID_VALUE, "Location exceeds max varying."));
        return false;
    }

    const auto *programObject = context->getProgram(program);
    if (!programObject)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "No such program."));
        return false;
    }

    if (!name)
    {
        context->handleError(Error(GL_INVALID_VALUE, "No name given."));
        return false;
    }

    if (angle::BeginsWith(name, "gl_"))
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Cannot bind a built-in variable."));
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
        context->handleError(
            Error(GL_INVALID_OPERATION, "GL_CHROMIUM_path_rendering is not available."));
        return false;
    }

    const auto *programObject = context->getProgram(program);
    if (!programObject || programObject->isFlaggedForDeletion())
    {
        context->handleError(Error(GL_INVALID_OPERATION, "No such program."));
        return false;
    }

    if (!programObject->isLinked())
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Program is not linked."));
        return false;
    }

    switch (genMode)
    {
        case GL_NONE:
            if (components != 0)
            {
                context->handleError(Error(GL_INVALID_VALUE, "Invalid components."));
                return false;
            }
            break;

        case GL_OBJECT_LINEAR_CHROMIUM:
        case GL_EYE_LINEAR_CHROMIUM:
        case GL_CONSTANT_CHROMIUM:
            if (components < 1 || components > 4)
            {
                context->handleError(Error(GL_INVALID_VALUE, "Invalid components."));
                return false;
            }
            if (!coeffs)
            {
                context->handleError(Error(GL_INVALID_VALUE, "No coefficients array given."));
                return false;
            }
            break;

        default:
            context->handleError(Error(GL_INVALID_ENUM, "Invalid gen mode."));
            return false;
    }

    // If the location is -1 then the command is silently ignored
    // and no further validation is needed.
    if (location == -1)
        return true;

    const auto &binding = programObject->getFragmentInputBindingInfo(location);

    if (!binding.valid)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "No such binding."));
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
                    Error(GL_INVALID_OPERATION,
                          "Fragment input type is not a floating point scalar or vector."));
                return false;
        }
        if (expectedComponents != components && genMode != GL_NONE)
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Unexpected number of components"));
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
        context->handleError(
            Error(GL_INVALID_OPERATION, "GL_CHROMIUM_copy_texture extension not available."));
        return false;
    }

    const gl::Texture *source = context->getTexture(sourceId);
    if (source == nullptr)
    {
        context->handleError(
            Error(GL_INVALID_VALUE, "Source texture is not a valid texture object."));
        return false;
    }

    if (!IsValidCopyTextureSourceTarget(context, source->getTarget()))
    {
        context->handleError(Error(GL_INVALID_VALUE, "Source texture a valid texture type."));
        return false;
    }

    GLenum sourceTarget = source->getTarget();
    ASSERT(sourceTarget != GL_TEXTURE_CUBE_MAP);
    if (source->getWidth(sourceTarget, 0) == 0 || source->getHeight(sourceTarget, 0) == 0)
    {
        context->handleError(
            Error(GL_INVALID_VALUE, "Level 0 of the source texture must be defined."));
        return false;
    }

    const gl::Format &sourceFormat = source->getFormat(sourceTarget, 0);
    if (!IsValidCopyTextureFormat(context, sourceFormat.format))
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "Source texture internal format is invalid."));
        return false;
    }

    const gl::Texture *dest = context->getTexture(destId);
    if (dest == nullptr)
    {
        context->handleError(
            Error(GL_INVALID_VALUE, "Destination texture is not a valid texture object."));
        return false;
    }

    if (!IsValidCopyTextureDestinationTarget(context, dest->getTarget()))
    {
        context->handleError(Error(GL_INVALID_VALUE, "Destination texture a valid texture type."));
        return false;
    }

    if (!IsValidCopyTextureDestinationFormatType(context, internalFormat, destType))
    {
        context->handleError(
            Error(GL_INVALID_OPERATION,
                  "Destination internal format and type combination is not valid."));
        return false;
    }

    if (dest->getImmutableFormat())
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Destination texture is immutable."));
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
        context->handleError(
            Error(GL_INVALID_OPERATION, "GL_CHROMIUM_copy_texture extension not available."));
        return false;
    }

    const gl::Texture *source = context->getTexture(sourceId);
    if (source == nullptr)
    {
        context->handleError(
            Error(GL_INVALID_VALUE, "Source texture is not a valid texture object."));
        return false;
    }

    if (!IsValidCopyTextureSourceTarget(context, source->getTarget()))
    {
        context->handleError(Error(GL_INVALID_VALUE, "Source texture a valid texture type."));
        return false;
    }

    GLenum sourceTarget = source->getTarget();
    ASSERT(sourceTarget != GL_TEXTURE_CUBE_MAP);
    if (source->getWidth(sourceTarget, 0) == 0 || source->getHeight(sourceTarget, 0) == 0)
    {
        context->handleError(
            Error(GL_INVALID_VALUE, "Level 0 of the source texture must be defined."));
        return false;
    }

    if (x < 0 || y < 0)
    {
        context->handleError(Error(GL_INVALID_VALUE, "x and y cannot be negative."));
        return false;
    }

    if (width < 0 || height < 0)
    {
        context->handleError(Error(GL_INVALID_VALUE, "width and height cannot be negative."));
        return false;
    }

    if (static_cast<size_t>(x + width) > source->getWidth(sourceTarget, 0) ||
        static_cast<size_t>(y + height) > source->getHeight(sourceTarget, 0))
    {
        context->handleError(
            Error(GL_INVALID_VALUE, "Source texture not large enough to copy from."));
        return false;
    }

    const gl::Format &sourceFormat = source->getFormat(sourceTarget, 0);
    if (!IsValidCopyTextureFormat(context, sourceFormat.format))
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "Source texture internal format is invalid."));
        return false;
    }

    const gl::Texture *dest = context->getTexture(destId);
    if (dest == nullptr)
    {
        context->handleError(
            Error(GL_INVALID_VALUE, "Destination texture is not a valid texture object."));
        return false;
    }

    if (!IsValidCopyTextureDestinationTarget(context, dest->getTarget()))
    {
        context->handleError(Error(GL_INVALID_VALUE, "Destination texture a valid texture type."));
        return false;
    }

    ASSERT(destTarget != GL_TEXTURE_CUBE_MAP);
    if (dest->getWidth(sourceTarget, 0) == 0 || dest->getHeight(sourceTarget, 0) == 0)
    {
        context->handleError(
            Error(GL_INVALID_VALUE, "Level 0 of the destination texture must be defined."));
        return false;
    }

    const gl::Format &destFormat = dest->getFormat(destTarget, 0);
    if (!IsValidCopyTextureDestinationFormatType(context, destFormat.format, destFormat.type))
    {
        context->handleError(
            Error(GL_INVALID_OPERATION,
                  "Destination internal format and type combination is not valid."));
        return false;
    }

    if (xoffset < 0 || yoffset < 0)
    {
        context->handleError(Error(GL_INVALID_VALUE, "xoffset and yoffset cannot be negative."));
        return false;
    }

    if (static_cast<size_t>(xoffset + width) > dest->getWidth(destTarget, 0) ||
        static_cast<size_t>(yoffset + height) > dest->getHeight(destTarget, 0))
    {
        context->handleError(
            Error(GL_INVALID_VALUE, "Destination texture not large enough to copy to."));
        return false;
    }

    return true;
}

bool ValidateCompressedCopyTextureCHROMIUM(Context *context, GLuint sourceId, GLuint destId)
{
    if (!context->getExtensions().copyCompressedTexture)
    {
        context->handleError(Error(GL_INVALID_OPERATION,
                                   "GL_CHROMIUM_copy_compressed_texture extension not available."));
        return false;
    }

    const gl::Texture *source = context->getTexture(sourceId);
    if (source == nullptr)
    {
        context->handleError(
            Error(GL_INVALID_VALUE, "Source texture is not a valid texture object."));
        return false;
    }

    if (source->getTarget() != GL_TEXTURE_2D)
    {
        context->handleError(
            Error(GL_INVALID_VALUE, "Source texture must be of type GL_TEXTURE_2D."));
        return false;
    }

    if (source->getWidth(GL_TEXTURE_2D, 0) == 0 || source->getHeight(GL_TEXTURE_2D, 0) == 0)
    {
        context->handleError(Error(GL_INVALID_VALUE, "Source texture must level 0 defined."));
        return false;
    }

    const gl::Format &sourceFormat = source->getFormat(GL_TEXTURE_2D, 0);
    if (!sourceFormat.info->compressed)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "Source texture must have a compressed internal format."));
        return false;
    }

    const gl::Texture *dest = context->getTexture(destId);
    if (dest == nullptr)
    {
        context->handleError(
            Error(GL_INVALID_VALUE, "Destination texture is not a valid texture object."));
        return false;
    }

    if (dest->getTarget() != GL_TEXTURE_2D)
    {
        context->handleError(
            Error(GL_INVALID_VALUE, "Destination texture must be of type GL_TEXTURE_2D."));
        return false;
    }

    if (dest->getImmutableFormat())
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Destination cannot be immutable."));
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
                context->handleError(
                    Error(GL_INVALID_ENUM, "GL_COMPUTE_SHADER requires OpenGL ES 3.1."));
                return false;
            }
            break;

        default:
            context->handleError(Error(GL_INVALID_ENUM, "Unknown shader type."));
            return false;
    }

    return true;
}

bool ValidateBufferData(ValidationContext *context,
                        GLenum target,
                        GLsizeiptr size,
                        const GLvoid *data,
                        GLenum usage)
{
    if (size < 0)
    {
        context->handleError(Error(GL_INVALID_VALUE));
        return false;
    }

    switch (usage)
    {
        case GL_STREAM_DRAW:
        case GL_STATIC_DRAW:
        case GL_DYNAMIC_DRAW:
            break;

        case GL_STREAM_READ:
        case GL_STREAM_COPY:
        case GL_STATIC_READ:
        case GL_STATIC_COPY:
        case GL_DYNAMIC_READ:
        case GL_DYNAMIC_COPY:
            if (context->getClientMajorVersion() < 3)
            {
                context->handleError(Error(GL_INVALID_ENUM));
                return false;
            }
            break;

        default:
            context->handleError(Error(GL_INVALID_ENUM));
            return false;
    }

    if (!ValidBufferTarget(context, target))
    {
        context->handleError(Error(GL_INVALID_ENUM));
        return false;
    }

    Buffer *buffer = context->getGLState().getTargetBuffer(target);

    if (!buffer)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    return true;
}

bool ValidateBufferSubData(ValidationContext *context,
                           GLenum target,
                           GLintptr offset,
                           GLsizeiptr size,
                           const GLvoid *data)
{
    if (size < 0 || offset < 0)
    {
        context->handleError(Error(GL_INVALID_VALUE));
        return false;
    }

    if (!ValidBufferTarget(context, target))
    {
        context->handleError(Error(GL_INVALID_ENUM));
        return false;
    }

    Buffer *buffer = context->getGLState().getTargetBuffer(target);

    if (!buffer)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    if (buffer->isMapped())
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    // Check for possible overflow of size + offset
    angle::CheckedNumeric<size_t> checkedSize(size);
    checkedSize += offset;
    if (!checkedSize.IsValid())
    {
        context->handleError(Error(GL_OUT_OF_MEMORY));
        return false;
    }

    if (size + offset > buffer->getSize())
    {
        context->handleError(Error(GL_INVALID_VALUE));
        return false;
    }

    return true;
}

bool ValidateRequestExtensionANGLE(ValidationContext *context, const GLchar *name)
{
    if (!context->getExtensions().requestExtension)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "GL_ANGLE_request_extension is not available."));
        return false;
    }

    const ExtensionInfoMap &extensionInfos = GetExtensionInfoMap();
    auto extension                         = extensionInfos.find(name);
    if (extension == extensionInfos.end() || !extension->second.Requestable)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Extension %s is not requestable.", name));
        return false;
    }

    return true;
}

bool ValidateActiveTexture(ValidationContext *context, GLenum texture)
{
    if (texture < GL_TEXTURE0 ||
        texture > GL_TEXTURE0 + context->getCaps().maxCombinedTextureImageUnits - 1)
    {
        context->handleError(Error(GL_INVALID_ENUM));
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
                context->handleError(Error(GL_INVALID_OPERATION));
                return false;
            }
            break;
        }
        case GL_FRAGMENT_SHADER:
        {
            if (programObject->getAttachedFragmentShader())
            {
                context->handleError(Error(GL_INVALID_OPERATION));
                return false;
            }
            break;
        }
        case GL_COMPUTE_SHADER:
        {
            if (programObject->getAttachedComputeShader())
            {
                context->handleError(Error(GL_INVALID_OPERATION));
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
        context->handleError(Error(GL_INVALID_VALUE, "Index exceeds MAX_VERTEX_ATTRIBS"));
        return false;
    }

    if (strncmp(name, "gl_", 3) == 0)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Cannot Bind built-in attributes"));
        return false;
    }

    return GetValidProgram(context, program) != nullptr;
}

bool ValidateBindBuffer(ValidationContext *context, GLenum target, GLuint buffer)
{
    if (!ValidBufferTarget(context, target))
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid Buffer target"));
        return false;
    }

    if (!context->getGLState().isBindGeneratesResourceEnabled() &&
        !context->isBufferGenerated(buffer))
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Buffer was not generated"));
        return false;
    }

    return true;
}

bool ValidateBindFramebuffer(ValidationContext *context, GLenum target, GLuint framebuffer)
{
    if (!ValidFramebufferTarget(target))
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid Framebuffer target"));
        return false;
    }

    if (!context->getGLState().isBindGeneratesResourceEnabled() &&
        !context->isFramebufferGenerated(framebuffer))
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Framebuffer was not generated"));
        return false;
    }

    return true;
}

bool ValidateBindRenderbuffer(ValidationContext *context, GLenum target, GLuint renderbuffer)
{
    if (target != GL_RENDERBUFFER)
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid Renderbuffer target"));
        return false;
    }

    if (!context->getGLState().isBindGeneratesResourceEnabled() &&
        !context->isRenderbufferGenerated(renderbuffer))
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Renderbuffer was not generated"));
        return false;
    }

    return true;
}

static bool ValidBlendEquationMode(GLenum mode)
{
    switch (mode)
    {
        case GL_FUNC_ADD:
        case GL_FUNC_SUBTRACT:
        case GL_FUNC_REVERSE_SUBTRACT:
        case GL_MIN:
        case GL_MAX:
            return true;

        default:
            return false;
    }
}

bool ValidateBlendColor(ValidationContext *context,
                        GLclampf red,
                        GLclampf green,
                        GLclampf blue,
                        GLclampf alpha)
{
    return true;
}

bool ValidateBlendEquation(ValidationContext *context, GLenum mode)
{
    if (!ValidBlendEquationMode(mode))
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid blend equation"));
        return false;
    }

    return true;
}

bool ValidateBlendEquationSeparate(ValidationContext *context, GLenum modeRGB, GLenum modeAlpha)
{
    if (!ValidBlendEquationMode(modeRGB))
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid RGB blend equation"));
        return false;
    }

    if (!ValidBlendEquationMode(modeAlpha))
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid alpha blend equation"));
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
        context->handleError(Error(GL_INVALID_ENUM, "Invalid blend function"));
        return false;
    }

    if (!ValidDstBlendFunc(dstRGB, context->getClientMajorVersion()))
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid blend function"));
        return false;
    }

    if (!ValidSrcBlendFunc(srcAlpha))
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid blend function"));
        return false;
    }

    if (!ValidDstBlendFunc(dstAlpha, context->getClientMajorVersion()))
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid blend function"));
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
                msg =
                    "Invalid simultaneous use of GL_CONSTANT_ALPHA/GL_ONE_MINUS_CONSTANT_ALPHA and "
                    "GL_CONSTANT_COLOR/GL_ONE_MINUS_CONSTANT_COLOR.";
            }
            else
            {
                msg =
                    "Simultaneous use of GL_CONSTANT_ALPHA/GL_ONE_MINUS_CONSTANT_ALPHA and "
                    "GL_CONSTANT_COLOR/GL_ONE_MINUS_CONSTANT_COLOR not supported by this "
                    "implementation.";
                ERR() << msg;
            }
            context->handleError(Error(GL_INVALID_OPERATION, msg));
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
                context->handleError(Error(GL_INVALID_ENUM, "Invalid name."));
                return false;
            }
            break;

        default:
            context->handleError(Error(GL_INVALID_ENUM, "Invalid name."));
            return false;
    }

    return true;
}

bool ValidateLineWidth(ValidationContext *context, GLfloat width)
{
    if (width <= 0.0f || isNaN(width))
    {
        context->handleError(Error(GL_INVALID_VALUE, "Invalid width value."));
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
                                 const GLvoid *ptr)
{
    if (!ValidateVertexFormatBase(context, index, size, type, false))
    {
        return false;
    }

    if (stride < 0)
    {
        context->handleError(Error(GL_INVALID_VALUE, "stride cannot be negative."));
        return false;
    }

    const Caps &caps = context->getCaps();
    if (context->getClientVersion() >= ES_3_1)
    {
        if (stride > caps.maxVertexAttribStride)
        {
            context->handleError(
                Error(GL_INVALID_VALUE, "stride cannot be greater than MAX_VERTEX_ATTRIB_STRIDE."));
            return false;
        }

        if (index >= caps.maxVertexAttribBindings)
        {
            context->handleError(
                Error(GL_INVALID_VALUE, "index must be smaller than MAX_VERTEX_ATTRIB_BINDINGS."));
            return false;
        }
    }

    // [OpenGL ES 3.0.2] Section 2.8 page 24:
    // An INVALID_OPERATION error is generated when a non-zero vertex array object
    // is bound, zero is bound to the ARRAY_BUFFER buffer object binding point,
    // and the pointer argument is not NULL.
    bool nullBufferAllowed = context->getGLState().areClientArraysEnabled() &&
                             context->getGLState().getVertexArray()->id() == 0;
    if (!nullBufferAllowed && context->getGLState().getArrayBufferId() == 0 && ptr != nullptr)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION,
                  "Client data cannot be used with a non-default vertex array object."));
        return false;
    }

    if (context->getExtensions().webglCompatibility)
    {
        // WebGL 1.0 [Section 6.14] Fixed point support
        // The WebGL API does not support the GL_FIXED data type.
        if (type == GL_FIXED)
        {
            context->handleError(Error(GL_INVALID_ENUM, "GL_FIXED is not supported in WebGL."));
            return false;
        }

        if (!ValidateWebGLVertexAttribPointer(context, type, normalized, stride, ptr, false))
        {
            return false;
        }
    }

    return true;
}

bool ValidateDepthRangef(ValidationContext *context, GLclampf zNear, GLclampf zFar)
{
    if (context->getExtensions().webglCompatibility && zNear > zFar)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Depth near > far."));
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
        context->handleError(
            Error(GL_INVALID_OPERATION, "GL_ANGLE_framebuffer_multisample not available"));
        return false;
    }

    // ANGLE_framebuffer_multisample states that the value of samples must be less than or equal
    // to MAX_SAMPLES_ANGLE (Context::getCaps().maxSamples) otherwise GL_INVALID_OPERATION is
    // generated.
    if (static_cast<GLuint>(samples) > context->getCaps().maxSamples)
    {
        context->handleError(Error(GL_INVALID_VALUE));
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
            context->handleError(Error(GL_OUT_OF_MEMORY));
            return false;
        }
    }

    return ValidateRenderbufferStorageParametersBase(context, target, samples, internalformat,
                                                     width, height);
}

bool ValidateCheckFramebufferStatus(ValidationContext *context, GLenum target)
{
    if (!ValidFramebufferTarget(target))
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid Framebuffer target"));
        return false;
    }

    return true;
}

bool ValidateClearColor(ValidationContext *context,
                        GLclampf red,
                        GLclampf green,
                        GLclampf blue,
                        GLclampf alpha)
{
    return true;
}

bool ValidateClearDepthf(ValidationContext *context, GLclampf depth)
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

bool ValidateCullFace(ValidationContext *context, GLenum mode)
{
    switch (mode)
    {
        case GL_FRONT:
        case GL_BACK:
        case GL_FRONT_AND_BACK:
            break;

        default:
            context->handleError(Error(GL_INVALID_ENUM, "Invalid cull face parameter"));
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
            context->handleError(
                Error(GL_INVALID_OPERATION, "Shader handle passed to DeleteProgram"));
            return false;
        }
        else
        {
            context->handleError(Error(GL_INVALID_VALUE, "Invalid program handle"));
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
            context->handleError(
                Error(GL_INVALID_OPERATION, "Program handle passed to DeleteShader"));
            return false;
        }
        else
        {
            context->handleError(Error(GL_INVALID_VALUE, "Invalid shader handle"));
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
            context->handleError(Error(GL_INVALID_ENUM, "Invalid depth function"));
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
        default:
            UNREACHABLE();
            return false;
    }

    if (attachedShader != shaderObject)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Cannot detach non-attached shader."));
        return false;
    }

    return true;
}

bool ValidateDisableVertexAttribArray(ValidationContext *context, GLuint index)
{
    if (index >= MAX_VERTEX_ATTRIBS)
    {
        context->handleError(Error(GL_INVALID_VALUE, "Attrib array index out of range"));
        return false;
    }

    return true;
}

bool ValidateEnableVertexAttribArray(ValidationContext *context, GLuint index)
{
    if (index >= MAX_VERTEX_ATTRIBS)
    {
        context->handleError(Error(GL_INVALID_VALUE, "Attrib array index out of range"));
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
            context->handleError(Error(GL_INVALID_ENUM, "Invalid mode for FrontFace"));
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
        context->handleError(Error(GL_INVALID_VALUE, "bufsize must be non-negative."));
        return false;
    }

    Program *programObject = GetValidProgram(context, program);

    if (!programObject)
    {
        return false;
    }

    if (index >= static_cast<GLuint>(programObject->getActiveAttributeCount()))
    {
        context->handleError(
            Error(GL_INVALID_VALUE, "index exeeds program active attribute count."));
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
        context->handleError(Error(GL_INVALID_VALUE, "bufsize must be non-negative."));
        return false;
    }

    Program *programObject = GetValidProgram(context, program);

    if (!programObject)
    {
        return false;
    }

    if (index >= static_cast<GLuint>(programObject->getActiveUniformCount()))
    {
        context->handleError(
            Error(GL_INVALID_VALUE, "index exceeds program active uniform count."));
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
        context->handleError(Error(GL_INVALID_VALUE, "max count must be non-negative."));
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
    Program *programObject = GetValidProgram(context, program);

    if (!programObject)
    {
        return false;
    }

    if (!programObject->isLinked())
    {
        context->handleError(Error(GL_INVALID_OPERATION, "program not linked."));
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
        context->handleError(Error(GL_INVALID_VALUE, "bufsize must be non-negative."));
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
        context->handleError(Error(GL_INVALID_VALUE, "bufsize must be non-negative."));
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
            context->handleError(
                Error(GL_INVALID_OPERATION, "compute shader precision not yet implemented."));
            return false;
        default:
            context->handleError(Error(GL_INVALID_ENUM, "invalid shader type."));
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
            context->handleError(Error(GL_INVALID_ENUM, "invalid precision type."));
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
        context->handleError(Error(GL_INVALID_VALUE, "bufsize must be non-negative."));
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

    Program *programObject = GetValidProgram(context, program);

    if (!programObject)
    {
        return false;
    }

    if (!programObject->isLinked())
    {
        context->handleError(Error(GL_INVALID_OPERATION, "program is not linked."));
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
            context->handleError(Error(GL_INVALID_ENUM, "invalid hint mode."));
            return false;
    }

    switch (target)
    {
        case GL_GENERATE_MIPMAP_HINT:
            break;

        case GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES:
            if (!context->getExtensions().standardDerivatives)
            {
                context->handleError(
                    Error(GL_INVALID_OPERATION, "hint requires OES_standard_derivatives."));
                return false;
            }
            break;

        default:
            context->handleError(Error(GL_INVALID_ENUM, "invalid hint target."));
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
                context->handleError(Error(GL_INVALID_ENUM));
                return false;

            case GL_UNPACK_ROW_LENGTH:
            case GL_UNPACK_SKIP_ROWS:
            case GL_UNPACK_SKIP_PIXELS:
                if (!context->getExtensions().unpackSubimage)
                {
                    context->handleError(Error(GL_INVALID_ENUM));
                    return false;
                }
                break;

            case GL_PACK_ROW_LENGTH:
            case GL_PACK_SKIP_ROWS:
            case GL_PACK_SKIP_PIXELS:
                if (!context->getExtensions().packSubimage)
                {
                    context->handleError(Error(GL_INVALID_ENUM));
                    return false;
                }
                break;
        }
    }

    if (param < 0)
    {
        context->handleError(Error(GL_INVALID_VALUE, "Cannot use negative values in PixelStorei"));
        return false;
    }

    switch (pname)
    {
        case GL_UNPACK_ALIGNMENT:
            if (param != 1 && param != 2 && param != 4 && param != 8)
            {
                context->handleError(Error(GL_INVALID_VALUE));
                return false;
            }
            break;

        case GL_PACK_ALIGNMENT:
            if (param != 1 && param != 2 && param != 4 && param != 8)
            {
                context->handleError(Error(GL_INVALID_VALUE));
                return false;
            }
            break;

        case GL_PACK_REVERSE_ROW_ORDER_ANGLE:
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
            context->handleError(Error(GL_INVALID_ENUM));
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

bool ValidateSampleCoverage(ValidationContext *context, GLclampf value, GLboolean invert)
{
    return true;
}

bool ValidateScissor(ValidationContext *context, GLint x, GLint y, GLsizei width, GLsizei height)
{
    if (width < 0 || height < 0)
    {
        context->handleError(
            Error(GL_INVALID_VALUE, "Scissor width and height must be non-negative."));
        return false;
    }

    return true;
}

bool ValidateShaderBinary(ValidationContext *context,
                          GLsizei n,
                          const GLuint *shaders,
                          GLenum binaryformat,
                          const GLvoid *binary,
                          GLsizei length)
{
    const std::vector<GLenum> &shaderBinaryFormats = context->getCaps().shaderBinaryFormats;
    if (std::find(shaderBinaryFormats.begin(), shaderBinaryFormats.end(), binaryformat) ==
        shaderBinaryFormats.end())
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid shader binary format."));
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
        context->handleError(Error(GL_INVALID_VALUE, "Count must be non-negative."));
        return false;
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
        context->handleError(Error(GL_INVALID_ENUM, "Invalid stencil function."));
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
        context->handleError(Error(GL_INVALID_ENUM, "Invalid stencil face."));
        return false;
    }

    if (!IsValidStencilFunc(func))
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid stencil function."));
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
        context->handleError(Error(GL_INVALID_ENUM, "Invalid stencil face."));
        return false;
    }

    return true;
}

bool ValidateStencilOp(ValidationContext *context, GLenum fail, GLenum zfail, GLenum zpass)
{
    if (!IsValidStencilOp(fail))
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid stencil fail op."));
        return false;
    }

    if (!IsValidStencilOp(zfail))
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid stencil z fail op."));
        return false;
    }

    if (!IsValidStencilOp(zpass))
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid stencil z pass op."));
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
        context->handleError(Error(GL_INVALID_ENUM, "Invalid stencil face."));
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

bool ValidateVertexAttribIndex(ValidationContext *context, GLuint index)
{
    if (index >= MAX_VERTEX_ATTRIBS)
    {
        context->handleError(Error(GL_INVALID_VALUE, "Vertex attrib index out of range."));
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
        context->handleError(
            Error(GL_INVALID_VALUE, "Viewport width and height must be non-negative."));
        return false;
    }

    return true;
}

bool ValidateDrawArrays(ValidationContext *context, GLenum mode, GLint first, GLsizei count)
{
    return ValidateDrawArraysCommon(context, mode, first, count, 1);
}

}  // namespace gl
