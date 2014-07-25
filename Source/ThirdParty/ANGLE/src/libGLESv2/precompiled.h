//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// precompiled.h: Precompiled header file for libGLESv2.

#define GL_APICALL
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#include <GLES2/gl2.h>

#define GL_GLEXT_PROTOTYPES
#include <GLES2/gl2ext.h>

#define EGLAPI
#include <EGL/egl.h>

#include <assert.h>
#include <cstddef>
#include <float.h>
#include <stdint.h>
#include <intrin.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <algorithm> // for std::min and std::max
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(ANGLE_ENABLE_D3D9)
#include <d3d9.h>
#include <D3Dcompiler.h>
#endif // ANGLE_ENABLE_D3D9

#if defined(ANGLE_ENABLE_D3D11)
#include <D3D10_1.h>
#include <D3D11.h>
#include <dxgi.h>
#if !ANGLE_SKIP_DXGI_1_2_CHECK
#include <dxgi1_2.h>
#endif
#include <D3Dcompiler.h>
#endif // ANGLE_ENABLE_D3D11
