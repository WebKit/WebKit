#ifndef _GLWDEFS_HPP
#define _GLWDEFS_HPP
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

#include "deDefs.hpp"

// Make __GLsync & other opaque types available in root namespace.
struct __GLsync;
struct _cl_context;
struct _cl_event;

/*--------------------------------------------------------------------*//*!
 * \brief OpenGL API
 *//*--------------------------------------------------------------------*/
namespace glw
{

// extern "C" since glwTypes.inl may contain function pointer types.
extern "C"
{

#include "glwTypes.inl"
}

} // namespace glw

#endif // _GLWDEFS_HPP
