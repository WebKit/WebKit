/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "RemotePageWebDeviceOrientationUpdateProviderProxy.h"

#if PLATFORM(IOS_FAMILY) && ENABLE(DEVICE_ORIENTATION)

#include "WebDeviceOrientationUpdateProviderProxy.h"
#include "WebDeviceOrientationUpdateProviderProxyMessages.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <WebCore/MotionManagerClient.h>

namespace WebKit {

Ref<RemotePageWebDeviceOrientationUpdateProviderProxy> RemotePageWebDeviceOrientationUpdateProviderProxy::create(WebCore::PageIdentifier pageID, WebProcessProxy& process, WebCore::MotionManagerClient* client)
{
    return adoptRef(*new RemotePageWebDeviceOrientationUpdateProviderProxy(pageID, process, client));
}

RemotePageWebDeviceOrientationUpdateProviderProxy::RemotePageWebDeviceOrientationUpdateProviderProxy(WebCore::PageIdentifier pageID, WebProcessProxy& process, WebCore::MotionManagerClient* client)
    : m_identifier(pageID)
    , m_client(client)
    , m_process(process)
{
    process.addMessageReceiver(Messages::WebDeviceOrientationUpdateProviderProxy::messageReceiverName(), m_identifier, *this);
}

RemotePageWebDeviceOrientationUpdateProviderProxy::~RemotePageWebDeviceOrientationUpdateProviderProxy()
{
    m_process->removeMessageReceiver(Messages::WebDeviceOrientationUpdateProviderProxy::messageReceiverName(), m_identifier);
}

void RemotePageWebDeviceOrientationUpdateProviderProxy::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (RefPtr provider = dynamicDowncast<WebDeviceOrientationUpdateProviderProxy>(m_client.get()))
        provider->didReceiveMessage(connection, decoder);
}

}
#endif // PLATFORM(IOS_FAMILY) && ENABLE(DEVICE_ORIENTATION)
