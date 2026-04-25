/*
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
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
#include "NetworkTransportStream.h"

#include <WebCore/Exception.h>
#include <WebCore/WebTransportReceiveStreamStats.h>
#include <WebCore/WebTransportSendStreamStats.h>
#include <wtf/CompletionHandler.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(NetworkTransportStream);

#if !PLATFORM(COCOA)
NetworkTransportStream::NetworkTransportStream()
    : m_identifier(WebCore::WebTransportStreamIdentifier::generate())
    , m_streamType(NetworkTransportStreamType::Bidirectional)
    , m_streamState(NetworkTransportStreamState::Ready)
{
}

void NetworkTransportStream::sendBytes(std::span<const uint8_t>, bool, CompletionHandler<void(std::optional<WebCore::Exception>&&)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

void NetworkTransportStream::cancelReceive(std::optional<WebCore::WebTransportStreamErrorCode>)
{
}

void NetworkTransportStream::cancelSend(std::optional<WebCore::WebTransportStreamErrorCode>)
{
}

void NetworkTransportStream::cancel(std::optional<WebCore::WebTransportStreamErrorCode>)
{
}

WebCore::WebTransportSendStreamStats NetworkTransportStream::getSendStreamStats()
{
    return { };
}

WebCore::WebTransportReceiveStreamStats NetworkTransportStream::getReceiveStreamStats()
{
    return { };
}

#endif

}
