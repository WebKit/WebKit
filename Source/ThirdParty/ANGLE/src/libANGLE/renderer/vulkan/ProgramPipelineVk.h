//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ProgramPipelineVk.h:
//    Defines the class interface for ProgramPipelineVk, implementing ProgramPipelineImpl.
//

#ifndef LIBANGLE_RENDERER_VULKAN_PROGRAMPIPELINEVK_H_
#define LIBANGLE_RENDERER_VULKAN_PROGRAMPIPELINEVK_H_

#include "libANGLE/renderer/ProgramPipelineImpl.h"

#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/ProgramExecutableVk.h"
#include "libANGLE/renderer/vulkan/ProgramVk.h"

namespace rx
{

class ProgramPipelineVk : public ProgramPipelineImpl
{
  public:
    ProgramPipelineVk(const gl::ProgramPipelineState &state);
    ~ProgramPipelineVk() override;

    void destroy(const gl::Context *context) override;
    void reset(ContextVk *contextVk);

    const ProgramExecutableVk &getExecutable() const { return mExecutable; }
    ProgramExecutableVk &getExecutable() { return mExecutable; }

    ProgramVk *getShaderProgram(const gl::State &glState, gl::ShaderType shaderType) const
    {
        gl::ProgramPipeline *pipeline = glState.getProgramPipeline();
        const gl::Program *program    = pipeline->getShaderProgram(shaderType);
        if (program)
        {
            return vk::GetImpl(program);
        }
        return nullptr;
    }

    void fillProgramStateMap(const ContextVk *contextVk,
                             gl::ShaderMap<const gl::ProgramState *> *programStatesOut);

    angle::Result link(const gl::Context *context) override;

    angle::Result updateUniforms(ContextVk *contextVk);

    bool dirtyUniforms(const gl::State &glState);
    void onProgramBind(ContextVk *contextVk);

  private:
    size_t calcUniformUpdateRequiredSpace(ContextVk *contextVk,
                                          const gl::ProgramExecutable &glExecutable,
                                          const gl::State &glState,
                                          gl::ShaderMap<VkDeviceSize> *uniformOffsets) const;
    void setAllDefaultUniformsDirty(const gl::State &glState);

    ProgramExecutableVk mExecutable;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_PROGRAMPIPELINEVK_H_
