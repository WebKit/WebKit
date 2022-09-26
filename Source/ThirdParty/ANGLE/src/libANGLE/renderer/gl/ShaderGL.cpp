//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ShaderGL.cpp: Implements the class methods for ShaderGL.

#include "libANGLE/renderer/gl/ShaderGL.h"

#include "common/debug.h"
#include "libANGLE/Compiler.h"
#include "libANGLE/Context.h"
#include "libANGLE/renderer/ContextImpl.h"
#include "libANGLE/renderer/gl/FunctionsGL.h"
#include "libANGLE/renderer/gl/RendererGL.h"
#include "libANGLE/trace.h"
#include "platform/FeaturesGL_autogen.h"

#include <iostream>

namespace rx
{

using CompileAndCheckShaderInWorkerFunctor = std::function<bool(const char *source)>;
class TranslateTaskGL : public angle::Closure
{
  public:
    TranslateTaskGL(ShHandle handle,
                    const ShCompileOptions &options,
                    const std::string &source,
                    CompileAndCheckShaderInWorkerFunctor &&compileAndCheckShaderInWorkerFunctor)
        : mHandle(handle),
          mOptions(options),
          mSource(source),
          mCompileAndCheckShaderInWorkerFunctor(std::move(compileAndCheckShaderInWorkerFunctor)),
          mResult(false),
          mWorkerAvailable(true)
    {}

    void operator()() override
    {
        ANGLE_TRACE_EVENT1("gpu.angle", "TranslateTaskGL::run", "source", mSource);
        const char *source = mSource.c_str();
        mResult            = sh::Compile(mHandle, &source, 1, mOptions);
        if (mResult)
        {
            mWorkerAvailable =
                mCompileAndCheckShaderInWorkerFunctor(sh::GetObjectCode(mHandle).c_str());
        }
    }

    bool getResult() { return mResult; }

    bool workerAvailable() { return mWorkerAvailable; }

    ShHandle getHandle() { return mHandle; }

  private:
    ShHandle mHandle;
    ShCompileOptions mOptions;
    std::string mSource;
    CompileAndCheckShaderInWorkerFunctor mCompileAndCheckShaderInWorkerFunctor;
    bool mResult;
    bool mWorkerAvailable;
};

using PostTranslateFunctor         = std::function<bool(std::string *infoLog)>;
using CompileAndCheckShaderFunctor = std::function<void(const char *source)>;
class WaitableCompileEventWorkerContext final : public WaitableCompileEvent
{
  public:
    WaitableCompileEventWorkerContext(std::shared_ptr<angle::WaitableEvent> waitableEvent,
                                      CompileAndCheckShaderFunctor &&compileAndCheckShaderFunctor,
                                      PostTranslateFunctor &&postTranslateFunctor,
                                      std::shared_ptr<TranslateTaskGL> translateTask)
        : WaitableCompileEvent(waitableEvent),
          mCompileAndCheckShaderFunctor(std::move(compileAndCheckShaderFunctor)),
          mPostTranslateFunctor(std::move(postTranslateFunctor)),
          mTranslateTask(translateTask)
    {}

    bool getResult() override { return mTranslateTask->getResult(); }

    bool postTranslate(std::string *infoLog) override
    {
        if (!mTranslateTask->workerAvailable())
        {
            ShHandle handle = mTranslateTask->getHandle();
            mCompileAndCheckShaderFunctor(sh::GetObjectCode(handle).c_str());
        }
        return mPostTranslateFunctor(infoLog);
    }

  private:
    CompileAndCheckShaderFunctor mCompileAndCheckShaderFunctor;
    PostTranslateFunctor mPostTranslateFunctor;
    std::shared_ptr<TranslateTaskGL> mTranslateTask;
};

using PeekCompletionFunctor = std::function<bool()>;
using CheckShaderFunctor    = std::function<void()>;

class WaitableCompileEventNativeParallel final : public WaitableCompileEvent
{
  public:
    WaitableCompileEventNativeParallel(PostTranslateFunctor &&postTranslateFunctor,
                                       bool result,
                                       CheckShaderFunctor &&checkShaderFunctor,
                                       PeekCompletionFunctor &&peekCompletionFunctor)
        : WaitableCompileEvent(std::shared_ptr<angle::WaitableEventDone>()),
          mPostTranslateFunctor(std::move(postTranslateFunctor)),
          mResult(result),
          mCheckShaderFunctor(std::move(checkShaderFunctor)),
          mPeekCompletionFunctor(std::move(peekCompletionFunctor))
    {}

    void wait() override { mCheckShaderFunctor(); }

    bool isReady() override { return mPeekCompletionFunctor(); }

    bool getResult() override { return mResult; }

    bool postTranslate(std::string *infoLog) override { return mPostTranslateFunctor(infoLog); }

  private:
    PostTranslateFunctor mPostTranslateFunctor;
    bool mResult;
    CheckShaderFunctor mCheckShaderFunctor;
    PeekCompletionFunctor mPeekCompletionFunctor;
};

class WaitableCompileEventDone final : public WaitableCompileEvent
{
  public:
    WaitableCompileEventDone(PostTranslateFunctor &&postTranslateFunctor, bool result)
        : WaitableCompileEvent(std::make_shared<angle::WaitableEventDone>()),
          mPostTranslateFunctor(std::move(postTranslateFunctor)),
          mResult(result)
    {}

    bool getResult() override { return mResult; }

    bool postTranslate(std::string *infoLog) override { return mPostTranslateFunctor(infoLog); }

  private:
    PostTranslateFunctor mPostTranslateFunctor;
    bool mResult;
};

ShaderGL::ShaderGL(const gl::ShaderState &data,
                   GLuint shaderID,
                   MultiviewImplementationTypeGL multiviewImplementationType,
                   const std::shared_ptr<RendererGL> &renderer)
    : ShaderImpl(data),
      mShaderID(shaderID),
      mMultiviewImplementationType(multiviewImplementationType),
      mRenderer(renderer),
      mCompileStatus(GL_FALSE)
{}

ShaderGL::~ShaderGL()
{
    ASSERT(mShaderID == 0);
}

void ShaderGL::destroy()
{
    mRenderer->getFunctions()->deleteShader(mShaderID);
    mShaderID = 0;
}

void ShaderGL::compileAndCheckShader(const char *source)
{
    compileShader(source);
    checkShader();
}

void ShaderGL::compileShader(const char *source)
{
    const FunctionsGL *functions = mRenderer->getFunctions();
    functions->shaderSource(mShaderID, 1, &source, nullptr);
    functions->compileShader(mShaderID);
}

void ShaderGL::checkShader()
{
    const FunctionsGL *functions = mRenderer->getFunctions();

    // Check for compile errors from the native driver
    mCompileStatus = GL_FALSE;
    functions->getShaderiv(mShaderID, GL_COMPILE_STATUS, &mCompileStatus);
    if (mCompileStatus == GL_FALSE)
    {
        // Compilation failed, put the error into the info log
        GLint infoLogLength = 0;
        functions->getShaderiv(mShaderID, GL_INFO_LOG_LENGTH, &infoLogLength);

        // Info log length includes the null terminator, so 1 means that the info log is an empty
        // string.
        if (infoLogLength > 1)
        {
            std::vector<char> buf(infoLogLength);
            functions->getShaderInfoLog(mShaderID, infoLogLength, nullptr, &buf[0]);

            mInfoLog += &buf[0];
            WARN() << std::endl << mInfoLog;
        }
        else
        {
            WARN() << std::endl << "Shader compilation failed with no info log.";
        }
    }
}

bool ShaderGL::peekCompletion()
{
    const FunctionsGL *functions = mRenderer->getFunctions();
    GLint status                 = GL_FALSE;
    functions->getShaderiv(mShaderID, GL_COMPLETION_STATUS, &status);
    return status == GL_TRUE;
}

bool ShaderGL::compileAndCheckShaderInWorker(const char *source)
{
    std::string workerInfoLog;
    ScopedWorkerContextGL worker(mRenderer.get(), &workerInfoLog);
    if (worker())
    {
        compileAndCheckShader(source);
        return true;
    }
    else
    {
#if !defined(NDEBUG)
        mInfoLog += "bindWorkerContext failed.\n" + workerInfoLog;
#endif
        return false;
    }
}

std::shared_ptr<WaitableCompileEvent> ShaderGL::compile(const gl::Context *context,
                                                        gl::ShCompilerInstance *compilerInstance,
                                                        ShCompileOptions *options)
{
    mInfoLog.clear();

    options->initGLPosition = true;

    bool isWebGL = context->isWebGL();
    if (isWebGL && mState.getShaderType() != gl::ShaderType::Compute)
    {
        options->initOutputVariables = true;
    }

    if (isWebGL && !context->getState().getEnableFeature(GL_TEXTURE_RECTANGLE_ANGLE))
    {
        options->disableARBTextureRectangle = true;
    }

    const angle::FeaturesGL &features = GetFeaturesGL(context);

    if (features.initFragmentOutputVariables.enabled)
    {
        options->initFragmentOutputVariables = true;
    }

    if (features.doWhileGLSLCausesGPUHang.enabled)
    {
        options->rewriteDoWhileLoops = true;
    }

    if (features.emulateAbsIntFunction.enabled)
    {
        options->emulateAbsIntFunction = true;
    }

    if (features.addAndTrueToLoopCondition.enabled)
    {
        options->addAndTrueToLoopCondition = true;
    }

    if (features.emulateIsnanFloat.enabled)
    {
        options->emulateIsnanFloatFunction = true;
    }

    if (features.emulateAtan2Float.enabled)
    {
        options->emulateAtan2FloatFunction = true;
    }

    if (features.useUnusedBlocksWithStandardOrSharedLayout.enabled)
    {
        options->useUnusedStandardSharedBlocks = true;
    }

    if (features.removeInvariantAndCentroidForESSL3.enabled)
    {
        options->removeInvariantAndCentroidForESSL3 = true;
    }

    if (features.rewriteFloatUnaryMinusOperator.enabled)
    {
        options->rewriteFloatUnaryMinusOperator = true;
    }

    if (!features.dontInitializeUninitializedLocals.enabled)
    {
        options->initializeUninitializedLocals = true;
    }

    if (features.clampPointSize.enabled)
    {
        options->clampPointSize = true;
    }

    if (features.dontUseLoopsToInitializeVariables.enabled)
    {
        options->dontUseLoopsToInitializeVariables = true;
    }

    if (features.clampFragDepth.enabled)
    {
        options->clampFragDepth = true;
    }

    if (features.rewriteRepeatedAssignToSwizzled.enabled)
    {
        options->rewriteRepeatedAssignToSwizzled = true;
    }

    if (mMultiviewImplementationType == MultiviewImplementationTypeGL::NV_VIEWPORT_ARRAY2)
    {
        options->initializeBuiltinsForInstancedMultiview = true;
        options->selectViewInNvGLSLVertexShader          = true;
    }

    if (features.clampArrayAccess.enabled || isWebGL)
    {
        options->clampIndirectArrayBounds = true;
    }

    if (features.vertexIDDoesNotIncludeBaseVertex.enabled)
    {
        options->addBaseVertexToVertexID = true;
    }

    if (features.unfoldShortCircuits.enabled)
    {
        options->unfoldShortCircuit = true;
    }

    if (features.removeDynamicIndexingOfSwizzledVector.enabled)
    {
        options->removeDynamicIndexingOfSwizzledVector = true;
    }

    if (features.preAddTexelFetchOffsets.enabled)
    {
        options->rewriteTexelFetchOffsetToTexelFetch = true;
    }

    if (features.regenerateStructNames.enabled)
    {
        options->regenerateStructNames = true;
    }

    if (features.rewriteRowMajorMatrices.enabled)
    {
        options->rewriteRowMajorMatrices = true;
    }

    if (features.passHighpToPackUnormSnormBuiltins.enabled)
    {
        options->passHighpToPackUnormSnormBuiltins = true;
    }

    if (mRenderer->getNativeExtensions().shaderPixelLocalStorageANGLE)
    {
        options->pls.type = mRenderer->getNativePixelLocalStorageType();
        if (mRenderer->getNativeExtensions().shaderPixelLocalStorageCoherentANGLE)
        {
            // Prefer vendor-specific extensions first. The PixelLocalStorageTest.Coherency test
            // doesn't always pass on Intel when we use the ARB extension.
            ShShaderOutput translatorOutputType = GetShaderOutputType(GetFunctionsGL(context));
            if (features.supportsFragmentShaderInterlockNV.enabled)
            {
                // This extension requires 430+. GetShaderOutputType() should always select 430+ on
                // a GL 4.3 context, where this extension is defined.
                ASSERT(mRenderer->getFunctions()->isAtLeastGL(gl::Version(4, 3)));
                ASSERT(translatorOutputType >= SH_GLSL_430_CORE_OUTPUT);
                options->pls.fragmentSynchronizationType =
                    ShFragmentSynchronizationType::FragmentShaderInterlock_NV_GL;
            }
            else if (features.supportsFragmentShaderOrderingINTEL.enabled)
            {
                // This extension requires 440+. GetShaderOutputType() should always select 440+ on
                // a GL 4.4 context, where this extension is defined.
                ASSERT(mRenderer->getFunctions()->isAtLeastGL(gl::Version(4, 4)));
                ASSERT(translatorOutputType >= SH_GLSL_440_CORE_OUTPUT);
                options->pls.fragmentSynchronizationType =
                    ShFragmentSynchronizationType::FragmentShaderOrdering_INTEL_GL;
            }
            else
            {
                ASSERT(features.supportsFragmentShaderInterlockARB.enabled);
                // This extension requires 450+. GetShaderOutputType() should always select 450+ on
                // a GL 4.5 context, where this extension is defined.
                ASSERT(mRenderer->getFunctions()->isAtLeastGL(gl::Version(4, 5)));
                ASSERT(translatorOutputType >= SH_GLSL_450_CORE_OUTPUT);
                options->pls.fragmentSynchronizationType =
                    ShFragmentSynchronizationType::FragmentShaderInterlock_ARB_GL;
            }
        }
    }

    auto workerThreadPool = context->getShaderCompileThreadPool();

    const std::string &source = mState.getSource();

    auto postTranslateFunctor = [this](std::string *infoLog) {
        if (mCompileStatus == GL_FALSE)
        {
            *infoLog = mInfoLog;
            return false;
        }
        return true;
    };

    if (mRenderer->hasNativeParallelCompile())
    {
        ShHandle handle = compilerInstance->getHandle();
        const char *str = source.c_str();
        bool result     = sh::Compile(handle, &str, 1, *options);
        if (result)
        {
            compileShader(sh::GetObjectCode(handle).c_str());
            auto checkShaderFunctor    = [this]() { checkShader(); };
            auto peekCompletionFunctor = [this]() { return peekCompletion(); };
            return std::make_shared<WaitableCompileEventNativeParallel>(
                std::move(postTranslateFunctor), result, std::move(checkShaderFunctor),
                std::move(peekCompletionFunctor));
        }
        else
        {
            return std::make_shared<WaitableCompileEventDone>([](std::string *) { return true; },
                                                              result);
        }
    }
    else if (workerThreadPool->isAsync())
    {
        auto compileAndCheckShaderInWorkerFunctor = [this](const char *source) {
            return compileAndCheckShaderInWorker(source);
        };
        auto translateTask =
            std::make_shared<TranslateTaskGL>(compilerInstance->getHandle(), *options, source,
                                              std::move(compileAndCheckShaderInWorkerFunctor));

        auto compileAndCheckShaderFunctor = [this](const char *source) {
            compileAndCheckShader(source);
        };
        return std::make_shared<WaitableCompileEventWorkerContext>(
            angle::WorkerThreadPool::PostWorkerTask(workerThreadPool, translateTask),
            std::move(compileAndCheckShaderFunctor), std::move(postTranslateFunctor),
            translateTask);
    }
    else
    {
        ShHandle handle = compilerInstance->getHandle();
        const char *str = source.c_str();
        bool result     = sh::Compile(handle, &str, 1, *options);
        if (result)
        {
            compileAndCheckShader(sh::GetObjectCode(handle).c_str());
        }
        return std::make_shared<WaitableCompileEventDone>(std::move(postTranslateFunctor), result);
    }
}

std::string ShaderGL::getDebugInfo() const
{
    return mState.getTranslatedSource();
}

GLuint ShaderGL::getShaderID() const
{
    return mShaderID;
}

}  // namespace rx
