//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Metal/MTLAccelerationStructureTypes.hpp
//
// Copyright 2020-2021 Apple Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//-------------------------------------------------------------------------------------------------------------------------------------------------------------

#pragma once

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

#include "MTLDefines.hpp"
#include "MTLPrivate.hpp"
#include "MTLResource.hpp"
#include "MTLStageInputOutputDescriptor.hpp"

#include "../Foundation/NSRange.hpp"

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

namespace MTL
{
struct PackedFloat3
{
    PackedFloat3();
    PackedFloat3(float x, float y, float z);

    float& operator[](int idx);
    float  operator[](int idx) const;

    union
    {
        struct
        {
            float x;
            float y;
            float z;
        };

        float elements[3];
    };
} _MTL_PACKED;

struct PackedFloat4x3
{
    PackedFloat4x3();
    PackedFloat4x3(const PackedFloat3& col0, const PackedFloat3& col1, const PackedFloat3& col2, const PackedFloat3& col3);

    PackedFloat3&       operator[](int idx);
    const PackedFloat3& operator[](int idx) const;

    PackedFloat3        columns[4];
} _MTL_PACKED;

struct AxisAlignedBoundingBox
{
    AxisAlignedBoundingBox();
    AxisAlignedBoundingBox(PackedFloat3 p);
    AxisAlignedBoundingBox(PackedFloat3 min, PackedFloat3 max);

    PackedFloat3 min;
    PackedFloat3 max;
} _MTL_PACKED;
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

_MTL_INLINE MTL::PackedFloat3::PackedFloat3()
    : x(0.0f)
    , y(0.0f)
    , z(0.0f)
{
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

_MTL_INLINE MTL::PackedFloat3::PackedFloat3(float _x, float _y, float _z)
    : x(_x)
    , y(_y)
    , z(_z)
{
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

_MTL_INLINE float& MTL::PackedFloat3::operator[](int idx)
{
    return elements[idx];
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

_MTL_INLINE float MTL::PackedFloat3::operator[](int idx) const
{
    return elements[idx];
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

_MTL_INLINE MTL::PackedFloat4x3::PackedFloat4x3()
{
    columns[0] = PackedFloat3(0.0f, 0.0f, 0.0f);
    columns[1] = PackedFloat3(0.0f, 0.0f, 0.0f);
    columns[2] = PackedFloat3(0.0f, 0.0f, 0.0f);
    columns[3] = PackedFloat3(0.0f, 0.0f, 0.0f);
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

_MTL_INLINE MTL::PackedFloat4x3::PackedFloat4x3(const PackedFloat3& col0, const PackedFloat3& col1, const PackedFloat3& col2, const PackedFloat3& col3)
{
    columns[0] = col0;
    columns[1] = col1;
    columns[2] = col2;
    columns[3] = col3;
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

_MTL_INLINE MTL::PackedFloat3& MTL::PackedFloat4x3::operator[](int idx)
{
    return columns[idx];
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

_MTL_INLINE const MTL::PackedFloat3& MTL::PackedFloat4x3::operator[](int idx) const
{
    return columns[idx];
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

_MTL_INLINE MTL::AxisAlignedBoundingBox::AxisAlignedBoundingBox()
    : min(INFINITY, INFINITY, INFINITY)
    , max(-INFINITY, -INFINITY, -INFINITY)
{
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

_MTL_INLINE MTL::AxisAlignedBoundingBox::AxisAlignedBoundingBox(PackedFloat3 p)
    : min(p)
    , max(p)
{
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

_MTL_INLINE MTL::AxisAlignedBoundingBox::AxisAlignedBoundingBox(PackedFloat3 _min, PackedFloat3 _max)
    : min(_min)
    , max(_max)
{
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------
