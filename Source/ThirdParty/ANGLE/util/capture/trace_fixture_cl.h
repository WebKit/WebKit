//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// trace_fixture_cl.h:
//   OpenCL-specific code for the ANGLE trace replays.
//

#ifndef ANGLE_TRACE_FIXTURE_CL_H_
#define ANGLE_TRACE_FIXTURE_CL_H_

#include <CL/cl.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include "trace_interface.h"
#include "traces_export.h"

#include "trace_egl_loader_autogen.h"
#include "trace_gles_loader_autogen.h"

#if defined(__cplusplus)
#    include <cstdio>
#    include <cstring>
#    include <limits>
#    include <unordered_map>
#    include <vector>

extern "C" {

// Functions implemented by traces.
// "not exported" tag is a hack to get around trace interpreter codegen -_-
/* not exported */ void SetupReplay();
/* not exported */ void ReplayFrame(uint32_t frameIndex);
/* not exported */ void ResetReplay();
/* not exported */ void FinishReplay();
/* not exported */ void SetupFirstFrame();

ANGLE_REPLAY_EXPORT void SetupEntryPoints(angle::TraceCallbacks *traceCallbacks,
                                          angle::TraceFunctions **traceFunctions);
#endif  // defined(__cplusplus)

extern uint8_t *gBinaryData;
extern uint8_t *gReadBuffer;

extern cl_platform_id *clPlatformsMap;
extern cl_device_id *clDevicesMap;
extern cl_context *clContextsMap;
extern cl_command_queue *clCommandQueuesMap;
extern cl_mem *clMemMap;
extern cl_event *clEventsMap;
extern cl_program *clProgramsMap;
extern cl_kernel *clKernelsMap;
extern cl_sampler *clSamplerMap;
extern void **clVoidMap;

extern std::vector<cl_platform_id> temporaryPlatformsList;
extern std::vector<cl_device_id> temporaryDevicesList;
extern std::vector<cl_kernel> temporaryKernelsList;
extern std::vector<cl_mem> temporaryBuffersList;
extern std::vector<cl_program> temporaryProgramsList;
extern std::vector<cl_event> temporaryEventsList;
extern cl_image_desc temporaryImageDesc;
extern std::vector<cl_context_properties> temporaryContextProps;
extern std::vector<const char *> temporaryCharPointerList;
extern std::vector<const void *> temporaryVoidPtrList;
extern std::vector<const unsigned char *> temporaryUnsignedCharPointerList;
extern void *temporaryVoidPtr;

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
                         uint32_t maxCLVoidPointer);

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
                        uint32_t maxCLVoidPointer);

const uint8_t *GetBinaryData(const size_t offset);
void InitializeBinaryDataLoader();

void UpdateCLContextPropertiesNoPlatform(size_t propSize, const cl_context_properties *propData);
void UpdateCLContextPropertiesWithPlatform(size_t propSize,
                                           const cl_context_properties *propData,
                                           size_t platformIdxInProps,
                                           size_t platformIdxInMap);

#if defined(__cplusplus)
}       // extern "C"
#endif  // defined(__cplusplus)

#endif  // ANGLE_TRACE_FIXTURE_CL_H_
