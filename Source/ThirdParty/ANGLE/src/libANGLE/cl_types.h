//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// cl_types.h: Defines common types for the OpenCL support in ANGLE.

#ifndef LIBANGLE_CLTYPES_H_
#define LIBANGLE_CLTYPES_H_

#include "libANGLE/CLBitField.h"
#include "libANGLE/CLRefPointer.h"
#include "libANGLE/Debug.h"

#include "common/PackedCLEnums_autogen.h"
#include "common/angleutils.h"

// Include frequently used standard headers
#include <algorithm>
#include <array>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace cl
{

class Buffer;
class CommandQueue;
class Context;
class Device;
class Event;
class Image;
class Kernel;
class Memory;
class Object;
class Platform;
class Program;
class Sampler;

using BufferPtr       = RefPointer<Buffer>;
using CommandQueuePtr = RefPointer<CommandQueue>;
using ContextPtr      = RefPointer<Context>;
using DevicePtr       = RefPointer<Device>;
using EventPtr        = RefPointer<Event>;
using KernelPtr       = RefPointer<Kernel>;
using MemoryPtr       = RefPointer<Memory>;
using PlatformPtr     = RefPointer<Platform>;
using ProgramPtr      = RefPointer<Program>;
using SamplerPtr      = RefPointer<Sampler>;

using BufferPtrs   = std::vector<BufferPtr>;
using DevicePtrs   = std::vector<DevicePtr>;
using EventPtrs    = std::vector<EventPtr>;
using KernelPtrs   = std::vector<KernelPtr>;
using MemoryPtrs   = std::vector<MemoryPtr>;
using PlatformPtrs = std::vector<PlatformPtr>;
using ProgramPtrs  = std::vector<ProgramPtr>;

using WorkgroupSize    = std::array<size_t, 3>;
using GlobalWorkOffset = std::array<size_t, 3>;
using GlobalWorkSize   = std::array<size_t, 3>;
using WorkgroupCount   = std::array<uint32_t, 3>;

template <typename T>
using EventStatusMap = std::array<T, 3>;

struct ImageDescriptor
{
    MemObjectType type;
    size_t width;
    size_t height;
    size_t depth;
    size_t arraySize;
    size_t rowPitch;
    size_t slicePitch;
    cl_uint numMipLevels;
    cl_uint numSamples;
};

struct MemOffsets
{
    size_t x, y, z;
};

struct Coordinate
{
    size_t x, y, z;
};

struct NDRange
{
    NDRange(cl_uint workDimensionsIn,
            const size_t *globalWorkOffsetIn,
            const size_t *globalWorkSizeIn,
            const size_t *localWorkSizeIn)
        : workDimensions(workDimensionsIn),
          globalWorkOffset({0, 0, 0}),
          globalWorkSize({1, 1, 1}),
          localWorkSize({1, 1, 1}),
          nullLocalWorkSize(localWorkSizeIn == nullptr)
    {
        for (cl_uint dim = 0; dim < workDimensionsIn; dim++)
        {
            if (globalWorkOffsetIn != nullptr)
            {
                globalWorkOffset[dim] = globalWorkOffsetIn[dim];
            }
            if (globalWorkSizeIn != nullptr)
            {
                globalWorkSize[dim] = globalWorkSizeIn[dim];
            }
            if (localWorkSizeIn != nullptr)
            {
                localWorkSize[dim] = localWorkSizeIn[dim];
            }
            ASSERT(globalWorkSize[dim] != 0);
            ASSERT(localWorkSize[dim] != 0);
        }
    }

    cl_uint workDimensions;
    GlobalWorkOffset globalWorkOffset;
    GlobalWorkSize globalWorkSize;
    WorkgroupSize localWorkSize;
    bool nullLocalWorkSize{false};
};

}  // namespace cl

#endif  // LIBANGLE_CLTYPES_H_
