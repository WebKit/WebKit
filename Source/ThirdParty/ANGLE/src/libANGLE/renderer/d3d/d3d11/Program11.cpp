//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Program11: D3D11 implementation of an OpenGL Program.

#include "libANGLE/renderer/d3d/d3d11/Program11.h"

#include "libANGLE/Context.h"
#include "libANGLE/renderer/d3d/d3d11/Context11.h"
#include "libANGLE/renderer/d3d/d3d11/Renderer11.h"
#include "libANGLE/renderer/d3d/d3d11/StateManager11.h"

namespace rx
{
Program11::Program11(const gl::ProgramState &programState, Renderer11 *renderer)
    : ProgramD3D(programState, renderer)
{}

Program11::~Program11() = default;

angle::Result Program11::syncState(const gl::Context *context,
                                   const gl::Program::DirtyBits &dirtyBits)
{
    Renderer11 *renderer11       = GetImplAs<Context11>(context)->getRenderer();
    StateManager11 *stateManager = renderer11->getStateManager();

    // This single flag should be replace by individual dirtyness.
    stateManager->invalidateProgramUniformBuffers();

    return angle::Result::Continue;
}
}  // namespace rx
