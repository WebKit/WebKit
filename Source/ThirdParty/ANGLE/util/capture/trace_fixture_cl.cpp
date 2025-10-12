//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// trace_fixture_cl.cpp:
//   OpenCL-specific code for the ANGLE trace replays.
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "trace_fixture_cl.h"
#include <string>

namespace
{

angle::TraceCallbacks *gTraceCallbacks = nullptr;

}

cl_platform_id *clPlatformsMap;
cl_device_id *clDevicesMap;
cl_context *clContextsMap;
cl_command_queue *clCommandQueuesMap;
cl_mem *clMemMap;
cl_event *clEventsMap;
cl_program *clProgramsMap;
cl_kernel *clKernelsMap;
cl_sampler *clSamplerMap;
void **clVoidMap;

std::vector<cl_platform_id> temporaryPlatformsList;
std::vector<cl_device_id> temporaryDevicesList;
std::vector<cl_kernel> temporaryKernelsList;
std::vector<cl_mem> temporaryBuffersList;
std::vector<cl_program> temporaryProgramsList;
std::vector<cl_event> temporaryEventsList;
cl_image_desc temporaryImageDesc;
std::vector<cl_context_properties> temporaryContextProps;
std::vector<const char *> temporaryCharPointerList;
std::vector<const void *> temporaryVoidPtrList;
std::vector<const unsigned char *> temporaryUnsignedCharPointerList;
void *temporaryVoidPtr;

angle::ReplayResourceMode gReplayResourceMode = angle::ReplayResourceMode::Active;

uint8_t *gBinaryData;
uint8_t *gReadBuffer;
angle::FrameCaptureBinaryData *gFrameCaptureBinaryData;
std::string gBinaryDataDir = ".";

template <typename T>
T *AllocateZeroedValues(size_t count)
{
    T *mem = new T[count + 1];
    memset(mem, 0, sizeof(T) * (count + 1));
    return mem;
}

GLuint *AllocateZeroedUints(size_t count)
{
    return AllocateZeroedValues<GLuint>(count);
}

void InitializeReplayCL2(const char *binaryDataFileName,
                         size_t maxClientArraySize,
                         size_t readBufferSize,
                         uint32_t maxCLPlatform,
                         uint32_t maxCLDevices,
                         uint32_t maxCLContexts,
                         uint32_t maxCLCommandQueues,
                         uint32_t maxCLMem,
                         uint32_t maxCLEvents,
                         uint32_t maxCLPrograms,
                         uint32_t maxCLKernels,
                         uint32_t maxCLSamplers,
                         uint32_t maxCLVoidPointer)
{
    gFrameCaptureBinaryData = gTraceCallbacks->ConfigureBinaryDataLoader(binaryDataFileName);
}

void InitializeReplayCL(const char *binaryDataFileName,
                        size_t maxClientArraySize,
                        size_t readBufferSize,
                        uint32_t maxCLPlatform,
                        uint32_t maxCLDevices,
                        uint32_t maxCLContexts,
                        uint32_t maxCLCommandQueues,
                        uint32_t maxCLMem,
                        uint32_t maxCLEvents,
                        uint32_t maxCLPrograms,
                        uint32_t maxCLKernels,
                        uint32_t maxCLSamplers,
                        uint32_t maxCLVoidPointer)
{
    if (!gFrameCaptureBinaryData)
    {
        gBinaryData = gTraceCallbacks->LoadBinaryData(binaryDataFileName);
    }

    gReadBuffer = new uint8_t[readBufferSize];

    clPlatformsMap     = AllocateZeroedValues<cl_platform_id>(maxCLPlatform);
    clDevicesMap       = AllocateZeroedValues<cl_device_id>(maxCLDevices);
    clContextsMap      = AllocateZeroedValues<cl_context>(maxCLContexts);
    clCommandQueuesMap = AllocateZeroedValues<cl_command_queue>(maxCLCommandQueues);
    clMemMap           = AllocateZeroedValues<cl_mem>(maxCLMem);
    clEventsMap        = AllocateZeroedValues<cl_event>(maxCLEvents);
    clProgramsMap      = AllocateZeroedValues<cl_program>(maxCLPrograms);
    clKernelsMap       = AllocateZeroedValues<cl_kernel>(maxCLKernels);
    clSamplerMap       = AllocateZeroedValues<cl_sampler>(maxCLSamplers);
    clVoidMap          = AllocateZeroedValues<void *>(maxCLVoidPointer);
}

void FinishReplay()
{
    delete[] gReadBuffer;

    delete[] clPlatformsMap;
    delete[] clDevicesMap;
    delete[] clContextsMap;
    delete[] clCommandQueuesMap;
    delete[] clMemMap;
    delete[] clEventsMap;
    delete[] clProgramsMap;
    delete[] clKernelsMap;
    delete[] clSamplerMap;
    delete[] clVoidMap;

    if (gFrameCaptureBinaryData)
    {
        gFrameCaptureBinaryData->closeBinaryDataLoader();
        delete gFrameCaptureBinaryData;
        gFrameCaptureBinaryData = nullptr;
    }
}

angle::TraceInfo gTraceInfo;
std::string gTraceGzPath;

struct TraceFunctionsImplCL : angle::TraceFunctions
{
    void SetupReplay() override { ::SetupReplay(); }

    void ReplayFrame(uint32_t frameIndex) override { ::ReplayFrame(frameIndex); }

    void ResetReplay() override { ::ResetReplay(); }

    void SetupFirstFrame() override { ::SetupFirstFrame(); }

    void FinishReplay() override { ::FinishReplay(); }

    void SetBinaryDataDir(const char *dataDir) override { gBinaryDataDir = dataDir; }

    void SetReplayResourceMode(const angle::ReplayResourceMode resourceMode) override
    {
        gReplayResourceMode = resourceMode;
    }

    void SetTraceInfo(const angle::TraceInfo &traceInfo) override { gTraceInfo = traceInfo; }

    void SetTraceGzPath(const std::string &traceGzPath) override { gTraceGzPath = traceGzPath; }
};

TraceFunctionsImplCL gTraceFunctionsImpl;

void SetupEntryPoints(angle::TraceCallbacks *traceCallbacks, angle::TraceFunctions **traceFunctions)
{
    gTraceCallbacks = traceCallbacks;
    *traceFunctions = &gTraceFunctionsImpl;
}

void UpdateCLContextPropertiesNoPlatform(size_t propSize, const cl_context_properties *propData)
{
    temporaryContextProps.resize(propSize);
    std::memcpy(temporaryContextProps.data(), propData, propSize);
}

void UpdateCLContextPropertiesWithPlatform(size_t propSize,
                                           const cl_context_properties *propData,
                                           size_t platformIdxInProps,
                                           size_t platformIdxInMap)
{
    UpdateCLContextPropertiesNoPlatform(propSize, propData);
    std::memcpy(&temporaryContextProps.data()[platformIdxInProps],
                &clPlatformsMap[platformIdxInMap], sizeof(cl_platform_id));
}

const uint8_t *GetBinaryData(const size_t offset)
{
    return gFrameCaptureBinaryData->getData(offset);
}

void InitializeBinaryDataLoader()
{
    gFrameCaptureBinaryData->initializeBinaryDataLoader();
}
