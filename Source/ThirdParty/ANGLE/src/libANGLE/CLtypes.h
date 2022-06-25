//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLtypes.h: Defines common types for the OpenCL support in ANGLE.

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

#define ANGLE_CL_TRY(expression)              \
    do                                        \
    {                                         \
        const cl_int _errorCode = expression; \
        if (_errorCode != CL_SUCCESS)         \
        {                                     \
            return _errorCode;                \
        }                                     \
    } while (0)

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

}  // namespace cl

#endif  // LIBANGLE_CLTYPES_H_
