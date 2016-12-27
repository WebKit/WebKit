//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// queryutils.h: Utilities for querying values from GL objects

#ifndef LIBANGLE_QUERYUTILS_H_
#define LIBANGLE_QUERYUTILS_H_

#include "angle_gl.h"
#include "common/angleutils.h"

namespace gl
{
class Buffer;
class Framebuffer;
class Program;
class Renderbuffer;
class Sampler;
class Shader;
class Texture;
struct TextureCaps;
struct UniformBlock;
struct VertexAttribute;
struct VertexAttribCurrentValueData;

void QueryFramebufferAttachmentParameteriv(const Framebuffer *framebuffer,
                                           GLenum attachment,
                                           GLenum pname,
                                           GLint *params);
void QueryBufferParameteriv(const Buffer *buffer, GLenum pname, GLint *params);
void QueryBufferParameteri64v(const Buffer *buffer, GLenum pname, GLint64 *params);
void QueryBufferPointerv(const Buffer *buffer, GLenum pname, void **params);
void QueryProgramiv(const Program *program, GLenum pname, GLint *params);
void QueryRenderbufferiv(const Renderbuffer *renderbuffer, GLenum pname, GLint *params);
void QueryShaderiv(const Shader *shader, GLenum pname, GLint *params);
void QueryTexParameterfv(const Texture *texture, GLenum pname, GLfloat *params);
void QueryTexParameteriv(const Texture *texture, GLenum pname, GLint *params);
void QuerySamplerParameterfv(const Sampler *sampler, GLenum pname, GLfloat *params);
void QuerySamplerParameteriv(const Sampler *sampler, GLenum pname, GLint *params);
void QueryVertexAttribfv(const VertexAttribute &attrib,
                         const VertexAttribCurrentValueData &currentValueData,
                         GLenum pname,
                         GLfloat *params);
void QueryVertexAttribiv(const VertexAttribute &attrib,
                         const VertexAttribCurrentValueData &currentValueData,
                         GLenum pname,
                         GLint *params);
void QueryVertexAttribPointerv(const VertexAttribute &attrib, GLenum pname, GLvoid **pointer);
void QueryVertexAttribIiv(const VertexAttribute &attrib,
                          const VertexAttribCurrentValueData &currentValueData,
                          GLenum pname,
                          GLint *params);
void QueryVertexAttribIuiv(const VertexAttribute &attrib,
                           const VertexAttribCurrentValueData &currentValueData,
                           GLenum pname,
                           GLuint *params);

void QueryActiveUniformBlockiv(const Program *program,
                               GLuint uniformBlockIndex,
                               GLenum pname,
                               GLint *params);

void QueryInternalFormativ(const TextureCaps &format, GLenum pname, GLsizei bufSize, GLint *params);

void SetTexParameterf(Texture *texture, GLenum pname, GLfloat param);
void SetTexParameterfv(Texture *texture, GLenum pname, const GLfloat *params);
void SetTexParameteri(Texture *texture, GLenum pname, GLint param);
void SetTexParameteriv(Texture *texture, GLenum pname, const GLint *params);

void SetSamplerParameterf(Sampler *sampler, GLenum pname, GLfloat param);
void SetSamplerParameterfv(Sampler *sampler, GLenum pname, const GLfloat *params);
void SetSamplerParameteri(Sampler *sampler, GLenum pname, GLint param);
void SetSamplerParameteriv(Sampler *sampler, GLenum pname, const GLint *params);
}

#endif  // LIBANGLE_QUERYUTILS_H_
