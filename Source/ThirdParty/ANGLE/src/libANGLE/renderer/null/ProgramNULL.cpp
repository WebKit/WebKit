//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ProgramNULL.cpp:
//    Implements the class methods for ProgramNULL.
//

#include "libANGLE/renderer/null/ProgramNULL.h"

#include "common/debug.h"

namespace rx
{
namespace
{
class LinkTaskNULL : public LinkTask
{
  public:
    ~LinkTaskNULL() override = default;
    std::vector<std::shared_ptr<LinkSubTask>> link(const gl::ProgramLinkedResources &resources,
                                                   const gl::ProgramMergedVaryings &mergedVaryings,
                                                   bool *areSubTasksOptionalOut) override
    {
        return {};
    }
    angle::Result getResult(const gl::Context *context, gl::InfoLog &infoLog) override
    {
        return angle::Result::Continue;
    }
};
}  // anonymous namespace

ProgramNULL::ProgramNULL(const gl::ProgramState &state) : ProgramImpl(state) {}

ProgramNULL::~ProgramNULL() {}

angle::Result ProgramNULL::load(const gl::Context *context,
                                gl::BinaryInputStream *stream,
                                std::shared_ptr<LinkTask> *loadTaskOut,
                                egl::CacheGetResult *resultOut)
{
    *loadTaskOut = {};
    *resultOut   = egl::CacheGetResult::GetSuccess;
    return angle::Result::Continue;
}

void ProgramNULL::save(const gl::Context *context, gl::BinaryOutputStream *stream) {}

void ProgramNULL::setBinaryRetrievableHint(bool retrievable) {}

void ProgramNULL::setSeparable(bool separable) {}

angle::Result ProgramNULL::link(const gl::Context *contextImpl,
                                std::shared_ptr<LinkTask> *linkTaskOut)
{
    *linkTaskOut = std::shared_ptr<LinkTask>(new LinkTaskNULL);
    return angle::Result::Continue;
}

GLboolean ProgramNULL::validate(const gl::Caps &caps)
{
    return GL_TRUE;
}

}  // namespace rx
