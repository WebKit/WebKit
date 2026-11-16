#ifndef _GLW_H
#define _GLW_H
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
 * \brief OpenGL wrapper.
 *
 * This wrapper exposes latest OpenGL core and OpenGL ES APIs in a manner
 * that is compatible with gl.h. Types and enumeration values are exposed
 * as-is. Functions are redirected to glw(Func) variants using preprocessor
 * defines.
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"

DE_BEGIN_EXTERN_C

/* Base types and defines. */
#include "glwTypes.inl"

/* Enumeration values. */
#include "glwEnums.inl"

/* API Versions. */
#include "glwVersions.inl"

/* Functions. */
#include "glwApi.inl"

DE_END_EXTERN_C

#endif /* _GLW_H */
