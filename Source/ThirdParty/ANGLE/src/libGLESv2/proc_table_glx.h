//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// getProcAddress loader table:
//   Mapping from a string entry point name to function address.
//

#ifndef LIBGL_GLX_PROC_TABLE_H_
#define LIBGL_GLX_PROC_TABLE_H_

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <angle_gl.h>
#include <stddef.h>
#include <utility>

using GLXPixmap   = XID;
using GLXDrawable = XID;
using GLXPbuffer  = XID;
using GLXContext  = XID;
#include <GLX/glxext.h>

namespace glx
{
using ProcEntry = std::pair<const char *, __GLXextFuncPtr>;

extern const glx::ProcEntry g_procTable[];
extern const size_t g_numProcs;
}  // namespace glx

#endif  // LIBGL_GLX_PROC_TABLE_H_
