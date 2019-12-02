//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationGL45.cpp: Validation functions for OpenGL 4.5 entry point parameters

#include "libANGLE/validationGL45_autogen.h"

namespace gl
{

bool ValidateBindTextureUnit(Context *context, GLuint unit, TextureID texture)
{
    return true;
}

bool ValidateBlitNamedFramebuffer(Context *context,
                                  GLuint readFramebuffer,
                                  GLuint drawFramebuffer,
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
    return true;
}

bool ValidateCheckNamedFramebufferStatus(Context *context, FramebufferID framebuffer, GLenum target)
{
    return true;
}

bool ValidateClearNamedBufferData(Context *context,
                                  BufferID buffer,
                                  GLenum internalformat,
                                  GLenum format,
                                  GLenum type,
                                  const void *data)
{
    return true;
}

bool ValidateClearNamedBufferSubData(Context *context,
                                     BufferID buffer,
                                     GLenum internalformat,
                                     GLintptr offset,
                                     GLsizeiptr size,
                                     GLenum format,
                                     GLenum type,
                                     const void *data)
{
    return true;
}

bool ValidateClearNamedFramebufferfi(Context *context,
                                     FramebufferID framebuffer,
                                     GLenum buffer,
                                     GLint drawbuffer,
                                     GLfloat depth,
                                     GLint stencil)
{
    return true;
}

bool ValidateClearNamedFramebufferfv(Context *context,
                                     FramebufferID framebuffer,
                                     GLenum buffer,
                                     GLint drawbuffer,
                                     const GLfloat *value)
{
    return true;
}

bool ValidateClearNamedFramebufferiv(Context *context,
                                     FramebufferID framebuffer,
                                     GLenum buffer,
                                     GLint drawbuffer,
                                     const GLint *value)
{
    return true;
}

bool ValidateClearNamedFramebufferuiv(Context *context,
                                      FramebufferID framebuffer,
                                      GLenum buffer,
                                      GLint drawbuffer,
                                      const GLuint *value)
{
    return true;
}

bool ValidateClipControl(Context *context, GLenum origin, GLenum depth)
{
    return true;
}

bool ValidateCompressedTextureSubImage1D(Context *context,
                                         TextureID texture,
                                         GLint level,
                                         GLint xoffset,
                                         GLsizei width,
                                         GLenum format,
                                         GLsizei imageSize,
                                         const void *data)
{
    return true;
}

bool ValidateCompressedTextureSubImage2D(Context *context,
                                         TextureID texture,
                                         GLint level,
                                         GLint xoffset,
                                         GLint yoffset,
                                         GLsizei width,
                                         GLsizei height,
                                         GLenum format,
                                         GLsizei imageSize,
                                         const void *data)
{
    return true;
}

bool ValidateCompressedTextureSubImage3D(Context *context,
                                         TextureID texture,
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
    return true;
}

bool ValidateCopyNamedBufferSubData(Context *context,
                                    GLuint readBuffer,
                                    GLuint writeBuffer,
                                    GLintptr readOffset,
                                    GLintptr writeOffset,
                                    GLsizeiptr size)
{
    return true;
}

bool ValidateCopyTextureSubImage1D(Context *context,
                                   TextureID texture,
                                   GLint level,
                                   GLint xoffset,
                                   GLint x,
                                   GLint y,
                                   GLsizei width)
{
    return true;
}

bool ValidateCopyTextureSubImage2D(Context *context,
                                   TextureID texture,
                                   GLint level,
                                   GLint xoffset,
                                   GLint yoffset,
                                   GLint x,
                                   GLint y,
                                   GLsizei width,
                                   GLsizei height)
{
    return true;
}

bool ValidateCopyTextureSubImage3D(Context *context,
                                   TextureID texture,
                                   GLint level,
                                   GLint xoffset,
                                   GLint yoffset,
                                   GLint zoffset,
                                   GLint x,
                                   GLint y,
                                   GLsizei width,
                                   GLsizei height)
{
    return true;
}

bool ValidateCreateBuffers(Context *context, GLsizei n, BufferID *buffers)
{
    return true;
}

bool ValidateCreateFramebuffers(Context *context, GLsizei n, GLuint *framebuffers)
{
    return true;
}

bool ValidateCreateProgramPipelines(Context *context, GLsizei n, GLuint *pipelines)
{
    return true;
}

bool ValidateCreateQueries(Context *context, GLenum target, GLsizei n, GLuint *ids)
{
    return true;
}

bool ValidateCreateRenderbuffers(Context *context, GLsizei n, RenderbufferID *renderbuffers)
{
    return true;
}

bool ValidateCreateSamplers(Context *context, GLsizei n, GLuint *samplers)
{
    return true;
}

bool ValidateCreateTextures(Context *context, GLenum target, GLsizei n, GLuint *textures)
{
    return true;
}

bool ValidateCreateTransformFeedbacks(Context *context, GLsizei n, GLuint *ids)
{
    return true;
}

bool ValidateCreateVertexArrays(Context *context, GLsizei n, VertexArrayID *arrays)
{
    return true;
}

bool ValidateDisableVertexArrayAttrib(Context *context, VertexArrayID vaobj, GLuint index)
{
    return true;
}

bool ValidateEnableVertexArrayAttrib(Context *context, VertexArrayID vaobj, GLuint index)
{
    return true;
}

bool ValidateFlushMappedNamedBufferRange(Context *context,
                                         BufferID buffer,
                                         GLintptr offset,
                                         GLsizeiptr length)
{
    return true;
}

bool ValidateGenerateTextureMipmap(Context *context, TextureID texture)
{
    return true;
}

bool ValidateGetCompressedTextureImage(Context *context,
                                       TextureID texture,
                                       GLint level,
                                       GLsizei bufSize,
                                       void *pixels)
{
    return true;
}

bool ValidateGetCompressedTextureSubImage(Context *context,
                                          TextureID texture,
                                          GLint level,
                                          GLint xoffset,
                                          GLint yoffset,
                                          GLint zoffset,
                                          GLsizei width,
                                          GLsizei height,
                                          GLsizei depth,
                                          GLsizei bufSize,
                                          void *pixels)
{
    return true;
}

bool ValidateGetNamedBufferParameteri64v(Context *context,
                                         BufferID buffer,
                                         GLenum pname,
                                         GLint64 *params)
{
    return true;
}

bool ValidateGetNamedBufferParameteriv(Context *context,
                                       BufferID buffer,
                                       GLenum pname,
                                       GLint *params)
{
    return true;
}

bool ValidateGetNamedBufferPointerv(Context *context, BufferID buffer, GLenum pname, void **params)
{
    return true;
}

bool ValidateGetNamedBufferSubData(Context *context,
                                   BufferID buffer,
                                   GLintptr offset,
                                   GLsizeiptr size,
                                   void *data)
{
    return true;
}

bool ValidateGetNamedFramebufferAttachmentParameteriv(Context *context,
                                                      FramebufferID framebuffer,
                                                      GLenum attachment,
                                                      GLenum pname,
                                                      GLint *params)
{
    return true;
}

bool ValidateGetNamedFramebufferParameteriv(Context *context,
                                            FramebufferID framebuffer,
                                            GLenum pname,
                                            GLint *param)
{
    return true;
}

bool ValidateGetNamedRenderbufferParameteriv(Context *context,
                                             RenderbufferID renderbuffer,
                                             GLenum pname,
                                             GLint *params)
{
    return true;
}

bool ValidateGetQueryBufferObjecti64v(Context *context,
                                      GLuint id,
                                      BufferID buffer,
                                      GLenum pname,
                                      GLintptr offset)
{
    return true;
}

bool ValidateGetQueryBufferObjectiv(Context *context,
                                    GLuint id,
                                    BufferID buffer,
                                    GLenum pname,
                                    GLintptr offset)
{
    return true;
}

bool ValidateGetQueryBufferObjectui64v(Context *context,
                                       GLuint id,
                                       BufferID buffer,
                                       GLenum pname,
                                       GLintptr offset)
{
    return true;
}

bool ValidateGetQueryBufferObjectuiv(Context *context,
                                     GLuint id,
                                     BufferID buffer,
                                     GLenum pname,
                                     GLintptr offset)
{
    return true;
}

bool ValidateGetTextureImage(Context *context,
                             TextureID texture,
                             GLint level,
                             GLenum format,
                             GLenum type,
                             GLsizei bufSize,
                             void *pixels)
{
    return true;
}

bool ValidateGetTextureLevelParameterfv(Context *context,
                                        TextureID texture,
                                        GLint level,
                                        GLenum pname,
                                        GLfloat *params)
{
    return true;
}

bool ValidateGetTextureLevelParameteriv(Context *context,
                                        TextureID texture,
                                        GLint level,
                                        GLenum pname,
                                        GLint *params)
{
    return true;
}

bool ValidateGetTextureParameterIiv(Context *context,
                                    TextureID texture,
                                    GLenum pname,
                                    GLint *params)
{
    return true;
}

bool ValidateGetTextureParameterIuiv(Context *context,
                                     TextureID texture,
                                     GLenum pname,
                                     GLuint *params)
{
    return true;
}

bool ValidateGetTextureParameterfv(Context *context,
                                   TextureID texture,
                                   GLenum pname,
                                   GLfloat *params)
{
    return true;
}

bool ValidateGetTextureParameteriv(Context *context, TextureID texture, GLenum pname, GLint *params)
{
    return true;
}

bool ValidateGetTextureSubImage(Context *context,
                                TextureID texture,
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
                                void *pixels)
{
    return true;
}

bool ValidateGetTransformFeedbacki64_v(Context *context,
                                       GLuint xfb,
                                       GLenum pname,
                                       GLuint index,
                                       GLint64 *param)
{
    return true;
}

bool ValidateGetTransformFeedbacki_v(Context *context,
                                     GLuint xfb,
                                     GLenum pname,
                                     GLuint index,
                                     GLint *param)
{
    return true;
}

bool ValidateGetTransformFeedbackiv(Context *context, GLuint xfb, GLenum pname, GLint *param)
{
    return true;
}

bool ValidateGetVertexArrayIndexed64iv(Context *context,
                                       VertexArrayID vaobj,
                                       GLuint index,
                                       GLenum pname,
                                       GLint64 *param)
{
    return true;
}

bool ValidateGetVertexArrayIndexediv(Context *context,
                                     VertexArrayID vaobj,
                                     GLuint index,
                                     GLenum pname,
                                     GLint *param)
{
    return true;
}

bool ValidateGetVertexArrayiv(Context *context, VertexArrayID vaobj, GLenum pname, GLint *param)
{
    return true;
}

bool ValidateGetnColorTable(Context *context,
                            GLenum target,
                            GLenum format,
                            GLenum type,
                            GLsizei bufSize,
                            void *table)
{
    return true;
}

bool ValidateGetnCompressedTexImage(Context *context,
                                    GLenum target,
                                    GLint lod,
                                    GLsizei bufSize,
                                    void *pixels)
{
    return true;
}

bool ValidateGetnConvolutionFilter(Context *context,
                                   GLenum target,
                                   GLenum format,
                                   GLenum type,
                                   GLsizei bufSize,
                                   void *image)
{
    return true;
}

bool ValidateGetnHistogram(Context *context,
                           GLenum target,
                           GLboolean reset,
                           GLenum format,
                           GLenum type,
                           GLsizei bufSize,
                           void *values)
{
    return true;
}

bool ValidateGetnMapdv(Context *context, GLenum target, GLenum query, GLsizei bufSize, GLdouble *v)
{
    return true;
}

bool ValidateGetnMapfv(Context *context, GLenum target, GLenum query, GLsizei bufSize, GLfloat *v)
{
    return true;
}

bool ValidateGetnMapiv(Context *context, GLenum target, GLenum query, GLsizei bufSize, GLint *v)
{
    return true;
}

bool ValidateGetnMinmax(Context *context,
                        GLenum target,
                        GLboolean reset,
                        GLenum format,
                        GLenum type,
                        GLsizei bufSize,
                        void *values)
{
    return true;
}

bool ValidateGetnPixelMapfv(Context *context, GLenum map, GLsizei bufSize, GLfloat *values)
{
    return true;
}

bool ValidateGetnPixelMapuiv(Context *context, GLenum map, GLsizei bufSize, GLuint *values)
{
    return true;
}

bool ValidateGetnPixelMapusv(Context *context, GLenum map, GLsizei bufSize, GLushort *values)
{
    return true;
}

bool ValidateGetnPolygonStipple(Context *context, GLsizei bufSize, GLubyte *pattern)
{
    return true;
}

bool ValidateGetnSeparableFilter(Context *context,
                                 GLenum target,
                                 GLenum format,
                                 GLenum type,
                                 GLsizei rowBufSize,
                                 void *row,
                                 GLsizei columnBufSize,
                                 void *column,
                                 void *span)
{
    return true;
}

bool ValidateGetnTexImage(Context *context,
                          GLenum target,
                          GLint level,
                          GLenum format,
                          GLenum type,
                          GLsizei bufSize,
                          void *pixels)
{
    return true;
}

bool ValidateGetnUniformdv(Context *context,
                           ShaderProgramID program,
                           GLint location,
                           GLsizei bufSize,
                           GLdouble *params)
{
    return true;
}

bool ValidateInvalidateNamedFramebufferData(Context *context,
                                            FramebufferID framebuffer,
                                            GLsizei numAttachments,
                                            const GLenum *attachments)
{
    return true;
}

bool ValidateInvalidateNamedFramebufferSubData(Context *context,
                                               FramebufferID framebuffer,
                                               GLsizei numAttachments,
                                               const GLenum *attachments,
                                               GLint x,
                                               GLint y,
                                               GLsizei width,
                                               GLsizei height)
{
    return true;
}

bool ValidateMapNamedBuffer(Context *context, BufferID buffer, GLenum access)
{
    return true;
}

bool ValidateMapNamedBufferRange(Context *context,
                                 BufferID buffer,
                                 GLintptr offset,
                                 GLsizeiptr length,
                                 GLbitfield access)
{
    return true;
}

bool ValidateNamedBufferData(Context *context,
                             BufferID buffer,
                             GLsizeiptr size,
                             const void *data,
                             GLenum usage)
{
    return true;
}

bool ValidateNamedBufferStorage(Context *context,
                                BufferID buffer,
                                GLsizeiptr size,
                                const void *data,
                                GLbitfield flags)
{
    return true;
}

bool ValidateNamedBufferSubData(Context *context,
                                BufferID buffer,
                                GLintptr offset,
                                GLsizeiptr size,
                                const void *data)
{
    return true;
}

bool ValidateNamedFramebufferDrawBuffer(Context *context, FramebufferID framebuffer, GLenum buf)
{
    return true;
}

bool ValidateNamedFramebufferDrawBuffers(Context *context,
                                         FramebufferID framebuffer,
                                         GLsizei n,
                                         const GLenum *bufs)
{
    return true;
}

bool ValidateNamedFramebufferParameteri(Context *context,
                                        FramebufferID framebuffer,
                                        GLenum pname,
                                        GLint param)
{
    return true;
}

bool ValidateNamedFramebufferReadBuffer(Context *context, FramebufferID framebuffer, GLenum src)
{
    return true;
}

bool ValidateNamedFramebufferRenderbuffer(Context *context,
                                          FramebufferID framebuffer,
                                          GLenum attachment,
                                          GLenum renderbuffertarget,
                                          RenderbufferID renderbuffer)
{
    return true;
}

bool ValidateNamedFramebufferTexture(Context *context,
                                     FramebufferID framebuffer,
                                     GLenum attachment,
                                     TextureID texture,
                                     GLint level)
{
    return true;
}

bool ValidateNamedFramebufferTextureLayer(Context *context,
                                          FramebufferID framebuffer,
                                          GLenum attachment,
                                          TextureID texture,
                                          GLint level,
                                          GLint layer)
{
    return true;
}

bool ValidateNamedRenderbufferStorage(Context *context,
                                      RenderbufferID renderbuffer,
                                      GLenum internalformat,
                                      GLsizei width,
                                      GLsizei height)
{
    return true;
}

bool ValidateNamedRenderbufferStorageMultisample(Context *context,
                                                 RenderbufferID renderbuffer,
                                                 GLsizei samples,
                                                 GLenum internalformat,
                                                 GLsizei width,
                                                 GLsizei height)
{
    return true;
}

bool ValidateTextureBarrier(Context *context)
{
    return true;
}

bool ValidateTextureBuffer(Context *context,
                           TextureID texture,
                           GLenum internalformat,
                           BufferID buffer)
{
    return true;
}

bool ValidateTextureBufferRange(Context *context,
                                TextureID texture,
                                GLenum internalformat,
                                BufferID buffer,
                                GLintptr offset,
                                GLsizeiptr size)
{
    return true;
}

bool ValidateTextureParameterIiv(Context *context,
                                 TextureID texture,
                                 GLenum pname,
                                 const GLint *params)
{
    return true;
}

bool ValidateTextureParameterIuiv(Context *context,
                                  TextureID texture,
                                  GLenum pname,
                                  const GLuint *params)
{
    return true;
}

bool ValidateTextureParameterf(Context *context, TextureID texture, GLenum pname, GLfloat param)
{
    return true;
}

bool ValidateTextureParameterfv(Context *context,
                                TextureID texture,
                                GLenum pname,
                                const GLfloat *param)
{
    return true;
}

bool ValidateTextureParameteri(Context *context, TextureID texture, GLenum pname, GLint param)
{
    return true;
}

bool ValidateTextureParameteriv(Context *context,
                                TextureID texture,
                                GLenum pname,
                                const GLint *param)
{
    return true;
}

bool ValidateTextureStorage1D(Context *context,
                              TextureID texture,
                              GLsizei levels,
                              GLenum internalformat,
                              GLsizei width)
{
    return true;
}

bool ValidateTextureStorage2D(Context *context,
                              TextureID texture,
                              GLsizei levels,
                              GLenum internalformat,
                              GLsizei width,
                              GLsizei height)
{
    return true;
}

bool ValidateTextureStorage2DMultisample(Context *context,
                                         TextureID texture,
                                         GLsizei samples,
                                         GLenum internalformat,
                                         GLsizei width,
                                         GLsizei height,
                                         GLboolean fixedsamplelocations)
{
    return true;
}

bool ValidateTextureStorage3D(Context *context,
                              TextureID texture,
                              GLsizei levels,
                              GLenum internalformat,
                              GLsizei width,
                              GLsizei height,
                              GLsizei depth)
{
    return true;
}

bool ValidateTextureStorage3DMultisample(Context *context,
                                         TextureID texture,
                                         GLsizei samples,
                                         GLenum internalformat,
                                         GLsizei width,
                                         GLsizei height,
                                         GLsizei depth,
                                         GLboolean fixedsamplelocations)
{
    return true;
}

bool ValidateTextureSubImage1D(Context *context,
                               TextureID texture,
                               GLint level,
                               GLint xoffset,
                               GLsizei width,
                               GLenum format,
                               GLenum type,
                               const void *pixels)
{
    return true;
}

bool ValidateTextureSubImage2D(Context *context,
                               TextureID texture,
                               GLint level,
                               GLint xoffset,
                               GLint yoffset,
                               GLsizei width,
                               GLsizei height,
                               GLenum format,
                               GLenum type,
                               const void *pixels)
{
    return true;
}

bool ValidateTextureSubImage3D(Context *context,
                               TextureID texture,
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
    return true;
}

bool ValidateTransformFeedbackBufferBase(Context *context,
                                         GLuint xfb,
                                         GLuint index,
                                         BufferID buffer)
{
    return true;
}

bool ValidateTransformFeedbackBufferRange(Context *context,
                                          GLuint xfb,
                                          GLuint index,
                                          BufferID buffer,
                                          GLintptr offset,
                                          GLsizeiptr size)
{
    return true;
}

bool ValidateUnmapNamedBuffer(Context *context, BufferID buffer)
{
    return true;
}

bool ValidateVertexArrayAttribBinding(Context *context,
                                      VertexArrayID vaobj,
                                      GLuint attribindex,
                                      GLuint bindingindex)
{
    return true;
}

bool ValidateVertexArrayAttribFormat(Context *context,
                                     VertexArrayID vaobj,
                                     GLuint attribindex,
                                     GLint size,
                                     GLenum type,
                                     GLboolean normalized,
                                     GLuint relativeoffset)
{
    return true;
}

bool ValidateVertexArrayAttribIFormat(Context *context,
                                      VertexArrayID vaobj,
                                      GLuint attribindex,
                                      GLint size,
                                      GLenum type,
                                      GLuint relativeoffset)
{
    return true;
}

bool ValidateVertexArrayAttribLFormat(Context *context,
                                      VertexArrayID vaobj,
                                      GLuint attribindex,
                                      GLint size,
                                      GLenum type,
                                      GLuint relativeoffset)
{
    return true;
}

bool ValidateVertexArrayBindingDivisor(Context *context,
                                       VertexArrayID vaobj,
                                       GLuint bindingindex,
                                       GLuint divisor)
{
    return true;
}

bool ValidateVertexArrayElementBuffer(Context *context, VertexArrayID vaobj, BufferID buffer)
{
    return true;
}

bool ValidateVertexArrayVertexBuffer(Context *context,
                                     VertexArrayID vaobj,
                                     GLuint bindingindex,
                                     BufferID buffer,
                                     GLintptr offset,
                                     GLsizei stride)
{
    return true;
}

bool ValidateVertexArrayVertexBuffers(Context *context,
                                      VertexArrayID vaobj,
                                      GLuint first,
                                      GLsizei count,
                                      const BufferID *buffers,
                                      const GLintptr *offsets,
                                      const GLsizei *strides)
{
    return true;
}

}  // namespace gl
