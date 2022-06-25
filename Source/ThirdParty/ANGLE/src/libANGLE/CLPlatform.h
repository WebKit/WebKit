//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLPlatform.h: Defines the cl::Platform class, which provides information about platform-specific
// OpenCL features.

#ifndef LIBANGLE_CLPLATFORM_H_
#define LIBANGLE_CLPLATFORM_H_

#include "libANGLE/CLObject.h"
#include "libANGLE/renderer/CLPlatformImpl.h"

#include "anglebase/no_destructor.h"

namespace cl
{

class Platform final : public _cl_platform_id, public Object
{
  public:
    // Front end entry functions, only called from OpenCL entry points

    static void Initialize(const cl_icd_dispatch &dispatch,
                           rx::CLPlatformImpl::CreateFuncs &&createFuncs);

    static Platform *GetDefault();
    static Platform *CastOrDefault(cl_platform_id platform);
    static bool IsValidOrDefault(const _cl_platform_id *platform);

    static cl_int GetPlatformIDs(cl_uint numEntries,
                                 cl_platform_id *platforms,
                                 cl_uint *numPlatforms);

    cl_int getInfo(PlatformInfo name, size_t valueSize, void *value, size_t *valueSizeRet) const;

    cl_int getDeviceIDs(DeviceType deviceType,
                        cl_uint numEntries,
                        cl_device_id *devices,
                        cl_uint *numDevices) const;

    static cl_context CreateContext(const cl_context_properties *properties,
                                    cl_uint numDevices,
                                    const cl_device_id *devices,
                                    ContextErrorCB notify,
                                    void *userData,
                                    cl_int &errorCode);

    static cl_context CreateContextFromType(const cl_context_properties *properties,
                                            DeviceType deviceType,
                                            ContextErrorCB notify,
                                            void *userData,
                                            cl_int &errorCode);

    cl_int unloadCompiler();

  public:
    ~Platform() override;

    const rx::CLPlatformImpl::Info &getInfo() const;
    cl_version getVersion() const;
    bool isVersionOrNewer(cl_uint major, cl_uint minor) const;
    const DevicePtrs &getDevices() const;

    template <typename T = rx::CLPlatformImpl>
    T &getImpl() const;

    static const PlatformPtrs &GetPlatforms();

    static constexpr const char *GetVendor();

  private:
    explicit Platform(const rx::CLPlatformImpl::CreateFunc &createFunc);

    DevicePtrs createDevices(rx::CLDeviceImpl::CreateDatas &&createDatas);

    static PlatformPtrs &GetPointers();

    const rx::CLPlatformImpl::Ptr mImpl;
    const rx::CLPlatformImpl::Info mInfo;
    const DevicePtrs mDevices;

    static constexpr char kVendor[]    = "ANGLE";
    static constexpr char kIcdSuffix[] = "ANGLE";
};

inline Platform *Platform::GetDefault()
{
    return GetPlatforms().empty() ? nullptr : GetPlatforms().front().get();
}

inline Platform *Platform::CastOrDefault(cl_platform_id platform)
{
    return platform != nullptr ? &platform->cast<Platform>() : GetDefault();
}

// Our CL implementation defines that a nullptr value chooses the platform that we provide as
// default, so this function returns true for a nullptr value if a default platform exists.
inline bool Platform::IsValidOrDefault(const _cl_platform_id *platform)
{
    return platform != nullptr ? IsValid(platform) : GetDefault() != nullptr;
}

inline const rx::CLPlatformImpl::Info &Platform::getInfo() const
{
    return mInfo;
}

inline cl_version Platform::getVersion() const
{
    return mInfo.version;
}

inline bool Platform::isVersionOrNewer(cl_uint major, cl_uint minor) const
{
    return mInfo.version >= CL_MAKE_VERSION(major, minor, 0u);
}

inline const DevicePtrs &Platform::getDevices() const
{
    return mDevices;
}

template <typename T>
inline T &Platform::getImpl() const
{
    return static_cast<T &>(*mImpl);
}

inline const PlatformPtrs &Platform::GetPlatforms()
{
    return GetPointers();
}

constexpr const char *Platform::GetVendor()
{
    return kVendor;
}

inline PlatformPtrs &Platform::GetPointers()
{
    static angle::base::NoDestructor<PlatformPtrs> sPointers;
    return *sPointers;
}

}  // namespace cl

#endif  // LIBANGLE_CLPLATFORM_H_
