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

    ProgramVk *getShaderProgram(gl::ShaderType shaderType) const
    {
        const gl::Program *program = mState.getShaderProgram(shaderType);
        return SafeGetImplAs<ProgramVk>(program);
    }

    void fillProgramStateMap(gl::ShaderMap<const gl::ProgramState *> *programStatesOut);

    angle::Result link(const gl::Context *glContext,
                       const gl::ProgramMergedVaryings &mergedVaryings,
                       const gl::ProgramVaryingPacking &varyingPacking) override;

    angle::Result updateUniforms(ContextVk *contextVk);

    void setAllDefaultUniformsDirty();
    bool hasDirtyUniforms() const;
    void onProgramBind();

  private:
    size_t calcUniformUpdateRequiredSpace(ContextVk *contextVk,
                                          gl::ShaderMap<VkDeviceSize> *uniformOffsets) const;

    ProgramExecutableVk mExecutable;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_PROGRAMPIPELINEVK_H_
