#ifndef _TCULIBDRM_HPP
#define _TCULIBDRM_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 NVIDIA, Inc.
 * Copyright (c) 2022 The Khronos Group Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
* \file
* \brief Drm utilities.
*//*--------------------------------------------------------------------*/

#if DEQP_SUPPORT_DRM && !defined(CTS_USES_VULKANSC)

#include "deDynamicLibrary.hpp"

#include "deUniquePtr.hpp"

#include <xf86drm.h>
#include <xf86drmMode.h>

namespace tcu
{

class LibDrm : protected de::DynamicLibrary
{
    typedef void (*FdDeleter)(int *);
    typedef void (*PFNDRMMODEFREERESOURCESPROC)(drmModeRes *);
    typedef void (*PFNDRMMODEFREECONNECTORPROC)(drmModeConnector *);
    typedef void (*PFNDRMMODEFREEENCODERPROC)(drmModeEncoder *);

public:
    typedef de::UniquePtr<int, FdDeleter> FdPtr;
    typedef de::UniquePtr<drmModeRes, PFNDRMMODEFREERESOURCESPROC> ResPtr;
    typedef de::UniquePtr<drmModeConnector, PFNDRMMODEFREECONNECTORPROC> ConnectorPtr;
    typedef de::UniquePtr<drmModeEncoder, PFNDRMMODEFREEENCODERPROC> EncoderPtr;

    LibDrm(void);
    virtual ~LibDrm(void);

    drmDevicePtr *getDevices(int *pNumDevices) const;
    const char *findDeviceNode(drmDevicePtr *devices, int count, int64_t major, int64_t minor) const;
    void freeDevices(drmDevicePtr *devices, int count) const;

    FdPtr openFd(const char *node) const;
    ResPtr getResources(int fd) const;
    ConnectorPtr getConnector(int fd, uint32_t connectorId) const;
    EncoderPtr getEncoder(int fd, uint32_t encoderId) const;
    FdPtr createLease(int fd, const uint32_t *objects, int numObjects, int flags) const;
    int authMagic(int fd, drm_magic_t magic) const;

private:
    int intGetDevices(drmDevicePtr devices[], int maxDevices) const;

    static const char *libDrmFiles[];

    typedef int (*PFNDRMGETDEVICES2PROC)(uint32_t, drmDevicePtr[], int);
    typedef int (*PFNDRMGETDEVICESPROC)(drmDevicePtr[], int);
    typedef void (*PFNDRMFREEDEVICESPROC)(drmDevicePtr[], int);
    typedef drmModeRes *(*PFNDRMMODEGETRESOURCESPROC)(int);
    typedef drmModeConnector *(*PFNDRMMODEGETCONNECTORPROC)(int, uint32_t);
    typedef drmModeEncoder *(*PFNDRMMODEGETENCODERPROC)(int, uint32_t);
    typedef int (*PFNDRMMODECREATELEASEPROC)(int, const uint32_t *, int, int, uint32_t *);
    typedef int (*PFNDRMAUTHMAGIC)(int, drm_magic_t);
    PFNDRMGETDEVICES2PROC pGetDevices2;
    PFNDRMGETDEVICESPROC pGetDevices;
    PFNDRMFREEDEVICESPROC pFreeDevices;
    PFNDRMMODEGETRESOURCESPROC pGetResources;
    PFNDRMMODEGETCONNECTORPROC pGetConnector;
    PFNDRMMODEGETENCODERPROC pGetEncoder;
    PFNDRMMODEFREERESOURCESPROC pFreeResources;
    PFNDRMMODEFREECONNECTORPROC pFreeConnector;
    PFNDRMMODEFREEENCODERPROC pFreeEncoder;
    PFNDRMMODECREATELEASEPROC pCreateLease;
    PFNDRMAUTHMAGIC pAuthMagic;
};

} // namespace tcu

#endif // DEQP_SUPPORT_DRM && !defined (CTS_USES_VULKANSC)

#endif // _TCULIBDRM_HPP
