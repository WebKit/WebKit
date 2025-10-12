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

#if USE(GBM)
#include "GBMDevice.h"

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

void DRMDeviceManager::initializeMainDevice(DRMDevice&& device)
{
    RELEASE_ASSERT(isMainThread());
    RELEASE_ASSERT(!m_mainDevice.isInitialized);
    m_mainDevice.isInitialized = true;
    m_mainDevice.device = WTFMove(device);
}

RefPtr<GBMDevice> DRMDeviceManager::mainGBMDevice(NodeType nodeType) const
{
    RELEASE_ASSERT(m_mainDevice.isInitialized);
    if (m_mainDevice.device.isNull())
        return nullptr;

    if (nodeType == NodeType::Render) {
        if (m_mainDevice.gbmRenderNode)
            return *m_mainDevice.gbmRenderNode;

        if (!m_mainDevice.device.renderNode.isNull()) {
            m_mainDevice.gbmRenderNode = GBMDevice::create(m_mainDevice.device.renderNode);
            return *m_mainDevice.gbmRenderNode;
        }
        // Fallback to primary node.
    }

    if (!m_mainDevice.gbmPrimaryNode)
        m_mainDevice.gbmPrimaryNode = GBMDevice::create(m_mainDevice.device.primaryNode);
    return *m_mainDevice.gbmPrimaryNode;
}

RefPtr<GBMDevice> DRMDeviceManager::gbmDevice(const DRMDevice& device, NodeType nodeType) const
{
    RELEASE_ASSERT(m_mainDevice.isInitialized);
    if (device.primaryNode == m_mainDevice.device.primaryNode)
        return mainGBMDevice(nodeType);

    return GBMDevice::create(nodeType == NodeType::Render && !device.renderNode.isNull() ? device.renderNode : device.primaryNode);
}

} // namespace WebCore

#endif // USE(GBM)
