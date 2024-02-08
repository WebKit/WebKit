//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ProgramWgpu.cpp:
//    Implements the class methods for ProgramWgpu.
//

#include "libANGLE/renderer/wgpu/ProgramWgpu.h"

#include "common/debug.h"

namespace rx
{
namespace
{
class LinkTaskWgpu : public LinkTask
{
  public:
    ~LinkTaskWgpu() override = default;
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

ProgramWgpu::ProgramWgpu(const gl::ProgramState &state) : ProgramImpl(state) {}

ProgramWgpu::~ProgramWgpu() {}

angle::Result ProgramWgpu::load(const gl::Context *context,
                                gl::BinaryInputStream *stream,
                                std::shared_ptr<LinkTask> *loadTaskOut,
                                egl::CacheGetResult *resultOut)
{
    *loadTaskOut = {};
    *resultOut   = egl::CacheGetResult::GetSuccess;
    return angle::Result::Continue;
}

void ProgramWgpu::save(const gl::Context *context, gl::BinaryOutputStream *stream) {}

void ProgramWgpu::setBinaryRetrievableHint(bool retrievable) {}

void ProgramWgpu::setSeparable(bool separable) {}

angle::Result ProgramWgpu::link(const gl::Context *contextImpl,
                                std::shared_ptr<LinkTask> *linkTaskOut)
{
    *linkTaskOut = std::shared_ptr<LinkTask>(new LinkTaskWgpu);
    return angle::Result::Continue;
}

GLboolean ProgramWgpu::validate(const gl::Caps &caps)
{
    return GL_TRUE;
}

}  // namespace rx
