//
// Copyright (c) 2013-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationES3.cpp: Validation functions for OpenGL ES 3.0 entry point parameters

#include "libANGLE/validationES3.h"

#include "base/numerics/safe_conversions.h"
#include "common/mathutil.h"
#include "common/utilities.h"
#include "libANGLE/validationES.h"
#include "libANGLE/Context.h"
#include "libANGLE/Texture.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/Renderbuffer.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/FramebufferAttachment.h"

using namespace angle;

namespace gl
{

static bool ValidateTexImageFormatCombination(gl::Context *context,
                                              GLenum internalFormat,
                                              GLenum format,
                                              GLenum type)
{
    // For historical reasons, glTexImage2D and glTexImage3D pass in their internal format as a
    // GLint instead of a GLenum. Therefor an invalid internal format gives a GL_INVALID_VALUE
    // error instead of a GL_INVALID_ENUM error. As this validation function is only called in
    // the validation codepaths for glTexImage2D/3D, we record a GL_INVALID_VALUE error.
    const gl::InternalFormat &formatInfo = gl::GetInternalFormatInfo(internalFormat);
    if (!formatInfo.textureSupport(context->getClientVersion(), context->getExtensions()))
    {
        context->handleError(Error(GL_INVALID_VALUE));
        return false;
    }

    // The type and format are valid if any supported internal format has that type and format
    if (!ValidES3Format(format) || !ValidES3Type(type))
    {
        context->handleError(Error(GL_INVALID_ENUM));
        return false;
    }

    // Check if this is a valid format combination to load texture data
    if (!ValidES3FormatCombination(format, type, internalFormat))
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    return true;
}

bool ValidateES3TexImageParametersBase(Context *context,
                                       GLenum target,
                                       GLint level,
                                       GLenum internalformat,
                                       bool isCompressed,
                                       bool isSubImage,
                                       GLint xoffset,
                                       GLint yoffset,
                                       GLint zoffset,
                                       GLsizei width,
                                       GLsizei height,
                                       GLsizei depth,
                                       GLint border,
                                       GLenum format,
                                       GLenum type,
                                       GLsizei imageSize,
                                       const GLvoid *pixels)
{
    // Validate image size
    if (!ValidImageSizeParameters(context, target, level, width, height, depth, isSubImage))
    {
        context->handleError(Error(GL_INVALID_VALUE));
        return false;
    }

    // Verify zero border
    if (border != 0)
    {
        context->handleError(Error(GL_INVALID_VALUE));
        return false;
    }

    if (xoffset < 0 || yoffset < 0 || zoffset < 0 ||
        std::numeric_limits<GLsizei>::max() - xoffset < width ||
        std::numeric_limits<GLsizei>::max() - yoffset < height ||
        std::numeric_limits<GLsizei>::max() - zoffset < depth)
    {
        context->handleError(Error(GL_INVALID_VALUE));
        return false;
    }

    const gl::Caps &caps = context->getCaps();

    switch (target)
    {
        case GL_TEXTURE_2D:
            if (static_cast<GLuint>(width) > (caps.max2DTextureSize >> level) ||
                static_cast<GLuint>(height) > (caps.max2DTextureSize >> level))
            {
                context->handleError(Error(GL_INVALID_VALUE));
                return false;
            }
            break;

        case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
            if (!isSubImage && width != height)
            {
                context->handleError(Error(GL_INVALID_VALUE));
                return false;
            }

            if (static_cast<GLuint>(width) > (caps.maxCubeMapTextureSize >> level))
            {
                context->handleError(Error(GL_INVALID_VALUE));
                return false;
            }
            break;

        case GL_TEXTURE_3D:
            if (static_cast<GLuint>(width) > (caps.max3DTextureSize >> level) ||
                static_cast<GLuint>(height) > (caps.max3DTextureSize >> level) ||
                static_cast<GLuint>(depth) > (caps.max3DTextureSize >> level))
            {
                context->handleError(Error(GL_INVALID_VALUE));
                return false;
            }
            break;

        case GL_TEXTURE_2D_ARRAY:
            if (static_cast<GLuint>(width) > (caps.max2DTextureSize >> level) ||
                static_cast<GLuint>(height) > (caps.max2DTextureSize >> level) ||
                static_cast<GLuint>(depth) > caps.maxArrayTextureLayers)
            {
                context->handleError(Error(GL_INVALID_VALUE));
                return false;
            }
            break;

        default:
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

    if (texture->getImmutableFormat() && !isSubImage)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    // Validate texture formats
    GLenum actualInternalFormat =
        isSubImage ? texture->getFormat(target, level).asSized() : internalformat;
    if (isSubImage && actualInternalFormat == GL_NONE)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Texture level does not exist."));
        return false;
    }

    const gl::InternalFormat &actualFormatInfo = gl::GetInternalFormatInfo(actualInternalFormat);
    if (isCompressed)
    {
        if (!actualFormatInfo.compressed)
        {
            context->handleError(Error(
                GL_INVALID_ENUM, "internalformat is not a supported compressed internal format."));
            return false;
        }

        if (!ValidCompressedImageSize(context, actualInternalFormat, xoffset, yoffset, width,
                                      height))
        {
            context->handleError(Error(GL_INVALID_OPERATION));
            return false;
        }

        if (!actualFormatInfo.textureSupport(context->getClientVersion(), context->getExtensions()))
        {
            context->handleError(Error(GL_INVALID_ENUM));
            return false;
        }

        if (target == GL_TEXTURE_3D)
        {
            context->handleError(Error(GL_INVALID_OPERATION));
            return false;
        }
    }
    else
    {
        if (!ValidateTexImageFormatCombination(context, actualInternalFormat, format, type))
        {
            return false;
        }

        if (target == GL_TEXTURE_3D && (format == GL_DEPTH_COMPONENT || format == GL_DEPTH_STENCIL))
        {
            context->handleError(Error(GL_INVALID_OPERATION));
            return false;
        }
    }

    // Validate sub image parameters
    if (isSubImage)
    {
        if (isCompressed != actualFormatInfo.compressed)
        {
            context->handleError(Error(GL_INVALID_OPERATION));
            return false;
        }

        if (width == 0 || height == 0 || depth == 0)
        {
            return false;
        }

        if (xoffset < 0 || yoffset < 0 || zoffset < 0)
        {
            context->handleError(Error(GL_INVALID_VALUE));
            return false;
        }

        if (std::numeric_limits<GLsizei>::max() - xoffset < width ||
            std::numeric_limits<GLsizei>::max() - yoffset < height ||
            std::numeric_limits<GLsizei>::max() - zoffset < depth)
        {
            context->handleError(Error(GL_INVALID_VALUE));
            return false;
        }

        if (static_cast<size_t>(xoffset + width) > texture->getWidth(target, level) ||
            static_cast<size_t>(yoffset + height) > texture->getHeight(target, level) ||
            static_cast<size_t>(zoffset + depth) > texture->getDepth(target, level))
        {
            context->handleError(Error(GL_INVALID_VALUE));
            return false;
        }
    }

    if (!ValidImageDataSize(context, target, width, height, 1, actualInternalFormat, type, pixels,
                            imageSize))
    {
        return false;
    }

    // Check for pixel unpack buffer related API errors
    gl::Buffer *pixelUnpackBuffer = context->getGLState().getTargetBuffer(GL_PIXEL_UNPACK_BUFFER);
    if (pixelUnpackBuffer != nullptr)
    {
        // ...data is not evenly divisible into the number of bytes needed to store in memory a
        // datum
        // indicated by type.
        if (!isCompressed)
        {
            size_t offset            = reinterpret_cast<size_t>(pixels);
            size_t dataBytesPerPixel = static_cast<size_t>(gl::GetTypeInfo(type).bytes);

            if ((offset % dataBytesPerPixel) != 0)
            {
                context->handleError(
                    Error(GL_INVALID_OPERATION, "Reads would overflow the pixel unpack buffer."));
                return false;
            }
        }

        // ...the buffer object's data store is currently mapped.
        if (pixelUnpackBuffer->isMapped())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Pixel unpack buffer is mapped."));
            return false;
        }
    }

    return true;
}

bool ValidateES3TexImage2DParameters(Context *context,
                                     GLenum target,
                                     GLint level,
                                     GLenum internalformat,
                                     bool isCompressed,
                                     bool isSubImage,
                                     GLint xoffset,
                                     GLint yoffset,
                                     GLint zoffset,
                                     GLsizei width,
                                     GLsizei height,
                                     GLsizei depth,
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

    return ValidateES3TexImageParametersBase(context, target, level, internalformat, isCompressed,
                                             isSubImage, xoffset, yoffset, zoffset, width, height,
                                             depth, border, format, type, imageSize, pixels);
}

bool ValidateES3TexImage3DParameters(Context *context,
                                     GLenum target,
                                     GLint level,
                                     GLenum internalformat,
                                     bool isCompressed,
                                     bool isSubImage,
                                     GLint xoffset,
                                     GLint yoffset,
                                     GLint zoffset,
                                     GLsizei width,
                                     GLsizei height,
                                     GLsizei depth,
                                     GLint border,
                                     GLenum format,
                                     GLenum type,
                                     GLsizei bufSize,
                                     const GLvoid *pixels)
{
    if (!ValidTexture3DDestinationTarget(context, target))
    {
        context->handleError(Error(GL_INVALID_ENUM));
        return false;
    }

    return ValidateES3TexImageParametersBase(context, target, level, internalformat, isCompressed,
                                             isSubImage, xoffset, yoffset, zoffset, width, height,
                                             depth, border, format, type, bufSize, pixels);
}

struct EffectiveInternalFormatInfo
{
    GLenum effectiveFormat;
    GLenum destFormat;
    GLuint minRedBits;
    GLuint maxRedBits;
    GLuint minGreenBits;
    GLuint maxGreenBits;
    GLuint minBlueBits;
    GLuint maxBlueBits;
    GLuint minAlphaBits;
    GLuint maxAlphaBits;
};

static bool QueryEffectiveFormatList(const InternalFormat &srcFormat,
                                     GLenum targetFormat,
                                     const EffectiveInternalFormatInfo *list,
                                     size_t size,
                                     GLenum *outEffectiveFormat)
{
    for (size_t curFormat = 0; curFormat < size; ++curFormat)
    {
        const EffectiveInternalFormatInfo &formatInfo = list[curFormat];
        if ((formatInfo.destFormat == targetFormat) &&
            (formatInfo.minRedBits <= srcFormat.redBits &&
             formatInfo.maxRedBits >= srcFormat.redBits) &&
            (formatInfo.minGreenBits <= srcFormat.greenBits &&
             formatInfo.maxGreenBits >= srcFormat.greenBits) &&
            (formatInfo.minBlueBits <= srcFormat.blueBits &&
             formatInfo.maxBlueBits >= srcFormat.blueBits) &&
            (formatInfo.minAlphaBits <= srcFormat.alphaBits &&
             formatInfo.maxAlphaBits >= srcFormat.alphaBits))
        {
            *outEffectiveFormat = formatInfo.effectiveFormat;
            return true;
        }
    }

    *outEffectiveFormat = GL_NONE;
    return false;
}

bool GetSizedEffectiveInternalFormatInfo(const InternalFormat &srcFormat,
                                         GLenum *outEffectiveFormat)
{
    // OpenGL ES 3.0.3 Specification, Table 3.17, pg 141:
    // Effective internal format coresponding to destination internal format and linear source
    // buffer component sizes.
    //                                       | Source channel min/max sizes |
    //   Effective Internal Format   |  N/A  |  R   |  G   |  B   |  A      |
    // clang-format off
    constexpr EffectiveInternalFormatInfo list[] = {
        { GL_ALPHA8_EXT,              GL_NONE, 0,  0, 0,  0, 0,  0, 1, 8 },
        { GL_R8,                      GL_NONE, 1,  8, 0,  0, 0,  0, 0, 0 },
        { GL_RG8,                     GL_NONE, 1,  8, 1,  8, 0,  0, 0, 0 },
        { GL_RGB565,                  GL_NONE, 1,  5, 1,  6, 1,  5, 0, 0 },
        { GL_RGB8,                    GL_NONE, 6,  8, 7,  8, 6,  8, 0, 0 },
        { GL_RGBA4,                   GL_NONE, 1,  4, 1,  4, 1,  4, 1, 4 },
        { GL_RGB5_A1,                 GL_NONE, 5,  5, 5,  5, 5,  5, 1, 1 },
        { GL_RGBA8,                   GL_NONE, 5,  8, 5,  8, 5,  8, 2, 8 },
        { GL_RGB10_A2,                GL_NONE, 9, 10, 9, 10, 9, 10, 2, 2 },
    };
    // clang-format on

    return QueryEffectiveFormatList(srcFormat, GL_NONE, list, ArraySize(list), outEffectiveFormat);
}

bool GetUnsizedEffectiveInternalFormatInfo(const InternalFormat &srcFormat,
                                           const InternalFormat &destFormat,
                                           GLenum *outEffectiveFormat)
{
    constexpr GLuint umax = UINT_MAX;

    // OpenGL ES 3.0.3 Specification, Table 3.17, pg 141:
    // Effective internal format coresponding to destination internal format andlinear source buffer
    // component sizes.
    //                                                   |   Source channel min/max sizes   |
    //     Effective Internal Format |   Dest Format     |   R   |    G   |    B   |    A   |
    // clang-format off
    constexpr EffectiveInternalFormatInfo list[] = {
        { GL_ALPHA8_EXT,             GL_ALPHA,           0, umax, 0, umax, 0, umax, 1,    8 },
        { GL_LUMINANCE8_EXT,         GL_LUMINANCE,       1,    8, 0, umax, 0, umax, 0, umax },
        { GL_LUMINANCE8_ALPHA8_EXT,  GL_LUMINANCE_ALPHA, 1,    8, 0, umax, 0, umax, 1,    8 },
        { GL_RGB565,                 GL_RGB,             1,    5, 1,    6, 1,    5, 0, umax },
        { GL_RGB8,                   GL_RGB,             6,    8, 7,    8, 6,    8, 0, umax },
        { GL_RGBA4,                  GL_RGBA,            1,    4, 1,    4, 1,    4, 1,    4 },
        { GL_RGB5_A1,                GL_RGBA,            5,    5, 5,    5, 5,    5, 1,    1 },
        { GL_RGBA8,                  GL_RGBA,            5,    8, 5,    8, 5,    8, 5,    8 },
    };
    // clang-format on

    return QueryEffectiveFormatList(srcFormat, destFormat.format, list, ArraySize(list),
                                    outEffectiveFormat);
}

static bool GetEffectiveInternalFormat(const InternalFormat &srcFormat,
                                       const InternalFormat &destFormat,
                                       GLenum *outEffectiveFormat)
{
    if (destFormat.pixelBytes > 0)
    {
        return GetSizedEffectiveInternalFormatInfo(srcFormat, outEffectiveFormat);
    }
    else
    {
        return GetUnsizedEffectiveInternalFormatInfo(srcFormat, destFormat, outEffectiveFormat);
    }
}

static bool EqualOrFirstZero(GLuint first, GLuint second)
{
    return first == 0 || first == second;
}

static bool IsValidES3CopyTexImageCombination(const Format &textureFormat,
                                              const Format &framebufferFormat,
                                              GLuint readBufferHandle)
{
    const auto &textureFormatInfo     = *textureFormat.info;
    const auto &framebufferFormatInfo = *framebufferFormat.info;

    if (!ValidES3CopyConversion(textureFormatInfo.format, framebufferFormatInfo.format))
    {
        return false;
    }

    // Section 3.8.5 of the GLES 3.0.3 spec states that source and destination formats
    // must both be signed, unsigned, or fixed point and both source and destinations
    // must be either both SRGB or both not SRGB. EXT_color_buffer_float adds allowed
    // conversion between fixed and floating point.

    if ((textureFormatInfo.colorEncoding == GL_SRGB) !=
        (framebufferFormatInfo.colorEncoding == GL_SRGB))
    {
        return false;
    }

    if (((textureFormatInfo.componentType == GL_INT) !=
         (framebufferFormatInfo.componentType == GL_INT)) ||
        ((textureFormatInfo.componentType == GL_UNSIGNED_INT) !=
         (framebufferFormatInfo.componentType == GL_UNSIGNED_INT)))
    {
        return false;
    }

    if ((textureFormatInfo.componentType == GL_UNSIGNED_NORMALIZED ||
         textureFormatInfo.componentType == GL_SIGNED_NORMALIZED ||
         textureFormatInfo.componentType == GL_FLOAT) &&
        !(framebufferFormatInfo.componentType == GL_UNSIGNED_NORMALIZED ||
          framebufferFormatInfo.componentType == GL_SIGNED_NORMALIZED ||
          framebufferFormatInfo.componentType == GL_FLOAT))
    {
        return false;
    }

    // GLES specification 3.0.3, sec 3.8.5, pg 139-140:
    // The effective internal format of the source buffer is determined with the following rules
    // applied in order:
    //    * If the source buffer is a texture or renderbuffer that was created with a sized internal
    //      format then the effective internal format is the source buffer's sized internal format.
    //    * If the source buffer is a texture that was created with an unsized base internal format,
    //      then the effective internal format is the source image array's effective internal
    //      format, as specified by table 3.12, which is determined from the <format> and <type>
    //      that were used when the source image array was specified by TexImage*.
    //    * Otherwise the effective internal format is determined by the row in table 3.17 or 3.18
    //      where Destination Internal Format matches internalformat and where the [source channel
    //      sizes] are consistent with the values of the source buffer's [channel sizes]. Table 3.17
    //      is used if the FRAMEBUFFER_ATTACHMENT_ENCODING is LINEAR and table 3.18 is used if the
    //      FRAMEBUFFER_ATTACHMENT_ENCODING is SRGB.
    const InternalFormat *sourceEffectiveFormat = NULL;
    if (readBufferHandle != 0)
    {
        // Not the default framebuffer, therefore the read buffer must be a user-created texture or
        // renderbuffer
        if (framebufferFormat.sized)
        {
            sourceEffectiveFormat = &framebufferFormatInfo;
        }
        else
        {
            // Renderbuffers cannot be created with an unsized internal format, so this must be an
            // unsized-format texture. We can use the same table we use when creating textures to
            // get its effective sized format.
            GLenum sizedInternalFormat =
                GetSizedInternalFormat(framebufferFormatInfo.format, framebufferFormatInfo.type);
            sourceEffectiveFormat = &GetInternalFormatInfo(sizedInternalFormat);
        }
    }
    else
    {
        // The effective internal format must be derived from the source framebuffer's channel
        // sizes. This is done in GetEffectiveInternalFormat for linear buffers (table 3.17)
        if (framebufferFormatInfo.colorEncoding == GL_LINEAR)
        {
            GLenum effectiveFormat;
            if (GetEffectiveInternalFormat(framebufferFormatInfo, textureFormatInfo,
                                           &effectiveFormat))
            {
                sourceEffectiveFormat = &GetInternalFormatInfo(effectiveFormat);
            }
            else
            {
                return false;
            }
        }
        else if (framebufferFormatInfo.colorEncoding == GL_SRGB)
        {
            // SRGB buffers can only be copied to sized format destinations according to table 3.18
            if (textureFormat.sized &&
                (framebufferFormatInfo.redBits >= 1 && framebufferFormatInfo.redBits <= 8) &&
                (framebufferFormatInfo.greenBits >= 1 && framebufferFormatInfo.greenBits <= 8) &&
                (framebufferFormatInfo.blueBits >= 1 && framebufferFormatInfo.blueBits <= 8) &&
                (framebufferFormatInfo.alphaBits >= 1 && framebufferFormatInfo.alphaBits <= 8))
            {
                sourceEffectiveFormat = &GetInternalFormatInfo(GL_SRGB8_ALPHA8);
            }
            else
            {
                return false;
            }
        }
        else
        {
            UNREACHABLE();
            return false;
        }
    }

    if (textureFormat.sized)
    {
        // Section 3.8.5 of the GLES 3.0.3 spec, pg 139, requires that, if the destination format is
        // sized, component sizes of the source and destination formats must exactly match if the
        // destination format exists.
        if (!EqualOrFirstZero(textureFormatInfo.redBits, sourceEffectiveFormat->redBits) ||
            !EqualOrFirstZero(textureFormatInfo.greenBits, sourceEffectiveFormat->greenBits) ||
            !EqualOrFirstZero(textureFormatInfo.blueBits, sourceEffectiveFormat->blueBits) ||
            !EqualOrFirstZero(textureFormatInfo.alphaBits, sourceEffectiveFormat->alphaBits))
        {
            return false;
        }
    }

    return true;  // A conversion function exists, and no rule in the specification has precluded
                  // conversion between these formats.
}

bool ValidateES3CopyTexImageParametersBase(ValidationContext *context,
                                           GLenum target,
                                           GLint level,
                                           GLenum internalformat,
                                           bool isSubImage,
                                           GLint xoffset,
                                           GLint yoffset,
                                           GLint zoffset,
                                           GLint x,
                                           GLint y,
                                           GLsizei width,
                                           GLsizei height,
                                           GLint border)
{
    Format textureFormat = Format::Invalid();
    if (!ValidateCopyTexImageParametersBase(context, target, level, internalformat, isSubImage,
                                            xoffset, yoffset, zoffset, x, y, width, height, border,
                                            &textureFormat))
    {
        return false;
    }
    ASSERT(textureFormat.valid() || !isSubImage);

    const auto &state            = context->getGLState();
    gl::Framebuffer *framebuffer = state.getReadFramebuffer();
    GLuint readFramebufferID     = framebuffer->id();

    if (framebuffer->checkStatus(context) != GL_FRAMEBUFFER_COMPLETE)
    {
        context->handleError(Error(GL_INVALID_FRAMEBUFFER_OPERATION));
        return false;
    }

    if (readFramebufferID != 0 && framebuffer->getSamples(context) != 0)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    const FramebufferAttachment *source = framebuffer->getReadColorbuffer();

    if (isSubImage)
    {
        if (!IsValidES3CopyTexImageCombination(textureFormat, source->getFormat(),
                                               readFramebufferID))
        {
            context->handleError(Error(GL_INVALID_OPERATION));
            return false;
        }
    }
    else
    {
        // Use format/type from the source FBO. (Might not be perfect for all cases?)
        const auto framebufferFormat = source->getFormat();
        Format copyFormat(internalformat, framebufferFormat.format, framebufferFormat.type);
        if (!IsValidES3CopyTexImageCombination(copyFormat, framebufferFormat, readFramebufferID))
        {
            context->handleError(Error(GL_INVALID_OPERATION));
            return false;
        }
    }

    // If width or height is zero, it is a no-op.  Return false without setting an error.
    return (width > 0 && height > 0);
}

bool ValidateES3CopyTexImage2DParameters(ValidationContext *context,
                                         GLenum target,
                                         GLint level,
                                         GLenum internalformat,
                                         bool isSubImage,
                                         GLint xoffset,
                                         GLint yoffset,
                                         GLint zoffset,
                                         GLint x,
                                         GLint y,
                                         GLsizei width,
                                         GLsizei height,
                                         GLint border)
{
    if (!ValidTexture2DDestinationTarget(context, target))
    {
        context->handleError(Error(GL_INVALID_ENUM));
        return false;
    }

    return ValidateES3CopyTexImageParametersBase(context, target, level, internalformat, isSubImage,
                                                 xoffset, yoffset, zoffset, x, y, width, height,
                                                 border);
}

bool ValidateES3CopyTexImage3DParameters(ValidationContext *context,
                                         GLenum target,
                                         GLint level,
                                         GLenum internalformat,
                                         bool isSubImage,
                                         GLint xoffset,
                                         GLint yoffset,
                                         GLint zoffset,
                                         GLint x,
                                         GLint y,
                                         GLsizei width,
                                         GLsizei height,
                                         GLint border)
{
    if (!ValidTexture3DDestinationTarget(context, target))
    {
        context->handleError(Error(GL_INVALID_ENUM));
        return false;
    }

    return ValidateES3CopyTexImageParametersBase(context, target, level, internalformat, isSubImage,
                                                 xoffset, yoffset, zoffset, x, y, width, height,
                                                 border);
}

bool ValidateES3TexStorageParametersBase(Context *context,
                                         GLenum target,
                                         GLsizei levels,
                                         GLenum internalformat,
                                         GLsizei width,
                                         GLsizei height,
                                         GLsizei depth)
{
    if (width < 1 || height < 1 || depth < 1 || levels < 1)
    {
        context->handleError(Error(GL_INVALID_VALUE));
        return false;
    }

    GLsizei maxDim = std::max(width, height);
    if (target != GL_TEXTURE_2D_ARRAY)
    {
        maxDim = std::max(maxDim, depth);
    }

    if (levels > gl::log2(maxDim) + 1)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    const gl::Caps &caps = context->getCaps();

    switch (target)
    {
        case GL_TEXTURE_2D:
        {
            if (static_cast<GLuint>(width) > caps.max2DTextureSize ||
                static_cast<GLuint>(height) > caps.max2DTextureSize)
            {
                context->handleError(Error(GL_INVALID_VALUE));
                return false;
            }
        }
        break;

        case GL_TEXTURE_CUBE_MAP:
        {
            if (width != height)
            {
                context->handleError(Error(GL_INVALID_VALUE));
                return false;
            }

            if (static_cast<GLuint>(width) > caps.maxCubeMapTextureSize)
            {
                context->handleError(Error(GL_INVALID_VALUE));
                return false;
            }
        }
        break;

        case GL_TEXTURE_3D:
        {
            if (static_cast<GLuint>(width) > caps.max3DTextureSize ||
                static_cast<GLuint>(height) > caps.max3DTextureSize ||
                static_cast<GLuint>(depth) > caps.max3DTextureSize)
            {
                context->handleError(Error(GL_INVALID_VALUE));
                return false;
            }
        }
        break;

        case GL_TEXTURE_2D_ARRAY:
        {
            if (static_cast<GLuint>(width) > caps.max2DTextureSize ||
                static_cast<GLuint>(height) > caps.max2DTextureSize ||
                static_cast<GLuint>(depth) > caps.maxArrayTextureLayers)
            {
                context->handleError(Error(GL_INVALID_VALUE));
                return false;
            }
        }
        break;

        default:
            UNREACHABLE();
            return false;
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

    const gl::InternalFormat &formatInfo = gl::GetInternalFormatInfo(internalformat);
    if (!formatInfo.textureSupport(context->getClientVersion(), context->getExtensions()))
    {
        context->handleError(Error(GL_INVALID_ENUM));
        return false;
    }

    if (formatInfo.pixelBytes == 0)
    {
        context->handleError(Error(GL_INVALID_ENUM));
        return false;
    }

    return true;
}

bool ValidateES3TexStorage2DParameters(Context *context,
                                       GLenum target,
                                       GLsizei levels,
                                       GLenum internalformat,
                                       GLsizei width,
                                       GLsizei height,
                                       GLsizei depth)
{
    if (!ValidTexture2DTarget(context, target))
    {
        context->handleError(Error(GL_INVALID_ENUM));
        return false;
    }

    return ValidateES3TexStorageParametersBase(context, target, levels, internalformat, width,
                                               height, depth);
}

bool ValidateES3TexStorage3DParameters(Context *context,
                                       GLenum target,
                                       GLsizei levels,
                                       GLenum internalformat,
                                       GLsizei width,
                                       GLsizei height,
                                       GLsizei depth)
{
    if (!ValidTexture3DTarget(context, target))
    {
        context->handleError(Error(GL_INVALID_ENUM));
        return false;
    }

    return ValidateES3TexStorageParametersBase(context, target, levels, internalformat, width,
                                               height, depth);
}

bool ValidateBeginQuery(gl::Context *context, GLenum target, GLuint id)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "GLES version < 3.0"));
        return false;
    }

    return ValidateBeginQueryBase(context, target, id);
}

bool ValidateEndQuery(gl::Context *context, GLenum target)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "GLES version < 3.0"));
        return false;
    }

    return ValidateEndQueryBase(context, target);
}

bool ValidateGetQueryiv(Context *context, GLenum target, GLenum pname, GLint *params)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "GLES version < 3.0"));
        return false;
    }

    return ValidateGetQueryivBase(context, target, pname, nullptr);
}

bool ValidateGetQueryObjectuiv(Context *context, GLuint id, GLenum pname, GLuint *params)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "GLES version < 3.0"));
        return false;
    }

    return ValidateGetQueryObjectValueBase(context, id, pname, nullptr);
}

bool ValidateFramebufferTextureLayer(Context *context,
                                     GLenum target,
                                     GLenum attachment,
                                     GLuint texture,
                                     GLint level,
                                     GLint layer)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    if (layer < 0)
    {
        context->handleError(Error(GL_INVALID_VALUE));
        return false;
    }

    if (!ValidateFramebufferTextureBase(context, target, attachment, texture, level))
    {
        return false;
    }

    const gl::Caps &caps = context->getCaps();
    if (texture != 0)
    {
        gl::Texture *tex = context->getTexture(texture);
        ASSERT(tex);

        switch (tex->getTarget())
        {
            case GL_TEXTURE_2D_ARRAY:
            {
                if (level > gl::log2(caps.max2DTextureSize))
                {
                    context->handleError(Error(GL_INVALID_VALUE));
                    return false;
                }

                if (static_cast<GLuint>(layer) >= caps.maxArrayTextureLayers)
                {
                    context->handleError(Error(GL_INVALID_VALUE));
                    return false;
                }
            }
            break;

            case GL_TEXTURE_3D:
            {
                if (level > gl::log2(caps.max3DTextureSize))
                {
                    context->handleError(Error(GL_INVALID_VALUE));
                    return false;
                }

                if (static_cast<GLuint>(layer) >= caps.max3DTextureSize)
                {
                    context->handleError(Error(GL_INVALID_VALUE));
                    return false;
                }
            }
            break;

            default:
                context->handleError(Error(GL_INVALID_OPERATION));
                return false;
        }

        const auto &format = tex->getFormat(tex->getTarget(), level);
        if (format.info->compressed)
        {
            context->handleError(Error(GL_INVALID_OPERATION));
            return false;
        }
    }

    return true;
}

bool ValidateInvalidateFramebuffer(Context *context,
                                   GLenum target,
                                   GLsizei numAttachments,
                                   const GLenum *attachments)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "Operation only supported on ES 3.0 and above"));
        return false;
    }

    bool defaultFramebuffer = false;

    switch (target)
    {
        case GL_DRAW_FRAMEBUFFER:
        case GL_FRAMEBUFFER:
            defaultFramebuffer = context->getGLState().getDrawFramebuffer()->id() == 0;
            break;
        case GL_READ_FRAMEBUFFER:
            defaultFramebuffer = context->getGLState().getReadFramebuffer()->id() == 0;
            break;
        default:
            context->handleError(Error(GL_INVALID_ENUM, "Invalid framebuffer target"));
            return false;
    }

    return ValidateDiscardFramebufferBase(context, target, numAttachments, attachments,
                                          defaultFramebuffer);
}

bool ValidateClearBuffer(ValidationContext *context)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    if (context->getGLState().getDrawFramebuffer()->checkStatus(context) != GL_FRAMEBUFFER_COMPLETE)
    {
        context->handleError(Error(GL_INVALID_FRAMEBUFFER_OPERATION));
        return false;
    }

    return true;
}

bool ValidateDrawRangeElements(Context *context,
                               GLenum mode,
                               GLuint start,
                               GLuint end,
                               GLsizei count,
                               GLenum type,
                               const GLvoid *indices,
                               IndexRange *indexRange)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Context does not support GLES3."));
        return false;
    }

    if (end < start)
    {
        context->handleError(Error(GL_INVALID_VALUE, "end < start"));
        return false;
    }

    if (!ValidateDrawElements(context, mode, count, type, indices, 0, indexRange))
    {
        return false;
    }

    if (indexRange->end > end || indexRange->start < start)
    {
        // GL spec says that behavior in this case is undefined - generating an error is fine.
        context->handleError(
            Error(GL_INVALID_OPERATION, "Indices are out of the start, end range."));
        return false;
    }
    return true;
}

bool ValidateGetUniformuiv(Context *context, GLuint program, GLint location, GLuint *params)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    return ValidateGetUniformBase(context, program, location);
}

bool ValidateReadBuffer(Context *context, GLenum src)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    const Framebuffer *readFBO = context->getGLState().getReadFramebuffer();

    if (readFBO == nullptr)
    {
        context->handleError(gl::Error(GL_INVALID_OPERATION, "No active read framebuffer."));
        return false;
    }

    if (src == GL_NONE)
    {
        return true;
    }

    if (src != GL_BACK && (src < GL_COLOR_ATTACHMENT0 || src > GL_COLOR_ATTACHMENT31))
    {
        context->handleError(gl::Error(GL_INVALID_ENUM, "Unknown enum for 'src' in ReadBuffer"));
        return false;
    }

    if (readFBO->id() == 0)
    {
        if (src != GL_BACK)
        {
            const char *errorMsg =
                "'src' must be GL_NONE or GL_BACK when reading from the default framebuffer.";
            context->handleError(gl::Error(GL_INVALID_OPERATION, errorMsg));
            return false;
        }
    }
    else
    {
        GLuint drawBuffer = static_cast<GLuint>(src - GL_COLOR_ATTACHMENT0);

        if (drawBuffer >= context->getCaps().maxDrawBuffers)
        {
            const char *errorMsg = "'src' is greater than MAX_DRAW_BUFFERS.";
            context->handleError(gl::Error(GL_INVALID_OPERATION, errorMsg));
            return false;
        }
    }

    return true;
}

bool ValidateCompressedTexImage3D(Context *context,
                                  GLenum target,
                                  GLint level,
                                  GLenum internalformat,
                                  GLsizei width,
                                  GLsizei height,
                                  GLsizei depth,
                                  GLint border,
                                  GLsizei imageSize,
                                  const GLvoid *data)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    if (!ValidTextureTarget(context, target))
    {
        context->handleError(Error(GL_INVALID_ENUM));
        return false;
    }

    // Validate image size
    if (!ValidImageSizeParameters(context, target, level, width, height, depth, false))
    {
        context->handleError(Error(GL_INVALID_VALUE));
        return false;
    }

    const InternalFormat &formatInfo = GetInternalFormatInfo(internalformat);
    if (!formatInfo.compressed)
    {
        context->handleError(Error(GL_INVALID_ENUM, "Not a valid compressed texture format"));
        return false;
    }

    auto blockSizeOrErr =
        formatInfo.computeCompressedImageSize(GL_UNSIGNED_BYTE, gl::Extents(width, height, depth));
    if (blockSizeOrErr.isError())
    {
        context->handleError(Error(GL_INVALID_VALUE));
        return false;
    }
    if (imageSize < 0 || static_cast<GLuint>(imageSize) != blockSizeOrErr.getResult())
    {
        context->handleError(Error(GL_INVALID_VALUE));
        return false;
    }

    // 3D texture target validation
    if (target != GL_TEXTURE_3D && target != GL_TEXTURE_2D_ARRAY)
    {
        context->handleError(
            Error(GL_INVALID_ENUM, "Must specify a valid 3D texture destination target"));
        return false;
    }

    // validateES3TexImageFormat sets the error code if there is an error
    if (!ValidateES3TexImage3DParameters(context, target, level, internalformat, true, false, 0, 0,
                                         0, width, height, depth, border, GL_NONE, GL_NONE, -1,
                                         data))
    {
        return false;
    }

    return true;
}

bool ValidateBindVertexArray(Context *context, GLuint array)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    return ValidateBindVertexArrayBase(context, array);
}

bool ValidateIsVertexArray(Context *context)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    return true;
}

static bool ValidateBindBufferCommon(Context *context,
                                     GLenum target,
                                     GLuint index,
                                     GLuint buffer,
                                     GLintptr offset,
                                     GLsizeiptr size)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    if (buffer != 0 && offset < 0)
    {
        context->handleError(Error(GL_INVALID_VALUE, "buffer is non-zero and offset is negative."));
        return false;
    }

    if (!context->getGLState().isBindGeneratesResourceEnabled() &&
        !context->isBufferGenerated(buffer))
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Buffer was not generated."));
        return false;
    }

    const Caps &caps = context->getCaps();
    switch (target)
    {
        case GL_TRANSFORM_FEEDBACK_BUFFER:
        {
            if (index >= caps.maxTransformFeedbackSeparateAttributes)
            {
                context->handleError(Error(GL_INVALID_VALUE,
                                           "index is greater than or equal to the number of "
                                           "TRANSFORM_FEEDBACK_BUFFER indexed binding points."));
                return false;
            }
            if (buffer != 0 && ((offset % 4) != 0 || (size % 4) != 0))
            {
                context->handleError(
                    Error(GL_INVALID_VALUE, "offset and size must be multiple of 4."));
                return false;
            }

            TransformFeedback *curTransformFeedback =
                context->getGLState().getCurrentTransformFeedback();
            if (curTransformFeedback && curTransformFeedback->isActive())
            {
                context->handleError(Error(GL_INVALID_OPERATION,
                                           "target is TRANSFORM_FEEDBACK_BUFFER and transform "
                                           "feedback is currently active."));
                return false;
            }
            break;
        }
        case GL_UNIFORM_BUFFER:
        {
            if (index >= caps.maxUniformBufferBindings)
            {
                context->handleError(Error(GL_INVALID_VALUE,
                                           "index is greater than or equal to the number of "
                                           "UNIFORM_BUFFER indexed binding points."));
                return false;
            }

            if (buffer != 0 && (offset % caps.uniformBufferOffsetAlignment) != 0)
            {
                context->handleError(
                    Error(GL_INVALID_VALUE,
                          "offset must be multiple of value of UNIFORM_BUFFER_OFFSET_ALIGNMENT."));
                return false;
            }
            break;
        }
        case GL_ATOMIC_COUNTER_BUFFER:
        {
            if (context->getClientVersion() < ES_3_1)
            {
                context->handleError(Error(
                    GL_INVALID_ENUM, "ATOMIC_COUNTER_BUFFER is not supported before GLES 3.1"));
                return false;
            }
            if (index >= caps.maxAtomicCounterBufferBindings)
            {
                context->handleError(Error(GL_INVALID_VALUE,
                                           "index is greater than or equal to the number of "
                                           "ATOMIC_COUNTER_BUFFER indexed binding points."));
                return false;
            }
            if (buffer != 0 && (offset % 4) != 0)
            {
                context->handleError(Error(GL_INVALID_VALUE, "offset must be a multiple of 4."));
                return false;
            }
            break;
        }
        case GL_SHADER_STORAGE_BUFFER:
        {
            if (context->getClientVersion() < ES_3_1)
            {
                context->handleError(
                    Error(GL_INVALID_ENUM, "SHADER_STORAGE_BUFFER is not supported in GLES3."));
                return false;
            }
            if (index >= caps.maxShaderStorageBufferBindings)
            {
                context->handleError(Error(GL_INVALID_VALUE,
                                           "index is greater than or equal to the number of "
                                           "SHADER_STORAGE_BUFFER indexed binding points."));
                return false;
            }
            if (buffer != 0 && (offset % caps.shaderStorageBufferOffsetAlignment) != 0)
            {
                context->handleError(Error(
                    GL_INVALID_VALUE,
                    "offset must be multiple of value of SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT."));
                return false;
            }
            break;
        }
        default:
            context->handleError(Error(GL_INVALID_ENUM, "the target is not supported."));
            return false;
    }

    return true;
}

bool ValidateBindBufferBase(Context *context, GLenum target, GLuint index, GLuint buffer)
{
    return ValidateBindBufferCommon(context, target, index, buffer, 0, 0);
}

bool ValidateBindBufferRange(Context *context,
                             GLenum target,
                             GLuint index,
                             GLuint buffer,
                             GLintptr offset,
                             GLsizeiptr size)
{
    if (buffer != 0 && size <= 0)
    {
        context->handleError(
            Error(GL_INVALID_VALUE, "buffer is non-zero and size is less than or equal to zero."));
        return false;
    }
    return ValidateBindBufferCommon(context, target, index, buffer, offset, size);
}

bool ValidateProgramBinary(Context *context,
                           GLuint program,
                           GLenum binaryFormat,
                           const void *binary,
                           GLint length)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    return ValidateProgramBinaryBase(context, program, binaryFormat, binary, length);
}

bool ValidateGetProgramBinary(Context *context,
                              GLuint program,
                              GLsizei bufSize,
                              GLsizei *length,
                              GLenum *binaryFormat,
                              void *binary)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    return ValidateGetProgramBinaryBase(context, program, bufSize, length, binaryFormat, binary);
}

bool ValidateProgramParameteri(Context *context, GLuint program, GLenum pname, GLint value)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Context does not support GLES3."));
        return false;
    }

    if (GetValidProgram(context, program) == nullptr)
    {
        return false;
    }

    switch (pname)
    {
        case GL_PROGRAM_BINARY_RETRIEVABLE_HINT:
            if (value != GL_FALSE && value != GL_TRUE)
            {
                context->handleError(Error(
                    GL_INVALID_VALUE, "Invalid value, expected GL_FALSE or GL_TRUE: %i", value));
                return false;
            }
            break;

        case GL_PROGRAM_SEPARABLE:
            if (context->getClientVersion() < ES_3_1)
            {
                context->handleError(
                    Error(GL_INVALID_ENUM, "PROGRAM_SEPARABLE is not supported before GLES 3.1"));
                return false;
            }

            if (value != GL_FALSE && value != GL_TRUE)
            {
                context->handleError(Error(
                    GL_INVALID_VALUE, "Invalid value, expected GL_FALSE or GL_TRUE: %i", value));
                return false;
            }
            break;

        default:
            context->handleError(Error(GL_INVALID_ENUM, "Invalid pname: 0x%X", pname));
            return false;
    }

    return true;
}

bool ValidateBlitFramebuffer(Context *context,
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
    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    return ValidateBlitFramebufferParameters(context, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0,
                                             dstX1, dstY1, mask, filter);
}

bool ValidateClearBufferiv(ValidationContext *context,
                           GLenum buffer,
                           GLint drawbuffer,
                           const GLint *value)
{
    switch (buffer)
    {
        case GL_COLOR:
            if (drawbuffer < 0 ||
                static_cast<GLuint>(drawbuffer) >= context->getCaps().maxDrawBuffers)
            {
                context->handleError(Error(GL_INVALID_VALUE));
                return false;
            }
            if (context->getExtensions().webglCompatibility)
            {
                constexpr GLenum validComponentTypes[] = {GL_INT};
                if (ValidateWebGLFramebufferAttachmentClearType(
                        context, drawbuffer, validComponentTypes, ArraySize(validComponentTypes)))
                {
                    return false;
                }
            }
            break;

        case GL_STENCIL:
            if (drawbuffer != 0)
            {
                context->handleError(Error(GL_INVALID_VALUE));
                return false;
            }
            break;

        default:
            context->handleError(Error(GL_INVALID_ENUM));
            return false;
    }

    return ValidateClearBuffer(context);
}

bool ValidateClearBufferuiv(ValidationContext *context,
                            GLenum buffer,
                            GLint drawbuffer,
                            const GLuint *value)
{
    switch (buffer)
    {
        case GL_COLOR:
            if (drawbuffer < 0 ||
                static_cast<GLuint>(drawbuffer) >= context->getCaps().maxDrawBuffers)
            {
                context->handleError(Error(GL_INVALID_VALUE));
                return false;
            }
            if (context->getExtensions().webglCompatibility)
            {
                constexpr GLenum validComponentTypes[] = {GL_UNSIGNED_INT};
                if (ValidateWebGLFramebufferAttachmentClearType(
                        context, drawbuffer, validComponentTypes, ArraySize(validComponentTypes)))
                {
                    return false;
                }
            }
            break;

        default:
            context->handleError(Error(GL_INVALID_ENUM));
            return false;
    }

    return ValidateClearBuffer(context);
}

bool ValidateClearBufferfv(ValidationContext *context,
                           GLenum buffer,
                           GLint drawbuffer,
                           const GLfloat *value)
{
    switch (buffer)
    {
        case GL_COLOR:
            if (drawbuffer < 0 ||
                static_cast<GLuint>(drawbuffer) >= context->getCaps().maxDrawBuffers)
            {
                context->handleError(Error(GL_INVALID_VALUE));
                return false;
            }
            if (context->getExtensions().webglCompatibility)
            {
                constexpr GLenum validComponentTypes[] = {GL_FLOAT, GL_UNSIGNED_NORMALIZED,
                                                          GL_SIGNED_NORMALIZED};
                if (ValidateWebGLFramebufferAttachmentClearType(
                        context, drawbuffer, validComponentTypes, ArraySize(validComponentTypes)))
                {
                    return false;
                }
            }
            break;

        case GL_DEPTH:
            if (drawbuffer != 0)
            {
                context->handleError(Error(GL_INVALID_VALUE));
                return false;
            }
            break;

        default:
            context->handleError(Error(GL_INVALID_ENUM));
            return false;
    }

    return ValidateClearBuffer(context);
}

bool ValidateClearBufferfi(ValidationContext *context,
                           GLenum buffer,
                           GLint drawbuffer,
                           GLfloat depth,
                           GLint stencil)
{
    switch (buffer)
    {
        case GL_DEPTH_STENCIL:
            if (drawbuffer != 0)
            {
                context->handleError(Error(GL_INVALID_VALUE));
                return false;
            }
            break;

        default:
            context->handleError(Error(GL_INVALID_ENUM));
            return false;
    }

    return ValidateClearBuffer(context);
}

bool ValidateDrawBuffers(ValidationContext *context, GLsizei n, const GLenum *bufs)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Context does not support GLES3."));
        return false;
    }

    return ValidateDrawBuffersBase(context, n, bufs);
}

bool ValidateCopyTexSubImage3D(Context *context,
                               GLenum target,
                               GLint level,
                               GLint xoffset,
                               GLint yoffset,
                               GLint zoffset,
                               GLint x,
                               GLint y,
                               GLsizei width,
                               GLsizei height)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    return ValidateES3CopyTexImage3DParameters(context, target, level, GL_NONE, true, xoffset,
                                               yoffset, zoffset, x, y, width, height, 0);
}

bool ValidateTexImage3D(Context *context,
                        GLenum target,
                        GLint level,
                        GLint internalformat,
                        GLsizei width,
                        GLsizei height,
                        GLsizei depth,
                        GLint border,
                        GLenum format,
                        GLenum type,
                        const GLvoid *pixels)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    return ValidateES3TexImage3DParameters(context, target, level, internalformat, false, false, 0,
                                           0, 0, width, height, depth, border, format, type, -1,
                                           pixels);
}

bool ValidateTexImage3DRobustANGLE(Context *context,
                                   GLenum target,
                                   GLint level,
                                   GLint internalformat,
                                   GLsizei width,
                                   GLsizei height,
                                   GLsizei depth,
                                   GLint border,
                                   GLenum format,
                                   GLenum type,
                                   GLsizei bufSize,
                                   const GLvoid *pixels)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    return ValidateES3TexImage3DParameters(context, target, level, internalformat, false, false, 0,
                                           0, 0, width, height, depth, border, format, type,
                                           bufSize, pixels);
}

bool ValidateTexSubImage3D(Context *context,
                           GLenum target,
                           GLint level,
                           GLint xoffset,
                           GLint yoffset,
                           GLint zoffset,
                           GLsizei width,
                           GLsizei height,
                           GLsizei depth,
                           GLenum format,
                           GLenum type,
                           const GLvoid *pixels)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    return ValidateES3TexImage3DParameters(context, target, level, GL_NONE, false, true, xoffset,
                                           yoffset, zoffset, width, height, depth, 0, format, type,
                                           -1, pixels);
}

bool ValidateTexSubImage3DRobustANGLE(Context *context,
                                      GLenum target,
                                      GLint level,
                                      GLint xoffset,
                                      GLint yoffset,
                                      GLint zoffset,
                                      GLsizei width,
                                      GLsizei height,
                                      GLsizei depth,
                                      GLenum format,
                                      GLenum type,
                                      GLsizei bufSize,
                                      const GLvoid *pixels)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    return ValidateES3TexImage3DParameters(context, target, level, GL_NONE, false, true, xoffset,
                                           yoffset, zoffset, width, height, depth, 0, format, type,
                                           bufSize, pixels);
}

bool ValidateCompressedTexSubImage3D(Context *context,
                                     GLenum target,
                                     GLint level,
                                     GLint xoffset,
                                     GLint yoffset,
                                     GLint zoffset,
                                     GLsizei width,
                                     GLsizei height,
                                     GLsizei depth,
                                     GLenum format,
                                     GLsizei imageSize,
                                     const GLvoid *data)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    const InternalFormat &formatInfo = GetInternalFormatInfo(format);
    if (!formatInfo.compressed)
    {
        context->handleError(Error(GL_INVALID_ENUM, "Not a valid compressed texture format"));
        return false;
    }

    auto blockSizeOrErr =
        formatInfo.computeCompressedImageSize(GL_UNSIGNED_BYTE, gl::Extents(width, height, depth));
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

    if (!data)
    {
        context->handleError(Error(GL_INVALID_VALUE));
        return false;
    }

    return ValidateES3TexImage3DParameters(context, target, level, GL_NONE, true, true, 0, 0, 0,
                                           width, height, depth, 0, GL_NONE, GL_NONE, -1, data);
}

bool ValidateGenQueries(Context *context, GLint n, GLuint *)
{
    return ValidateGenOrDeleteES3(context, n);
}

bool ValidateDeleteQueries(Context *context, GLint n, const GLuint *)
{
    return ValidateGenOrDeleteES3(context, n);
}

bool ValidateGenSamplers(Context *context, GLint count, GLuint *)
{
    return ValidateGenOrDeleteCountES3(context, count);
}

bool ValidateDeleteSamplers(Context *context, GLint count, const GLuint *)
{
    return ValidateGenOrDeleteCountES3(context, count);
}

bool ValidateGenTransformFeedbacks(Context *context, GLint n, GLuint *)
{
    return ValidateGenOrDeleteES3(context, n);
}

bool ValidateDeleteTransformFeedbacks(Context *context, GLint n, const GLuint *ids)
{
    if (!ValidateGenOrDeleteES3(context, n))
    {
        return false;
    }
    for (GLint i = 0; i < n; ++i)
    {
        auto *transformFeedback = context->getTransformFeedback(ids[i]);
        if (transformFeedback != nullptr && transformFeedback->isActive())
        {
            // ES 3.0.4 section 2.15.1 page 86
            context->handleError(
                Error(GL_INVALID_OPERATION, "Attempt to delete active transform feedback."));
            return false;
        }
    }
    return true;
}

bool ValidateGenVertexArrays(Context *context, GLint n, GLuint *)
{
    return ValidateGenOrDeleteES3(context, n);
}

bool ValidateDeleteVertexArrays(Context *context, GLint n, const GLuint *)
{
    return ValidateGenOrDeleteES3(context, n);
}

bool ValidateGenOrDeleteES3(Context *context, GLint n)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Context does not support GLES3."));
        return false;
    }
    return ValidateGenOrDelete(context, n);
}

bool ValidateGenOrDeleteCountES3(Context *context, GLint count)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Context does not support GLES3."));
        return false;
    }
    if (count < 0)
    {
        context->handleError(Error(GL_INVALID_VALUE, "count < 0"));
        return false;
    }
    return true;
}

bool ValidateBeginTransformFeedback(Context *context, GLenum primitiveMode)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Context does not support GLES3."));
        return false;
    }
    switch (primitiveMode)
    {
        case GL_TRIANGLES:
        case GL_LINES:
        case GL_POINTS:
            break;

        default:
            context->handleError(Error(GL_INVALID_ENUM, "Invalid primitive mode."));
            return false;
    }

    TransformFeedback *transformFeedback = context->getGLState().getCurrentTransformFeedback();
    ASSERT(transformFeedback != nullptr);

    if (transformFeedback->isActive())
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Transform feedback is already active."));
        return false;
    }
    return true;
}

bool ValidateGetBufferPointerv(Context *context, GLenum target, GLenum pname, GLvoid **params)
{
    return ValidateGetBufferPointervBase(context, target, pname, nullptr, params);
}

bool ValidateGetBufferPointervRobustANGLE(Context *context,
                                          GLenum target,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLvoid **params)
{
    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    if (!ValidateGetBufferPointervBase(context, target, pname, length, params))
    {
        return false;
    }

    if (!ValidateRobustBufferSize(context, bufSize, *length))
    {
        return false;
    }

    return true;
}

bool ValidateUnmapBuffer(Context *context, GLenum target)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    return ValidateUnmapBufferBase(context, target);
}

bool ValidateMapBufferRange(Context *context,
                            GLenum target,
                            GLintptr offset,
                            GLsizeiptr length,
                            GLbitfield access)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Context does not support GLES3."));
        return false;
    }

    return ValidateMapBufferRangeBase(context, target, offset, length, access);
}

bool ValidateFlushMappedBufferRange(Context *context,
                                    GLenum target,
                                    GLintptr offset,
                                    GLsizeiptr length)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Context does not support GLES3."));
        return false;
    }

    return ValidateFlushMappedBufferRangeBase(context, target, offset, length);
}

bool ValidateIndexedStateQuery(ValidationContext *context,
                               GLenum pname,
                               GLuint index,
                               GLsizei *length)
{
    if (length)
    {
        *length = 0;
    }

    GLenum nativeType;
    unsigned int numParams;
    if (!context->getIndexedQueryParameterInfo(pname, &nativeType, &numParams))
    {
        context->handleError(Error(GL_INVALID_ENUM));
        return false;
    }

    const Caps &caps = context->getCaps();
    switch (pname)
    {
        case GL_TRANSFORM_FEEDBACK_BUFFER_START:
        case GL_TRANSFORM_FEEDBACK_BUFFER_SIZE:
        case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
            if (index >= caps.maxTransformFeedbackSeparateAttributes)
            {
                context->handleError(Error(GL_INVALID_VALUE));
                return false;
            }
            break;

        case GL_UNIFORM_BUFFER_START:
        case GL_UNIFORM_BUFFER_SIZE:
        case GL_UNIFORM_BUFFER_BINDING:
            if (index >= caps.maxUniformBufferBindings)
            {
                context->handleError(Error(GL_INVALID_VALUE));
                return false;
            }
            break;

        case GL_MAX_COMPUTE_WORK_GROUP_SIZE:
        case GL_MAX_COMPUTE_WORK_GROUP_COUNT:
            if (index >= 3u)
            {
                context->handleError(Error(GL_INVALID_VALUE));
                return false;
            }
            break;

        case GL_ATOMIC_COUNTER_BUFFER_START:
        case GL_ATOMIC_COUNTER_BUFFER_SIZE:
        case GL_ATOMIC_COUNTER_BUFFER_BINDING:
            if (context->getClientVersion() < ES_3_1)
            {
                context->handleError(
                    Error(GL_INVALID_ENUM,
                          "Atomic Counter buffers are not supported in this version of GL"));
                return false;
            }
            if (index >= caps.maxAtomicCounterBufferBindings)
            {
                context->handleError(
                    Error(GL_INVALID_VALUE,
                          "index is outside the valid range for GL_ATOMIC_COUNTER_BUFFER_BINDING"));
                return false;
            }
            break;

        case GL_SHADER_STORAGE_BUFFER_START:
        case GL_SHADER_STORAGE_BUFFER_SIZE:
        case GL_SHADER_STORAGE_BUFFER_BINDING:
            if (context->getClientVersion() < ES_3_1)
            {
                context->handleError(
                    Error(GL_INVALID_ENUM,
                          "Shader storage buffers are not supported in this version of GL"));
                return false;
            }
            if (index >= caps.maxShaderStorageBufferBindings)
            {
                context->handleError(
                    Error(GL_INVALID_VALUE,
                          "index is outside the valid range for GL_SHADER_STORAGE_BUFFER_BINDING"));
                return false;
            }
            break;

        case GL_VERTEX_BINDING_BUFFER:
        case GL_VERTEX_BINDING_DIVISOR:
        case GL_VERTEX_BINDING_OFFSET:
        case GL_VERTEX_BINDING_STRIDE:
            if (context->getClientVersion() < ES_3_1)
            {
                context->handleError(
                    Error(GL_INVALID_ENUM,
                          "Vertex Attrib Bindings are not supported in this version of GL"));
                return false;
            }
            if (index >= caps.maxVertexAttribBindings)
            {
                context->handleError(
                    Error(GL_INVALID_VALUE,
                          "bindingindex must be smaller than MAX_VERTEX_ATTRIB_BINDINGS."));
                return false;
            }
            break;
        default:
            context->handleError(Error(GL_INVALID_ENUM));
            return false;
    }

    if (length)
    {
        *length = 1;
    }

    return true;
}

bool ValidateGetIntegeri_v(ValidationContext *context, GLenum target, GLuint index, GLint *data)
{
    if (context->getClientVersion() < ES_3_0)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Context does not support GLES3.0"));
        return false;
    }
    return ValidateIndexedStateQuery(context, target, index, nullptr);
}

bool ValidateGetIntegeri_vRobustANGLE(ValidationContext *context,
                                      GLenum target,
                                      GLuint index,
                                      GLsizei bufSize,
                                      GLsizei *length,
                                      GLint *data)
{
    if (context->getClientVersion() < ES_3_0)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Context does not support GLES3.0"));
        return false;
    }

    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    if (!ValidateIndexedStateQuery(context, target, index, length))
    {
        return false;
    }

    if (!ValidateRobustBufferSize(context, bufSize, *length))
    {
        return false;
    }

    return true;
}

bool ValidateGetInteger64i_v(ValidationContext *context, GLenum target, GLuint index, GLint64 *data)
{
    if (context->getClientVersion() < ES_3_0)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Context does not support GLES3.0"));
        return false;
    }
    return ValidateIndexedStateQuery(context, target, index, nullptr);
}

bool ValidateGetInteger64i_vRobustANGLE(ValidationContext *context,
                                        GLenum target,
                                        GLuint index,
                                        GLsizei bufSize,
                                        GLsizei *length,
                                        GLint64 *data)
{
    if (context->getClientVersion() < ES_3_0)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Context does not support GLES3.0"));
        return false;
    }

    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    if (!ValidateIndexedStateQuery(context, target, index, length))
    {
        return false;
    }

    if (!ValidateRobustBufferSize(context, bufSize, *length))
    {
        return false;
    }

    return true;
}

bool ValidateCopyBufferSubData(ValidationContext *context,
                               GLenum readTarget,
                               GLenum writeTarget,
                               GLintptr readOffset,
                               GLintptr writeOffset,
                               GLsizeiptr size)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "CopyBufferSubData requires ES 3 or greater"));
        return false;
    }

    if (!ValidBufferTarget(context, readTarget) || !ValidBufferTarget(context, writeTarget))
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid buffer target"));
        return false;
    }

    Buffer *readBuffer  = context->getGLState().getTargetBuffer(readTarget);
    Buffer *writeBuffer = context->getGLState().getTargetBuffer(writeTarget);

    if (!readBuffer || !writeBuffer)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "No buffer bound to target"));
        return false;
    }

    // Verify that readBuffer and writeBuffer are not currently mapped
    if (readBuffer->isMapped() || writeBuffer->isMapped())
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "Cannot call CopyBufferSubData on a mapped buffer"));
        return false;
    }

    CheckedNumeric<GLintptr> checkedReadOffset(readOffset);
    CheckedNumeric<GLintptr> checkedWriteOffset(writeOffset);
    CheckedNumeric<GLintptr> checkedSize(size);

    auto checkedReadSum  = checkedReadOffset + checkedSize;
    auto checkedWriteSum = checkedWriteOffset + checkedSize;

    if (!checkedReadSum.IsValid() || !checkedWriteSum.IsValid() ||
        !IsValueInRangeForNumericType<GLintptr>(readBuffer->getSize()) ||
        !IsValueInRangeForNumericType<GLintptr>(writeBuffer->getSize()))
    {
        context->handleError(
            Error(GL_INVALID_VALUE, "Integer overflow when validating copy offsets."));
        return false;
    }

    if (readOffset < 0 || writeOffset < 0 || size < 0)
    {
        context->handleError(
            Error(GL_INVALID_VALUE, "readOffset, writeOffset and size must all be non-negative"));
        return false;
    }

    if (checkedReadSum.ValueOrDie() > readBuffer->getSize() ||
        checkedWriteSum.ValueOrDie() > writeBuffer->getSize())
    {
        context->handleError(
            Error(GL_INVALID_VALUE, "Buffer offset overflow in CopyBufferSubData"));
        return false;
    }

    if (readBuffer == writeBuffer)
    {
        auto checkedOffsetDiff = (checkedReadOffset - checkedWriteOffset).Abs();
        if (!checkedOffsetDiff.IsValid())
        {
            // This shold not be possible.
            UNREACHABLE();
            context->handleError(
                Error(GL_INVALID_VALUE, "Integer overflow when validating same buffer copy."));
            return false;
        }

        if (checkedOffsetDiff.ValueOrDie() < size)
        {
            context->handleError(Error(GL_INVALID_VALUE));
            return false;
        }
    }

    return true;
}

bool ValidateGetStringi(Context *context, GLenum name, GLuint index)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "glGetStringi requires OpenGL ES 3.0 or higher."));
        return false;
    }

    switch (name)
    {
        case GL_EXTENSIONS:
            if (index >= context->getExtensionStringCount())
            {
                context->handleError(Error(
                    GL_INVALID_VALUE, "index must be less than the number of extension strings."));
                return false;
            }
            break;

        case GL_REQUESTABLE_EXTENSIONS_ANGLE:
            if (!context->getExtensions().requestExtension)
            {
                context->handleError(Error(GL_INVALID_ENUM, "Invalid name."));
                return false;
            }
            if (index >= context->getRequestableExtensionStringCount())
            {
                context->handleError(
                    Error(GL_INVALID_VALUE,
                          "index must be less than the number of requestable extension strings."));
                return false;
            }
            break;

        default:
            context->handleError(Error(GL_INVALID_ENUM, "Invalid name."));
            return false;
    }

    return true;
}

bool ValidateRenderbufferStorageMultisample(ValidationContext *context,
                                            GLenum target,
                                            GLsizei samples,
                                            GLenum internalformat,
                                            GLsizei width,
                                            GLsizei height)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    if (!ValidateRenderbufferStorageParametersBase(context, target, samples, internalformat, width,
                                                   height))
    {
        return false;
    }

    // The ES3 spec(section 4.4.2) states that the internal format must be sized and not an integer
    // format if samples is greater than zero.
    const gl::InternalFormat &formatInfo = gl::GetInternalFormatInfo(internalformat);
    if ((formatInfo.componentType == GL_UNSIGNED_INT || formatInfo.componentType == GL_INT) &&
        samples > 0)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    // The behavior is different than the ANGLE version, which would generate a GL_OUT_OF_MEMORY.
    const TextureCaps &formatCaps = context->getTextureCaps().get(internalformat);
    if (static_cast<GLuint>(samples) > formatCaps.getMaxSamples())
    {
        context->handleError(
            Error(GL_INVALID_OPERATION,
                  "Samples must not be greater than maximum supported value for the format."));
        return false;
    }

    return true;
}

bool ValidateVertexAttribIPointer(ValidationContext *context,
                                  GLuint index,
                                  GLint size,
                                  GLenum type,
                                  GLsizei stride,
                                  const GLvoid *pointer)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "VertexAttribIPointer requires OpenGL ES 3.0 or higher."));
        return false;
    }

    if (!ValidateVertexFormatBase(context, index, size, type, true))
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

        // [OpenGL ES 3.1] Section 10.3.1 page 245:
        // glVertexAttribBinding is part of the equivalent code of VertexAttribIPointer, so its
        // validation should be inherited.
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
    if (context->getGLState().getVertexArrayId() != 0 &&
        context->getGLState().getArrayBufferId() == 0 && pointer != nullptr)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION,
                  "Client data cannot be used with a non-default vertex array object."));
        return false;
    }

    if (context->getExtensions().webglCompatibility)
    {
        if (!ValidateWebGLVertexAttribPointer(context, type, false, stride, pointer, true))
        {
            return false;
        }
    }

    return true;
}

bool ValidateGetSynciv(Context *context,
                       GLsync sync,
                       GLenum pname,
                       GLsizei bufSize,
                       GLsizei *length,
                       GLint *values)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "GetSynciv requires OpenGL ES 3.0 or higher."));
        return false;
    }

    if (bufSize < 0)
    {
        context->handleError(Error(GL_INVALID_VALUE, "bufSize cannot be negative."));
        return false;
    }

    FenceSync *fenceSync = context->getFenceSync(sync);
    if (!fenceSync)
    {
        context->handleError(Error(GL_INVALID_VALUE, "Invalid sync object."));
        return false;
    }

    switch (pname)
    {
        case GL_OBJECT_TYPE:
        case GL_SYNC_CONDITION:
        case GL_SYNC_FLAGS:
        case GL_SYNC_STATUS:
            break;

        default:
            context->handleError(Error(GL_INVALID_ENUM, "Invalid pname."));
            return false;
    }

    return true;
}

}  // namespace gl
