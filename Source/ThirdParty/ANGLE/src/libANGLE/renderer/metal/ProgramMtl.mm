//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ProgramMtl.mm:
//    Implements the class methods for ProgramMtl.
//

#include "libANGLE/renderer/metal/ProgramMtl.h"

#include <TargetConditionals.h>

#include <sstream>

#include "common/WorkerThread.h"
#include "common/debug.h"
#include "common/system_utils.h"

#include "libANGLE/Context.h"
#include "libANGLE/ProgramLinkedResources.h"
#include "libANGLE/renderer/metal/CompilerMtl.h"
#include "libANGLE/renderer/metal/ContextMtl.h"
#include "libANGLE/renderer/metal/DisplayMtl.h"
#include "libANGLE/renderer/metal/blocklayoutMetal.h"
#include "libANGLE/renderer/metal/mtl_msl_utils.h"
#include "libANGLE/renderer/metal/mtl_utils.h"
#include "libANGLE/renderer/metal/renderermtl_utils.h"
#include "libANGLE/renderer/renderer_utils.h"
#include "libANGLE/trace.h"

namespace rx
{

namespace
{
inline std::map<std::string, std::string> GetDefaultSubstitutionDictionary()
{
    return {};
}

bool DisableFastMathForShaderCompilation(mtl::Context *context)
{
    return context->getDisplay()->getFeatures().intelDisableFastMath.enabled;
}

bool UsesInvariance(const mtl::TranslatedShaderInfo *translatedMslInfo)
{
    return translatedMslInfo->hasInvariant;
}

class Std140BlockLayoutEncoderFactory : public gl::CustomBlockLayoutEncoderFactory
{
  public:
    sh::BlockLayoutEncoder *makeEncoder() override { return new sh::Std140BlockEncoder(); }
};

class CompileMslTask final : public LinkSubTask
{
  public:
    CompileMslTask(ContextMtl *context,
                   mtl::TranslatedShaderInfo *translatedMslInfo,
                   const std::map<std::string, std::string> &substitutionMacros)
        : mContext(context),
          mTranslatedMslInfo(translatedMslInfo),
          mSubstitutionMacros(substitutionMacros)
    {}
    ~CompileMslTask() override = default;

    void operator()() override
    {
        mResult = CreateMslShaderLib(mContext, mInfoLog, mTranslatedMslInfo, mSubstitutionMacros);
    }

    angle::Result getResult(const gl::Context *context, gl::InfoLog &infoLog) override
    {
        if (!mInfoLog.empty())
        {
            infoLog << mInfoLog.str();
        }

        return mResult;
    }

  private:
    // TODO: remove this, inherit from mtl::Context and ensure thread-safety.
    // http://anglebug.com/8297
    ContextMtl *mContext;
    gl::InfoLog mInfoLog;
    mtl::TranslatedShaderInfo *mTranslatedMslInfo;
    std::map<std::string, std::string> mSubstitutionMacros;
    angle::Result mResult = angle::Result::Continue;
};
}  // namespace

class ProgramMtl::LinkTaskMtl final : public LinkTask
{
  public:
    LinkTaskMtl(const gl::Context *context, ProgramMtl *program)
        : mContext(context), mProgram(program)
    {}
    ~LinkTaskMtl() override = default;

    std::vector<std::shared_ptr<LinkSubTask>> link(
        const gl::ProgramLinkedResources &resources,
        const gl::ProgramMergedVaryings &mergedVaryings) override
    {
        std::vector<std::shared_ptr<LinkSubTask>> subTasks;
        mResult = mProgram->linkJobImpl(mContext, resources, &subTasks);
        return subTasks;
    }

    angle::Result getResult(const gl::Context *context, gl::InfoLog &infoLog) override
    {
        return mResult;
    }

  private:
    // TODO: remove this, inherit from mtl::Context and ensure thread-safety.
    // http://anglebug.com/8297
    const gl::Context *mContext;
    ProgramMtl *mProgram;
    angle::Result mResult = angle::Result::Continue;
};

class ProgramMtl::LoadTaskMtl final : public LinkTask
{
  public:
    LoadTaskMtl(std::vector<std::shared_ptr<LinkSubTask>> &&subTasks)
        : mSubTasks(std::move(subTasks))
    {}
    ~LoadTaskMtl() override = default;

    std::vector<std::shared_ptr<LinkSubTask>> load() override { return mSubTasks; }

    angle::Result getResult(const gl::Context *context, gl::InfoLog &infoLog) override
    {
        return angle::Result::Continue;
    }

  private:
    std::vector<std::shared_ptr<LinkSubTask>> mSubTasks;
};

// ProgramArgumentBufferEncoderMtl implementation
void ProgramArgumentBufferEncoderMtl::reset(ContextMtl *contextMtl)
{
    metalArgBufferEncoder = nil;
    bufferPool.destroy(contextMtl);
}

// ProgramShaderObjVariantMtl implementation
void ProgramShaderObjVariantMtl::reset(ContextMtl *contextMtl)
{
    metalShader = nil;

    uboArgBufferEncoder.reset(contextMtl);

    translatedSrcInfo = nullptr;
}

// ProgramMtl implementation
ProgramMtl::ProgramMtl(const gl::ProgramState &state) : ProgramImpl(state) {}

ProgramMtl::~ProgramMtl() = default;

void ProgramMtl::destroy(const gl::Context *context)
{
    getExecutable()->reset(mtl::GetImpl(context));
}

angle::Result ProgramMtl::load(const gl::Context *context,
                               gl::BinaryInputStream *stream,
                               std::shared_ptr<LinkTask> *loadTaskOut)
{

    ContextMtl *contextMtl = mtl::GetImpl(context);
    // NOTE(hqle): No transform feedbacks for now, since we only support ES 2.0 atm

    ANGLE_TRY(getExecutable()->load(contextMtl, stream));

    // TODO: parallelize the above too.  http://anglebug.com/8297
    std::vector<std::shared_ptr<LinkSubTask>> subTasks;

    ANGLE_TRY(compileMslShaderLibs(context, &subTasks));

    *loadTaskOut = std::shared_ptr<LinkTask>(new LoadTaskMtl(std::move(subTasks)));
    return angle::Result::Continue;
}

void ProgramMtl::save(const gl::Context *context, gl::BinaryOutputStream *stream)
{
    getExecutable()->save(stream);
}

void ProgramMtl::setBinaryRetrievableHint(bool retrievable) {}

void ProgramMtl::setSeparable(bool separable)
{
    UNIMPLEMENTED();
}

void ProgramMtl::prepareForLink(const gl::ShaderMap<ShaderImpl *> &shaders)
{
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        mAttachedShaders[shaderType].reset();

        if (shaders[shaderType] != nullptr)
        {
            const ShaderMtl *shaderMtl   = GetAs<ShaderMtl>(shaders[shaderType]);
            mAttachedShaders[shaderType] = shaderMtl->getCompiledState();
        }
    }
}

angle::Result ProgramMtl::link(const gl::Context *context, std::shared_ptr<LinkTask> *linkTaskOut)
{
    *linkTaskOut = std::shared_ptr<LinkTask>(new LinkTaskMtl(context, this));
    return angle::Result::Continue;
}

angle::Result ProgramMtl::linkJobImpl(const gl::Context *context,
                                      const gl::ProgramLinkedResources &resources,
                                      std::vector<std::shared_ptr<LinkSubTask>> *subTasksOut)
{
    ContextMtl *contextMtl              = mtl::GetImpl(context);
    ProgramExecutableMtl *executableMtl = getExecutable();

    // Link resources before calling GetShaderSource to make sure they are ready for the set/binding
    // assignment done in that function.
    linkResources(resources);

    ANGLE_TRY(executableMtl->initDefaultUniformBlocks(contextMtl, mState.getAttachedShaders()));
    executableMtl->linkUpdateHasFlatAttributes(mState.getAttachedShader(gl::ShaderType::Vertex));

    gl::ShaderMap<std::string> shaderSources;
    mtl::MSLGetShaderSource(mState, resources, &shaderSources);

    ANGLE_TRY(mtl::MTLGetMSL(contextMtl, mState.getExecutable(), contextMtl->getCaps(),
                             shaderSources, mAttachedShaders,
                             &executableMtl->mMslShaderTranslateInfo));
    executableMtl->mMslXfbOnlyVertexShaderInfo =
        executableMtl->mMslShaderTranslateInfo[gl::ShaderType::Vertex];

    return compileMslShaderLibs(context, subTasksOut);
}

angle::Result ProgramMtl::compileMslShaderLibs(
    const gl::Context *context,
    std::vector<std::shared_ptr<LinkSubTask>> *subTasksOut)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "ProgramMtl::compileMslShaderLibs");
    gl::InfoLog &infoLog = mState.getExecutable().getInfoLog();

    ContextMtl *contextMtl              = mtl::GetImpl(context);
    ProgramExecutableMtl *executableMtl = getExecutable();
    bool asyncCompile =
        contextMtl->getDisplay()->getFeatures().enableParallelMtlLibraryCompilation.enabled;
    mtl::LibraryCache &libraryCache = contextMtl->getDisplay()->getLibraryCache();

    for (gl::ShaderType shaderType : gl::kAllGLES2ShaderTypes)
    {
        mtl::TranslatedShaderInfo *translateInfo =
            &executableMtl->mMslShaderTranslateInfo[shaderType];
        std::map<std::string, std::string> macros = GetDefaultSubstitutionDictionary();
        bool disableFastMath                      = DisableFastMathForShaderCompilation(contextMtl);
        bool usesInvariance                       = UsesInvariance(translateInfo);

        // Check if the shader is already in the cache and use it instead of spawning a new thread
        translateInfo->metalLibrary = libraryCache.get(translateInfo->metalShaderSource, macros,
                                                       disableFastMath, usesInvariance);

        if (!translateInfo->metalLibrary)
        {
            if (asyncCompile)
            {
                subTasksOut->emplace_back(new CompileMslTask(contextMtl, translateInfo, macros));
            }
            else
            {
                ANGLE_TRY(CreateMslShaderLib(contextMtl, infoLog, translateInfo, macros));
            }
        }
    }

    return angle::Result::Continue;
}

void ProgramMtl::linkResources(const gl::ProgramLinkedResources &resources)
{
    Std140BlockLayoutEncoderFactory std140EncoderFactory;
    gl::ProgramLinkedResourcesLinker linker(&std140EncoderFactory);

    linker.linkResources(mState, resources);
}

GLboolean ProgramMtl::validate(const gl::Caps &caps)
{
    // No-op. The spec is very vague about the behavior of validation.
    return GL_TRUE;
}

}  // namespace rx
