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

#include "tcuLibDrm.hpp"

#include "tcuDefs.hpp"

#include "deMemory.h"

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#if !defined(__FreeBSD__)
// major() and minor() are defined in sys/types.h on FreeBSD, and in
// sys/sysmacros.h on Linux and Solaris.
#include <sys/sysmacros.h>
#endif // !defined(__FreeBSD__)

namespace tcu
{

LibDrm::LibDrm(void) : DynamicLibrary(libDrmFiles)
{
    pGetDevices2   = (PFNDRMGETDEVICES2PROC)getFunction("drmGetDevices2");
    pGetDevices    = (PFNDRMGETDEVICESPROC)getFunction("drmGetDevices");
    pFreeDevices   = (PFNDRMFREEDEVICESPROC)getFunction("drmFreeDevices");
    pGetResources  = (PFNDRMMODEGETRESOURCESPROC)getFunction("drmModeGetResources");
    pFreeResources = (PFNDRMMODEFREERESOURCESPROC)getFunction("drmModeFreeResources");
    pGetConnector  = (PFNDRMMODEGETCONNECTORPROC)getFunction("drmModeGetConnector");
    pFreeConnector = (PFNDRMMODEFREECONNECTORPROC)getFunction("drmModeFreeConnector");
    pGetEncoder    = (PFNDRMMODEGETENCODERPROC)getFunction("drmModeGetEncoder");
    pFreeEncoder   = (PFNDRMMODEFREEENCODERPROC)getFunction("drmModeFreeEncoder");
    pCreateLease   = (PFNDRMMODECREATELEASEPROC)getFunction("drmModeCreateLease");
    pAuthMagic     = (PFNDRMAUTHMAGIC)getFunction("drmAuthMagic");

    /* libdrm did not add this API until 2.4.65, return NotSupported if it's too old. */
    if (!pGetDevices2 && !pGetDevices)
        TCU_THROW(NotSupportedError, "Could not load a valid drmGetDevices() variant from libdrm");
    if (!pFreeDevices)
        TCU_THROW(NotSupportedError, "Could not load drmFreeDevices() from libdrm");
    if (!pGetResources)
        TCU_FAIL("Could not load drmModeGetResources() from libdrm");
    if (!pFreeResources)
        TCU_FAIL("Could not load drmModeFreeResources() from libdrm");
    if (!pGetConnector)
        TCU_FAIL("Could not load drmModeGetConnector() from libdrm");
    if (!pFreeConnector)
        TCU_FAIL("Could not load drmModeFreeConnector() from libdrm");
    if (!pGetEncoder)
        TCU_FAIL("Could not load drmModeGetEncoder() from libdrm");
    if (!pFreeEncoder)
        TCU_FAIL("Could not load drmModeFreeEncoder() from libdrm");
    if (!pCreateLease)
        TCU_FAIL("Could not load drmModeCreateLease() from libdrm");
    if (!pAuthMagic)
        TCU_FAIL("Could not load drmAuthMagic() from libdrm");
}

LibDrm::~LibDrm(void)
{
}

drmDevicePtr *LibDrm::getDevices(int *pNumDevices) const
{
    *pNumDevices = intGetDevices(nullptr, 0);

    if (*pNumDevices < 0)
        TCU_THROW(NotSupportedError, "Failed to query number of DRM devices in system");

    if (*pNumDevices == 0)
        return nullptr;

    drmDevicePtr *devs = new drmDevicePtr[*pNumDevices];

    *pNumDevices = intGetDevices(devs, *pNumDevices);

    if (*pNumDevices < 0)
    {
        delete[] devs;
        TCU_FAIL("Failed to query list of DRM devices in system");
    }

    return devs;
}

const char *LibDrm::findDeviceNode(drmDevicePtr *devices, int count, int64_t major, int64_t minor) const
{
    for (int i = 0; i < count; i++)
    {
        for (int j = 0; j < DRM_NODE_MAX; j++)
        {
            if (!(devices[i]->available_nodes & (1 << j)))
                continue;

            struct stat statBuf;
            deMemset(&statBuf, 0, sizeof(statBuf));
            int res = stat(devices[i]->nodes[j], &statBuf);

            if (res || !(statBuf.st_mode & S_IFCHR))
                continue;

            if (major == major(statBuf.st_rdev) && minor == minor(statBuf.st_rdev))
            {
                return devices[i]->nodes[j];
            }
        }
    }

    return nullptr;
}

void LibDrm::freeDevices(drmDevicePtr *devices, int count) const
{
    pFreeDevices(devices, count);
    delete[] devices;
}

static void closeAndDeleteFd(int *fd)
{
    if (fd)
    {
        close(*fd);
        delete fd;
    }
}

LibDrm::FdPtr LibDrm::openFd(const char *node) const
{
    int fd = open(node, O_RDWR);
    if (fd < 0)
        return FdPtr(nullptr);
    else
        return FdPtr(new int{fd}, closeAndDeleteFd);
}

LibDrm::ResPtr LibDrm::getResources(int fd) const
{
    return ResPtr(pGetResources(fd), pFreeResources);
}

LibDrm::ConnectorPtr LibDrm::getConnector(int fd, uint32_t connectorId) const
{
    return ConnectorPtr(pGetConnector(fd, connectorId), pFreeConnector);
}

LibDrm::EncoderPtr LibDrm::getEncoder(int fd, uint32_t encoderId) const
{
    return EncoderPtr(pGetEncoder(fd, encoderId), pFreeEncoder);
}

LibDrm::FdPtr LibDrm::createLease(int fd, const uint32_t *objects, int numObjects, int flags) const
{
    uint32_t leaseId;
    int leaseFd = pCreateLease(fd, objects, numObjects, flags, &leaseId);
    if (leaseFd < 0)
        return FdPtr(nullptr);
    else
        return FdPtr(new int{leaseFd}, closeAndDeleteFd);
}

int LibDrm::authMagic(int fd, drm_magic_t magic) const
{
    return pAuthMagic(fd, magic);
}

int LibDrm::intGetDevices(drmDevicePtr devices[], int maxDevices) const
{
    if (pGetDevices2)
        return pGetDevices2(0, devices, maxDevices);
    else
        return pGetDevices(devices, maxDevices);
}

const char *LibDrm::libDrmFiles[] = {"libdrm.so.2", "libdrm.so", nullptr};

} // namespace tcu

#endif // DEQP_SUPPORT_DRM && !defined (CTS_USES_VULKANSC)
