//
// Copyright 2023 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ProgramExecutableNULL.h: Implementation of ProgramExecutableImpl.

#ifndef LIBANGLE_RENDERER_NULL_PROGRAMEXECUTABLENULL_H_
#define LIBANGLE_RENDERER_NULL_PROGRAMEXECUTABLENULL_H_

#include "libANGLE/ProgramExecutable.h"
#include "libANGLE/renderer/ProgramExecutableImpl.h"

namespace rx
{
class ProgramExecutableNULL : public ProgramExecutableImpl
{
  public:
    ProgramExecutableNULL(const gl::ProgramExecutable *executable);
    ~ProgramExecutableNULL() override;

    void destroy(const gl::Context *context) override;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_NULL_PROGRAMEXECUTABLENULL_H_
