//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationGL45.cpp: Validation functions for OpenGL 4.5 entry point parameters

#include "libANGLE/validationGL45_autogen.h"

namespace gl
{

bool ValidateBindTextureUnit(const Context *context, GLuint unit, TextureID texture)
{
    return true;
}

bool ValidateBlitNamedFramebuffer(const Context *context,
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

bool ValidateCheckNamedFramebufferStatus(const Context *context,
                                         FramebufferID framebuffer,
                                         GLenum target)
{
    return true;
}

bool ValidateClearNamedBufferData(const Context *context,
                                  BufferID buffer,
                                  GLenum internalformat,
                                  GLenum format,
                                  GLenum type,
                                  const void *data)
{
    return true;
}

bool ValidateClearNamedBufferSubData(const Context *context,
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

bool ValidateClearNamedFramebufferfi(const Context *context,
                                     FramebufferID framebuffer,
                                     GLenum buffer,
                                     GLint drawbuffer,
                                     GLfloat depth,
                                     GLint stencil)
{
    return true;
}

bool ValidateClearNamedFramebufferfv(const Context *context,
                                     FramebufferID framebuffer,
                                     GLenum buffer,
                                     GLint drawbuffer,
                                     const GLfloat *value)
{
    return true;
}

bool ValidateClearNamedFramebufferiv(const Context *context,
                                     FramebufferID framebuffer,
                                     GLenum buffer,
                                     GLint drawbuffer,
                                     const GLint *value)
{
    return true;
}

bool ValidateClearNamedFramebufferuiv(const Context *context,
                                      FramebufferID framebuffer,
                                      GLenum buffer,
                                      GLint drawbuffer,
                                      const GLuint *value)
{
    return true;
}

bool ValidateClipControl(const Context *context, GLenum origin, GLenum depth)
{
    return true;
}

bool ValidateCompressedTextureSubImage1D(const Context *context,
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

bool ValidateCompressedTextureSubImage2D(const Context *context,
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

bool ValidateCompressedTextureSubImage3D(const Context *context,
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

bool ValidateCopyNamedBufferSubData(const Context *context,
                                    GLuint readBuffer,
                                    GLuint writeBuffer,
                                    GLintptr readOffset,
                                    GLintptr writeOffset,
                                    GLsizeiptr size)
{
    return true;
}

bool ValidateCopyTextureSubImage1D(const Context *context,
                                   TextureID texture,
                                   GLint level,
                                   GLint xoffset,
                                   GLint x,
                                   GLint y,
                                   GLsizei width)
{
    return true;
}

bool ValidateCopyTextureSubImage2D(const Context *context,
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

bool ValidateCopyTextureSubImage3D(const Context *context,
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

bool ValidateCreateBuffers(const Context *context, GLsizei n, const BufferID *buffers)
{
    return true;
}

bool ValidateCreateFramebuffers(const Context *context, GLsizei n, const GLuint *framebuffers)
{
    return true;
}

bool ValidateCreateProgramPipelines(const Context *context, GLsizei n, const GLuint *pipelines)
{
    return true;
}

bool ValidateCreateQueries(const Context *context, GLenum target, GLsizei n, const GLuint *ids)
{
    return true;
}

bool ValidateCreateRenderbuffers(const Context *context,
                                 GLsizei n,
                                 const RenderbufferID *renderbuffers)
{
    return true;
}

bool ValidateCreateSamplers(const Context *context, GLsizei n, const GLuint *samplers)
{
    return true;
}

bool ValidateCreateTextures(const Context *context,
                            GLenum target,
                            GLsizei n,
                            const GLuint *textures)
{
    return true;
}

bool ValidateCreateTransformFeedbacks(const Context *context, GLsizei n, const GLuint *ids)
{
    return true;
}

bool ValidateCreateVertexArrays(const Context *context, GLsizei n, const VertexArrayID *arrays)
{
    return true;
}

bool ValidateDisableVertexArrayAttrib(const Context *context, VertexArrayID vaobj, GLuint index)
{
    return true;
}

bool ValidateEnableVertexArrayAttrib(const Context *context, VertexArrayID vaobj, GLuint index)
{
    return true;
}

bool ValidateFlushMappedNamedBufferRange(const Context *context,
                                         BufferID buffer,
                                         GLintptr offset,
                                         GLsizeiptr length)
{
    return true;
}

bool ValidateGenerateTextureMipmap(const Context *context, TextureID texture)
{
    return true;
}

bool ValidateGetCompressedTextureImage(const Context *context,
                                       TextureID texture,
                                       GLint level,
                                       GLsizei bufSize,
                                       const void *pixels)
{
    return true;
}

bool ValidateGetCompressedTextureSubImage(const Context *context,
                                          TextureID texture,
                                          GLint level,
                                          GLint xoffset,
                                          GLint yoffset,
                                          GLint zoffset,
                                          GLsizei width,
                                          GLsizei height,
                                          GLsizei depth,
                                          GLsizei bufSize,
                                          const void *pixels)
{
    return true;
}

bool ValidateGetNamedBufferParameteri64v(const Context *context,
                                         BufferID buffer,
                                         GLenum pname,
                                         const GLint64 *params)
{
    return true;
}

bool ValidateGetNamedBufferParameteriv(const Context *context,
                                       BufferID buffer,
                                       GLenum pname,
                                       const GLint *params)
{
    return true;
}

bool ValidateGetNamedBufferPointerv(const Context *context,
                                    BufferID buffer,
                                    GLenum pname,
                                    void *const *params)
{
    return true;
}

bool ValidateGetNamedBufferSubData(const Context *context,
                                   BufferID buffer,
                                   GLintptr offset,
                                   GLsizeiptr size,
                                   const void *data)
{
    return true;
}

bool ValidateGetNamedFramebufferAttachmentParameteriv(const Context *context,
                                                      FramebufferID framebuffer,
                                                      GLenum attachment,
                                                      GLenum pname,
                                                      const GLint *params)
{
    return true;
}

bool ValidateGetNamedFramebufferParameteriv(const Context *context,
                                            FramebufferID framebuffer,
                                            GLenum pname,
                                            const GLint *param)
{
    return true;
}

bool ValidateGetNamedRenderbufferParameteriv(const Context *context,
                                             RenderbufferID renderbuffer,
                                             GLenum pname,
                                             const GLint *params)
{
    return true;
}

bool ValidateGetQueryBufferObjecti64v(const Context *context,
                                      GLuint id,
                                      BufferID buffer,
                                      GLenum pname,
                                      GLintptr offset)
{
    return true;
}

bool ValidateGetQueryBufferObjectiv(const Context *context,
                                    GLuint id,
                                    BufferID buffer,
                                    GLenum pname,
                                    GLintptr offset)
{
    return true;
}

bool ValidateGetQueryBufferObjectui64v(const Context *context,
                                       GLuint id,
                                       BufferID buffer,
                                       GLenum pname,
                                       GLintptr offset)
{
    return true;
}

bool ValidateGetQueryBufferObjectuiv(const Context *context,
                                     GLuint id,
                                     BufferID buffer,
                                     GLenum pname,
                                     GLintptr offset)
{
    return true;
}

bool ValidateGetTextureImage(const Context *context,
                             TextureID texture,
                             GLint level,
                             GLenum format,
                             GLenum type,
                             GLsizei bufSize,
                             const void *pixels)
{
    return true;
}

bool ValidateGetTextureLevelParameterfv(const Context *context,
                                        TextureID texture,
                                        GLint level,
                                        GLenum pname,
                                        const GLfloat *params)
{
    return true;
}

bool ValidateGetTextureLevelParameteriv(const Context *context,
                                        TextureID texture,
                                        GLint level,
                                        GLenum pname,
                                        const GLint *params)
{
    return true;
}

bool ValidateGetTextureParameterIiv(const Context *context,
                                    TextureID texture,
                                    GLenum pname,
                                    const GLint *params)
{
    return true;
}

bool ValidateGetTextureParameterIuiv(const Context *context,
                                     TextureID texture,
                                     GLenum pname,
                                     const GLuint *params)
{
    return true;
}

bool ValidateGetTextureParameterfv(const Context *context,
                                   TextureID texture,
                                   GLenum pname,
                                   const GLfloat *params)
{
    return true;
}

bool ValidateGetTextureParameteriv(const Context *context,
                                   TextureID texture,
                                   GLenum pname,
                                   const GLint *params)
{
    return true;
}

bool ValidateGetTextureSubImage(const Context *context,
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
                                const void *pixels)
{
    return true;
}

bool ValidateGetTransformFeedbacki64_v(const Context *context,
                                       GLuint xfb,
                                       GLenum pname,
                                       GLuint index,
                                       const GLint64 *param)
{
    return true;
}

bool ValidateGetTransformFeedbacki_v(const Context *context,
                                     GLuint xfb,
                                     GLenum pname,
                                     GLuint index,
                                     const GLint *param)
{
    return true;
}

bool ValidateGetTransformFeedbackiv(const Context *context,
                                    GLuint xfb,
                                    GLenum pname,
                                    const GLint *param)
{
    return true;
}

bool ValidateGetVertexArrayIndexed64iv(const Context *context,
                                       VertexArrayID vaobj,
                                       GLuint index,
                                       GLenum pname,
                                       const GLint64 *param)
{
    return true;
}

bool ValidateGetVertexArrayIndexediv(const Context *context,
                                     VertexArrayID vaobj,
                                     GLuint index,
                                     GLenum pname,
                                     const GLint *param)
{
    return true;
}

bool ValidateGetVertexArrayiv(const Context *context,
                              VertexArrayID vaobj,
                              GLenum pname,
                              const GLint *param)
{
    return true;
}

bool ValidateGetnColorTable(const Context *context,
                            GLenum target,
                            GLenum format,
                            GLenum type,
                            GLsizei bufSize,
                            const void *table)
{
    return true;
}

bool ValidateGetnCompressedTexImage(const Context *context,
                                    GLenum target,
                                    GLint lod,
                                    GLsizei bufSize,
                                    const void *pixels)
{
    return true;
}

bool ValidateGetnConvolutionFilter(const Context *context,
                                   GLenum target,
                                   GLenum format,
                                   GLenum type,
                                   GLsizei bufSize,
                                   const void *image)
{
    return true;
}

bool ValidateGetnHistogram(const Context *context,
                           GLenum target,
                           GLboolean reset,
                           GLenum format,
                           GLenum type,
                           GLsizei bufSize,
                           const void *values)
{
    return true;
}

bool ValidateGetnMapdv(const Context *context,
                       GLenum target,
                       GLenum query,
                       GLsizei bufSize,
                       const GLdouble *v)
{
    return true;
}

bool ValidateGetnMapfv(const Context *context,
                       GLenum target,
                       GLenum query,
                       GLsizei bufSize,
                       const GLfloat *v)
{
    return true;
}

bool ValidateGetnMapiv(const Context *context,
                       GLenum target,
                       GLenum query,
                       GLsizei bufSize,
                       const GLint *v)
{
    return true;
}

bool ValidateGetnMinmax(const Context *context,
                        GLenum target,
                        GLboolean reset,
                        GLenum format,
                        GLenum type,
                        GLsizei bufSize,
                        const void *values)
{
    return true;
}

bool ValidateGetnPixelMapfv(const Context *context,
                            GLenum map,
                            GLsizei bufSize,
                            const GLfloat *values)
{
    return true;
}

bool ValidateGetnPixelMapuiv(const Context *context,
                             GLenum map,
                             GLsizei bufSize,
                             const GLuint *values)
{
    return true;
}

bool ValidateGetnPixelMapusv(const Context *context,
                             GLenum map,
                             GLsizei bufSize,
                             const GLushort *values)
{
    return true;
}

bool ValidateGetnPolygonStipple(const Context *context, GLsizei bufSize, const GLubyte *pattern)
{
    return true;
}

bool ValidateGetnSeparableFilter(const Context *context,
                                 GLenum target,
                                 GLenum format,
                                 GLenum type,
                                 GLsizei rowBufSize,
                                 const void *row,
                                 GLsizei columnBufSize,
                                 const void *column,
                                 const void *span)
{
    return true;
}

bool ValidateGetnTexImage(const Context *context,
                          GLenum target,
                          GLint level,
                          GLenum format,
                          GLenum type,
                          GLsizei bufSize,
                          const void *pixels)
{
    return true;
}

bool ValidateGetnUniformdv(const Context *context,
                           ShaderProgramID program,
                           UniformLocation location,
                           GLsizei bufSize,
                           const GLdouble *params)
{
    return true;
}

bool ValidateInvalidateNamedFramebufferData(const Context *context,
                                            FramebufferID framebuffer,
                                            GLsizei numAttachments,
                                            const GLenum *attachments)
{
    return true;
}

bool ValidateInvalidateNamedFramebufferSubData(const Context *context,
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

bool ValidateMapNamedBuffer(const Context *context, BufferID buffer, GLenum access)
{
    return true;
}

bool ValidateMapNamedBufferRange(const Context *context,
                                 BufferID buffer,
                                 GLintptr offset,
                                 GLsizeiptr length,
                                 GLbitfield access)
{
    return true;
}

bool ValidateNamedBufferData(const Context *context,
                             BufferID buffer,
                             GLsizeiptr size,
                             const void *data,
                             GLenum usage)
{
    return true;
}

bool ValidateNamedBufferStorage(const Context *context,
                                BufferID buffer,
                                GLsizeiptr size,
                                const void *data,
                                GLbitfield flags)
{
    return true;
}

bool ValidateNamedBufferSubData(const Context *context,
                                BufferID buffer,
                                GLintptr offset,
                                GLsizeiptr size,
                                const void *data)
{
    return true;
}

bool ValidateNamedFramebufferDrawBuffer(const Context *context,
                                        FramebufferID framebuffer,
                                        GLenum buf)
{
    return true;
}

bool ValidateNamedFramebufferDrawBuffers(const Context *context,
                                         FramebufferID framebuffer,
                                         GLsizei n,
                                         const GLenum *bufs)
{
    return true;
}

bool ValidateNamedFramebufferParameteri(const Context *context,
                                        FramebufferID framebuffer,
                                        GLenum pname,
                                        GLint param)
{
    return true;
}

bool ValidateNamedFramebufferReadBuffer(const Context *context,
                                        FramebufferID framebuffer,
                                        GLenum src)
{
    return true;
}

bool ValidateNamedFramebufferRenderbuffer(const Context *context,
                                          FramebufferID framebuffer,
                                          GLenum attachment,
                                          GLenum renderbuffertarget,
                                          RenderbufferID renderbuffer)
{
    return true;
}

bool ValidateNamedFramebufferTexture(const Context *context,
                                     FramebufferID framebuffer,
                                     GLenum attachment,
                                     TextureID texture,
                                     GLint level)
{
    return true;
}

bool ValidateNamedFramebufferTextureLayer(const Context *context,
                                          FramebufferID framebuffer,
                                          GLenum attachment,
                                          TextureID texture,
                                          GLint level,
                                          GLint layer)
{
    return true;
}

bool ValidateNamedRenderbufferStorage(const Context *context,
                                      RenderbufferID renderbuffer,
                                      GLenum internalformat,
                                      GLsizei width,
                                      GLsizei height)
{
    return true;
}

bool ValidateNamedRenderbufferStorageMultisample(const Context *context,
                                                 RenderbufferID renderbuffer,
                                                 GLsizei samples,
                                                 GLenum internalformat,
                                                 GLsizei width,
                                                 GLsizei height)
{
    return true;
}

bool ValidateTextureBarrier(const Context *context)
{
    return true;
}

bool ValidateTextureBuffer(const Context *context,
                           TextureID texture,
                           GLenum internalformat,
                           BufferID buffer)
{
    return true;
}

bool ValidateTextureBufferRange(const Context *context,
                                TextureID texture,
                                GLenum internalformat,
                                BufferID buffer,
                                GLintptr offset,
                                GLsizeiptr size)
{
    return true;
}

bool ValidateTextureParameterIiv(const Context *context,
                                 TextureID texture,
                                 GLenum pname,
                                 const GLint *params)
{
    return true;
}

bool ValidateTextureParameterIuiv(const Context *context,
                                  TextureID texture,
                                  GLenum pname,
                                  const GLuint *params)
{
    return true;
}

bool ValidateTextureParameterf(const Context *context,
                               TextureID texture,
                               GLenum pname,
                               GLfloat param)
{
    return true;
}

bool ValidateTextureParameterfv(const Context *context,
                                TextureID texture,
                                GLenum pname,
                                const GLfloat *param)
{
    return true;
}

bool ValidateTextureParameteri(const Context *context, TextureID texture, GLenum pname, GLint param)
{
    return true;
}

bool ValidateTextureParameteriv(const Context *context,
                                TextureID texture,
                                GLenum pname,
                                const GLint *param)
{
    return true;
}

bool ValidateTextureStorage1D(const Context *context,
                              TextureID texture,
                              GLsizei levels,
                              GLenum internalformat,
                              GLsizei width)
{
    return true;
}

bool ValidateTextureStorage2D(const Context *context,
                              TextureID texture,
                              GLsizei levels,
                              GLenum internalformat,
                              GLsizei width,
                              GLsizei height)
{
    return true;
}

bool ValidateTextureStorage2DMultisample(const Context *context,
                                         TextureID texture,
                                         GLsizei samples,
                                         GLenum internalformat,
                                         GLsizei width,
                                         GLsizei height,
                                         GLboolean fixedsamplelocations)
{
    return true;
}

bool ValidateTextureStorage3D(const Context *context,
                              TextureID texture,
                              GLsizei levels,
                              GLenum internalformat,
                              GLsizei width,
                              GLsizei height,
                              GLsizei depth)
{
    return true;
}

bool ValidateTextureStorage3DMultisample(const Context *context,
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

bool ValidateTextureSubImage1D(const Context *context,
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

bool ValidateTextureSubImage2D(const Context *context,
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

bool ValidateTextureSubImage3D(const Context *context,
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

bool ValidateTransformFeedbackBufferBase(const Context *context,
                                         GLuint xfb,
                                         GLuint index,
                                         BufferID buffer)
{
    return true;
}

bool ValidateTransformFeedbackBufferRange(const Context *context,
                                          GLuint xfb,
                                          GLuint index,
                                          BufferID buffer,
                                          GLintptr offset,
                                          GLsizeiptr size)
{
    return true;
}

bool ValidateUnmapNamedBuffer(const Context *context, BufferID buffer)
{
    return true;
}

bool ValidateVertexArrayAttribBinding(const Context *context,
                                      VertexArrayID vaobj,
                                      GLuint attribindex,
                                      GLuint bindingindex)
{
    return true;
}

bool ValidateVertexArrayAttribFormat(const Context *context,
                                     VertexArrayID vaobj,
                                     GLuint attribindex,
                                     GLint size,
                                     GLenum type,
                                     GLboolean normalized,
                                     GLuint relativeoffset)
{
    return true;
}

bool ValidateVertexArrayAttribIFormat(const Context *context,
                                      VertexArrayID vaobj,
                                      GLuint attribindex,
                                      GLint size,
                                      GLenum type,
                                      GLuint relativeoffset)
{
    return true;
}

bool ValidateVertexArrayAttribLFormat(const Context *context,
                                      VertexArrayID vaobj,
                                      GLuint attribindex,
                                      GLint size,
                                      GLenum type,
                                      GLuint relativeoffset)
{
    return true;
}

bool ValidateVertexArrayBindingDivisor(const Context *context,
                                       VertexArrayID vaobj,
                                       GLuint bindingindex,
                                       GLuint divisor)
{
    return true;
}

bool ValidateVertexArrayElementBuffer(const Context *context, VertexArrayID vaobj, BufferID buffer)
{
    return true;
}

bool ValidateVertexArrayVertexBuffer(const Context *context,
                                     VertexArrayID vaobj,
                                     GLuint bindingindex,
                                     BufferID buffer,
                                     GLintptr offset,
                                     GLsizei stride)
{
    return true;
}

bool ValidateVertexArrayVertexBuffers(const Context *context,
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
