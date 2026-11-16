#ifndef _TCUVECTORTYPE_HPP
#define _TCUVECTORTYPE_HPP
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
 * \brief Vector type forward declarations.
 *
 * This header should be included instead of tcuVector.h if only vector
 * type name is required. Especially headers should avoid including full
 * Vector<T> implementation.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuFloat.hpp"

namespace tcu
{

template <typename T, int Size>
class Vector;

typedef Vector<float, 1> Vec1;
typedef Vector<float, 2> Vec2;
typedef Vector<float, 3> Vec3;
typedef Vector<float, 4> Vec4;
typedef Vector<float, 5> Vec5;

typedef Vector<tcu::Float16, 1> F16Vec1;
typedef Vector<tcu::Float16, 2> F16Vec2;
typedef Vector<tcu::Float16, 3> F16Vec3;
typedef Vector<tcu::Float16, 4> F16Vec4;
typedef Vector<tcu::Float16, 5> F16Vec5;

typedef Vector<int, 1> IVec1;
typedef Vector<int, 2> IVec2;
typedef Vector<int, 3> IVec3;
typedef Vector<int, 4> IVec4;
typedef Vector<int, 5> IVec5;

typedef Vector<uint32_t, 1> UVec1;
typedef Vector<uint32_t, 2> UVec2;
typedef Vector<uint32_t, 3> UVec3;
typedef Vector<uint32_t, 4> UVec4;
typedef Vector<uint32_t, 5> UVec5;

typedef Vector<int64_t, 1> I64Vec1;
typedef Vector<int64_t, 2> I64Vec2;
typedef Vector<int64_t, 3> I64Vec3;
typedef Vector<int64_t, 4> I64Vec4;
typedef Vector<int64_t, 5> I64Vec5;

typedef Vector<uint64_t, 1> U64Vec1;
typedef Vector<uint64_t, 2> U64Vec2;
typedef Vector<uint64_t, 3> U64Vec3;
typedef Vector<uint64_t, 4> U64Vec4;
typedef Vector<uint64_t, 5> U64Vec5;

typedef Vector<bool, 1> BVec1;
typedef Vector<bool, 2> BVec2;
typedef Vector<bool, 3> BVec3;
typedef Vector<bool, 4> BVec4;
typedef Vector<bool, 5> BVec5;

typedef Vector<double, 2> DVec2;
typedef Vector<double, 3> DVec3;
typedef Vector<double, 4> DVec4;
typedef Vector<double, 5> DVec5;

} // namespace tcu

#endif // _TCUVECTORTYPE_HPP
