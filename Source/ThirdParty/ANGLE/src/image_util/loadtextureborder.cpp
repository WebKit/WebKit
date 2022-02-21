//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// loadtextureborder.cpp: Defines border color load functions.

#include "image_util/loadtextureborder.h"

namespace angle
{

void LoadA8ToR8(angle::ColorF &mBorderColor)
{
    mBorderColor.red   = mBorderColor.alpha;
    mBorderColor.alpha = 0;
}

void LoadLA8ToR8G8(angle::ColorF &mBorderColor)
{
    mBorderColor.green = mBorderColor.alpha;
    mBorderColor.alpha = 0;
}

void LoadToNative(angle::ColorF &mBorderColor) {}

}  // namespace angle
