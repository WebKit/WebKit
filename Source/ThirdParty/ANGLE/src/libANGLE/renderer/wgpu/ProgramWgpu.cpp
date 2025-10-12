//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ProgramWgpu.cpp:
//    Implements the class methods for ProgramWgpu.
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "libANGLE/renderer/wgpu/ProgramWgpu.h"

#include "GLES2/gl2.h"
#include "common/PackedEnums.h"
#include "common/PackedGLEnums_autogen.h"
#include "common/debug.h"
#include "common/log_utils.h"
#include "libANGLE/Error.h"
#include "libANGLE/ProgramExecutable.h"
#include "libANGLE/renderer/wgpu/ProgramExecutableWgpu.h"
#include "libANGLE/renderer/wgpu/wgpu_utils.h"
#include "libANGLE/renderer/wgpu/wgpu_wgsl_util.h"
#include "libANGLE/trace.h"
#include "platform/autogen/FeaturesWgpu_autogen.h"

namespace rx
{
namespace
{
const bool kOutputFinalSource = false;

// Identical to Std140 encoder in all aspects, except it ignores opaque uniform types.
class WgpuDefaultBlockEncoder : public sh::Std140BlockEncoder
{
  public:
    void advanceOffset(GLenum type,
                       const std::vector<unsigned int> &arraySizes,
                       bool isRowMajorMatrix,
                       int arrayStride,
                       int matrixStride) override
    {
        if (gl::IsOpaqueType(type))
        {
            return;
        }

        sh::Std140BlockEncoder::advanceOffset(type, arraySizes, isRowMajorMatrix, arrayStride,
                                              matrixStride);
    }
};

angle::Result InitDefaultUniformBlock(const std::vector<sh::ShaderVariable> &uniforms,
                                      sh::BlockLayoutMap *blockLayoutMapOut,
                                      size_t *blockSizeOut)
{
    if (uniforms.empty())
    {
        *blockSizeOut = 0;
        return angle::Result::Continue;
    }

    WgpuDefaultBlockEncoder blockEncoder;
    sh::GetActiveUniformBlockInfo(uniforms, "", &blockEncoder, blockLayoutMapOut);

    // The default uniforms are packed into a struct, and so the size of the struct must be aligned
    // to kUniformStructAlignment;
    angle::CheckedNumeric blockSize =
        CheckedRoundUp(blockEncoder.getCurrentOffset(), webgpu::kUniformStructAlignment);
    if (!blockSize.IsValid())
    {
        ERR() << "Packing the default uniforms into a struct results in a struct that is too "
                 "large. Unaligned size = "
              << blockEncoder.getCurrentOffset()
              << ", alignment = " << webgpu::kUniformStructAlignment;
        return angle::Result::Stop;
    }

    *blockSizeOut = blockSize.ValueOrDie();
    return angle::Result::Continue;
}

std::string FormatWGPUCompilationMessage(const WGPUCompilationMessage &message)
{
    std::ostringstream oss;
    oss << message.lineNum << ":" << message.linePos << ": "
        << std::string(message.message.data, message.message.length);
    return oss.str();
}

class CreateWGPUShaderModuleTask : public LinkSubTask
{
  public:
    CreateWGPUShaderModuleTask(const DawnProcTable *wgpu,
                               webgpu::InstanceHandle instance,
                               webgpu::DeviceHandle device,
                               const angle::FeaturesWgpu &features,
                               const gl::SharedCompiledShaderState &compiledShaderState,
                               const gl::ProgramExecutable &executable,
                               gl::ProgramMergedVaryings mergedVaryings,
                               TranslatedWGPUShaderModule &resultShaderModule)
        : mProcTable(wgpu),
          mInstance(instance),
          mDevice(device),
          mFeatures(features),
          mCompiledShaderState(compiledShaderState),
          mExecutable(executable),
          mMergedVaryings(std::move(mergedVaryings)),
          mShaderModule(resultShaderModule)
    {}

    angle::Result getResult(const gl::Context *context, gl::InfoLog &infoLog) override
    {
        infoLog << mLog.str();
        return mResult;
    }

    void operator()() override
    {
        ANGLE_TRACE_EVENT0("gpu.angle", "CreateWGPUShaderModuleTask");

        gl::ShaderType shaderType = mCompiledShaderState->shaderType;

        ASSERT((mExecutable.getLinkedShaderStages() &
                ~gl::ShaderBitSet({gl::ShaderType::Vertex, gl::ShaderType::Fragment}))
                   .none());
        std::string finalShaderSource;
        if (shaderType == gl::ShaderType::Vertex)
        {
            finalShaderSource = webgpu::WgslAssignLocationsAndSamplerBindings(
                mExecutable, mCompiledShaderState->translatedSource, mExecutable.getProgramInputs(),
                mMergedVaryings, shaderType);
        }
        else if (shaderType == gl::ShaderType::Fragment)
        {
            finalShaderSource = webgpu::WgslAssignLocationsAndSamplerBindings(
                mExecutable, mCompiledShaderState->translatedSource,
                mExecutable.getOutputVariables(), mMergedVaryings, shaderType);
        }
        else
        {
            UNIMPLEMENTED();
        }
        if (kOutputFinalSource)
        {
            std::cout << finalShaderSource;
        }

        WGPUShaderSourceWGSL shaderModuleWGSLDescriptor = WGPU_SHADER_SOURCE_WGSL_INIT;
        shaderModuleWGSLDescriptor.code = {finalShaderSource.c_str(), finalShaderSource.length()};

        WGPUShaderModuleDescriptor shaderModuleDescriptor = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
        shaderModuleDescriptor.nextInChain                = &shaderModuleWGSLDescriptor.chain;

        mShaderModule.module = webgpu::ShaderModuleHandle::Acquire(
            mProcTable,
            mProcTable->deviceCreateShaderModule(mDevice.get(), &shaderModuleDescriptor));

        if (mFeatures.avoidWaitAny.enabled)
        {
            WGPUCompilationInfoCallbackInfo getCompilationInfoCallback =
                WGPU_COMPILATION_INFO_CALLBACK_INFO_INIT;
            getCompilationInfoCallback.mode = WGPUCallbackMode_AllowSpontaneous;
            getCompilationInfoCallback.callback =
                [](WGPUCompilationInfoRequestStatus status,
                   struct WGPUCompilationInfo const *compilationInfo, void *userdata1,
                   void *userdata2) {
                    ASSERT(userdata1 == nullptr);
                    ASSERT(userdata2 == nullptr);
                    for (size_t msgIdx = 0;
                         compilationInfo && msgIdx < compilationInfo->messageCount; ++msgIdx)
                    {
                        const WGPUCompilationMessage &message = compilationInfo->messages[msgIdx];
                        switch (message.type)
                        {
                            case WGPUCompilationMessageType_Error:
                                ERR() << FormatWGPUCompilationMessage(message);
                                break;
                            case WGPUCompilationMessageType_Warning:
                                WARN() << FormatWGPUCompilationMessage(message);
                                break;
                            case WGPUCompilationMessageType_Info:
                                INFO() << FormatWGPUCompilationMessage(message);
                                break;
                            default:
                                UNIMPLEMENTED();
                                break;
                        }
                    }
                };

            mProcTable->shaderModuleGetCompilationInfo(mShaderModule.module.get(),
                                                       getCompilationInfoCallback);
        }
        else
        {
            WGPUCompilationInfoCallbackInfo getCompilationInfoCallback =
                WGPU_COMPILATION_INFO_CALLBACK_INFO_INIT;
            getCompilationInfoCallback.mode = WGPUCallbackMode_WaitAnyOnly;
            getCompilationInfoCallback.callback =
                [](WGPUCompilationInfoRequestStatus status,
                   struct WGPUCompilationInfo const *compilationInfo, void *userdata1,
                   void *userdata2) {
                    CreateWGPUShaderModuleTask *task =
                        reinterpret_cast<CreateWGPUShaderModuleTask *>(userdata1);
                    ASSERT(userdata2 == nullptr);
                    if (status != WGPUCompilationInfoRequestStatus_Success)
                    {
                        task->mResult = angle::Result::Stop;
                    }

                    for (size_t msgIdx = 0;
                         compilationInfo && msgIdx < compilationInfo->messageCount; ++msgIdx)
                    {
                        const WGPUCompilationMessage &message = compilationInfo->messages[msgIdx];
                        switch (message.type)
                        {
                            case WGPUCompilationMessageType_Error:
                                task->mLog << "Error: ";
                                break;
                            case WGPUCompilationMessageType_Warning:
                                task->mLog << "Warning: ";
                                break;
                            case WGPUCompilationMessageType_Info:
                                task->mLog << "Info: ";
                                break;
                            default:
                                task->mLog << "Unknown: ";
                                break;
                        }
                        task->mLog << FormatWGPUCompilationMessage(message) << std::endl;
                    }
                };
            getCompilationInfoCallback.userdata1 = this;

            WGPUFutureWaitInfo waitInfo = WGPU_FUTURE_WAIT_INFO_INIT;
            waitInfo.future             = mProcTable->shaderModuleGetCompilationInfo(
                mShaderModule.module.get(), getCompilationInfoCallback);
            WGPUWaitStatus waitStatus =
                mProcTable->instanceWaitAny(mInstance.get(), 1, &waitInfo, -1);
            if (waitStatus != WGPUWaitStatus_Success)
            {
                mResult = angle::Result::Stop;
            }
        }
    }

  private:
    const DawnProcTable *mProcTable = nullptr;
    webgpu::InstanceHandle mInstance;
    webgpu::DeviceHandle mDevice;
    const angle::FeaturesWgpu &mFeatures;
    gl::SharedCompiledShaderState mCompiledShaderState;
    const gl::ProgramExecutable &mExecutable;
    gl::ProgramMergedVaryings mMergedVaryings;

    TranslatedWGPUShaderModule &mShaderModule;

    std::ostringstream mLog;
    angle::Result mResult = angle::Result::Continue;
};

class LinkTaskWgpu : public LinkTask
{
  public:
    LinkTaskWgpu(const DawnProcTable *wgpu,
                 webgpu::InstanceHandle instance,
                 webgpu::DeviceHandle device,
                 const angle::FeaturesWgpu &features,
                 ProgramWgpu *program)
        : mProcTable(wgpu),
          mInstance(instance),
          mDevice(device),
          mFeatures(features),
          mProgram(program),
          mExecutable(&mProgram->getState().getExecutable())
    {}
    ~LinkTaskWgpu() override = default;

    void link(const gl::ProgramLinkedResources &resources,
              const gl::ProgramMergedVaryings &mergedVaryings,
              std::vector<std::shared_ptr<LinkSubTask>> *linkSubTasksOut,
              std::vector<std::shared_ptr<LinkSubTask>> *postLinkSubTasksOut) override
    {
        ASSERT(linkSubTasksOut && linkSubTasksOut->empty());
        ASSERT(postLinkSubTasksOut && postLinkSubTasksOut->empty());

        ProgramExecutableWgpu *executable =
            GetImplAs<ProgramExecutableWgpu>(&mProgram->getState().getExecutable());

        const gl::ShaderMap<gl::SharedCompiledShaderState> &shaders =
            mProgram->getState().getAttachedShaders();
        for (gl::ShaderType shaderType : gl::AllShaderTypes())
        {
            if (shaders[shaderType])
            {
                auto task = std::make_shared<CreateWGPUShaderModuleTask>(
                    mProcTable, mInstance, mDevice, mFeatures, shaders[shaderType],
                    *executable->getExecutable(), mergedVaryings,
                    executable->getShaderModule(shaderType));
                linkSubTasksOut->push_back(task);
            }
        }

        // The default uniform block's CPU buffer needs to be allocated and the layout calculated,
        // now that the list of uniforms is known.
        angle::Result initUniformBlocksResult = initDefaultUniformBlocks();
        if (IsError(initUniformBlocksResult))
        {
            mLinkResult = initUniformBlocksResult;
            return;
        }

        mLinkResult = angle::Result::Continue;
    }

    angle::Result getResult(const gl::Context *context, gl::InfoLog &infoLog) override
    {
        return mLinkResult;
    }

  private:
    angle::Result initDefaultUniformBlocks()
    {
        ProgramExecutableWgpu *executableWgpu = webgpu::GetImpl(mExecutable);

        // Process vertex and fragment uniforms into std140 packing.
        gl::ShaderMap<sh::BlockLayoutMap> layoutMap;
        gl::ShaderMap<size_t> requiredBufferSize;
        requiredBufferSize.fill(0);

        ANGLE_TRY(generateUniformLayoutMapping(&layoutMap, &requiredBufferSize));
        initDefaultUniformLayoutMapping(&layoutMap);

        // All uniform initializations are complete, now resize the buffers accordingly and return
        ANGLE_TRY(executableWgpu->resizeUniformBlockMemory(requiredBufferSize));

        executableWgpu->markDefaultUniformsDirty();

        return angle::Result::Continue;
    }

    angle::Result generateUniformLayoutMapping(gl::ShaderMap<sh::BlockLayoutMap> *layoutMapOut,
                                               gl::ShaderMap<size_t> *requiredBufferSizeOut)
    {
        for (const gl::ShaderType shaderType : mExecutable->getLinkedShaderStages())
        {
            const gl::SharedCompiledShaderState &shader =
                mProgram->getState().getAttachedShader(shaderType);

            if (shader)
            {
                const std::vector<sh::ShaderVariable> &uniforms = shader->uniforms;
                ANGLE_TRY(InitDefaultUniformBlock(uniforms, &(*layoutMapOut)[shaderType],
                                                  &(*requiredBufferSizeOut)[shaderType]));
            }
        }

        return angle::Result::Continue;
    }

    void initDefaultUniformLayoutMapping(gl::ShaderMap<sh::BlockLayoutMap> *layoutMapOut)
    {
        // Init the default block layout info.
        ProgramExecutableWgpu *executableWgpu = webgpu::GetImpl(mExecutable);
        const auto &uniforms                  = mExecutable->getUniforms();

        for (const gl::VariableLocation &location : mExecutable->getUniformLocations())
        {
            gl::ShaderMap<sh::BlockMemberInfo> layoutInfo;

            if (location.used() && !location.ignored)
            {
                const auto &uniform = uniforms[location.index];
                if (uniform.isInDefaultBlock() && !uniform.isSampler() && !uniform.isImage() &&
                    !uniform.isFragmentInOut())
                {
                    std::string uniformName = mExecutable->getUniformNameByIndex(location.index);
                    if (uniform.isArray())
                    {
                        // Gets the uniform name without the [0] at the end.
                        uniformName = gl::StripLastArrayIndex(uniformName);
                        ASSERT(uniformName.size() !=
                               mExecutable->getUniformNameByIndex(location.index).size());
                    }

                    bool found = false;

                    for (const gl::ShaderType shaderType : mExecutable->getLinkedShaderStages())
                    {
                        auto it = (*layoutMapOut)[shaderType].find(uniformName);
                        if (it != (*layoutMapOut)[shaderType].end())
                        {
                            found                  = true;
                            layoutInfo[shaderType] = it->second;
                        }
                    }

                    ASSERT(found);
                }
            }

            for (const gl::ShaderType shaderType : mExecutable->getLinkedShaderStages())
            {
                executableWgpu->getSharedDefaultUniformBlock(shaderType)
                    ->uniformLayout.push_back(layoutInfo[shaderType]);
            }
        }
    }

    const DawnProcTable *mProcTable = nullptr;
    webgpu::InstanceHandle mInstance;
    webgpu::DeviceHandle mDevice;
    const angle::FeaturesWgpu &mFeatures;
    ProgramWgpu *mProgram = nullptr;
    const gl::ProgramExecutable *mExecutable;
    angle::Result mLinkResult = angle::Result::Stop;
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
    *resultOut   = egl::CacheGetResult::Success;
    return angle::Result::Continue;
}

void ProgramWgpu::save(const gl::Context *context, gl::BinaryOutputStream *stream) {}

void ProgramWgpu::setBinaryRetrievableHint(bool retrievable) {}

void ProgramWgpu::setSeparable(bool separable) {}

angle::Result ProgramWgpu::link(const gl::Context *context, std::shared_ptr<LinkTask> *linkTaskOut)
{
    const DawnProcTable *wgpu       = webgpu::GetProcs(context);
    const angle::FeaturesWgpu &features = webgpu::GetFeatures(context);
    webgpu::DeviceHandle device     = webgpu::GetDevice(context);
    webgpu::InstanceHandle instance = webgpu::GetInstance(context);

    *linkTaskOut =
        std::shared_ptr<LinkTask>(new LinkTaskWgpu(wgpu, instance, device, features, this));
    return angle::Result::Continue;
}

GLboolean ProgramWgpu::validate(const gl::Caps &caps)
{
    return GL_TRUE;
}

}  // namespace rx
