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

#include "config.h"
#include "WebTransport.h"

#include "JSDOMPromise.h"
#include "JSDOMPromiseDeferred.h"
#include "ReadableStream.h"
#include "WebTransportCongestionControl.h"
#include "WebTransportDatagramDuplexStream.h"
#include "WebTransportReliabilityMode.h"
#include <JavaScriptCore/JSGlobalObject.h>

namespace WebCore {

Ref<WebTransport> WebTransport::create(String&&, std::optional<WebTransportOptions>&&)
{
    return adoptRef(*new WebTransport());
}

void WebTransport::getStats(Ref<DeferredPromise>&& promise)
{
    promise->reject(nullptr);
}

static Ref<DOMPromise> createRejectingPromise(JSC::JSGlobalObject& globalObject)
{
    JSC::JSLockHolder lock(globalObject.vm());
    auto& jsDOMGlobalObject = *JSC::jsCast<JSDOMGlobalObject*>(&globalObject);
    auto deferredPromise = DeferredPromise::create(jsDOMGlobalObject);
    RunLoop::current().dispatch([deferredPromise] {
        deferredPromise->reject(nullptr);
    });
    return DOMPromise::create(jsDOMGlobalObject, *JSC::jsCast<JSC::JSPromise*>(deferredPromise->promise()));
}

Ref<DOMPromise> WebTransport::ready(JSC::JSGlobalObject& globalObject)
{
    return createRejectingPromise(globalObject);
}

WebTransportReliabilityMode WebTransport::reliability()
{
    return { };
}

WebTransportCongestionControl WebTransport::congestionControl()
{
    return { };
}

Ref<DOMPromise> WebTransport::closed(JSC::JSGlobalObject& globalObject)
{
    return createRejectingPromise(globalObject);
}

Ref<DOMPromise> WebTransport::draining(JSC::JSGlobalObject& globalObject)
{
    return createRejectingPromise(globalObject);
}

void WebTransport::close(std::optional<WebTransportCloseInfo>&&)
{
}

Ref<WebTransportDatagramDuplexStream> WebTransport::datagrams()
{
    return WebTransportDatagramDuplexStream::create();
}

void WebTransport::createBidirectionalStream(std::optional<WebTransportSendStreamOptions>&&, Ref<DeferredPromise>&& promise)
{
    promise->reject(nullptr);
}

ExceptionOr<Ref<ReadableStream>> WebTransport::incomingBidirectionalStreams(JSC::JSGlobalObject& globalObject)
{
    return ReadableStream::create(globalObject, std::nullopt, std::nullopt);
}

void WebTransport::createUnidirectionalStream(std::optional<WebTransportSendStreamOptions>&&, Ref<DeferredPromise>&&)
{
}

ExceptionOr<Ref<ReadableStream>> WebTransport::incomingUnidirectionalStreams(JSC::JSGlobalObject& globalObject)
{
    return ReadableStream::create(globalObject, std::nullopt, std::nullopt);
}

}
