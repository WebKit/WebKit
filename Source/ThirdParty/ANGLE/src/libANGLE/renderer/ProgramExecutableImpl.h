//
// Copyright 2023 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ProgramExecutableImpl.h: Defines the abstract rx::ProgramExecutableImpl class.

#ifndef LIBANGLE_RENDERER_PROGRAMEXECUTABLEIMPL_H_
#define LIBANGLE_RENDERER_PROGRAMEXECUTABLEIMPL_H_

#include "common/angleutils.h"

namespace gl
{
class Context;
class ProgramExecutable;
}  // namespace gl

namespace rx
{
// ProgramExecutable holds the result of link.  The backend ProgramExecutable* classes similarly
// hold additonaly backend-specific link results.  A program's executable is changed on successful
// link.  This allows the program to continue to work with its existing executable despite a failed
// relink.
class ProgramExecutableImpl : angle::NonCopyable
{
  public:
    ProgramExecutableImpl(const gl::ProgramExecutable *executable) : mExecutable(executable) {}
    virtual ~ProgramExecutableImpl() {}

    virtual void destroy(const gl::Context *context) {}

    const gl::ProgramExecutable *getExecutable() const { return mExecutable; }

  protected:
    const gl::ProgramExecutable *mExecutable;
};
}  // namespace rx

#endif  // LIBANGLE_RENDERER_PROGRAMEXECUTABLEIMPL_H_
