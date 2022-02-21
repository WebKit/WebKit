//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLPlatformCL.h: Defines the class interface for CLPlatformCL, implementing CLPlatformImpl.

#ifndef LIBANGLE_RENDERER_CL_CLPLATFORMCL_H_
#define LIBANGLE_RENDERER_CL_CLPLATFORMCL_H_

#include "libANGLE/renderer/CLPlatformImpl.h"

namespace rx
{

class CLPlatformCL : public CLPlatformImpl
{
  public:
    ~CLPlatformCL() override;

    cl_platform_id getNative() const;

    Info createInfo() const override;
    CLDeviceImpl::CreateDatas createDevices() const override;

    CLContextImpl::Ptr createContext(cl::Context &context,
                                     const cl::DevicePtrs &devices,
                                     bool userSync,
                                     cl_int &errorCode) override;

    CLContextImpl::Ptr createContextFromType(cl::Context &context,
                                             cl::DeviceType deviceType,
                                             bool userSync,
                                             cl_int &errorCode) override;

    cl_int unloadCompiler() override;

    static void Initialize(CreateFuncs &createFuncs, bool isIcd);

  private:
    CLPlatformCL(const cl::Platform &platform, cl_platform_id native);

    const cl_platform_id mNative;

    friend class CLContextCL;
};

inline cl_platform_id CLPlatformCL::getNative() const
{
    return mNative;
}

}  // namespace rx

#endif  // LIBANGLE_RENDERER_CL_CLPLATFORMCL_H_
