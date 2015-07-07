#include "precompiled.h"
//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationES.h: Validation functions for generic OpenGL ES entry point parameters

#include "libGLESv2/validationES.h"
#include "libGLESv2/validationES2.h"
#include "libGLESv2/validationES3.h"
#include "libGLESv2/Context.h"
#include "libGLESv2/Texture.h"
#include "libGLESv2/Framebuffer.h"
#include "libGLESv2/Renderbuffer.h"
#include "libGLESv2/formatutils.h"
#include "libGLESv2/main.h"

#include "common/mathutil.h"
#include "common/utilities.h"

namespace gl
{

bool ValidCap(const Context *context, GLenum cap)
{
    switch (cap)
    {
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
        return (context->getClientVersion() >= 3);
      default:
        return false;
    }
}

bool ValidTextureTarget(const Context *context, GLenum target)
{
    switch (target)
    {
      case GL_TEXTURE_2D:
      case GL_TEXTURE_CUBE_MAP:
        return true;

      case GL_TEXTURE_3D:
      case GL_TEXTURE_2D_ARRAY:
        return (context->getClientVersion() >= 3);

      default:
        return false;
    }
}

// This function differs from ValidTextureTarget in that the target must be
// usable as the destination of a 2D operation-- so a cube face is valid, but
// GL_TEXTURE_CUBE_MAP is not.
bool ValidTexture2DDestinationTarget(const Context *context, GLenum target)
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
      case GL_TEXTURE_2D_ARRAY:
      case GL_TEXTURE_3D:
        return (context->getClientVersion() >= 3);
      default:
        return false;
    }
}

bool ValidFramebufferTarget(GLenum target)
{
    META_ASSERT(GL_DRAW_FRAMEBUFFER_ANGLE == GL_DRAW_FRAMEBUFFER && GL_READ_FRAMEBUFFER_ANGLE == GL_READ_FRAMEBUFFER);

    switch (target)
    {
      case GL_FRAMEBUFFER:      return true;
      case GL_READ_FRAMEBUFFER: return true;
      case GL_DRAW_FRAMEBUFFER: return true;
      default:                  return false;
    }
}

bool ValidBufferTarget(const Context *context, GLenum target)
{
    switch (target)
    {
      case GL_ARRAY_BUFFER:
      case GL_ELEMENT_ARRAY_BUFFER:
        return true;

      case GL_PIXEL_PACK_BUFFER:
      case GL_PIXEL_UNPACK_BUFFER:
      case GL_COPY_READ_BUFFER:
      case GL_COPY_WRITE_BUFFER:
      case GL_TRANSFORM_FEEDBACK_BUFFER:
      case GL_UNIFORM_BUFFER:
        return (context->getClientVersion() >= 3);

      default:
        return false;
    }
}

bool ValidBufferParameter(const Context *context, GLenum pname)
{
    switch (pname)
    {
      case GL_BUFFER_USAGE:
      case GL_BUFFER_SIZE:
        return true;

      // GL_BUFFER_MAP_POINTER is a special case, and may only be
      // queried with GetBufferPointerv
      case GL_BUFFER_ACCESS_FLAGS:
      case GL_BUFFER_MAPPED:
      case GL_BUFFER_MAP_OFFSET:
      case GL_BUFFER_MAP_LENGTH:
        return (context->getClientVersion() >= 3);

      default:
        return false;
    }
}

bool ValidMipLevel(const Context *context, GLenum target, GLint level)
{
    int maxLevel = 0;
    switch (target)
    {
      case GL_TEXTURE_2D:                  maxLevel = context->getMaximum2DTextureLevel();      break;
      case GL_TEXTURE_CUBE_MAP:
      case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
      case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
      case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
      case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
      case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
      case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z: maxLevel = context->getMaximumCubeTextureLevel();    break;
      case GL_TEXTURE_3D:                  maxLevel = context->getMaximum3DTextureLevel();      break;
      case GL_TEXTURE_2D_ARRAY:            maxLevel = context->getMaximum2DArrayTextureLevel(); break;
      default: UNREACHABLE();
    }

    return level < maxLevel;
}

bool ValidImageSize(const gl::Context *context, GLenum target, GLint level, GLsizei width, GLsizei height, GLsizei depth)
{
    if (level < 0 || width < 0 || height < 0 || depth < 0)
    {
        return false;
    }

    if (!context->supportsNonPower2Texture() && (level != 0 || !gl::isPow2(width) || !gl::isPow2(height) || !gl::isPow2(depth)))
    {
        return false;
    }

    if (!ValidMipLevel(context, target, level))
    {
        return false;
    }

    return true;
}

bool ValidCompressedImageSize(const gl::Context *context, GLenum internalFormat, GLsizei width, GLsizei height)
{
    GLuint clientVersion = context->getClientVersion();
    if (!IsFormatCompressed(internalFormat, clientVersion))
    {
        return false;
    }

    GLint blockWidth = GetCompressedBlockWidth(internalFormat, clientVersion);
    GLint blockHeight = GetCompressedBlockHeight(internalFormat, clientVersion);
    if (width  < 0 || (width  > blockWidth  && width  % blockWidth  != 0) ||
        height < 0 || (height > blockHeight && height % blockHeight != 0))
    {
        return false;
    }

    return true;
}

bool ValidQueryType(const Context *context, GLenum queryType)
{
    META_ASSERT(GL_ANY_SAMPLES_PASSED == GL_ANY_SAMPLES_PASSED_EXT);
    META_ASSERT(GL_ANY_SAMPLES_PASSED_CONSERVATIVE == GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT);

    switch (queryType)
    {
      case GL_ANY_SAMPLES_PASSED:
      case GL_ANY_SAMPLES_PASSED_CONSERVATIVE:
        return true;
      case GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN:
        return (context->getClientVersion() >= 3);
      default:
        return false;
    }
}

bool ValidProgram(const Context *context, GLuint id)
{
    // ES3 spec (section 2.11.1) -- "Commands that accept shader or program object names will generate the
    // error INVALID_VALUE if the provided name is not the name of either a shader or program object and
    // INVALID_OPERATION if the provided name identifies an object that is not the expected type."

    if (context->getProgram(id) != NULL)
    {
        return true;
    }
    else if (context->getShader(id) != NULL)
    {
        // ID is the wrong type
        return gl::error(GL_INVALID_OPERATION, false);
    }
    else
    {
        // No shader/program object has this ID
        return gl::error(GL_INVALID_VALUE, false);
    }
}

bool ValidateRenderbufferStorageParameters(const gl::Context *context, GLenum target, GLsizei samples,
                                           GLenum internalformat, GLsizei width, GLsizei height,
                                           bool angleExtension)
{
    switch (target)
    {
      case GL_RENDERBUFFER:
        break;
      default:
        return gl::error(GL_INVALID_ENUM, false);
    }

    if (width < 0 || height < 0 || samples < 0)
    {
        return gl::error(GL_INVALID_VALUE, false);
    }

    if (!gl::IsValidInternalFormat(internalformat, context))
    {
        return gl::error(GL_INVALID_ENUM, false);
    }

    // ANGLE_framebuffer_multisample does not explicitly state that the internal format must be
    // sized but it does state that the format must be in the ES2.0 spec table 4.5 which contains
    // only sized internal formats. The ES3 spec (section 4.4.2) does, however, state that the
    // internal format must be sized and not an integer format if samples is greater than zero.
    if (!gl::IsSizedInternalFormat(internalformat, context->getClientVersion()))
    {
        return gl::error(GL_INVALID_ENUM, false);
    }

    GLenum componentType = gl::GetComponentType(internalformat, context->getClientVersion());
    if ((componentType == GL_UNSIGNED_INT || componentType == GL_INT) && samples > 0)
    {
        return gl::error(GL_INVALID_OPERATION, false);
    }

    if (!gl::IsColorRenderingSupported(internalformat, context) &&
        !gl::IsDepthRenderingSupported(internalformat, context) &&
        !gl::IsStencilRenderingSupported(internalformat, context))
    {
        return gl::error(GL_INVALID_ENUM, false);
    }

    if (std::max(width, height) > context->getMaximumRenderbufferDimension())
    {
        return gl::error(GL_INVALID_VALUE, false);
    }

    // ANGLE_framebuffer_multisample states that the value of samples must be less than or equal
    // to MAX_SAMPLES_ANGLE (Context::getMaxSupportedSamples) while the ES3.0 spec (section 4.4.2)
    // states that samples must be less than or equal to the maximum samples for the specified
    // internal format.
    if (angleExtension)
    {
        if (samples > context->getMaxSupportedSamples())
        {
            return gl::error(GL_INVALID_VALUE, false);
        }
    }
    else
    {
        if (samples > context->getMaxSupportedFormatSamples(internalformat))
        {
            return gl::error(GL_INVALID_VALUE, false);
        }
    }

    GLuint handle = context->getRenderbufferHandle();
    if (handle == 0)
    {
        return gl::error(GL_INVALID_OPERATION, false);
    }

    return true;
}

bool ValidateFramebufferRenderbufferParameters(gl::Context *context, GLenum target, GLenum attachment,
                                               GLenum renderbuffertarget, GLuint renderbuffer)
{
    gl::Framebuffer *framebuffer = context->getTargetFramebuffer(target);
    GLuint framebufferHandle = context->getTargetFramebufferHandle(target);

    if (!framebuffer || (framebufferHandle == 0 && renderbuffer != 0))
    {
        return gl::error(GL_INVALID_OPERATION, false);
    }

    if (attachment >= GL_COLOR_ATTACHMENT0_EXT && attachment <= GL_COLOR_ATTACHMENT15_EXT)
    {
        const unsigned int colorAttachment = (attachment - GL_COLOR_ATTACHMENT0_EXT);

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
            break;
          case GL_STENCIL_ATTACHMENT:
            break;
          case GL_DEPTH_STENCIL_ATTACHMENT:
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_ENUM, false);
            }
            break;
          default:
            return gl::error(GL_INVALID_ENUM, false);
        }
    }

    // [OpenGL ES 2.0.25] Section 4.4.3 page 112
    // [OpenGL ES 3.0.2] Section 4.4.2 page 201
    // 'renderbuffer' must be either zero or the name of an existing renderbuffer object of
    // type 'renderbuffertarget', otherwise an INVALID_OPERATION error is generated.
    if (renderbuffer != 0)
    {
        if (!context->getRenderbuffer(renderbuffer))
        {
            return gl::error(GL_INVALID_OPERATION, false);
        }
    }

    return true;
}

static bool IsPartialBlit(gl::Context *context, gl::Renderbuffer *readBuffer, gl::Renderbuffer *writeBuffer,
                          GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                          GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1)
{
    if (srcX0 != 0 || srcY0 != 0 || dstX0 != 0 || dstY0 != 0 ||
        dstX1 != writeBuffer->getWidth() || dstY1 != writeBuffer->getHeight() ||
        srcX1 != readBuffer->getWidth() || srcY1 != readBuffer->getHeight())
    {
        return true;
    }
    else if (context->isScissorTestEnabled())
    {
        int scissorX, scissorY, scissorWidth, scissorHeight;
        context->getScissorParams(&scissorX, &scissorY, &scissorWidth, &scissorHeight);

        return scissorX > 0 || scissorY > 0 ||
               scissorWidth < writeBuffer->getWidth() ||
               scissorHeight < writeBuffer->getHeight();
    }
    else
    {
        return false;
    }
}

bool ValidateBlitFramebufferParameters(gl::Context *context, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                                       GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask,
                                       GLenum filter, bool fromAngleExtension)
{
    switch (filter)
    {
      case GL_NEAREST:
        break;
      case GL_LINEAR:
        if (fromAngleExtension)
        {
            return gl::error(GL_INVALID_ENUM, false);
        }
        break;
      default:
        return gl::error(GL_INVALID_ENUM, false);
    }

    if ((mask & ~(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)) != 0)
    {
        return gl::error(GL_INVALID_VALUE, false);
    }

    if (mask == 0)
    {
        // ES3.0 spec, section 4.3.2 specifies that a mask of zero is valid and no
        // buffers are copied.
        return false;
    }

    if (fromAngleExtension && (srcX1 - srcX0 != dstX1 - dstX0 || srcY1 - srcY0 != dstY1 - dstY0))
    {
        ERR("Scaling and flipping in BlitFramebufferANGLE not supported by this implementation.");
        return gl::error(GL_INVALID_OPERATION, false);
    }

    // ES3.0 spec, section 4.3.2 states that linear filtering is only available for the
    // color buffer, leaving only nearest being unfiltered from above
    if ((mask & ~GL_COLOR_BUFFER_BIT) != 0 && filter != GL_NEAREST)
    {
        return gl::error(GL_INVALID_OPERATION, false);
    }

    if (context->getReadFramebufferHandle() == context->getDrawFramebufferHandle())
    {
        if (fromAngleExtension)
        {
            ERR("Blits with the same source and destination framebuffer are not supported by this "
                "implementation.");
        }
        return gl::error(GL_INVALID_OPERATION, false);
    }

    gl::Framebuffer *readFramebuffer = context->getReadFramebuffer();
    gl::Framebuffer *drawFramebuffer = context->getDrawFramebuffer();
    if (!readFramebuffer || readFramebuffer->completeness() != GL_FRAMEBUFFER_COMPLETE ||
        !drawFramebuffer || drawFramebuffer->completeness() != GL_FRAMEBUFFER_COMPLETE)
    {
        return gl::error(GL_INVALID_FRAMEBUFFER_OPERATION, false);
    }

    if (drawFramebuffer->getSamples() != 0)
    {
        return gl::error(GL_INVALID_OPERATION, false);
    }

    bool sameBounds = srcX0 == dstX0 && srcY0 == dstY0 && srcX1 == dstX1 && srcY1 == dstY1;

    GLuint clientVersion = context->getClientVersion();

    if (mask & GL_COLOR_BUFFER_BIT)
    {
        gl::Renderbuffer *readColorBuffer = readFramebuffer->getReadColorbuffer();
        gl::Renderbuffer *drawColorBuffer = drawFramebuffer->getFirstColorbuffer();

        if (readColorBuffer && drawColorBuffer)
        {
            GLenum readInternalFormat = readColorBuffer->getActualFormat();
            GLenum readComponentType = gl::GetComponentType(readInternalFormat, clientVersion);

            for (unsigned int i = 0; i < gl::IMPLEMENTATION_MAX_DRAW_BUFFERS; i++)
            {
                if (drawFramebuffer->isEnabledColorAttachment(i))
                {
                    GLenum drawInternalFormat = drawFramebuffer->getColorbuffer(i)->getActualFormat();
                    GLenum drawComponentType = gl::GetComponentType(drawInternalFormat, clientVersion);

                    // The GL ES 3.0.2 spec (pg 193) states that:
                    // 1) If the read buffer is fixed point format, the draw buffer must be as well
                    // 2) If the read buffer is an unsigned integer format, the draw buffer must be as well
                    // 3) If the read buffer is a signed integer format, the draw buffer must be as well
                    if ( (readComponentType == GL_UNSIGNED_NORMALIZED || readComponentType == GL_SIGNED_NORMALIZED) &&
                        !(drawComponentType == GL_UNSIGNED_NORMALIZED || drawComponentType == GL_SIGNED_NORMALIZED))
                    {
                        return gl::error(GL_INVALID_OPERATION, false);
                    }

                    if (readComponentType == GL_UNSIGNED_INT && drawComponentType != GL_UNSIGNED_INT)
                    {
                        return gl::error(GL_INVALID_OPERATION, false);
                    }

                    if (readComponentType == GL_INT && drawComponentType != GL_INT)
                    {
                        return gl::error(GL_INVALID_OPERATION, false);
                    }

                    if (readColorBuffer->getSamples() > 0 && (readInternalFormat != drawInternalFormat || !sameBounds))
                    {
                        return gl::error(GL_INVALID_OPERATION, false);
                    }
                }
            }

            if ((readComponentType == GL_INT || readComponentType == GL_UNSIGNED_INT) && filter == GL_LINEAR)
            {
                return gl::error(GL_INVALID_OPERATION, false);
            }

            if (fromAngleExtension)
            {
                const GLenum readColorbufferType = readFramebuffer->getReadColorbufferType();
                if (readColorbufferType != GL_TEXTURE_2D && readColorbufferType != GL_RENDERBUFFER)
                {
                    return gl::error(GL_INVALID_OPERATION, false);
                }

                for (unsigned int colorAttachment = 0; colorAttachment < gl::IMPLEMENTATION_MAX_DRAW_BUFFERS; colorAttachment++)
                {
                    if (drawFramebuffer->isEnabledColorAttachment(colorAttachment))
                    {
                        if (drawFramebuffer->getColorbufferType(colorAttachment) != GL_TEXTURE_2D &&
                            drawFramebuffer->getColorbufferType(colorAttachment) != GL_RENDERBUFFER)
                        {
                            return gl::error(GL_INVALID_OPERATION, false);
                        }

                        if (drawFramebuffer->getColorbuffer(colorAttachment)->getActualFormat() != readColorBuffer->getActualFormat())
                        {
                            return gl::error(GL_INVALID_OPERATION, false);
                        }
                    }
                }
                if (readFramebuffer->getSamples() != 0 && IsPartialBlit(context, readColorBuffer, drawColorBuffer,
                                                                        srcX0, srcY0, srcX1, srcY1,
                                                                        dstX0, dstY0, dstX1, dstY1))
                {
                    return gl::error(GL_INVALID_OPERATION, false);
                }
            }
        }
    }

    if (mask & GL_DEPTH_BUFFER_BIT)
    {
        gl::Renderbuffer *readDepthBuffer = readFramebuffer->getDepthbuffer();
        gl::Renderbuffer *drawDepthBuffer = drawFramebuffer->getDepthbuffer();

        if (readDepthBuffer && drawDepthBuffer)
        {
            if (readDepthBuffer->getActualFormat() != drawDepthBuffer->getActualFormat())
            {
                return gl::error(GL_INVALID_OPERATION, false);
            }

            if (readDepthBuffer->getSamples() > 0 && !sameBounds)
            {
                return gl::error(GL_INVALID_OPERATION, false);
            }

            if (fromAngleExtension)
            {
                if (IsPartialBlit(context, readDepthBuffer, drawDepthBuffer,
                                  srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1))
                {
                    ERR("Only whole-buffer depth and stencil blits are supported by this implementation.");
                    return gl::error(GL_INVALID_OPERATION, false); // only whole-buffer copies are permitted
                }

                if (readDepthBuffer->getSamples() != 0 || drawDepthBuffer->getSamples() != 0)
                {
                    return gl::error(GL_INVALID_OPERATION, false);
                }
            }
        }
    }

    if (mask & GL_STENCIL_BUFFER_BIT)
    {
        gl::Renderbuffer *readStencilBuffer = readFramebuffer->getStencilbuffer();
        gl::Renderbuffer *drawStencilBuffer = drawFramebuffer->getStencilbuffer();

        if (readStencilBuffer && drawStencilBuffer)
        {
            if (readStencilBuffer->getActualFormat() != drawStencilBuffer->getActualFormat())
            {
                return gl::error(GL_INVALID_OPERATION, false);
            }

            if (readStencilBuffer->getSamples() > 0 && !sameBounds)
            {
                return gl::error(GL_INVALID_OPERATION, false);
            }

            if (fromAngleExtension)
            {
                if (IsPartialBlit(context, readStencilBuffer, drawStencilBuffer,
                                  srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1))
                {
                    ERR("Only whole-buffer depth and stencil blits are supported by this implementation.");
                    return gl::error(GL_INVALID_OPERATION, false); // only whole-buffer copies are permitted
                }

                if (readStencilBuffer->getSamples() != 0 || drawStencilBuffer->getSamples() != 0)
                {
                    return gl::error(GL_INVALID_OPERATION, false);
                }
            }
        }
    }

    return true;
}

bool ValidateGetVertexAttribParameters(GLenum pname, int clientVersion)
{
    switch (pname)
    {
      case GL_VERTEX_ATTRIB_ARRAY_ENABLED:
      case GL_VERTEX_ATTRIB_ARRAY_SIZE:
      case GL_VERTEX_ATTRIB_ARRAY_STRIDE:
      case GL_VERTEX_ATTRIB_ARRAY_TYPE:
      case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:
      case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
      case GL_CURRENT_VERTEX_ATTRIB:
        return true;

      case GL_VERTEX_ATTRIB_ARRAY_DIVISOR:
        // Don't verify ES3 context because GL_VERTEX_ATTRIB_ARRAY_DIVISOR_ANGLE uses
        // the same constant.
        META_ASSERT(GL_VERTEX_ATTRIB_ARRAY_DIVISOR == GL_VERTEX_ATTRIB_ARRAY_DIVISOR_ANGLE);
        return true;

      case GL_VERTEX_ATTRIB_ARRAY_INTEGER:
        return ((clientVersion >= 3) ? true : gl::error(GL_INVALID_ENUM, false));

      default:
        return gl::error(GL_INVALID_ENUM, false);
    }
}

bool ValidateTexParamParameters(gl::Context *context, GLenum pname, GLint param)
{
    switch (pname)
    {
      case GL_TEXTURE_WRAP_R:
      case GL_TEXTURE_SWIZZLE_R:
      case GL_TEXTURE_SWIZZLE_G:
      case GL_TEXTURE_SWIZZLE_B:
      case GL_TEXTURE_SWIZZLE_A:
      case GL_TEXTURE_BASE_LEVEL:
      case GL_TEXTURE_MAX_LEVEL:
      case GL_TEXTURE_COMPARE_MODE:
      case GL_TEXTURE_COMPARE_FUNC:
      case GL_TEXTURE_MIN_LOD:
      case GL_TEXTURE_MAX_LOD:
        if (context->getClientVersion() < 3)
        {
            return gl::error(GL_INVALID_ENUM, false);
        }
        break;

      default: break;
    }

    switch (pname)
    {
      case GL_TEXTURE_WRAP_S:
      case GL_TEXTURE_WRAP_T:
      case GL_TEXTURE_WRAP_R:
        switch (param)
        {
          case GL_REPEAT:
          case GL_CLAMP_TO_EDGE:
          case GL_MIRRORED_REPEAT:
            return true;
          default:
            return gl::error(GL_INVALID_ENUM, false);
        }

      case GL_TEXTURE_MIN_FILTER:
        switch (param)
        {
          case GL_NEAREST:
          case GL_LINEAR:
          case GL_NEAREST_MIPMAP_NEAREST:
          case GL_LINEAR_MIPMAP_NEAREST:
          case GL_NEAREST_MIPMAP_LINEAR:
          case GL_LINEAR_MIPMAP_LINEAR:
            return true;
          default:
            return gl::error(GL_INVALID_ENUM, false);
        }
        break;

      case GL_TEXTURE_MAG_FILTER:
        switch (param)
        {
          case GL_NEAREST:
          case GL_LINEAR:
            return true;
          default:
            return gl::error(GL_INVALID_ENUM, false);
        }
        break;

      case GL_TEXTURE_USAGE_ANGLE:
        switch (param)
        {
          case GL_NONE:
          case GL_FRAMEBUFFER_ATTACHMENT_ANGLE:
            return true;
          default:
            return gl::error(GL_INVALID_ENUM, false);
        }
        break;

      case GL_TEXTURE_MAX_ANISOTROPY_EXT:
        if (!context->supportsTextureFilterAnisotropy())
        {
            return gl::error(GL_INVALID_ENUM, false);
        }

        // we assume the parameter passed to this validation method is truncated, not rounded
        if (param < 1)
        {
            return gl::error(GL_INVALID_VALUE, false);
        }
        return true;

      case GL_TEXTURE_MIN_LOD:
      case GL_TEXTURE_MAX_LOD:
        // any value is permissible
        return true;

      case GL_TEXTURE_COMPARE_MODE:
        // Acceptable mode parameters from GLES 3.0.2 spec, table 3.17
        switch (param)
        {
          case GL_NONE:
          case GL_COMPARE_REF_TO_TEXTURE:
            return true;
          default:
            return gl::error(GL_INVALID_ENUM, false);
        }
        break;

      case GL_TEXTURE_COMPARE_FUNC:
        // Acceptable function parameters from GLES 3.0.2 spec, table 3.17
        switch (param)
        {
          case GL_LEQUAL:
          case GL_GEQUAL:
          case GL_LESS:
          case GL_GREATER:
          case GL_EQUAL:
          case GL_NOTEQUAL:
          case GL_ALWAYS:
          case GL_NEVER:
            return true;
          default:
            return gl::error(GL_INVALID_ENUM, false);
        }
        break;

      case GL_TEXTURE_SWIZZLE_R:
      case GL_TEXTURE_SWIZZLE_G:
      case GL_TEXTURE_SWIZZLE_B:
      case GL_TEXTURE_SWIZZLE_A:
        switch (param)
        {
          case GL_RED:
          case GL_GREEN:
          case GL_BLUE:
          case GL_ALPHA:
          case GL_ZERO:
          case GL_ONE:
            return true;
          default:
            return gl::error(GL_INVALID_ENUM, false);
        }
        break;

      case GL_TEXTURE_BASE_LEVEL:
      case GL_TEXTURE_MAX_LEVEL:
        if (param < 0)
        {
            return gl::error(GL_INVALID_VALUE, false);
        }
        return true;

      default:
        return gl::error(GL_INVALID_ENUM, false);
    }
}

bool ValidateSamplerObjectParameter(GLenum pname)
{
    switch (pname)
    {
      case GL_TEXTURE_MIN_FILTER:
      case GL_TEXTURE_MAG_FILTER:
      case GL_TEXTURE_WRAP_S:
      case GL_TEXTURE_WRAP_T:
      case GL_TEXTURE_WRAP_R:
      case GL_TEXTURE_MIN_LOD:
      case GL_TEXTURE_MAX_LOD:
      case GL_TEXTURE_COMPARE_MODE:
      case GL_TEXTURE_COMPARE_FUNC:
        return true;

      default:
        return gl::error(GL_INVALID_ENUM, false);
    }
}

bool ValidateReadPixelsParameters(gl::Context *context, GLint x, GLint y, GLsizei width, GLsizei height,
                                  GLenum format, GLenum type, GLsizei *bufSize, GLvoid *pixels)
{
    gl::Framebuffer *framebuffer = context->getReadFramebuffer();

    if (framebuffer->completeness() != GL_FRAMEBUFFER_COMPLETE)
    {
        return gl::error(GL_INVALID_FRAMEBUFFER_OPERATION, false);
    }

    if (context->getReadFramebufferHandle() != 0 && framebuffer->getSamples() != 0)
    {
        return gl::error(GL_INVALID_OPERATION, false);
    }

    GLenum currentInternalFormat, currentFormat, currentType;
    int clientVersion = context->getClientVersion();

    // Failure in getCurrentReadFormatType indicates that no color attachment is currently bound,
    // and attempting to read back if that's the case is an error. The error will be registered
    // by getCurrentReadFormat.
    // Note: we need to explicitly check for framebuffer completeness here, before we call
    // getCurrentReadFormatType, because it generates a different (wrong) error for incomplete FBOs
    if (!context->getCurrentReadFormatType(&currentInternalFormat, &currentFormat, &currentType))
        return false;

    bool validReadFormat = (clientVersion < 3) ? ValidES2ReadFormatType(context, format, type) :
                                                 ValidES3ReadFormatType(context, currentInternalFormat, format, type);

    if (!(currentFormat == format && currentType == type) && !validReadFormat)
    {
        return gl::error(GL_INVALID_OPERATION, false);
    }

    GLenum sizedInternalFormat = IsSizedInternalFormat(format, clientVersion) ? format :
                                 GetSizedInternalFormat(format, type, clientVersion);

    GLsizei outputPitch = GetRowPitch(sizedInternalFormat, type, clientVersion, width, context->getPackAlignment());
    // sized query sanity check
    if (bufSize)
    {
        int requiredSize = outputPitch * height;
        if (requiredSize > *bufSize)
        {
            return gl::error(GL_INVALID_OPERATION, false);
        }
    }

    return true;
}

}
