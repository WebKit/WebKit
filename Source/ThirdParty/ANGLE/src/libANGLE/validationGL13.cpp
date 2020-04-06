//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationGL13.cpp: Validation functions for OpenGL 1.3 entry point parameters

#include "libANGLE/validationGL13_autogen.h"

namespace gl
{

bool ValidateCompressedTexImage1D(const Context *context,
                                  GLenum target,
                                  GLint level,
                                  GLenum internalformat,
                                  GLsizei width,
                                  GLint border,
                                  GLsizei imageSize,
                                  const void *data)
{
    return true;
}

bool ValidateCompressedTexSubImage1D(const Context *context,
                                     GLenum target,
                                     GLint level,
                                     GLint xoffset,
                                     GLsizei width,
                                     GLenum format,
                                     GLsizei imageSize,
                                     const void *data)
{
    return true;
}

bool ValidateGetCompressedTexImage(const Context *context,
                                   GLenum target,
                                   GLint level,
                                   const void *img)
{
    return true;
}

bool ValidateLoadTransposeMatrixd(const Context *context, const GLdouble *m)
{
    return true;
}

bool ValidateLoadTransposeMatrixf(const Context *context, const GLfloat *m)
{
    return true;
}

bool ValidateMultTransposeMatrixd(const Context *context, const GLdouble *m)
{
    return true;
}

bool ValidateMultTransposeMatrixf(const Context *context, const GLfloat *m)
{
    return true;
}

bool ValidateMultiTexCoord1d(const Context *context, GLenum target, GLdouble s)
{
    return true;
}

bool ValidateMultiTexCoord1dv(const Context *context, GLenum target, const GLdouble *v)
{
    return true;
}

bool ValidateMultiTexCoord1f(const Context *context, GLenum target, GLfloat s)
{
    return true;
}

bool ValidateMultiTexCoord1fv(const Context *context, GLenum target, const GLfloat *v)
{
    return true;
}

bool ValidateMultiTexCoord1i(const Context *context, GLenum target, GLint s)
{
    return true;
}

bool ValidateMultiTexCoord1iv(const Context *context, GLenum target, const GLint *v)
{
    return true;
}

bool ValidateMultiTexCoord1s(const Context *context, GLenum target, GLshort s)
{
    return true;
}

bool ValidateMultiTexCoord1sv(const Context *context, GLenum target, const GLshort *v)
{
    return true;
}

bool ValidateMultiTexCoord2d(const Context *context, GLenum target, GLdouble s, GLdouble t)
{
    return true;
}

bool ValidateMultiTexCoord2dv(const Context *context, GLenum target, const GLdouble *v)
{
    return true;
}

bool ValidateMultiTexCoord2f(const Context *context, GLenum target, GLfloat s, GLfloat t)
{
    return true;
}

bool ValidateMultiTexCoord2fv(const Context *context, GLenum target, const GLfloat *v)
{
    return true;
}

bool ValidateMultiTexCoord2i(const Context *context, GLenum target, GLint s, GLint t)
{
    return true;
}

bool ValidateMultiTexCoord2iv(const Context *context, GLenum target, const GLint *v)
{
    return true;
}

bool ValidateMultiTexCoord2s(const Context *context, GLenum target, GLshort s, GLshort t)
{
    return true;
}

bool ValidateMultiTexCoord2sv(const Context *context, GLenum target, const GLshort *v)
{
    return true;
}

bool ValidateMultiTexCoord3d(const Context *context,
                             GLenum target,
                             GLdouble s,
                             GLdouble t,
                             GLdouble r)
{
    return true;
}

bool ValidateMultiTexCoord3dv(const Context *context, GLenum target, const GLdouble *v)
{
    return true;
}

bool ValidateMultiTexCoord3f(const Context *context, GLenum target, GLfloat s, GLfloat t, GLfloat r)
{
    return true;
}

bool ValidateMultiTexCoord3fv(const Context *context, GLenum target, const GLfloat *v)
{
    return true;
}

bool ValidateMultiTexCoord3i(const Context *context, GLenum target, GLint s, GLint t, GLint r)
{
    return true;
}

bool ValidateMultiTexCoord3iv(const Context *context, GLenum target, const GLint *v)
{
    return true;
}

bool ValidateMultiTexCoord3s(const Context *context, GLenum target, GLshort s, GLshort t, GLshort r)
{
    return true;
}

bool ValidateMultiTexCoord3sv(const Context *context, GLenum target, const GLshort *v)
{
    return true;
}

bool ValidateMultiTexCoord4d(const Context *context,
                             GLenum target,
                             GLdouble s,
                             GLdouble t,
                             GLdouble r,
                             GLdouble q)
{
    return true;
}

bool ValidateMultiTexCoord4dv(const Context *context, GLenum target, const GLdouble *v)
{
    return true;
}

bool ValidateMultiTexCoord4fv(const Context *context, GLenum target, const GLfloat *v)
{
    return true;
}

bool ValidateMultiTexCoord4i(const Context *context,
                             GLenum target,
                             GLint s,
                             GLint t,
                             GLint r,
                             GLint q)
{
    return true;
}

bool ValidateMultiTexCoord4iv(const Context *context, GLenum target, const GLint *v)
{
    return true;
}

bool ValidateMultiTexCoord4s(const Context *context,
                             GLenum target,
                             GLshort s,
                             GLshort t,
                             GLshort r,
                             GLshort q)
{
    return true;
}

bool ValidateMultiTexCoord4sv(const Context *context, GLenum target, const GLshort *v)
{
    return true;
}

}  // namespace gl
