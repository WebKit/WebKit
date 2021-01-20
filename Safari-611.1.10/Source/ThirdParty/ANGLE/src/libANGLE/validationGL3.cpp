//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// validationGL3.cpp: Validation functions for OpenGL 3.0 entry point parameters

#include "libANGLE/validationGL3_autogen.h"

namespace gl
{

bool ValidateBeginConditionalRender(const Context *context, GLuint id, GLenum mode)
{
    return true;
}

bool ValidateBindFragDataLocation(const Context *context,
                                  ShaderProgramID program,
                                  GLuint color,
                                  const GLchar *name)
{
    return true;
}

bool ValidateClampColor(const Context *context, GLenum target, GLenum clamp)
{
    return true;
}

bool ValidateEndConditionalRender(const Context *context)
{
    return true;
}

bool ValidateFramebufferTexture1D(const Context *context,
                                  GLenum target,
                                  GLenum attachment,
                                  TextureTarget textargetPacked,
                                  TextureID texture,
                                  GLint level)
{
    return true;
}

bool ValidateFramebufferTexture3D(const Context *context,
                                  GLenum target,
                                  GLenum attachment,
                                  TextureTarget textargetPacked,
                                  TextureID texture,
                                  GLint level,
                                  GLint zoffset)
{
    return true;
}

bool ValidateVertexAttribI1i(const Context *context, GLuint index, GLint x)
{
    return true;
}

bool ValidateVertexAttribI1iv(const Context *context, GLuint index, const GLint *v)
{
    return true;
}

bool ValidateVertexAttribI1ui(const Context *context, GLuint index, GLuint x)
{
    return true;
}

bool ValidateVertexAttribI1uiv(const Context *context, GLuint index, const GLuint *v)
{
    return true;
}

bool ValidateVertexAttribI2i(const Context *context, GLuint index, GLint x, GLint y)
{
    return true;
}

bool ValidateVertexAttribI2iv(const Context *context, GLuint index, const GLint *v)
{
    return true;
}

bool ValidateVertexAttribI2ui(const Context *context, GLuint index, GLuint x, GLuint y)
{
    return true;
}

bool ValidateVertexAttribI2uiv(const Context *context, GLuint index, const GLuint *v)
{
    return true;
}

bool ValidateVertexAttribI3i(const Context *context, GLuint index, GLint x, GLint y, GLint z)
{
    return true;
}

bool ValidateVertexAttribI3iv(const Context *context, GLuint index, const GLint *v)
{
    return true;
}

bool ValidateVertexAttribI3ui(const Context *context, GLuint index, GLuint x, GLuint y, GLuint z)
{
    return true;
}

bool ValidateVertexAttribI3uiv(const Context *context, GLuint index, const GLuint *v)
{
    return true;
}

bool ValidateVertexAttribI4bv(const Context *context, GLuint index, const GLbyte *v)
{
    return true;
}

bool ValidateVertexAttribI4sv(const Context *context, GLuint index, const GLshort *v)
{
    return true;
}

bool ValidateVertexAttribI4ubv(const Context *context, GLuint index, const GLubyte *v)
{
    return true;
}

bool ValidateVertexAttribI4usv(const Context *context, GLuint index, const GLushort *v)
{
    return true;
}

}  // namespace gl
