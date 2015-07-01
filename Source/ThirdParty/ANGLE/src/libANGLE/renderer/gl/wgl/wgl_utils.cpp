//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// wgl_utils.cpp: Utility routines specific to the WGL->EGL implementation.

#include "libANGLE/renderer/gl/wgl/wgl_utils.h"

namespace rx
{

namespace wgl
{

PIXELFORMATDESCRIPTOR GetDefaultPixelFormatDescriptor()
{
    PIXELFORMATDESCRIPTOR pixelFormatDescriptor = { 0 };
    pixelFormatDescriptor.nSize = sizeof(pixelFormatDescriptor);
    pixelFormatDescriptor.nVersion = 1;
    pixelFormatDescriptor.dwFlags = PFD_DRAW_TO_WINDOW | PFD_GENERIC_ACCELERATED | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pixelFormatDescriptor.iPixelType = PFD_TYPE_RGBA;
    pixelFormatDescriptor.cColorBits = 24;
    pixelFormatDescriptor.cAlphaBits = 8;
    pixelFormatDescriptor.cDepthBits = 24;
    pixelFormatDescriptor.cStencilBits = 8;
    pixelFormatDescriptor.iLayerType = PFD_MAIN_PLANE;

    return pixelFormatDescriptor;
}

}

}
