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

#include "Exception.h"
#include "IDLTypes.h"
#include "JSDOMGlobalObject.h"
#include "JSWebTransportError.h"
#include "ScriptExecutionContextInlines.h"
#include "WebTransport.h"
#include "WebTransportError.h"
#include "WebTransportSession.h"
#include "WritableStream.h"
#include <wtf/CompletionHandler.h>
#include <wtf/RunLoop.h>

namespace WebCore {

WebTransportSendStreamSink::WebTransportSendStreamSink(WebTransport& transport, WebTransportStreamIdentifier identifier)
    : m_transport(transport)
    , m_identifier(identifier)
{
}

WebTransportSendStreamSink::~WebTransportSendStreamSink()
{
}

RefPtr<WritableStream> WebTransportSendStreamSink::stream() const
{
    return m_stream.get();
}

void WebTransportSendStreamSink::sendError(JSDOMGlobalObject& globalObject, JSC::JSValue error)
{
    if (m_isClosed || m_isCancelled)
        return;
    m_isCancelled = true;

    if (RefPtr stream = m_stream.get()) {
        Locker<JSC::JSLock> locker(globalObject.vm().apiLock());
        stream->errorIfPossible(globalObject, error);
    }

    if (RefPtr transport = m_transport.get())
        transport->sendStreamClosed(m_identifier);
}

void WebTransportSendStreamSink::write(ScriptExecutionContext& context, JSC::JSValue value, DOMPromiseDeferred<void>&& promise)
{
    RefPtr transport = m_transport.get();
    if (!transport)
        return promise.reject(Exception { ExceptionCode::InvalidStateError });

    RefPtr session = transport->session();
    if (!session)
        return promise.reject(Exception { ExceptionCode::InvalidStateError });

    if (!context.globalObject())
        return promise.reject(Exception { ExceptionCode::InvalidStateError });

    if (m_isClosed)
        return promise.reject(Exception { ExceptionCode::InvalidStateError });

    auto& globalObject = *JSC::jsCast<JSDOMGlobalObject*>(context.globalObject());
    auto scope = DECLARE_THROW_SCOPE(globalObject.vm());

    auto bufferSource = convert<IDLUnion<IDLArrayBuffer, IDLArrayBufferView>>(globalObject, value);
    if (bufferSource.hasException(scope)) [[unlikely]]
        return promise.settle(Exception { ExceptionCode::ExistingExceptionError });

    WTF::switchOn(bufferSource.releaseReturnValue(), [&] (auto&& arrayBufferOrView) {
        constexpr bool withFin { false };
        context.enqueueTaskWhenSettled(session->streamSendBytes(m_identifier, arrayBufferOrView->span(), withFin), TaskSource::Networking, [promise = WTF::move(promise)] (auto&& exception) mutable {
            if (!exception)
                promise.settle(Exception { ExceptionCode::NetworkError });
            else if (*exception)
                promise.settle(WTF::move(**exception));
            else
                promise.resolve();
        });
    });
}

void WebTransportSendStreamSink::close(JSDOMGlobalObject&)
{
    if (m_isClosed)
        return;
    m_isClosed = true;

    if (RefPtr transport = m_transport.get()) {
        if (RefPtr session = transport->session())
            session->streamSendBytes(m_identifier, { }, true);
        transport->sendStreamClosed(m_identifier);
    }
}

void WebTransportSendStreamSink::abort(JSDOMGlobalObject&, JSC::JSValue value, DOMPromiseDeferred<void>&& promise)
{
    auto scope = makeScopeExit([&promise] {
        promise.resolve();
    });

    if (m_isCancelled)
        return;
    m_isCancelled = true;

    RefPtr transport = m_transport.get();
    if (!transport)
        return;
    transport->sendStreamClosed(m_identifier);

    RefPtr session = transport->session();
    if (!session)
        return;

    std::optional<uint64_t> errorCode;
    if (auto* jsWebTransportError = JSC::jsDynamicCast<JSWebTransportError*>(value)) {
        Ref webTransportError = jsWebTransportError->wrapped();
        if (auto webTransportErrorCode = webTransportError->streamErrorCode())
            errorCode = static_cast<uint64_t>(*webTransportErrorCode);
    }
    session->cancelSendStream(m_identifier, errorCode);
}

}
