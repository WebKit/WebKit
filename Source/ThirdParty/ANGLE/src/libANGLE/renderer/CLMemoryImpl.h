//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLMemoryImpl.h: Defines the abstract rx::CLMemoryImpl class.

#ifndef LIBANGLE_RENDERER_CLMEMORYIMPL_H_
#define LIBANGLE_RENDERER_CLMEMORYIMPL_H_

#include "libANGLE/renderer/CLtypes.h"

namespace rx
{

class CLMemoryImpl : angle::NonCopyable
{
  public:
    using Ptr = std::unique_ptr<CLMemoryImpl>;

    CLMemoryImpl(const cl::Memory &memory);
    virtual ~CLMemoryImpl();

    virtual size_t getSize(cl_int &errorCode) const = 0;

    virtual CLMemoryImpl::Ptr createSubBuffer(const cl::Buffer &buffer,
                                              cl::MemFlags flags,
                                              size_t size,
                                              cl_int &errorCode) = 0;

  protected:
    const cl::Memory &mMemory;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_CLMEMORYIMPL_H_
