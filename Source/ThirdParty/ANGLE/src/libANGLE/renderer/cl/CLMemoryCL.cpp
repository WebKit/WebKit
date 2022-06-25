//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLMemoryCL.cpp: Implements the class methods for CLMemoryCL.

#include "libANGLE/renderer/cl/CLMemoryCL.h"

#include "libANGLE/renderer/cl/CLContextCL.h"

#include "libANGLE/CLBuffer.h"
#include "libANGLE/CLContext.h"

namespace rx
{

CLMemoryCL::CLMemoryCL(const cl::Memory &memory, cl_mem native)
    : CLMemoryImpl(memory), mNative(native)
{
    memory.getContext().getImpl<CLContextCL>().mData->mMemories.emplace(memory.getNative());
}

CLMemoryCL::~CLMemoryCL()
{
    const size_t numRemoved =
        mMemory.getContext().getImpl<CLContextCL>().mData->mMemories.erase(mMemory.getNative());
    ASSERT(numRemoved == 1u);

    if (mNative->getDispatch().clReleaseMemObject(mNative) != CL_SUCCESS)
    {
        ERR() << "Error while releasing CL memory object";
    }
}

size_t CLMemoryCL::getSize(cl_int &errorCode) const
{
    size_t size = 0u;
    errorCode = mNative->getDispatch().clGetMemObjectInfo(mNative, CL_MEM_SIZE, sizeof(size), &size,
                                                          nullptr);
    if (errorCode != CL_SUCCESS)
    {
        return 0u;
    }
    return size;
}

CLMemoryImpl::Ptr CLMemoryCL::createSubBuffer(const cl::Buffer &buffer,
                                              cl::MemFlags flags,
                                              size_t size,
                                              cl_int &errorCode)
{
    const cl_buffer_region region = {buffer.getOffset(), size};
    const cl_mem nativeBuffer     = mNative->getDispatch().clCreateSubBuffer(
        mNative, flags.get(), CL_BUFFER_CREATE_TYPE_REGION, &region, &errorCode);
    return CLMemoryImpl::Ptr(nativeBuffer != nullptr ? new CLMemoryCL(buffer, nativeBuffer)
                                                     : nullptr);
}

}  // namespace rx
