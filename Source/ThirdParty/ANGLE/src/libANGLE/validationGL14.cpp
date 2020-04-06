//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationGL14.cpp: Validation functions for OpenGL 1.4 entry point parameters

#include "libANGLE/validationGL14_autogen.h"

namespace gl
{

bool ValidateFogCoordPointer(const Context *context,
                             GLenum type,
                             GLsizei stride,
                             const void *pointer)
{
    return true;
}

bool ValidateFogCoordd(const Context *context, GLdouble coord)
{
    return true;
}

bool ValidateFogCoorddv(const Context *context, const GLdouble *coord)
{
    return true;
}

bool ValidateFogCoordf(const Context *context, GLfloat coord)
{
    return true;
}

bool ValidateFogCoordfv(const Context *context, const GLfloat *coord)
{
    return true;
}

bool ValidateMultiDrawArrays(const Context *context,
                             PrimitiveMode modePacked,
                             const GLint *first,
                             const GLsizei *count,
                             GLsizei drawcount)
{
    return true;
}

bool ValidateMultiDrawElements(const Context *context,
                               PrimitiveMode modePacked,
                               const GLsizei *count,
                               DrawElementsType typePacked,
                               const void *const *indices,
                               GLsizei drawcount)
{
    return true;
}

bool ValidatePointParameteri(const Context *context, GLenum pname, GLint param)
{
    return true;
}

bool ValidatePointParameteriv(const Context *context, GLenum pname, const GLint *params)
{
    return true;
}

bool ValidateSecondaryColor3b(const Context *context, GLbyte red, GLbyte green, GLbyte blue)
{
    return true;
}

bool ValidateSecondaryColor3bv(const Context *context, const GLbyte *v)
{
    return true;
}

bool ValidateSecondaryColor3d(const Context *context, GLdouble red, GLdouble green, GLdouble blue)
{
    return true;
}

bool ValidateSecondaryColor3dv(const Context *context, const GLdouble *v)
{
    return true;
}

bool ValidateSecondaryColor3f(const Context *context, GLfloat red, GLfloat green, GLfloat blue)
{
    return true;
}

bool ValidateSecondaryColor3fv(const Context *context, const GLfloat *v)
{
    return true;
}

bool ValidateSecondaryColor3i(const Context *context, GLint red, GLint green, GLint blue)
{
    return true;
}

bool ValidateSecondaryColor3iv(const Context *context, const GLint *v)
{
    return true;
}

bool ValidateSecondaryColor3s(const Context *context, GLshort red, GLshort green, GLshort blue)
{
    return true;
}

bool ValidateSecondaryColor3sv(const Context *context, const GLshort *v)
{
    return true;
}

bool ValidateSecondaryColor3ub(const Context *context, GLubyte red, GLubyte green, GLubyte blue)
{
    return true;
}

bool ValidateSecondaryColor3ubv(const Context *context, const GLubyte *v)
{
    return true;
}

bool ValidateSecondaryColor3ui(const Context *context, GLuint red, GLuint green, GLuint blue)
{
    return true;
}

bool ValidateSecondaryColor3uiv(const Context *context, const GLuint *v)
{
    return true;
}

bool ValidateSecondaryColor3us(const Context *context, GLushort red, GLushort green, GLushort blue)
{
    return true;
}

bool ValidateSecondaryColor3usv(const Context *context, const GLushort *v)
{
    return true;
}

bool ValidateSecondaryColorPointer(const Context *context,
                                   GLint size,
                                   GLenum type,
                                   GLsizei stride,
                                   const void *pointer)
{
    return true;
}

bool ValidateWindowPos2d(const Context *context, GLdouble x, GLdouble y)
{
    return true;
}

bool ValidateWindowPos2dv(const Context *context, const GLdouble *v)
{
    return true;
}

bool ValidateWindowPos2f(const Context *context, GLfloat x, GLfloat y)
{
    return true;
}

bool ValidateWindowPos2fv(const Context *context, const GLfloat *v)
{
    return true;
}

bool ValidateWindowPos2i(const Context *context, GLint x, GLint y)
{
    return true;
}

bool ValidateWindowPos2iv(const Context *context, const GLint *v)
{
    return true;
}

bool ValidateWindowPos2s(const Context *context, GLshort x, GLshort y)
{
    return true;
}

bool ValidateWindowPos2sv(const Context *context, const GLshort *v)
{
    return true;
}

bool ValidateWindowPos3d(const Context *context, GLdouble x, GLdouble y, GLdouble z)
{
    return true;
}

bool ValidateWindowPos3dv(const Context *context, const GLdouble *v)
{
    return true;
}

bool ValidateWindowPos3f(const Context *context, GLfloat x, GLfloat y, GLfloat z)
{
    return true;
}

bool ValidateWindowPos3fv(const Context *context, const GLfloat *v)
{
    return true;
}

bool ValidateWindowPos3i(const Context *context, GLint x, GLint y, GLint z)
{
    return true;
}

bool ValidateWindowPos3iv(const Context *context, const GLint *v)
{
    return true;
}

bool ValidateWindowPos3s(const Context *context, GLshort x, GLshort y, GLshort z)
{
    return true;
}

bool ValidateWindowPos3sv(const Context *context, const GLshort *v)
{
    return true;
}

}  // namespace gl
