/*
 * Copyright (C) 2023-2025 Apple Inc. All rights reserved.
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

#include "ReadableStream.h"

namespace WebCore {

class DeferredPromise;
class WebTransportSession;

struct WebTransportReceiveStreamStats;
struct WebTransportStreamIdentifierType;

using WebTransportStreamIdentifier = ObjectIdentifier<WebTransportStreamIdentifierType>;

class WebTransportReceiveStream final : public ReadableStream {
public:
    static ExceptionOr<Ref<WebTransportReceiveStream>> create(WebTransportStreamIdentifier, WebTransportSession&, JSDOMGlobalObject&, Ref<ReadableStreamSource>&&);
    ~WebTransportReceiveStream() final;

    void getStats(ScriptExecutionContext&, Ref<DeferredPromise>&&);
private:
    WebTransportReceiveStream(ScriptExecutionContext*, WebTransportStreamIdentifier, WebTransportSession&, Ref<InternalReadableStream>&&);

    virtual Type type() const { return Type::WebTransport; }

    const WebTransportStreamIdentifier m_identifier;
    const ThreadSafeWeakPtr<WebTransportSession> m_session;
};

}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::WebTransportReceiveStream)
static bool isType(const WebCore::ReadableStream& stream) { return stream.type() == WebCore::ReadableStream::Type::WebTransport; }
SPECIALIZE_TYPE_TRAITS_END()
