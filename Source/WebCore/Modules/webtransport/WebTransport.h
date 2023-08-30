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

#include "ExceptionOr.h"
#include <wtf/RefCounted.h>

namespace JSC {
class JSGlobalObject;
}

namespace WebCore {

enum class WebTransportReliabilityMode : uint8_t;
enum class WebTransportCongestionControl : uint8_t;
class DOMPromise;
class DeferredPromise;
class ReadableStream;
class WebTransportDatagramDuplexStream;
struct WebTransportSendStreamOptions;
struct WebTransportOptions;
struct WebTransportCloseInfo;

class WebTransport : public RefCounted<WebTransport> {
public:
    static Ref<WebTransport> create(String&&, std::optional<WebTransportOptions>&&);

    void getStats(Ref<DeferredPromise>&&);
    Ref<DOMPromise> ready(JSC::JSGlobalObject&);
    WebTransportReliabilityMode reliability();
    WebTransportCongestionControl congestionControl();
    Ref<DOMPromise> closed(JSC::JSGlobalObject&);
    Ref<DOMPromise> draining(JSC::JSGlobalObject&);
    void close(std::optional<WebTransportCloseInfo>&&);
    Ref<WebTransportDatagramDuplexStream> datagrams();
    void createBidirectionalStream(std::optional<WebTransportSendStreamOptions>&&, Ref<DeferredPromise>&&);
    ExceptionOr<Ref<ReadableStream>> incomingBidirectionalStreams(JSC::JSGlobalObject&);
    void createUnidirectionalStream(std::optional<WebTransportSendStreamOptions>&&, Ref<DeferredPromise>&&);
    ExceptionOr<Ref<ReadableStream>> incomingUnidirectionalStreams(JSC::JSGlobalObject&);
};

}
