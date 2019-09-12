//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// getProcAddress loader table:
//   Mapping from a string entry point name to function address.
//

#ifndef LIBGL_PROC_TABLE_H_
#define LIBGL_PROC_TABLE_H_

// Define _GDI32_ so that wingdi.h doesn't declare functions as imports
#ifndef _GDI32_
#    define _GDI32_
#endif

#include <angle_gl.h>

#include <WGL/wgl.h>
#include <stddef.h>
#include <utility>

// So that windows file winnt.h doesn't substitute for MemoryBarrier function
#ifdef MemoryBarrier
#    undef MemoryBarrier
#endif

namespace wgl
{
using ProcEntry = std::pair<const char *, PROC>;

extern wgl::ProcEntry g_procTable[];
extern size_t g_numProcs;
}  // namespace wgl

#endif  // LIBGL_PROC_TABLE_H_
