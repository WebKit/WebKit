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

#include "config.h"
#include "RTCDataChannelRemoteSource.h"

#if ENABLE(WEB_RTC)

#include "ProcessQualified.h"
#include "RTCDataChannelHandler.h"
#include "ScriptExecutionContextIdentifier.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RTCDataChannelRemoteSource);

Ref<RTCDataChannelRemoteSource> RTCDataChannelRemoteSource::create(RTCDataChannelIdentifier localIdentifier, RTCDataChannelIdentifier remoteIdentifier, UniqueRef<RTCDataChannelHandler>&& handler, Ref<RTCDataChannelRemoteSourceConnection>&& connection)
{
    return adoptRef(*new RTCDataChannelRemoteSource(localIdentifier, remoteIdentifier, WTF::move(handler), WTF::move(connection)));
}

RTCDataChannelRemoteSource::RTCDataChannelRemoteSource(RTCDataChannelIdentifier localIdentifier, RTCDataChannelIdentifier remoteIdentifier, UniqueRef<RTCDataChannelHandler>&& handler, Ref<RTCDataChannelRemoteSourceConnection>&& connection)
    : RTCDataChannelHandlerClient(std::nullopt, localIdentifier)
    , m_remoteIdentifier(remoteIdentifier)
    , m_handler(WTF::move(handler))
    , m_connection(WTF::move(connection))
{
    ASSERT(isMainThread());
    // FIXME: We should ask m_handler to call us on its own background thread.
    m_handler->setClient(*this, std::nullopt);
}

RTCDataChannelRemoteSource::~RTCDataChannelRemoteSource() = default;

void RTCDataChannelRemoteSource::didChangeReadyState(RTCDataChannelState state)
{
    ASSERT(isMainThread());
    if (m_isClosed)
        return;
    if (state == RTCDataChannelState::Closed)
        m_isClosed = true;
    m_connection->didChangeReadyState(m_remoteIdentifier, state);
}

void RTCDataChannelRemoteSource::didReceiveStringData(const String& text)
{
    ASSERT(isMainThread());
    if (m_isClosed)
        return;
    m_connection->didReceiveStringData(m_remoteIdentifier, text);
}

void RTCDataChannelRemoteSource::didReceiveRawData(std::span<const uint8_t> data)
{
    ASSERT(isMainThread());
    if (m_isClosed)
        return;
    m_connection->didReceiveRawData(m_remoteIdentifier, data);
}

void RTCDataChannelRemoteSource::didDetectError(Ref<RTCError>&& error)
{
    ASSERT(isMainThread());
    if (m_isClosed)
        return;
    m_connection->didDetectError(m_remoteIdentifier, error->errorDetail(), error->message());
}

void RTCDataChannelRemoteSource::bufferedAmountIsDecreasing(size_t amount)
{
    ASSERT(isMainThread());
    if (m_isClosed)
        return;
    m_connection->bufferedAmountIsDecreasing(m_remoteIdentifier, amount);
}

size_t RTCDataChannelRemoteSource::bufferedAmount() const
{
    ASSERT(isMainThread());
    return 0;
}

void RTCDataChannelRemoteSource::peerConnectionIsClosing()
{
    ASSERT(isMainThread());
    didChangeReadyState(RTCDataChannelState::Closed);
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
