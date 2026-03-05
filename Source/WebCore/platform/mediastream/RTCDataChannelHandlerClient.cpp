/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "config.h"
#include "RTCDataChannelHandlerClient.h"

#if ENABLE(WEB_RTC)

#include "RTCDataChannel.h"
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static Lock s_rtcDataChannelHandlerClientsLock;
static HashMap<RTCDataChannelLocalIdentifier, std::pair<std::optional<ScriptExecutionContextIdentifier>, WeakPtr<RTCDataChannelHandlerClient>>>& rtcDataChannelHandlerClients() WTF_REQUIRES_LOCK(s_rtcDataChannelHandlerClientsLock)
{
    static NeverDestroyed<HashMap<RTCDataChannelLocalIdentifier, std::pair<std::optional<ScriptExecutionContextIdentifier>, WeakPtr<RTCDataChannelHandlerClient>>>> map;
    return map;
}

void RTCDataChannelHandlerClient::peerConnectionIsClosing(RTCDataChannelIdentifier identifier)
{
    ASSERT(isMainThread());
    RefPtr<RTCDataChannelHandlerClient> mainThreadClient;
    {
        Locker locker { s_rtcDataChannelHandlerClientsLock };
        auto iterator = rtcDataChannelHandlerClients().find(identifier.object());
        if (iterator != rtcDataChannelHandlerClients().end()) {
            if (iterator->value.first) {
                ScriptExecutionContext::postTaskTo(*iterator->value.first, [weakClient = iterator->value.second](auto&) {
                    if (RefPtr client = weakClient.get())
                        client->peerConnectionIsClosing();
                });
                return;
            }

            mainThreadClient = iterator->value.second.get();
        }
    }
    if (mainThreadClient) {
        mainThreadClient->peerConnectionIsClosing();
        return;
    }

    RTCDataChannel::removeDetachedRTCDataChannel(identifier);
}

RTCDataChannelHandlerClient::RTCDataChannelHandlerClient(std::optional<ScriptExecutionContextIdentifier> contextIdentifier, RTCDataChannelIdentifier identifier)
    : m_identifier(identifier)
{
    Locker locker { s_rtcDataChannelHandlerClientsLock };
    ASSERT(!rtcDataChannelHandlerClients().contains(m_identifier.object()));
    rtcDataChannelHandlerClients().add(m_identifier.object(), std::make_pair(contextIdentifier, WeakPtr { *this }));
}

RTCDataChannelHandlerClient::~RTCDataChannelHandlerClient()
{
    unregister();
}

void RTCDataChannelHandlerClient::unregister()
{
    if (m_isUnregistered)
        return;

    m_isUnregistered = true;

    Locker locker { s_rtcDataChannelHandlerClientsLock };
    ASSERT(rtcDataChannelHandlerClients().contains(m_identifier.object()));
    ASSERT(rtcDataChannelHandlerClients().get(m_identifier.object()).second.get() == this);
    rtcDataChannelHandlerClients().remove(m_identifier.object());
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
