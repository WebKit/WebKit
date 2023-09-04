//
// Copyright 2023 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ProgramExecutableNULL.cpp: Implementation of ProgramExecutableNULL.

#include "libANGLE/renderer/null/ProgramExecutableNULL.h"

namespace rx
{
ProgramExecutableNULL::ProgramExecutableNULL(const gl::ProgramExecutable *executable)
    : ProgramExecutableImpl(executable)
{}

ProgramExecutableNULL::~ProgramExecutableNULL() = default;

void ProgramExecutableNULL::destroy(const gl::Context *context) {}
}  // namespace rx
