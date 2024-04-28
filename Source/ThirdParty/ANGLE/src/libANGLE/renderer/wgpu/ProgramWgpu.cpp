//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ProgramWgpu.cpp:
//    Implements the class methods for ProgramWgpu.
//

#include "libANGLE/renderer/wgpu/ProgramWgpu.h"

#include "common/debug.h"
#include "libANGLE/renderer/wgpu/ProgramExecutableWgpu.h"
#include "libANGLE/renderer/wgpu/wgpu_utils.h"
#include "libANGLE/trace.h"

#include <dawn/webgpu_cpp.h>

namespace rx
{
namespace
{
class CreateWGPUShaderModuleTask : public LinkSubTask
{
  public:
    CreateWGPUShaderModuleTask(wgpu::Instance instance,
                               wgpu::Device device,
                               const gl::SharedCompiledShaderState &compiledShaderState,
                               TranslatedWGPUShaderModule &resultShaderModule)
        : mInstance(instance),
          mDevice(device),
          mCompiledShaderState(compiledShaderState),
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

        wgpu::ShaderModuleWGSLDescriptor shaderModuleWGSLDescriptor;
        shaderModuleWGSLDescriptor.code = mCompiledShaderState->translatedSource.c_str();

        wgpu::ShaderModuleDescriptor shaderModuleDescriptor;
        shaderModuleDescriptor.nextInChain = &shaderModuleWGSLDescriptor;

        mShaderModule.module = mDevice.CreateShaderModule(&shaderModuleDescriptor);

        wgpu::CompilationInfoCallbackInfo callbackInfo;
        callbackInfo.mode     = wgpu::CallbackMode::WaitAnyOnly;
        callbackInfo.callback = [](WGPUCompilationInfoRequestStatus status,
                                   struct WGPUCompilationInfo const *compilationInfo,
                                   void *userdata) {
            CreateWGPUShaderModuleTask *task = static_cast<CreateWGPUShaderModuleTask *>(userdata);

            if (status != WGPUCompilationInfoRequestStatus_Success)
            {
                task->mResult = angle::Result::Stop;
            }

            for (size_t msgIdx = 0; msgIdx < compilationInfo->messageCount; ++msgIdx)
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
                task->mLog << message.lineNum << ":" << message.linePos << ": " << message.message
                           << std::endl;
            }
        };
        callbackInfo.userdata = this;

        wgpu::FutureWaitInfo waitInfo;
        waitInfo.future = mShaderModule.module.GetCompilationInfo(callbackInfo);

        wgpu::WaitStatus waitStatus = mInstance.WaitAny(1, &waitInfo, -1);
        if (waitStatus != wgpu::WaitStatus::Success)
        {
            mResult = angle::Result::Stop;
        }
    }

  private:
    wgpu::Instance mInstance;
    wgpu::Device mDevice;
    gl::SharedCompiledShaderState mCompiledShaderState;

    TranslatedWGPUShaderModule &mShaderModule;

    std::ostringstream mLog;
    angle::Result mResult = angle::Result::Continue;
};

class LinkTaskWgpu : public LinkTask
{
  public:
    LinkTaskWgpu(wgpu::Instance instance, wgpu::Device device, ProgramWgpu *program)
        : mInstance(instance), mDevice(device), mProgram(program)
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
                    mInstance, mDevice, shaders[shaderType],
                    executable->getShaderModule(shaderType));
                linkSubTasksOut->push_back(task);
            }
        }
    }

    angle::Result getResult(const gl::Context *context, gl::InfoLog &infoLog) override
    {
        return angle::Result::Continue;
    }

  private:
    wgpu::Instance mInstance;
    wgpu::Device mDevice;
    ProgramWgpu *mProgram = nullptr;
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
    wgpu::Device device     = webgpu::GetDevice(context);
    wgpu::Instance instance = webgpu::GetInstance(context);

    *linkTaskOut = std::shared_ptr<LinkTask>(new LinkTaskWgpu(instance, device, this));
    return angle::Result::Continue;
}

GLboolean ProgramWgpu::validate(const gl::Caps &caps)
{
    return GL_TRUE;
}

}  // namespace rx
