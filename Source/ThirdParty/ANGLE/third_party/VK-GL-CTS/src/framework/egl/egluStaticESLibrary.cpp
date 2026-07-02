/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
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
 * \brief EGL common defines and types
 *//*--------------------------------------------------------------------*/

#include "egluStaticESLibrary.hpp"
#include "tcuFunctionLibrary.hpp"

#define STATIC_LIB_NONE 0
#define STATIC_LIB_ES20 1
#define STATIC_LIB_ES30 2
#define STATIC_LIB_ES31 3
#define STATIC_LIB_ES32 4

#if defined(DEQP_GLES32_DIRECT_LINK)
#if (DE_OS == DE_OS_IOS)
#include <OpenGLES/ES32/gl.h>
#else
#include <GLES3/gl32.h>
#endif
#define STATIC_LIB STATIC_LIB_ES32
#elif defined(DEQP_GLES31_DIRECT_LINK)
#if (DE_OS == DE_OS_IOS)
#include <OpenGLES/ES31/gl.h>
#else
#include <GLES3/gl31.h>
#endif
#define STATIC_LIB STATIC_LIB_ES31
#elif defined(DEQP_GLES3_DIRECT_LINK)
#if (DE_OS == DE_OS_IOS)
#include <OpenGLES/ES3/gl.h>
#else
#include <GLES3/gl3.h>
#endif
#define STATIC_LIB STATIC_LIB_ES30
#elif defined(DEQP_GLES2_DIRECT_LINK)
#if (DE_OS == DE_OS_IOS)
#include <OpenGLES/ES2/gl.h>
#else
#include <GLES2/gl2.h>
#endif
#define STATIC_LIB STATIC_LIB_ES20
#else
#define STATIC_LIB STATIC_LIB_NONE
#endif

namespace eglu
{

tcu::FunctionLibrary *createStaticESLibrary(void)
{
#if (STATIC_LIB == STATIC_LIB_NONE)
    return new tcu::StaticFunctionLibrary(nullptr, 0);
#else
    static const tcu::StaticFunctionLibrary::Entry s_functions[] = {
#if (STATIC_LIB == STATIC_LIB_ES32)
#include "egluStaticES32Library.inl"
#elif (STATIC_LIB == STATIC_LIB_ES31)
#include "egluStaticES31Library.inl"
#elif (STATIC_LIB == STATIC_LIB_ES30)
#include "egluStaticES30Library.inl"
#elif (STATIC_LIB == STATIC_LIB_ES20)
#include "egluStaticES20Library.inl"
#else
#error "Unknown STATIC_LIB value"
#endif
    };

    return new tcu::StaticFunctionLibrary(&s_functions[0], DE_LENGTH_OF_ARRAY(s_functions));
#endif
}

} // namespace eglu
