//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLImage.cpp: Implements the cl::Image class.

#include "libANGLE/CLImage.h"

#include "libANGLE/cl_utils.h"

#include <cstring>

namespace cl
{

bool Image::IsTypeValid(MemObjectType imageType)
{
    switch (imageType)
    {
        case MemObjectType::Image1D:
        case MemObjectType::Image2D:
        case MemObjectType::Image3D:
        case MemObjectType::Image1D_Array:
        case MemObjectType::Image2D_Array:
        case MemObjectType::Image1D_Buffer:
            break;
        default:
            return false;
    }
    return true;
}

cl_int Image::getInfo(ImageInfo name, size_t valueSize, void *value, size_t *valueSizeRet) const
{
    size_t valSizeT       = 0u;
    void *valPointer      = nullptr;
    const void *copyValue = nullptr;
    size_t copySize       = 0u;

    switch (name)
    {
        case ImageInfo::Format:
            copyValue = &mFormat;
            copySize  = sizeof(mFormat);
            break;
        case ImageInfo::ElementSize:
            valSizeT  = GetElementSize(mFormat);
            copyValue = &valSizeT;
            copySize  = sizeof(valSizeT);
            break;
        case ImageInfo::RowPitch:
            copyValue = &mDesc.rowPitch;
            copySize  = sizeof(mDesc.rowPitch);
            break;
        case ImageInfo::SlicePitch:
            copyValue = &mDesc.slicePitch;
            copySize  = sizeof(mDesc.slicePitch);
            break;
        case ImageInfo::Width:
            copyValue = &mDesc.width;
            copySize  = sizeof(mDesc.width);
            break;
        case ImageInfo::Height:
            copyValue = &mDesc.height;
            copySize  = sizeof(mDesc.height);
            break;
        case ImageInfo::Depth:
            copyValue = &mDesc.depth;
            copySize  = sizeof(mDesc.depth);
            break;
        case ImageInfo::ArraySize:
            copyValue = &mDesc.arraySize;
            copySize  = sizeof(mDesc.arraySize);
            break;
        case ImageInfo::Buffer:
            valPointer = Memory::CastNative(mParent.get());
            copyValue  = &valPointer;
            copySize   = sizeof(valPointer);
            break;
        case ImageInfo::NumMipLevels:
            copyValue = &mDesc.numMipLevels;
            copySize  = sizeof(mDesc.numMipLevels);
            break;
        case ImageInfo::NumSamples:
            copyValue = &mDesc.numSamples;
            copySize  = sizeof(mDesc.numSamples);
            break;
        default:
            return CL_INVALID_VALUE;
    }

    if (value != nullptr)
    {
        // CL_INVALID_VALUE if size in bytes specified by param_value_size is < size of return type
        // as described in the Image Object Queries table and param_value is not NULL.
        if (valueSize < copySize)
        {
            return CL_INVALID_VALUE;
        }
        if (copyValue != nullptr)
        {
            std::memcpy(value, copyValue, copySize);
        }
    }
    if (valueSizeRet != nullptr)
    {
        *valueSizeRet = copySize;
    }
    return CL_SUCCESS;
}

Image::~Image() = default;

bool Image::isRegionValid(const size_t origin[3], const size_t region[3]) const
{
    switch (getType())
    {
        case MemObjectType::Image1D:
        case MemObjectType::Image1D_Buffer:
            return origin[0] + region[0] <= mDesc.width;
        case MemObjectType::Image2D:
            return origin[0] + region[0] <= mDesc.width && origin[1] + region[1] <= mDesc.height;
        case MemObjectType::Image3D:
            return origin[0] + region[0] <= mDesc.width && origin[1] + region[1] <= mDesc.height &&
                   origin[2] + region[2] <= mDesc.depth;
        case MemObjectType::Image1D_Array:
            return origin[0] + region[0] <= mDesc.width && origin[1] + region[1] <= mDesc.arraySize;
        case MemObjectType::Image2D_Array:
            return origin[0] + region[0] <= mDesc.width && origin[1] + region[1] <= mDesc.height &&
                   origin[2] + region[2] <= mDesc.arraySize;
        default:
            ASSERT(false);
            break;
    }
    return false;
}

Image::Image(Context &context,
             PropArray &&properties,
             MemFlags flags,
             const cl_image_format &format,
             const ImageDescriptor &desc,
             Memory *parent,
             void *hostPtr,
             cl_int &errorCode)
    : Memory(*this,
             context,
             std::move(properties),
             flags,
             format,
             desc,
             parent,
             hostPtr,
             errorCode),
      mFormat(format),
      mDesc(desc)
{}

}  // namespace cl
