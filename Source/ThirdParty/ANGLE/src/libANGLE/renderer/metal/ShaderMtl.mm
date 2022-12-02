//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ShaderMtl.mm:
//    Implements the class methods for ShaderMtl.
//

#include "libANGLE/renderer/metal/ShaderMtl.h"

#include "common/WorkerThread.h"
#include "common/debug.h"
#include "libANGLE/Context.h"
#include "libANGLE/Shader.h"
#include "libANGLE/renderer/metal/ContextMtl.h"
#include "libANGLE/renderer/metal/DisplayMtl.h"
#include "libANGLE/trace.h"

namespace rx
{

ShaderMtl::ShaderMtl(const gl::ShaderState &state) : ShaderImpl(state) {}

ShaderMtl::~ShaderMtl() {}

class TranslateTask : public angle::Closure
{
  public:
    TranslateTask(ShHandle handle, const ShCompileOptions &options, const std::string &source)
        : mHandle(handle), mOptions(options), mSource(source), mResult(false)
    {}

    void operator()() override
    {
        ANGLE_TRACE_EVENT1("gpu.angle", "TranslateTaskMetal::run", "source", mSource);
        const char *source = mSource.c_str();
        mResult            = sh::Compile(mHandle, &source, 1, mOptions);
    }

    bool getResult() { return mResult; }

    ShHandle getHandle() { return mHandle; }

  private:
    ShHandle mHandle;
    ShCompileOptions mOptions;
    std::string mSource;
    bool mResult;
};

class MTLWaitableCompileEventImpl final : public WaitableCompileEvent
{
  public:
    MTLWaitableCompileEventImpl(ShaderMtl *shader,
                                std::shared_ptr<angle::WaitableEvent> waitableEvent,
                                std::shared_ptr<TranslateTask> translateTask)
        : WaitableCompileEvent(waitableEvent), mTranslateTask(translateTask), mShader(shader)
    {}

    bool getResult() override { return mTranslateTask->getResult(); }

    bool postTranslate(std::string *infoLog) override
    {
        sh::TShHandleBase *base    = static_cast<sh::TShHandleBase *>(mTranslateTask->getHandle());
        auto translatorMetalDirect = base->getAsTranslatorMetalDirect();
        if (translatorMetalDirect != nullptr)
        {
            // Copy reflection from translation.
            mShader->translatorMetalReflection =
                *(translatorMetalDirect->getTranslatorMetalReflection());
            translatorMetalDirect->getTranslatorMetalReflection()->reset();
        }
        return true;
    }

  private:
    std::shared_ptr<TranslateTask> mTranslateTask;
    ShaderMtl *mShader;
};

std::shared_ptr<WaitableCompileEvent> ShaderMtl::compileImplMtl(
    const gl::Context *context,
    gl::ShCompilerInstance *compilerInstance,
    const std::string &source,
    ShCompileOptions *compileOptions)
{
// TODO(jcunningham): Remove this workaround once correct fix to move validation to the very end is
// in place. See: https://bugs.webkit.org/show_bug.cgi?id=224991
#if defined(ANGLE_ENABLE_ASSERTS) && 0
    compileOptions->validateAst = true;
#endif

    auto workerThreadPool = context->getWorkerThreadPool();
    auto translateTask =
        std::make_shared<TranslateTask>(compilerInstance->getHandle(), *compileOptions, source);

    return std::make_shared<MTLWaitableCompileEventImpl>(
        this, workerThreadPool->postWorkerTask(translateTask), translateTask);
}

std::shared_ptr<WaitableCompileEvent> ShaderMtl::compile(const gl::Context *context,
                                                         gl::ShCompilerInstance *compilerInstance,
                                                         ShCompileOptions *options)
{
    ContextMtl *contextMtl = mtl::GetImpl(context);
    DisplayMtl *displayMtl = contextMtl->getDisplay();

    options->initializeUninitializedLocals = true;

    if (context->isWebGL() && mState.getShaderType() != gl::ShaderType::Compute)
    {
        options->initOutputVariables = true;
    }

    if (displayMtl->getFeatures().intelExplicitBoolCastWorkaround.enabled)
    {
        options->addExplicitBoolCasts = true;
    }

    options->clampPointSize = true;
#if defined(ANGLE_PLATFORM_IOS) && !defined(ANGLE_PLATFORM_MACCATALYST)
    options->clampFragDepth = true;
#endif

    if (displayMtl->getFeatures().rewriteRowMajorMatrices.enabled)
    {
        options->rewriteRowMajorMatrices = true;
    }

    // Constants:
    options->metal.driverUniformsBindingIndex    = mtl::kDriverUniformsBindingIndex;
    options->metal.defaultUniformsBindingIndex   = mtl::kDefaultUniformsBindingIndex;
    options->metal.UBOArgumentBufferBindingIndex = mtl::kUBOArgumentBufferBindingIndex;

    // GL_ANGLE_shader_pixel_local_storage.
    if (displayMtl->getNativeExtensions().shaderPixelLocalStorageANGLE)
    {
        options->pls.type                        = displayMtl->getNativePixelLocalStorageType();
        options->pls.fragmentSynchronizationType = displayMtl->getPLSSynchronizationType();
    }

    return compileImplMtl(context, compilerInstance, getState().getSource(), options);
}

std::string ShaderMtl::getDebugInfo() const
{
    std::string debugInfo = mState.getTranslatedSource();
    if (debugInfo.empty())
    {
        return mState.getCompiledBinary().empty() ? "" : "<binary blob>";
    }
    return debugInfo;
}

}  // namespace rx
