#include "precompiled.h"
//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationES3.cpp: Validation functions for OpenGL ES 3.0 entry point parameters

#include "libGLESv2/validationES3.h"
#include "libGLESv2/validationES.h"
#include "libGLESv2/Context.h"
#include "libGLESv2/Texture.h"
#include "libGLESv2/Framebuffer.h"
#include "libGLESv2/Renderbuffer.h"
#include "libGLESv2/formatutils.h"
#include "libGLESv2/main.h"

#include "common/mathutil.h"

namespace gl
{

bool ValidateES3TexImageParameters(gl::Context *context, GLenum target, GLint level, GLenum internalformat, bool isCompressed, bool isSubImage,
                                   GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                                   GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
    // Validate image size
    if (!ValidImageSize(context, target, level, width, height, depth))
    {
        return gl::error(GL_INVALID_VALUE, false);
    }

    // Verify zero border
    if (border != 0)
    {
        return gl::error(GL_INVALID_VALUE, false);
    }

    gl::Texture *texture = NULL;
    bool textureCompressed = false;
    GLenum textureInternalFormat = GL_NONE;
    GLint textureLevelWidth = 0;
    GLint textureLevelHeight = 0;
    GLint textureLevelDepth = 0;
    switch (target)
    {
      case GL_TEXTURE_2D:
        {
            if (width > (context->getMaximum2DTextureDimension() >> level) ||
                height > (context->getMaximum2DTextureDimension() >> level))
            {
                return gl::error(GL_INVALID_VALUE, false);
            }

            gl::Texture2D *texture2d = context->getTexture2D();
            if (texture2d)
            {
                textureCompressed = texture2d->isCompressed(level);
                textureInternalFormat = texture2d->getInternalFormat(level);
                textureLevelWidth = texture2d->getWidth(level);
                textureLevelHeight = texture2d->getHeight(level);
                textureLevelDepth = 1;
                texture = texture2d;
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
            if (!isSubImage && width != height)
            {
                return gl::error(GL_INVALID_VALUE, false);
            }

            if (width > (context->getMaximumCubeTextureDimension() >> level))
            {
                return gl::error(GL_INVALID_VALUE, false);
            }

            gl::TextureCubeMap *textureCube = context->getTextureCubeMap();
            if (textureCube)
            {
                textureCompressed = textureCube->isCompressed(target, level);
                textureInternalFormat = textureCube->getInternalFormat(target, level);
                textureLevelWidth = textureCube->getWidth(target, level);
                textureLevelHeight = textureCube->getHeight(target, level);
                textureLevelDepth = 1;
                texture = textureCube;
            }
        }
        break;

      case GL_TEXTURE_3D:
        {
            if (width > (context->getMaximum3DTextureDimension() >> level) ||
                height > (context->getMaximum3DTextureDimension() >> level) ||
                depth > (context->getMaximum3DTextureDimension() >> level))
            {
                return gl::error(GL_INVALID_VALUE, false);
            }

            gl::Texture3D *texture3d = context->getTexture3D();
            if (texture3d)
            {
                textureCompressed = texture3d->isCompressed(level);
                textureInternalFormat = texture3d->getInternalFormat(level);
                textureLevelWidth = texture3d->getWidth(level);
                textureLevelHeight = texture3d->getHeight(level);
                textureLevelDepth = texture3d->getDepth(level);
                texture = texture3d;
            }
        }
        break;

        case GL_TEXTURE_2D_ARRAY:
          {
              if (width > (context->getMaximum2DTextureDimension() >> level) ||
                  height > (context->getMaximum2DTextureDimension() >> level) ||
                  depth > (context->getMaximum2DArrayTextureLayers() >> level))
              {
                  return gl::error(GL_INVALID_VALUE, false);
              }

              gl::Texture2DArray *texture2darray = context->getTexture2DArray();
              if (texture2darray)
              {
                  textureCompressed = texture2darray->isCompressed(level);
                  textureInternalFormat = texture2darray->getInternalFormat(level);
                  textureLevelWidth = texture2darray->getWidth(level);
                  textureLevelHeight = texture2darray->getHeight(level);
                  textureLevelDepth = texture2darray->getLayers(level);
                  texture = texture2darray;
              }
          }
          break;

      default:
        return gl::error(GL_INVALID_ENUM, false);
    }

    if (!texture)
    {
        return gl::error(GL_INVALID_OPERATION, false);
    }

    if (texture->isImmutable() && !isSubImage)
    {
        return gl::error(GL_INVALID_OPERATION, false);
    }

    // Validate texture formats
    GLenum actualInternalFormat = isSubImage ? textureInternalFormat : internalformat;
    if (isCompressed)
    {
        if (!ValidCompressedImageSize(context, actualInternalFormat, width, height))
        {
            return gl::error(GL_INVALID_OPERATION, false);
        }

        if (!gl::IsFormatCompressed(actualInternalFormat, context->getClientVersion()))
        {
            return gl::error(GL_INVALID_ENUM, false);
        }

        if (target == GL_TEXTURE_3D)
        {
            return gl::error(GL_INVALID_OPERATION, false);
        }
    }
    else
    {
        if (!gl::IsValidInternalFormat(actualInternalFormat, context) ||
            !gl::IsValidFormat(format, context->getClientVersion()) ||
            !gl::IsValidType(type, context->getClientVersion()))
        {
            return gl::error(GL_INVALID_ENUM, false);
        }

        if (!gl::IsValidFormatCombination(actualInternalFormat, format, type, context->getClientVersion()))
        {
            return gl::error(GL_INVALID_OPERATION, false);
        }

        if (target == GL_TEXTURE_3D && (format == GL_DEPTH_COMPONENT || format == GL_DEPTH_STENCIL))
        {
            return gl::error(GL_INVALID_OPERATION, false);
        }
    }

    // Validate sub image parameters
    if (isSubImage)
    {
        if (isCompressed != textureCompressed)
        {
            return gl::error(GL_INVALID_OPERATION, false);
        }

        if (isCompressed)
        {
            if ((width % 4 != 0 && width != textureLevelWidth) ||
                (height % 4 != 0 && height != textureLevelHeight))
            {
                return gl::error(GL_INVALID_OPERATION, false);
            }
        }

        if (width == 0 || height == 0 || depth == 0)
        {
            return false;
        }

        if (xoffset < 0 || yoffset < 0 || zoffset < 0)
        {
            return gl::error(GL_INVALID_VALUE, false);
        }

        if (std::numeric_limits<GLsizei>::max() - xoffset < width ||
            std::numeric_limits<GLsizei>::max() - yoffset < height ||
            std::numeric_limits<GLsizei>::max() - zoffset < depth)
        {
            return gl::error(GL_INVALID_VALUE, false);
        }

        if (xoffset + width > textureLevelWidth ||
            yoffset + height > textureLevelHeight ||
            zoffset + depth > textureLevelDepth)
        {
            return gl::error(GL_INVALID_VALUE, false);
        }
    }

    // Check for pixel unpack buffer related API errors
    gl::Buffer *pixelUnpackBuffer = context->getPixelUnpackBuffer();
    if (pixelUnpackBuffer != NULL)
    {
        // ...the data would be unpacked from the buffer object such that the memory reads required
        // would exceed the data store size.
        size_t widthSize = static_cast<size_t>(width);
        size_t heightSize = static_cast<size_t>(height);
        size_t depthSize = static_cast<size_t>(depth);
        size_t pixelBytes = static_cast<size_t>(gl::GetPixelBytes(actualInternalFormat, context->getClientVersion()));

        if (!rx::IsUnsignedMultiplicationSafe(widthSize, heightSize) ||
            !rx::IsUnsignedMultiplicationSafe(widthSize * heightSize, depthSize) ||
            !rx::IsUnsignedMultiplicationSafe(widthSize * heightSize * depthSize, pixelBytes))
        {
            // Overflow past the end of the buffer
            return gl::error(GL_INVALID_OPERATION, false);
        }

        size_t copyBytes = widthSize * heightSize * depthSize * pixelBytes;
        size_t offset = reinterpret_cast<size_t>(pixels);

        if (!rx::IsUnsignedAdditionSafe(offset, copyBytes) || ((offset + copyBytes) > static_cast<size_t>(pixelUnpackBuffer->size())))
        {
            // Overflow past the end of the buffer
            return gl::error(GL_INVALID_OPERATION, false);
        }

        // ...data is not evenly divisible into the number of bytes needed to store in memory a datum
        // indicated by type.
        size_t dataBytesPerPixel = static_cast<size_t>(gl::GetTypeBytes(type));

        if ((offset % dataBytesPerPixel) != 0)
        {
            return gl::error(GL_INVALID_OPERATION, false);
        }

        // ...the buffer object's data store is currently mapped.
        if (pixelUnpackBuffer->mapped())
        {
            return gl::error(GL_INVALID_OPERATION, false);
        }
    }

    return true;
}

bool ValidateES3CopyTexImageParameters(gl::Context *context, GLenum target, GLint level, GLenum internalformat,
                                       bool isSubImage, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y,
                                       GLsizei width, GLsizei height, GLint border)
{
    if (!ValidTexture2DDestinationTarget(context, target))
    {
        return gl::error(GL_INVALID_ENUM, false);
    }

    if (level < 0 || xoffset < 0 || yoffset < 0 || zoffset < 0 || width < 0 || height < 0)
    {
        return gl::error(GL_INVALID_VALUE, false);
    }

    if (std::numeric_limits<GLsizei>::max() - xoffset < width || std::numeric_limits<GLsizei>::max() - yoffset < height)
    {
        return gl::error(GL_INVALID_VALUE, false);
    }

    if (border != 0)
    {
        return gl::error(GL_INVALID_VALUE, false);
    }

    if (!ValidMipLevel(context, target, level))
    {
        return gl::error(GL_INVALID_VALUE, false);
    }

    gl::Framebuffer *framebuffer = context->getReadFramebuffer();

    if (framebuffer->completeness() != GL_FRAMEBUFFER_COMPLETE)
    {
        return gl::error(GL_INVALID_FRAMEBUFFER_OPERATION, false);
    }

    if (context->getReadFramebufferHandle() != 0 && framebuffer->getSamples() != 0)
    {
        return gl::error(GL_INVALID_OPERATION, false);
    }

    gl::Renderbuffer *source = framebuffer->getReadColorbuffer();
    GLenum colorbufferInternalFormat = source->getInternalFormat();
    gl::Texture *texture = NULL;
    GLenum textureInternalFormat = GL_NONE;
    bool textureCompressed = false;
    bool textureIsDepth = false;
    GLint textureLevelWidth = 0;
    GLint textureLevelHeight = 0;
    GLint textureLevelDepth = 0;
    switch (target)
    {
      case GL_TEXTURE_2D:
        {
            gl::Texture2D *texture2d = context->getTexture2D();
            if (texture2d)
            {
                textureInternalFormat = texture2d->getInternalFormat(level);
                textureCompressed = texture2d->isCompressed(level);
                textureIsDepth = texture2d->isDepth(level);
                textureLevelWidth = texture2d->getWidth(level);
                textureLevelHeight = texture2d->getHeight(level);
                textureLevelDepth = 1;
                texture = texture2d;
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
            gl::TextureCubeMap *textureCube = context->getTextureCubeMap();
            if (textureCube)
            {
                textureInternalFormat = textureCube->getInternalFormat(target, level);
                textureCompressed = textureCube->isCompressed(target, level);
                textureIsDepth = false;
                textureLevelWidth = textureCube->getWidth(target, level);
                textureLevelHeight = textureCube->getHeight(target, level);
                textureLevelDepth = 1;
                texture = textureCube;
            }
        }
        break;

      case GL_TEXTURE_2D_ARRAY:
        {
            gl::Texture2DArray *texture2dArray = context->getTexture2DArray();
            if (texture2dArray)
            {
                textureInternalFormat = texture2dArray->getInternalFormat(level);
                textureCompressed = texture2dArray->isCompressed(level);
                textureIsDepth = texture2dArray->isDepth(level);
                textureLevelWidth = texture2dArray->getWidth(level);
                textureLevelHeight = texture2dArray->getHeight(level);
                textureLevelDepth = texture2dArray->getLayers(level);
                texture = texture2dArray;
            }
        }
        break;

      case GL_TEXTURE_3D:
        {
            gl::Texture3D *texture3d = context->getTexture3D();
            if (texture3d)
            {
                textureInternalFormat = texture3d->getInternalFormat(level);
                textureCompressed = texture3d->isCompressed(level);
                textureIsDepth = texture3d->isDepth(level);
                textureLevelWidth = texture3d->getWidth(level);
                textureLevelHeight = texture3d->getHeight(level);
                textureLevelDepth = texture3d->getDepth(level);
                texture = texture3d;
            }
        }
        break;

      default:
        return gl::error(GL_INVALID_ENUM, false);
    }

    if (!texture)
    {
        return gl::error(GL_INVALID_OPERATION, false);
    }

    if (texture->isImmutable() && !isSubImage)
    {
        return gl::error(GL_INVALID_OPERATION, false);
    }

    if (textureIsDepth)
    {
        return gl::error(GL_INVALID_OPERATION, false);
    }

    if (textureCompressed)
    {
        if ((width % 4 != 0 && width != textureLevelWidth) ||
            (height % 4 != 0 && height != textureLevelHeight))
        {
            return gl::error(GL_INVALID_OPERATION, false);
        }
    }

    if (isSubImage)
    {
        if (xoffset + width > textureLevelWidth ||
            yoffset + height > textureLevelHeight ||
            zoffset >= textureLevelDepth)
        {
            return gl::error(GL_INVALID_VALUE, false);
        }

        if (!gl::IsValidCopyTexImageCombination(textureInternalFormat, colorbufferInternalFormat, context->getReadFramebufferHandle(),
                                                context->getClientVersion()))
        {
            return gl::error(GL_INVALID_OPERATION, false);
        }
    }
    else
    {
        if (!gl::IsValidCopyTexImageCombination(internalformat, colorbufferInternalFormat, context->getReadFramebufferHandle(),
                                                context->getClientVersion()))
        {
            return gl::error(GL_INVALID_OPERATION, false);
        }
    }



    // If width or height is zero, it is a no-op.  Return false without setting an error.
    return (width > 0 && height > 0);
}

bool ValidateES3TexStorageParameters(gl::Context *context, GLenum target, GLsizei levels, GLenum internalformat,
                                     GLsizei width, GLsizei height, GLsizei depth)
{
    if (width < 1 || height < 1 || depth < 1 || levels < 1)
    {
        return gl::error(GL_INVALID_VALUE, false);
    }

    if (levels > gl::log2(std::max(std::max(width, height), depth)) + 1)
    {
        return gl::error(GL_INVALID_OPERATION, false);
    }

    gl::Texture *texture = NULL;
    switch (target)
    {
      case GL_TEXTURE_2D:
        {
            texture = context->getTexture2D();

            if (width > (context->getMaximum2DTextureDimension()) ||
                height > (context->getMaximum2DTextureDimension()))
            {
                return gl::error(GL_INVALID_VALUE, false);
            }
        }
        break;

      case GL_TEXTURE_CUBE_MAP:
        {
            texture = context->getTextureCubeMap();

            if (width != height)
            {
                return gl::error(GL_INVALID_VALUE, false);
            }

            if (width > (context->getMaximumCubeTextureDimension()))
            {
                return gl::error(GL_INVALID_VALUE, false);
            }
        }
        break;

      case GL_TEXTURE_3D:
        {
            texture = context->getTexture3D();

            if (width > (context->getMaximum3DTextureDimension()) ||
                height > (context->getMaximum3DTextureDimension()) ||
                depth > (context->getMaximum3DTextureDimension()))
            {
                return gl::error(GL_INVALID_VALUE, false);
            }
        }
        break;

      case GL_TEXTURE_2D_ARRAY:
        {
            texture = context->getTexture2DArray();

            if (width > (context->getMaximum2DTextureDimension()) ||
                height > (context->getMaximum2DTextureDimension()) ||
                depth > (context->getMaximum2DArrayTextureLayers()))
            {
                return gl::error(GL_INVALID_VALUE, false);
            }
        }
        break;

      default:
        return gl::error(GL_INVALID_ENUM, false);
    }

    if (!texture || texture->id() == 0)
    {
        return gl::error(GL_INVALID_OPERATION, false);
    }

    if (texture->isImmutable())
    {
        return gl::error(GL_INVALID_OPERATION, false);
    }

    if (!gl::IsValidInternalFormat(internalformat, context))
    {
        return gl::error(GL_INVALID_ENUM, false);
    }

    if (!gl::IsSizedInternalFormat(internalformat, context->getClientVersion()))
    {
        return gl::error(GL_INVALID_ENUM, false);
    }

    return true;
}

bool ValidateES3FramebufferTextureParameters(gl::Context *context, GLenum target, GLenum attachment,
                                             GLenum textarget, GLuint texture, GLint level, GLint layer,
                                             bool layerCall)
{
    if (target != GL_FRAMEBUFFER && target != GL_DRAW_FRAMEBUFFER && target != GL_READ_FRAMEBUFFER)
    {
        return gl::error(GL_INVALID_ENUM, false);
    }

    if (attachment >= GL_COLOR_ATTACHMENT0 && attachment <= GL_COLOR_ATTACHMENT15)
    {
        const unsigned int colorAttachment = (attachment - GL_COLOR_ATTACHMENT0);
        if (colorAttachment >= context->getMaximumRenderTargets())
        {
            return gl::error(GL_INVALID_VALUE, false);
        }
    }
    else
    {
        switch (attachment)
        {
          case GL_DEPTH_ATTACHMENT:
          case GL_STENCIL_ATTACHMENT:
          case GL_DEPTH_STENCIL_ATTACHMENT:
            break;
          default:
            return gl::error(GL_INVALID_ENUM, false);
        }
    }

    if (texture != 0)
    {
        gl::Texture *tex = context->getTexture(texture);

        if (tex == NULL)
        {
            return gl::error(GL_INVALID_OPERATION, false);
        }

        if (level < 0)
        {
            return gl::error(GL_INVALID_VALUE, false);
        }

        if (layer < 0)
        {
            return gl::error(GL_INVALID_VALUE, false);
        }

        if (!layerCall)
        {
            switch (textarget)
            {
              case GL_TEXTURE_2D:
                {
                    if (level > gl::log2(context->getMaximum2DTextureDimension()))
                    {
                        return gl::error(GL_INVALID_VALUE, false);
                    }
                    if (tex->getTarget() != GL_TEXTURE_2D)
                    {
                        return gl::error(GL_INVALID_OPERATION, false);
                    }
                    gl::Texture2D *tex2d = static_cast<gl::Texture2D *>(tex);
                    if (tex2d->isCompressed(level))
                    {
                        return gl::error(GL_INVALID_OPERATION, false);
                    }
                    break;
                }

              case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
              case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
              case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
              case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
              case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
              case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
                {
                    if (level > gl::log2(context->getMaximumCubeTextureDimension()))
                    {
                        return gl::error(GL_INVALID_VALUE, false);
                    }
                    if (tex->getTarget() != GL_TEXTURE_CUBE_MAP)
                    {
                        return gl::error(GL_INVALID_OPERATION, false);
                    }
                    gl::TextureCubeMap *texcube = static_cast<gl::TextureCubeMap *>(tex);
                    if (texcube->isCompressed(textarget, level))
                    {
                        return gl::error(GL_INVALID_OPERATION, false);
                    }
                    break;
                }

              default:
                return gl::error(GL_INVALID_ENUM, false);
            }
        }
        else
        {
            switch (tex->getTarget())
            {
              case GL_TEXTURE_2D_ARRAY:
                {
                    if (level > gl::log2(context->getMaximum2DTextureDimension()))
                    {
                        return gl::error(GL_INVALID_VALUE, false);
                    }

                    if (layer >= context->getMaximum2DArrayTextureLayers())
                    {
                        return gl::error(GL_INVALID_VALUE, false);
                    }

                    gl::Texture2DArray *texArray = static_cast<gl::Texture2DArray *>(tex);
                    if (texArray->isCompressed(level))
                    {
                        return gl::error(GL_INVALID_OPERATION, false);
                    }

                    break;
                }

              case GL_TEXTURE_3D:
                {
                    if (level > gl::log2(context->getMaximum3DTextureDimension()))
                    {
                        return gl::error(GL_INVALID_VALUE, false);
                    }

                    if (layer >= context->getMaximum3DTextureDimension())
                    {
                        return gl::error(GL_INVALID_VALUE, false);
                    }

                    gl::Texture3D *tex3d = static_cast<gl::Texture3D *>(tex);
                    if (tex3d->isCompressed(level))
                    {
                        return gl::error(GL_INVALID_OPERATION, false);
                    }

                    break;
                }

              default:
                return gl::error(GL_INVALID_OPERATION, false);
            }
        }
    }

    gl::Framebuffer *framebuffer = NULL;
    GLuint framebufferHandle = 0;
    if (target == GL_READ_FRAMEBUFFER)
    {
        framebuffer = context->getReadFramebuffer();
        framebufferHandle = context->getReadFramebufferHandle();
    }
    else
    {
        framebuffer = context->getDrawFramebuffer();
        framebufferHandle = context->getDrawFramebufferHandle();
    }

    if (framebufferHandle == 0 || !framebuffer)
    {
        return gl::error(GL_INVALID_OPERATION, false);
    }

    return true;
}

bool ValidES3ReadFormatType(gl::Context *context, GLenum internalFormat, GLenum format, GLenum type)
{
    switch (format)
    {
      case GL_RGBA:
        switch (type)
        {
          case GL_UNSIGNED_BYTE:
            break;
          case GL_UNSIGNED_INT_2_10_10_10_REV:
            if (internalFormat != GL_RGB10_A2)
            {
                return false;
            }
            break;
          case GL_FLOAT:
            if (gl::GetComponentType(internalFormat, 3) != GL_FLOAT)
            {
                return false;
            }
            break;
          default:
            return false;
        }
        break;
      case GL_RGBA_INTEGER:
        switch (type)
        {
          case GL_INT:
            if (gl::GetComponentType(internalFormat, 3) != GL_INT)
            {
                return false;
            }
            break;
          case GL_UNSIGNED_INT:
            if (gl::GetComponentType(internalFormat, 3) != GL_UNSIGNED_INT)
            {
                return false;
            }
            break;
          default:
            return false;
        }
        break;
      case GL_BGRA_EXT:
        switch (type)
        {
          case GL_UNSIGNED_BYTE:
          case GL_UNSIGNED_SHORT_4_4_4_4_REV_EXT:
          case GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT:
            break;
          default:
            return false;
        }
        break;
      case GL_RG_EXT:
      case GL_RED_EXT:
        if (!context->supportsRGTextures())
        {
            return false;
        }
        switch (type)
        {
        case GL_UNSIGNED_BYTE:
            break;
        default:
            return false;
        }
        break;
      default:
        return false;
    }
    return true;
}

bool ValidateInvalidateFramebufferParameters(gl::Context *context, GLenum target, GLsizei numAttachments,
                                             const GLenum* attachments)
{
    bool defaultFramebuffer = false;

    switch (target)
    {
      case GL_DRAW_FRAMEBUFFER:
      case GL_FRAMEBUFFER:
        defaultFramebuffer = context->getDrawFramebufferHandle() == 0;
        break;
      case GL_READ_FRAMEBUFFER:
        defaultFramebuffer = context->getReadFramebufferHandle() == 0;
        break;
      default:
        return gl::error(GL_INVALID_ENUM, false);
    }

    for (int i = 0; i < numAttachments; ++i)
    {
        if (attachments[i] >= GL_COLOR_ATTACHMENT0 && attachments[i] <= GL_COLOR_ATTACHMENT15)
        {
            if (defaultFramebuffer)
            {
                return gl::error(GL_INVALID_ENUM, false);
            }

            if (attachments[i] >= GL_COLOR_ATTACHMENT0 + context->getMaximumRenderTargets())
            {
                return gl::error(GL_INVALID_OPERATION, false);
            }
        }
        else
        {
            switch (attachments[i])
            {
              case GL_DEPTH_ATTACHMENT:
              case GL_STENCIL_ATTACHMENT:
              case GL_DEPTH_STENCIL_ATTACHMENT:
                if (defaultFramebuffer)
                {
                    return gl::error(GL_INVALID_ENUM, false);
                }
                break;
              case GL_COLOR:
              case GL_DEPTH:
              case GL_STENCIL:
                if (!defaultFramebuffer)
                {
                    return gl::error(GL_INVALID_ENUM, false);
                }
                break;
              default:
                return gl::error(GL_INVALID_ENUM, false);
            }
        }
    }

    return true;
}

}
