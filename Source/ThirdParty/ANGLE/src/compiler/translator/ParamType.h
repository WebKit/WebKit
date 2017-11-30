//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ParamType:
//   Helper type for built-in function emulator tables. Defines types for parameters.

#ifndef COMPILER_TRANSLATOR_PARAMTYPE_H_
#define COMPILER_TRANSLATOR_PARAMTYPE_H_

#include "common/angleutils.h"
#include "compiler/translator/BaseTypes.h"

namespace sh
{

enum class ParamType : uint8_t
{
    Void,
    Bool1,
    Bool2,
    Bool3,
    Bool4,
    Float1,
    Float2,
    Float3,
    Float4,
    Int1,
    Int2,
    Int3,
    Int4,
    Mat2,
    Mat3,
    Mat4,
    Uint1,
    Uint2,
    Uint3,
    Uint4,
    Last,
};

struct ParamTypeInfo
{
    ParamType self;
    TBasicType basicType;
    int primarySize;
    int secondarySize;
};

constexpr ParamTypeInfo g_ParamTypeInfo[] = {
    {ParamType::Void, EbtVoid, 1, 1},    {ParamType::Bool1, EbtBool, 1, 1},
    {ParamType::Bool2, EbtBool, 2, 1},   {ParamType::Bool3, EbtBool, 3, 1},
    {ParamType::Bool4, EbtBool, 4, 1},   {ParamType::Float1, EbtFloat, 1, 1},
    {ParamType::Float2, EbtFloat, 2, 1}, {ParamType::Float3, EbtFloat, 3, 1},
    {ParamType::Float4, EbtFloat, 4, 1}, {ParamType::Int1, EbtInt, 1, 1},
    {ParamType::Int2, EbtInt, 2, 1},     {ParamType::Int3, EbtInt, 3, 1},
    {ParamType::Int4, EbtInt, 4, 1},     {ParamType::Mat2, EbtFloat, 2, 2},
    {ParamType::Mat3, EbtFloat, 3, 3},   {ParamType::Mat4, EbtFloat, 4, 4},
    {ParamType::Uint1, EbtUInt, 1, 1},   {ParamType::Uint2, EbtUInt, 2, 1},
    {ParamType::Uint3, EbtUInt, 3, 1},   {ParamType::Uint4, EbtUInt, 4, 1},
};

constexpr size_t ParamTypeIndex(ParamType paramType)
{
    return static_cast<size_t>(paramType);
}

constexpr size_t NumParamTypes()
{
    return ParamTypeIndex(ParamType::Last);
}

static_assert(ArraySize(g_ParamTypeInfo) == NumParamTypes(), "Invalid array size");

constexpr TBasicType GetBasicType(ParamType paramType)
{
    return g_ParamTypeInfo[ParamTypeIndex(paramType)].basicType;
}

constexpr int GetPrimarySize(ParamType paramType)
{
    return g_ParamTypeInfo[ParamTypeIndex(paramType)].primarySize;
}

constexpr int GetSecondarySize(ParamType paramType)
{
    return g_ParamTypeInfo[ParamTypeIndex(paramType)].secondarySize;
}

constexpr bool SameParamType(ParamType paramType,
                             TBasicType basicType,
                             int primarySize,
                             int secondarySize)
{
    return GetBasicType(paramType) == basicType && primarySize == GetPrimarySize(paramType) &&
           secondarySize == GetSecondarySize(paramType);
}

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_PARAMTYPE_H_
