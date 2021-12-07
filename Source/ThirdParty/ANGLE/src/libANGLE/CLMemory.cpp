//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLMemory.cpp: Implements the cl::Memory class.

#include "libANGLE/CLMemory.h"

#include "libANGLE/CLBuffer.h"
#include "libANGLE/CLContext.h"

#include <cstring>

namespace cl
{

namespace
{

MemFlags InheritMemFlags(MemFlags flags, Memory *parent)
{
    if (parent != nullptr)
    {
        const MemFlags parentFlags = parent->getFlags();
        const MemFlags access(CL_MEM_READ_WRITE | CL_MEM_READ_ONLY | CL_MEM_WRITE_ONLY);
        const MemFlags hostAccess(CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_READ_ONLY |
                                  CL_MEM_HOST_NO_ACCESS);
        const MemFlags hostPtrFlags(CL_MEM_USE_HOST_PTR | CL_MEM_ALLOC_HOST_PTR |
                                    CL_MEM_COPY_HOST_PTR);
        if (flags.isNotSet(access))
        {
            flags.set(parentFlags.mask(access));
        }
        if (flags.isNotSet(hostAccess))
        {
            flags.set(parentFlags.mask(hostAccess));
        }
        flags.set(parentFlags.mask(hostPtrFlags));
    }
    return flags;
}

}  // namespace

cl_int Memory::setDestructorCallback(MemoryCB pfnNotify, void *userData)
{
    mDestructorCallbacks->emplace(pfnNotify, userData);
    return CL_SUCCESS;
}

cl_int Memory::getInfo(MemInfo name, size_t valueSize, void *value, size_t *valueSizeRet) const
{
    static_assert(
        std::is_same<cl_uint, cl_bool>::value && std::is_same<cl_uint, cl_mem_object_type>::value,
        "OpenCL type mismatch");

    cl_uint valUInt       = 0u;
    void *valPointer      = nullptr;
    const void *copyValue = nullptr;
    size_t copySize       = 0u;

    switch (name)
    {
        case MemInfo::Type:
            valUInt   = ToCLenum(getType());
            copyValue = &valUInt;
            copySize  = sizeof(valUInt);
            break;
        case MemInfo::Flags:
            copyValue = &mFlags;
            copySize  = sizeof(mFlags);
            break;
        case MemInfo::Size:
            copyValue = &mSize;
            copySize  = sizeof(mSize);
            break;
        case MemInfo::HostPtr:
            copyValue = &mHostPtr;
            copySize  = sizeof(mHostPtr);
            break;
        case MemInfo::MapCount:
            valUInt   = mMapCount;
            copyValue = &valUInt;
            copySize  = sizeof(valUInt);
            break;
        case MemInfo::ReferenceCount:
            valUInt   = getRefCount();
            copyValue = &valUInt;
            copySize  = sizeof(valUInt);
            break;
        case MemInfo::Context:
            valPointer = mContext->getNative();
            copyValue  = &valPointer;
            copySize   = sizeof(valPointer);
            break;
        case MemInfo::AssociatedMemObject:
            valPointer = Memory::CastNative(mParent.get());
            copyValue  = &valPointer;
            copySize   = sizeof(valPointer);
            break;
        case MemInfo::Offset:
            copyValue = &mOffset;
            copySize  = sizeof(mOffset);
            break;
        case MemInfo::UsesSVM_Pointer:
            valUInt   = CL_FALSE;  // TODO(jplate) Check for SVM pointer anglebug.com/6002
            copyValue = &valUInt;
            copySize  = sizeof(valUInt);
            break;
        case MemInfo::Properties:
            copyValue = mProperties.data();
            copySize  = mProperties.size() * sizeof(decltype(mProperties)::value_type);
            break;
        default:
            return CL_INVALID_VALUE;
    }

    if (value != nullptr)
    {
        // CL_INVALID_VALUE if size in bytes specified by param_value_size is < size of return type
        // as described in the Memory Object Info table and param_value is not NULL.
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

Memory::~Memory()
{
    std::stack<CallbackData> callbacks;
    mDestructorCallbacks->swap(callbacks);
    while (!callbacks.empty())
    {
        const MemoryCB callback = callbacks.top().first;
        void *const userData    = callbacks.top().second;
        callbacks.pop();
        callback(this, userData);
    }
}

Memory::Memory(const Buffer &buffer,
               Context &context,
               PropArray &&properties,
               MemFlags flags,
               size_t size,
               void *hostPtr,
               cl_int &errorCode)
    : mContext(&context),
      mProperties(std::move(properties)),
      mFlags(flags),
      mHostPtr(flags.isSet(CL_MEM_USE_HOST_PTR) ? hostPtr : nullptr),
      mImpl(context.getImpl().createBuffer(buffer, size, hostPtr, errorCode)),
      mSize(size),
      mMapCount(0u)
{}

Memory::Memory(const Buffer &buffer,
               Buffer &parent,
               MemFlags flags,
               size_t offset,
               size_t size,
               cl_int &errorCode)
    : mContext(parent.mContext),
      mFlags(InheritMemFlags(flags, &parent)),
      mHostPtr(parent.mHostPtr != nullptr ? static_cast<char *>(parent.mHostPtr) + offset
                                          : nullptr),
      mParent(&parent),
      mOffset(offset),
      mImpl(parent.mImpl->createSubBuffer(buffer, flags, size, errorCode)),
      mSize(size),
      mMapCount(0u)
{}

Memory::Memory(const Image &image,
               Context &context,
               PropArray &&properties,
               MemFlags flags,
               const cl_image_format &format,
               const ImageDescriptor &desc,
               Memory *parent,
               void *hostPtr,
               cl_int &errorCode)
    : mContext(&context),
      mProperties(std::move(properties)),
      mFlags(InheritMemFlags(flags, parent)),
      mHostPtr(flags.isSet(CL_MEM_USE_HOST_PTR) ? hostPtr : nullptr),
      mParent(parent),
      mImpl(context.getImpl().createImage(image, flags, format, desc, hostPtr, errorCode)),
      mSize(mImpl ? mImpl->getSize(errorCode) : 0u),
      mMapCount(0u)
{}

}  // namespace cl
