/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL Utilities
 * ---------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief OpenGL wrapper base types and definitions.
 *
 * This header defines all standard OpenGL types using drawElements Base
 * Portability Library (delibs) types.
 *//*--------------------------------------------------------------------*/

#include "glwDefs.hpp"

namespace glw
{

// Verify typedefs

DE_STATIC_ASSERT(sizeof(GLbyte) == 1);
DE_STATIC_ASSERT(sizeof(GLubyte) == 1);
DE_STATIC_ASSERT(sizeof(GLshort) == 2);
DE_STATIC_ASSERT(sizeof(GLushort) == 2);
DE_STATIC_ASSERT(sizeof(GLint) == 4);
DE_STATIC_ASSERT(sizeof(GLuint) == 4);
DE_STATIC_ASSERT(sizeof(GLint64) == 8);
DE_STATIC_ASSERT(sizeof(GLuint64) == 8);

DE_STATIC_ASSERT(sizeof(GLhalf) == 2);
DE_STATIC_ASSERT(sizeof(GLfloat) == 4);
DE_STATIC_ASSERT(sizeof(GLclampf) == 4);
DE_STATIC_ASSERT(sizeof(GLdouble) == 8);
DE_STATIC_ASSERT(sizeof(GLclampd) == 8);

DE_STATIC_ASSERT(sizeof(GLchar) == 1);
DE_STATIC_ASSERT(sizeof(GLboolean) == 1);
DE_STATIC_ASSERT(sizeof(GLenum) == 4);
DE_STATIC_ASSERT(sizeof(GLbitfield) == 4);
DE_STATIC_ASSERT(sizeof(GLsizei) == 4);
DE_STATIC_ASSERT(sizeof(GLfixed) == 4);
DE_STATIC_ASSERT(sizeof(GLintptr) == sizeof(void *));
DE_STATIC_ASSERT(sizeof(GLsizeiptr) == sizeof(void *));

} // namespace glw
