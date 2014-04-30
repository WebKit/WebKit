#include "precompiled.h"
//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// libGLESv2.cpp: Implements the exported OpenGL ES 2.0 functions.

#include "common/version.h"

#include "libGLESv2/main.h"
#include "common/utilities.h"
#include "libGLESv2/formatutils.h"
#include "libGLESv2/Buffer.h"
#include "libGLESv2/Fence.h"
#include "libGLESv2/Framebuffer.h"
#include "libGLESv2/Renderbuffer.h"
#include "libGLESv2/Program.h"
#include "libGLESv2/ProgramBinary.h"
#include "libGLESv2/Texture.h"
#include "libGLESv2/Query.h"
#include "libGLESv2/Context.h"
#include "libGLESv2/VertexArray.h"
#include "libGLESv2/TransformFeedback.h"

#include "libGLESv2/validationES.h"
#include "libGLESv2/validationES2.h"
#include "libGLESv2/validationES3.h"
#include "libGLESv2/queryconversions.h"

extern "C"
{

// OpenGL ES 2.0 functions

void __stdcall glActiveTexture(GLenum texture)
{
    EVENT("(GLenum texture = 0x%X)", texture);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (texture < GL_TEXTURE0 || texture > GL_TEXTURE0 + context->getMaximumCombinedTextureImageUnits() - 1)
            {
                return gl::error(GL_INVALID_ENUM);
            }

            context->setActiveSampler(texture - GL_TEXTURE0);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glAttachShader(GLuint program, GLuint shader)
{
    EVENT("(GLuint program = %d, GLuint shader = %d)", program, shader);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            gl::Program *programObject = context->getProgram(program);
            gl::Shader *shaderObject = context->getShader(shader);

            if (!programObject)
            {
                if (context->getShader(program))
                {
                    return gl::error(GL_INVALID_OPERATION);
                }
                else
                {
                    return gl::error(GL_INVALID_VALUE);
                }
            }

            if (!shaderObject)
            {
                if (context->getProgram(shader))
                {
                    return gl::error(GL_INVALID_OPERATION);
                }
                else
                {
                    return gl::error(GL_INVALID_VALUE);
                }
            }

            if (!programObject->attachShader(shaderObject))
            {
                return gl::error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glBeginQueryEXT(GLenum target, GLuint id)
{
    EVENT("(GLenum target = 0x%X, GLuint %d)", target, id);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (!ValidQueryType(context, target))
            {
                return gl::error(GL_INVALID_ENUM);
            }

            if (id == 0)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            context->beginQuery(target, id);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glBindAttribLocation(GLuint program, GLuint index, const GLchar* name)
{
    EVENT("(GLuint program = %d, GLuint index = %d, const GLchar* name = 0x%0.8p)", program, index, name);

    try
    {
        if (index >= gl::MAX_VERTEX_ATTRIBS)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            gl::Program *programObject = context->getProgram(program);

            if (!programObject)
            {
                if (context->getShader(program))
                {
                    return gl::error(GL_INVALID_OPERATION);
                }
                else
                {
                    return gl::error(GL_INVALID_VALUE);
                }
            }

            if (strncmp(name, "gl_", 3) == 0)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            programObject->bindAttributeLocation(index, name);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glBindBuffer(GLenum target, GLuint buffer)
{
    EVENT("(GLenum target = 0x%X, GLuint buffer = %d)", target, buffer);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (!gl::ValidBufferTarget(context, target))
            {
                return gl::error(GL_INVALID_ENUM);
            }

            switch (target)
            {
              case GL_ARRAY_BUFFER:
                context->bindArrayBuffer(buffer);
                return;
              case GL_ELEMENT_ARRAY_BUFFER:
                context->bindElementArrayBuffer(buffer);
                return;
              case GL_COPY_READ_BUFFER:
                context->bindCopyReadBuffer(buffer);
                return;
              case GL_COPY_WRITE_BUFFER:
                context->bindCopyWriteBuffer(buffer);
                return;
              case GL_PIXEL_PACK_BUFFER:
                context->bindPixelPackBuffer(buffer);
                return;
              case GL_PIXEL_UNPACK_BUFFER:
                context->bindPixelUnpackBuffer(buffer);
                return;
              case GL_UNIFORM_BUFFER:
                context->bindGenericUniformBuffer(buffer);
                return;
              case GL_TRANSFORM_FEEDBACK_BUFFER:
                context->bindGenericTransformFeedbackBuffer(buffer);
                return;
              default:
                return gl::error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glBindFramebuffer(GLenum target, GLuint framebuffer)
{
    EVENT("(GLenum target = 0x%X, GLuint framebuffer = %d)", target, framebuffer);

    try
    {
        if (!gl::ValidFramebufferTarget(target))
        {
            return gl::error(GL_INVALID_ENUM);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (target == GL_READ_FRAMEBUFFER_ANGLE || target == GL_FRAMEBUFFER)
            {
                context->bindReadFramebuffer(framebuffer);
            }
            
            if (target == GL_DRAW_FRAMEBUFFER_ANGLE || target == GL_FRAMEBUFFER)
            {
                context->bindDrawFramebuffer(framebuffer);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glBindRenderbuffer(GLenum target, GLuint renderbuffer)
{
    EVENT("(GLenum target = 0x%X, GLuint renderbuffer = %d)", target, renderbuffer);

    try
    {
        if (target != GL_RENDERBUFFER)
        {
            return gl::error(GL_INVALID_ENUM);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            context->bindRenderbuffer(renderbuffer);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glBindTexture(GLenum target, GLuint texture)
{
    EVENT("(GLenum target = 0x%X, GLuint texture = %d)", target, texture);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            gl::Texture *textureObject = context->getTexture(texture);

            if (textureObject && textureObject->getTarget() != target && texture != 0)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            switch (target)
            {
              case GL_TEXTURE_2D:
                context->bindTexture2D(texture);
                return;
              case GL_TEXTURE_CUBE_MAP:
                context->bindTextureCubeMap(texture);
                return;
              case GL_TEXTURE_3D:
                if (context->getClientVersion() < 3)
                {
                    return gl::error(GL_INVALID_ENUM);
                }
                context->bindTexture3D(texture);
                return;
              case GL_TEXTURE_2D_ARRAY:
                if (context->getClientVersion() < 3)
                {
                    return gl::error(GL_INVALID_ENUM);
                }
                context->bindTexture2DArray(texture);
                return;
              default:
                return gl::error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glBlendColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
    EVENT("(GLclampf red = %f, GLclampf green = %f, GLclampf blue = %f, GLclampf alpha = %f)",
          red, green, blue, alpha);

    try
    {
        gl::Context* context = gl::getNonLostContext();

        if (context)
        {
            context->setBlendColor(gl::clamp01(red), gl::clamp01(green), gl::clamp01(blue), gl::clamp01(alpha));
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glBlendEquation(GLenum mode)
{
    glBlendEquationSeparate(mode, mode);
}

void __stdcall glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha)
{
    EVENT("(GLenum modeRGB = 0x%X, GLenum modeAlpha = 0x%X)", modeRGB, modeAlpha);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        switch (modeRGB)
        {
          case GL_FUNC_ADD:
          case GL_FUNC_SUBTRACT:
          case GL_FUNC_REVERSE_SUBTRACT:
          case GL_MIN:
          case GL_MAX:
            break;

          default:
            return gl::error(GL_INVALID_ENUM);
        }

        switch (modeAlpha)
        {
          case GL_FUNC_ADD:
          case GL_FUNC_SUBTRACT:
          case GL_FUNC_REVERSE_SUBTRACT:
          case GL_MIN:
          case GL_MAX:
            break;

          default:
            return gl::error(GL_INVALID_ENUM);
        }

        if (context)
        {
            context->setBlendEquation(modeRGB, modeAlpha);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glBlendFunc(GLenum sfactor, GLenum dfactor)
{
    glBlendFuncSeparate(sfactor, dfactor, sfactor, dfactor);
}

void __stdcall glBlendFuncSeparate(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha)
{
    EVENT("(GLenum srcRGB = 0x%X, GLenum dstRGB = 0x%X, GLenum srcAlpha = 0x%X, GLenum dstAlpha = 0x%X)",
          srcRGB, dstRGB, srcAlpha, dstAlpha);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        switch (srcRGB)
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
            break;
          default:
            return gl::error(GL_INVALID_ENUM);
        }

        switch (dstRGB)
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
            break;

          case GL_SRC_ALPHA_SATURATE:
            if (!context || context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_ENUM);
            }
            break;

          default:
            return gl::error(GL_INVALID_ENUM);
        }

        switch (srcAlpha)
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
            break;
          default:
            return gl::error(GL_INVALID_ENUM);
        }

        switch (dstAlpha)
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
            break;

          case GL_SRC_ALPHA_SATURATE:
            if (!context || context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_ENUM);
            }
            break;

          default:
            return gl::error(GL_INVALID_ENUM);
        }

        bool constantColorUsed = (srcRGB == GL_CONSTANT_COLOR || srcRGB == GL_ONE_MINUS_CONSTANT_COLOR ||
                                  dstRGB == GL_CONSTANT_COLOR || dstRGB == GL_ONE_MINUS_CONSTANT_COLOR);

        bool constantAlphaUsed = (srcRGB == GL_CONSTANT_ALPHA || srcRGB == GL_ONE_MINUS_CONSTANT_ALPHA ||
                                  dstRGB == GL_CONSTANT_ALPHA || dstRGB == GL_ONE_MINUS_CONSTANT_ALPHA);

        if (constantColorUsed && constantAlphaUsed)
        {
            ERR("Simultaneous use of GL_CONSTANT_ALPHA/GL_ONE_MINUS_CONSTANT_ALPHA and GL_CONSTANT_COLOR/GL_ONE_MINUS_CONSTANT_COLOR invalid under WebGL");
            return gl::error(GL_INVALID_OPERATION);
        }

        if (context)
        {
            context->setBlendFactors(srcRGB, dstRGB, srcAlpha, dstAlpha);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glBufferData(GLenum target, GLsizeiptr size, const GLvoid* data, GLenum usage)
{
    EVENT("(GLenum target = 0x%X, GLsizeiptr size = %d, const GLvoid* data = 0x%0.8p, GLenum usage = %d)",
          target, size, data, usage);

    try
    {
        if (size < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

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
            if (context && context->getClientVersion() < 3)
            {
              return gl::error(GL_INVALID_ENUM);
            }
            break;

          default:
            return gl::error(GL_INVALID_ENUM);
        }

        if (context)
        {
            if (!gl::ValidBufferTarget(context, target))
            {
                return gl::error(GL_INVALID_ENUM);
            }

            gl::Buffer *buffer = context->getTargetBuffer(target);

            if (!buffer)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            buffer->bufferData(data, size, usage);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid* data)
{
    EVENT("(GLenum target = 0x%X, GLintptr offset = %d, GLsizeiptr size = %d, const GLvoid* data = 0x%0.8p)",
          target, offset, size, data);

    try
    {
        if (size < 0 || offset < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        if (data == NULL)
        {
            return;
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (!gl::ValidBufferTarget(context, target))
            {
                return gl::error(GL_INVALID_ENUM);
            }

            gl::Buffer *buffer = context->getTargetBuffer(target);

            if (!buffer)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (buffer->mapped())
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if ((size_t)size + offset > buffer->size())
            {
                return gl::error(GL_INVALID_VALUE);
            }

            buffer->bufferSubData(data, size, offset);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

GLenum __stdcall glCheckFramebufferStatus(GLenum target)
{
    EVENT("(GLenum target = 0x%X)", target);

    try
    {
        if (!gl::ValidFramebufferTarget(target))
        {
            return gl::error(GL_INVALID_ENUM, 0);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            gl::Framebuffer *framebuffer = context->getTargetFramebuffer(target);
            ASSERT(framebuffer);
            return framebuffer->completeness();
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY, 0);
    }

    return 0;
}

void __stdcall glClear(GLbitfield mask)
{
    EVENT("(GLbitfield mask = 0x%X)", mask);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            gl::Framebuffer *framebufferObject = context->getDrawFramebuffer();

            if (!framebufferObject || framebufferObject->completeness() != GL_FRAMEBUFFER_COMPLETE)
            {
                return gl::error(GL_INVALID_FRAMEBUFFER_OPERATION);
            }

            if ((mask & ~(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) != 0)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            context->clear(mask);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
    EVENT("(GLclampf red = %f, GLclampf green = %f, GLclampf blue = %f, GLclampf alpha = %f)",
          red, green, blue, alpha);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            context->setClearColor(red, green, blue, alpha);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glClearDepthf(GLclampf depth)
{
    EVENT("(GLclampf depth = %f)", depth);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            context->setClearDepth(depth);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glClearStencil(GLint s)
{
    EVENT("(GLint s = %d)", s);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            context->setClearStencil(s);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
    EVENT("(GLboolean red = %d, GLboolean green = %u, GLboolean blue = %u, GLboolean alpha = %u)",
          red, green, blue, alpha);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            context->setColorMask(red == GL_TRUE, green == GL_TRUE, blue == GL_TRUE, alpha == GL_TRUE);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glCompileShader(GLuint shader)
{
    EVENT("(GLuint shader = %d)", shader);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            gl::Shader *shaderObject = context->getShader(shader);

            if (!shaderObject)
            {
                if (context->getProgram(shader))
                {
                    return gl::error(GL_INVALID_OPERATION);
                }
                else
                {
                    return gl::error(GL_INVALID_VALUE);
                }
            }

            shaderObject->compile();
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, 
                                      GLint border, GLsizei imageSize, const GLvoid* data)
{
    EVENT("(GLenum target = 0x%X, GLint level = %d, GLenum internalformat = 0x%X, GLsizei width = %d, " 
          "GLsizei height = %d, GLint border = %d, GLsizei imageSize = %d, const GLvoid* data = 0x%0.8p)",
          target, level, internalformat, width, height, border, imageSize, data);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3 &&
                !ValidateES2TexImageParameters(context, target, level, internalformat, true, false,
                                               0, 0, width, height, 0, GL_NONE, GL_NONE, data))
            {
                return;
            }

            if (context->getClientVersion() >= 3 &&
                !ValidateES3TexImageParameters(context, target, level, internalformat, true, false,
                                               0, 0, 0, width, height, 1, 0, GL_NONE, GL_NONE, data))
            {
                return;
            }

            if (imageSize < 0 || imageSize != (GLsizei)gl::GetBlockSize(internalformat, GL_UNSIGNED_BYTE, context->getClientVersion(), width, height))
            {
                return gl::error(GL_INVALID_VALUE);
            }

            switch (target)
            {
              case GL_TEXTURE_2D:
                {
                    gl::Texture2D *texture = context->getTexture2D();
                    texture->setCompressedImage(level, internalformat, width, height, imageSize, data);
                }
                break;

              case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
              case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
              case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
              case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
              case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
              case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
                {
                    gl::TextureCubeMap *texture = context->getTextureCubeMap();
                    texture->setCompressedImage(target, level, internalformat, width, height, imageSize, data);
                }
                break;

              default:
                return gl::error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                         GLenum format, GLsizei imageSize, const GLvoid* data)
{
    EVENT("(GLenum target = 0x%X, GLint level = %d, GLint xoffset = %d, GLint yoffset = %d, "
          "GLsizei width = %d, GLsizei height = %d, GLenum format = 0x%X, "
          "GLsizei imageSize = %d, const GLvoid* data = 0x%0.8p)",
          target, level, xoffset, yoffset, width, height, format, imageSize, data);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3 &&
                !ValidateES2TexImageParameters(context, target, level, GL_NONE, true, true,
                                               xoffset, yoffset, width, height, 0, GL_NONE, GL_NONE, data))
            {
                return;
            }

            if (context->getClientVersion() >= 3 &&
                !ValidateES3TexImageParameters(context, target, level, GL_NONE, true, true,
                                               xoffset, yoffset, 0, width, height, 1, 0, GL_NONE, GL_NONE, data))
            {
                return;
            }

            if (imageSize < 0 || imageSize != (GLsizei)gl::GetBlockSize(format, GL_UNSIGNED_BYTE, context->getClientVersion(), width, height))
            {
                return gl::error(GL_INVALID_VALUE);
            }

            switch (target)
            {
              case GL_TEXTURE_2D:
                {
                    gl::Texture2D *texture = context->getTexture2D();
                    texture->subImageCompressed(level, xoffset, yoffset, width, height, format, imageSize, data);
                }
                break;

              case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
              case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
              case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
              case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
              case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
              case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
                {
                    gl::TextureCubeMap *texture = context->getTextureCubeMap();
                    texture->subImageCompressed(target, level, xoffset, yoffset, width, height, format, imageSize, data);
                }
                break;

              default:
                return gl::error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border)
{
    EVENT("(GLenum target = 0x%X, GLint level = %d, GLenum internalformat = 0x%X, "
          "GLint x = %d, GLint y = %d, GLsizei width = %d, GLsizei height = %d, GLint border = %d)",
          target, level, internalformat, x, y, width, height, border);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3 &&
                !ValidateES2CopyTexImageParameters(context, target, level, internalformat, false,
                                                   0, 0, x, y, width, height, border))
            {
                return;
            }

            if (context->getClientVersion() >= 3 &&
                !ValidateES3CopyTexImageParameters(context, target, level, internalformat, false,
                                                   0, 0, 0, x, y, width, height, border))
            {
                return;
            }

            gl::Framebuffer *framebuffer = context->getReadFramebuffer();

            switch (target)
            {
              case GL_TEXTURE_2D:
                {
                    gl::Texture2D *texture = context->getTexture2D();
                    texture->copyImage(level, internalformat, x, y, width, height, framebuffer);
                }
                break;

              case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
              case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
              case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
              case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
              case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
              case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
                {
                    gl::TextureCubeMap *texture = context->getTextureCubeMap();
                    texture->copyImage(target, level, internalformat, x, y, width, height, framebuffer);
                }
                break;

             default:
                return gl::error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
    EVENT("(GLenum target = 0x%X, GLint level = %d, GLint xoffset = %d, GLint yoffset = %d, "
          "GLint x = %d, GLint y = %d, GLsizei width = %d, GLsizei height = %d)",
          target, level, xoffset, yoffset, x, y, width, height);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3 &&
                !ValidateES2CopyTexImageParameters(context, target, level, GL_NONE, true,
                                                   xoffset, yoffset, x, y, width, height, 0))
            {
                return;
            }

            if (context->getClientVersion() >= 3 &&
                !ValidateES3CopyTexImageParameters(context, target, level, GL_NONE, true,
                                                   xoffset, yoffset, 0, x, y, width, height, 0))
            {
                return;
            }

            gl::Framebuffer *framebuffer = context->getReadFramebuffer();

            switch (target)
            {
              case GL_TEXTURE_2D:
                {
                    gl::Texture2D *texture = context->getTexture2D();
                    texture->copySubImage(target, level, xoffset, yoffset, 0, x, y, width, height, framebuffer);
                }
                break;

              case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
              case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
              case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
              case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
              case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
              case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
                {
                    gl::TextureCubeMap *texture = context->getTextureCubeMap();
                    texture->copySubImage(target, level, xoffset, yoffset, 0, x, y, width, height, framebuffer);
                }
                break;

              default:
                return gl::error(GL_INVALID_ENUM);
            }
        }
    }

    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

GLuint __stdcall glCreateProgram(void)
{
    EVENT("()");

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            return context->createProgram();
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY, 0);
    }

    return 0;
}

GLuint __stdcall glCreateShader(GLenum type)
{
    EVENT("(GLenum type = 0x%X)", type);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            switch (type)
            {
              case GL_FRAGMENT_SHADER:
              case GL_VERTEX_SHADER:
                return context->createShader(type);
              default:
                return gl::error(GL_INVALID_ENUM, 0);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY, 0);
    }

    return 0;
}

void __stdcall glCullFace(GLenum mode)
{
    EVENT("(GLenum mode = 0x%X)", mode);

    try
    {
        switch (mode)
        {
          case GL_FRONT:
          case GL_BACK:
          case GL_FRONT_AND_BACK:
            {
                gl::Context *context = gl::getNonLostContext();

                if (context)
                {
                    context->setCullMode(mode);
                }
            }
            break;
          default:
            return gl::error(GL_INVALID_ENUM);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDeleteBuffers(GLsizei n, const GLuint* buffers)
{
    EVENT("(GLsizei n = %d, const GLuint* buffers = 0x%0.8p)", n, buffers);

    try
    {
        if (n < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            for (int i = 0; i < n; i++)
            {
                context->deleteBuffer(buffers[i]);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDeleteFencesNV(GLsizei n, const GLuint* fences)
{
    EVENT("(GLsizei n = %d, const GLuint* fences = 0x%0.8p)", n, fences);

    try
    {
        if (n < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            for (int i = 0; i < n; i++)
            {
                context->deleteFenceNV(fences[i]);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDeleteFramebuffers(GLsizei n, const GLuint* framebuffers)
{
    EVENT("(GLsizei n = %d, const GLuint* framebuffers = 0x%0.8p)", n, framebuffers);

    try
    {
        if (n < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            for (int i = 0; i < n; i++)
            {
                if (framebuffers[i] != 0)
                {
                    context->deleteFramebuffer(framebuffers[i]);
                }
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDeleteProgram(GLuint program)
{
    EVENT("(GLuint program = %d)", program);

    try
    {
        if (program == 0)
        {
            return;
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (!context->getProgram(program))
            {
                if(context->getShader(program))
                {
                    return gl::error(GL_INVALID_OPERATION);
                }
                else
                {
                    return gl::error(GL_INVALID_VALUE);
                }
            }

            context->deleteProgram(program);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDeleteQueriesEXT(GLsizei n, const GLuint *ids)
{
    EVENT("(GLsizei n = %d, const GLuint *ids = 0x%0.8p)", n, ids);

    try
    {
        if (n < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            for (int i = 0; i < n; i++)
            {
                context->deleteQuery(ids[i]);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers)
{
    EVENT("(GLsizei n = %d, const GLuint* renderbuffers = 0x%0.8p)", n, renderbuffers);

    try
    {
        if (n < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            for (int i = 0; i < n; i++)
            {
                context->deleteRenderbuffer(renderbuffers[i]);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDeleteShader(GLuint shader)
{
    EVENT("(GLuint shader = %d)", shader);

    try
    {
        if (shader == 0)
        {
            return;
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (!context->getShader(shader))
            {
                if(context->getProgram(shader))
                {
                    return gl::error(GL_INVALID_OPERATION);
                }
                else
                {
                    return gl::error(GL_INVALID_VALUE);
                }
            }

            context->deleteShader(shader);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDeleteTextures(GLsizei n, const GLuint* textures)
{
    EVENT("(GLsizei n = %d, const GLuint* textures = 0x%0.8p)", n, textures);

    try
    {
        if (n < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            for (int i = 0; i < n; i++)
            {
                if (textures[i] != 0)
                {
                    context->deleteTexture(textures[i]);
                }
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDepthFunc(GLenum func)
{
    EVENT("(GLenum func = 0x%X)", func);

    try
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
            return gl::error(GL_INVALID_ENUM);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            context->setDepthFunc(func);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDepthMask(GLboolean flag)
{
    EVENT("(GLboolean flag = %u)", flag);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            context->setDepthMask(flag != GL_FALSE);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDepthRangef(GLclampf zNear, GLclampf zFar)
{
    EVENT("(GLclampf zNear = %f, GLclampf zFar = %f)", zNear, zFar);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            context->setDepthRange(zNear, zFar);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDetachShader(GLuint program, GLuint shader)
{
    EVENT("(GLuint program = %d, GLuint shader = %d)", program, shader);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {

            gl::Program *programObject = context->getProgram(program);
            gl::Shader *shaderObject = context->getShader(shader);
            
            if (!programObject)
            {
                gl::Shader *shaderByProgramHandle;
                shaderByProgramHandle = context->getShader(program);
                if (!shaderByProgramHandle)
                {
                    return gl::error(GL_INVALID_VALUE);
                }
                else
                {
                    return gl::error(GL_INVALID_OPERATION);
                }
            }

            if (!shaderObject)
            {
                gl::Program *programByShaderHandle = context->getProgram(shader);
                if (!programByShaderHandle)
                {
                    return gl::error(GL_INVALID_VALUE);
                }
                else
                {
                    return gl::error(GL_INVALID_OPERATION);
                }
            }

            if (!programObject->detachShader(shaderObject))
            {
                return gl::error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDisable(GLenum cap)
{
    EVENT("(GLenum cap = 0x%X)", cap);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (!ValidCap(context, cap))
            {
                return gl::error(GL_INVALID_ENUM);
            }

            context->setCap(cap, false);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDisableVertexAttribArray(GLuint index)
{
    EVENT("(GLuint index = %d)", index);

    try
    {
        if (index >= gl::MAX_VERTEX_ATTRIBS)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            context->setEnableVertexAttribArray(index, false);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
    EVENT("(GLenum mode = 0x%X, GLint first = %d, GLsizei count = %d)", mode, first, count);

    try
    {
        if (count < 0 || first < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        // Check for mapped buffers
        if (context->hasMappedBuffer(GL_ARRAY_BUFFER))
        {
            return gl::error(GL_INVALID_OPERATION);
        }

        if (context)
        {
            gl::TransformFeedback *curTransformFeedback = context->getCurrentTransformFeedback();
            if (curTransformFeedback && curTransformFeedback->isStarted() && !curTransformFeedback->isPaused() &&
                curTransformFeedback->getDrawMode() != mode)
            {
                // It is an invalid operation to call DrawArrays or DrawArraysInstanced with a draw mode
                // that does not match the current transform feedback object's draw mode (if transform feedback
                // is active), (3.0.2, section 2.14, pg 86)
                return gl::error(GL_INVALID_OPERATION);
            }

            context->drawArrays(mode, first, count, 0);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDrawArraysInstancedANGLE(GLenum mode, GLint first, GLsizei count, GLsizei primcount)
{
    EVENT("(GLenum mode = 0x%X, GLint first = %d, GLsizei count = %d, GLsizei primcount = %d)", mode, first, count, primcount);

    try
    {
        if (count < 0 || first < 0 || primcount < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        if (primcount > 0)
        {
            gl::Context *context = gl::getNonLostContext();

            // Check for mapped buffers
            if (context->hasMappedBuffer(GL_ARRAY_BUFFER))
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (context)
            {
                gl::TransformFeedback *curTransformFeedback = context->getCurrentTransformFeedback();
                if (curTransformFeedback && curTransformFeedback->isStarted() && !curTransformFeedback->isPaused() &&
                    curTransformFeedback->getDrawMode() != mode)
                {
                    // It is an invalid operation to call DrawArrays or DrawArraysInstanced with a draw mode
                    // that does not match the current transform feedback object's draw mode (if transform feedback
                    // is active), (3.0.2, section 2.14, pg 86)
                    return gl::error(GL_INVALID_OPERATION);
                }

                context->drawArrays(mode, first, count, primcount);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices)
{
    EVENT("(GLenum mode = 0x%X, GLsizei count = %d, GLenum type = 0x%X, const GLvoid* indices = 0x%0.8p)",
          mode, count, type, indices);

    try
    {
        if (count < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            switch (type)
            {
              case GL_UNSIGNED_BYTE:
              case GL_UNSIGNED_SHORT:
                break;
              case GL_UNSIGNED_INT:
                if (!context->supports32bitIndices())
                {
                    return gl::error(GL_INVALID_ENUM);
                }
                break;
              default:
                return gl::error(GL_INVALID_ENUM);
            }

            gl::TransformFeedback *curTransformFeedback = context->getCurrentTransformFeedback();
            if (curTransformFeedback && curTransformFeedback->isStarted() && !curTransformFeedback->isPaused())
            {
                // It is an invalid operation to call DrawElements, DrawRangeElements or DrawElementsInstanced
                // while transform feedback is active, (3.0.2, section 2.14, pg 86)
                return gl::error(GL_INVALID_OPERATION);
            }

            // Check for mapped buffers
            if (context->hasMappedBuffer(GL_ARRAY_BUFFER) || context->hasMappedBuffer(GL_ELEMENT_ARRAY_BUFFER))
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            context->drawElements(mode, count, type, indices, 0);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDrawElementsInstancedANGLE(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount)
{
    EVENT("(GLenum mode = 0x%X, GLsizei count = %d, GLenum type = 0x%X, const GLvoid* indices = 0x%0.8p, GLsizei primcount = %d)",
          mode, count, type, indices, primcount);

    try
    {
        if (count < 0 || primcount < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        if (primcount > 0)
        {
            gl::Context *context = gl::getNonLostContext();

            if (context)
            {
                switch (type)
                {
                  case GL_UNSIGNED_BYTE:
                  case GL_UNSIGNED_SHORT:
                    break;
                  case GL_UNSIGNED_INT:
                    if (!context->supports32bitIndices())
                    {
                        return gl::error(GL_INVALID_ENUM);
                    }
                    break;
                  default:
                    return gl::error(GL_INVALID_ENUM);
                }

                gl::TransformFeedback *curTransformFeedback = context->getCurrentTransformFeedback();
                if (curTransformFeedback && curTransformFeedback->isStarted() && !curTransformFeedback->isPaused())
                {
                    // It is an invalid operation to call DrawElements, DrawRangeElements or DrawElementsInstanced
                    // while transform feedback is active, (3.0.2, section 2.14, pg 86)
                    return gl::error(GL_INVALID_OPERATION);
                }

                // Check for mapped buffers
                if (context->hasMappedBuffer(GL_ARRAY_BUFFER) || context->hasMappedBuffer(GL_ELEMENT_ARRAY_BUFFER))
                {
                    return gl::error(GL_INVALID_OPERATION);
                }

                context->drawElements(mode, count, type, indices, primcount);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glEnable(GLenum cap)
{
    EVENT("(GLenum cap = 0x%X)", cap);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (!ValidCap(context, cap))
            {
                return gl::error(GL_INVALID_ENUM);
            }

            context->setCap(cap, true);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glEnableVertexAttribArray(GLuint index)
{
    EVENT("(GLuint index = %d)", index);

    try
    {
        if (index >= gl::MAX_VERTEX_ATTRIBS)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            context->setEnableVertexAttribArray(index, true);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glEndQueryEXT(GLenum target)
{
    EVENT("GLenum target = 0x%X)", target);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (!ValidQueryType(context, target))
            {
                return gl::error(GL_INVALID_ENUM);
            }

            context->endQuery(target);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glFinishFenceNV(GLuint fence)
{
    EVENT("(GLuint fence = %d)", fence);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            gl::FenceNV *fenceObject = context->getFenceNV(fence);

            if (fenceObject == NULL)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (fenceObject->isFence() != GL_TRUE)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            fenceObject->finishFence();
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glFinish(void)
{
    EVENT("()");

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            context->sync(true);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glFlush(void)
{
    EVENT("()");

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            context->sync(false);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
    EVENT("(GLenum target = 0x%X, GLenum attachment = 0x%X, GLenum renderbuffertarget = 0x%X, "
          "GLuint renderbuffer = %d)", target, attachment, renderbuffertarget, renderbuffer);

    try
    {
        if (!gl::ValidFramebufferTarget(target) || (renderbuffertarget != GL_RENDERBUFFER && renderbuffer != 0))
        {
            return gl::error(GL_INVALID_ENUM);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (!gl::ValidateFramebufferRenderbufferParameters(context, target, attachment, renderbuffertarget, renderbuffer))
            {
                return;
            }

            gl::Framebuffer *framebuffer = context->getTargetFramebuffer(target);
            ASSERT(framebuffer);

            if (attachment >= GL_COLOR_ATTACHMENT0_EXT && attachment <= GL_COLOR_ATTACHMENT15_EXT)
            {
                unsigned int colorAttachment = (attachment - GL_COLOR_ATTACHMENT0_EXT);
                framebuffer->setColorbuffer(colorAttachment, GL_RENDERBUFFER, renderbuffer, 0, 0);
            }
            else
            {
                switch (attachment)
                {
                  case GL_DEPTH_ATTACHMENT:
                    framebuffer->setDepthbuffer(GL_RENDERBUFFER, renderbuffer, 0, 0);
                    break;
                  case GL_STENCIL_ATTACHMENT:
                    framebuffer->setStencilbuffer(GL_RENDERBUFFER, renderbuffer, 0, 0);
                    break;
                  case GL_DEPTH_STENCIL_ATTACHMENT:
                    framebuffer->setDepthStencilBuffer(GL_RENDERBUFFER, renderbuffer, 0, 0);
                    break;
                  default:
                    UNREACHABLE();
                    break;
                }
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
    EVENT("(GLenum target = 0x%X, GLenum attachment = 0x%X, GLenum textarget = 0x%X, "
          "GLuint texture = %d, GLint level = %d)", target, attachment, textarget, texture, level);

    try
    {
        gl::Context *context = gl::getNonLostContext();
        if (context)
        {
            if (context->getClientVersion() < 3 &&
                !ValidateES2FramebufferTextureParameters(context, target, attachment, textarget, texture, level))
            {
                return;
            }

            if (context->getClientVersion() >= 3 &&
                !ValidateES3FramebufferTextureParameters(context, target, attachment, textarget, texture, level, 0, false))
            {
                return;
            }

            if (texture == 0)
            {
                textarget = GL_NONE;
            }

            gl::Framebuffer *framebuffer = context->getTargetFramebuffer(target);

            if (attachment >= GL_COLOR_ATTACHMENT0_EXT && attachment <= GL_COLOR_ATTACHMENT15_EXT)
            {
                const unsigned int colorAttachment = (attachment - GL_COLOR_ATTACHMENT0_EXT);
                framebuffer->setColorbuffer(colorAttachment, textarget, texture, level, 0);
            }
            else
            {
                switch (attachment)
                {
                  case GL_DEPTH_ATTACHMENT:         framebuffer->setDepthbuffer(textarget, texture, level, 0);        break;
                  case GL_STENCIL_ATTACHMENT:       framebuffer->setStencilbuffer(textarget, texture, level, 0);      break;
                  case GL_DEPTH_STENCIL_ATTACHMENT: framebuffer->setDepthStencilBuffer(textarget, texture, level, 0); break;
                }
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glFrontFace(GLenum mode)
{
    EVENT("(GLenum mode = 0x%X)", mode);

    try
    {
        switch (mode)
        {
          case GL_CW:
          case GL_CCW:
            {
                gl::Context *context = gl::getNonLostContext();

                if (context)
                {
                    context->setFrontFace(mode);
                }
            }
            break;
          default:
            return gl::error(GL_INVALID_ENUM);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGenBuffers(GLsizei n, GLuint* buffers)
{
    EVENT("(GLsizei n = %d, GLuint* buffers = 0x%0.8p)", n, buffers);

    try
    {
        if (n < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            for (int i = 0; i < n; i++)
            {
                buffers[i] = context->createBuffer();
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGenerateMipmap(GLenum target)
{
    EVENT("(GLenum target = 0x%X)", target);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (!ValidTextureTarget(context, target))
            {
                return gl::error(GL_INVALID_ENUM);
            }

            gl::Texture *texture = context->getTargetTexture(target);

            if (texture == NULL)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            GLenum internalFormat = texture->getBaseLevelInternalFormat();

            // Internally, all texture formats are sized so checking if the format
            // is color renderable and filterable will not fail.

            bool validRenderable = (gl::IsColorRenderingSupported(internalFormat, context) ||
                                    gl::IsSizedInternalFormat(internalFormat, context->getClientVersion()));

            if (gl::IsDepthRenderingSupported(internalFormat, context) ||
                gl::IsFormatCompressed(internalFormat, context->getClientVersion()) ||
                !gl::IsTextureFilteringSupported(internalFormat, context) ||
                !validRenderable)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            // Non-power of 2 ES2 check
            if (!context->supportsNonPower2Texture() && (!gl::isPow2(texture->getBaseLevelWidth()) || !gl::isPow2(texture->getBaseLevelHeight())))
            {
                ASSERT(context->getClientVersion() <= 2 && (target == GL_TEXTURE_2D || target == GL_TEXTURE_CUBE_MAP));
                return gl::error(GL_INVALID_OPERATION);
            }

            // Cube completeness check
            if (target == GL_TEXTURE_CUBE_MAP)
            {
                gl::TextureCubeMap *textureCube = static_cast<gl::TextureCubeMap *>(texture);
                if (!textureCube->isCubeComplete())
                {
                    return gl::error(GL_INVALID_OPERATION);
                }
            }

            texture->generateMipmaps();
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGenFencesNV(GLsizei n, GLuint* fences)
{
    EVENT("(GLsizei n = %d, GLuint* fences = 0x%0.8p)", n, fences);

    try
    {
        if (n < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            for (int i = 0; i < n; i++)
            {
                fences[i] = context->createFenceNV();
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGenFramebuffers(GLsizei n, GLuint* framebuffers)
{
    EVENT("(GLsizei n = %d, GLuint* framebuffers = 0x%0.8p)", n, framebuffers);

    try
    {
        if (n < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            for (int i = 0; i < n; i++)
            {
                framebuffers[i] = context->createFramebuffer();
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGenQueriesEXT(GLsizei n, GLuint* ids)
{
    EVENT("(GLsizei n = %d, GLuint* ids = 0x%0.8p)", n, ids);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (n < 0)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            for (GLsizei i = 0; i < n; i++)
            {
                ids[i] = context->createQuery();
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGenRenderbuffers(GLsizei n, GLuint* renderbuffers)
{
    EVENT("(GLsizei n = %d, GLuint* renderbuffers = 0x%0.8p)", n, renderbuffers);

    try
    {
        if (n < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            for (int i = 0; i < n; i++)
            {
                renderbuffers[i] = context->createRenderbuffer();
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGenTextures(GLsizei n, GLuint* textures)
{
    EVENT("(GLsizei n = %d, GLuint* textures = 0x%0.8p)", n, textures);

    try
    {
        if (n < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            for (int i = 0; i < n; i++)
            {
                textures[i] = context->createTexture();
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetActiveAttrib(GLuint program, GLuint index, GLsizei bufsize, GLsizei *length, GLint *size, GLenum *type, GLchar *name)
{
    EVENT("(GLuint program = %d, GLuint index = %d, GLsizei bufsize = %d, GLsizei *length = 0x%0.8p, "
          "GLint *size = 0x%0.8p, GLenum *type = %0.8p, GLchar *name = %0.8p)",
          program, index, bufsize, length, size, type, name);

    try
    {
        if (bufsize < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            gl::Program *programObject = context->getProgram(program);

            if (!programObject)
            {
                if (context->getShader(program))
                {
                    return gl::error(GL_INVALID_OPERATION);
                }
                else
                {
                    return gl::error(GL_INVALID_VALUE);
                }
            }

            if (index >= (GLuint)programObject->getActiveAttributeCount())
            {
                return gl::error(GL_INVALID_VALUE);
            }

            programObject->getActiveAttribute(index, bufsize, length, size, type, name);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetActiveUniform(GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size, GLenum* type, GLchar* name)
{
    EVENT("(GLuint program = %d, GLuint index = %d, GLsizei bufsize = %d, "
          "GLsizei* length = 0x%0.8p, GLint* size = 0x%0.8p, GLenum* type = 0x%0.8p, GLchar* name = 0x%0.8p)",
          program, index, bufsize, length, size, type, name);

    try
    {
        if (bufsize < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            gl::Program *programObject = context->getProgram(program);

            if (!programObject)
            {
                if (context->getShader(program))
                {
                    return gl::error(GL_INVALID_OPERATION);
                }
                else
                {
                    return gl::error(GL_INVALID_VALUE);
                }
            }

            if (index >= (GLuint)programObject->getActiveUniformCount())
            {
                return gl::error(GL_INVALID_VALUE);
            }

            programObject->getActiveUniform(index, bufsize, length, size, type, name);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetAttachedShaders(GLuint program, GLsizei maxcount, GLsizei* count, GLuint* shaders)
{
    EVENT("(GLuint program = %d, GLsizei maxcount = %d, GLsizei* count = 0x%0.8p, GLuint* shaders = 0x%0.8p)",
          program, maxcount, count, shaders);

    try
    {
        if (maxcount < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            gl::Program *programObject = context->getProgram(program);

            if (!programObject)
            {
                if (context->getShader(program))
                {
                    return gl::error(GL_INVALID_OPERATION);
                }
                else
                {
                    return gl::error(GL_INVALID_VALUE);
                }
            }

            return programObject->getAttachedShaders(maxcount, count, shaders);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

int __stdcall glGetAttribLocation(GLuint program, const GLchar* name)
{
    EVENT("(GLuint program = %d, const GLchar* name = %s)", program, name);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {

            gl::Program *programObject = context->getProgram(program);

            if (!programObject)
            {
                if (context->getShader(program))
                {
                    return gl::error(GL_INVALID_OPERATION, -1);
                }
                else
                {
                    return gl::error(GL_INVALID_VALUE, -1);
                }
            }

            gl::ProgramBinary *programBinary = programObject->getProgramBinary();
            if (!programObject->isLinked() || !programBinary)
            {
                return gl::error(GL_INVALID_OPERATION, -1);
            }

            return programBinary->getAttributeLocation(name);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY, -1);
    }

    return -1;
}

void __stdcall glGetBooleanv(GLenum pname, GLboolean* params)
{
    EVENT("(GLenum pname = 0x%X, GLboolean* params = 0x%0.8p)",  pname, params);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            GLenum nativeType;
            unsigned int numParams = 0;
            if (!context->getQueryParameterInfo(pname, &nativeType, &numParams))
                return gl::error(GL_INVALID_ENUM);

            // pname is valid, but there are no parameters to return
            if (numParams == 0)
                return;

            if (nativeType == GL_BOOL)
            {
                context->getBooleanv(pname, params);
            }
            else
            {
                CastStateValues(context, nativeType, pname, numParams, params);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetBufferParameteriv(GLenum target, GLenum pname, GLint* params)
{
    EVENT("(GLenum target = 0x%X, GLenum pname = 0x%X, GLint* params = 0x%0.8p)", target, pname, params);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (!gl::ValidBufferTarget(context, target))
            {
                return gl::error(GL_INVALID_ENUM);
            }

            if (!gl::ValidBufferParameter(context, pname))
            {
                return gl::error(GL_INVALID_ENUM);
            }

            gl::Buffer *buffer = context->getTargetBuffer(target);

            if (!buffer)
            {
                // A null buffer means that "0" is bound to the requested buffer target
                return gl::error(GL_INVALID_OPERATION);
            }

            switch (pname)
            {
              case GL_BUFFER_USAGE:
                *params = static_cast<GLint>(buffer->usage());
                break;
              case GL_BUFFER_SIZE:
                *params = gl::clampCast<GLint>(buffer->size());
                break;
              case GL_BUFFER_ACCESS_FLAGS:
                *params = buffer->accessFlags();
                break;
              case GL_BUFFER_MAPPED:
                *params = static_cast<GLint>(buffer->mapped());
                break;
              case GL_BUFFER_MAP_OFFSET:
                *params = gl::clampCast<GLint>(buffer->mapOffset());
                break;
              case GL_BUFFER_MAP_LENGTH:
                *params = gl::clampCast<GLint>(buffer->mapLength());
                break;
              default: UNREACHABLE(); break;
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

GLenum __stdcall glGetError(void)
{
    EVENT("()");

    gl::Context *context = gl::getContext();

    if (context)
    {
        return context->getError();
    }

    return GL_NO_ERROR;
}

void __stdcall glGetFenceivNV(GLuint fence, GLenum pname, GLint *params)
{
    EVENT("(GLuint fence = %d, GLenum pname = 0x%X, GLint *params = 0x%0.8p)", fence, pname, params);

    try
    {
    
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            gl::FenceNV *fenceObject = context->getFenceNV(fence);

            if (fenceObject == NULL)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (fenceObject->isFence() != GL_TRUE)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            switch (pname)
            {
              case GL_FENCE_STATUS_NV:
              case GL_FENCE_CONDITION_NV:
                break;

              default: return gl::error(GL_INVALID_ENUM);
            }

            params[0] = fenceObject->getFencei(pname);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetFloatv(GLenum pname, GLfloat* params)
{
    EVENT("(GLenum pname = 0x%X, GLfloat* params = 0x%0.8p)", pname, params);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            GLenum nativeType;
            unsigned int numParams = 0;
            if (!context->getQueryParameterInfo(pname, &nativeType, &numParams))
                return gl::error(GL_INVALID_ENUM);

            // pname is valid, but that there are no parameters to return.
            if (numParams == 0)
                return;

            if (nativeType == GL_FLOAT)
            {
                context->getFloatv(pname, params);
            }
            else
            {
                CastStateValues(context, nativeType, pname, numParams, params);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint* params)
{
    EVENT("(GLenum target = 0x%X, GLenum attachment = 0x%X, GLenum pname = 0x%X, GLint* params = 0x%0.8p)",
          target, attachment, pname, params);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (!gl::ValidFramebufferTarget(target))
            {
                return gl::error(GL_INVALID_ENUM);
            }

            switch (pname)
            {
              case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
              case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
              case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
              case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
                break;
              case GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE:
              case GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE:
              case GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE:
              case GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE:
              case GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE:
              case GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE:
              case GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE:
              case GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING:
              case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER:
                if (context->getClientVersion() >= 3)
                {
                    break;
                }
              default:
                return gl::error(GL_INVALID_ENUM);
            }

            // Determine if the attachment is a valid enum
            switch (attachment)
            {
              case GL_BACK:
              case GL_FRONT:
              case GL_DEPTH:
              case GL_STENCIL:
              case GL_DEPTH_STENCIL_ATTACHMENT:
                if (context->getClientVersion() < 3)
                {
                    return gl::error(GL_INVALID_ENUM);
                }
                break;

              case GL_DEPTH_ATTACHMENT:
              case GL_STENCIL_ATTACHMENT:
                break;

              default:
                if (attachment < GL_COLOR_ATTACHMENT0_EXT ||
                    (attachment - GL_COLOR_ATTACHMENT0_EXT) >= context->getMaximumRenderTargets())
                {
                    return gl::error(GL_INVALID_ENUM);
                }
                break;
            }

            GLuint framebufferHandle = context->getTargetFramebufferHandle(target);
            ASSERT(framebufferHandle != GL_INVALID_INDEX);
            gl::Framebuffer *framebuffer = context->getFramebuffer(framebufferHandle);

            GLenum attachmentType;
            GLuint attachmentHandle;
            GLuint attachmentLevel;
            GLuint attachmentLayer;
            gl::Renderbuffer *renderbuffer;

            if(framebufferHandle == 0)
            {
                if(context->getClientVersion() < 3)
                {
                    return gl::error(GL_INVALID_OPERATION);
                }

                switch (attachment)
                {
                  case GL_BACK:
                    attachmentType = framebuffer->getColorbufferType(0);
                    attachmentHandle = framebuffer->getColorbufferHandle(0);
                    attachmentLevel = framebuffer->getColorbufferMipLevel(0);
                    attachmentLayer = framebuffer->getColorbufferLayer(0);
                    renderbuffer = framebuffer->getColorbuffer(0);
                    break;
                  case GL_DEPTH:
                    attachmentType = framebuffer->getDepthbufferType();
                    attachmentHandle = framebuffer->getDepthbufferHandle();
                    attachmentLevel = framebuffer->getDepthbufferMipLevel();
                    attachmentLayer = framebuffer->getDepthbufferLayer();
                    renderbuffer = framebuffer->getDepthbuffer();
                    break;
                  case GL_STENCIL:
                    attachmentType = framebuffer->getStencilbufferType();
                    attachmentHandle = framebuffer->getStencilbufferHandle();
                    attachmentLevel = framebuffer->getStencilbufferMipLevel();
                    attachmentLayer = framebuffer->getStencilbufferLayer();
                    renderbuffer = framebuffer->getStencilbuffer();
                    break;
                  default:
                    return gl::error(GL_INVALID_OPERATION);
                }
            }
            else
            {
                if (attachment >= GL_COLOR_ATTACHMENT0_EXT && attachment <= GL_COLOR_ATTACHMENT15_EXT)
                {
                    const unsigned int colorAttachment = (attachment - GL_COLOR_ATTACHMENT0_EXT);
                    attachmentType = framebuffer->getColorbufferType(colorAttachment);
                    attachmentHandle = framebuffer->getColorbufferHandle(colorAttachment);
                    attachmentLevel = framebuffer->getColorbufferMipLevel(colorAttachment);
                    attachmentLayer = framebuffer->getColorbufferLayer(colorAttachment);
                    renderbuffer = framebuffer->getColorbuffer(colorAttachment);
                }
                else
                {
                    switch (attachment)
                    {
                      case GL_DEPTH_ATTACHMENT:
                        attachmentType = framebuffer->getDepthbufferType();
                        attachmentHandle = framebuffer->getDepthbufferHandle();
                        attachmentLevel = framebuffer->getDepthbufferMipLevel();
                        attachmentLayer = framebuffer->getDepthbufferLayer();
                        renderbuffer = framebuffer->getDepthbuffer();
                        break;
                      case GL_STENCIL_ATTACHMENT:
                        attachmentType = framebuffer->getStencilbufferType();
                        attachmentHandle = framebuffer->getStencilbufferHandle();
                        attachmentLevel = framebuffer->getStencilbufferMipLevel();
                        attachmentLayer = framebuffer->getStencilbufferLayer();
                        renderbuffer = framebuffer->getStencilbuffer();
                        break;
                      case GL_DEPTH_STENCIL_ATTACHMENT:
                        if (framebuffer->getDepthbufferHandle() != framebuffer->getStencilbufferHandle())
                        {
                            return gl::error(GL_INVALID_OPERATION);
                        }
                        attachmentType = framebuffer->getDepthStencilbufferType();
                        attachmentHandle = framebuffer->getDepthStencilbufferHandle();
                        attachmentLevel = framebuffer->getDepthStencilbufferMipLevel();
                        attachmentLayer = framebuffer->getDepthStencilbufferLayer();
                        renderbuffer = framebuffer->getDepthStencilBuffer();
                        break;
                      default:
                        return gl::error(GL_INVALID_OPERATION);
                    }
                }
            }

            GLenum attachmentObjectType;   // Type category
            if (framebufferHandle == 0)
            {
                attachmentObjectType = GL_FRAMEBUFFER_DEFAULT;
            }
            else if (attachmentType == GL_NONE || attachmentType == GL_RENDERBUFFER)
            {
                attachmentObjectType = attachmentType;
            }
            else if (gl::IsInternalTextureTarget(attachmentType, context->getClientVersion()))
            {
                attachmentObjectType = GL_TEXTURE;
            }
            else
            {
                UNREACHABLE();
                return;
            }

            if (attachmentObjectType == GL_NONE)
            {
                // ES 2.0.25 spec pg 127 states that if the value of FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE
                // is NONE, then querying any other pname will generate INVALID_ENUM.

                // ES 3.0.2 spec pg 235 states that if the attachment type is none,
                // GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME will return zero and be an
                // INVALID_OPERATION for all other pnames

                switch (pname)
                {
                  case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
                    *params = attachmentObjectType;
                    break;

                  case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
                    if (context->getClientVersion() < 3)
                    {
                        return gl::error(GL_INVALID_ENUM);
                    }
                    *params = 0;
                    break;

                  default:
                    if (context->getClientVersion() < 3)
                    {
                        return gl::error(GL_INVALID_ENUM);
                    }
                    else
                    {
                        gl::error(GL_INVALID_OPERATION);
                    }
                }
            }
            else
            {
                ASSERT(attachmentObjectType == GL_RENDERBUFFER || attachmentObjectType == GL_TEXTURE ||
                       attachmentObjectType == GL_FRAMEBUFFER_DEFAULT);
                ASSERT(renderbuffer != NULL);

                switch (pname)
                {
                  case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
                    *params = attachmentObjectType;
                    break;

                  case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
                    if (attachmentObjectType != GL_RENDERBUFFER && attachmentObjectType != GL_TEXTURE)
                    {
                        return gl::error(GL_INVALID_ENUM);
                    }
                    *params = attachmentHandle;
                    break;

                  case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
                    if (attachmentObjectType != GL_TEXTURE)
                    {
                        return gl::error(GL_INVALID_ENUM);
                    }
                    *params = attachmentLevel;
                    break;

                  case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
                    if (attachmentObjectType != GL_TEXTURE)
                    {
                        return gl::error(GL_INVALID_ENUM);
                    }
                    *params = gl::IsCubemapTextureTarget(attachmentType) ? attachmentType : 0;
                    break;

                  case GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE:
                    *params = renderbuffer->getRedSize();
                    break;

                  case GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE:
                    *params = renderbuffer->getGreenSize();
                    break;

                  case GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE:
                    *params = renderbuffer->getBlueSize();
                    break;

                  case GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE:
                    *params = renderbuffer->getAlphaSize();
                    break;

                  case GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE:
                    *params = renderbuffer->getDepthSize();
                    break;

                  case GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE:
                    *params = renderbuffer->getStencilSize();
                    break;

                  case GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE:
                    if (attachment == GL_DEPTH_STENCIL)
                    {
                        gl::error(GL_INVALID_OPERATION);
                    }
                    *params = renderbuffer->getComponentType();
                    break;

                  case GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING:
                    *params = renderbuffer->getColorEncoding();
                    break;

                  case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER:
                    if (attachmentObjectType != GL_TEXTURE)
                    {
                        return gl::error(GL_INVALID_ENUM);
                    }
                    *params = attachmentLayer;
                    break;

                  default:
                    UNREACHABLE();
                    break;
                }
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

GLenum __stdcall glGetGraphicsResetStatusEXT(void)
{
    EVENT("()");

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            return context->getResetStatus();
        }

        return GL_NO_ERROR;
    }
    catch(std::bad_alloc&)
    {
        return GL_OUT_OF_MEMORY;
    }
}

void __stdcall glGetIntegerv(GLenum pname, GLint* params)
{
    EVENT("(GLenum pname = 0x%X, GLint* params = 0x%0.8p)", pname, params);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            GLenum nativeType;
            unsigned int numParams = 0;
            if (!context->getQueryParameterInfo(pname, &nativeType, &numParams))
                return gl::error(GL_INVALID_ENUM);

            // pname is valid, but there are no parameters to return
            if (numParams == 0)
                return;

            if (nativeType == GL_INT)
            {
                context->getIntegerv(pname, params);
            }
            else
            {
                CastStateValues(context, nativeType, pname, numParams, params);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetProgramiv(GLuint program, GLenum pname, GLint* params)
{
    EVENT("(GLuint program = %d, GLenum pname = %d, GLint* params = 0x%0.8p)", program, pname, params);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            gl::Program *programObject = context->getProgram(program);

            if (!programObject)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            if (context->getClientVersion() < 3)
            {
                switch (pname)
                {
                  case GL_ACTIVE_UNIFORM_BLOCKS:
                  case GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH:
                  case GL_TRANSFORM_FEEDBACK_BUFFER_MODE:
                  case GL_TRANSFORM_FEEDBACK_VARYINGS:
                  case GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH:
                    return gl::error(GL_INVALID_ENUM);
                }
            }

            switch (pname)
            {
              case GL_DELETE_STATUS:
                *params = programObject->isFlaggedForDeletion();
                return;
              case GL_LINK_STATUS:
                *params = programObject->isLinked();
                return;
              case GL_VALIDATE_STATUS:
                *params = programObject->isValidated();
                return;
              case GL_INFO_LOG_LENGTH:
                *params = programObject->getInfoLogLength();
                return;
              case GL_ATTACHED_SHADERS:
                *params = programObject->getAttachedShadersCount();
                return;
              case GL_ACTIVE_ATTRIBUTES:
                *params = programObject->getActiveAttributeCount();
                return;
              case GL_ACTIVE_ATTRIBUTE_MAX_LENGTH:
                *params = programObject->getActiveAttributeMaxLength();
                return;
              case GL_ACTIVE_UNIFORMS:
                *params = programObject->getActiveUniformCount();
                return;
              case GL_ACTIVE_UNIFORM_MAX_LENGTH:
                *params = programObject->getActiveUniformMaxLength();
                return;
              case GL_PROGRAM_BINARY_LENGTH_OES:
                *params = programObject->getProgramBinaryLength();
                return;
              case GL_ACTIVE_UNIFORM_BLOCKS:
                *params = programObject->getActiveUniformBlockCount();
                return;
              case GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH:
                *params = programObject->getActiveUniformBlockMaxLength();
                break;
              case GL_TRANSFORM_FEEDBACK_BUFFER_MODE:
                *params = programObject->getTransformFeedbackBufferMode();
                break;
              case GL_TRANSFORM_FEEDBACK_VARYINGS:
                *params = programObject->getTransformFeedbackVaryingCount();
                break;
              case GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH:
                *params = programObject->getTransformFeedbackVaryingMaxLength();
                break;
              default:
                return gl::error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetProgramInfoLog(GLuint program, GLsizei bufsize, GLsizei* length, GLchar* infolog)
{
    EVENT("(GLuint program = %d, GLsizei bufsize = %d, GLsizei* length = 0x%0.8p, GLchar* infolog = 0x%0.8p)",
          program, bufsize, length, infolog);

    try
    {
        if (bufsize < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            gl::Program *programObject = context->getProgram(program);

            if (!programObject)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            programObject->getInfoLog(bufsize, length, infolog);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetQueryivEXT(GLenum target, GLenum pname, GLint *params)
{
    EVENT("GLenum target = 0x%X, GLenum pname = 0x%X, GLint *params = 0x%0.8p)", target, pname, params);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (!ValidQueryType(context, target))
            {
                return gl::error(GL_INVALID_ENUM);
            }

            switch (pname)
            {
              case GL_CURRENT_QUERY_EXT:
                params[0] = context->getActiveQuery(target);
                break;

              default:
                return gl::error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetQueryObjectuivEXT(GLuint id, GLenum pname, GLuint *params)
{
    EVENT("(GLuint id = %d, GLenum pname = 0x%X, GLuint *params = 0x%0.8p)", id, pname, params);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            gl::Query *queryObject = context->getQuery(id, false, GL_NONE);

            if (!queryObject)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (context->getActiveQuery(queryObject->getType()) == id)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            switch(pname)
            {
              case GL_QUERY_RESULT_EXT:
                params[0] = queryObject->getResult();
                break;
              case GL_QUERY_RESULT_AVAILABLE_EXT:
                params[0] = queryObject->isResultAvailable();
                break;
              default:
                return gl::error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint* params)
{
    EVENT("(GLenum target = 0x%X, GLenum pname = 0x%X, GLint* params = 0x%0.8p)", target, pname, params);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (target != GL_RENDERBUFFER)
            {
                return gl::error(GL_INVALID_ENUM);
            }

            if (context->getRenderbufferHandle() == 0)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            gl::Renderbuffer *renderbuffer = context->getRenderbuffer(context->getRenderbufferHandle());

            switch (pname)
            {
              case GL_RENDERBUFFER_WIDTH:           *params = renderbuffer->getWidth();          break;
              case GL_RENDERBUFFER_HEIGHT:          *params = renderbuffer->getHeight();         break;
              case GL_RENDERBUFFER_INTERNAL_FORMAT: *params = renderbuffer->getInternalFormat(); break;
              case GL_RENDERBUFFER_RED_SIZE:        *params = renderbuffer->getRedSize();        break;
              case GL_RENDERBUFFER_GREEN_SIZE:      *params = renderbuffer->getGreenSize();      break;
              case GL_RENDERBUFFER_BLUE_SIZE:       *params = renderbuffer->getBlueSize();       break;
              case GL_RENDERBUFFER_ALPHA_SIZE:      *params = renderbuffer->getAlphaSize();      break;
              case GL_RENDERBUFFER_DEPTH_SIZE:      *params = renderbuffer->getDepthSize();      break;
              case GL_RENDERBUFFER_STENCIL_SIZE:    *params = renderbuffer->getStencilSize();    break;
              case GL_RENDERBUFFER_SAMPLES_ANGLE:
                if (context->getMaxSupportedSamples() != 0)
                {
                    *params = renderbuffer->getSamples();
                }
                else
                {
                    return gl::error(GL_INVALID_ENUM);
                }
                break;
              default:
                return gl::error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetShaderiv(GLuint shader, GLenum pname, GLint* params)
{
    EVENT("(GLuint shader = %d, GLenum pname = %d, GLint* params = 0x%0.8p)", shader, pname, params);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            gl::Shader *shaderObject = context->getShader(shader);

            if (!shaderObject)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            switch (pname)
            {
              case GL_SHADER_TYPE:
                *params = shaderObject->getType();
                return;
              case GL_DELETE_STATUS:
                *params = shaderObject->isFlaggedForDeletion();
                return;
              case GL_COMPILE_STATUS:
                *params = shaderObject->isCompiled() ? GL_TRUE : GL_FALSE;
                return;
              case GL_INFO_LOG_LENGTH:
                *params = shaderObject->getInfoLogLength();
                return;
              case GL_SHADER_SOURCE_LENGTH:
                *params = shaderObject->getSourceLength();
                return;
              case GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE:
                *params = shaderObject->getTranslatedSourceLength();
                return;
              default:
                return gl::error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetShaderInfoLog(GLuint shader, GLsizei bufsize, GLsizei* length, GLchar* infolog)
{
    EVENT("(GLuint shader = %d, GLsizei bufsize = %d, GLsizei* length = 0x%0.8p, GLchar* infolog = 0x%0.8p)",
          shader, bufsize, length, infolog);

    try
    {
        if (bufsize < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            gl::Shader *shaderObject = context->getShader(shader);

            if (!shaderObject)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            shaderObject->getInfoLog(bufsize, length, infolog);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetShaderPrecisionFormat(GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision)
{
    EVENT("(GLenum shadertype = 0x%X, GLenum precisiontype = 0x%X, GLint* range = 0x%0.8p, GLint* precision = 0x%0.8p)",
          shadertype, precisiontype, range, precision);

    try
    {
        switch (shadertype)
        {
          case GL_VERTEX_SHADER:
          case GL_FRAGMENT_SHADER:
            break;
          default:
            return gl::error(GL_INVALID_ENUM);
        }

        switch (precisiontype)
        {
          case GL_LOW_FLOAT:
          case GL_MEDIUM_FLOAT:
          case GL_HIGH_FLOAT:
            // Assume IEEE 754 precision
            range[0] = 127;
            range[1] = 127;
            *precision = 23;
            break;
          case GL_LOW_INT:
          case GL_MEDIUM_INT:
          case GL_HIGH_INT:
            // Some (most) hardware only supports single-precision floating-point numbers,
            // which can accurately represent integers up to +/-16777216
            range[0] = 24;
            range[1] = 24;
            *precision = 0;
            break;
          default:
            return gl::error(GL_INVALID_ENUM);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetShaderSource(GLuint shader, GLsizei bufsize, GLsizei* length, GLchar* source)
{
    EVENT("(GLuint shader = %d, GLsizei bufsize = %d, GLsizei* length = 0x%0.8p, GLchar* source = 0x%0.8p)",
          shader, bufsize, length, source);

    try
    {
        if (bufsize < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            gl::Shader *shaderObject = context->getShader(shader);

            if (!shaderObject)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            shaderObject->getSource(bufsize, length, source);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetTranslatedShaderSourceANGLE(GLuint shader, GLsizei bufsize, GLsizei* length, GLchar* source)
{
    EVENT("(GLuint shader = %d, GLsizei bufsize = %d, GLsizei* length = 0x%0.8p, GLchar* source = 0x%0.8p)",
          shader, bufsize, length, source);

    try
    {
        if (bufsize < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            gl::Shader *shaderObject = context->getShader(shader);

            if (!shaderObject)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            shaderObject->getTranslatedSource(bufsize, length, source);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

const GLubyte* __stdcall glGetString(GLenum name)
{
    EVENT("(GLenum name = 0x%X)", name);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        switch (name)
        {
          case GL_VENDOR:
            return (GLubyte*)"Google Inc.";
          case GL_RENDERER:
            return (GLubyte*)((context != NULL) ? context->getRendererString() : "ANGLE");
          case GL_VERSION:
            if (context->getClientVersion() == 2)
            {
                return (GLubyte*)"OpenGL ES 2.0 (ANGLE " ANGLE_VERSION_STRING ")";
            }
            else
            {
                return (GLubyte*)"OpenGL ES 3.0 (ANGLE " ANGLE_VERSION_STRING ")";
            }
          case GL_SHADING_LANGUAGE_VERSION:
            if (context->getClientVersion() == 2)
            {
                return (GLubyte*)"OpenGL ES GLSL ES 1.00 (ANGLE " ANGLE_VERSION_STRING ")";
            }
            else
            {
                return (GLubyte*)"OpenGL ES GLSL ES 3.00 (ANGLE " ANGLE_VERSION_STRING ")";
            }
          case GL_EXTENSIONS:
            return (GLubyte*)((context != NULL) ? context->getCombinedExtensionsString() : "");
          default:
            return gl::error(GL_INVALID_ENUM, (GLubyte*)NULL);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY, (GLubyte*)NULL);
    }
}

void __stdcall glGetTexParameterfv(GLenum target, GLenum pname, GLfloat* params)
{
    EVENT("(GLenum target = 0x%X, GLenum pname = 0x%X, GLfloat* params = 0x%0.8p)", target, pname, params);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            gl::Texture *texture = context->getTargetTexture(target);

            if (!texture)
            {
                return gl::error(GL_INVALID_ENUM);
            }

            switch (pname)
            {
              case GL_TEXTURE_MAG_FILTER:
                *params = (GLfloat)texture->getMagFilter();
                break;
              case GL_TEXTURE_MIN_FILTER:
                *params = (GLfloat)texture->getMinFilter();
                break;
              case GL_TEXTURE_WRAP_S:
                *params = (GLfloat)texture->getWrapS();
                break;
              case GL_TEXTURE_WRAP_T:
                *params = (GLfloat)texture->getWrapT();
                break;
              case GL_TEXTURE_WRAP_R:
                if (context->getClientVersion() < 3)
                {
                    return gl::error(GL_INVALID_ENUM);
                }
                *params = (GLfloat)texture->getWrapR();
                break;
              case GL_TEXTURE_IMMUTABLE_FORMAT:
                // Exposed to ES2.0 through EXT_texture_storage, no client version validation.
                *params = (GLfloat)(texture->isImmutable() ? GL_TRUE : GL_FALSE);
                break;
              case GL_TEXTURE_IMMUTABLE_LEVELS:
                if (context->getClientVersion() < 3)
                {
                    return gl::error(GL_INVALID_ENUM);
                }
                *params = (GLfloat)texture->immutableLevelCount();
                break;
              case GL_TEXTURE_USAGE_ANGLE:
                *params = (GLfloat)texture->getUsage();
                break;
              case GL_TEXTURE_MAX_ANISOTROPY_EXT:
                if (!context->supportsTextureFilterAnisotropy())
                {
                    return gl::error(GL_INVALID_ENUM);
                }
                *params = (GLfloat)texture->getMaxAnisotropy();
                break;
              case GL_TEXTURE_SWIZZLE_R:
                if (context->getClientVersion() < 3)
                {
                    return gl::error(GL_INVALID_ENUM);
                }
                *params = (GLfloat)texture->getSwizzleRed();
                break;
              case GL_TEXTURE_SWIZZLE_G:
                if (context->getClientVersion() < 3)
                {
                    return gl::error(GL_INVALID_ENUM);
                }
                *params = (GLfloat)texture->getSwizzleGreen();
                break;
              case GL_TEXTURE_SWIZZLE_B:
                if (context->getClientVersion() < 3)
                {
                    return gl::error(GL_INVALID_ENUM);
                }
                *params = (GLfloat)texture->getSwizzleBlue();
                break;
              case GL_TEXTURE_SWIZZLE_A:
                if (context->getClientVersion() < 3)
                {
                    return gl::error(GL_INVALID_ENUM);
                }
                *params = (GLfloat)texture->getSwizzleAlpha();
                break;
              case GL_TEXTURE_BASE_LEVEL:
                if (context->getClientVersion() < 3)
                {
                    return gl::error(GL_INVALID_ENUM);
                }
                *params = (GLfloat)texture->getBaseLevel();
                break;
              case GL_TEXTURE_MAX_LEVEL:
                if (context->getClientVersion() < 3)
                {
                    return gl::error(GL_INVALID_ENUM);
                }
                *params = (GLfloat)texture->getMaxLevel();
                break;
              case GL_TEXTURE_MIN_LOD:
                if (context->getClientVersion() < 3)
                {
                    return gl::error(GL_INVALID_ENUM);
                }
                *params = texture->getMinLod();
                break;
              case GL_TEXTURE_MAX_LOD:
                if (context->getClientVersion() < 3)
                {
                    return gl::error(GL_INVALID_ENUM);
                }
                *params = texture->getMaxLod();
                break;
              default:
                return gl::error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetTexParameteriv(GLenum target, GLenum pname, GLint* params)
{
    EVENT("(GLenum target = 0x%X, GLenum pname = 0x%X, GLint* params = 0x%0.8p)", target, pname, params);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            gl::Texture *texture = context->getTargetTexture(target);

            if (!texture)
            {
                return gl::error(GL_INVALID_ENUM);
            }

            switch (pname)
            {
              case GL_TEXTURE_MAG_FILTER:
                *params = texture->getMagFilter();
                break;
              case GL_TEXTURE_MIN_FILTER:
                *params = texture->getMinFilter();
                break;
              case GL_TEXTURE_WRAP_S:
                *params = texture->getWrapS();
                break;
              case GL_TEXTURE_WRAP_T:
                *params = texture->getWrapT();
                break;
              case GL_TEXTURE_WRAP_R:
                if (context->getClientVersion() < 3)
                {
                    return gl::error(GL_INVALID_ENUM);
                }
                *params = texture->getWrapR();
                break;
              case GL_TEXTURE_IMMUTABLE_FORMAT:
                // Exposed to ES2.0 through EXT_texture_storage, no client version validation.
                *params = texture->isImmutable() ? GL_TRUE : GL_FALSE;
                break;
              case GL_TEXTURE_IMMUTABLE_LEVELS:
                if (context->getClientVersion() < 3)
                {
                    return gl::error(GL_INVALID_ENUM);
                }
                *params = texture->immutableLevelCount();
                break;
              case GL_TEXTURE_USAGE_ANGLE:
                *params = texture->getUsage();
                break;
              case GL_TEXTURE_MAX_ANISOTROPY_EXT:
                if (!context->supportsTextureFilterAnisotropy())
                {
                    return gl::error(GL_INVALID_ENUM);
                }
                *params = (GLint)texture->getMaxAnisotropy();
                break;
              case GL_TEXTURE_SWIZZLE_R:
                if (context->getClientVersion() < 3)
                {
                    return gl::error(GL_INVALID_ENUM);
                }
                *params = texture->getSwizzleRed();
                break;
              case GL_TEXTURE_SWIZZLE_G:
                if (context->getClientVersion() < 3)
                {
                    return gl::error(GL_INVALID_ENUM);
                }
                *params = texture->getSwizzleGreen();
                break;
              case GL_TEXTURE_SWIZZLE_B:
                if (context->getClientVersion() < 3)
                {
                    return gl::error(GL_INVALID_ENUM);
                }
                *params = texture->getSwizzleBlue();
                break;
              case GL_TEXTURE_SWIZZLE_A:
                if (context->getClientVersion() < 3)
                {
                    return gl::error(GL_INVALID_ENUM);
                }
                *params = texture->getSwizzleAlpha();
                break;
              case GL_TEXTURE_BASE_LEVEL:
                if (context->getClientVersion() < 3)
                {
                    return gl::error(GL_INVALID_ENUM);
                }
                *params = texture->getBaseLevel();
                break;
              case GL_TEXTURE_MAX_LEVEL:
                if (context->getClientVersion() < 3)
                {
                    return gl::error(GL_INVALID_ENUM);
                }
                *params = texture->getMaxLevel();
                break;
              case GL_TEXTURE_MIN_LOD:
                if (context->getClientVersion() < 3)
                {
                    return gl::error(GL_INVALID_ENUM);
                }
                *params = (GLint)texture->getMinLod();
                break;
              case GL_TEXTURE_MAX_LOD:
                if (context->getClientVersion() < 3)
                {
                    return gl::error(GL_INVALID_ENUM);
                }
                *params = (GLint)texture->getMaxLod();
                break;
              default:
                return gl::error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetnUniformfvEXT(GLuint program, GLint location, GLsizei bufSize, GLfloat* params)
{
    EVENT("(GLuint program = %d, GLint location = %d, GLsizei bufSize = %d, GLfloat* params = 0x%0.8p)",
          program, location, bufSize, params);

    try
    {
        if (bufSize < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (program == 0)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            gl::Program *programObject = context->getProgram(program);

            if (!programObject || !programObject->isLinked())
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            gl::ProgramBinary *programBinary = programObject->getProgramBinary();
            if (!programBinary)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!programBinary->getUniformfv(location, &bufSize, params))
            {
                return gl::error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetUniformfv(GLuint program, GLint location, GLfloat* params)
{
    EVENT("(GLuint program = %d, GLint location = %d, GLfloat* params = 0x%0.8p)", program, location, params);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (program == 0)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            gl::Program *programObject = context->getProgram(program);

            if (!programObject || !programObject->isLinked())
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            gl::ProgramBinary *programBinary = programObject->getProgramBinary();
            if (!programBinary)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!programBinary->getUniformfv(location, NULL, params))
            {
                return gl::error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetnUniformivEXT(GLuint program, GLint location, GLsizei bufSize, GLint* params)
{
    EVENT("(GLuint program = %d, GLint location = %d, GLsizei bufSize = %d, GLint* params = 0x%0.8p)", 
          program, location, bufSize, params);

    try
    {
        if (bufSize < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (program == 0)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            gl::Program *programObject = context->getProgram(program);

            if (!programObject || !programObject->isLinked())
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            gl::ProgramBinary *programBinary = programObject->getProgramBinary();
            if (!programBinary)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!programBinary->getUniformiv(location, &bufSize, params))
            {
                return gl::error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetUniformiv(GLuint program, GLint location, GLint* params)
{
    EVENT("(GLuint program = %d, GLint location = %d, GLint* params = 0x%0.8p)", program, location, params);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (program == 0)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            gl::Program *programObject = context->getProgram(program);

            if (!programObject || !programObject->isLinked())
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            gl::ProgramBinary *programBinary = programObject->getProgramBinary();
            if (!programBinary)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!programBinary->getUniformiv(location, NULL, params))
            {
                return gl::error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

int __stdcall glGetUniformLocation(GLuint program, const GLchar* name)
{
    EVENT("(GLuint program = %d, const GLchar* name = 0x%0.8p)", program, name);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (strstr(name, "gl_") == name)
        {
            return -1;
        }

        if (context)
        {
            gl::Program *programObject = context->getProgram(program);

            if (!programObject)
            {
                if (context->getShader(program))
                {
                    return gl::error(GL_INVALID_OPERATION, -1);
                }
                else
                {
                    return gl::error(GL_INVALID_VALUE, -1);
                }
            }

            gl::ProgramBinary *programBinary = programObject->getProgramBinary();
            if (!programObject->isLinked() || !programBinary)
            {
                return gl::error(GL_INVALID_OPERATION, -1);
            }

            return programBinary->getUniformLocation(name);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY, -1);
    }

    return -1;
}

void __stdcall glGetVertexAttribfv(GLuint index, GLenum pname, GLfloat* params)
{
    EVENT("(GLuint index = %d, GLenum pname = 0x%X, GLfloat* params = 0x%0.8p)", index, pname, params);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (index >= gl::MAX_VERTEX_ATTRIBS)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            const gl::VertexAttribute &attribState = context->getVertexAttribState(index);

            if (!gl::ValidateGetVertexAttribParameters(pname, context->getClientVersion()))
            {
                return;
            }

            if (pname == GL_CURRENT_VERTEX_ATTRIB)
            {
                const gl::VertexAttribCurrentValueData &currentValueData = context->getVertexAttribCurrentValue(index);
                for (int i = 0; i < 4; ++i)
                {
                    params[i] = currentValueData.FloatValues[i];
                }
            }
            else
            {
                *params = attribState.querySingleParameter<GLfloat>(pname);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetVertexAttribiv(GLuint index, GLenum pname, GLint* params)
{
    EVENT("(GLuint index = %d, GLenum pname = 0x%X, GLint* params = 0x%0.8p)", index, pname, params);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (index >= gl::MAX_VERTEX_ATTRIBS)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            const gl::VertexAttribute &attribState = context->getVertexAttribState(index);

            if (!gl::ValidateGetVertexAttribParameters(pname, context->getClientVersion()))
            {
                return;
            }

            if (pname == GL_CURRENT_VERTEX_ATTRIB)
            {
                const gl::VertexAttribCurrentValueData &currentValueData = context->getVertexAttribCurrentValue(index);
                for (int i = 0; i < 4; ++i)
                {
                    float currentValue = currentValueData.FloatValues[i];
                    params[i] = gl::iround<GLint>(currentValue);
                }
            }
            else
            {
                *params = attribState.querySingleParameter<GLint>(pname);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetVertexAttribPointerv(GLuint index, GLenum pname, GLvoid** pointer)
{
    EVENT("(GLuint index = %d, GLenum pname = 0x%X, GLvoid** pointer = 0x%0.8p)", index, pname, pointer);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (index >= gl::MAX_VERTEX_ATTRIBS)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            if (pname != GL_VERTEX_ATTRIB_ARRAY_POINTER)
            {
                return gl::error(GL_INVALID_ENUM);
            }

            *pointer = const_cast<GLvoid*>(context->getVertexAttribPointer(index));
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glHint(GLenum target, GLenum mode)
{
    EVENT("(GLenum target = 0x%X, GLenum mode = 0x%X)", target, mode);

    try
    {
        switch (mode)
        {
          case GL_FASTEST:
          case GL_NICEST:
          case GL_DONT_CARE:
            break;
          default:
            return gl::error(GL_INVALID_ENUM); 
        }

        gl::Context *context = gl::getNonLostContext();
        switch (target)
        {
          case GL_GENERATE_MIPMAP_HINT:
            if (context) context->setGenerateMipmapHint(mode);
            break;
          case GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES:
            if (context) context->setFragmentShaderDerivativeHint(mode);
            break;
          default:
            return gl::error(GL_INVALID_ENUM);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

GLboolean __stdcall glIsBuffer(GLuint buffer)
{
    EVENT("(GLuint buffer = %d)", buffer);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context && buffer)
        {
            gl::Buffer *bufferObject = context->getBuffer(buffer);

            if (bufferObject)
            {
                return GL_TRUE;
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY, GL_FALSE);
    }

    return GL_FALSE;
}

GLboolean __stdcall glIsEnabled(GLenum cap)
{
    EVENT("(GLenum cap = 0x%X)", cap);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (!ValidCap(context, cap))
            {
                return gl::error(GL_INVALID_ENUM, false);
            }

            return context->getCap(cap);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY, false);
    }

    return false;
}

GLboolean __stdcall glIsFenceNV(GLuint fence)
{
    EVENT("(GLuint fence = %d)", fence);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            gl::FenceNV *fenceObject = context->getFenceNV(fence);

            if (fenceObject == NULL)
            {
                return GL_FALSE;
            }

            return fenceObject->isFence();
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY, GL_FALSE);
    }

    return GL_FALSE;
}

GLboolean __stdcall glIsFramebuffer(GLuint framebuffer)
{
    EVENT("(GLuint framebuffer = %d)", framebuffer);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context && framebuffer)
        {
            gl::Framebuffer *framebufferObject = context->getFramebuffer(framebuffer);

            if (framebufferObject)
            {
                return GL_TRUE;
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY, GL_FALSE);
    }

    return GL_FALSE;
}

GLboolean __stdcall glIsProgram(GLuint program)
{
    EVENT("(GLuint program = %d)", program);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context && program)
        {
            gl::Program *programObject = context->getProgram(program);

            if (programObject)
            {
                return GL_TRUE;
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY, GL_FALSE);
    }

    return GL_FALSE;
}

GLboolean __stdcall glIsQueryEXT(GLuint id)
{
    EVENT("(GLuint id = %d)", id);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            return (context->getQuery(id, false, GL_NONE) != NULL) ? GL_TRUE : GL_FALSE;
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY, GL_FALSE);
    }

    return GL_FALSE;
}

GLboolean __stdcall glIsRenderbuffer(GLuint renderbuffer)
{
    EVENT("(GLuint renderbuffer = %d)", renderbuffer);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context && renderbuffer)
        {
            gl::Renderbuffer *renderbufferObject = context->getRenderbuffer(renderbuffer);

            if (renderbufferObject)
            {
                return GL_TRUE;
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY, GL_FALSE);
    }

    return GL_FALSE;
}

GLboolean __stdcall glIsShader(GLuint shader)
{
    EVENT("(GLuint shader = %d)", shader);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context && shader)
        {
            gl::Shader *shaderObject = context->getShader(shader);

            if (shaderObject)
            {
                return GL_TRUE;
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY, GL_FALSE);
    }

    return GL_FALSE;
}

GLboolean __stdcall glIsTexture(GLuint texture)
{
    EVENT("(GLuint texture = %d)", texture);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context && texture)
        {
            gl::Texture *textureObject = context->getTexture(texture);

            if (textureObject)
            {
                return GL_TRUE;
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY, GL_FALSE);
    }

    return GL_FALSE;
}

void __stdcall glLineWidth(GLfloat width)
{
    EVENT("(GLfloat width = %f)", width);

    try
    {
        if (width <= 0.0f)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            context->setLineWidth(width);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glLinkProgram(GLuint program)
{
    EVENT("(GLuint program = %d)", program);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            gl::Program *programObject = context->getProgram(program);

            if (!programObject)
            {
                if (context->getShader(program))
                {
                    return gl::error(GL_INVALID_OPERATION);
                }
                else
                {
                    return gl::error(GL_INVALID_VALUE);
                }
            }

            context->linkProgram(program);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glPixelStorei(GLenum pname, GLint param)
{
    EVENT("(GLenum pname = 0x%X, GLint param = %d)", pname, param);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            switch (pname)
            {
              case GL_UNPACK_ALIGNMENT:
                if (param != 1 && param != 2 && param != 4 && param != 8)
                {
                    return gl::error(GL_INVALID_VALUE);
                }

                context->setUnpackAlignment(param);
                break;

              case GL_PACK_ALIGNMENT:
                if (param != 1 && param != 2 && param != 4 && param != 8)
                {
                    return gl::error(GL_INVALID_VALUE);
                }

                context->setPackAlignment(param);
                break;

              case GL_PACK_REVERSE_ROW_ORDER_ANGLE:
                context->setPackReverseRowOrder(param != 0);
                break;

              case GL_UNPACK_IMAGE_HEIGHT:
              case GL_UNPACK_SKIP_IMAGES:
              case GL_UNPACK_ROW_LENGTH:
              case GL_UNPACK_SKIP_ROWS:
              case GL_UNPACK_SKIP_PIXELS:
              case GL_PACK_ROW_LENGTH:
              case GL_PACK_SKIP_ROWS:
              case GL_PACK_SKIP_PIXELS:
                if (context->getClientVersion() < 3)
                {
                    return gl::error(GL_INVALID_ENUM);
                }
                UNIMPLEMENTED();
                break;

              default:
                return gl::error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glPolygonOffset(GLfloat factor, GLfloat units)
{
    EVENT("(GLfloat factor = %f, GLfloat units = %f)", factor, units);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            context->setPolygonOffsetParams(factor, units);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glReadnPixelsEXT(GLint x, GLint y, GLsizei width, GLsizei height,
                                GLenum format, GLenum type, GLsizei bufSize,
                                GLvoid *data)
{
    EVENT("(GLint x = %d, GLint y = %d, GLsizei width = %d, GLsizei height = %d, "
          "GLenum format = 0x%X, GLenum type = 0x%X, GLsizei bufSize = 0x%d, GLvoid *data = 0x%0.8p)",
          x, y, width, height, format, type, bufSize, data);

    try
    {
        if (width < 0 || height < 0 || bufSize < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (!gl::ValidateReadPixelsParameters(context, x, y, width, height,
                                                  format, type, &bufSize, data))
            {
                return;
            }

            context->readPixels(x, y, width, height, format, type, &bufSize, data);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height,
                            GLenum format, GLenum type, GLvoid* pixels)
{
    EVENT("(GLint x = %d, GLint y = %d, GLsizei width = %d, GLsizei height = %d, "
          "GLenum format = 0x%X, GLenum type = 0x%X, GLvoid* pixels = 0x%0.8p)",
          x, y, width, height, format, type,  pixels);

    try
    {
        if (width < 0 || height < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (!gl::ValidateReadPixelsParameters(context, x, y, width, height,
                                                  format, type, NULL, pixels))
            {
                return;
            }

            context->readPixels(x, y, width, height, format, type, NULL, pixels);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glReleaseShaderCompiler(void)
{
    EVENT("()");

    try
    {
        gl::Shader::releaseCompiler();
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glRenderbufferStorageMultisampleANGLE(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height)
{
    EVENT("(GLenum target = 0x%X, GLsizei samples = %d, GLenum internalformat = 0x%X, GLsizei width = %d, GLsizei height = %d)",
          target, samples, internalformat, width, height);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (!ValidateRenderbufferStorageParameters(context, target, samples, internalformat,
                                                       width, height, true))
            {
                return;
            }

            context->setRenderbufferStorage(width, height, internalformat, samples);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
    glRenderbufferStorageMultisampleANGLE(target, 0, internalformat, width, height);
}

void __stdcall glSampleCoverage(GLclampf value, GLboolean invert)
{
    EVENT("(GLclampf value = %f, GLboolean invert = %u)", value, invert);

    try
    {
        gl::Context* context = gl::getNonLostContext();

        if (context)
        {
            context->setSampleCoverageParams(gl::clamp01(value), invert == GL_TRUE);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glSetFenceNV(GLuint fence, GLenum condition)
{
    EVENT("(GLuint fence = %d, GLenum condition = 0x%X)", fence, condition);

    try
    {
        if (condition != GL_ALL_COMPLETED_NV)
        {
            return gl::error(GL_INVALID_ENUM);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            gl::FenceNV *fenceObject = context->getFenceNV(fence);

            if (fenceObject == NULL)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            fenceObject->setFence(condition);    
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glScissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
    EVENT("(GLint x = %d, GLint y = %d, GLsizei width = %d, GLsizei height = %d)", x, y, width, height);

    try
    {
        if (width < 0 || height < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context* context = gl::getNonLostContext();

        if (context)
        {
            context->setScissorParams(x, y, width, height);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glShaderBinary(GLsizei n, const GLuint* shaders, GLenum binaryformat, const GLvoid* binary, GLsizei length)
{
    EVENT("(GLsizei n = %d, const GLuint* shaders = 0x%0.8p, GLenum binaryformat = 0x%X, "
          "const GLvoid* binary = 0x%0.8p, GLsizei length = %d)",
          n, shaders, binaryformat, binary, length);

    try
    {
        // No binary shader formats are supported.
        return gl::error(GL_INVALID_ENUM);
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glShaderSource(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length)
{
    EVENT("(GLuint shader = %d, GLsizei count = %d, const GLchar** string = 0x%0.8p, const GLint* length = 0x%0.8p)",
          shader, count, string, length);

    try
    {
        if (count < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            gl::Shader *shaderObject = context->getShader(shader);

            if (!shaderObject)
            {
                if (context->getProgram(shader))
                {
                    return gl::error(GL_INVALID_OPERATION);
                }
                else
                {
                    return gl::error(GL_INVALID_VALUE);
                }
            }

            shaderObject->setSource(count, string, length);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glStencilFunc(GLenum func, GLint ref, GLuint mask)
{
    glStencilFuncSeparate(GL_FRONT_AND_BACK, func, ref, mask);
}

void __stdcall glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask)
{
    EVENT("(GLenum face = 0x%X, GLenum func = 0x%X, GLint ref = %d, GLuint mask = %d)", face, func, ref, mask);

    try
    {
        switch (face)
        {
          case GL_FRONT:
          case GL_BACK:
          case GL_FRONT_AND_BACK:
            break;
          default:
            return gl::error(GL_INVALID_ENUM);
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
            return gl::error(GL_INVALID_ENUM);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (face == GL_FRONT || face == GL_FRONT_AND_BACK)
            {
                context->setStencilParams(func, ref, mask);
            }

            if (face == GL_BACK || face == GL_FRONT_AND_BACK)
            {
                context->setStencilBackParams(func, ref, mask);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glStencilMask(GLuint mask)
{
    glStencilMaskSeparate(GL_FRONT_AND_BACK, mask);
}

void __stdcall glStencilMaskSeparate(GLenum face, GLuint mask)
{
    EVENT("(GLenum face = 0x%X, GLuint mask = %d)", face, mask);

    try
    {
        switch (face)
        {
          case GL_FRONT:
          case GL_BACK:
          case GL_FRONT_AND_BACK:
            break;
          default:
            return gl::error(GL_INVALID_ENUM);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (face == GL_FRONT || face == GL_FRONT_AND_BACK)
            {
                context->setStencilWritemask(mask);
            }

            if (face == GL_BACK || face == GL_FRONT_AND_BACK)
            {
                context->setStencilBackWritemask(mask);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glStencilOp(GLenum fail, GLenum zfail, GLenum zpass)
{
    glStencilOpSeparate(GL_FRONT_AND_BACK, fail, zfail, zpass);
}

void __stdcall glStencilOpSeparate(GLenum face, GLenum fail, GLenum zfail, GLenum zpass)
{
    EVENT("(GLenum face = 0x%X, GLenum fail = 0x%X, GLenum zfail = 0x%X, GLenum zpas = 0x%Xs)",
          face, fail, zfail, zpass);

    try
    {
        switch (face)
        {
          case GL_FRONT:
          case GL_BACK:
          case GL_FRONT_AND_BACK:
            break;
          default:
            return gl::error(GL_INVALID_ENUM);
        }

        switch (fail)
        {
          case GL_ZERO:
          case GL_KEEP:
          case GL_REPLACE:
          case GL_INCR:
          case GL_DECR:
          case GL_INVERT:
          case GL_INCR_WRAP:
          case GL_DECR_WRAP:
            break;
          default:
            return gl::error(GL_INVALID_ENUM);
        }

        switch (zfail)
        {
          case GL_ZERO:
          case GL_KEEP:
          case GL_REPLACE:
          case GL_INCR:
          case GL_DECR:
          case GL_INVERT:
          case GL_INCR_WRAP:
          case GL_DECR_WRAP:
            break;
          default:
            return gl::error(GL_INVALID_ENUM);
        }

        switch (zpass)
        {
          case GL_ZERO:
          case GL_KEEP:
          case GL_REPLACE:
          case GL_INCR:
          case GL_DECR:
          case GL_INVERT:
          case GL_INCR_WRAP:
          case GL_DECR_WRAP:
            break;
          default:
            return gl::error(GL_INVALID_ENUM);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (face == GL_FRONT || face == GL_FRONT_AND_BACK)
            {
                context->setStencilOperations(fail, zfail, zpass);
            }

            if (face == GL_BACK || face == GL_FRONT_AND_BACK)
            {
                context->setStencilBackOperations(fail, zfail, zpass);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

GLboolean __stdcall glTestFenceNV(GLuint fence)
{
    EVENT("(GLuint fence = %d)", fence);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            gl::FenceNV *fenceObject = context->getFenceNV(fence);

            if (fenceObject == NULL)
            {
                return gl::error(GL_INVALID_OPERATION, GL_TRUE);
            }

            if (fenceObject->isFence() != GL_TRUE)
            {
                return gl::error(GL_INVALID_OPERATION, GL_TRUE);
            }

            return fenceObject->testFence();
        }
    }
    catch(std::bad_alloc&)
    {
        gl::error(GL_OUT_OF_MEMORY);
    }
    
    return GL_TRUE;
}

void __stdcall glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
                            GLint border, GLenum format, GLenum type, const GLvoid* pixels)
{
    EVENT("(GLenum target = 0x%X, GLint level = %d, GLint internalformat = %d, GLsizei width = %d, GLsizei height = %d, "
          "GLint border = %d, GLenum format = 0x%X, GLenum type = 0x%X, const GLvoid* pixels = 0x%0.8p)",
          target, level, internalformat, width, height, border, format, type, pixels);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3 &&
                !ValidateES2TexImageParameters(context, target, level, internalformat, false, false,
                                               0, 0, width, height, border, format, type, pixels))
            {
                return;
            }

            if (context->getClientVersion() >= 3 &&
                !ValidateES3TexImageParameters(context, target, level, internalformat, false, false,
                                               0, 0, 0, width, height, 1, border, format, type, pixels))
            {
                return;
            }

            switch (target)
            {
              case GL_TEXTURE_2D:
                {
                    gl::Texture2D *texture = context->getTexture2D();
                    texture->setImage(level, width, height, internalformat, format, type, context->getUnpackState(), pixels);
                }
                break;
              case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
                {
                    gl::TextureCubeMap *texture = context->getTextureCubeMap();
                    texture->setImagePosX(level, width, height, internalformat, format, type, context->getUnpackState(), pixels);
                }
                break;
              case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
                {
                    gl::TextureCubeMap *texture = context->getTextureCubeMap();
                    texture->setImageNegX(level, width, height, internalformat, format, type, context->getUnpackState(), pixels);
                }
                break;
              case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
                {
                    gl::TextureCubeMap *texture = context->getTextureCubeMap();
                    texture->setImagePosY(level, width, height, internalformat, format, type, context->getUnpackState(), pixels);
                }
                break;
              case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
                {
                    gl::TextureCubeMap *texture = context->getTextureCubeMap();
                    texture->setImageNegY(level, width, height, internalformat, format, type, context->getUnpackState(), pixels);
                }
                break;
              case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
                {
                    gl::TextureCubeMap *texture = context->getTextureCubeMap();
                    texture->setImagePosZ(level, width, height, internalformat, format, type, context->getUnpackState(), pixels);
                }
                break;
              case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
                {
                    gl::TextureCubeMap *texture = context->getTextureCubeMap();
                    texture->setImageNegZ(level, width, height, internalformat, format, type, context->getUnpackState(), pixels);
                }
                break;
              default: UNREACHABLE();
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glTexParameterf(GLenum target, GLenum pname, GLfloat param)
{
    EVENT("(GLenum target = 0x%X, GLenum pname = 0x%X, GLint param = %f)", target, pname, param);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (!ValidateTexParamParameters(context, pname, static_cast<GLint>(param)))
            {
                return;
            }

            gl::Texture *texture = context->getTargetTexture(target);

            if (!texture)
            {
                return gl::error(GL_INVALID_ENUM);
            }

            switch (pname)
            {
              case GL_TEXTURE_WRAP_S:               texture->setWrapS(gl::uiround<GLenum>(param));       break;
              case GL_TEXTURE_WRAP_T:               texture->setWrapT(gl::uiround<GLenum>(param));       break;
              case GL_TEXTURE_WRAP_R:               texture->setWrapR(gl::uiround<GLenum>(param));       break;
              case GL_TEXTURE_MIN_FILTER:           texture->setMinFilter(gl::uiround<GLenum>(param));   break;
              case GL_TEXTURE_MAG_FILTER:           texture->setMagFilter(gl::uiround<GLenum>(param));   break;
              case GL_TEXTURE_USAGE_ANGLE:          texture->setUsage(gl::uiround<GLenum>(param));       break;
              case GL_TEXTURE_MAX_ANISOTROPY_EXT:   texture->setMaxAnisotropy(param, context->getTextureMaxAnisotropy()); break;
              case GL_TEXTURE_COMPARE_MODE:         texture->setCompareMode(gl::uiround<GLenum>(param)); break;
              case GL_TEXTURE_COMPARE_FUNC:         texture->setCompareFunc(gl::uiround<GLenum>(param)); break;
              case GL_TEXTURE_SWIZZLE_R:            texture->setSwizzleRed(gl::uiround<GLenum>(param));   break;
              case GL_TEXTURE_SWIZZLE_G:            texture->setSwizzleGreen(gl::uiround<GLenum>(param)); break;
              case GL_TEXTURE_SWIZZLE_B:            texture->setSwizzleBlue(gl::uiround<GLenum>(param));  break;
              case GL_TEXTURE_SWIZZLE_A:            texture->setSwizzleAlpha(gl::uiround<GLenum>(param)); break;
              case GL_TEXTURE_BASE_LEVEL:           texture->setBaseLevel(gl::iround<GLint>(param));      break;
              case GL_TEXTURE_MAX_LEVEL:            texture->setMaxLevel(gl::iround<GLint>(param));       break;
              case GL_TEXTURE_MIN_LOD:              texture->setMinLod(param);                            break;
              case GL_TEXTURE_MAX_LOD:              texture->setMaxLod(param);                            break;
              default: UNREACHABLE(); break;
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glTexParameterfv(GLenum target, GLenum pname, const GLfloat* params)
{
    glTexParameterf(target, pname, (GLfloat)*params);
}

void __stdcall glTexParameteri(GLenum target, GLenum pname, GLint param)
{
    EVENT("(GLenum target = 0x%X, GLenum pname = 0x%X, GLint param = %d)", target, pname, param);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (!ValidateTexParamParameters(context, pname, param))
            {
                return;
            }

            gl::Texture *texture = context->getTargetTexture(target);

            if (!texture)
            {
                return gl::error(GL_INVALID_ENUM);
            }

            switch (pname)
            {
              case GL_TEXTURE_WRAP_S:               texture->setWrapS((GLenum)param);       break;
              case GL_TEXTURE_WRAP_T:               texture->setWrapT((GLenum)param);       break;
              case GL_TEXTURE_WRAP_R:               texture->setWrapR((GLenum)param);       break;
              case GL_TEXTURE_MIN_FILTER:           texture->setMinFilter((GLenum)param);   break;
              case GL_TEXTURE_MAG_FILTER:           texture->setMagFilter((GLenum)param);   break;
              case GL_TEXTURE_USAGE_ANGLE:          texture->setUsage((GLenum)param);       break;
              case GL_TEXTURE_MAX_ANISOTROPY_EXT:   texture->setMaxAnisotropy((float)param, context->getTextureMaxAnisotropy()); break;
              case GL_TEXTURE_COMPARE_MODE:         texture->setCompareMode((GLenum)param); break;
              case GL_TEXTURE_COMPARE_FUNC:         texture->setCompareFunc((GLenum)param); break;
              case GL_TEXTURE_SWIZZLE_R:            texture->setSwizzleRed((GLenum)param);   break;
              case GL_TEXTURE_SWIZZLE_G:            texture->setSwizzleGreen((GLenum)param); break;
              case GL_TEXTURE_SWIZZLE_B:            texture->setSwizzleBlue((GLenum)param);  break;
              case GL_TEXTURE_SWIZZLE_A:            texture->setSwizzleAlpha((GLenum)param); break;
              case GL_TEXTURE_BASE_LEVEL:           texture->setBaseLevel(param);            break;
              case GL_TEXTURE_MAX_LEVEL:            texture->setMaxLevel(param);             break;
              case GL_TEXTURE_MIN_LOD:              texture->setMinLod((GLfloat)param);      break;
              case GL_TEXTURE_MAX_LOD:              texture->setMaxLod((GLfloat)param);      break;
              default: UNREACHABLE(); break;
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glTexParameteriv(GLenum target, GLenum pname, const GLint* params)
{
    glTexParameteri(target, pname, *params);
}

void __stdcall glTexStorage2DEXT(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)
{
    EVENT("(GLenum target = 0x%X, GLsizei levels = %d, GLenum internalformat = 0x%X, GLsizei width = %d, GLsizei height = %d)",
           target, levels, internalformat, width, height);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3 &&
                !ValidateES2TexStorageParameters(context, target, levels, internalformat, width, height))
            {
                return;
            }

            if (context->getClientVersion() >= 3 &&
                !ValidateES3TexStorageParameters(context, target, levels, internalformat, width, height, 1))
            {
                return;
            }

            switch (target)
            {
              case GL_TEXTURE_2D:
                {
                    gl::Texture2D *texture2d = context->getTexture2D();
                    texture2d->storage(levels, internalformat, width, height);
                }
                break;

              case GL_TEXTURE_CUBE_MAP:
                {
                    gl::TextureCubeMap *textureCube = context->getTextureCubeMap();
                    textureCube->storage(levels, internalformat, width);
                }
                break;

              default:
                return gl::error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                               GLenum format, GLenum type, const GLvoid* pixels)
{
    EVENT("(GLenum target = 0x%X, GLint level = %d, GLint xoffset = %d, GLint yoffset = %d, "
          "GLsizei width = %d, GLsizei height = %d, GLenum format = 0x%X, GLenum type = 0x%X, "
          "const GLvoid* pixels = 0x%0.8p)",
           target, level, xoffset, yoffset, width, height, format, type, pixels);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3 &&
                !ValidateES2TexImageParameters(context, target, level, GL_NONE, false, true,
                                               xoffset, yoffset, width, height, 0, format, type, pixels))
            {
                return;
            }

            if (context->getClientVersion() >= 3 &&
                !ValidateES3TexImageParameters(context, target, level, GL_NONE, false, true,
                                               xoffset, yoffset, 0, width, height, 1, 0, format, type, pixels))
            {
                return;
            }

            // Zero sized uploads are valid but no-ops
            if (width == 0 || height == 0)
            {
                return;
            }

            switch (target)
            {
              case GL_TEXTURE_2D:
                {
                    gl::Texture2D *texture = context->getTexture2D();
                    texture->subImage(level, xoffset, yoffset, width, height, format, type, context->getUnpackState(), pixels);
                }
                break;

              case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
              case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
              case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
              case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
              case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
              case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
                {
                    gl::TextureCubeMap *texture = context->getTextureCubeMap();
                    texture->subImage(target, level, xoffset, yoffset, width, height, format, type, context->getUnpackState(), pixels);
                }
                break;

              default:
                UNREACHABLE();
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glUniform1f(GLint location, GLfloat x)
{
    glUniform1fv(location, 1, &x);
}

void __stdcall glUniform1fv(GLint location, GLsizei count, const GLfloat* v)
{
    EVENT("(GLint location = %d, GLsizei count = %d, const GLfloat* v = 0x%0.8p)", location, count, v);

    try
    {
        if (count < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        if (location == -1)
        {
            return;
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            gl::ProgramBinary *programBinary = context->getCurrentProgramBinary();
            if (!programBinary)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!programBinary->setUniform1fv(location, count, v))
            {
                return gl::error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glUniform1i(GLint location, GLint x)
{
    glUniform1iv(location, 1, &x);
}

void __stdcall glUniform1iv(GLint location, GLsizei count, const GLint* v)
{
    EVENT("(GLint location = %d, GLsizei count = %d, const GLint* v = 0x%0.8p)", location, count, v);

    try
    {
        if (count < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        if (location == -1)
        {
            return;
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            gl::ProgramBinary *programBinary = context->getCurrentProgramBinary();
            if (!programBinary)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!programBinary->setUniform1iv(location, count, v))
            {
                return gl::error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glUniform2f(GLint location, GLfloat x, GLfloat y)
{
    GLfloat xy[2] = {x, y};

    glUniform2fv(location, 1, xy);
}

void __stdcall glUniform2fv(GLint location, GLsizei count, const GLfloat* v)
{
    EVENT("(GLint location = %d, GLsizei count = %d, const GLfloat* v = 0x%0.8p)", location, count, v);

    try
    {
        if (count < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }
        
        if (location == -1)
        {
            return;
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            gl::ProgramBinary *programBinary = context->getCurrentProgramBinary();
            if (!programBinary)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!programBinary->setUniform2fv(location, count, v))
            {
                return gl::error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glUniform2i(GLint location, GLint x, GLint y)
{
    GLint xy[2] = {x, y};

    glUniform2iv(location, 1, xy);
}

void __stdcall glUniform2iv(GLint location, GLsizei count, const GLint* v)
{
    EVENT("(GLint location = %d, GLsizei count = %d, const GLint* v = 0x%0.8p)", location, count, v);

    try
    {
        if (count < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        if (location == -1)
        {
            return;
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            gl::ProgramBinary *programBinary = context->getCurrentProgramBinary();
            if (!programBinary)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!programBinary->setUniform2iv(location, count, v))
            {
                return gl::error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glUniform3f(GLint location, GLfloat x, GLfloat y, GLfloat z)
{
    GLfloat xyz[3] = {x, y, z};

    glUniform3fv(location, 1, xyz);
}

void __stdcall glUniform3fv(GLint location, GLsizei count, const GLfloat* v)
{
    EVENT("(GLint location = %d, GLsizei count = %d, const GLfloat* v = 0x%0.8p)", location, count, v);

    try
    {
        if (count < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        if (location == -1)
        {
            return;
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            gl::ProgramBinary *programBinary = context->getCurrentProgramBinary();
            if (!programBinary)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!programBinary->setUniform3fv(location, count, v))
            {
                return gl::error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glUniform3i(GLint location, GLint x, GLint y, GLint z)
{
    GLint xyz[3] = {x, y, z};

    glUniform3iv(location, 1, xyz);
}

void __stdcall glUniform3iv(GLint location, GLsizei count, const GLint* v)
{
    EVENT("(GLint location = %d, GLsizei count = %d, const GLint* v = 0x%0.8p)", location, count, v);

    try
    {
        if (count < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        if (location == -1)
        {
            return;
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            gl::ProgramBinary *programBinary = context->getCurrentProgramBinary();
            if (!programBinary)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!programBinary->setUniform3iv(location, count, v))
            {
                return gl::error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glUniform4f(GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    GLfloat xyzw[4] = {x, y, z, w};

    glUniform4fv(location, 1, xyzw);
}

void __stdcall glUniform4fv(GLint location, GLsizei count, const GLfloat* v)
{
    EVENT("(GLint location = %d, GLsizei count = %d, const GLfloat* v = 0x%0.8p)", location, count, v);

    try
    {
        if (count < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        if (location == -1)
        {
            return;
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            gl::ProgramBinary *programBinary = context->getCurrentProgramBinary();
            if (!programBinary)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!programBinary->setUniform4fv(location, count, v))
            {
                return gl::error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glUniform4i(GLint location, GLint x, GLint y, GLint z, GLint w)
{
    GLint xyzw[4] = {x, y, z, w};

    glUniform4iv(location, 1, xyzw);
}

void __stdcall glUniform4iv(GLint location, GLsizei count, const GLint* v)
{
    EVENT("(GLint location = %d, GLsizei count = %d, const GLint* v = 0x%0.8p)", location, count, v);

    try
    {
        if (count < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        if (location == -1)
        {
            return;
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            gl::ProgramBinary *programBinary = context->getCurrentProgramBinary();
            if (!programBinary)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!programBinary->setUniform4iv(location, count, v))
            {
                return gl::error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
    EVENT("(GLint location = %d, GLsizei count = %d, GLboolean transpose = %u, const GLfloat* value = 0x%0.8p)",
          location, count, transpose, value);

    try
    {
        if (count < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        if (location == -1)
        {
            return;
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (transpose != GL_FALSE && context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            gl::ProgramBinary *programBinary = context->getCurrentProgramBinary();
            if (!programBinary)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!programBinary->setUniformMatrix2fv(location, count, transpose, value))
            {
                return gl::error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
    EVENT("(GLint location = %d, GLsizei count = %d, GLboolean transpose = %u, const GLfloat* value = 0x%0.8p)",
          location, count, transpose, value);

    try
    {
        if (count < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        if (location == -1)
        {
            return;
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (transpose != GL_FALSE && context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            gl::ProgramBinary *programBinary = context->getCurrentProgramBinary();
            if (!programBinary)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!programBinary->setUniformMatrix3fv(location, count, transpose, value))
            {
                return gl::error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
    EVENT("(GLint location = %d, GLsizei count = %d, GLboolean transpose = %u, const GLfloat* value = 0x%0.8p)",
          location, count, transpose, value);

    try
    {
        if (count < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        if (location == -1)
        {
            return;
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (transpose != GL_FALSE && context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            gl::ProgramBinary *programBinary = context->getCurrentProgramBinary();
            if (!programBinary)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!programBinary->setUniformMatrix4fv(location, count, transpose, value))
            {
                return gl::error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glUseProgram(GLuint program)
{
    EVENT("(GLuint program = %d)", program);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            gl::Program *programObject = context->getProgram(program);

            if (!programObject && program != 0)
            {
                if (context->getShader(program))
                {
                    return gl::error(GL_INVALID_OPERATION);
                }
                else
                {
                    return gl::error(GL_INVALID_VALUE);
                }
            }

            if (program != 0 && !programObject->isLinked())
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            context->useProgram(program);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glValidateProgram(GLuint program)
{
    EVENT("(GLuint program = %d)", program);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            gl::Program *programObject = context->getProgram(program);

            if (!programObject)
            {
                if (context->getShader(program))
                {
                    return gl::error(GL_INVALID_OPERATION);
                }
                else
                {
                    return gl::error(GL_INVALID_VALUE);
                }
            }

            programObject->validate();
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glVertexAttrib1f(GLuint index, GLfloat x)
{
    EVENT("(GLuint index = %d, GLfloat x = %f)", index, x);

    try
    {
        if (index >= gl::MAX_VERTEX_ATTRIBS)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            GLfloat vals[4] = { x, 0, 0, 1 };
            context->setVertexAttribf(index, vals);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glVertexAttrib1fv(GLuint index, const GLfloat* values)
{
    EVENT("(GLuint index = %d, const GLfloat* values = 0x%0.8p)", index, values);

    try
    {
        if (index >= gl::MAX_VERTEX_ATTRIBS)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            GLfloat vals[4] = { values[0], 0, 0, 1 };
            context->setVertexAttribf(index, vals);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glVertexAttrib2f(GLuint index, GLfloat x, GLfloat y)
{
    EVENT("(GLuint index = %d, GLfloat x = %f, GLfloat y = %f)", index, x, y);

    try
    {
        if (index >= gl::MAX_VERTEX_ATTRIBS)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            GLfloat vals[4] = { x, y, 0, 1 };
            context->setVertexAttribf(index, vals);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glVertexAttrib2fv(GLuint index, const GLfloat* values)
{
    EVENT("(GLuint index = %d, const GLfloat* values = 0x%0.8p)", index, values);

    try
    {
        if (index >= gl::MAX_VERTEX_ATTRIBS)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            GLfloat vals[4] = { values[0], values[1], 0, 1 };
            context->setVertexAttribf(index, vals);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glVertexAttrib3f(GLuint index, GLfloat x, GLfloat y, GLfloat z)
{
    EVENT("(GLuint index = %d, GLfloat x = %f, GLfloat y = %f, GLfloat z = %f)", index, x, y, z);

    try
    {
        if (index >= gl::MAX_VERTEX_ATTRIBS)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            GLfloat vals[4] = { x, y, z, 1 };
            context->setVertexAttribf(index, vals);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glVertexAttrib3fv(GLuint index, const GLfloat* values)
{
    EVENT("(GLuint index = %d, const GLfloat* values = 0x%0.8p)", index, values);

    try
    {
        if (index >= gl::MAX_VERTEX_ATTRIBS)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            GLfloat vals[4] = { values[0], values[1], values[2], 1 };
            context->setVertexAttribf(index, vals);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glVertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    EVENT("(GLuint index = %d, GLfloat x = %f, GLfloat y = %f, GLfloat z = %f, GLfloat w = %f)", index, x, y, z, w);

    try
    {
        if (index >= gl::MAX_VERTEX_ATTRIBS)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            GLfloat vals[4] = { x, y, z, w };
            context->setVertexAttribf(index, vals);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glVertexAttrib4fv(GLuint index, const GLfloat* values)
{
    EVENT("(GLuint index = %d, const GLfloat* values = 0x%0.8p)", index, values);

    try
    {
        if (index >= gl::MAX_VERTEX_ATTRIBS)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            context->setVertexAttribf(index, values);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glVertexAttribDivisorANGLE(GLuint index, GLuint divisor)
{
    EVENT("(GLuint index = %d, GLuint divisor = %d)", index, divisor);

    try
    {
        if (index >= gl::MAX_VERTEX_ATTRIBS)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            context->setVertexAttribDivisor(index, divisor);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* ptr)
{
    EVENT("(GLuint index = %d, GLint size = %d, GLenum type = 0x%X, "
          "GLboolean normalized = %u, GLsizei stride = %d, const GLvoid* ptr = 0x%0.8p)",
          index, size, type, normalized, stride, ptr);

    try
    {
        if (index >= gl::MAX_VERTEX_ATTRIBS)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        if (size < 1 || size > 4)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        switch (type)
        {
          case GL_BYTE:
          case GL_UNSIGNED_BYTE:
          case GL_SHORT:
          case GL_UNSIGNED_SHORT:
          case GL_FIXED:
          case GL_FLOAT:
            break;
          case GL_HALF_FLOAT:
          case GL_INT:
          case GL_UNSIGNED_INT:
          case GL_INT_2_10_10_10_REV:
          case GL_UNSIGNED_INT_2_10_10_10_REV:
            if (context && context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_ENUM);
            }
            else
            {
                break;
            }
          default:
            return gl::error(GL_INVALID_ENUM);
        }

        if (stride < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        if ((type == GL_INT_2_10_10_10_REV || type == GL_UNSIGNED_INT_2_10_10_10_REV) && size != 4)
        {
            return gl::error(GL_INVALID_OPERATION);
        }

        if (context)
        {
            // [OpenGL ES 3.0.2] Section 2.8 page 24:
            // An INVALID_OPERATION error is generated when a non-zero vertex array object
            // is bound, zero is bound to the ARRAY_BUFFER buffer object binding point,
            // and the pointer argument is not NULL.
            if (context->getVertexArrayHandle() != 0 && context->getArrayBufferHandle() == 0 && ptr != NULL)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            context->setVertexAttribState(index, context->getArrayBuffer(), size, type,
                                          normalized == GL_TRUE, false, stride, ptr);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
    EVENT("(GLint x = %d, GLint y = %d, GLsizei width = %d, GLsizei height = %d)", x, y, width, height);

    try
    {
        if (width < 0 || height < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            context->setViewportParams(x, y, width, height);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

// OpenGL ES 3.0 functions

void __stdcall glReadBuffer(GLenum mode)
{
    EVENT("(GLenum mode = 0x%X)", mode);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            // glReadBuffer
            UNIMPLEMENTED();
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid* indices)
{
    EVENT("(GLenum mode = 0x%X, GLuint start = %u, GLuint end = %u, GLsizei count = %d, GLenum type = 0x%X, "
          "const GLvoid* indices = 0x%0.8p)", mode, start, end, count, type, indices);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            // glDrawRangeElements
            UNIMPLEMENTED();
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glTexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid* pixels)
{
    EVENT("(GLenum target = 0x%X, GLint level = %d, GLint internalformat = %d, GLsizei width = %d, "
          "GLsizei height = %d, GLsizei depth = %d, GLint border = %d, GLenum format = 0x%X, "
          "GLenum type = 0x%X, const GLvoid* pixels = 0x%0.8p)",
          target, level, internalformat, width, height, depth, border, format, type, pixels);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            // validateES3TexImageFormat sets the error code if there is an error
            if (!ValidateES3TexImageParameters(context, target, level, internalformat, false, false,
                                               0, 0, 0, width, height, depth, border, format, type, pixels))
            {
                return;
            }

            switch(target)
            {
              case GL_TEXTURE_3D:
                {
                    gl::Texture3D *texture = context->getTexture3D();
                    texture->setImage(level, width, height, depth, internalformat, format, type, context->getUnpackState(), pixels);
                }
                break;

              case GL_TEXTURE_2D_ARRAY:
                {
                    gl::Texture2DArray *texture = context->getTexture2DArray();
                    texture->setImage(level, width, height, depth, internalformat, format, type, context->getUnpackState(), pixels);
                }
                break;

              default:
                return gl::error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid* pixels)
{
    EVENT("(GLenum target = 0x%X, GLint level = %d, GLint xoffset = %d, GLint yoffset = %d, "
          "GLint zoffset = %d, GLsizei width = %d, GLsizei height = %d, GLsizei depth = %d, "
          "GLenum format = 0x%X, GLenum type = 0x%X, const GLvoid* pixels = 0x%0.8p)",
          target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            // validateES3TexImageFormat sets the error code if there is an error
            if (!ValidateES3TexImageParameters(context, target, level, GL_NONE, false, true,
                                               xoffset, yoffset, zoffset, width, height, depth, 0,
                                               format, type, pixels))
            {
                return;
            }

            // Zero sized uploads are valid but no-ops
            if (width == 0 || height == 0 || depth == 0)
            {
                return;
            }

            switch(target)
            {
              case GL_TEXTURE_3D:
                {
                    gl::Texture3D *texture = context->getTexture3D();
                    texture->subImage(level, xoffset, yoffset, zoffset, width, height, depth, format, type, context->getUnpackState(), pixels);
                }
                break;

              case GL_TEXTURE_2D_ARRAY:
                {
                    gl::Texture2DArray *texture = context->getTexture2DArray();
                    texture->subImage(level, xoffset, yoffset, zoffset, width, height, depth, format, type, context->getUnpackState(), pixels);
                }
                break;

              default:
                return gl::error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glCopyTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
    EVENT("(GLenum target = 0x%X, GLint level = %d, GLint xoffset = %d, GLint yoffset = %d, "
          "GLint zoffset = %d, GLint x = %d, GLint y = %d, GLsizei width = %d, GLsizei height = %d)",
          target, level, xoffset, yoffset, zoffset, x, y, width, height);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!ValidateES3CopyTexImageParameters(context, target, level, GL_NONE, false, xoffset, yoffset, zoffset,
                                                   x, y, width, height, 0))
            {
                return;
            }

            // Zero sized copies are valid but no-ops
            if (width == 0 || height == 0)
            {
                return;
            }

            gl::Framebuffer *framebuffer = context->getReadFramebuffer();
            gl::Texture *texture = NULL;
            switch (target)
            {
              case GL_TEXTURE_3D:
                texture = context->getTexture3D();
                break;

              case GL_TEXTURE_2D_ARRAY:
                texture = context->getTexture2DArray();
                break;

              default:
                return gl::error(GL_INVALID_ENUM);
            }

            texture->copySubImage(target, level, xoffset, yoffset, zoffset, x, y, width, height, framebuffer);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glCompressedTexImage3D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const GLvoid* data)
{
    EVENT("(GLenum target = 0x%X, GLint level = %d, GLenum internalformat = 0x%X, GLsizei width = %d, "
          "GLsizei height = %d, GLsizei depth = %d, GLint border = %d, GLsizei imageSize = %d, "
          "const GLvoid* data = 0x%0.8p)",
          target, level, internalformat, width, height, depth, border, imageSize, data);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (imageSize < 0 || imageSize != (GLsizei)gl::GetBlockSize(internalformat, GL_UNSIGNED_BYTE, context->getClientVersion(), width, height))
            {
                return gl::error(GL_INVALID_VALUE);
            }

            // validateES3TexImageFormat sets the error code if there is an error
            if (!ValidateES3TexImageParameters(context, target, level, internalformat, true, false,
                                               0, 0, 0, width, height, depth, border, GL_NONE, GL_NONE, data))
            {
                return;
            }

            switch(target)
            {
              case GL_TEXTURE_3D:
                {
                    gl::Texture3D *texture = context->getTexture3D();
                    texture->setCompressedImage(level, internalformat, width, height, depth, imageSize, data);
                }
                break;

              case GL_TEXTURE_2D_ARRAY:
                {
                    gl::Texture2DArray *texture = context->getTexture2DArray();
                    texture->setCompressedImage(level, internalformat, width, height, depth, imageSize, data);
                }
                break;

              default:
                return gl::error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glCompressedTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const GLvoid* data)
{
    EVENT("(GLenum target = 0x%X, GLint level = %d, GLint xoffset = %d, GLint yoffset = %d, "
        "GLint zoffset = %d, GLsizei width = %d, GLsizei height = %d, GLsizei depth = %d, "
        "GLenum format = 0x%X, GLsizei imageSize = %d, const GLvoid* data = 0x%0.8p)",
        target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (imageSize < 0 || imageSize != (GLsizei)gl::GetBlockSize(format, GL_UNSIGNED_BYTE, context->getClientVersion(), width, height))
            {
                return gl::error(GL_INVALID_VALUE);
            }

            if (!data)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            // validateES3TexImageFormat sets the error code if there is an error
            if (!ValidateES3TexImageParameters(context, target, level, GL_NONE, true, true,
                                               0, 0, 0, width, height, depth, 0, GL_NONE, GL_NONE, data))
            {
                return;
            }

            // Zero sized uploads are valid but no-ops
            if (width == 0 || height == 0)
            {
                return;
            }

            switch(target)
            {
              case GL_TEXTURE_3D:
                {
                    gl::Texture3D *texture = context->getTexture3D();
                    texture->subImageCompressed(level, xoffset, yoffset, zoffset, width, height, depth,
                                                format, imageSize, data);
                }
                break;

              case GL_TEXTURE_2D_ARRAY:
                {
                    gl::Texture2DArray *texture = context->getTexture2DArray();
                    texture->subImageCompressed(level, xoffset, yoffset, zoffset, width, height, depth,
                                                format, imageSize, data);
                }
                break;

            default:
                return gl::error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGenQueries(GLsizei n, GLuint* ids)
{
    EVENT("(GLsizei n = %d, GLuint* ids = 0x%0.8p)", n, ids);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (n < 0)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            for (GLsizei i = 0; i < n; i++)
            {
                ids[i] = context->createQuery();
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDeleteQueries(GLsizei n, const GLuint* ids)
{
    EVENT("(GLsizei n = %d, GLuint* ids = 0x%0.8p)", n, ids);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (n < 0)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            for (GLsizei i = 0; i < n; i++)
            {
                context->deleteQuery(ids[i]);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

GLboolean __stdcall glIsQuery(GLuint id)
{
    EVENT("(GLuint id = %u)", id);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION, GL_FALSE);
            }

            return (context->getQuery(id, false, GL_NONE) != NULL) ? GL_TRUE : GL_FALSE;
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY, GL_FALSE);
    }

    return GL_FALSE;
}

void __stdcall glBeginQuery(GLenum target, GLuint id)
{
    EVENT("(GLenum target = 0x%X, GLuint id = %u)", target, id);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!ValidQueryType(context, target))
            {
                return gl::error(GL_INVALID_ENUM);
            }

            if (id == 0)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            context->beginQuery(target, id);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glEndQuery(GLenum target)
{
    EVENT("(GLenum target = 0x%X)", target);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!ValidQueryType(context, target))
            {
                return gl::error(GL_INVALID_ENUM);
            }

            context->endQuery(target);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetQueryiv(GLenum target, GLenum pname, GLint* params)
{
    EVENT("(GLenum target = 0x%X, GLenum pname = 0x%X, GLint* params = 0x%0.8p)", target, pname, params);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!ValidQueryType(context, target))
            {
                return gl::error(GL_INVALID_ENUM);
            }

            switch (pname)
            {
              case GL_CURRENT_QUERY:
                params[0] = context->getActiveQuery(target);
                break;

              default:
                return gl::error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetQueryObjectuiv(GLuint id, GLenum pname, GLuint* params)
{
    EVENT("(GLuint id = %u, GLenum pname = 0x%X, GLint* params = 0x%0.8p)", id, pname, params);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            gl::Query *queryObject = context->getQuery(id, false, GL_NONE);

            if (!queryObject)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (context->getActiveQuery(queryObject->getType()) == id)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            switch(pname)
            {
              case GL_QUERY_RESULT:
                params[0] = queryObject->getResult();
                break;
              case GL_QUERY_RESULT_AVAILABLE:
                params[0] = queryObject->isResultAvailable();
                break;
              default:
                return gl::error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

GLboolean __stdcall glUnmapBuffer(GLenum target)
{
    EVENT("(GLenum target = 0x%X)", target);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION, GL_FALSE);
            }

            return glUnmapBufferOES(target);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY, GL_FALSE);
    }

    return GL_FALSE;
}

void __stdcall glGetBufferPointerv(GLenum target, GLenum pname, GLvoid** params)
{
    EVENT("(GLenum target = 0x%X, GLenum pname = 0x%X, GLvoid** params = 0x%0.8p)", target, pname, params);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            glGetBufferPointervOES(target, pname, params);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDrawBuffers(GLsizei n, const GLenum* bufs)
{
    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            glDrawBuffersEXT(n, bufs);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glUniformMatrix2x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
    EVENT("(GLint location = %d, GLsizei count = %d, GLboolean transpose = %u, const GLfloat* value = 0x%0.8p)",
          location, count, transpose, value);

    try
    {
        if (count < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        if (location == -1)
        {
            return;
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            gl::ProgramBinary *programBinary = context->getCurrentProgramBinary();
            if (!programBinary)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!programBinary->setUniformMatrix2x3fv(location, count, transpose, value))
            {
                return gl::error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glUniformMatrix3x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
    EVENT("(GLint location = %d, GLsizei count = %d, GLboolean transpose = %u, const GLfloat* value = 0x%0.8p)",
          location, count, transpose, value);

    try
    {
        if (count < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        if (location == -1)
        {
            return;
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            gl::ProgramBinary *programBinary = context->getCurrentProgramBinary();
            if (!programBinary)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!programBinary->setUniformMatrix3x2fv(location, count, transpose, value))
            {
                return gl::error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glUniformMatrix2x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
    EVENT("(GLint location = %d, GLsizei count = %d, GLboolean transpose = %u, const GLfloat* value = 0x%0.8p)",
          location, count, transpose, value);

    try
    {
        if (count < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        if (location == -1)
        {
            return;
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            gl::ProgramBinary *programBinary = context->getCurrentProgramBinary();
            if (!programBinary)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!programBinary->setUniformMatrix2x4fv(location, count, transpose, value))
            {
                return gl::error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glUniformMatrix4x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
    EVENT("(GLint location = %d, GLsizei count = %d, GLboolean transpose = %u, const GLfloat* value = 0x%0.8p)",
          location, count, transpose, value);

    try
    {
        if (count < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        if (location == -1)
        {
            return;
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            gl::ProgramBinary *programBinary = context->getCurrentProgramBinary();
            if (!programBinary)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!programBinary->setUniformMatrix4x2fv(location, count, transpose, value))
            {
                return gl::error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glUniformMatrix3x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
    EVENT("(GLint location = %d, GLsizei count = %d, GLboolean transpose = %u, const GLfloat* value = 0x%0.8p)",
          location, count, transpose, value);

    try
    {
        if (count < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        if (location == -1)
        {
            return;
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            gl::ProgramBinary *programBinary = context->getCurrentProgramBinary();
            if (!programBinary)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!programBinary->setUniformMatrix3x4fv(location, count, transpose, value))
            {
                return gl::error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glUniformMatrix4x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
    EVENT("(GLint location = %d, GLsizei count = %d, GLboolean transpose = %u, const GLfloat* value = 0x%0.8p)",
          location, count, transpose, value);

    try
    {
        if (count < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        if (location == -1)
        {
            return;
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            gl::ProgramBinary *programBinary = context->getCurrentProgramBinary();
            if (!programBinary)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!programBinary->setUniformMatrix4x3fv(location, count, transpose, value))
            {
                return gl::error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)
{
    EVENT("(GLint srcX0 = %d, GLint srcY0 = %d, GLint srcX1 = %d, GLint srcY1 = %d, GLint dstX0 = %d, "
          "GLint dstY0 = %d, GLint dstX1 = %d, GLint dstY1 = %d, GLbitfield mask = 0x%X, GLenum filter = 0x%X)",
          srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);

    try
    {
        gl::Context *context = gl::getNonLostContext();
        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!ValidateBlitFramebufferParameters(context, srcX0, srcY0, srcX1, srcY1,
                                                   dstX0, dstY0, dstX1, dstY1, mask, filter,
                                                   false))
            {
                return;
            }

            context->blitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1,
                                     mask, filter);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glRenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height)
{
    EVENT("(GLenum target = 0x%X, GLsizei samples = %d, GLenum internalformat = 0x%X, GLsizei width = %d, GLsizei height = %d)",
        target, samples, internalformat, width, height);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!ValidateRenderbufferStorageParameters(context, target, samples, internalformat,
                                                       width, height, false))
            {
                return;
            }

            context->setRenderbufferStorage(width, height, internalformat, samples);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer)
{
    EVENT("(GLenum target = 0x%X, GLenum attachment = 0x%X, GLuint texture = %u, GLint level = %d, GLint layer = %d)",
        target, attachment, texture, level, layer);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!ValidateES3FramebufferTextureParameters(context, target, attachment, GL_NONE, texture, level, layer, true))
            {
                return;
            }

            gl::Framebuffer *framebuffer = context->getTargetFramebuffer(target);
            ASSERT(framebuffer);

            gl::Texture *textureObject = context->getTexture(texture);
            GLenum textarget = textureObject ? textureObject->getTarget() : GL_NONE;

            if (attachment >= GL_COLOR_ATTACHMENT0_EXT && attachment <= GL_COLOR_ATTACHMENT15_EXT)
            {
                const unsigned int colorAttachment = (attachment - GL_COLOR_ATTACHMENT0_EXT);
                framebuffer->setColorbuffer(colorAttachment, textarget, texture, level, layer);
            }
            else
            {
                switch (attachment)
                {
                case GL_DEPTH_ATTACHMENT:         framebuffer->setDepthbuffer(textarget, texture, level, layer);        break;
                case GL_STENCIL_ATTACHMENT:       framebuffer->setStencilbuffer(textarget, texture, level, layer);      break;
                case GL_DEPTH_STENCIL_ATTACHMENT: framebuffer->setDepthStencilBuffer(textarget, texture, level, layer); break;
                }
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

GLvoid* __stdcall glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access)
{
    EVENT("(GLenum target = 0x%X, GLintptr offset = %d, GLsizeiptr length = %d, GLbitfield access = 0x%X)",
          target, offset, length, access);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION, reinterpret_cast<GLvoid*>(NULL));
            }

            return glMapBufferRangeEXT(target, offset, length, access);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY, reinterpret_cast<GLvoid*>(NULL));
    }

    return NULL;
}

void __stdcall glFlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length)
{
    EVENT("(GLenum target = 0x%X, GLintptr offset = %d, GLsizeiptr length = %d)", target, offset, length);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            glFlushMappedBufferRangeEXT(target, offset, length);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glBindVertexArray(GLuint array)
{
    EVENT("(GLuint array = %u)", array);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            gl::VertexArray *vao = context->getVertexArray(array);

            if (!vao)
            {
                // The default VAO should always exist
                ASSERT(array != 0);
                return gl::error(GL_INVALID_OPERATION);
            }

            context->bindVertexArray(array);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDeleteVertexArrays(GLsizei n, const GLuint* arrays)
{
    EVENT("(GLsizei n = %d, const GLuint* arrays = 0x%0.8p)", n, arrays);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (n < 0)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            for (int arrayIndex = 0; arrayIndex < n; arrayIndex++)
            {
                if (arrays[arrayIndex] != 0)
                {
                    context->deleteVertexArray(arrays[arrayIndex]);
                }
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGenVertexArrays(GLsizei n, GLuint* arrays)
{
    EVENT("(GLsizei n = %d, GLuint* arrays = 0x%0.8p)", n, arrays);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (n < 0)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            for (int arrayIndex = 0; arrayIndex < n; arrayIndex++)
            {
                arrays[arrayIndex] = context->createVertexArray();
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

GLboolean __stdcall glIsVertexArray(GLuint array)
{
    EVENT("(GLuint array = %u)", array);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION, GL_FALSE);
            }

            if (array == 0)
            {
                return GL_FALSE;
            }

            gl::VertexArray *vao = context->getVertexArray(array);

            return (vao != NULL ? GL_TRUE : GL_FALSE);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY, GL_FALSE);
    }

    return GL_FALSE;
}

void __stdcall glGetIntegeri_v(GLenum target, GLuint index, GLint* data)
{
    EVENT("(GLenum target = 0x%X, GLuint index = %u, GLint* data = 0x%0.8p)",
          target, index, data);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            switch (target)
            {
              case GL_TRANSFORM_FEEDBACK_BUFFER_START:
              case GL_TRANSFORM_FEEDBACK_BUFFER_SIZE:
              case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
                if (index >= context->getMaxTransformFeedbackBufferBindings())
                    return gl::error(GL_INVALID_VALUE);
                break;
              case GL_UNIFORM_BUFFER_START:
              case GL_UNIFORM_BUFFER_SIZE:
              case GL_UNIFORM_BUFFER_BINDING:
                if (index >= context->getMaximumCombinedUniformBufferBindings())
                    return gl::error(GL_INVALID_VALUE);
                break;
              default:
                return gl::error(GL_INVALID_ENUM);
            }

            if (!(context->getIndexedIntegerv(target, index, data)))
            {
                GLenum nativeType;
                unsigned int numParams = 0;
                if (!context->getIndexedQueryParameterInfo(target, &nativeType, &numParams))
                    return gl::error(GL_INVALID_ENUM);

                if (numParams == 0)
                    return; // it is known that pname is valid, but there are no parameters to return

                if (nativeType == GL_INT_64_ANGLEX)
                {
                    GLint64 minIntValue = static_cast<GLint64>(std::numeric_limits<int>::min());
                    GLint64 maxIntValue = static_cast<GLint64>(std::numeric_limits<int>::max());
                    GLint64 *int64Params = new GLint64[numParams];

                    context->getIndexedInteger64v(target, index, int64Params);

                    for (unsigned int i = 0; i < numParams; ++i)
                    {
                        GLint64 clampedValue = std::max(std::min(int64Params[i], maxIntValue), minIntValue);
                        data[i] = static_cast<GLint>(clampedValue);
                    }

                    delete [] int64Params;
                }
                else
                {
                    UNREACHABLE();
                }
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glBeginTransformFeedback(GLenum primitiveMode)
{
    EVENT("(GLenum primitiveMode = 0x%X)", primitiveMode);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            switch (primitiveMode)
            {
              case GL_TRIANGLES:
              case GL_LINES:
              case GL_POINTS:
                break;
              default:
                return gl::error(GL_INVALID_ENUM);
            }

            gl::TransformFeedback *transformFeedback = context->getCurrentTransformFeedback();
            ASSERT(transformFeedback != NULL);

            if (transformFeedback->isStarted())
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (transformFeedback->isPaused())
            {
                transformFeedback->resume();
            }
            else
            {
                transformFeedback->start(primitiveMode);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glEndTransformFeedback(void)
{
    EVENT("(void)");

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            gl::TransformFeedback *transformFeedback = context->getCurrentTransformFeedback();
            ASSERT(transformFeedback != NULL);

            if (!transformFeedback->isStarted())
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            transformFeedback->stop();
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
    EVENT("(GLenum target = 0x%X, GLuint index = %u, GLuint buffer = %u, GLintptr offset = %d, GLsizeiptr size = %d)",
          target, index, buffer, offset, size);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            switch (target)
            {
              case GL_TRANSFORM_FEEDBACK_BUFFER:
                if (index >= context->getMaxTransformFeedbackBufferBindings())
                {
                    return gl::error(GL_INVALID_VALUE);
                }
                break;

              case GL_UNIFORM_BUFFER:
                if (index >= context->getMaximumCombinedUniformBufferBindings())
                {
                    return gl::error(GL_INVALID_VALUE);
                }
                break;

              default:
                return gl::error(GL_INVALID_ENUM);
            }

            if (buffer != 0 && size <= 0)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            switch (target)
            {
              case GL_TRANSFORM_FEEDBACK_BUFFER:

                // size and offset must be a multiple of 4
                if (buffer != 0 && ((offset % 4) != 0 || (size % 4) != 0))
                {
                    return gl::error(GL_INVALID_VALUE);
                }

                context->bindIndexedTransformFeedbackBuffer(buffer, index, offset, size);
                context->bindGenericTransformFeedbackBuffer(buffer);
                break;

              case GL_UNIFORM_BUFFER:

                // it is an error to bind an offset not a multiple of the alignment
                if (buffer != 0 && (offset % context->getUniformBufferOffsetAlignment()) != 0)
                {
                    return gl::error(GL_INVALID_VALUE);
                }

                context->bindIndexedUniformBuffer(buffer, index, offset, size);
                context->bindGenericUniformBuffer(buffer);
                break;

              default:
                UNREACHABLE();
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glBindBufferBase(GLenum target, GLuint index, GLuint buffer)
{
    EVENT("(GLenum target = 0x%X, GLuint index = %u, GLuint buffer = %u)",
          target, index, buffer);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            switch (target)
            {
              case GL_TRANSFORM_FEEDBACK_BUFFER:
                if (index >= context->getMaxTransformFeedbackBufferBindings())
                {
                    return gl::error(GL_INVALID_VALUE);
                }
                break;

              case GL_UNIFORM_BUFFER:
                if (index >= context->getMaximumCombinedUniformBufferBindings())
                {
                    return gl::error(GL_INVALID_VALUE);
                }
                break;

              default:
                return gl::error(GL_INVALID_ENUM);
            }

            switch (target)
            {
              case GL_TRANSFORM_FEEDBACK_BUFFER:
                context->bindIndexedTransformFeedbackBuffer(buffer, index, 0, 0);
                context->bindGenericTransformFeedbackBuffer(buffer);
                break;

              case GL_UNIFORM_BUFFER:
                context->bindIndexedUniformBuffer(buffer, index, 0, 0);
                context->bindGenericUniformBuffer(buffer);
                break;

              default:
                UNREACHABLE();
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glTransformFeedbackVaryings(GLuint program, GLsizei count, const GLchar* const* varyings, GLenum bufferMode)
{
    EVENT("(GLuint program = %u, GLsizei count = %d, const GLchar* const* varyings = 0x%0.8p, GLenum bufferMode = 0x%X)",
          program, count, varyings, bufferMode);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (count < 0)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            switch (bufferMode)
            {
              case GL_INTERLEAVED_ATTRIBS:
                break;
              case GL_SEPARATE_ATTRIBS:
                if (static_cast<GLuint>(count) > context->getMaxTransformFeedbackBufferBindings())
                {
                    return gl::error(GL_INVALID_VALUE);
                }
                break;
              default:
                return gl::error(GL_INVALID_ENUM);
            }

            if (!gl::ValidProgram(context, program))
            {
                return;
            }

            gl::Program *programObject = context->getProgram(program);
            ASSERT(programObject);

            programObject->setTransformFeedbackVaryings(count, varyings, bufferMode);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetTransformFeedbackVarying(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLsizei* size, GLenum* type, GLchar* name)
{
    EVENT("(GLuint program = %u, GLuint index = %u, GLsizei bufSize = %d, GLsizei* length = 0x%0.8p, "
          "GLsizei* size = 0x%0.8p, GLenum* type = 0x%0.8p, GLchar* name = 0x%0.8p)",
          program, index, bufSize, length, size, type, name);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (bufSize < 0)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            if (!gl::ValidProgram(context, program))
            {
                return;
            }

            gl::Program *programObject = context->getProgram(program);
            ASSERT(programObject);

            if (index >= static_cast<GLuint>(programObject->getTransformFeedbackVaryingCount()))
            {
                return gl::error(GL_INVALID_VALUE);
            }

            programObject->getTransformFeedbackVarying(index, bufSize, length, size, type, name);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const GLvoid* pointer)
{
    EVENT("(GLuint index = %u, GLint size = %d, GLenum type = 0x%X, GLsizei stride = %d, const GLvoid* pointer = 0x%0.8p)",
          index, size, type, stride, pointer);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }
        }

        if (index >= gl::MAX_VERTEX_ATTRIBS)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        if (size < 1 || size > 4)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        switch (type)
        {
          case GL_BYTE:
          case GL_UNSIGNED_BYTE:
          case GL_SHORT:
          case GL_UNSIGNED_SHORT:
          case GL_INT:
          case GL_UNSIGNED_INT:
          case GL_INT_2_10_10_10_REV:
          case GL_UNSIGNED_INT_2_10_10_10_REV:
            break;
          default:
            return gl::error(GL_INVALID_ENUM);
        }

        if (stride < 0)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        if ((type == GL_INT_2_10_10_10_REV || type == GL_UNSIGNED_INT_2_10_10_10_REV) && size != 4)
        {
            return gl::error(GL_INVALID_OPERATION);
        }

        if (context)
        {
            // [OpenGL ES 3.0.2] Section 2.8 page 24:
            // An INVALID_OPERATION error is generated when a non-zero vertex array object
            // is bound, zero is bound to the ARRAY_BUFFER buffer object binding point,
            // and the pointer argument is not NULL.
            if (context->getVertexArrayHandle() != 0 && context->getArrayBufferHandle() == 0 && pointer != NULL)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            context->setVertexAttribState(index, context->getArrayBuffer(), size, type, false, true,
                                          stride, pointer);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetVertexAttribIiv(GLuint index, GLenum pname, GLint* params)
{
    EVENT("(GLuint index = %u, GLenum pname = 0x%X, GLint* params = 0x%0.8p)",
          index, pname, params);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (index >= gl::MAX_VERTEX_ATTRIBS)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            const gl::VertexAttribute &attribState = context->getVertexAttribState(index);

            if (!gl::ValidateGetVertexAttribParameters(pname, context->getClientVersion()))
            {
                return;
            }

            if (pname == GL_CURRENT_VERTEX_ATTRIB)
            {
                const gl::VertexAttribCurrentValueData &currentValueData = context->getVertexAttribCurrentValue(index);
                for (int i = 0; i < 4; ++i)
                {
                    params[i] = currentValueData.IntValues[i];
                }
            }
            else
            {
                *params = attribState.querySingleParameter<GLint>(pname);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetVertexAttribIuiv(GLuint index, GLenum pname, GLuint* params)
{
    EVENT("(GLuint index = %u, GLenum pname = 0x%X, GLuint* params = 0x%0.8p)",
          index, pname, params);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (index >= gl::MAX_VERTEX_ATTRIBS)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            const gl::VertexAttribute &attribState = context->getVertexAttribState(index);

            if (!gl::ValidateGetVertexAttribParameters(pname, context->getClientVersion()))
            {
                return;
            }

            if (pname == GL_CURRENT_VERTEX_ATTRIB)
            {
                const gl::VertexAttribCurrentValueData &currentValueData = context->getVertexAttribCurrentValue(index);
                for (int i = 0; i < 4; ++i)
                {
                    params[i] = currentValueData.UnsignedIntValues[i];
                }
            }
            else
            {
                *params = attribState.querySingleParameter<GLuint>(pname);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glVertexAttribI4i(GLuint index, GLint x, GLint y, GLint z, GLint w)
{
    EVENT("(GLuint index = %u, GLint x = %d, GLint y = %d, GLint z = %d, GLint w = %d)",
          index, x, y, z, w);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (index >= gl::MAX_VERTEX_ATTRIBS)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            GLint vals[4] = { x, y, z, w };
            context->setVertexAttribi(index, vals);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glVertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w)
{
    EVENT("(GLuint index = %u, GLuint x = %u, GLuint y = %u, GLuint z = %u, GLuint w = %u)",
          index, x, y, z, w);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (index >= gl::MAX_VERTEX_ATTRIBS)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            GLuint vals[4] = { x, y, z, w };
            context->setVertexAttribu(index, vals);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glVertexAttribI4iv(GLuint index, const GLint* v)
{
    EVENT("(GLuint index = %u, const GLint* v = 0x%0.8p)", index, v);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (index >= gl::MAX_VERTEX_ATTRIBS)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            context->setVertexAttribi(index, v);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glVertexAttribI4uiv(GLuint index, const GLuint* v)
{
    EVENT("(GLuint index = %u, const GLuint* v = 0x%0.8p)", index, v);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (index >= gl::MAX_VERTEX_ATTRIBS)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            context->setVertexAttribu(index, v);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetUniformuiv(GLuint program, GLint location, GLuint* params)
{
    EVENT("(GLuint program = %u, GLint location = %d, GLuint* params = 0x%0.8p)",
          program, location, params);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (program == 0)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            gl::Program *programObject = context->getProgram(program);

            if (!programObject || !programObject->isLinked())
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            gl::ProgramBinary *programBinary = programObject->getProgramBinary();
            if (!programBinary)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!programBinary->getUniformuiv(location, NULL, params))
            {
                return gl::error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

GLint __stdcall glGetFragDataLocation(GLuint program, const GLchar *name)
{
    EVENT("(GLuint program = %u, const GLchar *name = 0x%0.8p)",
          program, name);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION, -1);
            }

            if (program == 0)
            {
                return gl::error(GL_INVALID_VALUE, -1);
            }

            gl::Program *programObject = context->getProgram(program);

            if (!programObject || !programObject->isLinked())
            {
                return gl::error(GL_INVALID_OPERATION, -1);
            }

            gl::ProgramBinary *programBinary = programObject->getProgramBinary();
            if (!programBinary)
            {
                return gl::error(GL_INVALID_OPERATION, -1);
            }

            return programBinary->getFragDataLocation(name);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY, 0);
    }

    return 0;
}

void __stdcall glUniform1ui(GLint location, GLuint v0)
{
    glUniform1uiv(location, 1, &v0);
}

void __stdcall glUniform2ui(GLint location, GLuint v0, GLuint v1)
{
    const GLuint xy[] = { v0, v1 };
    glUniform2uiv(location, 1, xy);
}

void __stdcall glUniform3ui(GLint location, GLuint v0, GLuint v1, GLuint v2)
{
    const GLuint xyz[] = { v0, v1, v2 };
    glUniform3uiv(location, 1, xyz);
}

void __stdcall glUniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3)
{
    const GLuint xyzw[] = { v0, v1, v2, v3 };
    glUniform4uiv(location, 1, xyzw);
}

void __stdcall glUniform1uiv(GLint location, GLsizei count, const GLuint* value)
{
    EVENT("(GLint location = %d, GLsizei count = %d, const GLuint* value = 0x%0.8p)",
          location, count, value);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            gl::ProgramBinary *programBinary = context->getCurrentProgramBinary();
            if (!programBinary)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!programBinary->setUniform1uiv(location, count, value))
            {
                return gl::error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glUniform2uiv(GLint location, GLsizei count, const GLuint* value)
{
    EVENT("(GLint location = %d, GLsizei count = %d, const GLuint* value = 0x%0.8p)",
          location, count, value);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            gl::ProgramBinary *programBinary = context->getCurrentProgramBinary();
            if (!programBinary)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!programBinary->setUniform2uiv(location, count, value))
            {
                return gl::error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glUniform3uiv(GLint location, GLsizei count, const GLuint* value)
{
    EVENT("(GLint location = %d, GLsizei count = %d, const GLuint* value)",
          location, count, value);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            gl::ProgramBinary *programBinary = context->getCurrentProgramBinary();
            if (!programBinary)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!programBinary->setUniform3uiv(location, count, value))
            {
                return gl::error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glUniform4uiv(GLint location, GLsizei count, const GLuint* value)
{
    EVENT("(GLint location = %d, GLsizei count = %d, const GLuint* value = 0x%0.8p)",
          location, count, value);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            gl::ProgramBinary *programBinary = context->getCurrentProgramBinary();
            if (!programBinary)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!programBinary->setUniform4uiv(location, count, value))
            {
                return gl::error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glClearBufferiv(GLenum buffer, GLint drawbuffer, const GLint* value)
{
    EVENT("(GLenum buffer = 0x%X, GLint drawbuffer = %d, const GLint* value = 0x%0.8p)",
          buffer, drawbuffer, value);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            switch (buffer)
            {
              case GL_COLOR:
                if (drawbuffer < 0 || drawbuffer >= static_cast<GLint>(context->getMaximumRenderTargets()))
                {
                    return gl::error(GL_INVALID_VALUE);
                }
                break;
              case GL_STENCIL:
                if (drawbuffer != 0)
                {
                    return gl::error(GL_INVALID_VALUE);
                }
                break;
              default:
                return gl::error(GL_INVALID_ENUM);
            }

            context->clearBufferiv(buffer, drawbuffer, value);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glClearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint* value)
{
    EVENT("(GLenum buffer = 0x%X, GLint drawbuffer = %d, const GLuint* value = 0x%0.8p)",
          buffer, drawbuffer, value);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            switch (buffer)
            {
              case GL_COLOR:
                if (drawbuffer < 0 || drawbuffer >= static_cast<GLint>(context->getMaximumRenderTargets()))
                {
                    return gl::error(GL_INVALID_VALUE);
                }
                break;
              default:
                return gl::error(GL_INVALID_ENUM);
            }

            context->clearBufferuiv(buffer, drawbuffer, value);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat* value)
{
    EVENT("(GLenum buffer = 0x%X, GLint drawbuffer = %d, const GLfloat* value = 0x%0.8p)",
          buffer, drawbuffer, value);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            switch (buffer)
            {
              case GL_COLOR:
                if (drawbuffer < 0 || drawbuffer >= static_cast<GLint>(context->getMaximumRenderTargets()))
                {
                    return gl::error(GL_INVALID_VALUE);
                }
                break;
              case GL_DEPTH:
                if (drawbuffer != 0)
                {
                    return gl::error(GL_INVALID_VALUE);
                }
                break;
              default:
                return gl::error(GL_INVALID_ENUM);
            }

            context->clearBufferfv(buffer, drawbuffer, value);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil)
{
    EVENT("(GLenum buffer = 0x%X, GLint drawbuffer = %d, GLfloat depth, GLint stencil = %d)",
          buffer, drawbuffer, depth, stencil);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            switch (buffer)
            {
              case GL_DEPTH_STENCIL:
                if (drawbuffer != 0)
                {
                    return gl::error(GL_INVALID_VALUE);
                }
                break;
              default:
                return gl::error(GL_INVALID_ENUM);
            }

            context->clearBufferfi(buffer, drawbuffer, depth, stencil);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

const GLubyte* __stdcall glGetStringi(GLenum name, GLuint index)
{
    EVENT("(GLenum name = 0x%X, GLuint index = %u)", name, index);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION, reinterpret_cast<GLubyte*>(NULL));
            }

            if (name != GL_EXTENSIONS)
            {
                return gl::error(GL_INVALID_ENUM, reinterpret_cast<GLubyte*>(NULL));
            }

            if (index >= context->getNumExtensions())
            {
                return gl::error(GL_INVALID_VALUE, reinterpret_cast<GLubyte*>(NULL));
            }
            
            return reinterpret_cast<const GLubyte*>(context->getExtensionString(index));
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY, reinterpret_cast<GLubyte*>(NULL));
    }

    return NULL;
}

void __stdcall glCopyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size)
{
    EVENT("(GLenum readTarget = 0x%X, GLenum writeTarget = 0x%X, GLintptr readOffset = %d, GLintptr writeOffset = %d, GLsizeiptr size = %d)",
          readTarget, writeTarget, readOffset, writeOffset, size);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!gl::ValidBufferTarget(context, readTarget) || !gl::ValidBufferTarget(context, readTarget))
            {
                return gl::error(GL_INVALID_ENUM);
            }

            gl::Buffer *readBuffer = context->getTargetBuffer(readTarget);
            gl::Buffer *writeBuffer = context->getTargetBuffer(writeTarget);

            if (!readBuffer || !writeBuffer)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (readBuffer->mapped() || writeBuffer->mapped())
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (readOffset < 0 || writeOffset < 0 || size < 0 ||
                static_cast<unsigned int>(readOffset + size) > readBuffer->size() ||
                static_cast<unsigned int>(writeOffset + size) > writeBuffer->size())
            {
                return gl::error(GL_INVALID_VALUE);
            }

            if (readBuffer == writeBuffer && abs(readOffset - writeOffset) < size)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            // TODO: Verify that readBuffer and writeBuffer are not currently mapped (GL_INVALID_OPERATION)

            // if size is zero, the copy is a successful no-op
            if (size > 0)
            {
                writeBuffer->copyBufferSubData(readBuffer, readOffset, writeOffset, size);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetUniformIndices(GLuint program, GLsizei uniformCount, const GLchar* const* uniformNames, GLuint* uniformIndices)
{
    EVENT("(GLuint program = %u, GLsizei uniformCount = %d, const GLchar* const* uniformNames = 0x%0.8p, GLuint* uniformIndices = 0x%0.8p)",
          program, uniformCount, uniformNames, uniformIndices);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (uniformCount < 0)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            gl::Program *programObject = context->getProgram(program);

            if (!programObject)
            {
                if (context->getShader(program))
                {
                    return gl::error(GL_INVALID_OPERATION);
                }
                else
                {
                    return gl::error(GL_INVALID_VALUE);
                }
            }

            gl::ProgramBinary *programBinary = programObject->getProgramBinary();
            if (!programObject->isLinked() || !programBinary)
            {
                for (int uniformId = 0; uniformId < uniformCount; uniformId++)
                {
                    uniformIndices[uniformId] = GL_INVALID_INDEX;
                }
            }
            else
            {
                for (int uniformId = 0; uniformId < uniformCount; uniformId++)
                {
                    uniformIndices[uniformId] = programBinary->getUniformIndex(uniformNames[uniformId]);
                }
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetActiveUniformsiv(GLuint program, GLsizei uniformCount, const GLuint* uniformIndices, GLenum pname, GLint* params)
{
    EVENT("(GLuint program = %u, GLsizei uniformCount = %d, const GLuint* uniformIndices = 0x%0.8p, GLenum pname = 0x%X, GLint* params = 0x%0.8p)",
          program, uniformCount, uniformIndices, pname, params);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (uniformCount < 0)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            gl::Program *programObject = context->getProgram(program);

            if (!programObject)
            {
                if (context->getShader(program))
                {
                    return gl::error(GL_INVALID_OPERATION);
                }
                else
                {
                    return gl::error(GL_INVALID_VALUE);
                }
            }

            switch (pname)
            {
              case GL_UNIFORM_TYPE:
              case GL_UNIFORM_SIZE:
              case GL_UNIFORM_NAME_LENGTH:
              case GL_UNIFORM_BLOCK_INDEX:
              case GL_UNIFORM_OFFSET:
              case GL_UNIFORM_ARRAY_STRIDE:
              case GL_UNIFORM_MATRIX_STRIDE:
              case GL_UNIFORM_IS_ROW_MAJOR:
                break;
              default:
                return gl::error(GL_INVALID_ENUM);
            }

            gl::ProgramBinary *programBinary = programObject->getProgramBinary();

            if (!programBinary && uniformCount > 0)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            for (int uniformId = 0; uniformId < uniformCount; uniformId++)
            {
                const GLuint index = uniformIndices[uniformId];

                if (index >= (GLuint)programBinary->getActiveUniformCount())
                {
                    return gl::error(GL_INVALID_VALUE);
                }
            }

            for (int uniformId = 0; uniformId < uniformCount; uniformId++)
            {
                const GLuint index = uniformIndices[uniformId];
                params[uniformId] = programBinary->getActiveUniformi(index, pname);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

GLuint __stdcall glGetUniformBlockIndex(GLuint program, const GLchar* uniformBlockName)
{
    EVENT("(GLuint program = %u, const GLchar* uniformBlockName = 0x%0.8p)", program, uniformBlockName);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION, GL_INVALID_INDEX);
            }

            gl::Program *programObject = context->getProgram(program);

            if (!programObject)
            {
                if (context->getShader(program))
                {
                    return gl::error(GL_INVALID_OPERATION, GL_INVALID_INDEX);
                }
                else
                {
                    return gl::error(GL_INVALID_VALUE, GL_INVALID_INDEX);
                }
            }

            gl::ProgramBinary *programBinary = programObject->getProgramBinary();
            if (!programBinary)
            {
                return GL_INVALID_INDEX;
            }

            return programBinary->getUniformBlockIndex(uniformBlockName);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY, 0);
    }

    return 0;
}

void __stdcall glGetActiveUniformBlockiv(GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint* params)
{
    EVENT("(GLuint program = %u, GLuint uniformBlockIndex = %u, GLenum pname = 0x%X, GLint* params = 0x%0.8p)",
          program, uniformBlockIndex, pname, params);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }
            gl::Program *programObject = context->getProgram(program);

            if (!programObject)
            {
                if (context->getShader(program))
                {
                    return gl::error(GL_INVALID_OPERATION);
                }
                else
                {
                    return gl::error(GL_INVALID_VALUE);
                }
            }

            gl::ProgramBinary *programBinary = programObject->getProgramBinary();

            if (!programBinary || uniformBlockIndex >= programBinary->getActiveUniformBlockCount())
            {
                return gl::error(GL_INVALID_VALUE);
            }

            switch (pname)
            {
              case GL_UNIFORM_BLOCK_BINDING:
                *params = static_cast<GLint>(programObject->getUniformBlockBinding(uniformBlockIndex));
                break;

              case GL_UNIFORM_BLOCK_DATA_SIZE:
              case GL_UNIFORM_BLOCK_NAME_LENGTH:
              case GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS:
              case GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES:
              case GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER:
              case GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER:
                programBinary->getActiveUniformBlockiv(uniformBlockIndex, pname, params);
                break;

              default:
                return gl::error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetActiveUniformBlockName(GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei* length, GLchar* uniformBlockName)
{
    EVENT("(GLuint program = %u, GLuint uniformBlockIndex = %u, GLsizei bufSize = %d, GLsizei* length = 0x%0.8p, GLchar* uniformBlockName = 0x%0.8p)",
          program, uniformBlockIndex, bufSize, length, uniformBlockName);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            gl::Program *programObject = context->getProgram(program);

            if (!programObject)
            {
                if (context->getShader(program))
                {
                    return gl::error(GL_INVALID_OPERATION);
                }
                else
                {
                    return gl::error(GL_INVALID_VALUE);
                }
            }

            gl::ProgramBinary *programBinary = programObject->getProgramBinary();

            if (!programBinary || uniformBlockIndex >= programBinary->getActiveUniformBlockCount())
            {
                return gl::error(GL_INVALID_VALUE);
            }

            programBinary->getActiveUniformBlockName(uniformBlockIndex, bufSize, length, uniformBlockName);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glUniformBlockBinding(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding)
{
    EVENT("(GLuint program = %u, GLuint uniformBlockIndex = %u, GLuint uniformBlockBinding = %u)",
          program, uniformBlockIndex, uniformBlockBinding);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (uniformBlockBinding >= context->getMaximumCombinedUniformBufferBindings())
            {
                return gl::error(GL_INVALID_VALUE);
            }

            gl::Program *programObject = context->getProgram(program);

            if (!programObject)
            {
                if (context->getShader(program))
                {
                    return gl::error(GL_INVALID_OPERATION);
                }
                else
                {
                    return gl::error(GL_INVALID_VALUE);
                }
            }

            gl::ProgramBinary *programBinary = programObject->getProgramBinary();

            // if never linked, there won't be any uniform blocks
            if (!programBinary || uniformBlockIndex >= programBinary->getActiveUniformBlockCount())
            {
                return gl::error(GL_INVALID_VALUE);
            }

            programObject->bindUniformBlock(uniformBlockIndex, uniformBlockBinding);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instanceCount)
{
    EVENT("(GLenum mode = 0x%X, GLint first = %d, GLsizei count = %d, GLsizei instanceCount = %d)",
          mode, first, count, instanceCount);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            // glDrawArraysInstanced
            UNIMPLEMENTED();
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices, GLsizei instanceCount)
{
    EVENT("(GLenum mode = 0x%X, GLsizei count = %d, GLenum type = 0x%X, const GLvoid* indices = 0x%0.8p, GLsizei instanceCount = %d)",
          mode, count, type, indices, instanceCount);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            // glDrawElementsInstanced
            UNIMPLEMENTED();
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

GLsync __stdcall glFenceSync(GLenum condition, GLbitfield flags)
{
    EVENT("(GLenum condition = 0x%X, GLbitfield flags = 0x%X)", condition, flags);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION, reinterpret_cast<GLsync>(0));
            }

            if (condition != GL_SYNC_GPU_COMMANDS_COMPLETE)
            {
                return gl::error(GL_INVALID_ENUM, reinterpret_cast<GLsync>(0));
            }

            if (flags != 0)
            {
                return gl::error(GL_INVALID_VALUE, reinterpret_cast<GLsync>(0));
            }

            return context->createFenceSync(condition);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY, reinterpret_cast<GLsync>(NULL));
    }

    return NULL;
}

GLboolean __stdcall glIsSync(GLsync sync)
{
    EVENT("(GLsync sync = 0x%0.8p)", sync);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION, GL_FALSE);
            }

            return (context->getFenceSync(sync) != NULL);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY, GL_FALSE);
    }

    return GL_FALSE;
}

void __stdcall glDeleteSync(GLsync sync)
{
    EVENT("(GLsync sync = 0x%0.8p)", sync);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (sync != static_cast<GLsync>(0) && !context->getFenceSync(sync))
            {
                return gl::error(GL_INVALID_VALUE);
            }

            context->deleteFenceSync(sync);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

GLenum __stdcall glClientWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout)
{
    EVENT("(GLsync sync = 0x%0.8p, GLbitfield flags = 0x%X, GLuint64 timeout = %llu)",
          sync, flags, timeout);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION, GL_WAIT_FAILED);
            }

            if ((flags & ~(GL_SYNC_FLUSH_COMMANDS_BIT)) != 0)
            {
                return gl::error(GL_INVALID_VALUE, GL_WAIT_FAILED);
            }

            gl::FenceSync *fenceSync = context->getFenceSync(sync);

            if (!fenceSync)
            {
                return gl::error(GL_INVALID_VALUE, GL_WAIT_FAILED);
            }

            return fenceSync->clientWait(flags, timeout);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY, GL_FALSE);
    }

    return GL_FALSE;
}

void __stdcall glWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout)
{
    EVENT("(GLsync sync = 0x%0.8p, GLbitfield flags = 0x%X, GLuint64 timeout = %llu)",
          sync, flags, timeout);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (flags != 0)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            if (timeout != GL_TIMEOUT_IGNORED)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            gl::FenceSync *fenceSync = context->getFenceSync(sync);

            if (!fenceSync)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            fenceSync->serverWait();
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetInteger64v(GLenum pname, GLint64* params)
{
    EVENT("(GLenum pname = 0x%X, GLint64* params = 0x%0.8p)",
          pname, params);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            GLenum nativeType;
            unsigned int numParams = 0;
            if (!context->getQueryParameterInfo(pname, &nativeType, &numParams))
                return gl::error(GL_INVALID_ENUM);

            // pname is valid, but that there are no parameters to return.
            if (numParams == 0)
                return;

            if (nativeType == GL_INT_64_ANGLEX)
            {
                context->getInteger64v(pname, params);
            }
            else
            {
                CastStateValues(context, nativeType, pname, numParams, params);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetSynciv(GLsync sync, GLenum pname, GLsizei bufSize, GLsizei* length, GLint* values)
{
    EVENT("(GLsync sync = 0x%0.8p, GLenum pname = 0x%X, GLsizei bufSize = %d, GLsizei* length = 0x%0.8p, GLint* values = 0x%0.8p)",
          sync, pname, bufSize, length, values);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (bufSize < 0)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            gl::FenceSync *fenceSync = context->getFenceSync(sync);

            if (!fenceSync)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            switch (pname)
            {
              case GL_OBJECT_TYPE:     values[0] = static_cast<GLint>(GL_SYNC_FENCE);              break;
              case GL_SYNC_STATUS:     values[0] = static_cast<GLint>(fenceSync->getStatus());     break;
              case GL_SYNC_CONDITION:  values[0] = static_cast<GLint>(fenceSync->getCondition());  break;
              case GL_SYNC_FLAGS:      values[0] = 0;                                              break;

              default:
                return gl::error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetInteger64i_v(GLenum target, GLuint index, GLint64* data)
{
    EVENT("(GLenum target = 0x%X, GLuint index = %u, GLint64* data = 0x%0.8p)",
          target, index, data);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            switch (target)
            {
              case GL_TRANSFORM_FEEDBACK_BUFFER_START:
              case GL_TRANSFORM_FEEDBACK_BUFFER_SIZE:
              case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
                if (index >= context->getMaxTransformFeedbackBufferBindings())
                    return gl::error(GL_INVALID_VALUE);
                break;
              case GL_UNIFORM_BUFFER_START:
              case GL_UNIFORM_BUFFER_SIZE:
              case GL_UNIFORM_BUFFER_BINDING:
                if (index >= context->getMaximumCombinedUniformBufferBindings())
                    return gl::error(GL_INVALID_VALUE);
                break;
              default:
                return gl::error(GL_INVALID_ENUM);
            }

            if (!(context->getIndexedInteger64v(target, index, data)))
            {
                GLenum nativeType;
                unsigned int numParams = 0;
                if (!context->getIndexedQueryParameterInfo(target, &nativeType, &numParams))
                    return gl::error(GL_INVALID_ENUM);

                if (numParams == 0)
                    return; // it is known that pname is valid, but there are no parameters to return

                if (nativeType == GL_INT)
                {
                    GLint *intParams = new GLint[numParams];

                    context->getIndexedIntegerv(target, index, intParams);

                    for (unsigned int i = 0; i < numParams; ++i)
                    {
                        data[i] = static_cast<GLint64>(intParams[i]);
                    }

                    delete [] intParams;
                }
                else
                {
                    UNREACHABLE();
                }
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetBufferParameteri64v(GLenum target, GLenum pname, GLint64* params)
{
    EVENT("(GLenum target = 0x%X, GLenum pname = 0x%X, GLint64* params = 0x%0.8p)",
          target, pname, params);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!gl::ValidBufferTarget(context, target))
            {
                return gl::error(GL_INVALID_ENUM);
            }

            if (!gl::ValidBufferParameter(context, pname))
            {
                return gl::error(GL_INVALID_ENUM);
            }

            gl::Buffer *buffer = context->getTargetBuffer(target);

            if (!buffer)
            {
                // A null buffer means that "0" is bound to the requested buffer target
                return gl::error(GL_INVALID_OPERATION);
            }

            switch (pname)
            {
              case GL_BUFFER_USAGE:
                *params = static_cast<GLint64>(buffer->usage());
                break;
              case GL_BUFFER_SIZE:
                *params = buffer->size();
                break;
              case GL_BUFFER_ACCESS_FLAGS:
                *params = static_cast<GLint64>(buffer->accessFlags());
                break;
              case GL_BUFFER_MAPPED:
                *params = static_cast<GLint64>(buffer->mapped());
                break;
              case GL_BUFFER_MAP_OFFSET:
                *params = buffer->mapOffset();
                break;
              case GL_BUFFER_MAP_LENGTH:
                *params = buffer->mapLength();
                break;
              default: UNREACHABLE(); break;
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGenSamplers(GLsizei count, GLuint* samplers)
{
    EVENT("(GLsizei count = %d, GLuint* samplers = 0x%0.8p)", count, samplers);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (count < 0)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            for (int i = 0; i < count; i++)
            {
                samplers[i] = context->createSampler();
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDeleteSamplers(GLsizei count, const GLuint* samplers)
{
    EVENT("(GLsizei count = %d, const GLuint* samplers = 0x%0.8p)", count, samplers);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (count < 0)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            for (int i = 0; i < count; i++)
            {
                context->deleteSampler(samplers[i]);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

GLboolean __stdcall glIsSampler(GLuint sampler)
{
    EVENT("(GLuint sampler = %u)", sampler);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION, GL_FALSE);
            }

            return context->isSampler(sampler);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY, GL_FALSE);
    }

    return GL_FALSE;
}

void __stdcall glBindSampler(GLuint unit, GLuint sampler)
{
    EVENT("(GLuint unit = %u, GLuint sampler = %u)", unit, sampler);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (sampler != 0 && !context->isSampler(sampler))
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (unit >= context->getMaximumCombinedTextureImageUnits())
            {
                return gl::error(GL_INVALID_VALUE);
            }

            context->bindSampler(unit, sampler);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glSamplerParameteri(GLuint sampler, GLenum pname, GLint param)
{
    EVENT("(GLuint sampler = %u, GLenum pname = 0x%X, GLint param = %d)", sampler, pname, param);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!gl::ValidateSamplerObjectParameter(pname))
            {
                return;
            }

            if (!gl::ValidateTexParamParameters(context, pname, param))
            {
                return;
            }

            if (!context->isSampler(sampler))
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            context->samplerParameteri(sampler, pname, param);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glSamplerParameteriv(GLuint sampler, GLenum pname, const GLint* param)
{
    glSamplerParameteri(sampler, pname, *param);
}

void __stdcall glSamplerParameterf(GLuint sampler, GLenum pname, GLfloat param)
{
    EVENT("(GLuint sampler = %u, GLenum pname = 0x%X, GLfloat param = %g)", sampler, pname, param);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!gl::ValidateSamplerObjectParameter(pname))
            {
                return;
            }

            if (!gl::ValidateTexParamParameters(context, pname, static_cast<GLint>(param)))
            {
                return;
            }

            if (!context->isSampler(sampler))
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            context->samplerParameterf(sampler, pname, param);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glSamplerParameterfv(GLuint sampler, GLenum pname, const GLfloat* param)
{
    glSamplerParameterf(sampler, pname, *param);
}

void __stdcall glGetSamplerParameteriv(GLuint sampler, GLenum pname, GLint* params)
{
    EVENT("(GLuint sampler = %u, GLenum pname = 0x%X, GLint* params = 0x%0.8p)", sampler, pname, params);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!gl::ValidateSamplerObjectParameter(pname))
            {
                return;
            }

            if (!context->isSampler(sampler))
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            *params = context->getSamplerParameteri(sampler, pname);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetSamplerParameterfv(GLuint sampler, GLenum pname, GLfloat* params)
{
    EVENT("(GLuint sample = %ur, GLenum pname = 0x%X, GLfloat* params = 0x%0.8p)", sampler, pname, params);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!gl::ValidateSamplerObjectParameter(pname))
            {
                return;
            }

            if (!context->isSampler(sampler))
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            *params = context->getSamplerParameterf(sampler, pname);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glVertexAttribDivisor(GLuint index, GLuint divisor)
{
    EVENT("(GLuint index = %u, GLuint divisor = %u)", index, divisor);

    try
    {
        if (index >= gl::MAX_VERTEX_ATTRIBS)
        {
            return gl::error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            context->setVertexAttribDivisor(index, divisor);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glBindTransformFeedback(GLenum target, GLuint id)
{
    EVENT("(GLenum target = 0x%X, GLuint id = %u)", target, id);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            switch (target)
            {
              case GL_TRANSFORM_FEEDBACK:
                {
                    // Cannot bind a transform feedback object if the current one is started and not paused (3.0.2 pg 85 section 2.14.1)
                    gl::TransformFeedback *curTransformFeedback = context->getCurrentTransformFeedback();
                    if (curTransformFeedback && curTransformFeedback->isStarted() && !curTransformFeedback->isPaused())
                    {
                        return gl::error(GL_INVALID_OPERATION);
                    }

                    // Cannot bind a transform feedback object that does not exist (3.0.2 pg 85 section 2.14.1)
                    if (context->getTransformFeedback(id) == NULL)
                    {
                        return gl::error(GL_INVALID_OPERATION);
                    }

                    context->bindTransformFeedback(id);
                }
                break;

              default:
                return gl::error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDeleteTransformFeedbacks(GLsizei n, const GLuint* ids)
{
    EVENT("(GLsizei n = %d, const GLuint* ids = 0x%0.8p)", n, ids);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            for (int i = 0; i < n; i++)
            {
                context->deleteTransformFeedback(ids[i]);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGenTransformFeedbacks(GLsizei n, GLuint* ids)
{
    EVENT("(GLsizei n = %d, GLuint* ids = 0x%0.8p)", n, ids);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            for (int i = 0; i < n; i++)
            {
                ids[i] = context->createTransformFeedback();
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

GLboolean __stdcall glIsTransformFeedback(GLuint id)
{
    EVENT("(GLuint id = %u)", id);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION, GL_FALSE);
            }

            return ((context->getTransformFeedback(id) != NULL) ? GL_TRUE : GL_FALSE);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY, GL_FALSE);
    }

    return GL_FALSE;
}

void __stdcall glPauseTransformFeedback(void)
{
    EVENT("(void)");

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            gl::TransformFeedback *transformFeedback = context->getCurrentTransformFeedback();
            ASSERT(transformFeedback != NULL);

            // Current transform feedback must be started and not paused in order to pause (3.0.2 pg 86)
            if (!transformFeedback->isStarted() || transformFeedback->isPaused())
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            transformFeedback->pause();
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glResumeTransformFeedback(void)
{
    EVENT("(void)");

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            gl::TransformFeedback *transformFeedback = context->getCurrentTransformFeedback();
            ASSERT(transformFeedback != NULL);

            // Current transform feedback must be started and paused in order to resume (3.0.2 pg 86)
            if (!transformFeedback->isStarted() || !transformFeedback->isPaused())
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            transformFeedback->resume();
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetProgramBinary(GLuint program, GLsizei bufSize, GLsizei* length, GLenum* binaryFormat, GLvoid* binary)
{
    EVENT("(GLuint program = %u, GLsizei bufSize = %d, GLsizei* length = 0x%0.8p, GLenum* binaryFormat = 0x%0.8p, GLvoid* binary = 0x%0.8p)",
          program, bufSize, length, binaryFormat, binary);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            // glGetProgramBinary
            UNIMPLEMENTED();
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glProgramBinary(GLuint program, GLenum binaryFormat, const GLvoid* binary, GLsizei length)
{
    EVENT("(GLuint program = %u, GLenum binaryFormat = 0x%X, const GLvoid* binary = 0x%0.8p, GLsizei length = %d)",
          program, binaryFormat, binary, length);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            // glProgramBinary
            UNIMPLEMENTED();
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glProgramParameteri(GLuint program, GLenum pname, GLint value)
{
    EVENT("(GLuint program = %u, GLenum pname = 0x%X, GLint value = %d)",
          program, pname, value);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            // glProgramParameteri
            UNIMPLEMENTED();
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glInvalidateFramebuffer(GLenum target, GLsizei numAttachments, const GLenum* attachments)
{
    EVENT("(GLenum target = 0x%X, GLsizei numAttachments = %d, const GLenum* attachments = 0x%0.8p)",
          target, numAttachments, attachments);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!ValidateInvalidateFramebufferParameters(context, target, numAttachments, attachments))
            {
                return;
            }

            int maxDimension = context->getMaximumRenderbufferDimension();
            context->invalidateFrameBuffer(target, numAttachments, attachments, 0, 0, maxDimension, maxDimension);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glInvalidateSubFramebuffer(GLenum target, GLsizei numAttachments, const GLenum* attachments, GLint x, GLint y, GLsizei width, GLsizei height)
{
    EVENT("(GLenum target = 0x%X, GLsizei numAttachments = %d, const GLenum* attachments = 0x%0.8p, GLint x = %d, "
          "GLint y = %d, GLsizei width = %d, GLsizei height = %d)",
          target, numAttachments, attachments, x, y, width, height);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!ValidateInvalidateFramebufferParameters(context, target, numAttachments, attachments))
            {
                return;
            }

            context->invalidateFrameBuffer(target, numAttachments, attachments, x, y, width, height);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)
{
    EVENT("(GLenum target = 0x%X, GLsizei levels = %d, GLenum internalformat = 0x%X, GLsizei width = %d, GLsizei height = %d)",
          target, levels, internalformat, width, height);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!ValidateES3TexStorageParameters(context, target, levels, internalformat, width, height, 1))
            {
                return;
            }

            switch (target)
            {
              case GL_TEXTURE_2D:
                {
                    gl::Texture2D *texture2d = context->getTexture2D();
                    texture2d->storage(levels, internalformat, width, height);
                }
                break;

              case GL_TEXTURE_CUBE_MAP:
                {
                    gl::TextureCubeMap *textureCube = context->getTextureCubeMap();
                    textureCube->storage(levels, internalformat, width);
                }
                break;

              default:
                return gl::error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glTexStorage3D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)
{
    EVENT("(GLenum target = 0x%X, GLsizei levels = %d, GLenum internalformat = 0x%X, GLsizei width = %d, "
          "GLsizei height = %d, GLsizei depth = %d)",
          target, levels, internalformat, width, height, depth);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!ValidateES3TexStorageParameters(context, target, levels, internalformat, width, height, depth))
            {
                return;
            }

            switch (target)
            {
              case GL_TEXTURE_3D:
                {
                    gl::Texture3D *texture3d = context->getTexture3D();
                    texture3d->storage(levels, internalformat, width, height, depth);
                }
                break;

              case GL_TEXTURE_2D_ARRAY:
                {
                    gl::Texture2DArray *texture2darray = context->getTexture2DArray();
                    texture2darray->storage(levels, internalformat, width, height, depth);
                }
                break;

              default:
                UNREACHABLE();
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetInternalformativ(GLenum target, GLenum internalformat, GLenum pname, GLsizei bufSize, GLint* params)
{
    EVENT("(GLenum target = 0x%X, GLenum internalformat = 0x%X, GLenum pname = 0x%X, GLsizei bufSize = %d, "
          "GLint* params = 0x%0.8p)",
          target, internalformat, pname, bufSize, params);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (context->getClientVersion() < 3)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!gl::IsColorRenderingSupported(internalformat, context) &&
                !gl::IsDepthRenderingSupported(internalformat, context) &&
                !gl::IsStencilRenderingSupported(internalformat, context))
            {
                return gl::error(GL_INVALID_ENUM);
            }

            if (target != GL_RENDERBUFFER)
            {
                return gl::error(GL_INVALID_ENUM);
            }

            if (bufSize < 0)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            switch (pname)
            {
              case GL_NUM_SAMPLE_COUNTS:
                if (bufSize != 0)
                    *params = context->getNumSampleCounts(internalformat);
                break;
              case GL_SAMPLES:
                context->getSampleCounts(internalformat, bufSize, params);
                break;
              default:
                return gl::error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

// Extension functions

void __stdcall glBlitFramebufferANGLE(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                                      GLbitfield mask, GLenum filter)
{
    EVENT("(GLint srcX0 = %d, GLint srcY0 = %d, GLint srcX1 = %d, GLint srcY1 = %d, "
          "GLint dstX0 = %d, GLint dstY0 = %d, GLint dstX1 = %d, GLint dstY1 = %d, "
          "GLbitfield mask = 0x%X, GLenum filter = 0x%X)",
          srcX0, srcY0, srcX1, srcX1, dstX0, dstY0, dstX1, dstY1, mask, filter);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (!ValidateBlitFramebufferParameters(context, srcX0, srcY0, srcX1, srcY1,
                                                   dstX0, dstY0, dstX1, dstY1, mask, filter,
                                                   true))
            {
                return;
            }

            context->blitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1,
                                     mask, filter);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glTexImage3DOES(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth,
                               GLint border, GLenum format, GLenum type, const GLvoid* pixels)
{
    EVENT("(GLenum target = 0x%X, GLint level = %d, GLenum internalformat = 0x%X, "
          "GLsizei width = %d, GLsizei height = %d, GLsizei depth = %d, GLint border = %d, "
          "GLenum format = 0x%X, GLenum type = 0x%x, const GLvoid* pixels = 0x%0.8p)",
          target, level, internalformat, width, height, depth, border, format, type, pixels);

    try
    {
        UNIMPLEMENTED();   // FIXME
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetProgramBinaryOES(GLuint program, GLsizei bufSize, GLsizei *length, 
                                     GLenum *binaryFormat, void *binary)
{
    EVENT("(GLenum program = 0x%X, bufSize = %d, length = 0x%0.8p, binaryFormat = 0x%0.8p, binary = 0x%0.8p)",
          program, bufSize, length, binaryFormat, binary);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            gl::Program *programObject = context->getProgram(program);

            if (!programObject || !programObject->isLinked())
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            gl::ProgramBinary *programBinary = programObject->getProgramBinary();

            if (!programBinary)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!programBinary->save(binary, bufSize, length))
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            *binaryFormat = GL_PROGRAM_BINARY_ANGLE;
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glProgramBinaryOES(GLuint program, GLenum binaryFormat,
                                  const void *binary, GLint length)
{
    EVENT("(GLenum program = 0x%X, binaryFormat = 0x%x, binary = 0x%0.8p, length = %d)",
          program, binaryFormat, binary, length);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (binaryFormat != GL_PROGRAM_BINARY_ANGLE)
            {
                return gl::error(GL_INVALID_ENUM);
            }

            gl::Program *programObject = context->getProgram(program);

            if (!programObject)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            context->setProgramBinary(program, binary, length);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDrawBuffersEXT(GLsizei n, const GLenum *bufs)
{
    EVENT("(GLenum n = %d, bufs = 0x%0.8p)", n, bufs);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (n < 0 || (unsigned int)n > context->getMaximumRenderTargets())
            {
                return gl::error(GL_INVALID_VALUE);
            }

            if (context->getDrawFramebufferHandle() == 0)
            {
                if (n != 1)
                {
                    return gl::error(GL_INVALID_OPERATION);
                }

                if (bufs[0] != GL_NONE && bufs[0] != GL_BACK)
                {
                    return gl::error(GL_INVALID_OPERATION);
                }
            }
            else
            {
                for (int colorAttachment = 0; colorAttachment < n; colorAttachment++)
                {
                    const GLenum attachment = GL_COLOR_ATTACHMENT0_EXT + colorAttachment;
                    if (bufs[colorAttachment] != GL_NONE && bufs[colorAttachment] != attachment)
                    {
                        return gl::error(GL_INVALID_OPERATION);
                    }
                }
            }

            gl::Framebuffer *framebuffer = context->getDrawFramebuffer();

            for (int colorAttachment = 0; colorAttachment < n; colorAttachment++)
            {
                framebuffer->setDrawBufferState(colorAttachment, bufs[colorAttachment]);
            }

            for (int colorAttachment = n; colorAttachment < (int)context->getMaximumRenderTargets(); colorAttachment++)
            {
                framebuffer->setDrawBufferState(colorAttachment, GL_NONE);
            }
        }
    }
    catch (std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetBufferPointervOES(GLenum target, GLenum pname, void** params)
{
    EVENT("(GLenum target = 0x%X, GLenum pname = 0x%X, GLvoid** params = 0x%0.8p)", target, pname, params);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (!context->supportsPBOs())
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!gl::ValidBufferTarget(context, target))
            {
                return gl::error(GL_INVALID_ENUM);
            }

            if (pname != GL_BUFFER_MAP_POINTER)
            {
                return gl::error(GL_INVALID_ENUM);
            }

            gl::Buffer *buffer = context->getTargetBuffer(target);

            if (!buffer || !buffer->mapped())
            {
                *params = NULL;
            }

            *params = buffer->mapPointer();
        }
    }
    catch (std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

void * __stdcall glMapBufferOES(GLenum target, GLenum access)
{
    EVENT("(GLenum target = 0x%X, GLbitfield access = 0x%X)", target, access);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (!gl::ValidBufferTarget(context, target))
            {
                return gl::error(GL_INVALID_ENUM, reinterpret_cast<GLvoid*>(NULL));
            }

            gl::Buffer *buffer = context->getTargetBuffer(target);

            if (buffer == NULL)
            {
                return gl::error(GL_INVALID_OPERATION, reinterpret_cast<GLvoid*>(NULL));
            }

            if (access != GL_WRITE_ONLY_OES)
            {
                return gl::error(GL_INVALID_ENUM, reinterpret_cast<GLvoid*>(NULL));
            }

            if (buffer->mapped())
            {
                return gl::error(GL_INVALID_OPERATION, reinterpret_cast<GLvoid*>(NULL));
            }

            return buffer->mapRange(0, buffer->size(), GL_MAP_WRITE_BIT);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY, reinterpret_cast<GLvoid*>(NULL));
    }

    return NULL;
}

GLboolean __stdcall glUnmapBufferOES(GLenum target)
{
    EVENT("(GLenum target = 0x%X)", target);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (!gl::ValidBufferTarget(context, target))
            {
                return gl::error(GL_INVALID_ENUM, GL_FALSE);
            }

            gl::Buffer *buffer = context->getTargetBuffer(target);

            if (buffer == NULL || !buffer->mapped())
            {
                return gl::error(GL_INVALID_OPERATION, GL_FALSE);
            }

            // TODO: detect if we had corruption. if so, throw an error and return false.

            buffer->unmap();

            return GL_TRUE;
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY, GL_FALSE);
    }

    return GL_FALSE;
}

void* __stdcall glMapBufferRangeEXT (GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access)
{
    EVENT("(GLenum target = 0x%X, GLintptr offset = %d, GLsizeiptr length = %d, GLbitfield access = 0x%X)",
          target, offset, length, access);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (!gl::ValidBufferTarget(context, target))
            {
                return gl::error(GL_INVALID_ENUM, reinterpret_cast<GLvoid*>(NULL));
            }

            if (offset < 0 || length < 0)
            {
                return gl::error(GL_INVALID_VALUE, reinterpret_cast<GLvoid*>(NULL));
            }

            gl::Buffer *buffer = context->getTargetBuffer(target);

            if (buffer == NULL)
            {
                return gl::error(GL_INVALID_OPERATION, reinterpret_cast<GLvoid*>(NULL));
            }

            // Check for buffer overflow
            size_t offsetSize = static_cast<size_t>(offset);
            size_t lengthSize = static_cast<size_t>(length);

            if (!rx::IsUnsignedAdditionSafe(offsetSize, lengthSize) ||
                offsetSize + lengthSize > static_cast<size_t>(buffer->size()))
            {
                return gl::error(GL_INVALID_VALUE, reinterpret_cast<GLvoid*>(NULL));
            }

            // Check for invalid bits in the mask
            GLbitfield allAccessBits = GL_MAP_READ_BIT |
                                       GL_MAP_WRITE_BIT |
                                       GL_MAP_INVALIDATE_RANGE_BIT |
                                       GL_MAP_INVALIDATE_BUFFER_BIT |
                                       GL_MAP_FLUSH_EXPLICIT_BIT |
                                       GL_MAP_UNSYNCHRONIZED_BIT;

            if (access & ~(allAccessBits))
            {
                return gl::error(GL_INVALID_VALUE, reinterpret_cast<GLvoid*>(NULL));
            }

            if (length == 0 || buffer->mapped())
            {
                return gl::error(GL_INVALID_OPERATION, reinterpret_cast<GLvoid*>(NULL));
            }

            // Check for invalid bit combinations
            if ((access & (GL_MAP_READ_BIT | GL_MAP_WRITE_BIT)) == 0)
            {
                return gl::error(GL_INVALID_OPERATION, reinterpret_cast<GLvoid*>(NULL));
            }

            GLbitfield writeOnlyBits = GL_MAP_INVALIDATE_RANGE_BIT |
                                       GL_MAP_INVALIDATE_BUFFER_BIT |
                                       GL_MAP_UNSYNCHRONIZED_BIT;

            if ((access & GL_MAP_READ_BIT) != 0 && (access & writeOnlyBits) != 0)
            {
                return gl::error(GL_INVALID_OPERATION, reinterpret_cast<GLvoid*>(NULL));
            }

            if ((access & GL_MAP_WRITE_BIT) == 0 && (access & GL_MAP_FLUSH_EXPLICIT_BIT) != 0)
            {
                return gl::error(GL_INVALID_OPERATION, reinterpret_cast<GLvoid*>(NULL));
            }

            return buffer->mapRange(offset, length, access);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY, reinterpret_cast<GLvoid*>(NULL));
    }

    return NULL;
}

void __stdcall glFlushMappedBufferRangeEXT (GLenum target, GLintptr offset, GLsizeiptr length)
{
    EVENT("(GLenum target = 0x%X, GLintptr offset = %d, GLsizeiptr length = %d)", target, offset, length);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            if (offset < 0 || length < 0)
            {
                return gl::error(GL_INVALID_VALUE);
            }

            if (!gl::ValidBufferTarget(context, target))
            {
                return gl::error(GL_INVALID_ENUM);
            }

            gl::Buffer *buffer = context->getTargetBuffer(target);

            if (buffer == NULL)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            if (!buffer->mapped() || (buffer->accessFlags() & GL_MAP_FLUSH_EXPLICIT_BIT) == 0)
            {
                return gl::error(GL_INVALID_OPERATION);
            }

            // Check for buffer overflow
            size_t offsetSize = static_cast<size_t>(offset);
            size_t lengthSize = static_cast<size_t>(length);

            if (!rx::IsUnsignedAdditionSafe(offsetSize, lengthSize) ||
                offsetSize + lengthSize > static_cast<size_t>(buffer->mapLength()))
            {
                return gl::error(GL_INVALID_VALUE);
            }

            // We do not currently support a non-trivial implementation of FlushMappedBufferRange
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY);
    }
}

__eglMustCastToProperFunctionPointerType __stdcall glGetProcAddress(const char *procname)
{
    struct Extension
    {
        const char *name;
        __eglMustCastToProperFunctionPointerType address;
    };

    static const Extension glExtensions[] =
    {
        {"glTexImage3DOES", (__eglMustCastToProperFunctionPointerType)glTexImage3DOES},
        {"glBlitFramebufferANGLE", (__eglMustCastToProperFunctionPointerType)glBlitFramebufferANGLE},
        {"glRenderbufferStorageMultisampleANGLE", (__eglMustCastToProperFunctionPointerType)glRenderbufferStorageMultisampleANGLE},
        {"glDeleteFencesNV", (__eglMustCastToProperFunctionPointerType)glDeleteFencesNV},
        {"glGenFencesNV", (__eglMustCastToProperFunctionPointerType)glGenFencesNV},
        {"glIsFenceNV", (__eglMustCastToProperFunctionPointerType)glIsFenceNV},
        {"glTestFenceNV", (__eglMustCastToProperFunctionPointerType)glTestFenceNV},
        {"glGetFenceivNV", (__eglMustCastToProperFunctionPointerType)glGetFenceivNV},
        {"glFinishFenceNV", (__eglMustCastToProperFunctionPointerType)glFinishFenceNV},
        {"glSetFenceNV", (__eglMustCastToProperFunctionPointerType)glSetFenceNV},
        {"glGetTranslatedShaderSourceANGLE", (__eglMustCastToProperFunctionPointerType)glGetTranslatedShaderSourceANGLE},
        {"glTexStorage2DEXT", (__eglMustCastToProperFunctionPointerType)glTexStorage2DEXT},
        {"glGetGraphicsResetStatusEXT", (__eglMustCastToProperFunctionPointerType)glGetGraphicsResetStatusEXT},
        {"glReadnPixelsEXT", (__eglMustCastToProperFunctionPointerType)glReadnPixelsEXT},
        {"glGetnUniformfvEXT", (__eglMustCastToProperFunctionPointerType)glGetnUniformfvEXT},
        {"glGetnUniformivEXT", (__eglMustCastToProperFunctionPointerType)glGetnUniformivEXT},
        {"glGenQueriesEXT", (__eglMustCastToProperFunctionPointerType)glGenQueriesEXT},
        {"glDeleteQueriesEXT", (__eglMustCastToProperFunctionPointerType)glDeleteQueriesEXT},
        {"glIsQueryEXT", (__eglMustCastToProperFunctionPointerType)glIsQueryEXT},
        {"glBeginQueryEXT", (__eglMustCastToProperFunctionPointerType)glBeginQueryEXT},
        {"glEndQueryEXT", (__eglMustCastToProperFunctionPointerType)glEndQueryEXT},
        {"glGetQueryivEXT", (__eglMustCastToProperFunctionPointerType)glGetQueryivEXT},
        {"glGetQueryObjectuivEXT", (__eglMustCastToProperFunctionPointerType)glGetQueryObjectuivEXT},
        {"glDrawBuffersEXT", (__eglMustCastToProperFunctionPointerType)glDrawBuffersEXT},
        {"glVertexAttribDivisorANGLE", (__eglMustCastToProperFunctionPointerType)glVertexAttribDivisorANGLE},
        {"glDrawArraysInstancedANGLE", (__eglMustCastToProperFunctionPointerType)glDrawArraysInstancedANGLE},
        {"glDrawElementsInstancedANGLE", (__eglMustCastToProperFunctionPointerType)glDrawElementsInstancedANGLE},
        {"glGetProgramBinaryOES", (__eglMustCastToProperFunctionPointerType)glGetProgramBinaryOES},
        {"glProgramBinaryOES", (__eglMustCastToProperFunctionPointerType)glProgramBinaryOES},
        {"glGetBufferPointervOES", (__eglMustCastToProperFunctionPointerType)glGetBufferPointervOES},
        {"glMapBufferOES", (__eglMustCastToProperFunctionPointerType)glMapBufferOES},
        {"glUnmapBufferOES", (__eglMustCastToProperFunctionPointerType)glUnmapBufferOES},
        {"glMapBufferRangeEXT", (__eglMustCastToProperFunctionPointerType)glMapBufferRangeEXT},
        {"glFlushMappedBufferRangeEXT", (__eglMustCastToProperFunctionPointerType)glFlushMappedBufferRangeEXT},    };

    for (unsigned int ext = 0; ext < ArraySize(glExtensions); ext++)
    {
        if (strcmp(procname, glExtensions[ext].name) == 0)
        {
            return (__eglMustCastToProperFunctionPointerType)glExtensions[ext].address;
        }
    }

    return NULL;
}

// Non-public functions used by EGL

bool __stdcall glBindTexImage(egl::Surface *surface)
{
    EVENT("(egl::Surface* surface = 0x%0.8p)",
          surface);

    try
    {
        gl::Context *context = gl::getNonLostContext();

        if (context)
        {
            gl::Texture2D *textureObject = context->getTexture2D();
            ASSERT(textureObject != NULL);

            if (textureObject->isImmutable())
            {
                return false;
            }

            textureObject->bindTexImage(surface);
        }
    }
    catch(std::bad_alloc&)
    {
        return gl::error(GL_OUT_OF_MEMORY, false);
    }

    return true;
}

}
