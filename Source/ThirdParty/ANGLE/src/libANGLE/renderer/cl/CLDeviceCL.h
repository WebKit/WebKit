//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLDeviceCL.h: Defines the class interface for CLDeviceCL, implementing CLDeviceImpl.

#ifndef LIBANGLE_RENDERER_CL_CLDEVICECL_H_
#define LIBANGLE_RENDERER_CL_CLDEVICECL_H_

#include "libANGLE/renderer/CLDeviceImpl.h"

namespace rx
{

class CLDeviceCL : public CLDeviceImpl
{
  public:
    ~CLDeviceCL() override;

    cl_device_id getNative() const;

    Info createInfo(cl::DeviceType type) const override;

    const cl::DeviceCaps &getCaps() const override;

    angle::Result createSubDevices(const cl_device_partition_property *properties,
                                   cl_uint numDevices,
                                   CreateFuncs &createFuncs,
                                   cl_uint *numDevicesRet) override;

  private:
    CLDeviceCL(const cl::Device &device, cl_device_id native);

    const cl_device_id mNative;

    friend class CLPlatformCL;

    // Object information is queried in OpenCL by providing allocated memory into which the
    // requested data is copied. If the size of the data is unknown, it can be queried first with an
    // additional call to the same function, but without requesting the data itself. This function
    // provides the functionality to request and validate the size and the data.
    template <typename T>
    bool GetDeviceInfo(cl::DeviceInfo name, std::vector<T> &vector) const
    {
        size_t size = 0u;
        if (mNative->getDispatch().clGetDeviceInfo(mNative, cl::ToCLenum(name), 0u, nullptr,
                                                   &size) == CL_SUCCESS &&
            (size % sizeof(T)) == 0u)  // size has to be a multiple of the data type
        {
            vector.resize(size / sizeof(T));
            if (mNative->getDispatch().clGetDeviceInfo(mNative, cl::ToCLenum(name), size,
                                                       vector.data(), nullptr) == CL_SUCCESS)
            {
                return true;
            }
        }
        ERR() << "Failed to query CL device info for " << name;
        return false;
    }

    // This queries the OpenCL device info for value types with known size
    template <typename T>
    bool GetDeviceInfo(cl::DeviceInfo name, T &value) const
    {
        if (mNative->getDispatch().clGetDeviceInfo(mNative, cl::ToCLenum(name), sizeof(T), &value,
                                                   nullptr) != CL_SUCCESS)
        {
            ERR() << "Failed to query CL device info for " << name;
            return false;
        }
        return true;
    }

    bool getInfoString(cl::DeviceInfo name, std::string *value) const;
};

inline cl_device_id CLDeviceCL::getNative() const
{
    return mNative;
}

inline const cl::DeviceCaps &CLDeviceCL::getCaps() const
{
    return mCaps;
}

}  // namespace rx

#endif  // LIBANGLE_RENDERER_CL_CLDEVICECL_H_
