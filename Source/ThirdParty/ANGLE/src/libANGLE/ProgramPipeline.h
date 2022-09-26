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

    ProgramExecutable &getExecutable() const
    {
        ASSERT(mExecutable);
        return *mExecutable;
    }

    void activeShaderProgram(Program *shaderProgram);
    void useProgramStages(const Context *context,
                          const gl::ShaderBitSet &shaderTypes,
                          Program *shaderProgram,
                          std::vector<angle::ObserverBinding> *programObserverBindings);

    Program *getActiveShaderProgram() { return mActiveShaderProgram; }

    GLboolean isValid() const { return mValid; }

    const Program *getShaderProgram(ShaderType shaderType) const { return mPrograms[shaderType]; }

    bool usesShaderProgram(ShaderProgramID program) const;

    void updateExecutableTextures();

    rx::SpecConstUsageBits getSpecConstUsageBits() const;

  private:
    void useProgramStage(const Context *context,
                         ShaderType shaderType,
                         Program *shaderProgram,
                         angle::ObserverBinding *programObserverBindings);

    friend class ProgramPipeline;

    std::string mLabel;

    // The active shader program
    Program *mActiveShaderProgram;
    // The shader programs for each stage.
    ShaderMap<Program *> mPrograms;

    GLboolean mValid;

    ProgramExecutable *mExecutable;

    bool mIsLinked;
};

class ProgramPipeline final : public RefCountObject<ProgramPipelineID>,
                              public LabeledObject,
                              public angle::ObserverInterface,
                              public angle::Subject
{
  public:
    ProgramPipeline(rx::GLImplFactory *factory, ProgramPipelineID handle);
    ~ProgramPipeline() override;

    void onDestroy(const Context *context) override;

    angle::Result setLabel(const Context *context, const std::string &label) override;
    const std::string &getLabel() const override;

    const ProgramPipelineState &getState() const { return mState; }
    ProgramPipelineState &getState() { return mState; }

    ProgramExecutable &getExecutable() const { return mState.getExecutable(); }

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

    angle::Result useProgramStages(const Context *context,
                                   GLbitfield stages,
                                   Program *shaderProgram);

    Program *getShaderProgram(ShaderType shaderType) const { return mState.mPrograms[shaderType]; }

    void resetIsLinked() { mState.mIsLinked = false; }
    angle::Result link(const gl::Context *context);

    // Ensure program pipeline is linked. Inlined to make sure its overhead is as low as possible.
    void resolveLink(const Context *context)
    {
        if (mState.mIsLinked)
        {
            // Already linked, nothing to do.
            return;
        }

        angle::Result linkResult = link(context);
        if (linkResult != angle::Result::Continue)
        {
            // If the link failed then log a warning, swallow the error and move on.
            WARN() << "ProgramPipeline link failed" << std::endl;
        }
        return;
    }

    void validate(const gl::Context *context);
    GLboolean isValid() const { return mState.isValid(); }
    bool isLinked() const { return mState.mIsLinked; }

    // ObserverInterface implementation.
    void onSubjectStateChange(angle::SubjectIndex index, angle::SubjectMessage message) override;

  private:
    bool linkVaryings(InfoLog &infoLog) const;
    void updateLinkedShaderStages();
    void updateExecutableAttributes();
    void updateTransformFeedbackMembers();
    void updateShaderStorageBlocks();
    void updateImageBindings();
    void updateExecutableGeometryProperties();
    void updateExecutableTessellationProperties();
    void updateFragmentInoutRangeAndEnablesPerSampleShading();
    void updateLinkedVaryings();
    void updateExecutable();

    std::unique_ptr<rx::ProgramPipelineImpl> mProgramPipelineImpl;

    ProgramPipelineState mState;

    std::vector<angle::ObserverBinding> mProgramObserverBindings;
    angle::ObserverBinding mExecutableObserverBinding;
};
}  // namespace gl

#endif  // LIBANGLE_PROGRAMPIPELINE_H_
