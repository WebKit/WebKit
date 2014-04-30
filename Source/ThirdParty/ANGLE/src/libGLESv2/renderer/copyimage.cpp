#include "precompiled.h"
//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// copyimage.cpp: Defines image copying functions

#include "libGLESv2/renderer/copyImage.h"

namespace rx
{

void CopyBGRAUByteToRGBAUByte(const void *source, void *dest)
{
    unsigned int argb = *(unsigned int*)source;
    *(unsigned int*)dest = (argb & 0xFF00FF00) |       // Keep alpha and green
                           (argb & 0x00FF0000) >> 16 | // Move red to blue
                           (argb & 0x000000FF) << 16;  // Move blue to red
}

}
