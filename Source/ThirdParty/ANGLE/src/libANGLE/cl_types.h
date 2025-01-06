//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// cl_types.h: Defines common types for the OpenCL support in ANGLE.

#ifndef LIBANGLE_CLTYPES_H_
#define LIBANGLE_CLTYPES_H_

#if defined(ANGLE_ENABLE_CL)
#    include "libANGLE/CLBitField.h"
#    include "libANGLE/CLRefPointer.h"
#    include "libANGLE/Debug.h"
#    include "libANGLE/angletypes.h"

#    include "common/PackedCLEnums_autogen.h"
#    include "common/angleutils.h"

// Include frequently used standard headers
#    include <algorithm>
#    include <array>
#    include <functional>
#    include <list>
#    include <memory>
#    include <string>
#    include <utility>
#    include <vector>

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

using Extents = ::gl::Extents;
using Offset  = ::gl::Offset;

struct KernelArg
{
    bool isSet;
    cl_uint index;
    size_t size;
    const void *valuePtr;
};

struct BufferBox
{
    BufferBox(const Offset &offset,
              const Extents &size,
              const size_t row_pitch,
              const size_t slice_pitch,
              const size_t element_size = 1)
        : mOrigin(offset),
          mSize(size),
          mRowPitch(row_pitch == 0 ? element_size * size.width : row_pitch),
          mSlicePitch(slice_pitch == 0 ? mRowPitch * size.height : slice_pitch),
          mElementSize(element_size)
    {}
    bool valid() const
    {
        return mSize.width != 0 && mSize.height != 0 && mSize.depth != 0 &&
               mRowPitch >= mSize.width * mElementSize && mSlicePitch >= mRowPitch * mSize.height &&
               mElementSize > 0;
    }
    bool operator==(const BufferBox &other) const
    {
        return (mOrigin == other.mOrigin && mSize == other.mSize && mRowPitch == other.mRowPitch &&
                mSlicePitch == other.mSlicePitch && mElementSize == other.mElementSize);
    }
    bool operator!=(const BufferBox &other) const { return !(*this == other); }

    size_t getRowOffset(size_t slice, size_t row) const
    {
        return ((mRowPitch * (mOrigin.y + row)) + (mOrigin.x * mElementSize)) +  // row offset
               (mSlicePitch * (mOrigin.z + slice));                              // slice offset
    }

    size_t getRowPitch() { return mRowPitch; }
    size_t getSlicePitch() { return mSlicePitch; }
    Offset mOrigin;
    Extents mSize;
    size_t mRowPitch;
    size_t mSlicePitch;
    size_t mElementSize;
};

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

    ImageDescriptor(MemObjectType type_,
                    size_t width_,
                    size_t height_,
                    size_t depth_,
                    size_t arraySize_,
                    size_t rowPitch_,
                    size_t slicePitch_,
                    cl_uint numMipLevels_,
                    cl_uint numSamples_)
        : type(type_),
          width(width_),
          height(height_),
          depth(depth_),
          arraySize(arraySize_),
          rowPitch(rowPitch_),
          slicePitch(slicePitch_),
          numMipLevels(numMipLevels_),
          numSamples(numSamples_)
    {
        if (type == MemObjectType::Image1D || type == MemObjectType::Image1D_Array ||
            type == MemObjectType::Image1D_Buffer)
        {
            height = 1;
        }
        if (type == MemObjectType::Image2D)
        {
            depth = 1;
        }
        if (!(type == MemObjectType::Image1D_Array || type == MemObjectType::Image2D_Array))
        {
            arraySize = 1;
        }
    }
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
        }
    }

    cl_uint workDimensions;
    GlobalWorkOffset globalWorkOffset;
    GlobalWorkSize globalWorkSize;
    WorkgroupSize localWorkSize;
    bool nullLocalWorkSize{false};
};

}  // namespace cl

#endif  // ANGLE_ENABLE_CL

#endif  // LIBANGLE_CLTYPES_H_
