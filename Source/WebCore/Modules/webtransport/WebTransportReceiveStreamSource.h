/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "ReadableStreamSource.h"

namespace WebCore {

class Exception;
class WebTransport;
class WebTransportReceiveStream;

struct WebTransportStreamIdentifierType;

using WebTransportStreamIdentifier = ObjectIdentifier<WebTransportStreamIdentifierType>;

class WebTransportReceiveStreamSource : public RefCountedReadableStreamSource {
public:
    static Ref<WebTransportReceiveStreamSource> createIncomingStreamsSource() { return adoptRef(*new WebTransportReceiveStreamSource()); }
    static Ref<WebTransportReceiveStreamSource> createIncomingDataSource(WebTransport& transport, WebTransportStreamIdentifier identifier) { return adoptRef(*new WebTransportReceiveStreamSource(transport, identifier)); }
    bool receiveIncomingStream(JSC::JSGlobalObject&, Ref<WebTransportReceiveStream>&);
    void receiveBytes(std::span<const uint8_t>, bool, std::optional<WebCore::Exception>&&);
    void receiveError(JSC::JSGlobalObject&, uint64_t errorCode);
private:
    WebTransportReceiveStreamSource();
    WebTransportReceiveStreamSource(WebTransport&, WebTransportStreamIdentifier);
    void setActive() final { }
    void setInactive() final { }
    void doStart() final { }
    void doPull() final { }
    void doCancel(JSC::JSValue) final { doCancel(); }

    void doCancel();

    bool m_isCancelled { false };
    bool m_isClosed { false };

    ThreadSafeWeakPtr<WebTransport> m_transport;
    const std::optional<WebTransportStreamIdentifier> m_identifier;
};

}
