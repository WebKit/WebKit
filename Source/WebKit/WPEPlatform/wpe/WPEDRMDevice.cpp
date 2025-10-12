/*
 * Copyright (C) 2025 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WPEDRMDevice.h"

#include "WPEDRMDevicePrivate.h"
#include <wtf/FastMalloc.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>

#if USE(LIBDRM)
#include <xf86drm.h>
#endif

/**
 * WPEDRMDevice:
 *
 * Boxed type representing a DRM device.
 */
struct _WPEDRMDevice {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(_WPEDRMDevice);

    _WPEDRMDevice(const char* primaryNode, const char *renderNode)
        : primaryNode(primaryNode)
        , renderNode(renderNode)
    {
    }

    CString primaryNode;
    CString renderNode;

    int referenceCount { 1 };
};
G_DEFINE_BOXED_TYPE(WPEDRMDevice, wpe_drm_device, wpe_drm_device_ref, wpe_drm_device_unref)

// If deviceFilename is nullpr, it returns the first device having a render node if found or the first device.
GRefPtr<WPEDRMDevice> wpeDRMDeviceCreateForDevice(const char* deviceFilename)
{
#if USE(LIBDRM)
    std::array<drmDevicePtr, 64> devices { };

    int numDevices = drmGetDevices2(0, devices.data(), std::size(devices));
    if (numDevices <= 0)
        return nullptr;

    unsigned primaryNodesCount = 0;
    GRefPtr<WPEDRMDevice> drmDevice;
    for (int i = 0; i < numDevices; ++i) {
        drmDevicePtr device = devices[i];
        bool hasPrimaryNode = device->available_nodes & (1 << DRM_NODE_PRIMARY);
        if (!hasPrimaryNode)
            continue;

        bool hasRenderNode = device->available_nodes & (1 << DRM_NODE_RENDER);
        const char* primaryNode = device->nodes[DRM_NODE_PRIMARY];
        const char* renderNode = hasRenderNode ? device->nodes[DRM_NODE_RENDER] : nullptr;
        if (deviceFilename) {
            if (!g_strcmp0(deviceFilename, primaryNode) || !g_strcmp0(deviceFilename, renderNode)) {
                drmDevice = adoptGRef(wpe_drm_device_new(primaryNode, renderNode));
                break;
            }
            continue;
        }

        primaryNodesCount++;
        if (drmDevice) {
            if (wpe_drm_device_get_render_node(drmDevice.get()))
                break;

            if (!hasRenderNode)
                continue;
        }

        drmDevice = adoptGRef(wpe_drm_device_new(primaryNode, renderNode));
    }
    drmFreeDevices(devices.data(), numDevices);

    if (!deviceFilename && drmDevice && primaryNodesCount > 1)
        g_warning("Infered DRM device (%s) using libdrm but multiple were found, you can override this with WPE_DRM_DEVICE", wpe_drm_device_get_primary_node(drmDevice.get()));

    return drmDevice;
#else
    return nullptr;
#endif
}

/**
 * wpe_drm_device_new:
 * @primary_node: the filename of the primary node
 * @render_node: (nullable): the filename of the render node, or %NULL
 *
 * Create a new #WPEDRMDevice for the given @primary_node and @render_node.
 *
 * Returns: (transfer full): a new #WPEDRMDevice
 */
WPEDRMDevice* wpe_drm_device_new(const char* primaryNode, const char *renderNode)
{
    g_return_val_if_fail(primaryNode, nullptr);

    auto* device = static_cast<WPEDRMDevice*>(fastMalloc(sizeof(WPEDRMDevice)));
    new (device) WPEDRMDevice(primaryNode, renderNode);
    return device;
}

/**
 * wpe_drm_device_ref:
 * @device: a #WPEDRMDevice
 *
 * Atomically acquires a reference on the given @device.
 *
 * This function is MT-safe and may be called from any thread.
 *
 * Returns: (transfer full): The same @device with an additional reference.
 */
WPEDRMDevice* wpe_drm_device_ref(WPEDRMDevice* device)
{
    g_return_val_if_fail(device, nullptr);

    g_atomic_int_inc(&device->referenceCount);
    return device;
}

/**
 * wpe_drm_device_unref:
 * @device: a #WPEDRMDevice
 *
 * Atomically releases a reference on the given @device.
 *
 * If the reference was the last, the resources associated to the
 * @device are freed. This function is MT-safe and may be called from
 * any thread.
 */
void wpe_drm_device_unref(WPEDRMDevice* device)
{
    g_return_if_fail(device);

    if (g_atomic_int_dec_and_test(&device->referenceCount)) {
        device->~WPEDRMDevice();
        fastFree(device);
    }
}

/**
 * wpe_drm_device_get_primary_node:
 * @device: a #WPEDRMDevice
 *
 * Get the filename of primary node of @device.
 *
 * Returns: (transfer none): the filename of primary node.
 */
const char* wpe_drm_device_get_primary_node(WPEDRMDevice* device)
{
    g_return_val_if_fail(device, nullptr);

    return device->primaryNode.data();
}

/**
 * wpe_drm_device_get_render_node:
 * @device: a #WPEDRMDevice
 *
 * Get the filename of render node of @device.
 *
 * Returns: (transfer none) (nullable): the filename of render node or %NULL
 */
const char* wpe_drm_device_get_render_node(WPEDRMDevice* device)
{
    g_return_val_if_fail(device, nullptr);

    return device->renderNode.data();
}
