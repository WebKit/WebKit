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
#include "WebTransportSendStreamSink.h"

#include "WebTransportSession.h"
#include <WebCore/IDLTypes.h>
#include <WebCore/JSDOMGlobalObject.h>
#include <wtf/CompletionHandler.h>
#include <wtf/RunLoop.h>

namespace WebKit {

WebTransportSendStreamSink::WebTransportSendStreamSink(WebTransportSession& session, WebTransportStreamIdentifier identifier)
    : m_session(session)
    , m_identifier(identifier)
{
    ASSERT(RunLoop::isMain());
}

WebTransportSendStreamSink::~WebTransportSendStreamSink()
{
    ASSERT(RunLoop::isMain());
}

void WebTransportSendStreamSink::write(WebCore::ScriptExecutionContext& context, JSC::JSValue value, WebCore::DOMPromiseDeferred<void>&& promise)
{
    ASSERT(RunLoop::isMain());
    if (!m_session)
        return promise.reject(WebCore::Exception { WebCore::ExceptionCode::InvalidStateError });

    if (!context.globalObject())
        return promise.reject(WebCore::Exception { WebCore::ExceptionCode::InvalidStateError });
    auto& globalObject = *JSC::jsCast<WebCore::JSDOMGlobalObject*>(context.globalObject());
    auto scope = DECLARE_THROW_SCOPE(globalObject.vm());
    auto arrayBufferOrView = convert<WebCore::IDLUnion<WebCore::IDLArrayBuffer, WebCore::IDLArrayBufferView>>(globalObject, value);
    if (scope.exception())
        return promise.settle(WebCore::Exception { WebCore::ExceptionCode::ExistingExceptionError });

    WTF::switchOn(arrayBufferOrView, [&](auto& arrayBufferOrView) {
        sendBytes({ static_cast<const uint8_t*>(arrayBufferOrView->data()), arrayBufferOrView->byteLength() }, [promise = WTFMove(promise)] () mutable {
            promise.resolve();
        });
    });
}

void WebTransportSendStreamSink::sendBytes(std::span<const uint8_t> bytes, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    if (!m_session)
        return completionHandler();

    m_session->streamSendBytes(m_identifier, bytes, false, WTFMove(completionHandler));
}

}
