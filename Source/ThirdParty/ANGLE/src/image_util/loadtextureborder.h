//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// loadtextureborder.h: Defines border color load functions.

#ifndef IMAGEUTIL_LOADTEXTUREBORDER_H_
#define IMAGEUTIL_LOADTEXTUREBORDER_H_

#include <stddef.h>
#include <stdint.h>
#include "common/Color.h"
namespace angle
{

void LoadA8ToR8(angle::ColorF &mBorderColor);

void LoadLA8ToR8G8(angle::ColorF &mBorderColor);

void LoadToNative(angle::ColorF &mBorderColor);

}  // namespace angle

#endif  // IMAGEUTIL_LOADTEXTUREBORDER_H_
