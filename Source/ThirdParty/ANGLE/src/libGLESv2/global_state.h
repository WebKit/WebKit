//
// Copyright(c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// global_state.h : Defines functions for querying the thread-local GL and EGL state.

#ifndef LIBGLESV2_GLOBALSTATE_H_
#define LIBGLESV2_GLOBALSTATE_H_

namespace gl
{
class Context;

Context *GetGlobalContext();
Context *GetValidGlobalContext();

}  // namespace gl

namespace egl
{
class Thread;

Thread *GetCurrentThread();

}  // namespace egl

#endif // LIBGLESV2_GLOBALSTATE_H_
