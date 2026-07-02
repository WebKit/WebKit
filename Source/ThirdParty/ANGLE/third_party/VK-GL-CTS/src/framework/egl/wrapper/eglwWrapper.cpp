/*-------------------------------------------------------------------------
 * drawElements Quality Program EGL Utilities
 * ------------------------------------------
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
 * \brief EGL API Library.
 *//*--------------------------------------------------------------------*/

#include "eglwWrapper.hpp"
#include "eglwLibrary.hpp"

namespace eglw
{
static const eglw::Library *s_library = nullptr;

void setCurrentThreadLibrary(const eglw::Library *egl)
{
    s_library = egl;
}

inline const eglw::Library *getCurrentThreadLibrary(void)
{
    return s_library;
}

DE_BEGIN_EXTERN_C

#include "eglwImpl.inl"
#include "eglwImplExt.inl"

DE_END_EXTERN_C
} // namespace eglw
