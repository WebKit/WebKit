//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ProgramPipeline.h: Defines the gl::ProgramPipeline class.
// Implements GL program pipeline objects and related functionality.
// [OpenGL ES 3.1] section 7.4 page 105.

#ifndef LIBANGLE_PROGRAMPIPELINE_H_
#define LIBANGLE_PROGRAMPIPELINE_H_

#include <memory>

#include "common/angleutils.h"
#include "libANGLE/Debug.h"
#include "libANGLE/Program.h"
#include "libANGLE/ProgramExecutable.h"
#include "libANGLE/RefCountObject.h"

namespace rx
{
class GLImplFactory;
class ProgramPipelineImpl;
}  // namespace rx

namespace gl
{
class Context;
class ProgramPipeline;

class ProgramPipelineState final : angle::NonCopyable
{
  public:
    ProgramPipelineState();
    ~ProgramPipelineState();

    const std::string &getLabel() const;

    // A PPO can have both graphics and compute programs attached, so
    // we don't know if the PPO is a 'graphics' or 'compute' PPO until the
    // actual draw/dispatch call.
    bool isCompute() const { return mIsCompute; }
    void setIsCompute(bool isCompute) { mIsCompute = isCompute; }

    const ProgramExecutable &getProgramExecutable() const
    {
        ASSERT(mExecutable);
        return *mExecutable;
    }
    ProgramExecutable &getProgramExecutable()
    {
        ASSERT(mExecutable);
        return *mExecutable;
    }

    void activeShaderProgram(Program *shaderProgram);
    void useProgramStages(const Context *context, GLbitfield stages, Program *shaderProgram);

    Program *getActiveShaderProgram() { return mActiveShaderProgram; }

    GLboolean isValid() const { return mValid; }

    const Program *getShaderProgram(ShaderType shaderType) const { return mPrograms[shaderType]; }

    bool usesShaderProgram(ShaderProgramID program) const;

    bool hasDefaultUniforms() const;
    bool hasTextures() const;
    bool hasUniformBuffers() const;
    bool hasStorageBuffers() const;
    bool hasAtomicCounterBuffers() const;
    bool hasImages() const;
    bool hasTransformFeedbackOutput() const;

  private:
    void useProgramStage(const Context *context, ShaderType shaderType, Program *shaderProgram);

    friend class ProgramPipeline;

    std::string mLabel;

    bool mIsCompute;

    // The active shader program
    Program *mActiveShaderProgram;
    // The shader programs for each stage.
    ShaderMap<Program *> mPrograms;

    GLboolean mValid;

    GLboolean mHasBeenBound;

    ProgramExecutable *mExecutable;
};

class ProgramPipeline final : public RefCountObject<ProgramPipelineID>, public LabeledObject
{
  public:
    ProgramPipeline(rx::GLImplFactory *factory, ProgramPipelineID handle);
    ~ProgramPipeline() override;

    void onDestroy(const Context *context) override;

    void setLabel(const Context *context, const std::string &label) override;
    const std::string &getLabel() const override;

    const ProgramPipelineState &getState() const { return mState; }

    const ProgramExecutable &getExecutable() const { return mState.getProgramExecutable(); }
    ProgramExecutable &getExecutable() { return mState.getProgramExecutable(); }

    rx::ProgramPipelineImpl *getImplementation() const;

    Program *getActiveShaderProgram() { return mState.getActiveShaderProgram(); }
    void activeShaderProgram(Program *shaderProgram);
    Program *getLinkedActiveShaderProgram(const Context *context)
    {
        Program *program = mState.getActiveShaderProgram();
        if (program)
        {
            program->resolveLink(context);
        }
        return program;
    }

    void useProgramStages(const Context *context, GLbitfield stages, Program *shaderProgram);

    void updateExecutableAttributes();
    void updateExecutableTextures();
    void updateExecutable();

    Program *getShaderProgram(ShaderType shaderType) const { return mState.mPrograms[shaderType]; }

    ProgramMergedVaryings getMergedVaryings() const;
    angle::Result link(const gl::Context *context);
    bool linkVaryings(InfoLog &infoLog) const;
    void validate(const gl::Context *context);
    bool validateSamplers(InfoLog *infoLog, const Caps &caps);

    bool usesShaderProgram(ShaderProgramID program) const
    {
        return mState.usesShaderProgram(program);
    }

    bool hasAnyDirtyBit() const { return mDirtyBits.any(); }

    GLboolean isValid() const { return mState.isValid(); }

    void bind() { mState.mHasBeenBound = true; }
    GLboolean hasBeenBound() const { return mState.mHasBeenBound; }

    // Program pipeline dirty bits.
    enum DirtyBitType
    {
        // One of the program stages in the PPO changed.
        DIRTY_BIT_PROGRAM_STAGE,
        DIRTY_BIT_DRAW_DISPATCH_CHANGE,

        DIRTY_BIT_COUNT = DIRTY_BIT_DRAW_DISPATCH_CHANGE + 1,
    };

    using DirtyBits = angle::BitSet<DIRTY_BIT_COUNT>;

    angle::Result syncState(const Context *context);
    void setDirtyBit(DirtyBitType dirtyBitType) { mDirtyBits.set(dirtyBitType); }

  private:
    void updateLinkedShaderStages();

    std::unique_ptr<rx::ProgramPipelineImpl> mProgramPipelineImpl;

    ProgramPipelineState mState;

    DirtyBits mDirtyBits;
};
}  // namespace gl

#endif  // LIBANGLE_PROGRAMPIPELINE_H_
