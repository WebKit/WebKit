#ifndef _EGLWLIBRARY_HPP
#define _EGLWLIBRARY_HPP
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

#include "eglwDefs.hpp"
#include "eglwFunctions.hpp"

namespace de
{
class DynamicLibrary;
}

namespace eglw
{

class Library
{
public:
    virtual ~Library()
    {
    }

    // Entry points:
    // virtual EGLBoolean initialize (EGLDisplay ...) const = 0;
#include "eglwLibrary.inl"
};

class FuncPtrLibrary : public Library
{
public:
    FuncPtrLibrary(void);
    ~FuncPtrLibrary(void);

#include "eglwFuncPtrLibraryDecl.inl"

protected:
    Functions m_egl;
};

class DefaultLibrary : public FuncPtrLibrary
{
public:
    DefaultLibrary(const char *dynamicLibraryName = getLibraryFileName());
    ~DefaultLibrary(void);

    static const char *getLibraryFileName(void);

protected:
    de::DynamicLibrary *m_dynLib;
};

} // namespace eglw

#endif // _EGLWLIBRARY_HPP
