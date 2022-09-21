/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEB_RTC)

#include "Connection.h"
#include "DataReference.h"
#include "WorkQueueMessageReceiver.h"
#include <WebCore/RTCDataChannelRemoteHandlerConnection.h>
#include <WebCore/RTCDataChannelRemoteSourceConnection.h>
#include <wtf/WorkQueue.h>

namespace WebKit {

class NetworkConnectionToWebProcess;

class RTCDataChannelRemoteManagerProxy final : public IPC::WorkQueueMessageReceiver {
public:
    static Ref<RTCDataChannelRemoteManagerProxy> create() { return adoptRef(*new RTCDataChannelRemoteManagerProxy); }

    void registerConnectionToWebProcess(NetworkConnectionToWebProcess&);
    void unregisterConnectionToWebProcess(NetworkConnectionToWebProcess&);

private:
    RTCDataChannelRemoteManagerProxy();

    // IPC::WorkQueueMessageReceiver overrides.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    // To source
    void sendData(WebCore::RTCDataChannelIdentifier, bool isRaw, const IPC::DataReference&);
    void close(WebCore::RTCDataChannelIdentifier);

    // To handler
    void changeReadyState(WebCore::RTCDataChannelIdentifier, WebCore::RTCDataChannelState);
    void receiveData(WebCore::RTCDataChannelIdentifier, bool isRaw, const IPC::DataReference&);
    void detectError(WebCore::RTCDataChannelIdentifier, WebCore::RTCErrorDetailType, const String&);
    void bufferedAmountIsDecreasing(WebCore::RTCDataChannelIdentifier, size_t amount);

    Ref<WorkQueue> m_queue;
    HashMap<WebCore::ProcessIdentifier, IPC::Connection::UniqueID> m_webProcessConnections;
};

} // namespace WebKit

#endif // ENABLE(WEB_RTC)
