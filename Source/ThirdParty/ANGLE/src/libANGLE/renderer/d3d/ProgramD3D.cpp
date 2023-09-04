//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ProgramD3D.cpp: Defines the rx::ProgramD3D class which implements rx::ProgramImpl.

#include "libANGLE/renderer/d3d/ProgramD3D.h"

#include "common/MemoryBuffer.h"
#include "common/bitset_utils.h"
#include "common/string_utils.h"
#include "common/utilities.h"
#include "libANGLE/Context.h"
#include "libANGLE/Program.h"
#include "libANGLE/ProgramLinkedResources.h"
#include "libANGLE/Uniform.h"
#include "libANGLE/VertexArray.h"
#include "libANGLE/features.h"
#include "libANGLE/queryconversions.h"
#include "libANGLE/renderer/ContextImpl.h"
#include "libANGLE/renderer/d3d/ContextD3D.h"
#include "libANGLE/renderer/d3d/ShaderD3D.h"
#include "libANGLE/renderer/d3d/ShaderExecutableD3D.h"
#include "libANGLE/renderer/d3d/VertexDataManager.h"
#include "libANGLE/renderer/renderer_utils.h"
#include "libANGLE/trace.h"

using namespace angle;

namespace rx
{
namespace
{
bool HasFlatInterpolationVarying(const std::vector<sh::ShaderVariable> &varyings)
{
    // Note: this assumes nested structs can only be packed with one interpolation.
    for (const auto &varying : varyings)
    {
        if (varying.interpolation == sh::INTERPOLATION_FLAT)
        {
            return true;
        }
    }

    return false;
}

bool FindFlatInterpolationVaryingPerShader(const gl::SharedCompiledShaderState &shader)
{
    ASSERT(shader);
    switch (shader->shaderType)
    {
        case gl::ShaderType::Vertex:
            return HasFlatInterpolationVarying(shader->outputVaryings);
        case gl::ShaderType::Fragment:
            return HasFlatInterpolationVarying(shader->inputVaryings);
        case gl::ShaderType::Geometry:
            return HasFlatInterpolationVarying(shader->inputVaryings) ||
                   HasFlatInterpolationVarying(shader->outputVaryings);
        default:
            UNREACHABLE();
            return false;
    }
}

bool FindFlatInterpolationVarying(const gl::ShaderMap<gl::SharedCompiledShaderState> &shaders)
{
    for (gl::ShaderType shaderType : gl::kAllGraphicsShaderTypes)
    {
        const gl::SharedCompiledShaderState &shader = shaders[shaderType];
        if (!shader)
        {
            continue;
        }

        if (FindFlatInterpolationVaryingPerShader(shader))
        {
            return true;
        }
    }

    return false;
}

class HLSLBlockLayoutEncoderFactory : public gl::CustomBlockLayoutEncoderFactory
{
  public:
    sh::BlockLayoutEncoder *makeEncoder() override
    {
        return new sh::HLSLBlockEncoder(sh::HLSLBlockEncoder::ENCODE_PACKED, false);
    }
};
}  // anonymous namespace

// ProgramD3DMetadata Implementation
ProgramD3DMetadata::ProgramD3DMetadata(
    RendererD3D *renderer,
    const gl::SharedCompiledShaderState &fragmentShader,
    const gl::ShaderMap<SharedCompiledShaderStateD3D> &attachedShaders,
    EGLenum clientType,
    int shaderVersion)
    : mRendererMajorShaderModel(renderer->getMajorShaderModel()),
      mShaderModelSuffix(renderer->getShaderModelSuffix()),
      mUsesInstancedPointSpriteEmulation(
          renderer->getFeatures().useInstancedPointSpriteEmulation.enabled),
      mUsesViewScale(renderer->presentPathFastEnabled()),
      mCanSelectViewInVertexShader(renderer->canSelectViewInVertexShader()),
      mFragmentShader(fragmentShader),
      mAttachedShaders(attachedShaders),
      mClientType(clientType),
      mShaderVersion(shaderVersion)
{}

ProgramD3DMetadata::~ProgramD3DMetadata() = default;

int ProgramD3DMetadata::getRendererMajorShaderModel() const
{
    return mRendererMajorShaderModel;
}

bool ProgramD3DMetadata::usesBroadcast(const gl::Version &clientVersion) const
{
    const SharedCompiledShaderStateD3D &shader = mAttachedShaders[gl::ShaderType::Fragment];
    return shader && shader->usesFragColor && shader->usesMultipleRenderTargets &&
           clientVersion.major < 3;
}

bool ProgramD3DMetadata::usesSecondaryColor() const
{
    const SharedCompiledShaderStateD3D &shader = mAttachedShaders[gl::ShaderType::Fragment];
    return shader && shader->usesSecondaryColor;
}

FragDepthUsage ProgramD3DMetadata::getFragDepthUsage() const
{
    const SharedCompiledShaderStateD3D &shader = mAttachedShaders[gl::ShaderType::Fragment];
    return shader ? shader->fragDepthUsage : FragDepthUsage::Unused;
}

bool ProgramD3DMetadata::usesPointCoord() const
{
    const SharedCompiledShaderStateD3D &shader = mAttachedShaders[gl::ShaderType::Fragment];
    return shader && shader->usesPointCoord;
}

bool ProgramD3DMetadata::usesFragCoord() const
{
    const SharedCompiledShaderStateD3D &shader = mAttachedShaders[gl::ShaderType::Fragment];
    return shader && shader->usesFragCoord;
}

bool ProgramD3DMetadata::usesPointSize() const
{
    const SharedCompiledShaderStateD3D &shader = mAttachedShaders[gl::ShaderType::Vertex];
    return shader && shader->usesPointSize;
}

bool ProgramD3DMetadata::usesInsertedPointCoordValue() const
{
    return (!usesPointSize() || !mUsesInstancedPointSpriteEmulation) && usesPointCoord() &&
           mRendererMajorShaderModel >= 4;
}

bool ProgramD3DMetadata::usesViewScale() const
{
    return mUsesViewScale;
}

bool ProgramD3DMetadata::hasMultiviewEnabled() const
{
    const SharedCompiledShaderStateD3D &shader = mAttachedShaders[gl::ShaderType::Vertex];
    return shader && shader->hasMultiviewEnabled;
}

bool ProgramD3DMetadata::usesVertexID() const
{
    const SharedCompiledShaderStateD3D &shader = mAttachedShaders[gl::ShaderType::Vertex];
    return shader && shader->usesVertexID;
}

bool ProgramD3DMetadata::usesViewID() const
{
    const SharedCompiledShaderStateD3D &shader = mAttachedShaders[gl::ShaderType::Fragment];
    return shader && shader->usesViewID;
}

bool ProgramD3DMetadata::canSelectViewInVertexShader() const
{
    return mCanSelectViewInVertexShader;
}

bool ProgramD3DMetadata::addsPointCoordToVertexShader() const
{
    // PointSprite emulation requiress that gl_PointCoord is present in the vertex shader
    // VS_OUTPUT structure to ensure compatibility with the generated PS_INPUT of the pixel shader.
    // Even with a geometry shader, the app can render triangles or lines and reference
    // gl_PointCoord in the fragment shader, requiring us to provide a placeholder value. For
    // simplicity, we always add this to the vertex shader when the fragment shader
    // references gl_PointCoord, even if we could skip it in the geometry shader.
    return (mUsesInstancedPointSpriteEmulation && usesPointCoord()) ||
           usesInsertedPointCoordValue();
}

bool ProgramD3DMetadata::usesTransformFeedbackGLPosition() const
{
    // gl_Position only needs to be outputted from the vertex shader if transform feedback is
    // active. This isn't supported on D3D11 Feature Level 9_3, so we don't output gl_Position from
    // the vertex shader in this case. This saves us 1 output vector.
    return !(mRendererMajorShaderModel >= 4 && mShaderModelSuffix != "");
}

bool ProgramD3DMetadata::usesSystemValuePointSize() const
{
    return !mUsesInstancedPointSpriteEmulation && usesPointSize();
}

bool ProgramD3DMetadata::usesMultipleFragmentOuts() const
{
    const SharedCompiledShaderStateD3D &shader = mAttachedShaders[gl::ShaderType::Fragment];
    return shader && shader->usesMultipleRenderTargets;
}

bool ProgramD3DMetadata::usesCustomOutVars() const
{
    switch (mClientType)
    {
        case EGL_OPENGL_API:
            return mShaderVersion >= 130;
        default:
            return mShaderVersion >= 300;
    }
}

bool ProgramD3DMetadata::usesSampleMask() const
{
    const SharedCompiledShaderStateD3D &shader = mAttachedShaders[gl::ShaderType::Fragment];
    return shader && shader->usesSampleMask;
}

const gl::SharedCompiledShaderState &ProgramD3DMetadata::getFragmentShader() const
{
    return mFragmentShader;
}

uint8_t ProgramD3DMetadata::getClipDistanceArraySize() const
{
    const SharedCompiledShaderStateD3D &shader = mAttachedShaders[gl::ShaderType::Vertex];
    return shader ? shader->clipDistanceSize : 0;
}

uint8_t ProgramD3DMetadata::getCullDistanceArraySize() const
{
    const SharedCompiledShaderStateD3D &shader = mAttachedShaders[gl::ShaderType::Vertex];
    return shader ? shader->cullDistanceSize : 0;
}

// ProgramD3D::GetExecutableTask class
class ProgramD3D::GetExecutableTask : public Closure, public d3d::Context
{
  public:
    GetExecutableTask(ProgramD3D *program, ProgramExecutableD3D *executable)
        : mProgram(program), mExecutable(executable)
    {}

    virtual angle::Result run() = 0;

    void operator()() override { mResult = run(); }

    angle::Result getResult() const { return mResult; }
    const gl::InfoLog &getInfoLog() const { return mInfoLog; }
    ShaderExecutableD3D *getExecutable() { return mShaderExecutable; }

    void handleResult(HRESULT hr,
                      const char *message,
                      const char *file,
                      const char *function,
                      unsigned int line) override
    {
        mStoredHR       = hr;
        mStoredMessage  = message;
        mStoredFile     = file;
        mStoredFunction = function;
        mStoredLine     = line;
    }

    void popError(d3d::Context *context)
    {
        ASSERT(mStoredFile);
        ASSERT(mStoredFunction);
        context->handleResult(mStoredHR, mStoredMessage.c_str(), mStoredFile, mStoredFunction,
                              mStoredLine);
    }

  protected:
    ProgramD3D *mProgram              = nullptr;
    ProgramExecutableD3D *mExecutable = nullptr;
    angle::Result mResult             = angle::Result::Continue;
    gl::InfoLog mInfoLog;
    ShaderExecutableD3D *mShaderExecutable = nullptr;
    HRESULT mStoredHR                      = S_OK;
    std::string mStoredMessage;
    const char *mStoredFile     = nullptr;
    const char *mStoredFunction = nullptr;
    unsigned int mStoredLine    = 0;
};

// ProgramD3D Implementation

unsigned int ProgramD3D::mCurrentSerial = 1;

ProgramD3D::ProgramD3D(const gl::ProgramState &state, RendererD3D *renderer)
    : ProgramImpl(state), mRenderer(renderer), mSerial(issueSerial())
{}

ProgramD3D::~ProgramD3D() = default;

void ProgramD3D::destroy(const gl::Context *context)
{
    reset();
}

class ProgramD3D::LoadBinaryTask : public ProgramD3D::GetExecutableTask
{
  public:
    LoadBinaryTask(ProgramD3D *program,
                   ProgramExecutableD3D *executable,
                   gl::BinaryInputStream *stream,
                   gl::InfoLog &infoLog)
        : ProgramD3D::GetExecutableTask(program, executable)
    {
        ASSERT(mProgram);
        ASSERT(mExecutable);
        ASSERT(stream);

        // Copy the remaining data from the stream locally so that the client can't modify it when
        // loading off thread.
        size_t dataSize    = stream->remainingSize();
        mDataCopySucceeded = mStreamData.resize(dataSize);
        if (mDataCopySucceeded)
        {
            memcpy(mStreamData.data(), stream->data() + stream->offset(), dataSize);
        }
    }

    angle::Result run() override
    {
        ANGLE_TRACE_EVENT0("gpu.angle", "ProgramD3D::LoadBinaryTask::run");
        if (!mDataCopySucceeded)
        {
            mInfoLog << "Failed to copy program binary data to local buffer.";
            return angle::Result::Incomplete;
        }

        gl::BinaryInputStream stream(mStreamData.data(), mStreamData.size());
        return mExecutable->loadBinaryShaderExecutables(this, mProgram->mRenderer, &stream,
                                                        mInfoLog);
    }

  private:
    bool mDataCopySucceeded;
    angle::MemoryBuffer mStreamData;
};

class ProgramD3D::LoadBinaryLinkEvent final : public LinkEvent
{
  public:
    LoadBinaryLinkEvent(std::shared_ptr<WorkerThreadPool> workerPool,
                        ProgramD3D *program,
                        ProgramExecutableD3D *executable,
                        gl::BinaryInputStream *stream,
                        gl::InfoLog &infoLog)
        : mTask(std::make_shared<ProgramD3D::LoadBinaryTask>(program, executable, stream, infoLog)),
          mWaitableEvent(workerPool->postWorkerTask(mTask))
    {}

    angle::Result wait(const gl::Context *context) override
    {
        mWaitableEvent->wait();

        // Continue and Incomplete are not errors. For Stop, pass the error to the ContextD3D.
        if (mTask->getResult() != angle::Result::Stop)
        {
            return angle::Result::Continue;
        }

        ContextD3D *contextD3D = GetImplAs<ContextD3D>(context);
        mTask->popError(contextD3D);
        return angle::Result::Stop;
    }

    bool isLinking() override { return !mWaitableEvent->isReady(); }

  private:
    std::shared_ptr<ProgramD3D::LoadBinaryTask> mTask;
    std::shared_ptr<WaitableEvent> mWaitableEvent;
};

std::unique_ptr<rx::LinkEvent> ProgramD3D::load(const gl::Context *context,
                                                gl::BinaryInputStream *stream)
{
    if (!getExecutable()->load(context, mRenderer, stream))
    {
        return nullptr;
    }

    return std::make_unique<LoadBinaryLinkEvent>(context->getShaderCompileThreadPool(), this,
                                                 getExecutable(), stream,
                                                 mState.getExecutable().getInfoLog());
}

void ProgramD3D::save(const gl::Context *context, gl::BinaryOutputStream *stream)
{
    getExecutable()->save(context, mRenderer, stream);
}

void ProgramD3D::setBinaryRetrievableHint(bool /* retrievable */) {}

void ProgramD3D::setSeparable(bool /* separable */) {}

class ProgramD3D::GetVertexExecutableTask : public ProgramD3D::GetExecutableTask
{
  public:
    GetVertexExecutableTask(ProgramD3D *program, ProgramExecutableD3D *executable)
        : GetExecutableTask(program, executable)
    {}
    angle::Result run() override
    {
        ANGLE_TRACE_EVENT0("gpu.angle", "ProgramD3D::GetVertexExecutableTask::run");

        mExecutable->updateCachedImage2DBindLayoutFromShader(gl::ShaderType::Vertex);
        ANGLE_TRY(mExecutable->getVertexExecutableForCachedInputLayout(
            this, mProgram->mRenderer, &mShaderExecutable, &mInfoLog));

        return angle::Result::Continue;
    }
};

class ProgramD3D::GetPixelExecutableTask : public ProgramD3D::GetExecutableTask
{
  public:
    GetPixelExecutableTask(ProgramD3D *program, ProgramExecutableD3D *executable)
        : GetExecutableTask(program, executable)
    {}
    angle::Result run() override
    {
        ANGLE_TRACE_EVENT0("gpu.angle", "ProgramD3D::GetPixelExecutableTask::run");
        if (!mProgram->mState.getAttachedShader(gl::ShaderType::Fragment))
        {
            return angle::Result::Continue;
        }

        mExecutable->updateCachedOutputLayoutFromShader();
        mExecutable->updateCachedImage2DBindLayoutFromShader(gl::ShaderType::Fragment);
        ANGLE_TRY(mExecutable->getPixelExecutableForCachedOutputLayout(
            this, mProgram->mRenderer, &mShaderExecutable, &mInfoLog));

        return angle::Result::Continue;
    }
};

class ProgramD3D::GetGeometryExecutableTask : public ProgramD3D::GetExecutableTask
{
  public:
    GetGeometryExecutableTask(ProgramD3D *program,
                              ProgramExecutableD3D *executable,
                              const gl::Caps &caps,
                              gl::ProvokingVertexConvention provokingVertex)
        : GetExecutableTask(program, executable), mCaps(caps), mProvokingVertex(provokingVertex)
    {}

    angle::Result run() override
    {
        ANGLE_TRACE_EVENT0("gpu.angle", "ProgramD3D::GetGeometryExecutableTask::run");
        // Auto-generate the geometry shader here, if we expect to be using point rendering in
        // D3D11.
        if (mExecutable->usesGeometryShader(mProgram->mRenderer, mProvokingVertex,
                                            gl::PrimitiveMode::Points))
        {
            ANGLE_TRY(mExecutable->getGeometryExecutableForPrimitiveType(
                this, mProgram->mRenderer, mCaps, mProvokingVertex, gl::PrimitiveMode::Points,
                &mShaderExecutable, &mInfoLog));
        }

        return angle::Result::Continue;
    }

  private:
    const gl::Caps &mCaps;
    gl::ProvokingVertexConvention mProvokingVertex;
};

class ProgramD3D::GetComputeExecutableTask : public ProgramD3D::GetExecutableTask
{
  public:
    GetComputeExecutableTask(ProgramD3D *program, ProgramExecutableD3D *executable)
        : GetExecutableTask(program, executable)
    {}
    angle::Result run() override
    {
        ANGLE_TRACE_EVENT0("gpu.angle", "ProgramD3D::GetComputeExecutableTask::run");
        mExecutable->updateCachedImage2DBindLayoutFromShader(gl::ShaderType::Compute);
        ShaderExecutableD3D *computeExecutable = nullptr;
        ANGLE_TRY(mExecutable->getComputeExecutableForImage2DBindLayout(
            this, mProgram->mRenderer, mProgram->mState.getAttachedShader(gl::ShaderType::Compute),
            &computeExecutable, &mInfoLog));

        return computeExecutable ? angle::Result::Continue : angle::Result::Incomplete;
    }
};

// The LinkEvent implementation for linking a rendering(VS, FS, GS) program.
class ProgramD3D::GraphicsProgramLinkEvent final : public LinkEvent
{
  public:
    GraphicsProgramLinkEvent(gl::InfoLog &infoLog,
                             std::shared_ptr<WorkerThreadPool> workerPool,
                             std::shared_ptr<ProgramD3D::GetVertexExecutableTask> vertexTask,
                             std::shared_ptr<ProgramD3D::GetPixelExecutableTask> pixelTask,
                             std::shared_ptr<ProgramD3D::GetGeometryExecutableTask> geometryTask,
                             bool useGS,
                             const SharedCompiledShaderStateD3D &vertexShader,
                             const SharedCompiledShaderStateD3D &fragmentShader)
        : mInfoLog(infoLog),
          mVertexTask(vertexTask),
          mPixelTask(pixelTask),
          mGeometryTask(geometryTask),
          mWaitEvents(
              {{std::shared_ptr<WaitableEvent>(workerPool->postWorkerTask(mVertexTask)),
                std::shared_ptr<WaitableEvent>(workerPool->postWorkerTask(mPixelTask)),
                std::shared_ptr<WaitableEvent>(workerPool->postWorkerTask(mGeometryTask))}}),
          mUseGS(useGS),
          mVertexShader(vertexShader),
          mFragmentShader(fragmentShader)
    {}

    angle::Result wait(const gl::Context *context) override
    {
        ANGLE_TRACE_EVENT0("gpu.angle", "ProgramD3D::GraphicsProgramLinkEvent::wait");
        WaitableEvent::WaitMany(&mWaitEvents);

        ANGLE_TRY(checkTask(context, mVertexTask.get()));
        ANGLE_TRY(checkTask(context, mPixelTask.get()));
        ANGLE_TRY(checkTask(context, mGeometryTask.get()));

        if (mVertexTask->getResult() == angle::Result::Incomplete ||
            mPixelTask->getResult() == angle::Result::Incomplete ||
            mGeometryTask->getResult() == angle::Result::Incomplete)
        {
            return angle::Result::Incomplete;
        }

        ShaderExecutableD3D *defaultVertexExecutable = mVertexTask->getExecutable();
        ShaderExecutableD3D *defaultPixelExecutable  = mPixelTask->getExecutable();
        ShaderExecutableD3D *pointGS                 = mGeometryTask->getExecutable();

        if (mUseGS && pointGS)
        {
            // Geometry shaders are currently only used internally, so there is no corresponding
            // shader object at the interface level. For now the geometry shader debug info is
            // prepended to the vertex shader.
            mVertexShader->appendDebugInfo("// GEOMETRY SHADER BEGIN\n\n");
            mVertexShader->appendDebugInfo(pointGS->getDebugInfo());
            mVertexShader->appendDebugInfo("\nGEOMETRY SHADER END\n\n\n");
        }

        if (defaultVertexExecutable)
        {
            mVertexShader->appendDebugInfo(defaultVertexExecutable->getDebugInfo());
        }

        if (defaultPixelExecutable)
        {
            mFragmentShader->appendDebugInfo(defaultPixelExecutable->getDebugInfo());
        }

        bool isLinked = (defaultVertexExecutable && defaultPixelExecutable && (!mUseGS || pointGS));
        if (!isLinked)
        {
            mInfoLog << "Failed to create D3D Shaders";
        }
        return isLinked ? angle::Result::Continue : angle::Result::Incomplete;
    }

    bool isLinking() override { return !WaitableEvent::AllReady(&mWaitEvents); }

  private:
    angle::Result checkTask(const gl::Context *context, ProgramD3D::GetExecutableTask *task)
    {
        if (!task->getInfoLog().empty())
        {
            mInfoLog << task->getInfoLog().str();
        }

        // Continue and Incomplete are not errors. For Stop, pass the error to the ContextD3D.
        if (task->getResult() != angle::Result::Stop)
        {
            return angle::Result::Continue;
        }

        ContextD3D *contextD3D = GetImplAs<ContextD3D>(context);
        task->popError(contextD3D);
        return angle::Result::Stop;
    }

    gl::InfoLog &mInfoLog;
    std::shared_ptr<ProgramD3D::GetVertexExecutableTask> mVertexTask;
    std::shared_ptr<ProgramD3D::GetPixelExecutableTask> mPixelTask;
    std::shared_ptr<ProgramD3D::GetGeometryExecutableTask> mGeometryTask;
    std::array<std::shared_ptr<WaitableEvent>, 3> mWaitEvents;
    bool mUseGS;
    SharedCompiledShaderStateD3D mVertexShader;
    SharedCompiledShaderStateD3D mFragmentShader;
};

// The LinkEvent implementation for linking a computing program.
class ProgramD3D::ComputeProgramLinkEvent final : public LinkEvent
{
  public:
    ComputeProgramLinkEvent(gl::InfoLog &infoLog,
                            std::shared_ptr<ProgramD3D::GetComputeExecutableTask> computeTask,
                            std::shared_ptr<WaitableEvent> event)
        : mInfoLog(infoLog), mComputeTask(computeTask), mWaitEvent(event)
    {}

    bool isLinking() override { return !mWaitEvent->isReady(); }

    angle::Result wait(const gl::Context *context) override
    {
        ANGLE_TRACE_EVENT0("gpu.angle", "ProgramD3D::ComputeProgramLinkEvent::wait");
        mWaitEvent->wait();

        angle::Result result = mComputeTask->getResult();
        if (result != angle::Result::Continue)
        {
            mInfoLog << "Failed to create D3D compute shader.";
        }
        return result;
    }

  private:
    gl::InfoLog &mInfoLog;
    std::shared_ptr<ProgramD3D::GetComputeExecutableTask> mComputeTask;
    std::shared_ptr<WaitableEvent> mWaitEvent;
};

std::unique_ptr<LinkEvent> ProgramD3D::compileProgramExecutables(const gl::Context *context)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "ProgramD3D::compileProgramExecutables");

    ProgramExecutableD3D *executableD3D = getExecutable();

    auto vertexTask   = std::make_shared<GetVertexExecutableTask>(this, executableD3D);
    auto pixelTask    = std::make_shared<GetPixelExecutableTask>(this, executableD3D);
    auto geometryTask = std::make_shared<GetGeometryExecutableTask>(
        this, executableD3D, context->getCaps(), context->getState().getProvokingVertex());
    bool useGS = executableD3D->usesGeometryShader(
        mRenderer, context->getState().getProvokingVertex(), gl::PrimitiveMode::Points);
    const SharedCompiledShaderStateD3D &vertexShaderD3D =
        executableD3D->mAttachedShaders[gl::ShaderType::Vertex];
    const SharedCompiledShaderStateD3D &fragmentShaderD3D =
        executableD3D->mAttachedShaders[gl::ShaderType::Fragment];

    return std::make_unique<GraphicsProgramLinkEvent>(
        mState.getExecutable().getInfoLog(), context->getShaderCompileThreadPool(), vertexTask,
        pixelTask, geometryTask, useGS, vertexShaderD3D, fragmentShaderD3D);
}

std::unique_ptr<LinkEvent> ProgramD3D::compileComputeExecutable(const gl::Context *context)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "ProgramD3D::compileComputeExecutable");
    auto computeTask = std::make_shared<GetComputeExecutableTask>(this, getExecutable());

    std::shared_ptr<WaitableEvent> waitableEvent;

    // TODO(jie.a.chen@intel.com): Fix the flaky bug.
    // http://anglebug.com/3349
    bool compileInParallel = false;
    if (!compileInParallel)
    {
        (*computeTask)();
        waitableEvent = std::make_shared<WaitableEventDone>();
    }
    else
    {
        waitableEvent = context->getShaderCompileThreadPool()->postWorkerTask(computeTask);
    }

    return std::make_unique<ComputeProgramLinkEvent>(mState.getExecutable().getInfoLog(),
                                                     computeTask, waitableEvent);
}

void ProgramD3D::prepareForLink(const gl::ShaderMap<ShaderImpl *> &shaders)
{
    ProgramExecutableD3D *executableD3D = getExecutable();

    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        executableD3D->mAttachedShaders[shaderType].reset();

        if (shaders[shaderType] != nullptr)
        {
            const ShaderD3D *shaderD3D                  = GetAs<ShaderD3D>(shaders[shaderType]);
            executableD3D->mAttachedShaders[shaderType] = shaderD3D->getCompiledState();
        }
    }
}

std::unique_ptr<LinkEvent> ProgramD3D::link(const gl::Context *context,
                                            const gl::ProgramLinkedResources &resources,
                                            gl::ProgramMergedVaryings && /*mergedVaryings*/)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "ProgramD3D::link");
    const gl::Version &clientVersion = context->getClientVersion();
    const gl::Caps &caps             = context->getCaps();
    EGLenum clientType               = context->getClientType();

    // Ensure the compiler is initialized to avoid race conditions.
    angle::Result result = mRenderer->ensureHLSLCompilerInitialized(GetImplAs<ContextD3D>(context));
    if (result != angle::Result::Continue)
    {
        return std::make_unique<LinkEventDone>(result);
    }

    ProgramExecutableD3D *executableD3D = getExecutable();

    reset();

    const gl::SharedCompiledShaderState &computeShader =
        mState.getAttachedShader(gl::ShaderType::Compute);
    if (!computeShader)
    {
        for (gl::ShaderType shaderType : gl::kAllGraphicsShaderTypes)
        {
            const SharedCompiledShaderStateD3D &shader =
                executableD3D->mAttachedShaders[shaderType];
            if (shader)
            {
                const std::set<std::string> &slowCompilingUniformBlockSet =
                    shader->slowCompilingUniformBlockSet;
                if (slowCompilingUniformBlockSet.size() > 0)
                {
                    std::ostringstream stream;
                    stream << "You could get a better shader compiling performance if you re-write"
                           << " the uniform block(s)\n[ ";
                    for (const std::string &str : slowCompilingUniformBlockSet)
                    {
                        stream << str << " ";
                    }
                    stream << "]\nin the " << gl::GetShaderTypeString(shaderType) << " shader.\n";

                    stream << "You could get more details from "
                              "https://chromium.googlesource.com/angle/angle/+/refs/heads/main/"
                              "src/libANGLE/renderer/d3d/d3d11/"
                              "UniformBlockToStructuredBufferTranslation.md\n";
                    ANGLE_PERF_WARNING(context->getState().getDebug(), GL_DEBUG_SEVERITY_MEDIUM,
                                       stream.str().c_str());
                }
            }
        }
    }

    if (computeShader)
    {
        executableD3D->mShaderSamplers[gl::ShaderType::Compute].resize(
            caps.maxShaderTextureImageUnits[gl::ShaderType::Compute]);
        executableD3D->mImages[gl::ShaderType::Compute].resize(caps.maxImageUnits);
        executableD3D->mReadonlyImages[gl::ShaderType::Compute].resize(caps.maxImageUnits);

        executableD3D->mShaderUniformsDirty.set(gl::ShaderType::Compute);

        linkResources(resources);

        for (const sh::ShaderVariable &uniform : computeShader->uniforms)
        {
            if (gl::IsImageType(uniform.type) && gl::IsImage2DType(uniform.type))
            {
                executableD3D->mImage2DUniforms[gl::ShaderType::Compute].push_back(uniform);
            }
        }

        executableD3D->defineUniformsAndAssignRegisters(mRenderer, mState.getAttachedShaders());

        return compileComputeExecutable(context);
    }
    else
    {
        for (gl::ShaderType shaderType : gl::kAllGraphicsShaderTypes)
        {
            const gl::SharedCompiledShaderState &shader = mState.getAttachedShader(shaderType);
            if (shader)
            {
                executableD3D->mAttachedShaders[shaderType]->generateWorkarounds(
                    &executableD3D->mShaderWorkarounds[shaderType]);
                executableD3D->mShaderSamplers[shaderType].resize(
                    caps.maxShaderTextureImageUnits[shaderType]);
                executableD3D->mImages[shaderType].resize(caps.maxImageUnits);
                executableD3D->mReadonlyImages[shaderType].resize(caps.maxImageUnits);

                executableD3D->mShaderUniformsDirty.set(shaderType);

                for (const sh::ShaderVariable &uniform : shader->uniforms)
                {
                    if (gl::IsImageType(uniform.type) && gl::IsImage2DType(uniform.type))
                    {
                        executableD3D->mImage2DUniforms[shaderType].push_back(uniform);
                    }
                }
            }
        }

        if (mRenderer->getNativeLimitations().noFrontFacingSupport)
        {
            const SharedCompiledShaderStateD3D &fragmentShader =
                executableD3D->mAttachedShaders[gl::ShaderType::Fragment];
            if (fragmentShader && fragmentShader->usesFrontFacing)
            {
                mState.getExecutable().getInfoLog()
                    << "The current renderer doesn't support gl_FrontFacing";
                return std::make_unique<LinkEventDone>(angle::Result::Incomplete);
            }
        }

        const gl::VaryingPacking &varyingPacking =
            resources.varyingPacking.getOutputPacking(gl::ShaderType::Vertex);

        ProgramD3DMetadata metadata(
            mRenderer, mState.getAttachedShader(gl::ShaderType::Fragment),
            executableD3D->mAttachedShaders, clientType,
            mState.getAttachedShader(gl::ShaderType::Vertex)->shaderVersion);
        BuiltinVaryingsD3D builtins(metadata, varyingPacking);

        DynamicHLSL::GenerateShaderLinkHLSL(mRenderer, caps, mState.getAttachedShaders(),
                                            executableD3D->mAttachedShaders, metadata,
                                            varyingPacking, builtins, &executableD3D->mShaderHLSL);

        executableD3D->mUsesPointSize =
            executableD3D->mAttachedShaders[gl::ShaderType::Vertex] &&
            executableD3D->mAttachedShaders[gl::ShaderType::Vertex]->usesPointSize;
        DynamicHLSL::GetPixelShaderOutputKey(mRenderer, caps, clientVersion, mState, metadata,
                                             &executableD3D->mPixelShaderKey);
        executableD3D->mFragDepthUsage      = metadata.getFragDepthUsage();
        executableD3D->mUsesSampleMask      = metadata.usesSampleMask();
        executableD3D->mUsesVertexID        = metadata.usesVertexID();
        executableD3D->mUsesViewID          = metadata.usesViewID();
        executableD3D->mHasMultiviewEnabled = metadata.hasMultiviewEnabled();

        // Cache if we use flat shading
        executableD3D->mUsesFlatInterpolation =
            FindFlatInterpolationVarying(mState.getAttachedShaders());

        if (mRenderer->getMajorShaderModel() >= 4)
        {
            executableD3D->mGeometryShaderPreamble = DynamicHLSL::GenerateGeometryShaderPreamble(
                mRenderer, varyingPacking, builtins, executableD3D->mHasMultiviewEnabled,
                metadata.canSelectViewInVertexShader());
        }

        executableD3D->initAttribLocationsToD3DSemantic(
            mState.getAttachedShader(gl::ShaderType::Vertex));

        executableD3D->defineUniformsAndAssignRegisters(mRenderer, mState.getAttachedShaders());

        executableD3D->gatherTransformFeedbackVaryings(mRenderer, varyingPacking,
                                                       mState.getTransformFeedbackVaryingNames(),
                                                       builtins[gl::ShaderType::Vertex]);

        linkResources(resources);

        if (mState.getAttachedShader(gl::ShaderType::Vertex))
        {
            executableD3D->updateCachedInputLayoutFromShader(
                mRenderer, mState.getAttachedShader(gl::ShaderType::Vertex));
        }

        return compileProgramExecutables(context);
    }
}

GLboolean ProgramD3D::validate(const gl::Caps & /*caps*/)
{
    // TODO(jmadill): Do something useful here?
    return GL_TRUE;
}

void ProgramD3D::setUniform1fv(GLint location, GLsizei count, const GLfloat *v)
{
    setUniformInternal(location, count, v, GL_FLOAT);
}

void ProgramD3D::setUniform2fv(GLint location, GLsizei count, const GLfloat *v)
{
    setUniformInternal(location, count, v, GL_FLOAT_VEC2);
}

void ProgramD3D::setUniform3fv(GLint location, GLsizei count, const GLfloat *v)
{
    setUniformInternal(location, count, v, GL_FLOAT_VEC3);
}

void ProgramD3D::setUniform4fv(GLint location, GLsizei count, const GLfloat *v)
{
    setUniformInternal(location, count, v, GL_FLOAT_VEC4);
}

void ProgramD3D::setUniformMatrix2fv(GLint location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLfloat *value)
{
    setUniformMatrixfvInternal<2, 2>(location, count, transpose, value);
}

void ProgramD3D::setUniformMatrix3fv(GLint location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLfloat *value)
{
    setUniformMatrixfvInternal<3, 3>(location, count, transpose, value);
}

void ProgramD3D::setUniformMatrix4fv(GLint location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLfloat *value)
{
    setUniformMatrixfvInternal<4, 4>(location, count, transpose, value);
}

void ProgramD3D::setUniformMatrix2x3fv(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    setUniformMatrixfvInternal<2, 3>(location, count, transpose, value);
}

void ProgramD3D::setUniformMatrix3x2fv(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    setUniformMatrixfvInternal<3, 2>(location, count, transpose, value);
}

void ProgramD3D::setUniformMatrix2x4fv(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    setUniformMatrixfvInternal<2, 4>(location, count, transpose, value);
}

void ProgramD3D::setUniformMatrix4x2fv(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    setUniformMatrixfvInternal<4, 2>(location, count, transpose, value);
}

void ProgramD3D::setUniformMatrix3x4fv(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    setUniformMatrixfvInternal<3, 4>(location, count, transpose, value);
}

void ProgramD3D::setUniformMatrix4x3fv(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    setUniformMatrixfvInternal<4, 3>(location, count, transpose, value);
}

void ProgramD3D::setUniform1iv(GLint location, GLsizei count, const GLint *v)
{
    setUniformInternal(location, count, v, GL_INT);
}

void ProgramD3D::setUniform2iv(GLint location, GLsizei count, const GLint *v)
{
    setUniformInternal(location, count, v, GL_INT_VEC2);
}

void ProgramD3D::setUniform3iv(GLint location, GLsizei count, const GLint *v)
{
    setUniformInternal(location, count, v, GL_INT_VEC3);
}

void ProgramD3D::setUniform4iv(GLint location, GLsizei count, const GLint *v)
{
    setUniformInternal(location, count, v, GL_INT_VEC4);
}

void ProgramD3D::setUniform1uiv(GLint location, GLsizei count, const GLuint *v)
{
    setUniformInternal(location, count, v, GL_UNSIGNED_INT);
}

void ProgramD3D::setUniform2uiv(GLint location, GLsizei count, const GLuint *v)
{
    setUniformInternal(location, count, v, GL_UNSIGNED_INT_VEC2);
}

void ProgramD3D::setUniform3uiv(GLint location, GLsizei count, const GLuint *v)
{
    setUniformInternal(location, count, v, GL_UNSIGNED_INT_VEC3);
}

void ProgramD3D::setUniform4uiv(GLint location, GLsizei count, const GLuint *v)
{
    setUniformInternal(location, count, v, GL_UNSIGNED_INT_VEC4);
}

// Assume count is already clamped.
template <typename T>
void ProgramD3D::setUniformImpl(D3DUniform *targetUniform,
                                const gl::VariableLocation &locationInfo,
                                GLsizei count,
                                const T *v,
                                uint8_t *targetState,
                                GLenum uniformType)
{
    const int components                  = targetUniform->typeInfo.componentCount;
    const unsigned int arrayElementOffset = locationInfo.arrayIndex;
    const int blockSize                   = 4;

    if (targetUniform->typeInfo.type == uniformType)
    {
        T *dest         = reinterpret_cast<T *>(targetState) + arrayElementOffset * blockSize;
        const T *source = v;

        // If the component is equal to the block size, we can optimize to a single memcpy.
        // Otherwise, we have to do partial block writes.
        if (components == blockSize)
        {
            memcpy(dest, source, components * count * sizeof(T));
        }
        else
        {
            for (GLint i = 0; i < count; i++, dest += blockSize, source += components)
            {
                memcpy(dest, source, components * sizeof(T));
            }
        }
    }
    else
    {
        ASSERT(targetUniform->typeInfo.type == gl::VariableBoolVectorType(uniformType));
        GLint *boolParams = reinterpret_cast<GLint *>(targetState) + arrayElementOffset * 4;

        for (GLint i = 0; i < count; i++)
        {
            GLint *dest     = boolParams + (i * 4);
            const T *source = v + (i * components);

            for (int c = 0; c < components; c++)
            {
                dest[c] = (source[c] == static_cast<T>(0)) ? GL_FALSE : GL_TRUE;
            }
        }
    }
}

template <typename T>
void ProgramD3D::setUniformInternal(GLint location, GLsizei count, const T *v, GLenum uniformType)
{
    ProgramExecutableD3D *executableD3D = getExecutable();

    const gl::VariableLocation &locationInfo = mState.getUniformLocations()[location];
    D3DUniform *targetUniform                = executableD3D->mD3DUniforms[locationInfo.index];

    if (targetUniform->typeInfo.isSampler)
    {
        ASSERT(uniformType == GL_INT);
        size_t size = count * sizeof(T);
        GLint *dest = &targetUniform->mSamplerData[locationInfo.arrayIndex];
        if (memcmp(dest, v, size) != 0)
        {
            memcpy(dest, v, size);
            executableD3D->mDirtySamplerMapping = true;
        }
        return;
    }

    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        uint8_t *targetState = targetUniform->mShaderData[shaderType];
        if (targetState)
        {
            setUniformImpl(targetUniform, locationInfo, count, v, targetState, uniformType);
            executableD3D->mShaderUniformsDirty.set(shaderType);
        }
    }
}

template <int cols, int rows>
void ProgramD3D::setUniformMatrixfvInternal(GLint location,
                                            GLsizei countIn,
                                            GLboolean transpose,
                                            const GLfloat *value)
{
    ProgramExecutableD3D *executableD3D = getExecutable();

    const gl::VariableLocation &uniformLocation = mState.getUniformLocations()[location];
    D3DUniform *targetUniform       = executableD3D->getD3DUniformFromLocation(uniformLocation);
    unsigned int arrayElementOffset = uniformLocation.arrayIndex;
    unsigned int elementCount       = targetUniform->getArraySizeProduct();

    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        if (targetUniform->mShaderData[shaderType])
        {
            SetFloatUniformMatrixHLSL<cols, rows>::Run(arrayElementOffset, elementCount, countIn,
                                                       transpose, value,
                                                       targetUniform->mShaderData[shaderType]);
            executableD3D->mShaderUniformsDirty.set(shaderType);
        }
    }
}

void ProgramD3D::reset()
{
    getExecutable()->reset();
}

unsigned int ProgramD3D::getSerial() const
{
    return mSerial;
}

unsigned int ProgramD3D::issueSerial()
{
    return mCurrentSerial++;
}

template <typename DestT>
void ProgramD3D::getUniformInternal(GLint location, DestT *dataOut) const
{
    const ProgramExecutableD3D *executableD3D = getExecutable();

    const gl::VariableLocation &locationInfo = mState.getUniformLocations()[location];
    const gl::LinkedUniform &uniform         = mState.getUniforms()[locationInfo.index];

    const D3DUniform *targetUniform = executableD3D->getD3DUniformFromLocation(locationInfo);
    const uint8_t *srcPointer       = targetUniform->getDataPtrToElement(locationInfo.arrayIndex);

    if (gl::IsMatrixType(uniform.getType()))
    {
        GetMatrixUniform(uniform.getType(), dataOut, reinterpret_cast<const DestT *>(srcPointer),
                         true);
    }
    else
    {
        memcpy(dataOut, srcPointer, uniform.getElementSize());
    }
}

void ProgramD3D::getUniformfv(const gl::Context *context, GLint location, GLfloat *params) const
{
    getUniformInternal(location, params);
}

void ProgramD3D::getUniformiv(const gl::Context *context, GLint location, GLint *params) const
{
    getUniformInternal(location, params);
}

void ProgramD3D::getUniformuiv(const gl::Context *context, GLint location, GLuint *params) const
{
    getUniformInternal(location, params);
}

void ProgramD3D::linkResources(const gl::ProgramLinkedResources &resources)
{
    HLSLBlockLayoutEncoderFactory hlslEncoderFactory;
    gl::ProgramLinkedResourcesLinker linker(&hlslEncoderFactory);

    linker.linkResources(mState, resources);

    ProgramExecutableD3D *executableD3D = getExecutable();

    executableD3D->initializeUniformBlocks();
    executableD3D->initializeShaderStorageBlocks(mState.getAttachedShaders());
}

}  // namespace rx
