//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLKernel.h: Defines the cl::Kernel class, which is a function declared in an OpenCL program.

#ifndef LIBANGLE_CLKERNEL_H_
#define LIBANGLE_CLKERNEL_H_

#include "libANGLE/CLObject.h"
#include "libANGLE/renderer/CLKernelImpl.h"

namespace cl
{

class Kernel final : public _cl_kernel, public Object
{
  public:
    // Front end entry functions, only called from OpenCL entry points

    cl_int setArg(cl_uint argIndex, size_t argSize, const void *argValue);

    cl_int getInfo(KernelInfo name, size_t valueSize, void *value, size_t *valueSizeRet) const;

    cl_int getWorkGroupInfo(cl_device_id device,
                            KernelWorkGroupInfo name,
                            size_t valueSize,
                            void *value,
                            size_t *valueSizeRet) const;

    cl_int getArgInfo(cl_uint argIndex,
                      KernelArgInfo name,
                      size_t valueSize,
                      void *value,
                      size_t *valueSizeRet) const;

  public:
    ~Kernel() override;

    const Program &getProgram() const;
    const rx::CLKernelImpl::Info &getInfo() const;

    template <typename T = rx::CLKernelImpl>
    T &getImpl() const;

  private:
    Kernel(Program &program, const char *name, cl_int &errorCode);
    Kernel(Program &program, const rx::CLKernelImpl::CreateFunc &createFunc, cl_int &errorCode);

    const ProgramPtr mProgram;
    const rx::CLKernelImpl::Ptr mImpl;
    const rx::CLKernelImpl::Info mInfo;

    friend class Object;
    friend class Program;
};

inline const Program &Kernel::getProgram() const
{
    return *mProgram;
}

inline const rx::CLKernelImpl::Info &Kernel::getInfo() const
{
    return mInfo;
}

template <typename T>
inline T &Kernel::getImpl() const
{
    return static_cast<T &>(*mImpl);
}

}  // namespace cl

#endif  // LIBANGLE_CLKERNEL_H_
