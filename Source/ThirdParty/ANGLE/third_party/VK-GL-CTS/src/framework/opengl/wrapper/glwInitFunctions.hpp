#ifndef _GLWINITFUNCTIONS_HPP
#define _GLWINITFUNCTIONS_HPP
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
 * \brief Function table initialization.
 *//*--------------------------------------------------------------------*/

#include "glwDefs.hpp"
#include "glwFunctions.hpp"
#include "glwFunctionLoader.hpp"

namespace glw
{

void initES20(Functions *gl, const FunctionLoader *loader); //!< Load all OpenGL ES 2.0 functions.
void initES30(Functions *gl, const FunctionLoader *loader); //!< Load all OpenGL ES 3.0 functions.
void initES31(Functions *gl, const FunctionLoader *loader); //!< Load all OpenGL ES 3.1 functions.
void initES32(Functions *gl, const FunctionLoader *loader); //!< Load all OpenGL ES 3.2 functions.

void initGL30Core(Functions *gl, const FunctionLoader *loader); //!< Load all GL 3.0 core functions.
void initGL31Core(Functions *gl, const FunctionLoader *loader); //!< Load all GL 3.1 core functions.
void initGL32Core(Functions *gl, const FunctionLoader *loader); //!< Load all GL 3.2 core functions.
void initGL33Core(Functions *gl, const FunctionLoader *loader); //!< Load all GL 3.3 core functions.
void initGL40Core(Functions *gl, const FunctionLoader *loader); //!< Load all GL 4.0 core functions.
void initGL41Core(Functions *gl, const FunctionLoader *loader); //!< Load all GL 4.1 core functions.
void initGL42Core(Functions *gl, const FunctionLoader *loader); //!< Load all GL 4.2 core functions.
void initGL43Core(Functions *gl, const FunctionLoader *loader); //!< Load all GL 4.3 core functions.
void initGL44Core(Functions *gl, const FunctionLoader *loader); //!< Load all GL 4.4 core functions.
void initGL45Core(Functions *gl, const FunctionLoader *loader); //!< Load all GL 4.5 core functions.
void initGL46Core(Functions *gl, const FunctionLoader *loader); //!< Load all GL 4.6 core functions.

void initExtensionsGL(Functions *gl, const FunctionLoader *loader, int numExtensions,
                      const char *const *extensions); //!< Load all supported GL core extension functions.
void initExtensionsES(Functions *gl, const FunctionLoader *loader, int numExtensions,
                      const char *const *extensions); //!< Load all supported GLES extension functions.

} // namespace glw

#endif // _GLWINITFUNCTIONS_HPP
