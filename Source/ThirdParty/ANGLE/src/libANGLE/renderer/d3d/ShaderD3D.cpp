//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ShaderD3D.cpp: Defines the rx::ShaderD3D class which implements rx::ShaderImpl.

#include "libANGLE/renderer/d3d/ShaderD3D.h"

#include "common/system_utils.h"
#include "common/utilities.h"
#include "libANGLE/Caps.h"
#include "libANGLE/Compiler.h"
#include "libANGLE/Context.h"
#include "libANGLE/Shader.h"
#include "libANGLE/features.h"
#include "libANGLE/renderer/ContextImpl.h"
#include "libANGLE/renderer/d3d/ProgramD3D.h"
#include "libANGLE/renderer/d3d/RendererD3D.h"
#include "libANGLE/trace.h"

namespace rx
{

class TranslateTaskD3D : public angle::Closure
{
  public:
    TranslateTaskD3D(ShHandle handle,
                     const ShCompileOptions &options,
                     const std::string &source,
                     const std::string &sourcePath)
        : mHandle(handle),
          mOptions(options),
          mSource(source),
          mSourcePath(sourcePath),
          mResult(false)
    {}

    void operator()() override
    {
        ANGLE_TRACE_EVENT1("gpu.angle", "TranslateTask::run", "source", mSource);
        std::vector<const char *> srcStrings;
        if (!mSourcePath.empty())
        {
            srcStrings.push_back(mSourcePath.c_str());
        }
        srcStrings.push_back(mSource.c_str());

        mResult = sh::Compile(mHandle, &srcStrings[0], srcStrings.size(), mOptions);
    }

    bool getResult() { return mResult; }

  private:
    ShHandle mHandle;
    ShCompileOptions mOptions;
    std::string mSource;
    std::string mSourcePath;
    bool mResult;
};

using PostTranslateFunctor =
    std::function<bool(gl::ShCompilerInstance *compiler, std::string *infoLog)>;

class WaitableCompileEventD3D final : public WaitableCompileEvent
{
  public:
    WaitableCompileEventD3D(std::shared_ptr<angle::WaitableEvent> waitableEvent,
                            gl::ShCompilerInstance *compilerInstance,
                            PostTranslateFunctor &&postTranslateFunctor,
                            std::shared_ptr<TranslateTaskD3D> translateTask)
        : WaitableCompileEvent(waitableEvent),
          mCompilerInstance(compilerInstance),
          mPostTranslateFunctor(std::move(postTranslateFunctor)),
          mTranslateTask(translateTask)
    {}

    bool getResult() override { return mTranslateTask->getResult(); }

    bool postTranslate(std::string *infoLog) override
    {
        return mPostTranslateFunctor(mCompilerInstance, infoLog);
    }

  private:
    gl::ShCompilerInstance *mCompilerInstance;
    PostTranslateFunctor mPostTranslateFunctor;
    std::shared_ptr<TranslateTaskD3D> mTranslateTask;
};

CompiledShaderStateD3D::CompiledShaderStateD3D()
    : compilerOutputType(SH_ESSL_OUTPUT),
      usesMultipleRenderTargets(false),
      usesFragColor(false),
      usesFragData(false),
      usesSecondaryColor(false),
      usesFragCoord(false),
      usesFrontFacing(false),
      usesHelperInvocation(false),
      usesPointSize(false),
      usesPointCoord(false),
      usesDepthRange(false),
      usesSampleID(false),
      usesSamplePosition(false),
      usesSampleMaskIn(false),
      usesSampleMask(false),
      hasMultiviewEnabled(false),
      usesVertexID(false),
      usesViewID(false),
      usesDiscardRewriting(false),
      usesNestedBreak(false),
      requiresIEEEStrictCompiling(false),
      fragDepthUsage(FragDepthUsage::Unused),
      clipDistanceSize(0),
      cullDistanceSize(0),
      readonlyImage2DRegisterIndex(0),
      image2DRegisterIndex(0)
{}

CompiledShaderStateD3D::~CompiledShaderStateD3D() = default;

ShaderD3D::ShaderD3D(const gl::ShaderState &state, RendererD3D *renderer)
    : ShaderImpl(state), mRenderer(renderer)
{}

ShaderD3D::~ShaderD3D() {}

std::string ShaderD3D::getDebugInfo() const
{
    if (!mCompiledState || mCompiledState->debugInfo.empty())
    {
        return "";
    }

    return mCompiledState->debugInfo + std::string("\n// ") +
           gl::GetShaderTypeString(mState.getShaderType()) + " SHADER END\n";
}

void CompiledShaderStateD3D::generateWorkarounds(CompilerWorkaroundsD3D *workarounds) const
{
    if (usesDiscardRewriting)
    {
        // ANGLE issue 486:
        // Work-around a D3D9 compiler bug that presents itself when using conditional discard, by
        // disabling optimization
        workarounds->skipOptimization = true;
    }
    else if (usesNestedBreak)
    {
        // ANGLE issue 603:
        // Work-around a D3D9 compiler bug that presents itself when using break in a nested loop,
        // by maximizing optimization We want to keep the use of
        // ANGLE_D3D_WORKAROUND_MAX_OPTIMIZATION minimal to prevent hangs, so usesDiscard takes
        // precedence
        workarounds->useMaxOptimization = true;
    }

    if (requiresIEEEStrictCompiling)
    {
        // IEEE Strictness for D3D compiler needs to be enabled for NaNs to work.
        workarounds->enableIEEEStrictness = true;
    }
}

unsigned int CompiledShaderStateD3D::getUniformRegister(const std::string &uniformName) const
{
    ASSERT(uniformRegisterMap.count(uniformName) > 0);
    return uniformRegisterMap.find(uniformName)->second;
}

unsigned int CompiledShaderStateD3D::getUniformBlockRegister(const std::string &blockName) const
{
    ASSERT(uniformBlockRegisterMap.count(blockName) > 0);
    return uniformBlockRegisterMap.find(blockName)->second;
}

bool CompiledShaderStateD3D::shouldUniformBlockUseStructuredBuffer(
    const std::string &blockName) const
{
    ASSERT(uniformBlockUseStructuredBufferMap.count(blockName) > 0);
    return uniformBlockUseStructuredBufferMap.find(blockName)->second;
}

unsigned int CompiledShaderStateD3D::getShaderStorageBlockRegister(
    const std::string &blockName) const
{
    ASSERT(shaderStorageBlockRegisterMap.count(blockName) > 0);
    return shaderStorageBlockRegisterMap.find(blockName)->second;
}

bool CompiledShaderStateD3D::useImage2DFunction(const std::string &functionName) const
{
    if (usedImage2DFunctionNames.empty())
    {
        return false;
    }

    return usedImage2DFunctionNames.find(functionName) != usedImage2DFunctionNames.end();
}

const std::set<std::string> &CompiledShaderStateD3D::getSlowCompilingUniformBlockSet() const
{
    return slowCompilingUniformBlockSet;
}

const std::map<std::string, unsigned int> &GetUniformRegisterMap(
    const std::map<std::string, unsigned int> *uniformRegisterMap)
{
    ASSERT(uniformRegisterMap);
    return *uniformRegisterMap;
}

const std::set<std::string> &GetSlowCompilingUniformBlockSet(
    const std::set<std::string> *slowCompilingUniformBlockSet)
{
    ASSERT(slowCompilingUniformBlockSet);
    return *slowCompilingUniformBlockSet;
}

const std::set<std::string> &GetUsedImage2DFunctionNames(
    const std::set<std::string> *usedImage2DFunctionNames)
{
    ASSERT(usedImage2DFunctionNames);
    return *usedImage2DFunctionNames;
}

std::shared_ptr<WaitableCompileEvent> ShaderD3D::compile(const gl::Context *context,
                                                         gl::ShCompilerInstance *compilerInstance,
                                                         ShCompileOptions *options)
{
    // Create a new compiled shader state.  Currently running program link jobs will use the
    // previous state.
    mCompiledState = std::make_shared<CompiledShaderStateD3D>();

    std::string sourcePath;

    const angle::FeaturesD3D &features = mRenderer->getFeatures();
    const gl::Extensions &extensions   = mRenderer->getNativeExtensions();

    const std::string &source = mState.getSource();

#if !defined(ANGLE_ENABLE_WINDOWS_UWP)
    if (gl::DebugAnnotationsActive(context))
    {
        sourcePath = angle::CreateTemporaryFile().value();
        writeFile(sourcePath.c_str(), source.c_str(), source.length());
        options->lineDirectives = true;
        options->sourcePath     = true;
    }
#endif

    if (features.expandIntegerPowExpressions.enabled)
    {
        options->expandSelectHLSLIntegerPowExpressions = true;
    }

    if (features.getDimensionsIgnoresBaseLevel.enabled)
    {
        options->HLSLGetDimensionsIgnoresBaseLevel = true;
    }

    if (features.preAddTexelFetchOffsets.enabled)
    {
        options->rewriteTexelFetchOffsetToTexelFetch = true;
    }
    if (features.rewriteUnaryMinusOperator.enabled)
    {
        options->rewriteIntegerUnaryMinusOperator = true;
    }
    if (features.emulateIsnanFloat.enabled)
    {
        options->emulateIsnanFloatFunction = true;
    }
    if (features.skipVSConstantRegisterZero.enabled &&
        mState.getShaderType() == gl::ShaderType::Vertex)
    {
        options->skipD3DConstantRegisterZero = true;
    }
    if (features.forceAtomicValueResolution.enabled)
    {
        options->forceAtomicValueResolution = true;
    }
    if (features.allowTranslateUniformBlockToStructuredBuffer.enabled)
    {
        options->allowTranslateUniformBlockToStructuredBuffer = true;
    }
    if (extensions.multiviewOVR || extensions.multiview2OVR)
    {
        options->initializeBuiltinsForInstancedMultiview = true;
    }
    if (extensions.shaderPixelLocalStorageANGLE)
    {
        options->pls = mRenderer->getNativePixelLocalStorageOptions();
    }

    auto postTranslateFunctor = [this](gl::ShCompilerInstance *compiler, std::string *infoLog) {
        const std::string &translatedSource = mState.getCompiledState()->translatedSource;
        CompiledShaderStateD3D *state       = mCompiledState.get();

        // TODO(jmadill): We shouldn't need to cache this.
        state->compilerOutputType = compiler->getShaderOutputType();

        state->usesMultipleRenderTargets =
            translatedSource.find("GL_USES_MRT") != std::string::npos;
        state->usesFragColor = translatedSource.find("GL_USES_FRAG_COLOR") != std::string::npos;
        state->usesFragData  = translatedSource.find("GL_USES_FRAG_DATA") != std::string::npos;
        state->usesSecondaryColor =
            translatedSource.find("GL_USES_SECONDARY_COLOR") != std::string::npos;
        state->usesFragCoord   = translatedSource.find("GL_USES_FRAG_COORD") != std::string::npos;
        state->usesFrontFacing = translatedSource.find("GL_USES_FRONT_FACING") != std::string::npos;
        state->usesSampleID    = translatedSource.find("GL_USES_SAMPLE_ID") != std::string::npos;
        state->usesSamplePosition =
            translatedSource.find("GL_USES_SAMPLE_POSITION") != std::string::npos;
        state->usesSampleMaskIn =
            translatedSource.find("GL_USES_SAMPLE_MASK_IN") != std::string::npos;
        state->usesSampleMask =
            translatedSource.find("GL_USES_SAMPLE_MASK_OUT") != std::string::npos;
        state->usesHelperInvocation =
            translatedSource.find("GL_USES_HELPER_INVOCATION") != std::string::npos;
        state->usesPointSize  = translatedSource.find("GL_USES_POINT_SIZE") != std::string::npos;
        state->usesPointCoord = translatedSource.find("GL_USES_POINT_COORD") != std::string::npos;
        state->usesDepthRange = translatedSource.find("GL_USES_DEPTH_RANGE") != std::string::npos;
        state->hasMultiviewEnabled =
            translatedSource.find("GL_MULTIVIEW_ENABLED") != std::string::npos;
        state->usesVertexID = translatedSource.find("GL_USES_VERTEX_ID") != std::string::npos;
        state->usesViewID   = translatedSource.find("GL_USES_VIEW_ID") != std::string::npos;
        state->usesDiscardRewriting =
            translatedSource.find("ANGLE_USES_DISCARD_REWRITING") != std::string::npos;
        state->usesNestedBreak =
            translatedSource.find("ANGLE_USES_NESTED_BREAK") != std::string::npos;
        state->requiresIEEEStrictCompiling =
            translatedSource.find("ANGLE_REQUIRES_IEEE_STRICT_COMPILING") != std::string::npos;

        ShHandle compilerHandle = compiler->getHandle();

        if (translatedSource.find("GL_USES_FRAG_DEPTH_GREATER") != std::string::npos)
        {
            state->fragDepthUsage = FragDepthUsage::Greater;
        }
        else if (translatedSource.find("GL_USES_FRAG_DEPTH_LESS") != std::string::npos)
        {
            state->fragDepthUsage = FragDepthUsage::Less;
        }
        else if (translatedSource.find("GL_USES_FRAG_DEPTH") != std::string::npos)
        {
            state->fragDepthUsage = FragDepthUsage::Any;
        }
        state->clipDistanceSize = sh::GetClipDistanceArraySize(compilerHandle);
        state->cullDistanceSize = sh::GetCullDistanceArraySize(compilerHandle);
        state->uniformRegisterMap =
            GetUniformRegisterMap(sh::GetUniformRegisterMap(compilerHandle));
        state->readonlyImage2DRegisterIndex = sh::GetReadonlyImage2DRegisterIndex(compilerHandle);
        state->image2DRegisterIndex         = sh::GetImage2DRegisterIndex(compilerHandle);
        state->usedImage2DFunctionNames =
            GetUsedImage2DFunctionNames(sh::GetUsedImage2DFunctionNames(compilerHandle));

        for (const sh::InterfaceBlock &interfaceBlock : mState.getCompiledState()->uniformBlocks)
        {
            if (interfaceBlock.active)
            {
                unsigned int index = static_cast<unsigned int>(-1);
                bool blockRegisterResult =
                    sh::GetUniformBlockRegister(compilerHandle, interfaceBlock.name, &index);
                ASSERT(blockRegisterResult);
                bool useStructuredBuffer =
                    sh::ShouldUniformBlockUseStructuredBuffer(compilerHandle, interfaceBlock.name);

                state->uniformBlockRegisterMap[interfaceBlock.name] = index;
                state->uniformBlockUseStructuredBufferMap[interfaceBlock.name] =
                    useStructuredBuffer;
            }
        }

        state->slowCompilingUniformBlockSet =
            GetSlowCompilingUniformBlockSet(sh::GetSlowCompilingUniformBlockSet(compilerHandle));

        for (const sh::InterfaceBlock &interfaceBlock :
             mState.getCompiledState()->shaderStorageBlocks)
        {
            if (interfaceBlock.active)
            {
                unsigned int index = static_cast<unsigned int>(-1);
                bool blockRegisterResult =
                    sh::GetShaderStorageBlockRegister(compilerHandle, interfaceBlock.name, &index);
                ASSERT(blockRegisterResult);

                state->shaderStorageBlockRegisterMap[interfaceBlock.name] = index;
            }
        }

        state->debugInfo += std::string("// ") + gl::GetShaderTypeString(mState.getShaderType()) +
                            " SHADER BEGIN\n";
        state->debugInfo += "\n// GLSL BEGIN\n\n" + mState.getSource() + "\n\n// GLSL END\n\n\n";
        state->debugInfo +=
            "// INITIAL HLSL BEGIN\n\n" + translatedSource + "\n// INITIAL HLSL END\n\n\n";
        // Successive steps will append more info
        return true;
    };

    auto workerThreadPool = context->getShaderCompileThreadPool();
    auto translateTask = std::make_shared<TranslateTaskD3D>(compilerInstance->getHandle(), *options,
                                                            source, sourcePath);

    return std::make_shared<WaitableCompileEventD3D>(
        workerThreadPool->postWorkerTask(translateTask), compilerInstance,
        std::move(postTranslateFunctor), translateTask);
}

bool CompiledShaderStateD3D::hasUniform(const std::string &name) const
{
    return uniformRegisterMap.find(name) != uniformRegisterMap.end();
}

}  // namespace rx
