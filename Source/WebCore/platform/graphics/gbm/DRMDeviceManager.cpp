/*
 * Copyright (C) 2024 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DRMDeviceManager.h"

#include <wtf/text/WTFString.h>

#if USE(LIBDRM)
#include "DRMDeviceNode.h"
#include <xf86drm.h>

namespace WebCore {

DRMDeviceManager& DRMDeviceManager::singleton()
{
    static std::unique_ptr<DRMDeviceManager> s_manager;
    static std::once_flag s_onceFlag;
    std::call_once(s_onceFlag, [] {
        s_manager = makeUnique<DRMDeviceManager>();
    });
    return *s_manager;
}

DRMDeviceManager::~DRMDeviceManager() = default;

static void drmForeachDevice(Function<bool(drmDevice*)>&& functor)
{
    drmDevicePtr devices[64];
    memset(devices, 0, sizeof(devices));

    int numDevices = drmGetDevices2(0, devices, std::size(devices));
    if (numDevices <= 0)
        return;

    for (int i = 0; i < numDevices; ++i) {
        if (!functor(devices[i]))
            break;
    }
    drmFreeDevices(devices, numDevices);
}

void DRMDeviceManager::initializeMainDevice(const String& deviceFile)
{
    RELEASE_ASSERT(isMainThread());
    RELEASE_ASSERT(!m_mainDevice.isInitialized);
    m_mainDevice.isInitialized = true;
    if (deviceFile.isEmpty())
        return;

    drmForeachDevice([&](drmDevice* device) {
        for (int i = 0; i < DRM_NODE_MAX; ++i) {
            if (!(device->available_nodes & (1 << i)))
                continue;

            if (String::fromUTF8(device->nodes[i]) == deviceFile) {
                RELEASE_ASSERT(device->available_nodes & (1 << DRM_NODE_PRIMARY));
                if (device->available_nodes & (1 << DRM_NODE_RENDER)) {
                    m_mainDevice.primaryNode = DRMDeviceNode::create(CString { device->nodes[DRM_NODE_PRIMARY] });
                    m_mainDevice.renderNode = DRMDeviceNode::create(CString { device->nodes[DRM_NODE_RENDER] });
                } else
                    m_mainDevice.primaryNode = DRMDeviceNode::create(CString { device->nodes[DRM_NODE_PRIMARY] });
                return false;
            }
        }
        return true;
    });

    if (!m_mainDevice.primaryNode)
        WTFLogAlways("Failed to find DRM device for %s", deviceFile.utf8().data());
}

RefPtr<DRMDeviceNode> DRMDeviceManager::mainDeviceNode(DRMDeviceManager::NodeType nodeType) const
{
    RELEASE_ASSERT(m_mainDevice.isInitialized);

    if (nodeType == NodeType::Render)
        return m_mainDevice.renderNode ? m_mainDevice.renderNode : m_mainDevice.primaryNode;

    return m_mainDevice.primaryNode ? m_mainDevice.primaryNode : m_mainDevice.renderNode;
}

#if USE(GBM)
struct gbm_device* DRMDeviceManager::mainGBMDeviceNode(NodeType nodeType) const
{
    auto node = mainDeviceNode(nodeType);
    return node ? node->gbmDevice() : nullptr;
}
#endif

RefPtr<DRMDeviceNode> DRMDeviceManager::deviceNode(const CString& filename)
{
    RELEASE_ASSERT(isMainThread());
    RELEASE_ASSERT(m_mainDevice.isInitialized);

    if (filename.isNull())
        return nullptr;

    auto node = [&] -> RefPtr<DRMDeviceNode> {
        if (m_mainDevice.primaryNode && m_mainDevice.primaryNode->filename() == filename)
            return m_mainDevice.primaryNode;

        if (m_mainDevice.renderNode && m_mainDevice.renderNode->filename() == filename)
            return m_mainDevice.renderNode;

        return DRMDeviceNode::create(CString { filename.data() });
    }();

#if USE(GBM)
    // Make sure GBMDevice is created in the main thread.
    if (node)
        node->gbmDevice();
#endif

    return node;
}

} // namespace WebCore

#endif // USE(LIBDRM)
