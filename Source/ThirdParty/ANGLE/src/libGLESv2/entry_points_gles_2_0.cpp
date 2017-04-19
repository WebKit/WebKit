//
// Copyright(c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// entry_points_gles_2_0.cpp : Implements the GLES 2.0 entry points.

#include "libGLESv2/entry_points_gles_2_0.h"

#include "libGLESv2/global_state.h"

#include "libANGLE/formatutils.h"
#include "libANGLE/Buffer.h"
#include "libANGLE/Compiler.h"
#include "libANGLE/Context.h"
#include "libANGLE/Error.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/Renderbuffer.h"
#include "libANGLE/Shader.h"
#include "libANGLE/Program.h"
#include "libANGLE/Texture.h"
#include "libANGLE/VertexArray.h"
#include "libANGLE/FramebufferAttachment.h"

#include "libANGLE/validationES.h"
#include "libANGLE/validationES2.h"
#include "libANGLE/validationES3.h"
#include "libANGLE/queryconversions.h"
#include "libANGLE/queryutils.h"

#include "common/debug.h"
#include "common/utilities.h"

namespace gl
{

void GL_APIENTRY ActiveTexture(GLenum texture)
{
    EVENT("(GLenum texture = 0x%X)", texture);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateActiveTexture(context, texture))
        {
            return;
        }

        context->activeTexture(texture);
    }
}

void GL_APIENTRY AttachShader(GLuint program, GLuint shader)
{
    EVENT("(GLuint program = %d, GLuint shader = %d)", program, shader);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateAttachShader(context, program, shader))
        {
            return;
        }

        context->attachShader(program, shader);
    }
}

void GL_APIENTRY BindAttribLocation(GLuint program, GLuint index, const GLchar* name)
{
    EVENT("(GLuint program = %d, GLuint index = %d, const GLchar* name = 0x%0.8p)", program, index, name);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateBindAttribLocation(context, program, index, name))
        {
            return;
        }

        context->bindAttribLocation(program, index, name);
    }
}

void GL_APIENTRY BindBuffer(GLenum target, GLuint buffer)
{
    EVENT("(GLenum target = 0x%X, GLuint buffer = %d)", target, buffer);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateBindBuffer(context, target, buffer))
        {
            return;
        }

        context->bindBuffer(target, buffer);
    }
}

void GL_APIENTRY BindFramebuffer(GLenum target, GLuint framebuffer)
{
    EVENT("(GLenum target = 0x%X, GLuint framebuffer = %d)", target, framebuffer);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateBindFramebuffer(context, target, framebuffer))
        {
            return;
        }

        context->bindFramebuffer(target, framebuffer);
    }
}

void GL_APIENTRY BindRenderbuffer(GLenum target, GLuint renderbuffer)
{
    EVENT("(GLenum target = 0x%X, GLuint renderbuffer = %d)", target, renderbuffer);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateBindRenderbuffer(context, target, renderbuffer))
        {
            return;
        }

        context->bindRenderbuffer(target, renderbuffer);
    }
}

void GL_APIENTRY BindTexture(GLenum target, GLuint texture)
{
    EVENT("(GLenum target = 0x%X, GLuint texture = %d)", target, texture);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateBindTexture(context, target, texture))
        {
            return;
        }

        context->bindTexture(target, texture);
    }
}

void GL_APIENTRY BlendColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
    EVENT("(GLclampf red = %f, GLclampf green = %f, GLclampf blue = %f, GLclampf alpha = %f)",
          red, green, blue, alpha);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateBlendColor(context, red, green, blue, alpha))
        {
            return;
        }

        context->blendColor(red, green, blue, alpha);
    }
}

void GL_APIENTRY BlendEquation(GLenum mode)
{
    EVENT("(GLenum mode = 0x%X)", mode);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateBlendEquation(context, mode))
        {
            return;
        }

        context->blendEquation(mode);
    }
}

void GL_APIENTRY BlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha)
{
    EVENT("(GLenum modeRGB = 0x%X, GLenum modeAlpha = 0x%X)", modeRGB, modeAlpha);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateBlendEquationSeparate(context, modeRGB, modeAlpha))
        {
            return;
        }

        context->blendEquationSeparate(modeRGB, modeAlpha);
    }
}

void GL_APIENTRY BlendFunc(GLenum sfactor, GLenum dfactor)
{
    EVENT("(GLenum sfactor = 0x%X, GLenum dfactor = 0x%X)", sfactor, dfactor);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateBlendFunc(context, sfactor, dfactor))
        {
            return;
        }

        context->blendFunc(sfactor, dfactor);
    }
}

void GL_APIENTRY BlendFuncSeparate(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha)
{
    EVENT("(GLenum srcRGB = 0x%X, GLenum dstRGB = 0x%X, GLenum srcAlpha = 0x%X, GLenum dstAlpha = 0x%X)",
          srcRGB, dstRGB, srcAlpha, dstAlpha);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateBlendFuncSeparate(context, srcRGB, dstRGB, srcAlpha, dstAlpha))
        {
            return;
        }

        context->blendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
    }
}

void GL_APIENTRY BufferData(GLenum target, GLsizeiptr size, const GLvoid* data, GLenum usage)
{
    EVENT("(GLenum target = 0x%X, GLsizeiptr size = %d, const GLvoid* data = 0x%0.8p, GLenum usage = %d)",
          target, size, data, usage);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateBufferData(context, target, size, data, usage))
        {
            return;
        }

        context->bufferData(target, size, data, usage);
    }
}

void GL_APIENTRY BufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid* data)
{
    EVENT("(GLenum target = 0x%X, GLintptr offset = %d, GLsizeiptr size = %d, const GLvoid* data = 0x%0.8p)",
          target, offset, size, data);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateBufferSubData(context, target, offset, size, data))
        {
            return;
        }

        context->bufferSubData(target, offset, size, data);
    }
}

GLenum GL_APIENTRY CheckFramebufferStatus(GLenum target)
{
    EVENT("(GLenum target = 0x%X)", target);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateCheckFramebufferStatus(context, target))
        {
            return 0;
        }

        return context->checkFramebufferStatus(target);
    }

    return 0;
}

void GL_APIENTRY Clear(GLbitfield mask)
{
    EVENT("(GLbitfield mask = 0x%X)", mask);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateClear(context, mask))
        {
            return;
        }

        context->clear(mask);
    }
}

void GL_APIENTRY ClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
    EVENT("(GLclampf red = %f, GLclampf green = %f, GLclampf blue = %f, GLclampf alpha = %f)",
          red, green, blue, alpha);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateClearColor(context, red, green, blue, alpha))
        {
            return;
        }

        context->clearColor(red, green, blue, alpha);
    }
}

void GL_APIENTRY ClearDepthf(GLclampf depth)
{
    EVENT("(GLclampf depth = %f)", depth);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateClearDepthf(context, depth))
        {
            return;
        }

        context->clearDepthf(depth);
    }
}

void GL_APIENTRY ClearStencil(GLint s)
{
    EVENT("(GLint s = %d)", s);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateClearStencil(context, s))
        {
            return;
        }

        context->clearStencil(s);
    }
}

void GL_APIENTRY ColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
    EVENT("(GLboolean red = %d, GLboolean green = %u, GLboolean blue = %u, GLboolean alpha = %u)",
          red, green, blue, alpha);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateColorMask(context, red, green, blue, alpha))
        {
            return;
        }

        context->colorMask(red, green, blue, alpha);
    }
}

void GL_APIENTRY CompileShader(GLuint shader)
{
    EVENT("(GLuint shader = %d)", shader);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateCompileShader(context, shader))
        {
            return;
        }

        context->compileShader(shader);
    }
}

void GL_APIENTRY CompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height,
                                      GLint border, GLsizei imageSize, const GLvoid* data)
{
    EVENT("(GLenum target = 0x%X, GLint level = %d, GLenum internalformat = 0x%X, GLsizei width = %d, "
          "GLsizei height = %d, GLint border = %d, GLsizei imageSize = %d, const GLvoid* data = 0x%0.8p)",
          target, level, internalformat, width, height, border, imageSize, data);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateCompressedTexImage2D(context, target, level, internalformat, width, height,
                                          border, imageSize, data))
        {
            return;
        }

        context->compressedTexImage2D(target, level, internalformat, width, height, border,
                                      imageSize, data);
    }
}

void GL_APIENTRY CompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                         GLenum format, GLsizei imageSize, const GLvoid* data)
{
    EVENT("(GLenum target = 0x%X, GLint level = %d, GLint xoffset = %d, GLint yoffset = %d, "
          "GLsizei width = %d, GLsizei height = %d, GLenum format = 0x%X, "
          "GLsizei imageSize = %d, const GLvoid* data = 0x%0.8p)",
          target, level, xoffset, yoffset, width, height, format, imageSize, data);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateCompressedTexSubImage2D(context, target, level, xoffset, yoffset, width,
                                             height, format, imageSize, data))
        {
            return;
        }

        context->compressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format,
                                         imageSize, data);
    }
}

void GL_APIENTRY CopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border)
{
    EVENT("(GLenum target = 0x%X, GLint level = %d, GLenum internalformat = 0x%X, "
          "GLint x = %d, GLint y = %d, GLsizei width = %d, GLsizei height = %d, GLint border = %d)",
          target, level, internalformat, x, y, width, height, border);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateCopyTexImage2D(context, target, level, internalformat, x, y, width, height,
                                    border))
        {
            return;
        }
        context->copyTexImage2D(target, level, internalformat, x, y, width, height, border);
    }
}

void GL_APIENTRY CopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
    EVENT("(GLenum target = 0x%X, GLint level = %d, GLint xoffset = %d, GLint yoffset = %d, "
          "GLint x = %d, GLint y = %d, GLsizei width = %d, GLsizei height = %d)",
          target, level, xoffset, yoffset, x, y, width, height);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateCopyTexSubImage2D(context, target, level, xoffset, yoffset, x, y, width,
                                       height))
        {
            return;
        }

        context->copyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
    }
}

GLuint GL_APIENTRY CreateProgram(void)
{
    EVENT("()");

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateCreateProgram(context))
        {
            return 0;
        }

        return context->createProgram();
    }

    return 0;
}

GLuint GL_APIENTRY CreateShader(GLenum type)
{
    EVENT("(GLenum type = 0x%X)", type);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateCreateShader(context, type))
        {
            return 0;
        }
        return context->createShader(type);
    }
    return 0;
}

void GL_APIENTRY CullFace(GLenum mode)
{
    EVENT("(GLenum mode = 0x%X)", mode);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateCullFace(context, mode))
        {
            return;
        }

        context->cullFace(mode);
    }
}

void GL_APIENTRY DeleteBuffers(GLsizei n, const GLuint* buffers)
{
    EVENT("(GLsizei n = %d, const GLuint* buffers = 0x%0.8p)", n, buffers);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateDeleteBuffers(context, n, buffers))
        {
            return;
        }

        context->deleteBuffers(n, buffers);
    }
}

void GL_APIENTRY DeleteFramebuffers(GLsizei n, const GLuint* framebuffers)
{
    EVENT("(GLsizei n = %d, const GLuint* framebuffers = 0x%0.8p)", n, framebuffers);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateDeleteFramebuffers(context, n, framebuffers))
        {
            return;
        }

        context->deleteFramebuffers(n, framebuffers);
    }
}

void GL_APIENTRY DeleteProgram(GLuint program)
{
    EVENT("(GLuint program = %d)", program);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateDeleteProgram(context, program))
        {
            return;
        }

        context->deleteProgram(program);
    }
}

void GL_APIENTRY DeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers)
{
    EVENT("(GLsizei n = %d, const GLuint* renderbuffers = 0x%0.8p)", n, renderbuffers);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateDeleteRenderbuffers(context, n, renderbuffers))
        {
            return;
        }

        context->deleteRenderbuffers(n, renderbuffers);
    }
}

void GL_APIENTRY DeleteShader(GLuint shader)
{
    EVENT("(GLuint shader = %d)", shader);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateDeleteShader(context, shader))
        {
            return;
        }

        context->deleteShader(shader);
    }
}

void GL_APIENTRY DeleteTextures(GLsizei n, const GLuint* textures)
{
    EVENT("(GLsizei n = %d, const GLuint* textures = 0x%0.8p)", n, textures);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateDeleteTextures(context, n, textures))
        {
            return;
        }

        context->deleteTextures(n, textures);
    }
}

void GL_APIENTRY DepthFunc(GLenum func)
{
    EVENT("(GLenum func = 0x%X)", func);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateDepthFunc(context, func))
        {
            return;
        }

        context->depthFunc(func);
    }
}

void GL_APIENTRY DepthMask(GLboolean flag)
{
    EVENT("(GLboolean flag = %u)", flag);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateDepthMask(context, flag))
        {
            return;
        }

        context->depthMask(flag);
    }
}

void GL_APIENTRY DepthRangef(GLclampf zNear, GLclampf zFar)
{
    EVENT("(GLclampf zNear = %f, GLclampf zFar = %f)", zNear, zFar);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateDepthRangef(context, zNear, zFar))
        {
            return;
        }

        context->depthRangef(zNear, zFar);
    }
}

void GL_APIENTRY DetachShader(GLuint program, GLuint shader)
{
    EVENT("(GLuint program = %d, GLuint shader = %d)", program, shader);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateDetachShader(context, program, shader))
        {
            return;
        }

        context->detachShader(program, shader);
    }
}

void GL_APIENTRY Disable(GLenum cap)
{
    EVENT("(GLenum cap = 0x%X)", cap);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateDisable(context, cap))
        {
            return;
        }

        context->disable(cap);
    }
}

void GL_APIENTRY DisableVertexAttribArray(GLuint index)
{
    EVENT("(GLuint index = %d)", index);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateDisableVertexAttribArray(context, index))
        {
            return;
        }

        context->disableVertexAttribArray(index);
    }
}

void GL_APIENTRY DrawArrays(GLenum mode, GLint first, GLsizei count)
{
    EVENT("(GLenum mode = 0x%X, GLint first = %d, GLsizei count = %d)", mode, first, count);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateDrawArrays(context, mode, first, count))
        {
            return;
        }

        context->drawArrays(mode, first, count);
    }
}

void GL_APIENTRY DrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices)
{
    EVENT("(GLenum mode = 0x%X, GLsizei count = %d, GLenum type = 0x%X, const GLvoid* indices = 0x%0.8p)",
          mode, count, type, indices);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        // TODO(jmadill): Cache index range in the context.
        IndexRange indexRange;
        if (!ValidateDrawElements(context, mode, count, type, indices, 1, &indexRange))
        {
            return;
        }

        context->drawElements(mode, count, type, indices, indexRange);
    }
}

void GL_APIENTRY Enable(GLenum cap)
{
    EVENT("(GLenum cap = 0x%X)", cap);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateEnable(context, cap))
        {
            return;
        }

        context->enable(cap);
    }
}

void GL_APIENTRY EnableVertexAttribArray(GLuint index)
{
    EVENT("(GLuint index = %d)", index);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateEnableVertexAttribArray(context, index))
        {
            return;
        }

        context->enableVertexAttribArray(index);
    }
}

void GL_APIENTRY Finish(void)
{
    EVENT("()");

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateFinish(context))
        {
            return;
        }

        context->finish();
    }
}

void GL_APIENTRY Flush(void)
{
    EVENT("()");

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateFlush(context))
        {
            return;
        }

        context->flush();
    }
}

void GL_APIENTRY FramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
    EVENT("(GLenum target = 0x%X, GLenum attachment = 0x%X, GLenum renderbuffertarget = 0x%X, "
          "GLuint renderbuffer = %d)", target, attachment, renderbuffertarget, renderbuffer);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateFramebufferRenderbuffer(context, target, attachment, renderbuffertarget,
                                             renderbuffer))
        {
            return;
        }

        context->framebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
    }
}

void GL_APIENTRY FramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
    EVENT("(GLenum target = 0x%X, GLenum attachment = 0x%X, GLenum textarget = 0x%X, "
          "GLuint texture = %d, GLint level = %d)", target, attachment, textarget, texture, level);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateFramebufferTexture2D(context, target, attachment, textarget, texture, level))
        {
            return;
        }

        context->framebufferTexture2D(target, attachment, textarget, texture, level);
    }
}

void GL_APIENTRY FrontFace(GLenum mode)
{
    EVENT("(GLenum mode = 0x%X)", mode);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateFrontFace(context, mode))
        {
            return;
        }

        context->frontFace(mode);
    }
}

void GL_APIENTRY GenBuffers(GLsizei n, GLuint* buffers)
{
    EVENT("(GLsizei n = %d, GLuint* buffers = 0x%0.8p)", n, buffers);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateGenBuffers(context, n, buffers))
        {
            return;
        }

        context->genBuffers(n, buffers);
    }
}

void GL_APIENTRY GenerateMipmap(GLenum target)
{
    EVENT("(GLenum target = 0x%X)", target);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateGenerateMipmap(context, target))
        {
            return;
        }

        context->generateMipmap(target);
    }
}

void GL_APIENTRY GenFramebuffers(GLsizei n, GLuint* framebuffers)
{
    EVENT("(GLsizei n = %d, GLuint* framebuffers = 0x%0.8p)", n, framebuffers);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateGenFramebuffers(context, n, framebuffers))
        {
            return;
        }

        context->genFramebuffers(n, framebuffers);
    }
}

void GL_APIENTRY GenRenderbuffers(GLsizei n, GLuint* renderbuffers)
{
    EVENT("(GLsizei n = %d, GLuint* renderbuffers = 0x%0.8p)", n, renderbuffers);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateGenRenderbuffers(context, n, renderbuffers))
        {
            return;
        }

        context->genRenderbuffers(n, renderbuffers);
    }
}

void GL_APIENTRY GenTextures(GLsizei n, GLuint* textures)
{
    EVENT("(GLsizei n = %d, GLuint* textures = 0x%0.8p)", n, textures);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateGenTextures(context, n, textures))
        {
            return;
        }

        context->genTextures(n, textures);
    }
}

void GL_APIENTRY GetActiveAttrib(GLuint program, GLuint index, GLsizei bufsize, GLsizei *length, GLint *size, GLenum *type, GLchar *name)
{
    EVENT("(GLuint program = %d, GLuint index = %d, GLsizei bufsize = %d, GLsizei *length = 0x%0.8p, "
          "GLint *size = 0x%0.8p, GLenum *type = %0.8p, GLchar *name = %0.8p)",
          program, index, bufsize, length, size, type, name);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateGetActiveAttrib(context, program, index, bufsize, length, size, type, name))
        {
            return;
        }

        context->getActiveAttrib(program, index, bufsize, length, size, type, name);
    }
}

void GL_APIENTRY GetActiveUniform(GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size, GLenum* type, GLchar* name)
{
    EVENT("(GLuint program = %d, GLuint index = %d, GLsizei bufsize = %d, "
          "GLsizei* length = 0x%0.8p, GLint* size = 0x%0.8p, GLenum* type = 0x%0.8p, GLchar* name = 0x%0.8p)",
          program, index, bufsize, length, size, type, name);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateGetActiveUniform(context, program, index, bufsize, length, size, type, name))
        {
            return;
        }

        context->getActiveUniform(program, index, bufsize, length, size, type, name);
    }
}

void GL_APIENTRY GetAttachedShaders(GLuint program, GLsizei maxcount, GLsizei* count, GLuint* shaders)
{
    EVENT("(GLuint program = %d, GLsizei maxcount = %d, GLsizei* count = 0x%0.8p, GLuint* shaders = 0x%0.8p)",
          program, maxcount, count, shaders);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateGetAttachedShaders(context, program, maxcount, count, shaders))
        {
            return;
        }

        context->getAttachedShaders(program, maxcount, count, shaders);
    }
}

GLint GL_APIENTRY GetAttribLocation(GLuint program, const GLchar* name)
{
    EVENT("(GLuint program = %d, const GLchar* name = %s)", program, name);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateGetAttribLocation(context, program, name))
        {
            return -1;
        }

        return context->getAttribLocation(program, name);
    }

    return -1;
}

void GL_APIENTRY GetBooleanv(GLenum pname, GLboolean* params)
{
    EVENT("(GLenum pname = 0x%X, GLboolean* params = 0x%0.8p)",  pname, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateGetBooleanv(context, pname, params))
        {
            return;
        }

        context->getBooleanv(pname, params);
    }
}

void GL_APIENTRY GetBufferParameteriv(GLenum target, GLenum pname, GLint* params)
{
    EVENT("(GLenum target = 0x%X, GLenum pname = 0x%X, GLint* params = 0x%0.8p)", target, pname, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateGetBufferParameteriv(context, target, pname, params))
        {
            return;
        }

        context->getBufferParameteriv(target, pname, params);
    }
}

GLenum GL_APIENTRY GetError(void)
{
    EVENT("()");

    Context *context = GetGlobalContext();

    if (context)
    {
        if (!context->skipValidation() && !ValidateGetError(context))
        {
            return GL_NO_ERROR;
        }

        return context->getError();
    }

    return GL_NO_ERROR;
}

void GL_APIENTRY GetFloatv(GLenum pname, GLfloat* params)
{
    EVENT("(GLenum pname = 0x%X, GLfloat* params = 0x%0.8p)", pname, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateGetFloatv(context, pname, params))
        {
            return;
        }

        context->getFloatv(pname, params);
    }
}

void GL_APIENTRY GetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint* params)
{
    EVENT("(GLenum target = 0x%X, GLenum attachment = 0x%X, GLenum pname = 0x%X, GLint* params = 0x%0.8p)",
          target, attachment, pname, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        GLsizei numParams = 0;
        if (!context->skipValidation() &&
            !ValidateGetFramebufferAttachmentParameteriv(context, target, attachment, pname,
                                                         &numParams))
        {
            return;
        }

        context->getFramebufferAttachmentParameteriv(target, attachment, pname, params);
    }
}

void GL_APIENTRY GetIntegerv(GLenum pname, GLint* params)
{
    EVENT("(GLenum pname = 0x%X, GLint* params = 0x%0.8p)", pname, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateGetIntegerv(context, pname, params))
        {
            return;
        }

        context->getIntegerv(pname, params);
    }
}

void GL_APIENTRY GetProgramiv(GLuint program, GLenum pname, GLint* params)
{
    EVENT("(GLuint program = %d, GLenum pname = %d, GLint* params = 0x%0.8p)", program, pname, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        GLsizei numParams = 0;
        if (!context->skipValidation() &&
            !ValidateGetProgramiv(context, program, pname, &numParams))
        {
            return;
        }

        context->getProgramiv(program, pname, params);
    }
}

void GL_APIENTRY GetProgramInfoLog(GLuint program, GLsizei bufsize, GLsizei* length, GLchar* infolog)
{
    EVENT("(GLuint program = %d, GLsizei bufsize = %d, GLsizei* length = 0x%0.8p, GLchar* infolog = 0x%0.8p)",
          program, bufsize, length, infolog);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateGetProgramInfoLog(context, program, bufsize, length, infolog))
        {
            return;
        }

        context->getInfoLog(program, bufsize, length, infolog);
    }
}

void GL_APIENTRY GetRenderbufferParameteriv(GLenum target, GLenum pname, GLint* params)
{
    EVENT("(GLenum target = 0x%X, GLenum pname = 0x%X, GLint* params = 0x%0.8p)", target, pname, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateGetRenderbufferParameteriv(context, target, pname, params))
        {
            return;
        }

        context->getRenderbufferParameteriv(target, pname, params);
    }
}

void GL_APIENTRY GetShaderiv(GLuint shader, GLenum pname, GLint* params)
{
    EVENT("(GLuint shader = %d, GLenum pname = %d, GLint* params = 0x%0.8p)", shader, pname, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateGetShaderiv(context, shader, pname, params))
        {
            return;
        }

        context->getShaderiv(shader, pname, params);
    }
}

void GL_APIENTRY GetShaderInfoLog(GLuint shader, GLsizei bufsize, GLsizei* length, GLchar* infolog)
{
    EVENT("(GLuint shader = %d, GLsizei bufsize = %d, GLsizei* length = 0x%0.8p, GLchar* infolog = 0x%0.8p)",
          shader, bufsize, length, infolog);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateGetShaderInfoLog(context, shader, bufsize, length, infolog))
        {
            return;
        }

        context->getShaderInfoLog(shader, bufsize, length, infolog);
    }
}

void GL_APIENTRY GetShaderPrecisionFormat(GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision)
{
    EVENT("(GLenum shadertype = 0x%X, GLenum precisiontype = 0x%X, GLint* range = 0x%0.8p, GLint* precision = 0x%0.8p)",
          shadertype, precisiontype, range, precision);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateGetShaderPrecisionFormat(context, shadertype, precisiontype, range, precision))
        {
            return;
        }

        context->getShaderPrecisionFormat(shadertype, precisiontype, range, precision);
    }
}

void GL_APIENTRY GetShaderSource(GLuint shader, GLsizei bufsize, GLsizei* length, GLchar* source)
{
    EVENT("(GLuint shader = %d, GLsizei bufsize = %d, GLsizei* length = 0x%0.8p, GLchar* source = 0x%0.8p)",
          shader, bufsize, length, source);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateGetShaderSource(context, shader, bufsize, length, source))
        {
            return;
        }

        context->getShaderSource(shader, bufsize, length, source);
    }
}

const GLubyte *GL_APIENTRY GetString(GLenum name)
{
    EVENT("(GLenum name = 0x%X)", name);

    Context *context = GetValidGlobalContext();

    if (context)
    {
        if (!context->skipValidation() && !ValidateGetString(context, name))
        {
            return nullptr;
        }

        return context->getString(name);
    }

    return nullptr;
}

void GL_APIENTRY GetTexParameterfv(GLenum target, GLenum pname, GLfloat* params)
{
    EVENT("(GLenum target = 0x%X, GLenum pname = 0x%X, GLfloat* params = 0x%0.8p)", target, pname, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateGetTexParameterfv(context, target, pname, params))
        {
            return;
        }

        context->getTexParameterfv(target, pname, params);
    }
}

void GL_APIENTRY GetTexParameteriv(GLenum target, GLenum pname, GLint* params)
{
    EVENT("(GLenum target = 0x%X, GLenum pname = 0x%X, GLint* params = 0x%0.8p)", target, pname, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateGetTexParameteriv(context, target, pname, params))
        {
            return;
        }

        context->getTexParameteriv(target, pname, params);
    }
}

void GL_APIENTRY GetUniformfv(GLuint program, GLint location, GLfloat* params)
{
    EVENT("(GLuint program = %d, GLint location = %d, GLfloat* params = 0x%0.8p)", program, location, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateGetUniformfv(context, program, location, params))
        {
            return;
        }

        context->getUniformfv(program, location, params);
    }
}

void GL_APIENTRY GetUniformiv(GLuint program, GLint location, GLint* params)
{
    EVENT("(GLuint program = %d, GLint location = %d, GLint* params = 0x%0.8p)", program, location, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateGetUniformiv(context, program, location, params))
        {
            return;
        }

        context->getUniformiv(program, location, params);
    }
}

GLint GL_APIENTRY GetUniformLocation(GLuint program, const GLchar* name)
{
    EVENT("(GLuint program = %d, const GLchar* name = 0x%0.8p)", program, name);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateGetUniformLocation(context, program, name))
        {
            return -1;
        }

        return context->getUniformLocation(program, name);
    }

    return -1;
}

void GL_APIENTRY GetVertexAttribfv(GLuint index, GLenum pname, GLfloat* params)
{
    EVENT("(GLuint index = %d, GLenum pname = 0x%X, GLfloat* params = 0x%0.8p)", index, pname, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateGetVertexAttribfv(context, index, pname, params))
        {
            return;
        }

        context->getVertexAttribfv(index, pname, params);
    }
}

void GL_APIENTRY GetVertexAttribiv(GLuint index, GLenum pname, GLint* params)
{
    EVENT("(GLuint index = %d, GLenum pname = 0x%X, GLint* params = 0x%0.8p)", index, pname, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateGetVertexAttribiv(context, index, pname, params))
        {
            return;
        }

        context->getVertexAttribiv(index, pname, params);
    }
}

void GL_APIENTRY GetVertexAttribPointerv(GLuint index, GLenum pname, GLvoid** pointer)
{
    EVENT("(GLuint index = %d, GLenum pname = 0x%X, GLvoid** pointer = 0x%0.8p)", index, pname, pointer);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateGetVertexAttribPointerv(context, index, pname, pointer))
        {
            return;
        }

        context->getVertexAttribPointerv(index, pname, pointer);
    }
}

void GL_APIENTRY Hint(GLenum target, GLenum mode)
{
    EVENT("(GLenum target = 0x%X, GLenum mode = 0x%X)", target, mode);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateHint(context, target, mode))
        {
            return;
        }

        context->hint(target, mode);
    }
}

GLboolean GL_APIENTRY IsBuffer(GLuint buffer)
{
    EVENT("(GLuint buffer = %d)", buffer);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateIsBuffer(context, buffer))
        {
            return GL_FALSE;
        }

        return context->isBuffer(buffer);
    }

    return GL_FALSE;
}

GLboolean GL_APIENTRY IsEnabled(GLenum cap)
{
    EVENT("(GLenum cap = 0x%X)", cap);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateIsEnabled(context, cap))
        {
            return GL_FALSE;
        }

        return context->isEnabled(cap);
    }

    return GL_FALSE;
}

GLboolean GL_APIENTRY IsFramebuffer(GLuint framebuffer)
{
    EVENT("(GLuint framebuffer = %d)", framebuffer);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateIsFramebuffer(context, framebuffer))
        {
            return GL_FALSE;
        }

        return context->isFramebuffer(framebuffer);
    }

    return GL_FALSE;
}

GLboolean GL_APIENTRY IsProgram(GLuint program)
{
    EVENT("(GLuint program = %d)", program);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateIsProgram(context, program))
        {
            return GL_FALSE;
        }

        return context->isProgram(program);
    }

    return GL_FALSE;
}

GLboolean GL_APIENTRY IsRenderbuffer(GLuint renderbuffer)
{
    EVENT("(GLuint renderbuffer = %d)", renderbuffer);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateIsRenderbuffer(context, renderbuffer))
        {
            return GL_FALSE;
        }

        return context->isRenderbuffer(renderbuffer);
    }

    return GL_FALSE;
}

GLboolean GL_APIENTRY IsShader(GLuint shader)
{
    EVENT("(GLuint shader = %d)", shader);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateIsShader(context, shader))
        {
            return GL_FALSE;
        }

        return context->isShader(shader);
    }

    return GL_FALSE;
}

GLboolean GL_APIENTRY IsTexture(GLuint texture)
{
    EVENT("(GLuint texture = %d)", texture);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateIsTexture(context, texture))
        {
            return GL_FALSE;
        }

        return context->isTexture(texture);
    }

    return GL_FALSE;
}

void GL_APIENTRY LineWidth(GLfloat width)
{
    EVENT("(GLfloat width = %f)", width);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateLineWidth(context, width))
        {
            return;
        }

        context->lineWidth(width);
    }
}

void GL_APIENTRY LinkProgram(GLuint program)
{
    EVENT("(GLuint program = %d)", program);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateLinkProgram(context, program))
        {
            return;
        }

        context->linkProgram(program);
    }
}

void GL_APIENTRY PixelStorei(GLenum pname, GLint param)
{
    EVENT("(GLenum pname = 0x%X, GLint param = %d)", pname, param);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidatePixelStorei(context, pname, param))
        {
            return;
        }

        context->pixelStorei(pname, param);
    }
}

void GL_APIENTRY PolygonOffset(GLfloat factor, GLfloat units)
{
    EVENT("(GLfloat factor = %f, GLfloat units = %f)", factor, units);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidatePolygonOffset(context, factor, units))
        {
            return;
        }

        context->polygonOffset(factor, units);
    }
}

void GL_APIENTRY ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height,
                            GLenum format, GLenum type, GLvoid* pixels)
{
    EVENT("(GLint x = %d, GLint y = %d, GLsizei width = %d, GLsizei height = %d, "
          "GLenum format = 0x%X, GLenum type = 0x%X, GLvoid* pixels = 0x%0.8p)",
          x, y, width, height, format, type,  pixels);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateReadPixels(context, x, y, width, height, format, type, pixels))
        {
            return;
        }

        context->readPixels(x, y, width, height, format, type, pixels);
    }
}

void GL_APIENTRY ReleaseShaderCompiler(void)
{
    EVENT("()");

    Context *context = GetValidGlobalContext();

    if (context)
    {
        if (!context->skipValidation() && !ValidateReleaseShaderCompiler(context))
        {
            return;
        }

        context->releaseShaderCompiler();
    }
}

void GL_APIENTRY RenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
    EVENT("(GLenum target = 0x%X, GLenum internalformat = 0x%X, GLsizei width = %d, GLsizei height = %d)",
          target, internalformat, width, height);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateRenderbufferStorage(context, target, internalformat, width, height))
        {
            return;
        }

        context->renderbufferStorage(target, internalformat, width, height);
    }
}

void GL_APIENTRY SampleCoverage(GLclampf value, GLboolean invert)
{
    EVENT("(GLclampf value = %f, GLboolean invert = %u)", value, invert);

    Context* context = GetValidGlobalContext();

    if (context)
    {
        if (!context->skipValidation() && !ValidateSampleCoverage(context, value, invert))
        {
            return;
        }

        context->sampleCoverage(value, invert);
    }
}

void GL_APIENTRY Scissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
    EVENT("(GLint x = %d, GLint y = %d, GLsizei width = %d, GLsizei height = %d)", x, y, width, height);

    Context* context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateScissor(context, x, y, width, height))
        {
            return;
        }

        context->scissor(x, y, width, height);
    }
}

void GL_APIENTRY ShaderBinary(GLsizei n, const GLuint* shaders, GLenum binaryformat, const GLvoid* binary, GLsizei length)
{
    EVENT("(GLsizei n = %d, const GLuint* shaders = 0x%0.8p, GLenum binaryformat = 0x%X, "
          "const GLvoid* binary = 0x%0.8p, GLsizei length = %d)",
          n, shaders, binaryformat, binary, length);

    Context* context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateShaderBinary(context, n, shaders, binaryformat, binary, length))
        {
            return;
        }

        context->shaderBinary(n, shaders, binaryformat, binary, length);
    }
}

void GL_APIENTRY ShaderSource(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length)
{
    EVENT("(GLuint shader = %d, GLsizei count = %d, const GLchar** string = 0x%0.8p, const GLint* length = 0x%0.8p)",
          shader, count, string, length);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateShaderSource(context, shader, count, string, length))
        {
            return;
        }

        context->shaderSource(shader, count, string, length);
    }
}

void GL_APIENTRY StencilFunc(GLenum func, GLint ref, GLuint mask)
{
    EVENT("(GLenum func = 0x%X, GLint ref = %d, GLuint mask = %d)", func, ref, mask);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateStencilFunc(context, func, ref, mask))
        {
            return;
        }

        context->stencilFunc(func, ref, mask);
    }
}

void GL_APIENTRY StencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask)
{
    EVENT("(GLenum face = 0x%X, GLenum func = 0x%X, GLint ref = %d, GLuint mask = %d)", face, func, ref, mask);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateStencilFuncSeparate(context, face, func, ref, mask))
        {
            return;
        }

        context->stencilFuncSeparate(face, func, ref, mask);
    }
}

void GL_APIENTRY StencilMask(GLuint mask)
{
    EVENT("(GLuint mask = %d)", mask);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateStencilMask(context, mask))
        {
            return;
        }

        context->stencilMask(mask);
    }
}

void GL_APIENTRY StencilMaskSeparate(GLenum face, GLuint mask)
{
    EVENT("(GLenum face = 0x%X, GLuint mask = %d)", face, mask);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateStencilMaskSeparate(context, face, mask))
        {
            return;
        }

        context->stencilMaskSeparate(face, mask);
    }
}

void GL_APIENTRY StencilOp(GLenum fail, GLenum zfail, GLenum zpass)
{
    EVENT("(GLenum fail = 0x%X, GLenum zfail = 0x%X, GLenum zpas = 0x%Xs)", fail, zfail, zpass);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateStencilOp(context, fail, zfail, zpass))
        {
            return;
        }

        context->stencilOp(fail, zfail, zpass);
    }
}

void GL_APIENTRY StencilOpSeparate(GLenum face, GLenum fail, GLenum zfail, GLenum zpass)
{
    EVENT("(GLenum face = 0x%X, GLenum fail = 0x%X, GLenum zfail = 0x%X, GLenum zpas = 0x%Xs)",
          face, fail, zfail, zpass);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateStencilOpSeparate(context, face, fail, zfail, zpass))
        {
            return;
        }

        context->stencilOpSeparate(face, fail, zfail, zpass);
    }
}

void GL_APIENTRY TexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
                            GLint border, GLenum format, GLenum type, const GLvoid* pixels)
{
    EVENT("(GLenum target = 0x%X, GLint level = %d, GLint internalformat = %d, GLsizei width = %d, GLsizei height = %d, "
          "GLint border = %d, GLenum format = 0x%X, GLenum type = 0x%X, const GLvoid* pixels = 0x%0.8p)",
          target, level, internalformat, width, height, border, format, type, pixels);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateTexImage2D(context, target, level, internalformat, width, height, border,
                                format, type, pixels))
        {
            return;
        }

        context->texImage2D(target, level, internalformat, width, height, border, format, type,
                            pixels);
    }
}

void GL_APIENTRY TexParameterf(GLenum target, GLenum pname, GLfloat param)
{
    EVENT("(GLenum target = 0x%X, GLenum pname = 0x%X, GLint param = %f)", target, pname, param);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateTexParameterf(context, target, pname, param))
        {
            return;
        }

        context->texParameterf(target, pname, param);
    }
}

void GL_APIENTRY TexParameterfv(GLenum target, GLenum pname, const GLfloat *params)
{
    EVENT("(GLenum target = 0x%X, GLenum pname = 0x%X, const GLfloat* params = 0x%0.8p)", target,
          pname, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateTexParameterfv(context, target, pname, params))
        {
            return;
        }

        context->texParameterfv(target, pname, params);
    }
}

void GL_APIENTRY TexParameteri(GLenum target, GLenum pname, GLint param)
{
    EVENT("(GLenum target = 0x%X, GLenum pname = 0x%X, GLint param = %d)", target, pname, param);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateTexParameteri(context, target, pname, param))
        {
            return;
        }

        context->texParameteri(target, pname, param);
    }
}

void GL_APIENTRY TexParameteriv(GLenum target, GLenum pname, const GLint *params)
{
    EVENT("(GLenum target = 0x%X, GLenum pname = 0x%X, const GLint* params = 0x%0.8p)", target,
          pname, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateTexParameteriv(context, target, pname, params))
        {
            return;
        }

        context->texParameteriv(target, pname, params);
    }
}

void GL_APIENTRY TexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                               GLenum format, GLenum type, const GLvoid* pixels)
{
    EVENT("(GLenum target = 0x%X, GLint level = %d, GLint xoffset = %d, GLint yoffset = %d, "
          "GLsizei width = %d, GLsizei height = %d, GLenum format = 0x%X, GLenum type = 0x%X, "
          "const GLvoid* pixels = 0x%0.8p)",
           target, level, xoffset, yoffset, width, height, format, type, pixels);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateTexSubImage2D(context, target, level, xoffset, yoffset, width, height, format,
                                   type, pixels))
        {
            return;
        }

        context->texSubImage2D(target, level, xoffset, yoffset, width, height, format, type,
                               pixels);
    }
}

void GL_APIENTRY Uniform1f(GLint location, GLfloat x)
{
    EVENT("(GLint location = %d, GLfloat x = %f)", location, x);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateUniform1f(context, location, x))
        {
            return;
        }

        context->uniform1f(location, x);
    }
}

void GL_APIENTRY Uniform1fv(GLint location, GLsizei count, const GLfloat* v)
{
    EVENT("(GLint location = %d, GLsizei count = %d, const GLfloat* v = 0x%0.8p)", location, count, v);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateUniform1fv(context, location, count, v))
        {
            return;
        }

        context->uniform1fv(location, count, v);
    }
}

void GL_APIENTRY Uniform1i(GLint location, GLint x)
{
    EVENT("(GLint location = %d, GLint x = %d)", location, x);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateUniform1iv(context, location, 1, &x))
        {
            return;
        }

        context->uniform1i(location, x);
    }
}

void GL_APIENTRY Uniform1iv(GLint location, GLsizei count, const GLint* v)
{
    EVENT("(GLint location = %d, GLsizei count = %d, const GLint* v = 0x%0.8p)", location, count, v);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateUniform1iv(context, location, count, v))
        {
            return;
        }

        context->uniform1iv(location, count, v);
    }
}

void GL_APIENTRY Uniform2f(GLint location, GLfloat x, GLfloat y)
{
    EVENT("(GLint location = %d, GLfloat x = %f, GLfloat y = %f)", location, x, y);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateUniform2f(context, location, x, y))
        {
            return;
        }

        context->uniform2f(location, x, y);
    }
}

void GL_APIENTRY Uniform2fv(GLint location, GLsizei count, const GLfloat* v)
{
    EVENT("(GLint location = %d, GLsizei count = %d, const GLfloat* v = 0x%0.8p)", location, count, v);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateUniform2fv(context, location, count, v))
        {
            return;
        }

        context->uniform2fv(location, count, v);
    }
}

void GL_APIENTRY Uniform2i(GLint location, GLint x, GLint y)
{
    EVENT("(GLint location = %d, GLint x = %d, GLint y = %d)", location, x, y);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateUniform2i(context, location, x, y))
        {
            return;
        }

        context->uniform2i(location, x, y);
    }
}

void GL_APIENTRY Uniform2iv(GLint location, GLsizei count, const GLint* v)
{
    EVENT("(GLint location = %d, GLsizei count = %d, const GLint* v = 0x%0.8p)", location, count, v);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateUniform2iv(context, location, count, v))
        {
            return;
        }

        context->uniform2iv(location, count, v);
    }
}

void GL_APIENTRY Uniform3f(GLint location, GLfloat x, GLfloat y, GLfloat z)
{
    EVENT("(GLint location = %d, GLfloat x = %f, GLfloat y = %f, GLfloat z = %f)", location, x, y,
          z);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateUniform3f(context, location, x, y, z))
        {
            return;
        }

        context->uniform3f(location, x, y, z);
    }
}

void GL_APIENTRY Uniform3fv(GLint location, GLsizei count, const GLfloat* v)
{
    EVENT("(GLint location = %d, GLsizei count = %d, const GLfloat* v = 0x%0.8p)", location, count, v);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateUniform3fv(context, location, count, v))
        {
            return;
        }

        context->uniform3fv(location, count, v);
    }
}

void GL_APIENTRY Uniform3i(GLint location, GLint x, GLint y, GLint z)
{
    EVENT("(GLint location = %d, GLint x = %d, GLint y = %d, GLint z = %d)", location, x, y, z);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateUniform3i(context, location, x, y, z))
        {
            return;
        }

        context->uniform3i(location, x, y, z);
    }
}

void GL_APIENTRY Uniform3iv(GLint location, GLsizei count, const GLint* v)
{
    EVENT("(GLint location = %d, GLsizei count = %d, const GLint* v = 0x%0.8p)", location, count, v);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateUniform3iv(context, location, count, v))
        {
            return;
        }

        context->uniform3iv(location, count, v);
    }
}

void GL_APIENTRY Uniform4f(GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    EVENT("(GLint location = %d, GLfloat x = %f, GLfloat y = %f, GLfloat z = %f, GLfloat w = %f)",
          location, x, y, z, w);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateUniform4f(context, location, x, y, z, w))
        {
            return;
        }

        context->uniform4f(location, x, y, z, w);
    }
}

void GL_APIENTRY Uniform4fv(GLint location, GLsizei count, const GLfloat* v)
{
    EVENT("(GLint location = %d, GLsizei count = %d, const GLfloat* v = 0x%0.8p)", location, count, v);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateUniform4fv(context, location, count, v))
        {
            return;
        }

        context->uniform4fv(location, count, v);
    }
}

void GL_APIENTRY Uniform4i(GLint location, GLint x, GLint y, GLint z, GLint w)
{
    EVENT("(GLint location = %d, GLint x = %d, GLint y = %d, GLint z = %d, GLint w = %d)", location,
          x, y, z, w);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateUniform4i(context, location, x, y, z, w))
        {
            return;
        }

        context->uniform4i(location, x, y, z, w);
    }
}

void GL_APIENTRY Uniform4iv(GLint location, GLsizei count, const GLint* v)
{
    EVENT("(GLint location = %d, GLsizei count = %d, const GLint* v = 0x%0.8p)", location, count, v);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateUniform4iv(context, location, count, v))
        {
            return;
        }

        context->uniform4iv(location, count, v);
    }
}

void GL_APIENTRY UniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
    EVENT("(GLint location = %d, GLsizei count = %d, GLboolean transpose = %u, const GLfloat* value = 0x%0.8p)",
          location, count, transpose, value);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateUniformMatrix2fv(context, location, count, transpose, value))
        {
            return;
        }

        context->uniformMatrix2fv(location, count, transpose, value);
    }
}

void GL_APIENTRY UniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
    EVENT("(GLint location = %d, GLsizei count = %d, GLboolean transpose = %u, const GLfloat* value = 0x%0.8p)",
          location, count, transpose, value);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateUniformMatrix3fv(context, location, count, transpose, value))
        {
            return;
        }

        context->uniformMatrix3fv(location, count, transpose, value);
    }
}

void GL_APIENTRY UniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
    EVENT("(GLint location = %d, GLsizei count = %d, GLboolean transpose = %u, const GLfloat* value = 0x%0.8p)",
          location, count, transpose, value);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateUniformMatrix4fv(context, location, count, transpose, value))
        {
            return;
        }

        context->uniformMatrix4fv(location, count, transpose, value);
    }
}

void GL_APIENTRY UseProgram(GLuint program)
{
    EVENT("(GLuint program = %d)", program);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateUseProgram(context, program))
        {
            return;
        }

        context->useProgram(program);
    }
}

void GL_APIENTRY ValidateProgram(GLuint program)
{
    EVENT("(GLuint program = %d)", program);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateValidateProgram(context, program))
        {
            return;
        }

        context->validateProgram(program);
    }
}

void GL_APIENTRY VertexAttrib1f(GLuint index, GLfloat x)
{
    EVENT("(GLuint index = %d, GLfloat x = %f)", index, x);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateVertexAttrib1f(context, index, x))
        {
            return;
        }

        context->vertexAttrib1f(index, x);
    }
}

void GL_APIENTRY VertexAttrib1fv(GLuint index, const GLfloat* values)
{
    EVENT("(GLuint index = %d, const GLfloat* values = 0x%0.8p)", index, values);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateVertexAttrib1fv(context, index, values))
        {
            return;
        }

        context->vertexAttrib1fv(index, values);
    }
}

void GL_APIENTRY VertexAttrib2f(GLuint index, GLfloat x, GLfloat y)
{
    EVENT("(GLuint index = %d, GLfloat x = %f, GLfloat y = %f)", index, x, y);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateVertexAttrib2f(context, index, x, y))
        {
            return;
        }

        context->vertexAttrib2f(index, x, y);
    }
}

void GL_APIENTRY VertexAttrib2fv(GLuint index, const GLfloat* values)
{
    EVENT("(GLuint index = %d, const GLfloat* values = 0x%0.8p)", index, values);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateVertexAttrib2fv(context, index, values))
        {
            return;
        }

        context->vertexAttrib2fv(index, values);
    }
}

void GL_APIENTRY VertexAttrib3f(GLuint index, GLfloat x, GLfloat y, GLfloat z)
{
    EVENT("(GLuint index = %d, GLfloat x = %f, GLfloat y = %f, GLfloat z = %f)", index, x, y, z);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateVertexAttrib3f(context, index, x, y, z))
        {
            return;
        }

        context->vertexAttrib3f(index, x, y, z);
    }
}

void GL_APIENTRY VertexAttrib3fv(GLuint index, const GLfloat* values)
{
    EVENT("(GLuint index = %d, const GLfloat* values = 0x%0.8p)", index, values);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateVertexAttrib3fv(context, index, values))
        {
            return;
        }

        context->vertexAttrib3fv(index, values);
    }
}

void GL_APIENTRY VertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    EVENT("(GLuint index = %d, GLfloat x = %f, GLfloat y = %f, GLfloat z = %f, GLfloat w = %f)", index, x, y, z, w);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateVertexAttrib4f(context, index, x, y, z, w))
        {
            return;
        }

        context->vertexAttrib4f(index, x, y, z, w);
    }
}

void GL_APIENTRY VertexAttrib4fv(GLuint index, const GLfloat* values)
{
    EVENT("(GLuint index = %d, const GLfloat* values = 0x%0.8p)", index, values);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateVertexAttrib4fv(context, index, values))
        {
            return;
        }

        context->vertexAttrib4fv(index, values);
    }
}

void GL_APIENTRY VertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* ptr)
{
    EVENT("(GLuint index = %d, GLint size = %d, GLenum type = 0x%X, "
          "GLboolean normalized = %u, GLsizei stride = %d, const GLvoid* ptr = 0x%0.8p)",
          index, size, type, normalized, stride, ptr);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateVertexAttribPointer(context, index, size, type, normalized, stride, ptr))
        {
            return;
        }

        context->vertexAttribPointer(index, size, type, normalized, stride, ptr);
    }
}

void GL_APIENTRY Viewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
    EVENT("(GLint x = %d, GLint y = %d, GLsizei width = %d, GLsizei height = %d)", x, y, width, height);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateViewport(context, x, y, width, height))
        {
            return;
        }

        context->viewport(x, y, width, height);
    }
}

}  // namespace gl
