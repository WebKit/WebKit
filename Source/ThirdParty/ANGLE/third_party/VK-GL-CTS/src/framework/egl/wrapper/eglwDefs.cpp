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

#include "eglwDefs.hpp"

namespace eglw
{

// Verify typedefs

DE_STATIC_ASSERT(sizeof(EGLint) == 4);
DE_STATIC_ASSERT(sizeof(EGLBoolean) == 4);
DE_STATIC_ASSERT(sizeof(EGLAttrib) == sizeof(void *));
DE_STATIC_ASSERT(sizeof(EGLTime) == 8);

} // namespace eglw
