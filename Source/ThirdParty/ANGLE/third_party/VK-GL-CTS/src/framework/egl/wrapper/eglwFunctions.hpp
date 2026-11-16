#ifndef _EGLWFUNCTIONS_HPP
#define _EGLWFUNCTIONS_HPP
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
 * \brief EGL API Functions.
 *//*--------------------------------------------------------------------*/

#include "eglwDefs.hpp"

namespace eglw
{

// Function typedefs.
extern "C"
{
#include "eglwFunctionTypes.inl"
}

class Functions
{
public:
    // Function definitions:
    // eglInitializeFunc initialize;
#include "eglwFunctions.inl"

    Functions(void);
};

typedef EGLW_APICALL void(EGLW_APIENTRY *GenericFuncType)(void);

class FunctionLoader
{
public:
    virtual ~FunctionLoader()
    {
    }
    virtual GenericFuncType get(const char *name) const = 0;
};

void initCore(Functions *dst, const FunctionLoader *loader);
void initExtensions(Functions *dst, const FunctionLoader *loader);

} // namespace eglw

#endif // _EGLWFUNCTIONS_HPP
