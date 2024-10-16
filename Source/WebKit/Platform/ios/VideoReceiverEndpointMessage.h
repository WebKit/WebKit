/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(LINEAR_MEDIA_PLAYER)

#include <WebCore/HTMLMediaElementIdentifier.h>
#include <WebCore/MediaPlayerIdentifier.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/VideoReceiverEndpoint.h>
#include <wtf/Markable.h>
#include <wtf/OSObjectPtr.h>
#include <wtf/text/ASCIILiteral.h>

namespace WebKit {

class VideoReceiverEndpointMessage {
public:
    VideoReceiverEndpointMessage(std::optional<WebCore::ProcessIdentifier>, WebCore::HTMLMediaElementIdentifier, std::optional<WebCore::MediaPlayerIdentifier>, WebCore::VideoReceiverEndpoint);

    static ASCIILiteral messageName() { return "video-receiver-endpoint"_s; }
    static VideoReceiverEndpointMessage decode(xpc_object_t);
    OSObjectPtr<xpc_object_t> encode() const;

    std::optional<WebCore::ProcessIdentifier> processIdentifier() const { return m_processIdentifier; }
    WebCore::HTMLMediaElementIdentifier mediaElementIdentifier() const { return m_mediaElementIdentifier; }
    std::optional<WebCore::MediaPlayerIdentifier> playerIdentifier() const { return m_playerIdentifier; }
    const WebCore::VideoReceiverEndpoint& endpoint() const { return m_endpoint; }

private:
    Markable<WebCore::ProcessIdentifier> m_processIdentifier;
    WebCore::HTMLMediaElementIdentifier m_mediaElementIdentifier;
    Markable<WebCore::MediaPlayerIdentifier> m_playerIdentifier;
    WebCore::VideoReceiverEndpoint m_endpoint;
};

} // namespace WebKit

#endif // ENABLE(LINEAR_MEDIA_PLAYER)
