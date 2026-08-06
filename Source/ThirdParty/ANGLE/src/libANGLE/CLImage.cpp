//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLImage.cpp: Implements the cl::Image class.
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "CL/cl_half.h"

#include "libANGLE/CLContext.h"
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

angle::Result Image::getInfo(ImageInfo name,
                             size_t valueSize,
                             void *value,
                             size_t *valueSizeRet) const
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
            copyValue = Is1DImage(mDesc.type) ? &valSizeT : &mDesc.height;
            copySize  = sizeof(mDesc.height);
            break;
        case ImageInfo::Depth:
            copyValue = Is3DImage(mDesc.type) ? &mDesc.depth : &valSizeT;
            copySize  = sizeof(mDesc.depth);
            break;
        case ImageInfo::ArraySize:
            copyValue = IsArrayType(mDesc.type) ? &mDesc.arraySize : &valSizeT;
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
            ANGLE_CL_RETURN_ERROR(CL_INVALID_VALUE);
    }

    if (value != nullptr)
    {
        // CL_INVALID_VALUE if size in bytes specified by param_value_size is < size of return type
        // as described in the Image Object Queries table and param_value is not NULL.
        if (valueSize < copySize)
        {
            ANGLE_CL_RETURN_ERROR(CL_INVALID_VALUE);
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
    return angle::Result::Continue;
}

PixelColor Image::packPixels(const void *fillColor) const
{
    PixelColor packedColor;
    const cl_image_format &fmt = getFormat();
    const size_t channelCount  = GetChannelCount(fmt.image_channel_order);
    ChannelMapping swizzle     = GetChannelOrderMapping(fmt.image_channel_order);

    switch (fmt.image_channel_data_type)
    {
        case CL_UNORM_INT8:
        {
            const float *src = static_cast<const float *>(fillColor);
            for (size_t i = 0; i < channelCount; ++i)
            {
                packedColor.u8[i] =
                    gl::floatToNormalized<uint8_t>(gl::clamp(src[swizzle[i]], 0.0f, 1.0f));
            }
            break;
        }
        case CL_UNORM_INT16:
        {
            const float *src = static_cast<const float *>(fillColor);
            for (size_t i = 0; i < channelCount; ++i)
            {
                packedColor.u16[i] =
                    gl::floatToNormalized<uint16_t>(gl::clamp(src[swizzle[i]], 0.0f, 1.0f));
            }
            break;
        }
        case CL_SNORM_INT8:
        {
            const float *src = static_cast<const float *>(fillColor);
            for (size_t i = 0; i < channelCount; ++i)
            {
                packedColor.s8[i] =
                    gl::floatToNormalized<int8_t>(gl::clamp(src[swizzle[i]], -1.0f, 1.0f));
            }
            break;
        }
        case CL_SNORM_INT16:
        {
            const float *src = static_cast<const float *>(fillColor);
            for (size_t i = 0; i < channelCount; ++i)
            {
                packedColor.s16[i] =
                    gl::floatToNormalized<int16_t>(gl::clamp(src[swizzle[i]], -1.0f, 1.0f));
            }
            break;
        }
        case CL_SIGNED_INT8:
        case CL_SIGNED_INT16:
        case CL_SIGNED_INT32:
        {
            const int *src = static_cast<const int *>(fillColor);
            for (size_t i = 0; i < channelCount; ++i)
            {
                if (fmt.image_channel_data_type == CL_SIGNED_INT8)
                {
                    packedColor.s8[i] = gl::clampCast<int8_t>(src[swizzle[i]]);
                }
                else if (fmt.image_channel_data_type == CL_SIGNED_INT16)
                {
                    packedColor.s16[i] = gl::clampCast<int16_t>(src[swizzle[i]]);
                }
                else
                {
                    packedColor.s32[i] = src[swizzle[i]];
                }
            }
            break;
        }
        case CL_UNSIGNED_INT8:
        case CL_UNSIGNED_INT16:
        case CL_UNSIGNED_INT32:
        {
            const unsigned *src = static_cast<const unsigned *>(fillColor);
            for (size_t i = 0; i < channelCount; ++i)
            {
                if (fmt.image_channel_data_type == CL_UNSIGNED_INT8)
                {
                    packedColor.u8[i] = gl::clampCast<uint8_t>(src[swizzle[i]]);
                }
                else if (fmt.image_channel_data_type == CL_UNSIGNED_INT16)
                {
                    packedColor.u16[i] = gl::clampCast<uint16_t>(src[swizzle[i]]);
                }
                else
                {
                    packedColor.u32[i] = src[swizzle[i]];
                }
            }
            break;
        }
        case CL_HALF_FLOAT:
        {
            const float *src = static_cast<const float *>(fillColor);
            for (size_t i = 0; i < channelCount; ++i)
            {
                packedColor.fp16[i] = cl_half_from_float(src[swizzle[i]], CL_HALF_RTE);
            }
            break;
        }
        case CL_FLOAT:
        {
            const float *src = static_cast<const float *>(fillColor);
            for (size_t i = 0; i < channelCount; ++i)
            {
                packedColor.fp32[i] = src[swizzle[i]];
            }
            break;
        }
        default:
            UNIMPLEMENTED();
            break;
    }
    return packedColor;
}

Image::~Image() = default;

bool Image::isRegionValid(const cl::Offset &origin, const cl::Extents &region) const
{
    switch (getType())
    {
        case MemObjectType::Image1D:
        case MemObjectType::Image1D_Buffer:
            return origin.x + region.width <= mDesc.width;
        case MemObjectType::Image2D:
            return origin.x + region.width <= mDesc.width &&
                   origin.y + region.height <= mDesc.height;
        case MemObjectType::Image3D:
            return origin.x + region.width <= mDesc.width &&
                   origin.y + region.height <= mDesc.height &&
                   origin.z + region.depth <= mDesc.depth;
        case MemObjectType::Image1D_Array:
            return origin.x + region.width <= mDesc.width &&
                   origin.y + region.height <= mDesc.arraySize;
        case MemObjectType::Image2D_Array:
            return origin.x + region.width <= mDesc.width &&
                   origin.y + region.height <= mDesc.height &&
                   origin.z + region.depth <= mDesc.arraySize;
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
             void *hostPtr)
    : Memory(context, std::move(properties), flags, parent, hostPtr), mFormat(format), mDesc(desc)
{
    mSize = getSliceSize() * getDepth() * getArraySize();
    ANGLE_CL_IMPL_TRY(context.getImpl().createImage(*this, hostPtr, &mImpl));
}

}  // namespace cl
