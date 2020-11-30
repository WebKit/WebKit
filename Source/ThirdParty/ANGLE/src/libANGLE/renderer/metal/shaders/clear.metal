//
// Copyright 2019 The ANGLE Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// clear.metal: Implements viewport clearing.

#include "common.h"

struct ClearParams
{
    float4 clearColor;
    float clearDepth;
};

vertex float4 clearVS(unsigned int vid [[ vertex_id ]],
                      constant ClearParams &clearParams [[buffer(0)]])
{
    return float4(gCorners[vid], clearParams.clearDepth, 1.0);
}

fragment float4 clearFS(constant ClearParams &clearParams [[buffer(0)]])
{
    return clearParams.clearColor;
}
