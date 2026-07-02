#ifndef _GLWENUMS_HPP
#define _GLWENUMS_HPP
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
 * \brief OpenGL Enumeration Values
 * \note Do not include this anywhere where you'd like to include native
 *         GL headers or otherwise preprocessor will be rather unhappy about
 *         duplicate declarations.
 *
 * \todo [2013-04-08 pyry] This is .hpp just for consistency. Should we make
 *                           it .h instead since it doesn't contain anything
 *                           C++ -specific.
 *//*--------------------------------------------------------------------*/

#include "deDefs.hpp"

#include "glwEnums.inl"

#endif // _GLWENUMS_HPP
