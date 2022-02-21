//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLProgram.h: Defines the cl::Program class, which consists of a set of OpenCL kernels.

#ifndef LIBANGLE_CLPROGRAM_H_
#define LIBANGLE_CLPROGRAM_H_

#include "libANGLE/CLDevice.h"
#include "libANGLE/CLKernel.h"
#include "libANGLE/renderer/CLProgramImpl.h"

#include "common/Spinlock.h"
#include "common/SynchronizedValue.h"

#include <atomic>

namespace cl
{

class Program final : public _cl_program, public Object
{
  public:
    // Front end entry functions, only called from OpenCL entry points

    cl_int build(cl_uint numDevices,
                 const cl_device_id *deviceList,
                 const char *options,
                 ProgramCB pfnNotify,
                 void *userData);

    cl_int compile(cl_uint numDevices,
                   const cl_device_id *deviceList,
                   const char *options,
                   cl_uint numInputHeaders,
                   const cl_program *inputHeaders,
                   const char **headerIncludeNames,
                   ProgramCB pfnNotify,
                   void *userData);

    cl_int getInfo(ProgramInfo name, size_t valueSize, void *value, size_t *valueSizeRet) const;

    cl_int getBuildInfo(cl_device_id device,
                        ProgramBuildInfo name,
                        size_t valueSize,
                        void *value,
                        size_t *valueSizeRet) const;

    cl_kernel createKernel(const char *kernel_name, cl_int &errorCode);

    cl_int createKernels(cl_uint numKernels, cl_kernel *kernels, cl_uint *numKernelsRet);

  public:
    ~Program() override;

    Context &getContext();
    const Context &getContext() const;
    const DevicePtrs &getDevices() const;
    bool hasDevice(const _cl_device_id *device) const;

    bool isBuilding() const;
    bool hasAttachedKernels() const;

    template <typename T = rx::CLProgramImpl>
    T &getImpl() const;

    void callback();

  private:
    Program(Context &context, std::string &&source, cl_int &errorCode);
    Program(Context &context, const void *il, size_t length, cl_int &errorCode);

    Program(Context &context,
            DevicePtrs &&devices,
            const size_t *lengths,
            const unsigned char **binaries,
            cl_int *binaryStatus,
            cl_int &errorCode);

    Program(Context &context, DevicePtrs &&devices, const char *kernelNames, cl_int &errorCode);

    Program(Context &context,
            const DevicePtrs &devices,
            const char *options,
            const cl::ProgramPtrs &inputPrograms,
            ProgramCB pfnNotify,
            void *userData,
            cl_int &errorCode);

    using CallbackData = std::pair<ProgramCB, void *>;

    const ContextPtr mContext;
    const DevicePtrs mDevices;
    const std::string mIL;

    // mCallback might be accessed from implementation initialization
    // and needs to be initialized first.
    angle::SynchronizedValue<CallbackData, angle::Spinlock> mCallback;
    std::atomic<cl_uint> mNumAttachedKernels;

    const rx::CLProgramImpl::Ptr mImpl;
    const std::string mSource;

    friend class Kernel;
    friend class Object;
};

inline Context &Program::getContext()
{
    return *mContext;
}

inline const Context &Program::getContext() const
{
    return *mContext;
}

inline const DevicePtrs &Program::getDevices() const
{
    return mDevices;
}

inline bool Program::hasDevice(const _cl_device_id *device) const
{
    return std::find(mDevices.cbegin(), mDevices.cend(), device) != mDevices.cend();
}

inline bool Program::isBuilding() const
{
    return mCallback->first != nullptr;
}

inline bool Program::hasAttachedKernels() const
{
    return mNumAttachedKernels != 0u;
}

template <typename T>
inline T &Program::getImpl() const
{
    return static_cast<T &>(*mImpl);
}

}  // namespace cl

#endif  // LIBANGLE_CLPROGRAM_H_
