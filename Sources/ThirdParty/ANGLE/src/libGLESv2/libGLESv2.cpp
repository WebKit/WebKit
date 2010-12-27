//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// libGLESv2.cpp: Implements the exported OpenGL ES 2.0 functions.

#define GL_APICALL
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <exception>
#include <limits>

#include "common/debug.h"

#include "libGLESv2/main.h"
#include "libGLESv2/mathutil.h"
#include "libGLESv2/utilities.h"
#include "libGLESv2/Buffer.h"
#include "libGLESv2/Context.h"
#include "libGLESv2/Framebuffer.h"
#include "libGLESv2/Program.h"
#include "libGLESv2/Renderbuffer.h"
#include "libGLESv2/Shader.h"
#include "libGLESv2/Texture.h"

extern "C"
{

void __stdcall glActiveTexture(GLenum texture)
{
    TRACE("(GLenum texture = 0x%X)", texture);

    try
    {
        if (texture < GL_TEXTURE0 || texture > GL_TEXTURE0 + gl::MAX_TEXTURE_IMAGE_UNITS - 1)
        {
            return error(GL_INVALID_ENUM);
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            context->setActiveSampler(texture - GL_TEXTURE0);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glAttachShader(GLuint program, GLuint shader)
{
    TRACE("(GLuint program = %d, GLuint shader = %d)", program, shader);

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            gl::Program *programObject = context->getProgram(program);
            gl::Shader *shaderObject = context->getShader(shader);

            if (!programObject)
            {
                if (context->getShader(program))
                {
                    return error(GL_INVALID_OPERATION);
                }
                else
                {
                    return error(GL_INVALID_VALUE);
                }
            }

            if (!shaderObject)
            {
                if (context->getProgram(shader))
                {
                    return error(GL_INVALID_OPERATION);
                }
                else
                {
                    return error(GL_INVALID_VALUE);
                }
            }

            if (!programObject->attachShader(shaderObject))
            {
                return error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glBindAttribLocation(GLuint program, GLuint index, const GLchar* name)
{
    TRACE("(GLuint program = %d, GLuint index = %d, const GLchar* name = 0x%0.8p)", program, index, name);

    try
    {
        if (index >= gl::MAX_VERTEX_ATTRIBS)
        {
            return error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            gl::Program *programObject = context->getProgram(program);

            if (!programObject)
            {
                if (context->getShader(program))
                {
                    return error(GL_INVALID_OPERATION);
                }
                else
                {
                    return error(GL_INVALID_VALUE);
                }
            }

            if (strncmp(name, "gl_", 3) == 0)
            {
                return error(GL_INVALID_OPERATION);
            }

            programObject->bindAttributeLocation(index, name);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glBindBuffer(GLenum target, GLuint buffer)
{
    TRACE("(GLenum target = 0x%X, GLuint buffer = %d)", target, buffer);

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            switch (target)
            {
              case GL_ARRAY_BUFFER:
                context->bindArrayBuffer(buffer);
                return;
              case GL_ELEMENT_ARRAY_BUFFER:
                context->bindElementArrayBuffer(buffer);
                return;
              default:
                return error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glBindFramebuffer(GLenum target, GLuint framebuffer)
{
    TRACE("(GLenum target = 0x%X, GLuint framebuffer = %d)", target, framebuffer);

    try
    {
        if (target != GL_FRAMEBUFFER && target != GL_DRAW_FRAMEBUFFER_ANGLE && target != GL_READ_FRAMEBUFFER_ANGLE)
        {
            return error(GL_INVALID_ENUM);
        }

        gl::Context *context = gl::getContext();

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
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glBindRenderbuffer(GLenum target, GLuint renderbuffer)
{
    TRACE("(GLenum target = 0x%X, GLuint renderbuffer = %d)", target, renderbuffer);

    try
    {
        if (target != GL_RENDERBUFFER)
        {
            return error(GL_INVALID_ENUM);
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            context->bindRenderbuffer(renderbuffer);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glBindTexture(GLenum target, GLuint texture)
{
    TRACE("(GLenum target = 0x%X, GLuint texture = %d)", target, texture);

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            gl::Texture *textureObject = context->getTexture(texture);

            if (textureObject && textureObject->getTarget() != target && texture != 0)
            {
                return error(GL_INVALID_OPERATION);
            }

            switch (target)
            {
              case GL_TEXTURE_2D:
                context->bindTexture2D(texture);
                return;
              case GL_TEXTURE_CUBE_MAP:
                context->bindTextureCubeMap(texture);
                return;
              default:
                return error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glBlendColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
    TRACE("(GLclampf red = %f, GLclampf green = %f, GLclampf blue = %f, GLclampf alpha = %f)",
          red, green, blue, alpha);

    try
    {
        gl::Context* context = gl::getContext();

        if (context)
        {
            context->setBlendColor(gl::clamp01(red), gl::clamp01(green), gl::clamp01(blue), gl::clamp01(alpha));
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glBlendEquation(GLenum mode)
{
    glBlendEquationSeparate(mode, mode);
}

void __stdcall glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha)
{
    TRACE("(GLenum modeRGB = 0x%X, GLenum modeAlpha = 0x%X)", modeRGB, modeAlpha);

    try
    {
        switch (modeRGB)
        {
          case GL_FUNC_ADD:
          case GL_FUNC_SUBTRACT:
          case GL_FUNC_REVERSE_SUBTRACT:
            break;
          default:
            return error(GL_INVALID_ENUM);
        }

        switch (modeAlpha)
        {
          case GL_FUNC_ADD:
          case GL_FUNC_SUBTRACT:
          case GL_FUNC_REVERSE_SUBTRACT:
            break;
          default:
            return error(GL_INVALID_ENUM);
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            context->setBlendEquation(modeRGB, modeAlpha);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glBlendFunc(GLenum sfactor, GLenum dfactor)
{
    glBlendFuncSeparate(sfactor, dfactor, sfactor, dfactor);
}

void __stdcall glBlendFuncSeparate(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha)
{
    TRACE("(GLenum srcRGB = 0x%X, GLenum dstRGB = 0x%X, GLenum srcAlpha = 0x%X, GLenum dstAlpha = 0x%X)",
          srcRGB, dstRGB, srcAlpha, dstAlpha);

    try
    {
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
            return error(GL_INVALID_ENUM);
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
          default:
            return error(GL_INVALID_ENUM);
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
            return error(GL_INVALID_ENUM);
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
          default:
            return error(GL_INVALID_ENUM);
        }

        bool constantColorUsed = (srcRGB == GL_CONSTANT_COLOR || srcRGB == GL_ONE_MINUS_CONSTANT_COLOR ||
                                  dstRGB == GL_CONSTANT_COLOR || dstRGB == GL_ONE_MINUS_CONSTANT_COLOR);

        bool constantAlphaUsed = (srcRGB == GL_CONSTANT_ALPHA || srcRGB == GL_ONE_MINUS_CONSTANT_ALPHA ||
                                  dstRGB == GL_CONSTANT_ALPHA || dstRGB == GL_ONE_MINUS_CONSTANT_ALPHA);

        if (constantColorUsed && constantAlphaUsed)
        {
            ERR("Simultaneous use of GL_CONSTANT_ALPHA/GL_ONE_MINUS_CONSTANT_ALPHA and GL_CONSTANT_COLOR/GL_ONE_MINUS_CONSTANT_COLOR invalid under WebGL");
            return error(GL_INVALID_OPERATION);
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            context->setBlendFactors(srcRGB, dstRGB, srcAlpha, dstAlpha);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glBufferData(GLenum target, GLsizeiptr size, const GLvoid* data, GLenum usage)
{
    TRACE("(GLenum target = 0x%X, GLsizeiptr size = %d, const GLvoid* data = 0x%0.8p, GLenum usage = %d)",
          target, size, data, usage);

    try
    {
        if (size < 0)
        {
            return error(GL_INVALID_VALUE);
        }

        switch (usage)
        {
          case GL_STREAM_DRAW:
          case GL_STATIC_DRAW:
          case GL_DYNAMIC_DRAW:
            break;
          default:
            return error(GL_INVALID_ENUM);
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            gl::Buffer *buffer;

            switch (target)
            {
              case GL_ARRAY_BUFFER:
                buffer = context->getArrayBuffer();
                break;
              case GL_ELEMENT_ARRAY_BUFFER:
                buffer = context->getElementArrayBuffer();
                break;
              default:
                return error(GL_INVALID_ENUM);
            }

            if (!buffer)
            {
                return error(GL_INVALID_OPERATION);
            }

            buffer->bufferData(data, size, usage);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid* data)
{
    TRACE("(GLenum target = 0x%X, GLintptr offset = %d, GLsizeiptr size = %d, const GLvoid* data = 0x%0.8p)",
          target, offset, size, data);

    try
    {
        if (size < 0 || offset < 0)
        {
            return error(GL_INVALID_VALUE);
        }

        if (data == NULL)
        {
            return;
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            gl::Buffer *buffer;

            switch (target)
            {
              case GL_ARRAY_BUFFER:
                buffer = context->getArrayBuffer();
                break;
              case GL_ELEMENT_ARRAY_BUFFER:
                buffer = context->getElementArrayBuffer();
                break;
              default:
                return error(GL_INVALID_ENUM);
            }

            if (!buffer)
            {
                return error(GL_INVALID_OPERATION);
            }

            if ((size_t)size + offset > buffer->size())
            {
                return error(GL_INVALID_VALUE);
            }

            buffer->bufferSubData(data, size, offset);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

GLenum __stdcall glCheckFramebufferStatus(GLenum target)
{
    TRACE("(GLenum target = 0x%X)", target);

    try
    {
        if (target != GL_FRAMEBUFFER && target != GL_DRAW_FRAMEBUFFER_ANGLE && target != GL_READ_FRAMEBUFFER_ANGLE)
        {
            return error(GL_INVALID_ENUM, 0);
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            gl::Framebuffer *framebuffer = NULL;
            if (target == GL_READ_FRAMEBUFFER_ANGLE)
            {
                framebuffer = context->getReadFramebuffer();
            }
            else
            {
                framebuffer = context->getDrawFramebuffer();
            }

            return framebuffer->completeness();
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY, 0);
    }

    return 0;
}

void __stdcall glClear(GLbitfield mask)
{
    TRACE("(GLbitfield mask = %X)", mask);

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            context->clear(mask);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
    TRACE("(GLclampf red = %f, GLclampf green = %f, GLclampf blue = %f, GLclampf alpha = %f)",
          red, green, blue, alpha);

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            context->setClearColor(red, green, blue, alpha);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glClearDepthf(GLclampf depth)
{
    TRACE("(GLclampf depth = %f)", depth);

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            context->setClearDepth(depth);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glClearStencil(GLint s)
{
    TRACE("(GLint s = %d)", s);

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            context->setClearStencil(s);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
    TRACE("(GLboolean red = %d, GLboolean green = %d, GLboolean blue = %d, GLboolean alpha = %d)",
          red, green, blue, alpha);

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            context->setColorMask(red == GL_TRUE, green == GL_TRUE, blue == GL_TRUE, alpha == GL_TRUE);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glCompileShader(GLuint shader)
{
    TRACE("(GLuint shader = %d)", shader);

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            gl::Shader *shaderObject = context->getShader(shader);

            if (!shaderObject)
            {
                if (context->getProgram(shader))
                {
                    return error(GL_INVALID_OPERATION);
                }
                else
                {
                    return error(GL_INVALID_VALUE);
                }
            }

            shaderObject->compile();
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, 
                                      GLint border, GLsizei imageSize, const GLvoid* data)
{
    TRACE("(GLenum target = 0x%X, GLint level = %d, GLenum internalformat = 0x%X, GLsizei width = %d, " 
          "GLsizei height = %d, GLint border = %d, GLsizei imageSize = %d, const GLvoid* data = 0x%0.8p)",
          target, level, internalformat, width, height, border, imageSize, data);

    try
    {
        if (level < 0 || level > gl::MAX_TEXTURE_LEVELS)
        {
            return error(GL_INVALID_VALUE);
        }

        if (width < 0 || height < 0 || (level > 0 && !gl::isPow2(width)) || (level > 0 && !gl::isPow2(height)) || border != 0 || imageSize < 0)
        {
            return error(GL_INVALID_VALUE);
        }

        switch (target)
        {
          case GL_TEXTURE_2D:
            if (width > (gl::MAX_TEXTURE_SIZE >> level) || height > (gl::MAX_TEXTURE_SIZE >> level))
            {
                return error(GL_INVALID_VALUE);
            }
            break;
          case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
          case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
          case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
          case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
          case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
          case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
            if (width != height)
            {
                return error(GL_INVALID_VALUE);
            }

            if (width > (gl::MAX_CUBE_MAP_TEXTURE_SIZE >> level) || height > (gl::MAX_CUBE_MAP_TEXTURE_SIZE >> level))
            {
                return error(GL_INVALID_VALUE);
            }
            break;
          default:
            return error(GL_INVALID_ENUM);
        }

        switch (internalformat)
        {
          case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
          case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
            break;
          default:
            return error(GL_INVALID_ENUM);
        }

        if (border != 0)
        {
            return error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            if (!context->supportsCompressedTextures())
            {
                return error(GL_INVALID_ENUM); // in this case, it's as though the internal format switch failed
            }

            if (imageSize != gl::ComputeCompressedSize(width, height, internalformat))
            {
                return error(GL_INVALID_VALUE);
            }

            if (target == GL_TEXTURE_2D)
            {
                gl::Texture2D *texture = context->getTexture2D();

                if (!texture)
                {
                    return error(GL_INVALID_OPERATION);
                }

                texture->setCompressedImage(level, internalformat, width, height, imageSize, data);
            }
            else
            {
                gl::TextureCubeMap *texture = context->getTextureCubeMap();

                if (!texture)
                {
                    return error(GL_INVALID_OPERATION);
                }

                switch (target)
                {
                  case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
                  case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
                  case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
                  case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
                  case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
                  case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
                    texture->setCompressedImage(target, level, internalformat, width, height, imageSize, data);
                    break;
                  default: UNREACHABLE();
                }
            }
        }

    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                         GLenum format, GLsizei imageSize, const GLvoid* data)
{
    TRACE("(GLenum target = 0x%X, GLint level = %d, GLint xoffset = %d, GLint yoffset = %d, "
          "GLsizei width = %d, GLsizei height = %d, GLenum format = 0x%X, "
          "GLsizei imageSize = %d, const GLvoid* data = 0x%0.8p)",
          target, level, xoffset, yoffset, width, height, format, imageSize, data);

    try
    {
        if (!gl::IsTextureTarget(target))
        {
            return error(GL_INVALID_ENUM);
        }

        if (level < 0 || level > gl::MAX_TEXTURE_LEVELS)
        {
            return error(GL_INVALID_VALUE);
        }

        if (xoffset < 0 || yoffset < 0 || width < 0 || height < 0 || 
            (level > 0 && !gl::isPow2(width)) || (level > 0 && !gl::isPow2(height)) || imageSize < 0)
        {
            return error(GL_INVALID_VALUE);
        }

        switch (format)
        {
          case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
          case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
            break;
          default:
            return error(GL_INVALID_ENUM);
        }

        if (width == 0 || height == 0 || data == NULL)
        {
            return;
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            if (!context->supportsCompressedTextures())
            {
                return error(GL_INVALID_ENUM); // in this case, it's as though the format switch has failed.
            }

            if (imageSize != gl::ComputeCompressedSize(width, height, format))
            {
                return error(GL_INVALID_VALUE);
            }

            if (xoffset % 4 != 0 || yoffset % 4 != 0)
            {
                return error(GL_INVALID_OPERATION); // we wait to check the offsets until this point, because the multiple-of-four restriction
                                                    // does not exist unless DXT1 textures are supported.
            }

            if (target == GL_TEXTURE_2D)
            {
                gl::Texture2D *texture = context->getTexture2D();

                if (!texture)
                {
                    return error(GL_INVALID_OPERATION);
                }

                if (!texture->isCompressed())
                {
                    return error(GL_INVALID_OPERATION);
                }

                if ((width % 4 != 0 && width != texture->getWidth()) || 
                    (height % 4 != 0 && height != texture->getHeight()))
                {
                    return error(GL_INVALID_OPERATION);
                }

                texture->subImageCompressed(level, xoffset, yoffset, width, height, format, imageSize, data);
            }
            else if (gl::IsCubemapTextureTarget(target))
            {
                gl::TextureCubeMap *texture = context->getTextureCubeMap();

                if (!texture)
                {
                    return error(GL_INVALID_OPERATION);
                }

                if (!texture->isCompressed())
                {
                    return error(GL_INVALID_OPERATION);
                }

                if ((width % 4 != 0 && width != texture->getWidth()) || 
                    (height % 4 != 0 && height != texture->getHeight()))
                {
                    return error(GL_INVALID_OPERATION);
                }

                texture->subImageCompressed(target, level, xoffset, yoffset, width, height, format, imageSize, data);
            }
            else
            {
                UNREACHABLE();
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border)
{
    TRACE("(GLenum target = 0x%X, GLint level = %d, GLenum internalformat = 0x%X, "
          "GLint x = %d, GLint y = %d, GLsizei width = %d, GLsizei height = %d, GLint border = %d)",
          target, level, internalformat, x, y, width, height, border);

    try
    {
        if (level < 0 || width < 0 || height < 0)
        {
            return error(GL_INVALID_VALUE);
        }

        if (level > 0 && (!gl::isPow2(width) || !gl::isPow2(height)))
        {
            return error(GL_INVALID_VALUE);
        }

        switch (target)
        {
          case GL_TEXTURE_2D:
            if (width > (gl::MAX_TEXTURE_SIZE >> level) || height > (gl::MAX_TEXTURE_SIZE >> level))
            {
                return error(GL_INVALID_VALUE);
            }
            break;
          case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
          case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
          case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
          case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
          case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
          case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
            if (width != height)
            {
                return error(GL_INVALID_VALUE);
            }

            if (width > (gl::MAX_CUBE_MAP_TEXTURE_SIZE >> level) || height > (gl::MAX_CUBE_MAP_TEXTURE_SIZE >> level))
            {
                return error(GL_INVALID_VALUE);
            }
            break;
          default:
            return error(GL_INVALID_ENUM);
        }

        switch (internalformat)
        {
          case GL_ALPHA:
          case GL_LUMINANCE:
          case GL_LUMINANCE_ALPHA:
          case GL_RGB:
          case GL_RGBA:
          case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:  // Compressed textures are not supported here, but if they are unsupported altogether,
          case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT: // a different error is generated than otherwise. That is handled below.
            break;
          default:
            return error(GL_INVALID_VALUE);
        }

        if (border != 0)
        {
            return error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            if (internalformat == GL_COMPRESSED_RGB_S3TC_DXT1_EXT || 
                internalformat == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT)
            {
                if (context->supportsCompressedTextures())
                {
                    return error(GL_INVALID_OPERATION);
                }
                else
                {
                    return error(GL_INVALID_ENUM);
                }
            }

            gl::Framebuffer *framebuffer = context->getReadFramebuffer();
            if (framebuffer->completeness() != GL_FRAMEBUFFER_COMPLETE)
            {
                return error(GL_INVALID_FRAMEBUFFER_OPERATION);
            }

            if (context->getReadFramebufferHandle() != 0 && framebuffer->getColorbuffer()->getSamples() != 0)
            {
                return error(GL_INVALID_OPERATION);
            }

            gl::Colorbuffer *source = framebuffer->getColorbuffer();
            if (target == GL_TEXTURE_2D)
            {
                gl::Texture2D *texture = context->getTexture2D();

                if (!texture)
                {
                    return error(GL_INVALID_OPERATION);
                }
                
                if (texture->isCompressed())
                {
                    return error(GL_INVALID_OPERATION);
                }

                texture->copyImage(level, internalformat, x, y, width, height, source);
            }
            else if (gl::IsCubemapTextureTarget(target))
            {
                gl::TextureCubeMap *texture = context->getTextureCubeMap();

                if (!texture)
                {
                    return error(GL_INVALID_OPERATION);
                }

                if (texture->isCompressed())
                {
                    return error(GL_INVALID_OPERATION);
                }

                texture->copyImage(target, level, internalformat, x, y, width, height, source);
            }
            else
            {
                UNREACHABLE();
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
    TRACE("(GLenum target = 0x%X, GLint level = %d, GLint xoffset = %d, GLint yoffset = %d, "
          "GLint x = %d, GLint y = %d, GLsizei width = %d, GLsizei height = %d)",
          target, level, xoffset, yoffset, x, y, width, height);

    try
    {
        if (!gl::IsTextureTarget(target))
        {
            return error(GL_INVALID_ENUM);
        }

        if (level < 0 || level > gl::MAX_TEXTURE_LEVELS || xoffset < 0 || yoffset < 0 || width < 0 || height < 0)
        {
            return error(GL_INVALID_VALUE);
        }

        if (std::numeric_limits<GLsizei>::max() - xoffset < width || std::numeric_limits<GLsizei>::max() - yoffset < height)
        {
            return error(GL_INVALID_VALUE);
        }

        if (width == 0 || height == 0)
        {
            return;
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            gl::Framebuffer *framebuffer = context->getReadFramebuffer();
            if (framebuffer->completeness() != GL_FRAMEBUFFER_COMPLETE)
            {
                return error(GL_INVALID_FRAMEBUFFER_OPERATION);
            }

            if (context->getReadFramebufferHandle() != 0 && framebuffer->getColorbuffer()->getSamples() != 0)
            {
                return error(GL_INVALID_OPERATION);
            }

            gl::Colorbuffer *source = framebuffer->getColorbuffer();
            if (target == GL_TEXTURE_2D)
            {
                gl::Texture2D *texture = context->getTexture2D();

                if (!texture)
                {
                    return error(GL_INVALID_OPERATION);
                }

                if (texture->isCompressed())
                {
                    return error(GL_INVALID_OPERATION);
                }

                texture->copySubImage(level, xoffset, yoffset, x, y, width, height, source);
            }
            else if (gl::IsCubemapTextureTarget(target))
            {
                gl::TextureCubeMap *texture = context->getTextureCubeMap();

                if (!texture)
                {
                    return error(GL_INVALID_OPERATION);
                }

                if (texture->isCompressed())
                {
                    return error(GL_INVALID_OPERATION);
                }

                texture->copySubImage(target, level, xoffset, yoffset, x, y, width, height, source);
            }
            else
            {
                UNREACHABLE();
            }
        }
    }

    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

GLuint __stdcall glCreateProgram(void)
{
    TRACE("()");

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            return context->createProgram();
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY, 0);
    }

    return 0;
}

GLuint __stdcall glCreateShader(GLenum type)
{
    TRACE("(GLenum type = 0x%X)", type);

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            switch (type)
            {
              case GL_FRAGMENT_SHADER:
              case GL_VERTEX_SHADER:
                return context->createShader(type);
              default:
                return error(GL_INVALID_ENUM, 0);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY, 0);
    }

    return 0;
}

void __stdcall glCullFace(GLenum mode)
{
    TRACE("(GLenum mode = 0x%X)", mode);

    try
    {
        switch (mode)
        {
          case GL_FRONT:
          case GL_BACK:
          case GL_FRONT_AND_BACK:
            {
                gl::Context *context = gl::getContext();

                if (context)
                {
                    context->setCullMode(mode);
                }
            }
            break;
          default:
            return error(GL_INVALID_ENUM);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDeleteBuffers(GLsizei n, const GLuint* buffers)
{
    TRACE("(GLsizei n = %d, const GLuint* buffers = 0x%0.8p)", n, buffers);

    try
    {
        if (n < 0)
        {
            return error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getContext();

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
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDeleteFramebuffers(GLsizei n, const GLuint* framebuffers)
{
    TRACE("(GLsizei n = %d, const GLuint* framebuffers = 0x%0.8p)", n, framebuffers);

    try
    {
        if (n < 0)
        {
            return error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getContext();

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
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDeleteProgram(GLuint program)
{
    TRACE("(GLuint program = %d)", program);

    try
    {
        if (program == 0)
        {
            return;
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            if (!context->getProgram(program))
            {
                if(context->getShader(program))
                {
                    return error(GL_INVALID_OPERATION);
                }
                else
                {
                    return error(GL_INVALID_VALUE);
                }
            }

            context->deleteProgram(program);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers)
{
    TRACE("(GLsizei n = %d, const GLuint* renderbuffers = 0x%0.8p)", n, renderbuffers);

    try
    {
        if (n < 0)
        {
            return error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getContext();

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
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDeleteShader(GLuint shader)
{
    TRACE("(GLuint shader = %d)", shader);

    try
    {
        if (shader == 0)
        {
            return;
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            if (!context->getShader(shader))
            {
                if(context->getProgram(shader))
                {
                    return error(GL_INVALID_OPERATION);
                }
                else
                {
                    return error(GL_INVALID_VALUE);
                }
            }

            context->deleteShader(shader);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDeleteTextures(GLsizei n, const GLuint* textures)
{
    TRACE("(GLsizei n = %d, const GLuint* textures = 0x%0.8p)", n, textures);

    try
    {
        if (n < 0)
        {
            return error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getContext();

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
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDepthFunc(GLenum func)
{
    TRACE("(GLenum func = 0x%X)", func);

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
            return error(GL_INVALID_ENUM);
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            context->setDepthFunc(func);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDepthMask(GLboolean flag)
{
    TRACE("(GLboolean flag = %d)", flag);

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            context->setDepthMask(flag != GL_FALSE);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDepthRangef(GLclampf zNear, GLclampf zFar)
{
    TRACE("(GLclampf zNear = %f, GLclampf zFar = %f)", zNear, zFar);

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            context->setDepthRange(zNear, zFar);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDetachShader(GLuint program, GLuint shader)
{
    TRACE("(GLuint program = %d, GLuint shader = %d)", program, shader);

    try
    {
        gl::Context *context = gl::getContext();

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
                    return error(GL_INVALID_VALUE);
                }
                else
                {
                    return error(GL_INVALID_OPERATION);
                }
            }

            if (!shaderObject)
            {
                gl::Program *programByShaderHandle = context->getProgram(shader);
                if (!programByShaderHandle)
                {
                    return error(GL_INVALID_VALUE);
                }
                else
                {
                    return error(GL_INVALID_OPERATION);
                }
            }

            if (!programObject->detachShader(shaderObject))
            {
                return error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDisable(GLenum cap)
{
    TRACE("(GLenum cap = 0x%X)", cap);

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            switch (cap)
            {
              case GL_CULL_FACE:                context->setCullFace(false);              break;
              case GL_POLYGON_OFFSET_FILL:      context->setPolygonOffsetFill(false);     break;
              case GL_SAMPLE_ALPHA_TO_COVERAGE: context->setSampleAlphaToCoverage(false); break;
              case GL_SAMPLE_COVERAGE:          context->setSampleCoverage(false);        break;
              case GL_SCISSOR_TEST:             context->setScissorTest(false);           break;
              case GL_STENCIL_TEST:             context->setStencilTest(false);           break;
              case GL_DEPTH_TEST:               context->setDepthTest(false);             break;
              case GL_BLEND:                    context->setBlend(false);                 break;
              case GL_DITHER:                   context->setDither(false);                break;
              default:
                return error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDisableVertexAttribArray(GLuint index)
{
    TRACE("(GLuint index = %d)", index);

    try
    {
        if (index >= gl::MAX_VERTEX_ATTRIBS)
        {
            return error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            context->setVertexAttribEnabled(index, false);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
    TRACE("(GLenum mode = 0x%X, GLint first = %d, GLsizei count = %d)", mode, first, count);

    try
    {
        if (count < 0 || first < 0)
        {
            return error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            context->drawArrays(mode, first, count);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices)
{
    TRACE("(GLenum mode = 0x%X, GLsizei count = %d, GLenum type = 0x%X, const GLvoid* indices = 0x%0.8p)",
          mode, count, type, indices);

    try
    {
        if (count < 0)
        {
            return error(GL_INVALID_VALUE);
        }

        switch (type)
        {
          case GL_UNSIGNED_BYTE:
          case GL_UNSIGNED_SHORT:
          case GL_UNSIGNED_INT:
            break;
          default:
            return error(GL_INVALID_ENUM);
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            context->drawElements(mode, count, type, indices);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glEnable(GLenum cap)
{
    TRACE("(GLenum cap = 0x%X)", cap);

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            switch (cap)
            {
              case GL_CULL_FACE:                context->setCullFace(true);              break;
              case GL_POLYGON_OFFSET_FILL:      context->setPolygonOffsetFill(true);     break;
              case GL_SAMPLE_ALPHA_TO_COVERAGE: context->setSampleAlphaToCoverage(true); break;
              case GL_SAMPLE_COVERAGE:          context->setSampleCoverage(true);        break;
              case GL_SCISSOR_TEST:             context->setScissorTest(true);           break;
              case GL_STENCIL_TEST:             context->setStencilTest(true);           break;
              case GL_DEPTH_TEST:               context->setDepthTest(true);             break;
              case GL_BLEND:                    context->setBlend(true);                 break;
              case GL_DITHER:                   context->setDither(true);                break;
              default:
                return error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glEnableVertexAttribArray(GLuint index)
{
    TRACE("(GLuint index = %d)", index);

    try
    {
        if (index >= gl::MAX_VERTEX_ATTRIBS)
        {
            return error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            context->setVertexAttribEnabled(index, true);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glFinish(void)
{
    TRACE("()");

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            context->finish();
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glFlush(void)
{
    TRACE("()");

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            context->flush();
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
    TRACE("(GLenum target = 0x%X, GLenum attachment = 0x%X, GLenum renderbuffertarget = 0x%X, "
          "GLuint renderbuffer = %d)", target, attachment, renderbuffertarget, renderbuffer);

    try
    {
        if ((target != GL_FRAMEBUFFER && target != GL_DRAW_FRAMEBUFFER_ANGLE && target != GL_READ_FRAMEBUFFER_ANGLE)
            || renderbuffertarget != GL_RENDERBUFFER)
        {
            return error(GL_INVALID_ENUM);
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            gl::Framebuffer *framebuffer = NULL;
            GLuint framebufferHandle = 0;
            if (target == GL_READ_FRAMEBUFFER_ANGLE)
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
                return error(GL_INVALID_OPERATION);
            }

            switch (attachment)
            {
              case GL_COLOR_ATTACHMENT0:
                framebuffer->setColorbuffer(GL_RENDERBUFFER, renderbuffer);
                break;
              case GL_DEPTH_ATTACHMENT:
                framebuffer->setDepthbuffer(GL_RENDERBUFFER, renderbuffer);
                break;
              case GL_STENCIL_ATTACHMENT:
                framebuffer->setStencilbuffer(GL_RENDERBUFFER, renderbuffer);
                break;
              default:
                return error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
    TRACE("(GLenum target = 0x%X, GLenum attachment = 0x%X, GLenum textarget = 0x%X, "
          "GLuint texture = %d, GLint level = %d)", target, attachment, textarget, texture, level);

    try
    {
        if (target != GL_FRAMEBUFFER && target != GL_DRAW_FRAMEBUFFER_ANGLE && target != GL_READ_FRAMEBUFFER_ANGLE)
        {
            return error(GL_INVALID_ENUM);
        }

        switch (attachment)
        {
          case GL_COLOR_ATTACHMENT0:
          case GL_DEPTH_ATTACHMENT:
          case GL_STENCIL_ATTACHMENT:
            break;
          default:
            return error(GL_INVALID_ENUM);
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            if (texture == 0)
            {
                textarget = GL_NONE;
            }
            else
            {
                gl::Texture *tex = context->getTexture(texture);

                if (tex == NULL)
                {
                    return error(GL_INVALID_OPERATION);
                }

                if (tex->isCompressed())
                {
                    return error(GL_INVALID_OPERATION);
                }

                switch (textarget)
                {
                  case GL_TEXTURE_2D:
                    if (tex->getTarget() != GL_TEXTURE_2D)
                    {
                        return error(GL_INVALID_OPERATION);
                    }
                    break;

                  case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
                  case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
                  case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
                  case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
                  case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
                  case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
                    if (tex->getTarget() != GL_TEXTURE_CUBE_MAP)
                    {
                        return error(GL_INVALID_OPERATION);
                    }
                    break;

                  default:
                    return error(GL_INVALID_ENUM);
                }

                if (level != 0)
                {
                    return error(GL_INVALID_VALUE);
                }
            }

            gl::Framebuffer *framebuffer = NULL;
            GLuint framebufferHandle = 0;
            if (target == GL_READ_FRAMEBUFFER_ANGLE)
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
                return error(GL_INVALID_OPERATION);
            }

            switch (attachment)
            {
              case GL_COLOR_ATTACHMENT0:  framebuffer->setColorbuffer(textarget, texture);   break;
              case GL_DEPTH_ATTACHMENT:   framebuffer->setDepthbuffer(textarget, texture);   break;
              case GL_STENCIL_ATTACHMENT: framebuffer->setStencilbuffer(textarget, texture); break;
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glFrontFace(GLenum mode)
{
    TRACE("(GLenum mode = 0x%X)", mode);

    try
    {
        switch (mode)
        {
          case GL_CW:
          case GL_CCW:
            {
                gl::Context *context = gl::getContext();

                if (context)
                {
                    context->setFrontFace(mode);
                }
            }
            break;
          default:
            return error(GL_INVALID_ENUM);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGenBuffers(GLsizei n, GLuint* buffers)
{
    TRACE("(GLsizei n = %d, GLuint* buffers = 0x%0.8p)", n, buffers);

    try
    {
        if (n < 0)
        {
            return error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getContext();

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
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGenerateMipmap(GLenum target)
{
    TRACE("(GLenum target = 0x%X)", target);

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            gl::Texture *texture;

            switch (target)
            {
              case GL_TEXTURE_2D:
                texture = context->getTexture2D();
                break;

              case GL_TEXTURE_CUBE_MAP:
                texture = context->getTextureCubeMap();
                break;

              default:
                return error(GL_INVALID_ENUM);
            }

            if (texture->isCompressed())
            {
                return error(GL_INVALID_OPERATION);
            }

            texture->generateMipmaps();
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGenFramebuffers(GLsizei n, GLuint* framebuffers)
{
    TRACE("(GLsizei n = %d, GLuint* framebuffers = 0x%0.8p)", n, framebuffers);

    try
    {
        if (n < 0)
        {
            return error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getContext();

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
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGenRenderbuffers(GLsizei n, GLuint* renderbuffers)
{
    TRACE("(GLsizei n = %d, GLuint* renderbuffers = 0x%0.8p)", n, renderbuffers);

    try
    {
        if (n < 0)
        {
            return error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getContext();

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
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGenTextures(GLsizei n, GLuint* textures)
{
    TRACE("(GLsizei n = %d, GLuint* textures =  0x%0.8p)", n, textures);

    try
    {
        if (n < 0)
        {
            return error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getContext();

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
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetActiveAttrib(GLuint program, GLuint index, GLsizei bufsize, GLsizei *length, GLint *size, GLenum *type, GLchar *name)
{
    TRACE("(GLuint program = %d, GLuint index = %d, GLsizei bufsize = %d, GLsizei *length = 0x%0.8p, "
          "GLint *size = 0x%0.8p, GLenum *type = %0.8p, GLchar *name = %0.8p)",
          program, index, bufsize, length, size, type, name);

    try
    {
        if (bufsize < 0)
        {
            return error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            gl::Program *programObject = context->getProgram(program);

            if (!programObject)
            {
                if (context->getShader(program))
                {
                    return error(GL_INVALID_OPERATION);
                }
                else
                {
                    return error(GL_INVALID_VALUE);
                }
            }

            if (index >= (GLuint)programObject->getActiveAttributeCount())
            {
                return error(GL_INVALID_VALUE);
            }

            programObject->getActiveAttribute(index, bufsize, length, size, type, name);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetActiveUniform(GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size, GLenum* type, GLchar* name)
{
    TRACE("(GLuint program = %d, GLuint index = %d, GLsizei bufsize = %d, "
          "GLsizei* length = 0x%0.8p, GLint* size = 0x%0.8p, GLenum* type = 0x%0.8p, GLchar* name = 0x%0.8p)",
          program, index, bufsize, length, size, type, name);

    try
    {
        if (bufsize < 0)
        {
            return error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            gl::Program *programObject = context->getProgram(program);

            if (!programObject)
            {
                if (context->getShader(program))
                {
                    return error(GL_INVALID_OPERATION);
                }
                else
                {
                    return error(GL_INVALID_VALUE);
                }
            }

            if (index >= (GLuint)programObject->getActiveUniformCount())
            {
                return error(GL_INVALID_VALUE);
            }

            programObject->getActiveUniform(index, bufsize, length, size, type, name);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetAttachedShaders(GLuint program, GLsizei maxcount, GLsizei* count, GLuint* shaders)
{
    TRACE("(GLuint program = %d, GLsizei maxcount = %d, GLsizei* count = 0x%0.8p, GLuint* shaders = 0x%0.8p)",
          program, maxcount, count, shaders);

    try
    {
        if (maxcount < 0)
        {
            return error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            gl::Program *programObject = context->getProgram(program);

            if (!programObject)
            {
                if (context->getShader(program))
                {
                    return error(GL_INVALID_OPERATION);
                }
                else
                {
                    return error(GL_INVALID_VALUE);
                }
            }

            return programObject->getAttachedShaders(maxcount, count, shaders);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

int __stdcall glGetAttribLocation(GLuint program, const GLchar* name)
{
    TRACE("(GLuint program = %d, const GLchar* name = %s)", program, name);

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {

            gl::Program *programObject = context->getProgram(program);

            if (!programObject)
            {
                if (context->getShader(program))
                {
                    return error(GL_INVALID_OPERATION, -1);
                }
                else
                {
                    return error(GL_INVALID_VALUE, -1);
                }
            }

            if (!programObject->isLinked())
            {
                return error(GL_INVALID_OPERATION, -1);
            }

            return programObject->getAttributeLocation(name);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY, -1);
    }

    return -1;
}

void __stdcall glGetBooleanv(GLenum pname, GLboolean* params)
{
    TRACE("(GLenum pname = 0x%X, GLboolean* params = 0x%0.8p)",  pname, params);

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            if (!(context->getBooleanv(pname, params)))
            {
                GLenum nativeType;
                unsigned int numParams = 0;
                if (!context->getQueryParameterInfo(pname, &nativeType, &numParams))
                    return error(GL_INVALID_ENUM);

                if (numParams == 0)
                    return; // it is known that the pname is valid, but there are no parameters to return

                if (nativeType == GL_FLOAT)
                {
                    GLfloat *floatParams = NULL;
                    floatParams = new GLfloat[numParams];

                    context->getFloatv(pname, floatParams);

                    for (unsigned int i = 0; i < numParams; ++i)
                    {
                        if (floatParams[i] == 0.0f)
                            params[i] = GL_FALSE;
                        else
                            params[i] = GL_TRUE;
                    }

                    delete [] floatParams;
                }
                else if (nativeType == GL_INT)
                {
                    GLint *intParams = NULL;
                    intParams = new GLint[numParams];

                    context->getIntegerv(pname, intParams);

                    for (unsigned int i = 0; i < numParams; ++i)
                    {
                        if (intParams[i] == 0)
                            params[i] = GL_FALSE;
                        else
                            params[i] = GL_TRUE;
                    }

                    delete [] intParams;
                }
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetBufferParameteriv(GLenum target, GLenum pname, GLint* params)
{
    TRACE("(GLenum target = 0x%X, GLenum pname = 0x%X, GLint* params = 0x%0.8p)", target, pname, params);

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            gl::Buffer *buffer;

            switch (target)
            {
              case GL_ARRAY_BUFFER:
                buffer = context->getArrayBuffer();
                break;
              case GL_ELEMENT_ARRAY_BUFFER:
                buffer = context->getElementArrayBuffer();
                break;
              default: return error(GL_INVALID_ENUM);
            }

            if (!buffer)
            {
                // A null buffer means that "0" is bound to the requested buffer target
                return error(GL_INVALID_OPERATION);
            }

            switch (pname)
            {
              case GL_BUFFER_USAGE:
                *params = buffer->usage();
                break;
              case GL_BUFFER_SIZE:
                *params = buffer->size();
                break;
              default: return error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

GLenum __stdcall glGetError(void)
{
    TRACE("()");

    gl::Context *context = gl::getContext();

    if (context)
    {
        return context->getError();
    }

    return GL_NO_ERROR;
}

void __stdcall glGetFloatv(GLenum pname, GLfloat* params)
{
    TRACE("(GLenum pname = 0x%X, GLfloat* params = 0x%0.8p)", pname, params);

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            if (!(context->getFloatv(pname, params)))
            {
                GLenum nativeType;
                unsigned int numParams = 0;
                if (!context->getQueryParameterInfo(pname, &nativeType, &numParams))
                    return error(GL_INVALID_ENUM);

                if (numParams == 0)
                    return; // it is known that the pname is valid, but that there are no parameters to return.

                if (nativeType == GL_BOOL)
                {
                    GLboolean *boolParams = NULL;
                    boolParams = new GLboolean[numParams];

                    context->getBooleanv(pname, boolParams);

                    for (unsigned int i = 0; i < numParams; ++i)
                    {
                        if (boolParams[i] == GL_FALSE)
                            params[i] = 0.0f;
                        else
                            params[i] = 1.0f;
                    }

                    delete [] boolParams;
                }
                else if (nativeType == GL_INT)
                {
                    GLint *intParams = NULL;
                    intParams = new GLint[numParams];

                    context->getIntegerv(pname, intParams);

                    for (unsigned int i = 0; i < numParams; ++i)
                    {
                        params[i] = (GLfloat)intParams[i];
                    }

                    delete [] intParams;
                }
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint* params)
{
    TRACE("(GLenum target = 0x%X, GLenum attachment = 0x%X, GLenum pname = 0x%X, GLint* params = 0x%0.8p)",
          target, attachment, pname, params);

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            if (target != GL_FRAMEBUFFER && target != GL_DRAW_FRAMEBUFFER_ANGLE && target != GL_READ_FRAMEBUFFER_ANGLE)
            {
                return error(GL_INVALID_ENUM);
            }

            gl::Framebuffer *framebuffer = NULL;
            if (target == GL_READ_FRAMEBUFFER_ANGLE)
            {
                if(context->getReadFramebufferHandle() == 0)
                {
                    return error(GL_INVALID_OPERATION);
                }

                framebuffer = context->getReadFramebuffer();
            }
            else 
            {
                if (context->getDrawFramebufferHandle() == 0)
                {
                    return error(GL_INVALID_OPERATION);
                }

                framebuffer = context->getDrawFramebuffer();
            }

            GLenum attachmentType;
            GLuint attachmentHandle;
            switch (attachment)
            {
              case GL_COLOR_ATTACHMENT0:    
                attachmentType = framebuffer->getColorbufferType();
                attachmentHandle = framebuffer->getColorbufferHandle(); 
                break;
              case GL_DEPTH_ATTACHMENT:     
                attachmentType = framebuffer->getDepthbufferType();
                attachmentHandle = framebuffer->getDepthbufferHandle();
                break;
              case GL_STENCIL_ATTACHMENT:   
                attachmentType = framebuffer->getStencilbufferType();
                attachmentHandle = framebuffer->getStencilbufferHandle();
                break;
              default: return error(GL_INVALID_ENUM);
            }

            GLenum attachmentObjectType;   // Type category
            if (attachmentType == GL_NONE || attachmentType == GL_RENDERBUFFER)
            {
                attachmentObjectType = attachmentType;
            }
            else if (gl::IsTextureTarget(attachmentType))
            {
                attachmentObjectType = GL_TEXTURE;
            }
            else UNREACHABLE();

            switch (pname)
            {
              case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
                *params = attachmentObjectType;
                break;
              case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
                if (attachmentObjectType == GL_RENDERBUFFER || attachmentObjectType == GL_TEXTURE)
                {
                    *params = attachmentHandle;
                }
                else
                {
                    return error(GL_INVALID_ENUM);
                }
                break;
              case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
                if (attachmentObjectType == GL_TEXTURE)
                {
                    *params = 0; // FramebufferTexture2D will not allow level to be set to anything else in GL ES 2.0
                }
                else
                {
                    return error(GL_INVALID_ENUM);
                }
                break;
              case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
                if (attachmentObjectType == GL_TEXTURE)
                {
                    if (gl::IsCubemapTextureTarget(attachmentType))
                    {
                        *params = attachmentType;
                    }
                    else
                    {
                        *params = 0;
                    }
                }
                else
                {
                    return error(GL_INVALID_ENUM);
                }
                break;
              default:
                return error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetIntegerv(GLenum pname, GLint* params)
{
    TRACE("(GLenum pname = 0x%X, GLint* params = 0x%0.8p)", pname, params);

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            if (!(context->getIntegerv(pname, params)))
            {
                GLenum nativeType;
                unsigned int numParams = 0;
                if (!context->getQueryParameterInfo(pname, &nativeType, &numParams))
                    return error(GL_INVALID_ENUM);

                if (numParams == 0)
                    return; // it is known that pname is valid, but there are no parameters to return

                if (nativeType == GL_BOOL)
                {
                    GLboolean *boolParams = NULL;
                    boolParams = new GLboolean[numParams];

                    context->getBooleanv(pname, boolParams);

                    for (unsigned int i = 0; i < numParams; ++i)
                    {
                        if (boolParams[i] == GL_FALSE)
                            params[i] = 0;
                        else
                            params[i] = 1;
                    }

                    delete [] boolParams;
                }
                else if (nativeType == GL_FLOAT)
                {
                    GLfloat *floatParams = NULL;
                    floatParams = new GLfloat[numParams];

                    context->getFloatv(pname, floatParams);

                    for (unsigned int i = 0; i < numParams; ++i)
                    {
                        if (pname == GL_DEPTH_RANGE || pname == GL_COLOR_CLEAR_VALUE || pname == GL_DEPTH_CLEAR_VALUE || pname == GL_BLEND_COLOR)
                        {
                            params[i] = (GLint)(((GLfloat)(0xFFFFFFFF) * floatParams[i] - 1.0f) / 2.0f);
                        }
                        else
                            params[i] = (GLint)(floatParams[i] > 0.0f ? floor(floatParams[i] + 0.5) : ceil(floatParams[i] - 0.5));
                    }

                    delete [] floatParams;
                }
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetProgramiv(GLuint program, GLenum pname, GLint* params)
{
    TRACE("(GLuint program = %d, GLenum pname = %d, GLint* params = 0x%0.8p)", program, pname, params);

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            gl::Program *programObject = context->getProgram(program);

            if (!programObject)
            {
                return error(GL_INVALID_VALUE);
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
              default:
                return error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetProgramInfoLog(GLuint program, GLsizei bufsize, GLsizei* length, GLchar* infolog)
{
    TRACE("(GLuint program = %d, GLsizei bufsize = %d, GLsizei* length = 0x%0.8p, GLchar* infolog = 0x%0.8p)",
          program, bufsize, length, infolog);

    try
    {
        if (bufsize < 0)
        {
            return error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            gl::Program *programObject = context->getProgram(program);

            if (!programObject)
            {
                return error(GL_INVALID_VALUE);
            }

            programObject->getInfoLog(bufsize, length, infolog);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint* params)
{
    TRACE("(GLenum target = 0x%X, GLenum pname = 0x%X, GLint* params = 0x%0.8p)", target, pname, params);

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            if (target != GL_RENDERBUFFER)
            {
                return error(GL_INVALID_ENUM);
            }

            if (context->getRenderbufferHandle() == 0)
            {
                return error(GL_INVALID_OPERATION);
            }

            gl::Renderbuffer *renderbuffer = context->getRenderbuffer(context->getRenderbufferHandle());

            switch (pname)
            {
              case GL_RENDERBUFFER_WIDTH:
                *params = renderbuffer->getWidth();
                break;
              case GL_RENDERBUFFER_HEIGHT:
                *params = renderbuffer->getHeight();
                break;
              case GL_RENDERBUFFER_INTERNAL_FORMAT:
                *params = renderbuffer->getFormat();
                break;
              case GL_RENDERBUFFER_RED_SIZE:
                if (renderbuffer->isColorbuffer())
                {
                    *params = static_cast<gl::Colorbuffer*>(renderbuffer->getStorage())->getRedSize();
                }
                else
                {
                    *params = 0;
                }
                break;
              case GL_RENDERBUFFER_GREEN_SIZE:
                if (renderbuffer->isColorbuffer())
                {
                    *params = static_cast<gl::Colorbuffer*>(renderbuffer->getStorage())->getGreenSize();
                }
                else
                {
                    *params = 0;
                }
                break;
              case GL_RENDERBUFFER_BLUE_SIZE:
                if (renderbuffer->isColorbuffer())
                {
                    *params = static_cast<gl::Colorbuffer*>(renderbuffer->getStorage())->getBlueSize();
                }
                else
                {
                    *params = 0;
                }
                break;
              case GL_RENDERBUFFER_ALPHA_SIZE:
                if (renderbuffer->isColorbuffer())
                {
                    *params = static_cast<gl::Colorbuffer*>(renderbuffer->getStorage())->getAlphaSize();
                }
                else
                {
                    *params = 0;
                }
                break;
              case GL_RENDERBUFFER_DEPTH_SIZE:
                if (renderbuffer->isDepthbuffer())
                {
                    *params = static_cast<gl::Depthbuffer*>(renderbuffer->getStorage())->getDepthSize();
                }
                else
                {
                    *params = 0;
                }
                break;
              case GL_RENDERBUFFER_STENCIL_SIZE:
                if (renderbuffer->isStencilbuffer())
                {
                    *params = static_cast<gl::Stencilbuffer*>(renderbuffer->getStorage())->getStencilSize();
                }
                else
                {
                    *params = 0;
                }
                break;
              case GL_RENDERBUFFER_SAMPLES_ANGLE:
                {
                    if (context->getMaxSupportedSamples() != 0)
                    {
                        *params = renderbuffer->getStorage()->getSamples();
                    }
                    else
                    {
                        return error(GL_INVALID_ENUM);
                    }
                }
                break;
              default:
                return error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetShaderiv(GLuint shader, GLenum pname, GLint* params)
{
    TRACE("(GLuint shader = %d, GLenum pname = %d, GLint* params = 0x%0.8p)", shader, pname, params);

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            gl::Shader *shaderObject = context->getShader(shader);

            if (!shaderObject)
            {
                return error(GL_INVALID_VALUE);
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
              default:
                return error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetShaderInfoLog(GLuint shader, GLsizei bufsize, GLsizei* length, GLchar* infolog)
{
    TRACE("(GLuint shader = %d, GLsizei bufsize = %d, GLsizei* length = 0x%0.8p, GLchar* infolog = 0x%0.8p)",
          shader, bufsize, length, infolog);

    try
    {
        if (bufsize < 0)
        {
            return error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            gl::Shader *shaderObject = context->getShader(shader);

            if (!shaderObject)
            {
                return error(GL_INVALID_VALUE);
            }

            shaderObject->getInfoLog(bufsize, length, infolog);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetShaderPrecisionFormat(GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision)
{
    TRACE("(GLenum shadertype = 0x%X, GLenum precisiontype = 0x%X, GLint* range = 0x%0.8p, GLint* precision = 0x%0.8p)",
          shadertype, precisiontype, range, precision);

    try
    {
        switch (shadertype)
        {
          case GL_VERTEX_SHADER:
          case GL_FRAGMENT_SHADER:
            break;
          default:
            return error(GL_INVALID_ENUM);
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
            return error(GL_INVALID_ENUM);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetShaderSource(GLuint shader, GLsizei bufsize, GLsizei* length, GLchar* source)
{
    TRACE("(GLuint shader = %d, GLsizei bufsize = %d, GLsizei* length = 0x%0.8p, GLchar* source = 0x%0.8p)",
          shader, bufsize, length, source);

    try
    {
        if (bufsize < 0)
        {
            return error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            gl::Shader *shaderObject = context->getShader(shader);

            if (!shaderObject)
            {
                return error(GL_INVALID_OPERATION);
            }

            shaderObject->getSource(bufsize, length, source);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

const GLubyte* __stdcall glGetString(GLenum name)
{
    TRACE("(GLenum name = 0x%X)", name);

    try
    {
        gl::Context *context = gl::getContext();

        switch (name)
        {
          case GL_VENDOR:
            return (GLubyte*)"TransGaming Inc.";
          case GL_RENDERER:
            return (GLubyte*)"ANGLE";
          case GL_VERSION:
            return (GLubyte*)"OpenGL ES 2.0 (git-devel "__DATE__ " " __TIME__")";
          case GL_SHADING_LANGUAGE_VERSION:
            return (GLubyte*)"OpenGL ES GLSL ES 1.00 (git-devel "__DATE__ " " __TIME__")";
          case GL_EXTENSIONS:
            return (GLubyte*)((context != NULL) ? context->getExtensionString() : "");
          default:
            return error(GL_INVALID_ENUM, (GLubyte*)NULL);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY, (GLubyte*)NULL);
    }

    return NULL;
}

void __stdcall glGetTexParameterfv(GLenum target, GLenum pname, GLfloat* params)
{
    TRACE("(GLenum target = 0x%X, GLenum pname = 0x%X, GLfloat* params = 0x%0.8p)", target, pname, params);

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            gl::Texture *texture;

            switch (target)
            {
              case GL_TEXTURE_2D:
                texture = context->getTexture2D();
                break;
              case GL_TEXTURE_CUBE_MAP:
                texture = context->getTextureCubeMap();
                break;
              default:
                return error(GL_INVALID_ENUM);
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
              default:
                return error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetTexParameteriv(GLenum target, GLenum pname, GLint* params)
{
    TRACE("(GLenum target = 0x%X, GLenum pname = 0x%X, GLint* params = 0x%0.8p)", target, pname, params);

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            gl::Texture *texture;

            switch (target)
            {
              case GL_TEXTURE_2D:
                texture = context->getTexture2D();
                break;
              case GL_TEXTURE_CUBE_MAP:
                texture = context->getTextureCubeMap();
                break;
              default:
                return error(GL_INVALID_ENUM);
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
              default:
                return error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetUniformfv(GLuint program, GLint location, GLfloat* params)
{
    TRACE("(GLuint program = %d, GLint location = %d, GLfloat* params = 0x%0.8p)", program, location, params);

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            if (program == 0)
            {
                return error(GL_INVALID_VALUE);
            }

            gl::Program *programObject = context->getProgram(program);

            if (!programObject || !programObject->isLinked())
            {
                return error(GL_INVALID_OPERATION);
            }

            if (!programObject->getUniformfv(location, params))
            {
                return error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetUniformiv(GLuint program, GLint location, GLint* params)
{
    TRACE("(GLuint program = %d, GLint location = %d, GLint* params = 0x%0.8p)", program, location, params);

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            if (program == 0)
            {
                return error(GL_INVALID_VALUE);
            }

            gl::Program *programObject = context->getProgram(program);

            if (!programObject || !programObject->isLinked())
            {
                return error(GL_INVALID_OPERATION);
            }

            if (!programObject)
            {
                return error(GL_INVALID_OPERATION);
            }

            if (!programObject->getUniformiv(location, params))
            {
                return error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

int __stdcall glGetUniformLocation(GLuint program, const GLchar* name)
{
    TRACE("(GLuint program = %d, const GLchar* name = 0x%0.8p)", program, name);

    try
    {
        gl::Context *context = gl::getContext();

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
                    return error(GL_INVALID_OPERATION, -1);
                }
                else
                {
                    return error(GL_INVALID_VALUE, -1);
                }
            }

            if (!programObject->isLinked())
            {
                return error(GL_INVALID_OPERATION, -1);
            }

            return programObject->getUniformLocation(name, false);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY, -1);
    }

    return -1;
}

void __stdcall glGetVertexAttribfv(GLuint index, GLenum pname, GLfloat* params)
{
    TRACE("(GLuint index = %d, GLenum pname = 0x%X, GLfloat* params = 0x%0.8p)", index, pname, params);

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            if (index >= gl::MAX_VERTEX_ATTRIBS)
            {
                return error(GL_INVALID_VALUE);
            }

            const gl::AttributeState &attribState = context->getVertexAttribState(index);

            switch (pname)
            {
              case GL_VERTEX_ATTRIB_ARRAY_ENABLED:
                *params = (GLfloat)(attribState.mEnabled ? GL_TRUE : GL_FALSE);
                break;
              case GL_VERTEX_ATTRIB_ARRAY_SIZE:
                *params = (GLfloat)attribState.mSize;
                break;
              case GL_VERTEX_ATTRIB_ARRAY_STRIDE:
                *params = (GLfloat)attribState.mStride;
                break;
              case GL_VERTEX_ATTRIB_ARRAY_TYPE:
                *params = (GLfloat)attribState.mType;
                break;
              case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:
                *params = (GLfloat)(attribState.mNormalized ? GL_TRUE : GL_FALSE);
                break;
              case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
                *params = (GLfloat)attribState.mBoundBuffer.id();
                break;
              case GL_CURRENT_VERTEX_ATTRIB:
                for (int i = 0; i < 4; ++i)
                {
                    params[i] = attribState.mCurrentValue[i];
                }
                break;
              default: return error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetVertexAttribiv(GLuint index, GLenum pname, GLint* params)
{
    TRACE("(GLuint index = %d, GLenum pname = 0x%X, GLint* params = 0x%0.8p)", index, pname, params);

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            if (index >= gl::MAX_VERTEX_ATTRIBS)
            {
                return error(GL_INVALID_VALUE);
            }

            const gl::AttributeState &attribState = context->getVertexAttribState(index);

            switch (pname)
            {
              case GL_VERTEX_ATTRIB_ARRAY_ENABLED:
                *params = (attribState.mEnabled ? GL_TRUE : GL_FALSE);
                break;
              case GL_VERTEX_ATTRIB_ARRAY_SIZE:
                *params = attribState.mSize;
                break;
              case GL_VERTEX_ATTRIB_ARRAY_STRIDE:
                *params = attribState.mStride;
                break;
              case GL_VERTEX_ATTRIB_ARRAY_TYPE:
                *params = attribState.mType;
                break;
              case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:
                *params = (attribState.mNormalized ? GL_TRUE : GL_FALSE);
                break;
              case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
                *params = attribState.mBoundBuffer.id();
                break;
              case GL_CURRENT_VERTEX_ATTRIB:
                for (int i = 0; i < 4; ++i)
                {
                    float currentValue = attribState.mCurrentValue[i];
                    params[i] = (GLint)(currentValue > 0.0f ? floor(currentValue + 0.5f) : ceil(currentValue - 0.5f));
                }
                break;
              default: return error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glGetVertexAttribPointerv(GLuint index, GLenum pname, GLvoid** pointer)
{
    TRACE("(GLuint index = %d, GLenum pname = 0x%X, GLvoid** pointer = 0x%0.8p)", index, pname, pointer);

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            if (index >= gl::MAX_VERTEX_ATTRIBS)
            {
                return error(GL_INVALID_VALUE);
            }

            if (pname != GL_VERTEX_ATTRIB_ARRAY_POINTER)
            {
                return error(GL_INVALID_ENUM);
            }

            *pointer = const_cast<GLvoid*>(context->getVertexAttribPointer(index));
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glHint(GLenum target, GLenum mode)
{
    TRACE("(GLenum target = 0x%X, GLenum mode = 0x%X)", target, mode);

    try
    {
        switch (target)
        {
          case GL_GENERATE_MIPMAP_HINT:
            switch (mode)
            {
              case GL_FASTEST:
              case GL_NICEST:
              case GL_DONT_CARE:
                break;
              default:
                return error(GL_INVALID_ENUM); 
            }
            break;
          default:
              return error(GL_INVALID_ENUM);
        }

        gl::Context *context = gl::getContext();
        if (context)
        {
            if (target == GL_GENERATE_MIPMAP_HINT)
                context->setGenerateMipmapHint(mode);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

GLboolean __stdcall glIsBuffer(GLuint buffer)
{
    TRACE("(GLuint buffer = %d)", buffer);

    try
    {
        gl::Context *context = gl::getContext();

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
        return error(GL_OUT_OF_MEMORY, GL_FALSE);
    }

    return GL_FALSE;
}

GLboolean __stdcall glIsEnabled(GLenum cap)
{
    TRACE("(GLenum cap = 0x%X)", cap);

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            switch (cap)
            {
              case GL_CULL_FACE:                return context->isCullFaceEnabled();
              case GL_POLYGON_OFFSET_FILL:      return context->isPolygonOffsetFillEnabled();
              case GL_SAMPLE_ALPHA_TO_COVERAGE: return context->isSampleAlphaToCoverageEnabled();
              case GL_SAMPLE_COVERAGE:          return context->isSampleCoverageEnabled();
              case GL_SCISSOR_TEST:             return context->isScissorTestEnabled();
              case GL_STENCIL_TEST:             return context->isStencilTestEnabled();
              case GL_DEPTH_TEST:               return context->isDepthTestEnabled();
              case GL_BLEND:                    return context->isBlendEnabled();
              case GL_DITHER:                   return context->isDitherEnabled();
              default:
                return error(GL_INVALID_ENUM, false);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY, false);
    }

    return false;
}

GLboolean __stdcall glIsFramebuffer(GLuint framebuffer)
{
    TRACE("(GLuint framebuffer = %d)", framebuffer);

    try
    {
        gl::Context *context = gl::getContext();

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
        return error(GL_OUT_OF_MEMORY, GL_FALSE);
    }

    return GL_FALSE;
}

GLboolean __stdcall glIsProgram(GLuint program)
{
    TRACE("(GLuint program = %d)", program);

    try
    {
        gl::Context *context = gl::getContext();

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
        return error(GL_OUT_OF_MEMORY, GL_FALSE);
    }

    return GL_FALSE;
}

GLboolean __stdcall glIsRenderbuffer(GLuint renderbuffer)
{
    TRACE("(GLuint renderbuffer = %d)", renderbuffer);

    try
    {
        gl::Context *context = gl::getContext();

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
        return error(GL_OUT_OF_MEMORY, GL_FALSE);
    }

    return GL_FALSE;
}

GLboolean __stdcall glIsShader(GLuint shader)
{
    TRACE("(GLuint shader = %d)", shader);

    try
    {
        gl::Context *context = gl::getContext();

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
        return error(GL_OUT_OF_MEMORY, GL_FALSE);
    }

    return GL_FALSE;
}

GLboolean __stdcall glIsTexture(GLuint texture)
{
    TRACE("(GLuint texture = %d)", texture);

    try
    {
        gl::Context *context = gl::getContext();

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
        return error(GL_OUT_OF_MEMORY, GL_FALSE);
    }

    return GL_FALSE;
}

void __stdcall glLineWidth(GLfloat width)
{
    TRACE("(GLfloat width = %f)", width);

    try
    {
        if (width <= 0.0f)
        {
            return error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            context->setLineWidth(width);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glLinkProgram(GLuint program)
{
    TRACE("(GLuint program = %d)", program);

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            gl::Program *programObject = context->getProgram(program);

            if (!programObject)
            {
                if (context->getShader(program))
                {
                    return error(GL_INVALID_OPERATION);
                }
                else
                {
                    return error(GL_INVALID_VALUE);
                }
            }

            programObject->link();
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glPixelStorei(GLenum pname, GLint param)
{
    TRACE("(GLenum pname = 0x%X, GLint param = %d)", pname, param);

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            switch (pname)
            {
              case GL_UNPACK_ALIGNMENT:
                if (param != 1 && param != 2 && param != 4 && param != 8)
                {
                    return error(GL_INVALID_VALUE);
                }

                context->setUnpackAlignment(param);
                break;

              case GL_PACK_ALIGNMENT:
                if (param != 1 && param != 2 && param != 4 && param != 8)
                {
                    return error(GL_INVALID_VALUE);
                }

                context->setPackAlignment(param);
                break;

              default:
                return error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glPolygonOffset(GLfloat factor, GLfloat units)
{
    TRACE("(GLfloat factor = %f, GLfloat units = %f)", factor, units);

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            context->setPolygonOffsetParams(factor, units);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid* pixels)
{
    TRACE("(GLint x = %d, GLint y = %d, GLsizei width = %d, GLsizei height = %d, "
          "GLenum format = 0x%X, GLenum type = 0x%X, GLvoid* pixels = 0x%0.8p)",
          x, y, width, height, format, type,  pixels);

    try
    {
        if (width < 0 || height < 0)
        {
            return error(GL_INVALID_VALUE);
        }

        switch (format)
        {
          case GL_RGBA:
            switch (type)
            {
              case GL_UNSIGNED_BYTE:
                break;
              default:
                return error(GL_INVALID_OPERATION);
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
                return error(GL_INVALID_OPERATION);
            }
            break;
          case gl::IMPLEMENTATION_COLOR_READ_FORMAT:
            switch (type)
            {
              case gl::IMPLEMENTATION_COLOR_READ_TYPE:
                break;
              default:
                return error(GL_INVALID_OPERATION);
            }
            break;
          default:
            return error(GL_INVALID_OPERATION);
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            context->readPixels(x, y, width, height, format, type, pixels);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glReleaseShaderCompiler(void)
{
    TRACE("()");

    try
    {
        gl::Shader::releaseCompiler();
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glRenderbufferStorageMultisampleANGLE(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height)
{
    TRACE("(GLenum target = 0x%X, GLsizei samples = %d, GLenum internalformat = 0x%X, GLsizei width = %d, GLsizei height = %d)",
          target, samples, internalformat, width, height);

    try
    {
        switch (target)
        {
          case GL_RENDERBUFFER:
            break;
          default:
            return error(GL_INVALID_ENUM);
        }

        switch (internalformat)
        {
          case GL_DEPTH_COMPONENT16:
          case GL_RGBA4:
          case GL_RGB5_A1:
          case GL_RGB565:
          case GL_STENCIL_INDEX8:
          case GL_DEPTH24_STENCIL8_OES:
          case GL_RGB8_OES:
          case GL_RGBA8_OES:
            break;
          default:
            return error(GL_INVALID_ENUM);
        }

        if (width < 0 || height < 0 || width > gl::MAX_RENDERBUFFER_SIZE || height > gl::MAX_RENDERBUFFER_SIZE || samples < 0)
        {
            return error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            if (samples > context->getMaxSupportedSamples())
            {
                return error(GL_INVALID_VALUE);
            }

            GLuint handle = context->getRenderbufferHandle();
            if (handle == 0)
            {
                return error(GL_INVALID_OPERATION);
            }

            switch (internalformat)
            {
              case GL_DEPTH_COMPONENT16:
                context->setRenderbufferStorage(new gl::Depthbuffer(width, height, samples));
                break;
              case GL_RGBA4:
              case GL_RGB5_A1:
              case GL_RGB565:
              case GL_RGB8_OES:
              case GL_RGBA8_OES:
                context->setRenderbufferStorage(new gl::Colorbuffer(width, height, internalformat, samples));
                break;
              case GL_STENCIL_INDEX8:
                context->setRenderbufferStorage(new gl::Stencilbuffer(width, height, samples));
                break;
              case GL_DEPTH24_STENCIL8_OES:
                context->setRenderbufferStorage(new gl::DepthStencilbuffer(width, height, samples));
                break;
              default:
                return error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
    glRenderbufferStorageMultisampleANGLE(target, 0, internalformat, width, height);
}

void __stdcall glSampleCoverage(GLclampf value, GLboolean invert)
{
    TRACE("(GLclampf value = %f, GLboolean invert = %d)", value, invert);

    try
    {
        gl::Context* context = gl::getContext();

        if (context)
        {
            context->setSampleCoverageParams(gl::clamp01(value), invert == GL_TRUE);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glScissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
    TRACE("(GLint x = %d, GLint y = %d, GLsizei width = %d, GLsizei height = %d)", x, y, width, height);

    try
    {
        if (width < 0 || height < 0)
        {
            return error(GL_INVALID_VALUE);
        }

        gl::Context* context = gl::getContext();

        if (context)
        {
            context->setScissorParams(x, y, width, height);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glShaderBinary(GLsizei n, const GLuint* shaders, GLenum binaryformat, const GLvoid* binary, GLsizei length)
{
    TRACE("(GLsizei n = %d, const GLuint* shaders = 0x%0.8p, GLenum binaryformat = 0x%X, "
          "const GLvoid* binary = 0x%0.8p, GLsizei length = %d)",
          n, shaders, binaryformat, binary, length);

    try
    {
        // No binary shader formats are supported.
        return error(GL_INVALID_ENUM);
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glShaderSource(GLuint shader, GLsizei count, const GLchar** string, const GLint* length)
{
    TRACE("(GLuint shader = %d, GLsizei count = %d, const GLchar** string = 0x%0.8p, const GLint* length = 0x%0.8p)",
          shader, count, string, length);

    try
    {
        if (count < 0)
        {
            return error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            gl::Shader *shaderObject = context->getShader(shader);

            if (!shaderObject)
            {
                if (context->getProgram(shader))
                {
                    return error(GL_INVALID_OPERATION);
                }
                else
                {
                    return error(GL_INVALID_VALUE);
                }
            }

            shaderObject->setSource(count, string, length);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glStencilFunc(GLenum func, GLint ref, GLuint mask)
{
    glStencilFuncSeparate(GL_FRONT_AND_BACK, func, ref, mask);
}

void __stdcall glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask)
{
    TRACE("(GLenum face = 0x%X, GLenum func = 0x%X, GLint ref = %d, GLuint mask = %d)", face, func, ref, mask);

    try
    {
        switch (face)
        {
          case GL_FRONT:
          case GL_BACK:
          case GL_FRONT_AND_BACK:
            break;
          default:
            return error(GL_INVALID_ENUM);
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
            return error(GL_INVALID_ENUM);
        }

        gl::Context *context = gl::getContext();

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
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glStencilMask(GLuint mask)
{
    glStencilMaskSeparate(GL_FRONT_AND_BACK, mask);
}

void __stdcall glStencilMaskSeparate(GLenum face, GLuint mask)
{
    TRACE("(GLenum face = 0x%X, GLuint mask = %d)", face, mask);

    try
    {
        switch (face)
        {
          case GL_FRONT:
          case GL_BACK:
          case GL_FRONT_AND_BACK:
            break;
          default:
            return error(GL_INVALID_ENUM);
        }

        gl::Context *context = gl::getContext();

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
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glStencilOp(GLenum fail, GLenum zfail, GLenum zpass)
{
    glStencilOpSeparate(GL_FRONT_AND_BACK, fail, zfail, zpass);
}

void __stdcall glStencilOpSeparate(GLenum face, GLenum fail, GLenum zfail, GLenum zpass)
{
    TRACE("(GLenum face = 0x%X, GLenum fail = 0x%X, GLenum zfail = 0x%X, GLenum zpas = 0x%Xs)",
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
            return error(GL_INVALID_ENUM);
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
            return error(GL_INVALID_ENUM);
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
            return error(GL_INVALID_ENUM);
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
            return error(GL_INVALID_ENUM);
        }

        gl::Context *context = gl::getContext();

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
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
                            GLint border, GLenum format, GLenum type, const GLvoid* pixels)
{
    TRACE("(GLenum target = 0x%X, GLint level = %d, GLint internalformat = %d, GLsizei width = %d, GLsizei height = %d, "
          "GLint border = %d, GLenum format = 0x%X, GLenum type = 0x%X, const GLvoid* pixels =  0x%0.8p)",
          target, level, internalformat, width, height, border, format, type, pixels);

    try
    {
        if (level < 0 || width < 0 || height < 0)
        {
            return error(GL_INVALID_VALUE);
        }

        if (level > 0 && (!gl::isPow2(width) || !gl::isPow2(height)))
        {
            return error(GL_INVALID_VALUE);
        }

        switch (target)
        {
          case GL_TEXTURE_2D:
            if (width > (gl::MAX_TEXTURE_SIZE >> level) || height > (gl::MAX_TEXTURE_SIZE >> level))
            {
                return error(GL_INVALID_VALUE);
            }
            break;
          case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
          case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
          case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
          case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
          case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
          case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
            if (width != height)
            {
                return error(GL_INVALID_VALUE);
            }

            if (width > (gl::MAX_CUBE_MAP_TEXTURE_SIZE >> level) || height > (gl::MAX_CUBE_MAP_TEXTURE_SIZE >> level))
            {
                return error(GL_INVALID_VALUE);
            }
            break;
          default:
            return error(GL_INVALID_ENUM);
        }

        if (internalformat != format)
        {
            return error(GL_INVALID_OPERATION);
        }

        switch (internalformat)
        {
          case GL_ALPHA:
          case GL_LUMINANCE:
          case GL_LUMINANCE_ALPHA:
            switch (type)
            {
              case GL_UNSIGNED_BYTE:
                break;
              default:
                return error(GL_INVALID_ENUM);
            }
            break;
          case GL_RGB:
            switch (type)
            {
              case GL_UNSIGNED_BYTE:
              case GL_UNSIGNED_SHORT_5_6_5:
                break;
              default:
                return error(GL_INVALID_ENUM);
            }
            break;
          case GL_RGBA:
            switch (type)
            {
              case GL_UNSIGNED_BYTE:
              case GL_UNSIGNED_SHORT_4_4_4_4:
              case GL_UNSIGNED_SHORT_5_5_5_1:
                break;
              default:
                return error(GL_INVALID_ENUM);
            }
            break;
          case GL_BGRA_EXT:
            switch (type)
            {
              case GL_UNSIGNED_BYTE:
                break;
              default:
                return error(GL_INVALID_ENUM);
            }
            break;
          case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:  // error cases for compressed textures are handled below
          case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
            break; 
          default:
            return error(GL_INVALID_VALUE);
        }

        if (border != 0)
        {
            return error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            if (internalformat == GL_COMPRESSED_RGB_S3TC_DXT1_EXT ||
                internalformat == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT)
            {
                if (context->supportsCompressedTextures())
                {
                    return error(GL_INVALID_OPERATION);
                }
                else
                {
                    return error(GL_INVALID_ENUM);
                }
            }

            if (target == GL_TEXTURE_2D)
            {
                gl::Texture2D *texture = context->getTexture2D();

                if (!texture)
                {
                    return error(GL_INVALID_OPERATION);
                }

                texture->setImage(level, internalformat, width, height, format, type, context->getUnpackAlignment(), pixels);
            }
            else
            {
                gl::TextureCubeMap *texture = context->getTextureCubeMap();

                if (!texture)
                {
                    return error(GL_INVALID_OPERATION);
                }

                switch (target)
                {
                  case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
                    texture->setImagePosX(level, internalformat, width, height, format, type, context->getUnpackAlignment(), pixels);
                    break;
                  case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
                    texture->setImageNegX(level, internalformat, width, height, format, type, context->getUnpackAlignment(), pixels);
                    break;
                  case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
                    texture->setImagePosY(level, internalformat, width, height, format, type, context->getUnpackAlignment(), pixels);
                    break;
                  case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
                    texture->setImageNegY(level, internalformat, width, height, format, type, context->getUnpackAlignment(), pixels);
                    break;
                  case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
                    texture->setImagePosZ(level, internalformat, width, height, format, type, context->getUnpackAlignment(), pixels);
                    break;
                  case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
                    texture->setImageNegZ(level, internalformat, width, height, format, type, context->getUnpackAlignment(), pixels);
                    break;
                  default: UNREACHABLE();
                }
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glTexParameterf(GLenum target, GLenum pname, GLfloat param)
{
    glTexParameteri(target, pname, (GLint)param);
}

void __stdcall glTexParameterfv(GLenum target, GLenum pname, const GLfloat* params)
{
    glTexParameteri(target, pname, (GLint)*params);
}

void __stdcall glTexParameteri(GLenum target, GLenum pname, GLint param)
{
    TRACE("(GLenum target = 0x%X, GLenum pname = 0x%X, GLint param = %d)", target, pname, param);

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            gl::Texture *texture;

            switch (target)
            {
              case GL_TEXTURE_2D:
                texture = context->getTexture2D();
                break;
              case GL_TEXTURE_CUBE_MAP:
                texture = context->getTextureCubeMap();
                break;
              default:
                return error(GL_INVALID_ENUM);
            }

            switch (pname)
            {
              case GL_TEXTURE_WRAP_S:
                if (!texture->setWrapS((GLenum)param))
                {
                    return error(GL_INVALID_ENUM);
                }
                break;
              case GL_TEXTURE_WRAP_T:
                if (!texture->setWrapT((GLenum)param))
                {
                    return error(GL_INVALID_ENUM);
                }
                break;
              case GL_TEXTURE_MIN_FILTER:
                if (!texture->setMinFilter((GLenum)param))
                {
                    return error(GL_INVALID_ENUM);
                }
                break;
              case GL_TEXTURE_MAG_FILTER:
                if (!texture->setMagFilter((GLenum)param))
                {
                    return error(GL_INVALID_ENUM);
                }
                break;
              default:
                return error(GL_INVALID_ENUM);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glTexParameteriv(GLenum target, GLenum pname, const GLint* params)
{
    glTexParameteri(target, pname, *params);
}

void __stdcall glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                               GLenum format, GLenum type, const GLvoid* pixels)
{
    TRACE("(GLenum target = 0x%X, GLint level = %d, GLint xoffset = %d, GLint yoffset = %d, "
          "GLsizei width = %d, GLsizei height = %d, GLenum format = 0x%X, GLenum type = 0x%X, "
          "const GLvoid* pixels = 0x%0.8p)",
           target, level, xoffset, yoffset, width, height, format, type, pixels);

    try
    {
        if (!gl::IsTextureTarget(target))
        {
            return error(GL_INVALID_ENUM);
        }

        if (level < 0 || level > gl::MAX_TEXTURE_LEVELS || xoffset < 0 || yoffset < 0 || width < 0 || height < 0)
        {
            return error(GL_INVALID_VALUE);
        }

        if (std::numeric_limits<GLsizei>::max() - xoffset < width || std::numeric_limits<GLsizei>::max() - yoffset < height)
        {
            return error(GL_INVALID_VALUE);
        }

        if (!gl::CheckTextureFormatType(format, type))
        {
            return error(GL_INVALID_ENUM);
        }

        if (width == 0 || height == 0 || pixels == NULL)
        {
            return;
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            if (target == GL_TEXTURE_2D)
            {
                gl::Texture2D *texture = context->getTexture2D();

                if (!texture)
                {
                    return error(GL_INVALID_OPERATION);
                }

                if (texture->isCompressed())
                {
                    return error(GL_INVALID_OPERATION);
                }

                texture->subImage(level, xoffset, yoffset, width, height, format, type, context->getUnpackAlignment(), pixels);
            }
            else if (gl::IsCubemapTextureTarget(target))
            {
                gl::TextureCubeMap *texture = context->getTextureCubeMap();

                if (!texture)
                {
                    return error(GL_INVALID_OPERATION);
                }

                if (texture->isCompressed())
                {
                    return error(GL_INVALID_OPERATION);
                }

                texture->subImage(target, level, xoffset, yoffset, width, height, format, type, context->getUnpackAlignment(), pixels);
            }
            else
            {
                UNREACHABLE();
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glUniform1f(GLint location, GLfloat x)
{
    glUniform1fv(location, 1, &x);
}

void __stdcall glUniform1fv(GLint location, GLsizei count, const GLfloat* v)
{
    TRACE("(GLint location = %d, GLsizei count = %d, const GLfloat* v = 0x%0.8p)", location, count, v);

    try
    {
        if (count < 0)
        {
            return error(GL_INVALID_VALUE);
        }

        if (location == -1)
        {
            return;
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            gl::Program *program = context->getCurrentProgram();

            if (!program)
            {
                return error(GL_INVALID_OPERATION);
            }

            if (!program->setUniform1fv(location, count, v))
            {
                return error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glUniform1i(GLint location, GLint x)
{
    glUniform1iv(location, 1, &x);
}

void __stdcall glUniform1iv(GLint location, GLsizei count, const GLint* v)
{
    TRACE("(GLint location = %d, GLsizei count = %d, const GLint* v = 0x%0.8p)", location, count, v);

    try
    {
        if (count < 0)
        {
            return error(GL_INVALID_VALUE);
        }

        if (location == -1)
        {
            return;
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            gl::Program *program = context->getCurrentProgram();

            if (!program)
            {
                return error(GL_INVALID_OPERATION);
            }

            if (!program->setUniform1iv(location, count, v))
            {
                return error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glUniform2f(GLint location, GLfloat x, GLfloat y)
{
    GLfloat xy[2] = {x, y};

    glUniform2fv(location, 1, (GLfloat*)&xy);
}

void __stdcall glUniform2fv(GLint location, GLsizei count, const GLfloat* v)
{
    TRACE("(GLint location = %d, GLsizei count = %d, const GLfloat* v = 0x%0.8p)", location, count, v);

    try
    {
        if (count < 0)
        {
            return error(GL_INVALID_VALUE);
        }
        
        if (location == -1)
        {
            return;
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            gl::Program *program = context->getCurrentProgram();

            if (!program)
            {
                return error(GL_INVALID_OPERATION);
            }

            if (!program->setUniform2fv(location, count, v))
            {
                return error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glUniform2i(GLint location, GLint x, GLint y)
{
    GLint xy[4] = {x, y};

    glUniform2iv(location, 1, (GLint*)&xy);
}

void __stdcall glUniform2iv(GLint location, GLsizei count, const GLint* v)
{
    TRACE("(GLint location = %d, GLsizei count = %d, const GLint* v = 0x%0.8p)", location, count, v);

    try
    {
        if (count < 0)
        {
            return error(GL_INVALID_VALUE);
        }

        if (location == -1)
        {
            return;
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            gl::Program *program = context->getCurrentProgram();

            if (!program)
            {
                return error(GL_INVALID_OPERATION);
            }

            if (!program->setUniform2iv(location, count, v))
            {
                return error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glUniform3f(GLint location, GLfloat x, GLfloat y, GLfloat z)
{
    GLfloat xyz[3] = {x, y, z};

    glUniform3fv(location, 1, (GLfloat*)&xyz);
}

void __stdcall glUniform3fv(GLint location, GLsizei count, const GLfloat* v)
{
    TRACE("(GLint location = %d, GLsizei count = %d, const GLfloat* v = 0x%0.8p)", location, count, v);

    try
    {
        if (count < 0)
        {
            return error(GL_INVALID_VALUE);
        }

        if (location == -1)
        {
            return;
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            gl::Program *program = context->getCurrentProgram();

            if (!program)
            {
                return error(GL_INVALID_OPERATION);
            }

            if (!program->setUniform3fv(location, count, v))
            {
                return error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glUniform3i(GLint location, GLint x, GLint y, GLint z)
{
    GLint xyz[3] = {x, y, z};

    glUniform3iv(location, 1, (GLint*)&xyz);
}

void __stdcall glUniform3iv(GLint location, GLsizei count, const GLint* v)
{
    TRACE("(GLint location = %d, GLsizei count = %d, const GLint* v = 0x%0.8p)", location, count, v);

    try
    {
        if (count < 0)
        {
            return error(GL_INVALID_VALUE);
        }

        if (location == -1)
        {
            return;
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            gl::Program *program = context->getCurrentProgram();

            if (!program)
            {
                return error(GL_INVALID_OPERATION);
            }

            if (!program->setUniform3iv(location, count, v))
            {
                return error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glUniform4f(GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    GLfloat xyzw[4] = {x, y, z, w};

    glUniform4fv(location, 1, (GLfloat*)&xyzw);
}

void __stdcall glUniform4fv(GLint location, GLsizei count, const GLfloat* v)
{
    TRACE("(GLint location = %d, GLsizei count = %d, const GLfloat* v = 0x%0.8p)", location, count, v);

    try
    {
        if (count < 0)
        {
            return error(GL_INVALID_VALUE);
        }

        if (location == -1)
        {
            return;
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            gl::Program *program = context->getCurrentProgram();

            if (!program)
            {
                return error(GL_INVALID_OPERATION);
            }

            if (!program->setUniform4fv(location, count, v))
            {
                return error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glUniform4i(GLint location, GLint x, GLint y, GLint z, GLint w)
{
    GLint xyzw[4] = {x, y, z, w};

    glUniform4iv(location, 1, (GLint*)&xyzw);
}

void __stdcall glUniform4iv(GLint location, GLsizei count, const GLint* v)
{
    TRACE("(GLint location = %d, GLsizei count = %d, const GLint* v = 0x%0.8p)", location, count, v);

    try
    {
        if (count < 0)
        {
            return error(GL_INVALID_VALUE);
        }

        if (location == -1)
        {
            return;
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            gl::Program *program = context->getCurrentProgram();

            if (!program)
            {
                return error(GL_INVALID_OPERATION);
            }

            if (!program->setUniform4iv(location, count, v))
            {
                return error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
    TRACE("(GLint location = %d, GLsizei count = %d, GLboolean transpose = %d, const GLfloat* value = 0x%0.8p)",
          location, count, transpose, value);

    try
    {
        if (count < 0 || transpose != GL_FALSE)
        {
            return error(GL_INVALID_VALUE);
        }

        if (location == -1)
        {
            return;
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            gl::Program *program = context->getCurrentProgram();

            if (!program)
            {
                return error(GL_INVALID_OPERATION);
            }

            if (!program->setUniformMatrix2fv(location, count, value))
            {
                return error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
    TRACE("(GLint location = %d, GLsizei count = %d, GLboolean transpose = %d, const GLfloat* value = 0x%0.8p)",
          location, count, transpose, value);

    try
    {
        if (count < 0 || transpose != GL_FALSE)
        {
            return error(GL_INVALID_VALUE);
        }

        if (location == -1)
        {
            return;
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            gl::Program *program = context->getCurrentProgram();

            if (!program)
            {
                return error(GL_INVALID_OPERATION);
            }

            if (!program->setUniformMatrix3fv(location, count, value))
            {
                return error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
    TRACE("(GLint location = %d, GLsizei count = %d, GLboolean transpose = %d, const GLfloat* value = 0x%0.8p)",
          location, count, transpose, value);

    try
    {
        if (count < 0 || transpose != GL_FALSE)
        {
            return error(GL_INVALID_VALUE);
        }

        if (location == -1)
        {
            return;
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            gl::Program *program = context->getCurrentProgram();

            if (!program)
            {
                return error(GL_INVALID_OPERATION);
            }

            if (!program->setUniformMatrix4fv(location, count, value))
            {
                return error(GL_INVALID_OPERATION);
            }
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glUseProgram(GLuint program)
{
    TRACE("(GLuint program = %d)", program);

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            gl::Program *programObject = context->getProgram(program);

            if (!programObject && program != 0)
            {
                if (context->getShader(program))
                {
                    return error(GL_INVALID_OPERATION);
                }
                else
                {
                    return error(GL_INVALID_VALUE);
                }
            }

            if (program != 0 && !programObject->isLinked())
            {
                return error(GL_INVALID_OPERATION);
            }

            context->useProgram(program);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glValidateProgram(GLuint program)
{
    TRACE("(GLuint program = %d)", program);

    try
    {
        gl::Context *context = gl::getContext();

        if (context)
        {
            gl::Program *programObject = context->getProgram(program);

            if (!programObject)
            {
                if (context->getShader(program))
                {
                    return error(GL_INVALID_OPERATION);
                }
                else
                {
                    return error(GL_INVALID_VALUE);
                }
            }

            programObject->validate();
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glVertexAttrib1f(GLuint index, GLfloat x)
{
    TRACE("(GLuint index = %d, GLfloat x = %f)", index, x);

    try
    {
        if (index >= gl::MAX_VERTEX_ATTRIBS)
        {
            return error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            GLfloat vals[4] = { x, 0, 0, 1 };
            context->setVertexAttrib(index, vals);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glVertexAttrib1fv(GLuint index, const GLfloat* values)
{
    TRACE("(GLuint index = %d, const GLfloat* values = 0x%0.8p)", index, values);

    try
    {
        if (index >= gl::MAX_VERTEX_ATTRIBS)
        {
            return error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            GLfloat vals[4] = { values[0], 0, 0, 1 };
            context->setVertexAttrib(index, vals);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glVertexAttrib2f(GLuint index, GLfloat x, GLfloat y)
{
    TRACE("(GLuint index = %d, GLfloat x = %f, GLfloat y = %f)", index, x, y);

    try
    {
        if (index >= gl::MAX_VERTEX_ATTRIBS)
        {
            return error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            GLfloat vals[4] = { x, y, 0, 1 };
            context->setVertexAttrib(index, vals);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glVertexAttrib2fv(GLuint index, const GLfloat* values)
{
    TRACE("(GLuint index = %d, const GLfloat* values = 0x%0.8p)", index, values);

    try
    {
        if (index >= gl::MAX_VERTEX_ATTRIBS)
        {
            return error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            GLfloat vals[4] = { values[0], values[1], 0, 1 };
            context->setVertexAttrib(index, vals);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glVertexAttrib3f(GLuint index, GLfloat x, GLfloat y, GLfloat z)
{
    TRACE("(GLuint index = %d, GLfloat x = %f, GLfloat y = %f, GLfloat z = %f)", index, x, y, z);

    try
    {
        if (index >= gl::MAX_VERTEX_ATTRIBS)
        {
            return error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            GLfloat vals[4] = { x, y, z, 1 };
            context->setVertexAttrib(index, vals);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glVertexAttrib3fv(GLuint index, const GLfloat* values)
{
    TRACE("(GLuint index = %d, const GLfloat* values = 0x%0.8p)", index, values);

    try
    {
        if (index >= gl::MAX_VERTEX_ATTRIBS)
        {
            return error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            GLfloat vals[4] = { values[0], values[1], values[2], 1 };
            context->setVertexAttrib(index, vals);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glVertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    TRACE("(GLuint index = %d, GLfloat x = %f, GLfloat y = %f, GLfloat z = %f, GLfloat w = %f)", index, x, y, z, w);

    try
    {
        if (index >= gl::MAX_VERTEX_ATTRIBS)
        {
            return error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            GLfloat vals[4] = { x, y, z, w };
            context->setVertexAttrib(index, vals);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glVertexAttrib4fv(GLuint index, const GLfloat* values)
{
    TRACE("(GLuint index = %d, const GLfloat* values = 0x%0.8p)", index, values);

    try
    {
        if (index >= gl::MAX_VERTEX_ATTRIBS)
        {
            return error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            context->setVertexAttrib(index, values);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* ptr)
{
    TRACE("(GLuint index = %d, GLint size = %d, GLenum type = 0x%X, "
          "GLboolean normalized = %d, GLsizei stride = %d, const GLvoid* ptr = 0x%0.8p)",
          index, size, type, normalized, stride, ptr);

    try
    {
        if (index >= gl::MAX_VERTEX_ATTRIBS)
        {
            return error(GL_INVALID_VALUE);
        }

        if (size < 1 || size > 4)
        {
            return error(GL_INVALID_VALUE);
        }

        switch (type)
        {
          case GL_BYTE:
          case GL_UNSIGNED_BYTE:
          case GL_SHORT:
          case GL_UNSIGNED_SHORT:
          case GL_FIXED:
          case GL_FLOAT:
            break;
          default:
            return error(GL_INVALID_ENUM);
        }

        if (stride < 0)
        {
            return error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            context->setVertexAttribState(index, context->getArrayBuffer(), size, type, (normalized == GL_TRUE), stride, ptr);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
    TRACE("(GLint x = %d, GLint y = %d, GLsizei width = %d, GLsizei height = %d)", x, y, width, height);

    try
    {
        if (width < 0 || height < 0)
        {
            return error(GL_INVALID_VALUE);
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            context->setViewportParams(x, y, width, height);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glBlitFramebufferANGLE(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                                      GLbitfield mask, GLenum filter)
{
    TRACE("(GLint srcX0 = %d, GLint srcY0 = %d, GLint srcX1 = %d, GLint srcY1 = %d, "
          "GLint dstX0 = %d, GLint dstY0 = %d, GLint dstX1 = %d, GLint dstY1 = %d, "
          "GLbitfield mask = 0x%X, GLenum filter = 0x%X)",
          srcX0, srcY0, srcX1, srcX1, dstX0, dstY0, dstX1, dstY1, mask, filter);

    try
    {
        switch (filter)
        {
          case GL_NEAREST:
            break;
          default:
            return error(GL_INVALID_ENUM);
        }

        if ((mask & ~(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)) != 0)
        {
            return error(GL_INVALID_VALUE);
        }

        if (srcX1 - srcX0 != dstX1 - dstX0 || srcY1 - srcY0 != dstY1 - dstY0)
        {
            ERR("Scaling and flipping in BlitFramebufferANGLE not supported by this implementation");
            return error(GL_INVALID_OPERATION);
        }

        gl::Context *context = gl::getContext();

        if (context)
        {
            if (context->getReadFramebufferHandle() == context->getDrawFramebufferHandle())
            {
                ERR("Blits with the same source and destination framebuffer are not supported by this implementation.");
                return error(GL_INVALID_OPERATION);
            }

            context->blitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask);
        }
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
    }
}

void __stdcall glTexImage3DOES(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth,
                               GLint border, GLenum format, GLenum type, const GLvoid* pixels)
{
    TRACE("(GLenum target = 0x%X, GLint level = %d, GLenum internalformat = 0x%X, "
          "GLsizei width = %d, GLsizei height = %d, GLsizei depth = %d, GLint border = %d, "
          "GLenum format = 0x%X, GLenum type = 0x%x, const GLvoid* pixels = 0x%0.8p)",
          target, level, internalformat, width, height, depth, border, format, type, pixels);

    try
    {
        UNIMPLEMENTED();   // FIXME
    }
    catch(std::bad_alloc&)
    {
        return error(GL_OUT_OF_MEMORY);
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
    };

    for (int ext = 0; ext < sizeof(glExtensions) / sizeof(Extension); ext++)
    {
        if (strcmp(procname, glExtensions[ext].name) == 0)
        {
            return (__eglMustCastToProperFunctionPointerType)glExtensions[ext].address;
        }
    }

    return NULL;
}

}
