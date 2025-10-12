//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FrameCaptureCL.cpp:
//   ANGLE CL Frame capture implementation.
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "libANGLE/capture/FrameCapture.h"

#include "common/angle_version_info.h"
#include "common/frame_capture_utils.h"
#include "common/serializer/JsonSerializer.h"
#include "common/string_utils.h"
#include "common/system_utils.h"
#include "libANGLE/CLBuffer.h"
#include "libANGLE/CLCommandQueue.h"
#include "libANGLE/CLContext.h"
#include "libANGLE/CLImage.h"
#include "libANGLE/CLProgram.h"
#include "libANGLE/capture/capture_cl_autogen.h"
#include "libANGLE/capture/serialize.h"
#include "libANGLE/cl_utils.h"
#include "libGLESv2/cl_stubs_autogen.h"

#if !ANGLE_CAPTURE_ENABLED
#    error Frame capture must be enabled to include this file.
#endif  // !ANGLE_CAPTURE_ENABLED

#ifndef ANGLE_ENABLE_CL
#    error OpenCL must be enabled to include this file.
#endif  // !ANGLE_ENABLE_CL

namespace angle
{

// Some replay functions can get quite large. If over a certain size, this method breaks up the
// function into parts to avoid overflowing the stack and causing slow compilation.
void WriteCppReplayFunctionWithPartsCL(ReplayFunc replayFunc,
                                       ReplayWriter &replayWriter,
                                       uint32_t frameIndex,
                                       FrameCaptureBinaryData *binaryData,
                                       const std::vector<CallCapture> &calls,
                                       std::stringstream &header,
                                       std::stringstream &out)
{
    out << "void "
        << FmtFunction(replayFunc, kNoContextId, FuncUsage::Definition, frameIndex, kNoPartId)
        << "\n"
        << "{\n";

    for (const CallCapture &call : calls)
    {
        // Process active calls for Setup and inactive calls for SetupInactive
        if ((call.isActive && replayFunc != ReplayFunc::SetupInactive) ||
            (!call.isActive && replayFunc == ReplayFunc::SetupInactive))
        {
            out << "    ";
            WriteCppReplayForCallCL(call, replayWriter, out, header, binaryData);
            out << ";\n";
        }
    }
    out << "}\n";
}

void WriteCppReplayForCallCL(const CallCapture &call,
                             ReplayWriter &replayWriter,
                             std::ostream &out,
                             std::ostream &header,
                             FrameCaptureBinaryData *binaryData)
{
    if (call.customFunctionName == "Comment")
    {
        // Just write it directly to the file and move on
        WriteComment(out, call);
        return;
    }

    std::ostringstream callOut;
    std::ostringstream postCallAdditions;

    const ParamCapture &returnValue = call.params.getReturnValue();
    switch (returnValue.type)
    {
        case ParamType::Tcl_context:
            callOut << "clContextsMap["
                    << cl::Platform::GetDefault()->getFrameCaptureShared()->getIndex(
                           &returnValue.value.cl_contextVal)
                    << "] = ";
            break;
        case ParamType::Tcl_command_queue:
            callOut << "clCommandQueuesMap["
                    << cl::Platform::GetDefault()->getFrameCaptureShared()->getIndex(
                           &returnValue.value.cl_command_queueVal)
                    << "] = ";
            break;
        case ParamType::Tcl_mem:
            callOut << "clMemMap["
                    << cl::Platform::GetDefault()->getFrameCaptureShared()->getIndex(
                           &returnValue.value.cl_memVal)
                    << "] = ";
            break;
        case ParamType::Tcl_sampler:
            callOut << "clSamplerMap["
                    << cl::Platform::GetDefault()->getFrameCaptureShared()->getIndex(
                           &returnValue.value.cl_samplerVal)
                    << "] = ";
            break;
        case ParamType::Tcl_program:
            callOut << "clProgramsMap["
                    << cl::Platform::GetDefault()->getFrameCaptureShared()->getIndex(
                           &returnValue.value.cl_programVal)
                    << "] = ";
            break;
        case ParamType::Tcl_kernel:
            callOut << "clKernelsMap["
                    << cl::Platform::GetDefault()->getFrameCaptureShared()->getIndex(
                           &returnValue.value.cl_kernelVal)
                    << "] = ";
            break;
        case ParamType::Tcl_event:
            callOut << "clEventsMap["
                    << cl::Platform::GetDefault()->getFrameCaptureShared()->getIndex(
                           &returnValue.value.cl_eventVal)
                    << "] = ";
            break;
        case ParamType::TvoidPointer:
            if (cl::Platform::GetDefault()->getFrameCaptureShared()->getCLVoidIndex(
                    returnValue.value.voidPointerVal) != SIZE_MAX)
            {
                callOut << "clVoidMap["
                        << cl::Platform::GetDefault()->getFrameCaptureShared()->getCLVoidIndex(
                               returnValue.value.voidPointerVal)
                        << "] = ";
            }
            break;
        default:
            break;
    }

    callOut << call.name() << "(";

    bool first = true;
    for (const ParamCapture &param : call.params.getParamCaptures())
    {
        if (!first)
        {
            callOut << ", ";
        }

        if (param.arrayClientPointerIndex != -1 && param.value.voidConstPointerVal != nullptr)
        {
            callOut << "gClientArrays[" << param.arrayClientPointerIndex << "]";
        }
        else if (param.readBufferSizeBytes > 0)
        {
            callOut << "(" << ParamTypeToString(param.type) << ")gReadBuffer";
        }
        else if (param.data.empty())
        {
            if (param.type == ParamType::Tcl_platform_idPointer &&
                param.value.cl_platform_idPointerVal)
            {
                callOut << "clPlatformsMap";
            }
            else if (param.type == ParamType::Tcl_platform_id)
            {
                callOut << "clPlatformsMap["
                        << cl::Platform::GetDefault()->getFrameCaptureShared()->getIndex(
                               &param.value.cl_platform_idVal)
                        << "]";
            }
            else if (param.type == ParamType::Tcl_device_idPointer &&
                     param.value.cl_device_idPointerVal)
            {
                std::vector<size_t> tempDeviceIndices =
                    cl::Platform::GetDefault()->getFrameCaptureShared()->getCLObjVector(&param);

                cl_uint numDevices = call.params.getParamCaptures()[2].value.cl_uintVal;
                out << "temporaryDevicesList.clear();\n    temporaryDevicesList.resize("
                    << numDevices << ");\n    ";
                callOut << "temporaryDevicesList.data()";
                for (cl_uint i = 0; i < numDevices; ++i)
                {
                    postCallAdditions << ";\n    clDevicesMap[" << tempDeviceIndices[i]
                                      << "] = temporaryDevicesList[" << i << "]";
                }
            }
            else if (param.type == ParamType::Tcl_device_id)
            {
                callOut << "clDevicesMap["
                        << cl::Platform::GetDefault()->getFrameCaptureShared()->getIndex(
                               &param.value.cl_device_idVal)
                        << "]";
            }
            else if (param.type == ParamType::Tcl_context)
            {
                callOut << "clContextsMap["
                        << cl::Platform::GetDefault()->getFrameCaptureShared()->getIndex(
                               &param.value.cl_contextVal)
                        << "]";
            }
            else if (param.type == ParamType::Tcl_command_queue)
            {
                callOut << "clCommandQueuesMap["
                        << cl::Platform::GetDefault()->getFrameCaptureShared()->getIndex(
                               &param.value.cl_command_queueVal)
                        << "]";
            }
            else if (param.type == ParamType::Tcl_mem)
            {
                callOut << "clMemMap["
                        << cl::Platform::GetDefault()->getFrameCaptureShared()->getIndex(
                               &param.value.cl_memVal)
                        << "]";
            }
            else if (param.type == ParamType::Tcl_sampler)
            {
                callOut << "clSamplerMap["
                        << cl::Platform::GetDefault()->getFrameCaptureShared()->getIndex(
                               &param.value.cl_samplerVal)
                        << "]";
            }
            else if (param.type == ParamType::Tcl_program)
            {
                callOut << "clProgramsMap["
                        << cl::Platform::GetDefault()->getFrameCaptureShared()->getIndex(
                               &param.value.cl_programVal)
                        << "]";
            }
            else if (param.type == ParamType::Tcl_kernel)
            {
                callOut << "clKernelsMap["
                        << cl::Platform::GetDefault()->getFrameCaptureShared()->getIndex(
                               &param.value.cl_kernelVal)
                        << "]";
            }
            else if (param.type == ParamType::Tcl_event)
            {
                callOut << "clEventsMap["
                        << cl::Platform::GetDefault()->getFrameCaptureShared()->getIndex(
                               &param.value.cl_eventVal)
                        << "]";
            }
            else if (param.type == ParamType::Tcl_eventPointer)
            {
                if (param.value.cl_eventPointerVal)
                {
                    callOut << "&clEventsMap["
                            << cl::Platform::GetDefault()->getFrameCaptureShared()->getIndex(
                                   &param.value.cl_eventVal)
                            << "]";
                }
                else
                {
                    callOut << "NULL";
                }
            }
            else if (param.type == ParamType::TvoidConstPointer)
            {
                if (cl::Platform::GetDefault()->getFrameCaptureShared()->getIndex(
                        &param.value.cl_memVal) != SIZE_MAX)
                {
                    callOut << "(const void *)" << "&clMemMap["
                            << cl::Platform::GetDefault()->getFrameCaptureShared()->getIndex(
                                   &param.value.cl_memVal)
                            << "]";
                }
                else if (cl::Platform::GetDefault()->getFrameCaptureShared()->getIndex(
                             &param.value.cl_samplerVal) != SIZE_MAX)
                {
                    callOut << "(const void *)" << "&clSamplerMap["
                            << cl::Platform::GetDefault()->getFrameCaptureShared()->getIndex(
                                   &param.value.cl_samplerVal)
                            << "]";
                }
                else if (cl::Platform::GetDefault()->getFrameCaptureShared()->getIndex(
                             &param.value.cl_command_queueVal) != SIZE_MAX)
                {
                    callOut << "(const void *)" << "&clCommandQueuesMap["
                            << cl::Platform::GetDefault()->getFrameCaptureShared()->getIndex(
                                   &param.value.cl_command_queueVal)
                            << "]";
                }
                else if (cl::Platform::GetDefault()->getFrameCaptureShared()->getCLVoidIndex(
                             param.value.voidConstPointerVal) != SIZE_MAX)
                {
                    callOut << "clVoidMap["
                            << cl::Platform::GetDefault()->getFrameCaptureShared()->getCLVoidIndex(
                                   param.value.voidConstPointerVal)
                            << "]";
                }
                else
                {
                    WriteParamCaptureReplay(callOut, call, param);
                }
            }
            else if (param.type == ParamType::TvoidPointer)
            {
                if (cl::Platform::GetDefault()->getFrameCaptureShared()->getCLVoidIndex(
                        param.value.voidPointerVal) != SIZE_MAX)
                {
                    callOut << "clVoidMap["
                            << cl::Platform::GetDefault()->getFrameCaptureShared()->getCLVoidIndex(
                                   param.value.voidPointerVal)
                            << "]";
                }
                else
                {
                    WriteParamCaptureReplay(callOut, call, param);
                }
            }
            else if (param.type == ParamType::Tcl_mem_destructor_func_type ||
                     param.type == ParamType::Tcl_callback_func_type ||
                     param.type == ParamType::Tcl_svm_free_callback_func_type ||
                     param.type == ParamType::Tcl_program_func_type ||
                     param.type == ParamType::Tcl_context_destructor_func_type ||
                     param.type == ParamType::Tcl_context_func_type ||
                     param.type == ParamType::Tcl_void_func_type)
            {
                callOut << "NULL";
            }
            else if (param.type == ParamType::Tcl_memConstPointer && cl::Platform::GetDefault()
                                                                         ->getFrameCaptureShared()
                                                                         ->getCLObjVector(&param)
                                                                         .size())
            {
                std::vector<size_t> tempBufferIndices =
                    cl::Platform::GetDefault()->getFrameCaptureShared()->getCLObjVector(&param);
                out << "temporaryBuffersList = {";
                for (uint32_t i = 0; i < tempBufferIndices.size(); ++i)
                {
                    out << (i != 0 ? ", " : "") << "clMemMap[" << tempBufferIndices.at(i) << "]";
                }
                out << "};\n    ";
                callOut << "temporaryBuffersList.data()";
            }
            else if (param.type == ParamType::Tcl_eventConstPointer)
            {
                std::vector<size_t> tempEventIndices =
                    cl::Platform::GetDefault()->getFrameCaptureShared()->getCLObjVector(&param);
                if (tempEventIndices.empty())
                {
                    callOut << "NULL";
                }
                else
                {
                    out << "temporaryEventsList = {";
                    for (uint32_t i = 0; i < tempEventIndices.size(); ++i)
                    {
                        out << (i != 0 ? ", " : "") << "clEventsMap[" << tempEventIndices.at(i)
                            << "]";
                    }
                    out << "};\n    ";
                    callOut << "temporaryEventsList.data()";
                }
            }
            else if (param.type == ParamType::Tcl_device_idConstPointer)
            {
                std::vector<size_t> tempDeviceIndices =
                    cl::Platform::GetDefault()->getFrameCaptureShared()->getCLObjVector(&param);
                if (tempDeviceIndices.empty())
                {
                    callOut << "NULL";
                }
                else
                {
                    out << "temporaryDevicesList = {";
                    for (uint32_t i = 0; i < tempDeviceIndices.size(); ++i)
                    {
                        if (i != 0)
                        {
                            out << ", ";
                        }
                        out << "clDevicesMap[" << tempDeviceIndices.at(i) << "]";
                    }
                    out << "};\n    ";
                    callOut << "temporaryDevicesList.data()";
                }
            }
            else if (param.type == ParamType::Tcl_kernelPointer)
            {
                std::vector<size_t> tempKernelIndices =
                    cl::Platform::GetDefault()->getFrameCaptureShared()->getCLObjVector(&param);
                cl_uint numKernels = call.params.getParamCaptures()[1].value.cl_uintVal;
                out << "temporaryKernelsList.clear();\ntemporaryKernelsList.resize(" << numKernels
                    << ");\n    ";
                callOut << "temporaryKernelsList.data()";
                for (cl_uint i = 0; i < numKernels; ++i)
                {
                    postCallAdditions << ";\n    clKernelsMap[" << tempKernelIndices[i]
                                      << "] = temporaryKernelsList[" << i << "]";
                }
            }
            else if (param.type == ParamType::TvoidConstPointerPointer &&
                     cl::Platform::GetDefault()
                         ->getFrameCaptureShared()
                         ->getCLObjVector(&param)
                         .size())
            {
                std::vector<size_t> offsets =
                    cl::Platform::GetDefault()->getFrameCaptureShared()->getCLObjVector(&param);
                out << "temporaryVoidPtrList = {";
                for (size_t i = 0; i < offsets.size(); ++i)
                {
                    out << (i != 0 ? ", " : "") << "&((char*)temporaryVoidPtr)[" << offsets.at(i)
                        << "]";
                }
                out << "};\n    ";
                callOut << "temporaryVoidPtrList.data()";
            }
            else if (param.type == ParamType::TvoidPointerPointer ||
                     param.type == ParamType::TvoidConstPointerPointer)
            {
                std::vector<size_t> tempVoidIndices =
                    cl::Platform::GetDefault()->getFrameCaptureShared()->getCLObjVector(&param);
                out << "temporaryVoidPtrList = {";
                for (uint32_t i = 0; i < tempVoidIndices.size(); ++i)
                {
                    out << (i != 0 ? ", " : "") << "clVoidMap[" << tempVoidIndices.at(i) << "]";
                }
                out << "};\n    ";
                callOut << "temporaryVoidPtrList.data()";
            }
            else if (param.type == ParamType::Tcl_programConstPointer && param.value.size_tVal)
            {
                std::vector<size_t> tempProgramIndices =
                    cl::Platform::GetDefault()->getFrameCaptureShared()->getCLObjVector(&param);
                out << "temporaryProgramsList = {";
                for (uint32_t i = 0; i < tempProgramIndices.size(); ++i)
                {
                    out << (i != 0 ? ", " : "") << "clProgramsMap[" << tempProgramIndices.at(i)
                        << "]";
                }
                out << "};\n    ";
                callOut << "temporaryProgramsList.data()";
            }
            else if (param.type == ParamType::Tcl_context_propertiesConstPointer)
            {
                if (param.value.cl_context_propertiesConstPointerVal)
                {
                    callOut << "temporaryContextProps.data()";
                }
                else
                {
                    WriteParamCaptureReplay(callOut, call, param);
                }
            }
            else
            {
                WriteParamCaptureReplay(callOut, call, param);
            }
        }
        else
        {
            switch (param.type)
            {
                case ParamType::TcharConstPointerPointer:
                    WriteStringPointerParamReplay(replayWriter, callOut, header, call, param);
                    break;
                case ParamType::Tcl_device_idPointer:
                    callOut << "clDevicesMap";
                    break;
                case ParamType::TcharUnsignedConstPointerPointer:
                {
                    std::string tempStructureName = "temporaryCharPointerList";
                    std::string tempStructureType = "(const char *)";
                    if (param.type == ParamType::TcharUnsignedConstPointerPointer)
                    {
                        tempStructureName = "temporaryUnsignedCharPointerList";
                        tempStructureType = "(const unsigned char *)";
                    }
                    const std::vector<uint8_t> *data;
                    out << tempStructureName << " = {";
                    for (size_t i = 0; i < param.data.size(); ++i)
                    {
                        if (i != 0)
                        {
                            out << ", ";
                        }
                        data          = &param.data[i];
                        const size_t offset = binaryData->append(data->data(), data->size());
                        out << tempStructureType << "GetBinaryData[" << offset << "]";
                    }
                    out << "};\n    ";
                    callOut << tempStructureName << ".data()";
                    break;
                }
                case ParamType::Tcl_image_descConstPointer:
                    cl_image_desc tempImageDesc;
                    std::memcpy(&tempImageDesc, param.data[0].data(), sizeof(cl_image_desc));
                    if (tempImageDesc.mem_object)
                    {
                        out << "    std::memcpy(&temporaryImageDesc, ";
                        WriteBinaryParamReplay(replayWriter, out, header, call, param, binaryData);
                        out << ", sizeof(cl_image_desc));\ntemporaryImageDesc.mem_object = "
                               "clMemMap["
                            << cl::Platform::GetDefault()->getFrameCaptureShared()->getIndex(
                                   &tempImageDesc.mem_object)
                            << "];\n    ";
                        callOut << "&temporaryImageDesc";
                    }
                    else
                    {
                        WriteBinaryParamReplay(replayWriter, callOut, header, call, param,
                                               binaryData);
                    }
                    break;
                case ParamType::TvoidPointer:
                {
                    // For clEnqueueNativeKernel
                    if (call.entryPoint == EntryPoint::CLEnqueueNativeKernel)
                    {
                        std::vector<size_t> bufferIndices =
                            cl::Platform::GetDefault()->getFrameCaptureShared()->getCLObjVector(
                                &param);
                        size_t totalSize = call.params.getParamCaptures()[3].value.size_tVal;
                        out << "temporaryVoidPtr = (void *)std::malloc(" << totalSize
                            << ");\nstd::memcpy(&temporaryVoidPtr, ";
                        WriteBinaryParamReplay(replayWriter, out, header, call, param, binaryData);
                        out << ", " << totalSize << ");\n    ";
                        callOut << "temporaryVoidPtr";
                    }
                    else
                    {
                        WriteBinaryParamReplay(replayWriter, callOut, header, call, param,
                                               binaryData);
                    }
                    break;
                }
                default:
                    WriteBinaryParamReplay(replayWriter, callOut, header, call, param, binaryData);
                    break;
            }
        }

        first = false;
    }

    callOut << ")";

    out << callOut.str() << postCallAdditions.str();
}

void WriteInitReplayCallCL(bool compression,
                           std::ostream &out,
                           const std::string &captureLabel,
                           size_t maxClientArraySize,
                           size_t readBufferSize,
                           const std::map<ParamType, uint32_t> &maxCLParamsSize)
{
    std::string binaryDataFileName = GetBinaryDataFilePath(compression, captureLabel);
    out << "    // binaryDataFileName = " << binaryDataFileName << "\n";
    out << "    // maxClientArraySize = " << maxClientArraySize << "\n";
    out << "    // readBufferSize = " << readBufferSize << "\n";
    out << "    // clPlatformsMapSize = " << maxCLParamsSize.at(ParamType::Tcl_platform_idPointer)
        << "\n";
    out << "    // clDevicesMapSize = " << maxCLParamsSize.at(ParamType::Tcl_device_idPointer)
        << "\n";
    out << "    // clContextsMapSize = " << maxCLParamsSize.at(ParamType::Tcl_context) << "\n";
    out << "    // clCommandQueuesMapSize = " << maxCLParamsSize.at(ParamType::Tcl_command_queue)
        << "\n";
    out << "    // clMemMapSize = " << maxCLParamsSize.at(ParamType::Tcl_mem) << "\n";
    out << "    // clEventsMapSize = " << maxCLParamsSize.at(ParamType::Tcl_eventPointer) << "\n";
    out << "    // clProgramsMapSize = " << maxCLParamsSize.at(ParamType::Tcl_program) << "\n";
    out << "    // clKernelsMapSize = " << maxCLParamsSize.at(ParamType::Tcl_kernel) << "\n";
    out << "    // clSamplerMapSize = " << maxCLParamsSize.at(ParamType::Tcl_sampler) << "\n";
    out << "    // clVoidMapSize = " << maxCLParamsSize.at(ParamType::TvoidPointer) << "\n";
    out << "    InitializeReplayCL2(\"" << binaryDataFileName << "\", " << maxClientArraySize
        << ", " << readBufferSize << ", " << maxCLParamsSize.at(ParamType::Tcl_platform_idPointer)
        << ", " << maxCLParamsSize.at(ParamType::Tcl_device_idPointer) << ", "
        << maxCLParamsSize.at(ParamType::Tcl_context) << ", "
        << maxCLParamsSize.at(ParamType::Tcl_command_queue) << ", "
        << maxCLParamsSize.at(ParamType::Tcl_mem) << ", "
        << maxCLParamsSize.at(ParamType::Tcl_eventPointer) << ", "
        << maxCLParamsSize.at(ParamType::Tcl_program) << ", "
        << maxCLParamsSize.at(ParamType::Tcl_kernel) << ", "
        << maxCLParamsSize.at(ParamType::Tcl_sampler) << ", "
        << maxCLParamsSize.at(ParamType::TvoidPointer) << ");\n";

    // Load binary data
    out << "    InitializeBinaryDataLoader();\n";
}

void FrameCaptureShared::trackCLMemUpdate(const cl_mem *mem, bool referenced)
{
    std::vector<cl_mem> &mCLDirtyMem = mResourceTrackerCL.mCLDirtyMem;
    // retained or created cl mem object
    if (referenced)
    {
        // Potentially mark as dirty
        auto it = std::find(mCLDirtyMem.begin(), mCLDirtyMem.end(), *mem);
        if (it == mCLDirtyMem.end())
        {
            mCLDirtyMem.push_back(*mem);
        }
    }
    else
    {
        std::unordered_map<cl_mem, cl_mem> &mCLSubBufferToParent =
            mResourceTrackerCL.mCLSubBufferToParent;
        if ((*mem)->cast<cl::Memory>().getRefCount() == 1)
        {
            auto it = std::find(mCLDirtyMem.begin(), mCLDirtyMem.end(), *mem);
            if (it != mCLDirtyMem.end())
            {
                mCLDirtyMem.erase(it);
            }

            if (removeUnneededOpenCLCalls)
            {
                removeCLMemOccurrences(mem, &mFrameCalls);
            }
            mCLSubBufferToParent.erase(*mem);
            (*mem)->cast<cl::Memory>().release();
        }
        if (mCLSubBufferToParent.find(*mem) != mCLSubBufferToParent.end())
        {
            trackCLMemUpdate(&mCLSubBufferToParent[*mem], false);
        }
    }
}

void FrameCaptureShared::trackCLProgramUpdate(const cl_program *program,
                                              bool referenced,
                                              cl_uint numLinkedPrograms,
                                              const cl_program *linkedPrograms)
{
    std::unordered_map<cl_program, cl_uint> &mCLProgramLinkCounter =
        mResourceTrackerCL.mCLProgramLinkCounter;
    std::unordered_map<cl_program, std::vector<cl_program>> &mCLLinkedPrograms =
        mResourceTrackerCL.mCLLinkedPrograms;
    // retained or created cl program object
    if (referenced)
    {
        // Increment link count for this program
        if (mCLProgramLinkCounter.find(*program) == mCLProgramLinkCounter.end())
        {
            mCLProgramLinkCounter[*program] = 0;
        }
        ++mCLProgramLinkCounter[*program];

        // Setup the linked programs if this call is from capturing clCompileProgram or
        // clLinkProgram
        if (numLinkedPrograms)
        {
            mCLLinkedPrograms[*program] = std::vector<cl_program>();
            for (cl_uint i = 0; i < numLinkedPrograms; ++i)
            {
                mCLLinkedPrograms[*program].push_back(linkedPrograms[i]);
            }
        }

        // Go through the linked programs and increment their link counts
        for (size_t i = 0; mCLLinkedPrograms.find(*program) != mCLLinkedPrograms.end() &&
                           i < mCLLinkedPrograms[*program].size();
             ++i)
        {
            trackCLProgramUpdate(&mCLLinkedPrograms[*program].at(i), true, 0, nullptr);
        }
    }
    else
    {
        // Decrement link count for this program and the linked programs
        --mCLProgramLinkCounter[*program];
        for (size_t i = 0; mCLLinkedPrograms.find(*program) != mCLLinkedPrograms.end() &&
                           i < mCLLinkedPrograms[*program].size();
             ++i)
        {
            trackCLProgramUpdate(&mCLLinkedPrograms[*program].at(i), false, 0, nullptr);
        }

        // Remove the calls containing this object if the link count is 0
        if (mCLProgramLinkCounter[*program] == 0)
        {
            mCLProgramLinkCounter.erase(*program);
            if (mCLLinkedPrograms.find(*program) != mCLLinkedPrograms.end())
            {
                mCLLinkedPrograms.erase(*program);
            }

            if (removeUnneededOpenCLCalls)
            {
                removeCLProgramOccurrences(program, &mFrameCalls);
            }
        }
    }
}

void FrameCaptureShared::injectMemcpy(void *src,
                                      void *dest,
                                      size_t size,
                                      std::vector<CallCapture> *calls)
{
    // Inject memcpy call before unmap

    // Create param buffer
    ParamBuffer paramBuffer;

    // Create dest parameter
    ParamCapture destParam("dest", ParamType::TvoidConstPointer);
    InitParamValue(ParamType::TvoidPointer, dest, &destParam.value);
    paramBuffer.addParam(std::move(destParam));

    // Create src param
    ParamCapture updateMemory("src", ParamType::TvoidConstPointer);
    CaptureMemory(src, size, &updateMemory);
    paramBuffer.addParam(std::move(updateMemory));

    paramBuffer.addValueParam<size_t>("size", ParamType::Tsize_t, size);

    calls->emplace(calls->end() - 1, "std::memcpy", std::move(paramBuffer));
}

void FrameCaptureShared::captureUpdateCLObjs(std::vector<CallCapture> *calls)
{
    std::vector<cl_mem> &mCLDirtyMem         = mResourceTrackerCL.mCLDirtyMem;
    std::vector<void *> &mCLDirtySVM         = mResourceTrackerCL.mCLDirtySVM;
    cl_command_queue &mCLCurrentCommandQueue = mResourceTrackerCL.mCLCurrentCommandQueue;
    for (uint32_t i = 0; i < mCLDirtyMem.size(); ++i)
    {
        cl_mem_object_type memType;
        if (IsError(mCLDirtyMem.at(i)->cast<cl::Memory>().getInfo(
                cl::MemInfo::Type, sizeof(cl_mem_object_type), &memType, nullptr)))
        {
            continue;
        }
        if (memType == CL_MEM_OBJECT_BUFFER)
        {
            void *ptr;

            if (calls->back().entryPoint == EntryPoint::CLEnqueueUnmapMemObject)
            {
                CallCapture *mapCall = &mResourceTrackerCL.mCLMapCall.at(
                    calls->back()
                        .params.getParam("mapped_ptr", ParamType::TvoidPointer, 2)
                        .value.voidPointerVal);
                size_t offset =
                    mapCall->params.getParam("offset", ParamType::Tsize_t, 4).value.size_tVal;
                size_t size =
                    mapCall->params.getParam("size", ParamType::Tsize_t, 5).value.size_tVal;
                ptr = malloc(size);

                // Call clEnqueueReadBuffer to get the current data in the buffer
                EnqueueReadBuffer(mCLCurrentCommandQueue, mCLDirtyMem.at(i), true, offset, size,
                                  ptr, 0, nullptr, nullptr);

                // Inject memcpy call BEFORE unmap
                injectMemcpy(ptr,
                             calls->back()
                                 .params.getParam("mapped_ptr", ParamType::TvoidPointer, 2)
                                 .value.voidPointerVal,
                             size, calls);
            }
            else
            {
                size_t bufferSize = mCLDirtyMem.at(i)->cast<cl::Buffer>().getSize();
                ptr               = malloc(bufferSize);

                // Call clEnqueueReadBuffer to get the current data in the buffer
                EnqueueReadBuffer(mCLCurrentCommandQueue, mCLDirtyMem.at(i), true, 0, bufferSize,
                                  ptr, 0, nullptr, nullptr);

                // Pretend that a "clEnqueueWriteBuffer" was called with the above data retrieved
                calls->push_back(CaptureEnqueueWriteBuffer(true, mCLCurrentCommandQueue,
                                                           mCLDirtyMem.at(i), true, 0, bufferSize,
                                                           ptr, 0, nullptr, nullptr, CL_SUCCESS));

                // Implicit release, so going into the starting frame the buffer has the correct
                // reference count
                mCLDirtyMem.at(i)->cast<cl::Memory>().release();
            }
            free(ptr);
        }
        else if (memType == CL_MEM_OBJECT_PIPE)
        {
            UNIMPLEMENTED();
        }
        else
        {
            cl::Image *clImg = &mCLDirtyMem.at(i)->cast<cl::Image>();
            void *ptr;

            if (calls->back().entryPoint == EntryPoint::CLEnqueueUnmapMemObject)
            {
                CallCapture *mapCall = &mResourceTrackerCL.mCLMapCall.at(
                    calls->back()
                        .params.getParam("mapped_ptr", ParamType::TvoidPointer, 2)
                        .value.voidPointerVal);
                const size_t *origin = (const size_t *)mapCall->params
                                           .getParam("origin", ParamType::Tsize_tConstPointer, 4)
                                           .data.back()
                                           .data();
                const size_t *region = (const size_t *)mapCall->params
                                           .getParam("region", ParamType::Tsize_tConstPointer, 5)
                                           .data.back()
                                           .data();

                size_t rowPitch = mapCall->params.getParam("image_row_pitch", ParamType::Tsize_t, 6)
                                      .value.size_tVal;
                size_t slicePitch =
                    mapCall->params.getParam("image_slice_pitch", ParamType::Tsize_t, 7)
                        .value.size_tVal;

                // Get the image size to allocate the size of ptr
                size_t totalSize = (region[2] - 1) * slicePitch + (region[1] - 1) * rowPitch +
                                   region[0] * clImg->getElementSize();
                ptr = malloc(totalSize);

                // Call clEnqueueReadBuffer to get the current data in the image
                EnqueueReadImage(mCLCurrentCommandQueue, mCLDirtyMem.at(i), true, origin, region,
                                 rowPitch, slicePitch, ptr, 0, nullptr, nullptr);

                // Inject memcpy call BEFORE unmap
                injectMemcpy(ptr,
                             calls->back()
                                 .params.getParam("mapped_ptr", ParamType::TvoidPointer, 2)
                                 .value.voidPointerVal,
                             totalSize, calls);
            }
            else
            {
                ptr              = malloc(clImg->getSize());
                size_t origin[3] = {0, 0, 0};
                size_t region[3] = {clImg->getWidth(), clImg->getHeight(), clImg->getDepth()};

                // Call clEnqueueReadBuffer to get the current data in the image
                EnqueueReadImage(mCLCurrentCommandQueue, mCLDirtyMem.at(i), true, origin, region,
                                 clImg->getRowSize(), clImg->getSliceSize(), ptr, 0, nullptr,
                                 nullptr);

                // Pretend that a "clEnqueueWriteImage" was called with the above data retrieved
                calls->push_back(CaptureEnqueueWriteImage(
                    true, mCLCurrentCommandQueue, mCLDirtyMem.at(i), true, origin, region,
                    clImg->getRowSize(), clImg->getSliceSize(), ptr, 0, nullptr, nullptr,
                    CL_SUCCESS));

                // Implicit release, so going into the starting frame the buffer has the correct
                // reference count
                mCLDirtyMem.at(i)->cast<cl::Memory>().release();
            }

            free(ptr);
        }
    }
    for (uint32_t i = 0; i < mCLDirtySVM.size(); ++i)
    {
        size_t SVMSize = mResourceTrackerCL.SVMToSize[mCLDirtySVM.at(i)];

        // Call clEnqueueSVMMap to get the current data in the SVM pointer
        cl::MemFlags flags;
        flags.set(CL_MAP_READ);
        EnqueueSVMMap(mCLCurrentCommandQueue, true, flags, mCLDirtySVM.at(i), SVMSize, 0, nullptr,
                      nullptr);

        // Pretend that a "clEnqueueSVMMemcpy" was called with the above data retrieved
        calls->push_back(CaptureEnqueueSVMMemcpy(true, mCLCurrentCommandQueue, true,
                                                 mCLDirtySVM.at(i), mCLDirtySVM.at(i), SVMSize, 0,
                                                 nullptr, nullptr, CL_SUCCESS));

        // Call clEnqueueSVMUnmap to get the current data in the SVM pointer
        EnqueueSVMUnmap(mCLCurrentCommandQueue, mResourceTrackerCL.mCLDirtySVM.at(i), 0, nullptr,
                        nullptr);
    }
    mCLDirtyMem.clear();
    mCLDirtySVM.clear();
}

void FrameCaptureShared::removeCLMemOccurrences(const cl_mem *mem, std::vector<CallCapture> *calls)
{
    // This function gets called when it captures a clReleaseMemObj prior to the starting frame
    // that sets the reference count to 0, meaning that this cl_mem object isn't necessary for
    // the wanted frames. So, we can remove the calls that use it.

    for (size_t i = 0; i < calls->size(); ++i)
    {
        CallCapture *call = &calls->at(i);
        cl_mem foundMem;
        switch (call->entryPoint)
        {
            case EntryPoint::CLCreateBuffer:
            case EntryPoint::CLCreateBufferWithProperties:
            case EntryPoint::CLCreateImage:
            case EntryPoint::CLCreateImageWithProperties:
            case EntryPoint::CLCreateImage2D:
            case EntryPoint::CLCreateImage3D:
            case EntryPoint::CLCreatePipe:
            {
                foundMem = call->params.getReturnValue().value.cl_memVal;
                break;
            }
            case EntryPoint::CLCreateSubBuffer:
            {
                foundMem = call->params.getReturnValue().value.cl_memVal;
                if (foundMem != *mem)
                {
                    foundMem =
                        call->params.getParam("buffer", ParamType::Tcl_mem, 0).value.cl_memVal;
                }
                break;
            }
            case EntryPoint::CLEnqueueReadBuffer:
            case EntryPoint::CLEnqueueWriteBuffer:
            case EntryPoint::CLEnqueueReadBufferRect:
            case EntryPoint::CLEnqueueWriteBufferRect:
            case EntryPoint::CLEnqueueMapBuffer:
            {
                // Can get rid of these calls because the buffer is no longer needed
                foundMem = call->params.getParam("buffer", ParamType::Tcl_mem, 1).value.cl_memVal;
                break;
            }
            case EntryPoint::CLEnqueueReadImage:
            case EntryPoint::CLEnqueueWriteImage:
            case EntryPoint::CLEnqueueMapImage:
            {
                // Can get rid of these calls because the image is no longer needed
                foundMem = call->params.getParam("image", ParamType::Tcl_mem, 1).value.cl_memVal;
                break;
            }
            case EntryPoint::CLEnqueueCopyBuffer:
            case EntryPoint::CLEnqueueCopyBufferRect:
            case EntryPoint::CLEnqueueCopyImage:
            case EntryPoint::CLEnqueueCopyBufferToImage:
            case EntryPoint::CLEnqueueCopyImageToBuffer:
            {
                // Can get rid of these calls because the obj is no longer needed
                std::string srcType = "src_";
                srcType += ((call->entryPoint == EntryPoint::CLEnqueueCopyImage ||
                             call->entryPoint == EntryPoint::CLEnqueueCopyImageToBuffer)
                                ? "image"
                                : "buffer");
                std::string dstType = "dst_";
                dstType += ((call->entryPoint == EntryPoint::CLEnqueueCopyImage ||
                             call->entryPoint == EntryPoint::CLEnqueueCopyBufferToImage)
                                ? "image"
                                : "buffer");
                foundMem =
                    call->params.getParam(srcType.c_str(), ParamType::Tcl_mem, 1).value.cl_memVal;
                if (foundMem != *mem)
                {
                    foundMem = call->params.getParam(dstType.c_str(), ParamType::Tcl_mem, 2)
                                   .value.cl_memVal;
                }
                break;
            }
            case EntryPoint::CLReleaseMemObject:
            case EntryPoint::CLRetainMemObject:
            case EntryPoint::CLGetMemObjectInfo:
            case EntryPoint::CLSetMemObjectDestructorCallback:
            case EntryPoint::CLEnqueueUnmapMemObject:
            {
                foundMem =
                    call->params
                        .getParam("memobj", ParamType::Tcl_mem,
                                  call->entryPoint == EntryPoint::CLEnqueueUnmapMemObject ? 1 : 0)
                        .value.cl_memVal;
                break;
            }
            case EntryPoint::CLGetImageInfo:
            {
                foundMem = call->params.getParam("image", ParamType::Tcl_mem, 0).value.cl_memVal;
                break;
            }
            case EntryPoint::CLSetKernelArg:
            {
                foundMem = call->params.getParam("arg_value", ParamType::TvoidConstPointer, 3)
                               .value.cl_memVal;
                if (cl::Platform::GetDefault()->getFrameCaptureShared()->getIndex(&foundMem) ==
                    SIZE_MAX)
                {
                    continue;
                }
                break;
            }
            // Leave commented until external memory is upstream
            // case EntryPoint::CLEnqueueAcquireExternalMemObjectsKHR:
            // case EntryPoint::CLEnqueueReleaseExternalMemObjectsKHR:
            case EntryPoint::CLEnqueueMigrateMemObjects:
            {
                const cl_mem *memObjs =
                    call->params.getParam("mem_objects", ParamType::Tcl_memConstPointer, 2)
                        .value.cl_memConstPointerVal;
                cl_uint numMemObjs =
                    call->params.getParam("num_mem_objects", ParamType::Tcl_uint, 1)
                        .value.cl_uintVal;

                std::vector<cl_mem> newMemObjs;
                for (cl_uint memObjIndex = 0; i < numMemObjs; ++i)
                {
                    if (memObjs[memObjIndex] != *mem)
                    {
                        newMemObjs.push_back(memObjs[memObjIndex]);
                    }
                }

                // If all the mem objects used in this array are released, I can remove this call
                if (newMemObjs.empty())
                {
                    foundMem = *mem;
                }
                else
                {
                    call->params.setValueParamAtIndex("num_mem_objects", ParamType::Tcl_uint,
                                                      newMemObjs.size(), 1);
                    setCLObjVectorMap(
                        newMemObjs.data(), newMemObjs.size(),
                        &call->params.getParam("mem_objects", ParamType::Tcl_memConstPointer, 2),
                        &angle::FrameCaptureShared::getIndex);
                    continue;
                }
                break;
            }
            default:
                continue;
        }

        if (foundMem == *mem)
        {
            removeCLCall(calls, i);
            --i;
        }
    }
}

void FrameCaptureShared::removeCLKernelOccurrences(const cl_kernel *kernel,
                                                   std::vector<CallCapture> *calls)
{
    // This function gets called when it captures a clReleaseProgram prior to the starting frame
    // that sets the program's reference count to 0. This ensures that the kernels in that program
    // are/should be released as well, meaning that this cl_kernel object isn't necessary for
    // the wanted frames. So, we can remove the calls that use it.
    // We cannot remove cl_kernel occurrences at the time of clReleaseKernel because the kernel may
    // be an input to clCloneKernel and clCreateKernelsInProgram.

    for (size_t i = 0; i < calls->size(); ++i)
    {
        CallCapture *call = &calls->at(i);
        cl_kernel foundKernel;
        switch (call->entryPoint)
        {
            case EntryPoint::CLCreateKernel:
            {
                foundKernel = call->params.getReturnValue().value.cl_kernelVal;
                break;
            }
            case EntryPoint::CLCloneKernel:
            {
                foundKernel = call->params.getReturnValue().value.cl_kernelVal;
                if (foundKernel != *kernel)
                {
                    foundKernel = call->params.getParam("source_kernel", ParamType::Tcl_kernel, 0)
                                      .value.cl_kernelVal;
                }
                break;
            }
            case EntryPoint::CLRetainKernel:
            case EntryPoint::CLReleaseKernel:
            case EntryPoint::CLSetKernelArg:
            case EntryPoint::CLSetKernelArgSVMPointer:
            case EntryPoint::CLSetKernelExecInfo:
            case EntryPoint::CLGetKernelInfo:
            case EntryPoint::CLGetKernelArgInfo:
            case EntryPoint::CLGetKernelWorkGroupInfo:
            case EntryPoint::CLGetKernelSubGroupInfo:
            {
                foundKernel =
                    call->params.getParam("kernel", ParamType::Tcl_kernel, 0).value.cl_kernelVal;
                break;
            }
            case EntryPoint::CLEnqueueNDRangeKernel:
            case EntryPoint::CLEnqueueTask:
            {
                foundKernel =
                    call->params.getParam("kernel", ParamType::Tcl_kernel, 1).value.cl_kernelVal;
                break;
            }

            default:
                continue;
        }

        if (foundKernel == *kernel)
        {
            removeCLCall(calls, i);
            --i;
        }
    }
}

void FrameCaptureShared::removeCLProgramOccurrences(const cl_program *program,
                                                    std::vector<CallCapture> *calls)
{
    // This function gets called when it captures a clReleaseMemObj prior to the starting frame
    // that sets the reference count to 0, and the program is not linked to any other program,
    // meaning that this cl_mem object isn't necessary for the wanted frames. So, we can
    // remove the calls that use it.

    for (size_t i = 0; i < calls->size(); ++i)
    {
        CallCapture *call = &calls->at(i);
        cl_program foundProgram;
        switch (call->entryPoint)
        {
            case EntryPoint::CLCreateProgramWithSource:
            case EntryPoint::CLCreateProgramWithBinary:
            case EntryPoint::CLCreateProgramWithBuiltInKernels:
            case EntryPoint::CLCreateProgramWithIL:
            case EntryPoint::CLLinkProgram:
            {
                foundProgram = call->params.getReturnValue().value.cl_programVal;
                break;
            }
            case EntryPoint::CLRetainProgram:
            case EntryPoint::CLReleaseProgram:
            case EntryPoint::CLBuildProgram:
            case EntryPoint::CLGetProgramInfo:
            case EntryPoint::CLGetProgramBuildInfo:
            case EntryPoint::CLCreateKernel:
            case EntryPoint::CLCreateKernelsInProgram:
            case EntryPoint::CLUnloadPlatformCompiler:
            case EntryPoint::CLCompileProgram:
            {
                uint8_t programIndex = call->entryPoint == EntryPoint::CLCompileProgram ? 1 : 0;
                foundProgram =
                    call->params.getParam("program", ParamType::Tcl_program, programIndex)
                        .value.cl_programVal;
                break;
            }
            default:
                continue;
        }

        if (foundProgram == *program)
        {
            removeCLCall(calls, i);
            --i;
        }
    }

    if (mResourceTrackerCL.mCLProgramToKernels.find(*program) !=
        mResourceTrackerCL.mCLProgramToKernels.end())
    {
        for (size_t i = 0; i < mResourceTrackerCL.mCLProgramToKernels[*program].size(); ++i)
        {
            removeCLKernelOccurrences(&mResourceTrackerCL.mCLProgramToKernels[*program].at(i),
                                      calls);
        }
        mResourceTrackerCL.mCLProgramToKernels.erase(*program);
    }
}

void FrameCaptureShared::removeCLCall(std::vector<CallCapture> *callVector, size_t &callIndex)
{
    CallCapture *call                       = &callVector->at(callIndex);
    const std::vector<ParamCapture> *params = &call->params.getParamCaptures();
    cl_context context                      = nullptr;

    // Checks if there is an event that is implicitly created during the deleted call.
    // If there is, need to inject a clCreateUserEvent call and a clSetUserEventStatus call.
    for (auto &param : *params)
    {
        if (param.type == ParamType::Tcl_context)
        {
            context = param.value.cl_contextVal;
        }
        else if (param.type == ParamType::Tcl_command_queue)
        {
            context =
                param.value.cl_command_queueVal->cast<cl::CommandQueue>().getContext().getNative();
        }
        else if (param.type == ParamType::Tcl_eventPointer && param.value.cl_eventVal != nullptr &&
                 context)
        {
            // Capture the creation of a successful event if the CL call being removed created an
            // event (ex: clEnqueueReadBuffer)
            cl_event event = param.value.cl_eventVal;
            callVector->insert(callVector->begin() + callIndex,
                               CaptureSetUserEventStatus(true, event, CL_COMPLETE, CL_SUCCESS));
            callVector->insert(callVector->begin() + callIndex,
                               CaptureCreateUserEvent(true, context, nullptr, event));
            callIndex += 2;
            break;
        }
    }
    callVector->erase(callVector->begin() + callIndex);
}

void FrameCaptureShared::maybeCapturePreCallUpdatesCL(CallCapture &call)
{
    switch (call.entryPoint)
    {
        case EntryPoint::CLGetExtensionFunctionAddress:
        case EntryPoint::CLGetExtensionFunctionAddressForPlatform:
        {
            uint32_t index = call.entryPoint == EntryPoint::CLGetExtensionFunctionAddress ? 0 : 1;
            std::string funcName =
                (const char *)call.params.getParam("func_name", ParamType::TcharConstPointer, index)
                    .value.charConstPointerPointerVal;
            call.customFunctionName =
                funcName + " = (" + funcName + "_fn)" + GetEntryPointName(call.entryPoint);

            if (std::find(mExtFuncsAdded.begin(), mExtFuncsAdded.end(), funcName) ==
                mExtFuncsAdded.end())
            {
                mExtFuncsAdded.push_back(funcName);
            }
            break;
        }
        case EntryPoint::CLCreateContext:
        case EntryPoint::CLCreateContextFromType:
        {
            if (call.params.getParam("properties", ParamType::Tcl_context_propertiesConstPointer, 0)
                    .value.cl_context_propertiesConstPointerVal)
            {
                size_t propSize        = 0;
                size_t platformIDIndex = 0;
                const cl_context_properties *propertiesData =
                    (cl_context_properties *)call.params
                        .getParam("properties", ParamType::Tcl_context_propertiesConstPointer, 0)
                        .data[0]
                        .data();
                while (propertiesData[propSize] != 0)
                {
                    if (propertiesData[propSize] == CL_CONTEXT_PLATFORM)
                    {
                        // "Each property name is immediately followed by the corresponding desired
                        // value"
                        platformIDIndex = propSize + 1;
                    }
                    ++propSize;
                }
                ++propSize;

                if (platformIDIndex == 0)
                {
                    ParamBuffer params;

                    params.addValueParam("propSize", ParamType::Tsize_t, propSize);

                    ParamCapture propertiesParam("propData",
                                                 ParamType::Tcl_context_propertiesConstPointer);
                    InitParamValue(ParamType::Tcl_context_propertiesConstPointer, propertiesData,
                                   &propertiesParam.value);
                    CaptureMemory(propertiesData, propSize * sizeof(cl_context_properties),
                                  &propertiesParam);
                    params.addParam(std::move(propertiesParam));
                    mFrameCalls.emplace_back(
                        CallCapture("UpdateCLContextPropertiesNoPlatform", std::move(params)));

                    call.params
                        .getParam("properties", ParamType::Tcl_context_propertiesConstPointer, 0)
                        .data.clear();
                    break;
                }

                // Create call to UpdateCLContextProperties
                ParamBuffer params;

                params.addValueParam("propSize", ParamType::Tsize_t, propSize);

                ParamCapture propertiesParam("propData",
                                             ParamType::Tcl_context_propertiesConstPointer);
                InitParamValue(ParamType::Tcl_context_propertiesConstPointer, propertiesData,
                               &propertiesParam.value);
                CaptureMemory(propertiesData, propSize * sizeof(cl_context_properties),
                              &propertiesParam);
                params.addParam(std::move(propertiesParam));

                params.addValueParam("platformIdxInProps", ParamType::Tsize_t, platformIDIndex);
                params.addValueParam("platformIdxInMap", ParamType::Tsize_t,
                                     getIndex((cl_platform_id *)&propertiesData[platformIDIndex]));

                call.params.getParam("properties", ParamType::Tcl_context_propertiesConstPointer, 0)
                    .data.clear();

                mFrameCalls.emplace_back(
                    CallCapture("UpdateCLContextPropertiesWithPlatform", std::move(params)));
            }
            break;
        }
        default:
            break;
    }

    updateReadBufferSize(call.params.getReadBufferSize());
}

void FrameCaptureShared::addCLResetObj(const ParamCapture &param)
{
    mResourceTrackerCL.mCLResetObjs.push_back(angle::ParamCapture("resetObj", param.type));
    auto paramValue =
        &mResourceTrackerCL.mCLResetObjs.at(mResourceTrackerCL.mCLResetObjs.size() - 1).value;
    switch (param.type)
    {
        case ParamType::Tcl_device_id:
            InitParamValue(param.type, param.value.cl_device_idVal, paramValue);
            break;
        case ParamType::Tcl_mem:
            InitParamValue(param.type, param.value.cl_memVal, paramValue);
            break;
        case ParamType::Tcl_kernel:
            InitParamValue(param.type, param.value.cl_kernelVal, paramValue);
            break;
        case ParamType::Tcl_program:
            InitParamValue(param.type, param.value.cl_programVal, paramValue);
            break;
        case ParamType::Tcl_command_queue:
            InitParamValue(param.type, param.value.cl_command_queueVal, paramValue);
            break;
        case ParamType::Tcl_context:
            InitParamValue(param.type, param.value.cl_contextVal, paramValue);
            break;
        case ParamType::Tcl_sampler:
            InitParamValue(param.type, param.value.cl_samplerVal, paramValue);
            break;
        case ParamType::Tcl_event:
            InitParamValue(param.type, param.value.cl_eventVal, paramValue);
            break;
        default:
            break;
    }
}

void FrameCaptureShared::removeCLResetObj(const ParamCapture &param)
{
    std::vector<ParamCapture> &mCLResetObjs = mResourceTrackerCL.mCLResetObjs;
    for (size_t i = 0; i < mCLResetObjs.size(); ++i)
    {
        bool foundCLObj =
            param.type == mCLResetObjs.at(i).type &&
            ((param.type == ParamType::Tcl_device_id &&
              param.value.cl_device_idVal == mCLResetObjs.at(i).value.cl_device_idVal) ||
             (param.type == ParamType::Tcl_mem &&
              param.value.cl_memVal == mCLResetObjs.at(i).value.cl_memVal) ||
             (param.type == ParamType::Tcl_kernel &&
              param.value.cl_kernelVal == mCLResetObjs.at(i).value.cl_kernelVal) ||
             (param.type == ParamType::Tcl_program &&
              param.value.cl_programVal == mCLResetObjs.at(i).value.cl_programVal) ||
             (param.type == ParamType::Tcl_command_queue &&
              param.value.cl_command_queueVal == mCLResetObjs.at(i).value.cl_command_queueVal) ||
             (param.type == ParamType::Tcl_context &&
              param.value.cl_contextVal == mCLResetObjs.at(i).value.cl_contextVal) ||
             (param.type == ParamType::Tcl_sampler &&
              param.value.cl_samplerVal == mCLResetObjs.at(i).value.cl_samplerVal) ||
             (param.type == ParamType::Tcl_event &&
              param.value.cl_eventVal == mCLResetObjs.at(i).value.cl_eventVal));

        if (foundCLObj)
        {
            mCLResetObjs.erase(mCLResetObjs.begin() + i);
            break;
        }
    }
}

void FrameCaptureShared::printCLResetObjs(std::stringstream &stream)
{
    std::vector<ParamCapture> &mCLResetObjs = mResourceTrackerCL.mCLResetObjs;
    for (size_t i = 0; i < mCLResetObjs.size(); ++i)
    {
        stream << "    ";
        switch (mCLResetObjs.at(i).type)
        {
            case ParamType::Tcl_device_id:
                stream << "clReleaseDevice(clDevicesMap["
                       << std::to_string(getIndex(&mCLResetObjs.at(i).value.cl_device_idVal))
                       << "]);";
                break;
            case ParamType::Tcl_mem:
                stream << "clReleaseMemObject(clMemMap["
                       << std::to_string(getIndex(&mCLResetObjs.at(i).value.cl_memVal)) << "]);";
                break;
            case ParamType::Tcl_kernel:
                stream << "clReleaseKernel(clKernelsMap["
                       << std::to_string(getIndex(&mCLResetObjs.at(i).value.cl_kernelVal)) << "]);";
                break;
            case ParamType::Tcl_program:
                stream << "clReleaseProgram(clProgramsMap["
                       << std::to_string(getIndex(&mCLResetObjs.at(i).value.cl_programVal))
                       << "]);";
                break;
            case ParamType::Tcl_command_queue:
                stream << "clReleaseCommandQueue(clCommandQueuesMap["
                       << std::to_string(getIndex(&mCLResetObjs.at(i).value.cl_command_queueVal))
                       << "]);";
                break;
            case ParamType::Tcl_context:
                stream << "clReleaseContext(clContextsMap["
                       << std::to_string(getIndex(&mCLResetObjs.at(i).value.cl_contextVal))
                       << "]);";
                break;
            case ParamType::Tcl_sampler:
                stream << "clReleaseSampler(clSamplersMap["
                       << std::to_string(getIndex(&mCLResetObjs.at(i).value.cl_samplerVal))
                       << "]);";
                break;
            case ParamType::Tcl_event:
                stream << "clReleaseEvent(clEventsMap["
                       << std::to_string(getIndex(&mCLResetObjs.at(i).value.cl_eventVal)) << "]);";
                break;
            default:
                break;
        }
        stream << "\n";
    }
}

void FrameCaptureShared::updateResourceCountsFromParamCaptureCL(const ParamCapture &param,
                                                                const CallCapture &call)
{
    switch (param.type)
    {
        case ParamType::Tcl_platform_idPointer:
            if (call.entryPoint == EntryPoint::CLIcdGetPlatformIDsKHR ||
                call.entryPoint == EntryPoint::CLGetPlatformIDs)
            {
                mMaxCLParamsSize[param.type] +=
                    sizeof(cl_platform_id) * ((call.params.getParamCaptures()[0]).value.cl_uintVal);
            }
            break;
        case ParamType::Tcl_device_idPointer:
            if (call.entryPoint == EntryPoint::CLGetDeviceIDs)
            {
                mMaxCLParamsSize[param.type] +=
                    sizeof(cl_device_id) * ((call.params.getParamCaptures()[2]).value.cl_uintVal);
            }
            break;
        case ParamType::Tcl_context:
            if (call.entryPoint == EntryPoint::CLCreateContext ||
                call.entryPoint == EntryPoint::CLCreateContextFromType)
            {
                if ((getIndex(&param.value.cl_contextVal) + 1) * sizeof(cl_context) >
                    mMaxCLParamsSize[param.type])
                {
                    mMaxCLParamsSize[param.type] =
                        (uint32_t)((getIndex(&param.value.cl_contextVal) + 1) * sizeof(cl_context));
                }
                addCLResetObj(param);
            }
            break;
        case ParamType::Tcl_command_queue:
            if (call.entryPoint == EntryPoint::CLCreateCommandQueueWithProperties ||
                call.entryPoint == EntryPoint::CLCreateCommandQueue)
            {
                if ((getIndex(&param.value.cl_command_queueVal) + 1) * sizeof(cl_command_queue) >
                    mMaxCLParamsSize[param.type])
                {
                    mMaxCLParamsSize[param.type] =
                        (uint32_t)((getIndex(&param.value.cl_command_queueVal) + 1) *
                                   sizeof(cl_command_queue));
                }
                addCLResetObj(param);
            }
            break;
        case ParamType::Tcl_mem:
            if (call.entryPoint == EntryPoint::CLCreateBufferWithProperties ||
                call.entryPoint == EntryPoint::CLCreateBuffer ||
                call.entryPoint == EntryPoint::CLCreateSubBuffer ||
                call.entryPoint == EntryPoint::CLCreateImageWithProperties ||
                call.entryPoint == EntryPoint::CLCreateImage ||
                call.entryPoint == EntryPoint::CLCreateImage2D ||
                call.entryPoint == EntryPoint::CLCreateImage3D)
            {
                if ((getIndex(&param.value.cl_memVal) + 1) * sizeof(cl_mem) >
                    mMaxCLParamsSize[param.type])
                {
                    mMaxCLParamsSize[param.type] =
                        (uint32_t)((getIndex(&param.value.cl_memVal) + 1) * sizeof(cl_mem));
                }
                addCLResetObj(param);
            }
            break;
        case ParamType::Tcl_eventPointer:
        {
            if (param.value.cl_eventVal)
            {
                if ((getIndex(&param.value.cl_eventVal) + 1) * sizeof(cl_event) >
                    mMaxCLParamsSize[param.type])
                {
                    mMaxCLParamsSize[param.type] =
                        (uint32_t)((getIndex(&param.value.cl_eventVal) + 1) * sizeof(cl_event));
                }
                angle::ParamCapture eventParam("event", angle::ParamType::Tcl_event);
                InitParamValue(angle::ParamType::Tcl_event, param.value.cl_eventVal,
                               &eventParam.value);
                addCLResetObj(eventParam);
            }
            break;
        }
        case ParamType::Tcl_program:
            if (call.entryPoint == EntryPoint::CLCreateProgramWithSource ||
                call.entryPoint == EntryPoint::CLCreateProgramWithBinary ||
                call.entryPoint == EntryPoint::CLCreateProgramWithBuiltInKernels ||
                call.entryPoint == EntryPoint::CLLinkProgram ||
                call.entryPoint == EntryPoint::CLCreateProgramWithIL)
            {
                if ((getIndex(&param.value.cl_programVal) + 1) * sizeof(cl_program) >
                    mMaxCLParamsSize[param.type])
                {
                    mMaxCLParamsSize[param.type] =
                        (uint32_t)((getIndex(&param.value.cl_programVal) + 1) * sizeof(cl_program));
                }
                addCLResetObj(param);
            }
            break;
        case ParamType::Tcl_kernel:
            if (call.entryPoint == EntryPoint::CLCreateKernel ||
                call.entryPoint == EntryPoint::CLCloneKernel)
            {
                if ((getIndex(&param.value.cl_kernelVal) + 1) * sizeof(cl_kernel) >
                    mMaxCLParamsSize[param.type])
                {
                    mMaxCLParamsSize[param.type] =
                        (uint32_t)((getIndex(&param.value.cl_kernelVal) + 1) * sizeof(cl_kernel));
                }
                addCLResetObj(param);
            }
            break;
        case ParamType::Tcl_sampler:
            if (call.entryPoint == EntryPoint::CLCreateSampler ||
                call.entryPoint == EntryPoint::CLCreateSamplerWithProperties)
            {
                if ((getIndex(&param.value.cl_samplerVal) + 1) * sizeof(cl_sampler) >
                    mMaxCLParamsSize[param.type])
                {
                    mMaxCLParamsSize[param.type] =
                        (uint32_t)((getIndex(&param.value.cl_samplerVal) + 1) * sizeof(cl_sampler));
                }
                addCLResetObj(param);
            }
            break;
        case ParamType::TvoidPointer:
            if (call.entryPoint == EntryPoint::CLEnqueueMapImage ||
                call.entryPoint == EntryPoint::CLEnqueueMapBuffer)
            {
                mMaxCLParamsSize[param.type] += sizeof(void *);
            }
            break;
        default:
            break;
    }
}

void FrameCaptureShared::updateResourceCountsFromCallCaptureCL(const CallCapture &call)
{
    for (const ParamCapture &param : call.params.getParamCaptures())
    {
        updateResourceCountsFromParamCaptureCL(param, call);
    }

    // Update resource IDs in the return value.
    switch (call.entryPoint)
    {
        case EntryPoint::CLCreateContext:
        case EntryPoint::CLCreateContextFromType:
            setIndex(&call.params.getReturnValue().value.cl_contextVal);
            updateResourceCountsFromParamCaptureCL(call.params.getReturnValue(), call);
            break;
        case EntryPoint::CLCreateBuffer:
        case EntryPoint::CLCreateBufferWithProperties:
        case EntryPoint::CLCreateSubBuffer:
        case EntryPoint::CLCreateImageWithProperties:
        case EntryPoint::CLCreateImage:
        case EntryPoint::CLCreateImage2D:
        case EntryPoint::CLCreateImage3D:
        case EntryPoint::CLCreatePipe:
            setIndex(&call.params.getReturnValue().value.cl_memVal);
            updateResourceCountsFromParamCaptureCL(call.params.getReturnValue(), call);
            break;
        case EntryPoint::CLCreateSampler:
        case EntryPoint::CLCreateSamplerWithProperties:
            setIndex(&call.params.getReturnValue().value.cl_samplerVal);
            updateResourceCountsFromParamCaptureCL(call.params.getReturnValue(), call);
            break;
        case EntryPoint::CLCreateCommandQueue:
        case EntryPoint::CLCreateCommandQueueWithProperties:
            setIndex(&call.params.getReturnValue().value.cl_command_queueVal);
            updateResourceCountsFromParamCaptureCL(call.params.getReturnValue(), call);
            break;
        case EntryPoint::CLCreateProgramWithSource:
        case EntryPoint::CLCreateProgramWithBinary:
        case EntryPoint::CLCreateProgramWithBuiltInKernels:
        case EntryPoint::CLLinkProgram:
        case EntryPoint::CLCreateProgramWithIL:
            setIndex(&call.params.getReturnValue().value.cl_programVal);
            updateResourceCountsFromParamCaptureCL(call.params.getReturnValue(), call);
            break;
        case EntryPoint::CLCreateKernel:
        case EntryPoint::CLCloneKernel:
            setIndex(&call.params.getReturnValue().value.cl_kernelVal);
            updateResourceCountsFromParamCaptureCL(call.params.getReturnValue(), call);
            break;
        case EntryPoint::CLEnqueueMapBuffer:
        case EntryPoint::CLEnqueueMapImage:
        case EntryPoint::CLSVMAlloc:
            if (call.params.getReturnValue().value.voidPointerVal)
            {
                setCLVoidIndex(call.params.getReturnValue().value.voidPointerVal);
                updateResourceCountsFromParamCaptureCL(call.params.getReturnValue(), call);
            }
            break;
        case EntryPoint::CLCreateUserEvent:
            setIndex(&call.params.getReturnValue().value.cl_eventVal);
            break;
        case EntryPoint::CLReleaseDevice:
        case EntryPoint::CLReleaseCommandQueue:
        case EntryPoint::CLReleaseContext:
        case EntryPoint::CLReleaseEvent:
        case EntryPoint::CLReleaseKernel:
        case EntryPoint::CLReleaseMemObject:
        case EntryPoint::CLReleaseProgram:
        case EntryPoint::CLReleaseSampler:
            removeCLResetObj(call.params.getParamCaptures()[0]);
            break;
        default:
            break;
    }
}

void FrameCaptureShared::captureCLCall(CallCapture &&inCall, bool isCallValid)
{
    if (!mCallCaptured)
    {
        mReplayWriter.captureAPI = CaptureAPI::CL;
        mBinaryData.clear();
        mCallCaptured = true;
        std::atexit(onCLProgramEnd);
    }

    if (checkForCaptureEnd())
    {
        onEndCLCapture();
        mCaptureEndFrame = 0;
        return;
    }

    if (mFrameIndex <= mCaptureEndFrame)
    {
        if ((mFrameIndex == mCaptureStartFrame - 1) ||
            ((mFrameIndex == 1) && (mCaptureStartFrame == 1)))
        {
            std::string fileName = GetBinaryDataFilePath(mCompression, mCaptureLabel);
            mBinaryData.initializeBinaryDataStore(mCompression, mOutDirectory, fileName);
        }

        // Keep track of return values from OpenCL calls
        updateResourceCountsFromCallCaptureCL(inCall);

        // Set to true if the call signifies the end of a frame
        // ex: clEnqueueNDRangeKernel
        bool frameEnd = false;

        // Covers pre call updates, like updating the read buffer size
        maybeCapturePreCallUpdatesCL(inCall);

        // If it's an unnecessary call for replay (ex: clGetDeviceInfo)
        if (mCLOptionalCalls.find(inCall.entryPoint) == mCLOptionalCalls.end())
        {
            if (mCLEndFrameCalls.find(inCall.entryPoint) != mCLEndFrameCalls.end())
            {
                frameEnd = true;
            }

            mFrameCalls.emplace_back(std::move(inCall));
        }
        else
        {
            saveCLGetInfo(inCall);
            return;
        }

        // For kernel argument memory snapshots
        maybeCapturePostCallUpdatesCL();
        if (mFrameIndex >= mCaptureStartFrame ||
            (mFrameIndex + 1 == mCaptureStartFrame && frameEnd))
        {
            // Maybe add clEnqueueWrite* or memcpy for memory snapshots
            captureUpdateCLObjs(&mFrameCalls);
        }

        if (frameEnd && mFrameIndex >= mCaptureStartFrame)
        {
            mActiveFrameIndices.push_back(mFrameIndex);
            writeMainContextCppReplayCL();
            if (mFrameIndex == mCaptureEndFrame)
            {
                writeCppReplayIndexFilesCL();
            }
            reset();
        }

        if (frameEnd)
        {
            if (mFrameIndex == (mCaptureStartFrame == 0 ? 0 : mCaptureStartFrame - 1))
            {
                mCLSetupCalls = std::move(mFrameCalls);
            }
            ++mFrameIndex;
        }
    }
}

void FrameCaptureShared::maybeCapturePostCallUpdatesCL()
{
    CallCapture &lastCall = mFrameCalls.back();
    switch (lastCall.entryPoint)
    {
        case EntryPoint::CLEnqueueMapBuffer:
        {
            // Recreate the map call to store in the mCLMapCall unordered_map
            // so later upon the unmap call, the original map data will be available
            cl_command_queue command_queue =
                lastCall.params.getParam("command_queue", ParamType::Tcl_command_queue, 0)
                    .value.cl_command_queueVal;
            cl_mem buffer =
                lastCall.params.getParam("buffer", ParamType::Tcl_mem, 1).value.cl_memVal;
            cl_bool blocking_map =
                lastCall.params.getParam("blocking_map", ParamType::Tcl_bool, 2).value.cl_boolVal;
            cl::MapFlags map_flags =
                lastCall.params.getParam("map_flagsPacked", ParamType::TMapFlags, 3)
                    .value.MapFlagsVal;
            size_t offset =
                lastCall.params.getParam("offset", ParamType::Tsize_t, 4).value.size_tVal;
            size_t size = lastCall.params.getParam("size", ParamType::Tsize_t, 5).value.size_tVal;

            mResourceTrackerCL.mCLMapCall.emplace(
                lastCall.params.getReturnValue().value.voidPointerVal,
                CaptureEnqueueMapBuffer(true, command_queue, buffer, blocking_map, map_flags,
                                        offset, size, 0, nullptr, nullptr, nullptr, nullptr));
            break;
        }
        case EntryPoint::CLEnqueueMapImage:
        {
            // Recreate the map call to store in the mCLMapCall unordered_map
            // so later upon the unmap call, the original map data will be available
            cl_command_queue command_queue =
                lastCall.params.getParam("command_queue", ParamType::Tcl_command_queue, 0)
                    .value.cl_command_queueVal;
            cl_mem image = lastCall.params.getParam("image", ParamType::Tcl_mem, 1).value.cl_memVal;
            cl_bool blocking_map =
                lastCall.params.getParam("blocking_map", ParamType::Tcl_bool, 2).value.cl_boolVal;
            cl::MapFlags map_flags =
                lastCall.params.getParam("map_flagsPacked", ParamType::TMapFlags, 3)
                    .value.MapFlagsVal;
            const size_t *origin =
                lastCall.params.getParam("origin", ParamType::Tsize_tConstPointer, 4)
                    .value.size_tConstPointerVal;
            const size_t *region =
                lastCall.params.getParam("region", ParamType::Tsize_tConstPointer, 5)
                    .value.size_tConstPointerVal;
            size_t *image_row_pitch =
                lastCall.params.getParam("image_row_pitch", ParamType::Tsize_tPointer, 6)
                    .value.size_tPointerVal;
            size_t *image_slice_pitch =
                lastCall.params.getParam("image_slice_pitch", ParamType::Tsize_tPointer, 7)
                    .value.size_tPointerVal;

            mResourceTrackerCL.mCLMapCall.emplace(
                lastCall.params.getReturnValue().value.voidPointerVal,
                CaptureEnqueueMapImage(true, command_queue, image, blocking_map, map_flags, origin,
                                       region, image_row_pitch, image_slice_pitch, 0, nullptr,
                                       nullptr, nullptr, nullptr));
            mResourceTrackerCL.mCLMapCall.at(lastCall.params.getReturnValue().value.voidPointerVal)
                .params.setValueParamAtIndex("image_row_pitch", ParamType::Tsize_t,
                                             *image_row_pitch, 6);
            mResourceTrackerCL.mCLMapCall.at(lastCall.params.getReturnValue().value.voidPointerVal)
                .params.setValueParamAtIndex("image_slice_pitch", ParamType::Tsize_t,
                                             image_slice_pitch == nullptr ? 0 : *image_slice_pitch,
                                             7);
            break;
        }
        case EntryPoint::CLEnqueueUnmapMemObject:
        {
            if (mFrameIndex >= mCaptureStartFrame)
            {
                // Mark as dirty
                cl_mem *mem =
                    &lastCall.params.getParam("memobj", ParamType::Tcl_mem, 1).value.cl_memVal;
                mResourceTrackerCL.mCLCurrentCommandQueue =
                    lastCall.params.getParam("command_queue", ParamType::Tcl_command_queue, 0)
                        .value.cl_command_queueVal;
                CallCapture *mapCall = &mResourceTrackerCL.mCLMapCall.at(
                    lastCall.params.getParam("mapped_ptr", ParamType::TvoidPointer, 2)
                        .value.voidPointerVal);
                auto it = std::find(mResourceTrackerCL.mCLDirtyMem.begin(),
                                    mResourceTrackerCL.mCLDirtyMem.end(), *mem);
                if (it == mResourceTrackerCL.mCLDirtyMem.end() &&
                    mapCall->params.getParam("map_flagsPacked", ParamType::TMapFlags, 3)
                            .value.MapFlagsVal.mask(CL_MAP_WRITE |
                                                    CL_MAP_WRITE_INVALIDATE_REGION) != 0u)
                {
                    mResourceTrackerCL.mCLDirtyMem.push_back(*mem);
                }
            }
            break;
        }
        case EntryPoint::CLEnqueueSVMUnmap:
        {
            // Mark as dirty
            void *svm = &lastCall.params.getParam("svm_ptr", ParamType::TvoidPointer, 1)
                             .value.voidPointerVal;
            mResourceTrackerCL.mCLCurrentCommandQueue =
                lastCall.params.getParam("command_queue", ParamType::Tcl_command_queue, 0)
                    .value.cl_command_queueVal;
            mResourceTrackerCL.mCLDirtySVM.push_back(svm);
            break;
        }
        default:
            break;
    }

    // OpenCL calls that come before the starting frame
    if (mFrameIndex < mCaptureStartFrame)
    {
        std::unordered_map<cl_program, std::vector<cl_kernel>> &mCLProgramToKernels =
            mResourceTrackerCL.mCLProgramToKernels;
        switch (lastCall.entryPoint)
        {
            // There should be no unnecessary enqueue functions prior to the starting frame.
            // captureUpdateCLObjs accounts for it by dynamically adding
            // CLEnqueueWriteBuffer/CLEnqueueWriteImage to ensure the cl_mem objects
            // have the needed info upon replay
            case EntryPoint::CLEnqueueNDRangeKernel:
            case EntryPoint::CLEnqueueNativeKernel:
            case EntryPoint::CLEnqueueTask:
            case EntryPoint::CLEnqueueReadBuffer:
            case EntryPoint::CLEnqueueWriteBuffer:
            case EntryPoint::CLEnqueueReadBufferRect:
            case EntryPoint::CLEnqueueWriteBufferRect:
            case EntryPoint::CLEnqueueReadImage:
            case EntryPoint::CLEnqueueWriteImage:
            case EntryPoint::CLEnqueueCopyBuffer:
            case EntryPoint::CLEnqueueCopyBufferRect:
            case EntryPoint::CLEnqueueCopyImage:
            case EntryPoint::CLEnqueueCopyBufferToImage:
            case EntryPoint::CLEnqueueCopyImageToBuffer:
            case EntryPoint::CLEnqueueFillBuffer:
            case EntryPoint::CLEnqueueFillImage:
            case EntryPoint::CLEnqueueWaitForEvents:
            case EntryPoint::CLEnqueueMarkerWithWaitList:
            case EntryPoint::CLEnqueueBarrierWithWaitList:
            case EntryPoint::CLEnqueueBarrier:
            case EntryPoint::CLEnqueueMarker:
            case EntryPoint::CLEnqueueMigrateMemObjects:
            case EntryPoint::CLEnqueueSVMMemcpy:
            case EntryPoint::CLEnqueueSVMMemFill:
            case EntryPoint::CLEnqueueSVMMigrateMem:
            {
                size_t index = mFrameCalls.size() - 1;
                removeCLCall(&mFrameCalls, index);
                break;
            }
            case EntryPoint::CLCreateBuffer:
            case EntryPoint::CLCreateBufferWithProperties:
            case EntryPoint::CLCreateImage:
            case EntryPoint::CLCreateImageWithProperties:
            case EntryPoint::CLCreateImage2D:
            case EntryPoint::CLCreateImage3D:
            case EntryPoint::CLCreatePipe:
            case EntryPoint::CLCreateSubBuffer:
            {
                const cl_mem *newBuff = &lastCall.params.getReturnValue().value.cl_memVal;

                // Set the parent
                if (lastCall.entryPoint == EntryPoint::CLCreateSubBuffer)
                {
                    cl_mem parent =
                        lastCall.params.getParam("buffer", ParamType::Tcl_mem, 0).value.cl_memVal;
                    mResourceTrackerCL.mCLSubBufferToParent[*newBuff] = parent;
                }

                // Implicit retain
                (*newBuff)->cast<cl::Memory>().retain();

                // Add buffer as tracked
                trackCLMemUpdate(newBuff, true);
                break;
            }
            case EntryPoint::CLReleaseMemObject:
            {
                // Potentially remove buffer/image (and potentially parents) as tracked
                trackCLMemUpdate(
                    &lastCall.params.getParam("memobj", ParamType::Tcl_mem, 0).value.cl_memVal,
                    false);
                break;
            }
            case EntryPoint::CLCreateCommandQueue:
            case EntryPoint::CLCreateCommandQueueWithProperties:
            {
                mResourceTrackerCL.mCLCurrentCommandQueue =
                    lastCall.params.getReturnValue().value.cl_command_queueVal;
                break;
            }
            case EntryPoint::CLCreateProgramWithSource:
            case EntryPoint::CLCreateProgramWithBinary:
            case EntryPoint::CLCreateProgramWithBuiltInKernels:
            case EntryPoint::CLCreateProgramWithIL:
            {
                mCLProgramToKernels[lastCall.params.getReturnValue().value.cl_programVal] =
                    std::vector<cl_kernel>();
                trackCLProgramUpdate(&lastCall.params.getReturnValue().value.cl_programVal, true, 0,
                                     nullptr);
                break;
            }
            case EntryPoint::CLRetainProgram:
            {
                trackCLProgramUpdate(&lastCall.params.getParam("program", ParamType::Tcl_program, 0)
                                          .value.cl_programVal,
                                     true, 0, nullptr);
                break;
            }
            case EntryPoint::CLCompileProgram:
            {
                const cl_program *program =
                    &lastCall.params.getParam("program", ParamType::Tcl_program, 0)
                         .value.cl_programVal;
                trackCLProgramUpdate(
                    program, true,
                    lastCall.params.getParam("num_input_headers", ParamType::Tcl_uint, 4)
                        .value.cl_uintVal,
                    lastCall.params.getParam("input_headers", ParamType::Tcl_programConstPointer, 5)
                        .value.cl_programConstPointerVal);
                break;
            }
            case EntryPoint::CLLinkProgram:
            {
                const cl_program *program = &lastCall.params.getReturnValue().value.cl_programVal;
                mCLProgramToKernels[*program] = std::vector<cl_kernel>();
                trackCLProgramUpdate(
                    program, true,
                    lastCall.params.getParam("num_input_programs", ParamType::Tcl_uint, 4)
                        .value.cl_uintVal,
                    lastCall.params
                        .getParam("input_programs", ParamType::Tcl_programConstPointer, 5)
                        .value.cl_programConstPointerVal);
                break;
            }
            case EntryPoint::CLReleaseProgram:
            {
                trackCLProgramUpdate(&lastCall.params.getParam("program", ParamType::Tcl_program, 0)
                                          .value.cl_programVal,
                                     false, 0, nullptr);
                break;
            }
            case EntryPoint::CLCreateKernel:
            {
                cl_program *program =
                    &lastCall.params.getParam("program", ParamType::Tcl_program, 0)
                         .value.cl_programVal;
                const cl_kernel *kernel = &lastCall.params.getReturnValue().value.cl_kernelVal;
                mCLProgramToKernels[*program].push_back(*kernel);
                mResourceTrackerCL.mCLKernelToProgram[*kernel] = *program;
                break;
            }
            case EntryPoint::CLCloneKernel:
            {
                cl_program *program =
                    &mResourceTrackerCL.mCLKernelToProgram[lastCall.params
                                                               .getParam("source_kernel",
                                                                         ParamType::Tcl_kernel, 0)
                                                               .value.cl_kernelVal];
                const cl_kernel *kernel = &lastCall.params.getReturnValue().value.cl_kernelVal;
                mCLProgramToKernels[*program].push_back(*kernel);
                mResourceTrackerCL.mCLKernelToProgram[*kernel] = *program;
                break;
            }
            case EntryPoint::CLSVMAlloc:
            {
                void *svm = lastCall.params.getReturnValue().value.voidPointerVal;

                // Potentially mark as dirty
                auto it = std::find(mResourceTrackerCL.mCLDirtySVM.begin(),
                                    mResourceTrackerCL.mCLDirtySVM.end(), svm);
                if (it == mResourceTrackerCL.mCLDirtySVM.end())
                {
                    mResourceTrackerCL.mCLDirtySVM.push_back(svm);
                }
                break;
            }
            case EntryPoint::CLSVMFree:
            {
                void *svm = lastCall.params.getParam("svm_pointer", ParamType::TvoidPointer, 1)
                                .value.voidPointerVal;
                auto it = std::find(mResourceTrackerCL.mCLDirtySVM.begin(),
                                    mResourceTrackerCL.mCLDirtySVM.end(), svm);
                if (it != mResourceTrackerCL.mCLDirtySVM.end())
                {
                    mResourceTrackerCL.mCLDirtySVM.erase(it);
                }
                break;
            }
            case EntryPoint::CLEnqueueSVMFree:
            {
                for (cl_uint svmIndex = 0;
                     svmIndex < lastCall.params.getParam("num_svm_pointers", ParamType::Tcl_uint, 1)
                                    .value.cl_uintVal;
                     ++svmIndex)
                {
                    void *svm =
                        lastCall.params.getParam("svm_pointers", ParamType::TvoidPointerPointer, 1)
                            .value.voidPointerPointerVal[svmIndex];
                    auto it = std::find(mResourceTrackerCL.mCLDirtySVM.begin(),
                                        mResourceTrackerCL.mCLDirtySVM.end(), svm);
                    if (it != mResourceTrackerCL.mCLDirtySVM.end())
                    {
                        mResourceTrackerCL.mCLDirtySVM.erase(it);
                    }
                }
                break;
            }
            default:
                break;
        }
    }
}

void FrameCaptureShared::onCLProgramEnd()
{
    if (cl::Platform::GetDefault()->getFrameCaptureShared()->onEndCLCapture())
    {
        delete cl::Platform::GetDefault()->getFrameCaptureShared();
    }
}

bool FrameCaptureShared::onEndCLCapture()
{
    if (mFrameIndex >= mCaptureStartFrame && mFrameIndex <= mCaptureEndFrame)
    {
        mActiveFrameIndices.push_back(mFrameIndex);
        mCaptureEndFrame = mFrameIndex;
        writeMainContextCppReplayCL();
        writeCppReplayIndexFilesCL();
        return true;
    }
    return false;
}

ResourceTrackerCL::ResourceTrackerCL() = default;

ResourceTrackerCL::~ResourceTrackerCL() = default;

void FrameCaptureShared::setCLPlatformIndices(cl_platform_id *platforms, size_t numPlatforms)
{
    for (uint32_t i = 0; i < numPlatforms; ++i)
    {
        setIndex(&platforms[i]);
    }
}

void FrameCaptureShared::setCLDeviceIndices(cl_device_id *devices, size_t numDevices)
{
    for (uint32_t i = 0; i < numDevices; ++i)
    {
        setIndex(&devices[i]);
    }
}

size_t FrameCaptureShared::getCLVoidIndex(const void *v)
{
    if (mResourceTrackerCL.mCLVoidIndices.find(v) == mResourceTrackerCL.mCLVoidIndices.end())
    {
        return SIZE_MAX;
    }
    return mResourceTrackerCL.mCLVoidIndices[v];
}

void FrameCaptureShared::setCLVoidIndex(const void *v)
{
    if (mResourceTrackerCL.mCLVoidIndices.find(v) == mResourceTrackerCL.mCLVoidIndices.end())
    {
        size_t tempSize                      = mResourceTrackerCL.mCLVoidIndices.size();
        mResourceTrackerCL.mCLVoidIndices[v] = tempSize;
    }
}

void FrameCaptureShared::setCLVoidVectorIndex(const void *pointers[],
                                              size_t numPointers,
                                              const angle::ParamCapture *paramCaptureKey)
{
    mResourceTrackerCL.mCLParamIDToIndexVector[paramCaptureKey->uniqueID] = std::vector<size_t>();
    for (size_t i = 0; i < numPointers; ++i)
    {
        mResourceTrackerCL.mCLParamIDToIndexVector[paramCaptureKey->uniqueID].push_back(
            getCLVoidIndex(pointers[i]));
    }
}

void FrameCaptureShared::setOffsetsVector(const void *args,
                                          const void **argsLocations,
                                          size_t numLocations,
                                          const angle::ParamCapture *paramCaptureKey)
{
    mResourceTrackerCL.mCLParamIDToIndexVector[paramCaptureKey->uniqueID] = std::vector<size_t>();
    for (size_t i = 0; i < numLocations; ++i)
    {
        mResourceTrackerCL.mCLParamIDToIndexVector[paramCaptureKey->uniqueID].push_back(
            (char *)argsLocations[i] - (char *)args);
    }
}

std::vector<size_t> FrameCaptureShared::getCLObjVector(const angle::ParamCapture *paramCaptureKey)
{
    if (mResourceTrackerCL.mCLParamIDToIndexVector.find(paramCaptureKey->uniqueID) !=
        mResourceTrackerCL.mCLParamIDToIndexVector.end())
    {
        return mResourceTrackerCL.mCLParamIDToIndexVector[paramCaptureKey->uniqueID];
    }
    return std::vector<size_t>();
}

template <typename T>
std::unordered_map<T, size_t> &FrameCaptureShared::getMap()
{
    ASSERT(false);
    return std::unordered_map<T, size_t>();
}
template <>
std::unordered_map<cl_platform_id, size_t> &FrameCaptureShared::getMap<cl_platform_id>()
{
    return mResourceTrackerCL.mCLPlatformIDIndices;
}
template <>
std::unordered_map<cl_device_id, size_t> &FrameCaptureShared::getMap<cl_device_id>()
{
    return mResourceTrackerCL.mCLDeviceIDIndices;
}
template <>
std::unordered_map<cl_context, size_t> &FrameCaptureShared::getMap<cl_context>()
{
    return mResourceTrackerCL.mCLContextIndices;
}
template <>
std::unordered_map<cl_event, size_t> &FrameCaptureShared::getMap<cl_event>()
{
    return mResourceTrackerCL.mCLEventsIndices;
}
template <>
std::unordered_map<cl_command_queue, size_t> &FrameCaptureShared::getMap<cl_command_queue>()
{
    return mResourceTrackerCL.mCLCommandQueueIndices;
}
template <>
std::unordered_map<cl_mem, size_t> &FrameCaptureShared::getMap<cl_mem>()
{
    return mResourceTrackerCL.mCLMemIndices;
}
template <>
std::unordered_map<cl_sampler, size_t> &FrameCaptureShared::getMap<cl_sampler>()
{
    return mResourceTrackerCL.mCLSamplerIndices;
}
template <>
std::unordered_map<cl_program, size_t> &FrameCaptureShared::getMap<cl_program>()
{
    return mResourceTrackerCL.mCLProgramIndices;
}
template <>
std::unordered_map<cl_kernel, size_t> &FrameCaptureShared::getMap<cl_kernel>()
{
    return mResourceTrackerCL.mCLKernelIndices;
}

void FrameCaptureShared::writeJSONCL()
{

    JsonSerializer json;
    json.startGroup("TraceMetadata");
    json.addBool("IsBinaryDataCompressed", mCompression);
    json.addScalar("CaptureRevision", GetANGLERevision());
    json.addScalar("FrameStart", mCaptureStartFrame);
    json.addScalar("FrameEnd", mFrameIndex);
    json.addBool("IsOpenCL", true);
    json.endGroup();

    json.startGroup("BinaryMetadata");
    json.addScalar("Version", mIndexInfo.version);
    json.addScalar("BlockCount", mIndexInfo.blockCount);
    // These values are handled as strings to avoid json-related underflows
    std::stringstream blockSizeString;
    blockSizeString << mIndexInfo.blockSize;
    json.addString("BlockSize", blockSizeString.str());
    std::stringstream resSizeString;
    resSizeString << mIndexInfo.residentSize;
    json.addString("ResidentSize", resSizeString.str());
    std::stringstream offsetString;
    offsetString << mIndexInfo.indexOffset;
    json.addString("IndexOffset", offsetString.str());
    json.endGroup();

    {
        const std::vector<std::string> &traceFiles = mReplayWriter.getAndResetWrittenFiles();
        json.addVectorOfStrings("TraceFiles", traceFiles);
    }

    {
        std::stringstream jsonFileNameStream;
        jsonFileNameStream << mOutDirectory << FmtCapturePrefix(kNoContextId, mCaptureLabel)
                           << ".json";
        std::string jsonFileName = jsonFileNameStream.str();

        SaveFileHelper saveData(jsonFileName);
        saveData.write(reinterpret_cast<const uint8_t *>(json.data()), json.length());
    }
}

void FrameCaptureShared::saveCLGetInfo(const CallCapture &call)
{
    std::string prevCall = "";
    size_t size;
    std::ostringstream objectStream;
    std::string clObject;
    JsonSerializer json;

    json.startGroup(call.name());

    // Below ONLY for clGetSupportedImageFormats
    if (call.entryPoint == EntryPoint::CLGetSupportedImageFormats)
    {
        const cl_image_format *data =
            call.params.getParam("image_formats", ParamType::Tcl_image_formatPointer, 4)
                .value.cl_image_formatPointerVal;
        if (!data)
        {
            return;
        }
        size_t *sizePointer =
            call.params.getParam("num_image_formats", ParamType::Tcl_uintPointer, 5)
                .value.size_tPointerVal;
        if (!sizePointer)
        {
            size = call.params.getParam("num_entries", ParamType::Tcl_uint, 3).value.cl_uintVal;
        }
        else
        {
            size = *sizePointer;
        }

        cl_context context = call.params.getParamCaptures()[0].value.cl_contextVal;
        objectStream << static_cast<void *>(context);
        clObject = objectStream.str();
        json.addString("context", clObject);
        json.addScalar("flags", call.params.getParamCaptures()[1].value.MemFlagsVal.get());

        cl::MemObjectType imageType =
            call.params.getParam("image_typePacked", ParamType::TMemObjectType, 2)
                .value.MemObjectTypeVal;
        std::ostringstream oss;
        oss << imageType;
        std::string infoString = oss.str();
        json.startGroup(infoString);
        for (size_t j = 0; j < size; ++j)
        {
            std::ostringstream temp;
            temp << (j + 1);
            json.addScalar("image_channel_order" + temp.str(), data[j].image_channel_order);
            json.addScalar("image_channel_data_type" + temp.str(), data[j].image_channel_data_type);
        }

        json.endGroup();
        json.endGroup();
        return;
    }

    // Get the param_value and size
    bool offsetData = 0;
    switch (call.entryPoint)
    {
        case EntryPoint::CLGetProgramBuildInfo:
        case EntryPoint::CLGetKernelArgInfo:
        case EntryPoint::CLGetKernelWorkGroupInfo:
        {
            offsetData = 1;
            break;
        }
        default:
            break;
    }

    const void *data = call.params.getParam("param_value", ParamType::TvoidPointer, 3 + offsetData)
                           .value.voidPointerVal;
    if (!data)
    {
        return;
    }
    size_t *sizePointer =
        call.params.getParam("param_value_size_ret", ParamType::Tsize_tPointer, 4 + offsetData)
            .value.size_tPointerVal;
    if (!sizePointer)
    {
        size = call.params.getParam("param_value_size", ParamType::Tsize_t, 2 + offsetData)
                   .value.size_tVal;
    }
    else
    {
        size = *sizePointer;
    }

    // Get string representation of OpenCL object specified
    switch (call.entryPoint)
    {
        case EntryPoint::CLGetPlatformInfo:
        {
            cl_platform_id platform = call.params.getParamCaptures()[0].value.cl_platform_idVal;
            objectStream << static_cast<void *>(platform);
            break;
        }
        case EntryPoint::CLGetDeviceInfo:
        {
            cl_device_id device = call.params.getParamCaptures()[0].value.cl_device_idVal;
            objectStream << static_cast<void *>(device);
            break;
        }
        case EntryPoint::CLGetContextInfo:
        {
            cl_context context = call.params.getParamCaptures()[0].value.cl_contextVal;
            objectStream << static_cast<void *>(context);
            break;
        }
        case EntryPoint::CLGetCommandQueueInfo:
        {
            cl_command_queue commandQueue =
                call.params.getParamCaptures()[0].value.cl_command_queueVal;
            objectStream << static_cast<void *>(commandQueue);
            break;
        }
        case EntryPoint::CLGetProgramInfo:
        case EntryPoint::CLGetProgramBuildInfo:
        {
            cl_program program = call.params.getParamCaptures()[0].value.cl_programVal;
            objectStream << static_cast<void *>(program);
            break;
        }
        case EntryPoint::CLGetKernelInfo:
        case EntryPoint::CLGetKernelArgInfo:
        case EntryPoint::CLGetKernelWorkGroupInfo:
        {
            cl_kernel kernel = call.params.getParamCaptures()[0].value.cl_kernelVal;
            objectStream << static_cast<void *>(kernel);
            break;
        }
        case EntryPoint::CLGetEventInfo:
        case EntryPoint::CLGetEventProfilingInfo:
        {
            cl_event event = call.params.getParamCaptures()[0].value.cl_eventVal;
            objectStream << static_cast<void *>(event);
            break;
        }
        case EntryPoint::CLGetMemObjectInfo:
        case EntryPoint::CLGetImageInfo:
        {
            cl_mem mem = call.params.getParamCaptures()[0].value.cl_memVal;
            objectStream << static_cast<void *>(mem);
            break;
        }
        case EntryPoint::CLGetSamplerInfo:
        {
            cl_sampler sampler = call.params.getParamCaptures()[0].value.cl_samplerVal;
            objectStream << static_cast<void *>(sampler);
            break;
        }
        default:
            break;
    }
    clObject = objectStream.str();

    // Go through the param_name options
    switch (call.entryPoint)
    {
        case EntryPoint::CLGetPlatformInfo:
        {
            cl::PlatformInfo info = call.params.getParamCaptures()[1].value.PlatformInfoVal;
            std::ostringstream oss;
            oss << info;
            std::string infoString = oss.str();
            json.addString("platform", clObject);

            switch (ToCLenum(info))
            {
                case CL_PLATFORM_PROFILE:
                case CL_PLATFORM_VERSION:
                case CL_PLATFORM_NAME:
                case CL_PLATFORM_VENDOR:
                case CL_PLATFORM_EXTENSIONS:
                case CL_PLATFORM_ICD_SUFFIX_KHR:
                {
                    json.addCString(infoString, static_cast<const char *>(data));
                    break;
                }
                case CL_PLATFORM_EXTENSIONS_WITH_VERSION:
                {
                    const cl_name_version *nameVersion = static_cast<const cl_name_version *>(data);
                    json.startGroup(infoString);
                    for (size_t j = 0; j < size / sizeof(cl_name_version); ++j)
                    {
                        json.addScalar(nameVersion[j].name, nameVersion[j].version);
                    }
                    json.endGroup();
                    break;
                }
                case CL_PLATFORM_NUMERIC_VERSION:
                    json.addScalar(infoString, *static_cast<const cl_uint *>(data));
                    break;
                case CL_PLATFORM_HOST_TIMER_RESOLUTION:
                case CL_PLATFORM_COMMAND_BUFFER_CAPABILITIES_KHR:
                    json.addScalar(infoString, *static_cast<const cl_ulong *>(data));
                    break;
                case CL_PLATFORM_EXTERNAL_MEMORY_IMPORT_HANDLE_TYPES_KHR:
                case CL_PLATFORM_SEMAPHORE_TYPES_KHR:
                case CL_PLATFORM_SEMAPHORE_IMPORT_HANDLE_TYPES_KHR:
                case CL_PLATFORM_SEMAPHORE_EXPORT_HANDLE_TYPES_KHR:
                    json.addVector(infoString,
                                   std::vector<cl_uint>((cl_uint *)data,
                                                        (cl_uint *)data + size / sizeof(cl_uint)));
                    break;
                default:
                    // Not supported or cannot add to JSON file
                    break;
            }

            break;
        }
        case EntryPoint::CLGetDeviceInfo:
        {
            cl::DeviceInfo info = call.params.getParamCaptures()[1].value.DeviceInfoVal;
            std::ostringstream oss;
            oss << info;
            std::string infoString = oss.str();
            json.addString("device", clObject);
            switch (ToCLenum(info))
            {
                case CL_DEVICE_IL_VERSION:
                case CL_DEVICE_LATEST_CONFORMANCE_VERSION_PASSED:
                case CL_DEVICE_OPENCL_C_VERSION:
                case CL_DEVICE_EXTENSIONS:
                case CL_DEVICE_VERSION:
                case CL_DEVICE_PROFILE:
                case CL_DRIVER_VERSION:
                case CL_DEVICE_VENDOR:
                case CL_DEVICE_NAME:
                    json.addCString(infoString, static_cast<const char *>(data));
                    break;
                case CL_DEVICE_TYPE:
                case CL_DEVICE_MAX_MEM_ALLOC_SIZE:
                case CL_DEVICE_LOCAL_MEM_SIZE:
                case CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE:
                case CL_DEVICE_GLOBAL_MEM_SIZE:
                case CL_DEVICE_GLOBAL_MEM_CACHE_SIZE:
                case CL_DEVICE_HALF_FP_CONFIG:
                case CL_DEVICE_SINGLE_FP_CONFIG:
                case CL_DEVICE_DEVICE_ENQUEUE_CAPABILITIES:
                case CL_DEVICE_ATOMIC_FENCE_CAPABILITIES:
                case CL_DEVICE_ATOMIC_MEMORY_CAPABILITIES:
                case CL_DEVICE_SVM_CAPABILITIES:
                case CL_DEVICE_QUEUE_ON_DEVICE_PROPERTIES:
                case CL_DEVICE_PARTITION_AFFINITY_DOMAIN:
                case CL_DEVICE_DOUBLE_FP_CONFIG:
                case CL_DEVICE_QUEUE_ON_HOST_PROPERTIES:
                case CL_DEVICE_EXECUTION_CAPABILITIES:
                    // cl_ulong and cl_bitfield
                    json.addScalar(infoString, *static_cast<const cl_ulong *>(data));
                    break;
                case CL_DEVICE_VENDOR_ID:
                case CL_DEVICE_MAX_COMPUTE_UNITS:
                case CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS:
                case CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR:
                case CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT:
                case CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT:
                case CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG:
                case CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT:
                case CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE:
                case CL_DEVICE_PREFERRED_VECTOR_WIDTH_HALF:
                case CL_DEVICE_NATIVE_VECTOR_WIDTH_CHAR:
                case CL_DEVICE_NATIVE_VECTOR_WIDTH_SHORT:
                case CL_DEVICE_NATIVE_VECTOR_WIDTH_INT:
                case CL_DEVICE_NATIVE_VECTOR_WIDTH_LONG:
                case CL_DEVICE_NATIVE_VECTOR_WIDTH_FLOAT:
                case CL_DEVICE_NATIVE_VECTOR_WIDTH_DOUBLE:
                case CL_DEVICE_NATIVE_VECTOR_WIDTH_HALF:
                case CL_DEVICE_MAX_CLOCK_FREQUENCY:
                case CL_DEVICE_ADDRESS_BITS:
                case CL_DEVICE_IMAGE_SUPPORT:
                case CL_DEVICE_MAX_READ_IMAGE_ARGS:
                case CL_DEVICE_MAX_WRITE_IMAGE_ARGS:
                case CL_DEVICE_MAX_READ_WRITE_IMAGE_ARGS:
                case CL_DEVICE_PIPE_SUPPORT:
                case CL_DEVICE_GENERIC_ADDRESS_SPACE_SUPPORT:
                case CL_DEVICE_WORK_GROUP_COLLECTIVE_FUNCTIONS_SUPPORT:
                case CL_DEVICE_NON_UNIFORM_WORK_GROUP_SUPPORT:
                case CL_DEVICE_NUMERIC_VERSION:
                case CL_DEVICE_SUB_GROUP_INDEPENDENT_FORWARD_PROGRESS:
                case CL_DEVICE_MAX_NUM_SUB_GROUPS:
                case CL_DEVICE_PREFERRED_LOCAL_ATOMIC_ALIGNMENT:
                case CL_DEVICE_PREFERRED_GLOBAL_ATOMIC_ALIGNMENT:
                case CL_DEVICE_PREFERRED_PLATFORM_ATOMIC_ALIGNMENT:
                case CL_DEVICE_PIPE_MAX_PACKET_SIZE:
                case CL_DEVICE_PIPE_MAX_ACTIVE_RESERVATIONS:
                case CL_DEVICE_MAX_PIPE_ARGS:
                case CL_DEVICE_MAX_ON_DEVICE_EVENTS:
                case CL_DEVICE_MAX_ON_DEVICE_QUEUES:
                case CL_DEVICE_QUEUE_ON_DEVICE_MAX_SIZE:
                case CL_DEVICE_QUEUE_ON_DEVICE_PREFERRED_SIZE:
                case CL_DEVICE_IMAGE_BASE_ADDRESS_ALIGNMENT:
                case CL_DEVICE_IMAGE_PITCH_ALIGNMENT:
                case CL_DEVICE_PREFERRED_INTEROP_USER_SYNC:
                case CL_DEVICE_REFERENCE_COUNT:
                case CL_DEVICE_PARTITION_MAX_SUB_DEVICES:
                case CL_DEVICE_LINKER_AVAILABLE:
                case CL_DEVICE_HOST_UNIFIED_MEMORY:
                case CL_DEVICE_COMPILER_AVAILABLE:
                case CL_DEVICE_AVAILABLE:
                case CL_DEVICE_ENDIAN_LITTLE:
                case CL_DEVICE_ERROR_CORRECTION_SUPPORT:
                case CL_DEVICE_LOCAL_MEM_TYPE:
                case CL_DEVICE_MAX_CONSTANT_ARGS:
                case CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE:
                case CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE:
                case CL_DEVICE_MEM_BASE_ADDR_ALIGN:
                case CL_DEVICE_MAX_SAMPLERS:
                    json.addScalar(infoString, *static_cast<const cl_uint *>(data));
                    break;
                case CL_DEVICE_MAX_WORK_GROUP_SIZE:
                case CL_DEVICE_IMAGE2D_MAX_WIDTH:
                case CL_DEVICE_IMAGE2D_MAX_HEIGHT:
                case CL_DEVICE_IMAGE3D_MAX_WIDTH:
                case CL_DEVICE_IMAGE3D_MAX_HEIGHT:
                case CL_DEVICE_IMAGE3D_MAX_DEPTH:
                case CL_DEVICE_IMAGE_MAX_BUFFER_SIZE:
                case CL_DEVICE_IMAGE_MAX_ARRAY_SIZE:
                case CL_DEVICE_PREFERRED_WORK_GROUP_SIZE_MULTIPLE:
                case CL_DEVICE_GLOBAL_VARIABLE_PREFERRED_TOTAL_SIZE:
                case CL_DEVICE_MAX_GLOBAL_VARIABLE_SIZE:
                case CL_DEVICE_PRINTF_BUFFER_SIZE:
                case CL_DEVICE_PROFILING_TIMER_RESOLUTION:
                case CL_DEVICE_MAX_PARAMETER_SIZE:
                    json.addScalar(infoString, *static_cast<const size_t *>(data));
                    break;
                case CL_DEVICE_MAX_WORK_ITEM_SIZES:
                    json.addVector(infoString,
                                   std::vector<size_t>((size_t *)data,
                                                       (size_t *)data + size / sizeof(size_t)));
                    break;
                case CL_DEVICE_PARTITION_TYPE:
                case CL_DEVICE_PARTITION_PROPERTIES:
                    json.addVector(infoString, std::vector<cl_ulong>(
                                                   (cl_ulong *)data,
                                                   (cl_ulong *)data + size / sizeof(cl_ulong)));
                    break;
                case CL_DEVICE_PARENT_DEVICE:
                {
                    std::ostringstream voidStream;
                    voidStream << static_cast<void *>(*(cl_device_id *)data);
                    std::string memLoc = voidStream.str();
                    json.addCString(infoString, memLoc.c_str());
                    break;
                }
                case CL_DEVICE_PLATFORM:
                {
                    std::ostringstream voidStream;
                    voidStream << static_cast<void *>(*(cl_platform_id *)data);
                    std::string memLoc = voidStream.str();
                    json.addCString(infoString, memLoc.c_str());
                    break;
                }
                case CL_DEVICE_ILS_WITH_VERSION:
                case CL_DEVICE_OPENCL_C_FEATURES:
                case CL_DEVICE_OPENCL_C_ALL_VERSIONS:
                case CL_DEVICE_BUILT_IN_KERNELS_WITH_VERSION:
                case CL_DEVICE_EXTENSIONS_WITH_VERSION:
                {
                    const cl_name_version *nameVersion = static_cast<const cl_name_version *>(data);
                    json.startGroup(infoString);
                    for (size_t j = 0; j < size / sizeof(cl_name_version); ++j)
                    {
                        json.addScalar(nameVersion[j].name, nameVersion[j].version);
                    }
                    json.endGroup();
                    break;
                }

                default:
                    // Not supported or cannot add to JSON file
                    break;
            }

            break;
        }
        case EntryPoint::CLGetContextInfo:
        {
            cl::ContextInfo info = call.params.getParamCaptures()[1].value.ContextInfoVal;
            std::ostringstream oss;
            oss << info;
            std::string infoString = oss.str();

            json.addString("context", clObject);
            switch (ToCLenum(info))
            {
                case CL_PLATFORM_PROFILE:
                case CL_PLATFORM_VERSION:
                case CL_PLATFORM_NAME:
                case CL_PLATFORM_VENDOR:
                case CL_PLATFORM_EXTENSIONS:
                case CL_PLATFORM_ICD_SUFFIX_KHR:
                {
                    json.addCString(infoString, static_cast<const char *>(data));
                    break;
                }
                case CL_CONTEXT_REFERENCE_COUNT:
                case CL_CONTEXT_NUM_DEVICES:
                    json.addScalar(infoString, *static_cast<const cl_uint *>(data));
                    break;
                case CL_CONTEXT_PROPERTIES:
                    json.addVector(infoString, std::vector<cl_ulong>(
                                                   (cl_ulong *)data,
                                                   (cl_ulong *)data + size / sizeof(cl_ulong)));
                    break;
                case CL_CONTEXT_DEVICES:
                {
                    const cl_device_id *devices = static_cast<const cl_device_id *>(data);
                    std::vector<std::string> devicesStrings;
                    for (size_t j = 0; j < size / sizeof(cl_device_id); ++j)
                    {
                        std::ostringstream voidStream;
                        voidStream << static_cast<void *>(devices[j]);
                        devicesStrings.push_back(voidStream.str());
                    }
                    json.addVectorOfStrings(infoString, devicesStrings);
                    break;
                }
                default:
                    // Not supported or cannot add to JSON file
                    break;
            }

            break;
        }
        case EntryPoint::CLGetCommandQueueInfo:
        {
            cl::CommandQueueInfo info = call.params.getParamCaptures()[1].value.CommandQueueInfoVal;
            std::ostringstream oss;
            oss << info;
            std::string infoString = oss.str();

            json.addString("command_queue", clObject);
            switch (ToCLenum(info))
            {
                case CL_QUEUE_DEVICE_DEFAULT:
                {
                    std::ostringstream voidStream;
                    voidStream << static_cast<void *>(*(cl_command_queue *)data);
                    std::string memLoc = voidStream.str();
                    json.addCString(infoString, memLoc.c_str());
                    break;
                }
                case CL_QUEUE_CONTEXT:
                {
                    std::ostringstream voidStream;
                    voidStream << static_cast<void *>(*(cl_context *)data);
                    std::string memLoc = voidStream.str();
                    json.addCString(infoString, memLoc.c_str());
                    break;
                }
                case CL_QUEUE_DEVICE:
                {
                    std::ostringstream voidStream;
                    voidStream << static_cast<void *>(*(cl_device_id *)data);
                    std::string memLoc = voidStream.str();
                    json.addCString(infoString, memLoc.c_str());
                    break;
                }
                case CL_QUEUE_REFERENCE_COUNT:
                case CL_QUEUE_SIZE:
                {
                    json.addScalar(infoString, *static_cast<const cl_uint *>(data));
                    break;
                }
                case CL_QUEUE_PROPERTIES:
                {
                    json.addScalar(infoString, *static_cast<const cl_ulong *>(data));
                    break;
                }
                case CL_QUEUE_PROPERTIES_ARRAY:
                {
                    json.addVector(infoString, std::vector<cl_ulong>(
                                                   (cl_ulong *)data,
                                                   (cl_ulong *)data + size / sizeof(cl_ulong)));
                    break;
                }
                default:
                    // Not supported or cannot add to JSON file
                    break;
            }

            break;
        }
        case EntryPoint::CLGetProgramInfo:
        {
            cl::ProgramInfo info = call.params.getParamCaptures()[1].value.ProgramInfoVal;
            std::ostringstream oss;
            oss << info;
            std::string infoString = oss.str();

            json.addString("program", clObject);
            switch (ToCLenum(info))
            {
                case CL_PROGRAM_SOURCE:
                case CL_PROGRAM_IL:
                case CL_PROGRAM_KERNEL_NAMES:
                {
                    json.addCString(infoString, static_cast<const char *>(data));
                    break;
                }
                case CL_PROGRAM_CONTEXT:
                {
                    std::ostringstream voidStream;
                    voidStream << static_cast<void *>(*(cl_context *)data);
                    std::string memLoc = voidStream.str();
                    json.addCString(infoString, memLoc.c_str());
                    break;
                }
                case CL_PROGRAM_REFERENCE_COUNT:
                case CL_PROGRAM_NUM_DEVICES:
                case CL_PROGRAM_SCOPE_GLOBAL_CTORS_PRESENT:
                case CL_PROGRAM_SCOPE_GLOBAL_DTORS_PRESENT:
                {
                    json.addScalar(infoString, *static_cast<const cl_uint *>(data));
                    break;
                }
                case CL_PROGRAM_DEVICES:
                {
                    const cl_device_id *devices = static_cast<const cl_device_id *>(data);
                    std::vector<std::string> devicesStrings;
                    for (size_t j = 0; j < size / sizeof(cl_device_id); ++j)
                    {
                        std::ostringstream voidStream;
                        voidStream << static_cast<void *>(devices[j]);
                        devicesStrings.push_back(voidStream.str());
                    }
                    json.addVectorOfStrings(infoString, devicesStrings);
                    break;
                }
                case CL_PROGRAM_NUM_KERNELS:
                {
                    json.addScalar(infoString, *static_cast<const size_t *>(data));
                    break;
                }
                case CL_PROGRAM_BINARY_SIZES:
                    json.addVector(infoString,
                                   std::vector<size_t>((size_t *)data,
                                                       (size_t *)data + size / sizeof(size_t)));
                    break;
                case CL_PROGRAM_BINARIES:
                    json.addVector(infoString,
                                   std::vector<unsigned char>(
                                       (unsigned char *)data,
                                       (unsigned char *)data + size / sizeof(unsigned char)));
                    break;
                default:
                    // Not supported or cannot add to JSON file
                    break;
            }

            break;
        }
        case EntryPoint::CLGetProgramBuildInfo:
        {
            cl::ProgramBuildInfo info = call.params.getParamCaptures()[2].value.ProgramBuildInfoVal;
            std::ostringstream oss;
            oss << info;
            std::string infoString = oss.str();

            json.addString("program", clObject);
            cl_device_id device = call.params.getParamCaptures()[1].value.cl_device_idVal;
            std::ostringstream objectStream2;
            objectStream2 << static_cast<void *>(device);
            clObject = objectStream2.str();
            json.addString("device", clObject);
            switch (ToCLenum(info))
            {
                case CL_PROGRAM_BUILD_OPTIONS:
                case CL_PROGRAM_BUILD_LOG:
                {
                    json.addCString(infoString, static_cast<const char *>(data));
                    break;
                }
                case CL_PROGRAM_BINARY_TYPE:
                {
                    json.addScalar(infoString, *static_cast<const cl_uint *>(data));
                    break;
                }
                case CL_PROGRAM_BUILD_STATUS:
                {
                    json.addScalar(infoString, *static_cast<const cl_int *>(data));
                    break;
                }
                case CL_PROGRAM_BUILD_GLOBAL_VARIABLE_TOTAL_SIZE:
                {
                    json.addScalar(infoString, *static_cast<const size_t *>(data));
                    break;
                }
                default:
                    // Not supported or cannot add to JSON file
                    break;
            }
            break;
        }
        case EntryPoint::CLGetKernelInfo:
        {
            cl::KernelInfo info = call.params.getParamCaptures()[1].value.KernelInfoVal;
            std::ostringstream oss;
            oss << info;
            std::string infoString = oss.str();

            json.addString("kernel", clObject);
            switch (ToCLenum(info))
            {
                case CL_KERNEL_FUNCTION_NAME:
                case CL_KERNEL_ATTRIBUTES:
                {
                    json.addCString(infoString, static_cast<const char *>(data));
                    break;
                }
                case CL_KERNEL_NUM_ARGS:
                case CL_KERNEL_REFERENCE_COUNT:
                {
                    json.addScalar(infoString, *static_cast<const cl_uint *>(data));
                    break;
                }
                case CL_KERNEL_CONTEXT:
                {
                    std::ostringstream voidStream;
                    voidStream << static_cast<void *>(*(cl_context *)data);
                    std::string memLoc = voidStream.str();
                    json.addCString(infoString, memLoc.c_str());
                    break;
                }
                case CL_KERNEL_PROGRAM:
                {
                    std::ostringstream voidStream;
                    voidStream << static_cast<void *>(*(cl_program *)data);
                    std::string memLoc = voidStream.str();
                    json.addCString(infoString, memLoc.c_str());
                    break;
                }
                default:
                    // Not supported or cannot add to JSON file
                    break;
            }
            break;
        }
        case EntryPoint::CLGetKernelArgInfo:
        {
            cl::KernelArgInfo info = call.params.getParamCaptures()[2].value.KernelArgInfoVal;
            std::ostringstream oss;
            oss << info;
            std::string infoString = oss.str();

            json.addString("kernel", clObject);
            cl_uint index = call.params.getParamCaptures()[1].value.cl_uintVal;
            json.addScalar("arg_index", index);
            switch (ToCLenum(info))
            {
                case CL_KERNEL_ARG_TYPE_NAME:
                case CL_KERNEL_ARG_NAME:
                {
                    json.addCString(infoString, static_cast<const char *>(data));
                    break;
                }
                case CL_KERNEL_ARG_ADDRESS_QUALIFIER:
                case CL_KERNEL_ARG_ACCESS_QUALIFIER:
                {
                    json.addScalar(infoString, *static_cast<const cl_uint *>(data));
                    break;
                }
                case CL_KERNEL_ARG_TYPE_QUALIFIER:
                {
                    json.addScalar(infoString, *static_cast<const cl_ulong *>(data));
                    break;
                }
                default:
                    // Not supported or cannot add to JSON file
                    break;
            }
            break;
        }
        case EntryPoint::CLGetKernelWorkGroupInfo:
        {
            cl::KernelWorkGroupInfo info =
                call.params.getParamCaptures()[2].value.KernelWorkGroupInfoVal;
            std::ostringstream oss;
            oss << info;
            std::string infoString = oss.str();

            json.addString("kernel", clObject);
            cl_device_id device = call.params.getParamCaptures()[1].value.cl_device_idVal;
            std::ostringstream objectStream2;
            objectStream2 << static_cast<void *>(device);
            clObject = objectStream2.str();
            json.addString("device", clObject);
            switch (ToCLenum(info))
            {
                case CL_KERNEL_LOCAL_MEM_SIZE:
                case CL_KERNEL_PRIVATE_MEM_SIZE:
                {
                    json.addScalar(infoString, *static_cast<const cl_ulong *>(data));
                    break;
                }
                case CL_KERNEL_WORK_GROUP_SIZE:
                case CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE:
                {
                    json.addScalar(infoString, *static_cast<const size_t *>(data));
                    break;
                }
                case CL_KERNEL_GLOBAL_WORK_SIZE:
                case CL_KERNEL_COMPILE_WORK_GROUP_SIZE:
                {
                    json.addVector(infoString,
                                   std::vector<size_t>((size_t *)data, (size_t *)data + 3));
                    break;
                }
                default:
                    // Not supported or cannot add to JSON file
                    break;
            }
            break;
        }
        case EntryPoint::CLGetEventInfo:
        {
            cl::EventInfo info = call.params.getParamCaptures()[1].value.EventInfoVal;
            std::ostringstream oss;
            oss << info;
            std::string infoString = oss.str();

            json.addString("event", clObject);
            switch (ToCLenum(info))
            {
                case CL_EVENT_REFERENCE_COUNT:
                case CL_EVENT_COMMAND_TYPE:
                {
                    json.addScalar(infoString, *static_cast<const cl_uint *>(data));
                    break;
                }
                case CL_EVENT_COMMAND_EXECUTION_STATUS:
                {
                    json.addScalar(infoString, *static_cast<const cl_int *>(data));
                    break;
                }
                case CL_EVENT_CONTEXT:
                {
                    std::ostringstream voidStream;
                    voidStream << static_cast<void *>(*(cl_context *)data);
                    std::string memLoc = voidStream.str();
                    json.addCString(infoString, memLoc.c_str());
                    break;
                }
                case CL_EVENT_COMMAND_QUEUE:
                {
                    std::ostringstream voidStream;
                    voidStream << static_cast<void *>(*(cl_command_queue *)data);
                    std::string memLoc = voidStream.str();
                    json.addCString(infoString, memLoc.c_str());
                    break;
                }
                default:
                    // Not supported or cannot add to JSON file
                    break;
            }
            break;
        }
        case EntryPoint::CLGetEventProfilingInfo:
        {
            cl::ProfilingInfo info = call.params.getParamCaptures()[1].value.ProfilingInfoVal;
            std::ostringstream oss;
            oss << info;
            std::string infoString = oss.str();

            json.addString("event", clObject);
            switch (ToCLenum(info))
            {
                case CL_PROFILING_COMMAND_QUEUED:
                case CL_PROFILING_COMMAND_SUBMIT:
                case CL_PROFILING_COMMAND_START:
                case CL_PROFILING_COMMAND_END:
                case CL_PROFILING_COMMAND_COMPLETE:
                {
                    json.addScalar(infoString, *static_cast<const cl_ulong *>(data));
                    break;
                }
                default:
                    // Not supported or cannot add to JSON file
                    break;
            }
            break;
        }
        case EntryPoint::CLGetMemObjectInfo:
        {
            cl::MemInfo info = call.params.getParamCaptures()[1].value.MemInfoVal;
            std::ostringstream oss;
            oss << info;
            std::string infoString = oss.str();

            json.addString("memObj", clObject);
            switch (ToCLenum(info))
            {
                case CL_MEM_TYPE:
                case CL_MEM_MAP_COUNT:
                case CL_MEM_REFERENCE_COUNT:
                case CL_MEM_USES_SVM_POINTER:
                {
                    json.addScalar(infoString, *static_cast<const cl_uint *>(data));
                    break;
                }
                case CL_MEM_FLAGS:
                {
                    json.addScalar(infoString, *static_cast<const cl_ulong *>(data));
                    break;
                }
                case CL_MEM_SIZE:
                case CL_MEM_OFFSET:
                {
                    json.addScalar(infoString, *static_cast<const size_t *>(data));
                    break;
                }
                case CL_MEM_HOST_PTR:
                {
                    std::ostringstream voidStream;
                    voidStream << data;
                    std::string memLoc = voidStream.str();
                    json.addCString(infoString, memLoc.c_str());
                    break;
                }
                case CL_MEM_CONTEXT:
                {
                    std::ostringstream voidStream;
                    voidStream << static_cast<void *>(*(cl_context *)data);
                    std::string memLoc = voidStream.str();
                    json.addCString(infoString, memLoc.c_str());
                    break;
                }
                case CL_MEM_ASSOCIATED_MEMOBJECT:
                {
                    std::ostringstream voidStream;
                    voidStream << static_cast<void *>(*(cl_mem *)data);
                    std::string memLoc = voidStream.str();
                    json.addCString(infoString, memLoc.c_str());
                    break;
                }
                case CL_MEM_PROPERTIES:
                {
                    json.addVector(infoString, std::vector<cl_mem_properties>(
                                                   (cl_mem_properties *)data,
                                                   (cl_mem_properties *)data +
                                                       size / sizeof(cl_mem_properties)));
                    break;
                }
                default:
                    // Not supported or cannot add to JSON file
                    break;
            }
            break;
        }
        case EntryPoint::CLGetImageInfo:
        {
            cl::ImageInfo info = call.params.getParamCaptures()[1].value.ImageInfoVal;
            std::ostringstream oss;
            oss << info;
            std::string infoString = oss.str();

            json.addString("image", clObject);
            switch (ToCLenum(info))
            {
                case CL_IMAGE_FORMAT:
                {
                    json.startGroup(infoString);
                    json.addScalar("image_channel_order",
                                   static_cast<const cl_image_format *>(data)->image_channel_order);
                    json.addScalar(
                        "image_channel_data_type",
                        static_cast<const cl_image_format *>(data)->image_channel_data_type);
                    json.endGroup();
                    break;
                }
                case CL_IMAGE_NUM_MIP_LEVELS:
                case CL_IMAGE_NUM_SAMPLES:
                {
                    json.addScalar(infoString, *static_cast<const cl_uint *>(data));
                    break;
                }
                case CL_IMAGE_ELEMENT_SIZE:
                case CL_IMAGE_ROW_PITCH:
                case CL_IMAGE_SLICE_PITCH:
                case CL_IMAGE_WIDTH:
                case CL_IMAGE_HEIGHT:
                case CL_IMAGE_DEPTH:
                case CL_IMAGE_ARRAY_SIZE:
                {
                    json.addScalar(infoString, *static_cast<const size_t *>(data));
                    break;
                }
                case CL_IMAGE_BUFFER:
                {
                    std::ostringstream voidStream;
                    voidStream << static_cast<void *>(*(cl_mem *)data);
                    std::string memLoc = voidStream.str();
                    json.addCString(infoString, memLoc.c_str());
                    break;
                }
                default:
                    // Not supported or cannot add to JSON file
                    break;
            }
            break;
        }
        case EntryPoint::CLGetSamplerInfo:
        {
            cl::SamplerInfo info = call.params.getParamCaptures()[1].value.SamplerInfoVal;
            std::ostringstream oss;
            oss << info;
            std::string infoString = oss.str();

            json.addString("image", clObject);
            switch (ToCLenum(info))
            {
                case CL_SAMPLER_REFERENCE_COUNT:
                case CL_SAMPLER_NORMALIZED_COORDS:
                case CL_SAMPLER_ADDRESSING_MODE:
                case CL_SAMPLER_FILTER_MODE:
                {
                    json.addScalar(infoString, *static_cast<const cl_uint *>(data));
                    break;
                }
                case CL_SAMPLER_PROPERTIES:
                {
                    json.addVector(infoString, std::vector<cl_sampler_properties>(
                                                   (cl_sampler_properties *)data,
                                                   (cl_sampler_properties *)data +
                                                       size / sizeof(cl_sampler_properties)));
                    break;
                }
                case CL_SAMPLER_CONTEXT:
                {
                    std::ostringstream voidStream;
                    voidStream << static_cast<void *>(*(cl_context *)data);
                    std::string memLoc = voidStream.str();
                    json.addCString(infoString, memLoc.c_str());
                    break;
                }
                default:
                    // Not supported or cannot add to JSON file
                    break;
            }
            break;
        }
        default:
            break;
    }

    json.endGroup();

    mCLInfoJson += std::string(json.data()) + ",\n";
}

void FrameCaptureShared::writeJSONCLGetInfo()
{
    std::stringstream jsonFileNameStream;
    jsonFileNameStream << mOutDirectory << FmtCapturePrefix(kNoContextId, mCaptureLabel)
                       << "_OpenCL_info.json";
    std::string jsonFileName = jsonFileNameStream.str();

    SaveFileHelper saveData(jsonFileName);

    saveData.write(reinterpret_cast<const uint8_t *>(mCLInfoJson.c_str()), mCLInfoJson.length());
}

void FrameCaptureShared::writeCppReplayIndexFilesCL()
{
    // Ensure the last frame is written. This will no-op if the frame is already written.
    mReplayWriter.saveFrame();

    {
        std::stringstream header;

        header << "#pragma once\n";
        header << "\n";
        header << "#define CL_NO_EXTENSION_PROTOTYPES\n";
        header << "#include <angle_cl.h>\n";
        header << "#include <stdint.h>\n";
        header << "#include \"trace_fixture_cl.h\"\n";

        std::string includes = header.str();
        mReplayWriter.setHeaderPrologue(includes);
    }

    {
        std::stringstream source;

        source << "#include \"" << FmtCapturePrefix(kNoContextId, mCaptureLabel) << ".h\"\n";
        source << "#include \"trace_fixture_cl.h\"\n";

        std::string sourcePrologue = source.str();
        mReplayWriter.setSourcePrologue(sourcePrologue);
    }

    {
        std::string proto = "void InitReplay(void)";

        std::stringstream source;
        source << proto << "\n";
        source << "{\n";
        WriteInitReplayCallCL(mCompression, source, mCaptureLabel, 0, mReadBufferSize,
                              mMaxCLParamsSize);
        source << "}\n";

        mReplayWriter.addPrivateFunction(proto, std::stringstream(), source);
    }

    {
        std::string proto = "void ReplayFrame(uint32_t frameIndex)";

        std::stringstream source;

        source << proto << "\n";
        source << "{\n";
        source << "    switch (frameIndex)\n";
        source << "    {\n";
        for (uint32_t frameIndex : mActiveFrameIndices)
        {
            source << "        case " << frameIndex << ":\n";
            source << "            " << FmtReplayFunction(kNoContextId, FuncUsage::Call, frameIndex)
                   << ";\n";
            source << "            break;\n";
        }
        source << "        default:\n";
        source << "            break;\n";
        source << "    }\n";
        source << "}\n";

        mReplayWriter.addPublicFunction(proto, std::stringstream(), source);
    }

    for (auto extFuncName : mExtFuncsAdded)
    {
        mReplayWriter.addStaticVariable(extFuncName + "_fn", extFuncName);
    }

    std::stringstream protoSetupStream;
    protoSetupStream << "void SetupFirstFrame(void)";
    std::string protoSetup = protoSetupStream.str();
    std::stringstream headerStreamSetup;
    std::stringstream bodyStreamSetup;
    WriteCppReplayFunctionWithPartsCL(ReplayFunc::SetupFirstFrame, mReplayWriter,
                                      mCaptureStartFrame, &mBinaryData, mCLSetupCalls,
                                      headerStreamSetup, bodyStreamSetup);
    mReplayWriter.addPublicFunction(protoSetup, headerStreamSetup, bodyStreamSetup);

    {
        std::string proto = "void ResetReplay(void)";
        std::stringstream source;
        source << proto << "\n" << "{\n";
        printCLResetObjs(source);
        source << "}\n";
        mReplayWriter.addPublicFunction(proto, std::stringstream(), source);
    }

    {
        std::stringstream fnameStream;
        fnameStream << mOutDirectory << FmtCapturePrefix(kNoContextId, mCaptureLabel);
        std::string fnamePattern = fnameStream.str();

        mReplayWriter.setFilenamePattern(fnamePattern);
    }

    mReplayWriter.saveIndexFilesAndHeader();

    // Finalize binary data file
    mIndexInfo = mBinaryData.closeBinaryDataStore();
    writeJSONCL();
    writeJSONCLGetInfo();
}

void FrameCaptureShared::writeMainContextCppReplayCL()
{
    {
        std::stringstream header;

        header << "#include \"" << FmtCapturePrefix(kNoContextId, mCaptureLabel) << ".h\"\n";
        header << "#include \"trace_fixture_cl.h\"\n";

        std::string headerString = header.str();
        mReplayWriter.setSourcePrologue(headerString);
    }

    uint32_t frameIndex = getReplayFrameIndex();

    if (frameIndex == 1)
    {
        {
            std::string proto = "void SetupReplay(void)";

            std::stringstream out;

            out << proto << "\n";
            out << "{\n";

            // Setup all of the shared objects.
            out << "    InitReplay();\n";

            out << "}\n";

            mReplayWriter.addPublicFunction(proto, std::stringstream(), out);
        }
    }

    if (!mFrameCalls.empty())
    {
        std::stringstream protoStream;
        protoStream << "void "
                    << FmtReplayFunction(kNoContextId, FuncUsage::Prototype, mFrameIndex);
        std::string proto = protoStream.str();
        std::stringstream headerStream;
        std::stringstream bodyStream;

        WriteCppReplayFunctionWithPartsCL(ReplayFunc::Replay, mReplayWriter, mFrameIndex,
                                          &mBinaryData, mFrameCalls, headerStream, bodyStream);

        mReplayWriter.addPrivateFunction(proto, headerStream, bodyStream);
    }

    {
        std::stringstream fnamePatternStream;
        fnamePatternStream << mOutDirectory << FmtCapturePrefix(kNoContextId, mCaptureLabel);
        std::string fnamePattern = fnamePatternStream.str();

        mReplayWriter.setFilenamePattern(fnamePattern);
    }

    if (mFrameIndex == mCaptureEndFrame)
    {
        mReplayWriter.saveFrame();
    }
}

}  // namespace angle
