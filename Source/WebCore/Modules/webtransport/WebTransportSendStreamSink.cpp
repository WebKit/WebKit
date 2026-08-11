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

#include "AbortSignal.h"
#include "EventLoop.h"
#include "Exception.h"
#include "IDLTypes.h"
#include "JSDOMConvertBufferSource.h"
#include "JSDOMConvertUnion.h"
#include "JSDOMGlobalObject.h"
#include "JSDOMPromiseDeferred.h"
#include "JSWebTransportError.h"
#include "ScriptExecutionContextInlines.h"
#include "WebTransport.h"
#include "WebTransportError.h"
#include "WebTransportSession.h"
#include "WritableStream.h"
#include <wtf/CompletionHandler.h>
#include <wtf/RunLoop.h>
#include <wtf/Scope.h>

namespace WebCore {

WebTransportSendStreamSink::WebTransportSendStreamSink(WebTransport& transport, WebTransportStreamIdentifier identifier)
    : m_transport(transport)
    , m_identifier(identifier)
{
}

WebTransportSendStreamSink::~WebTransportSendStreamSink()
{
    if (m_abortSignal && m_abortAlgorithmIdentifier)
        protect(m_abortSignal)->removeAlgorithm(*m_abortAlgorithmIdentifier);
}

void WebTransportSendStreamSink::start(std::unique_ptr<WritableStreamDefaultController>&& controller)
{
    WritableStreamSink::start(WTF::move(controller));

    // The abort signal fires synchronously during writer.abort(), before the abort() algorithm runs,
    // so observing it is the only way to cancel an in-flight close().
    if (RefPtr signal = abortSignal()) {
        m_abortAlgorithmIdentifier = signal->addAlgorithm([weakThis = WeakPtr { *this }](JSC::JSValue reason) {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->cancel(reason);
        });
        m_abortSignal = WTF::move(signal);
    }
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

    if (!context.globalObject())
        return promise.reject(Exception { ExceptionCode::InvalidStateError });

    if (m_isClosed)
        return promise.reject(Exception { ExceptionCode::InvalidStateError });

    auto& globalObject = *downcast<JSDOMGlobalObject>(context.globalObject());
    auto scope = DECLARE_THROW_SCOPE(globalObject.vm());

    auto bufferSource = convert<IDLUnion<IDLArrayBuffer, IDLArrayBufferView>>(globalObject, value);
    if (bufferSource.hasException(scope)) [[unlikely]]
        return promise.settle(Exception { ExceptionCode::ExistingExceptionError });

    WTF::switchOn(bufferSource.releaseReturnValue(), [&] (auto&& arrayBufferOrView) {
        constexpr bool withFin { false };
        context.enqueueTaskWhenSettled(transport->session()->streamSendBytes(m_identifier, arrayBufferOrView->span(), withFin), TaskSource::Networking, [promise = WTF::move(promise)] (auto&& exception) mutable {
            if (!exception)
                promise.settle(Exception { ExceptionCode::NetworkError });
            else if (*exception)
                promise.settle(WTF::move(**exception));
            else
                promise.resolve();
        });
    });
}

// https://w3c.github.io/webtransport/#webtransportsendstream-close
void WebTransportSendStreamSink::close(JSDOMGlobalObject& globalObject, DOMPromiseDeferred<void>&& promise)
{
    if (m_isClosed || m_isCancelled)
        return promise.reject(Exception { ExceptionCode::InvalidStateError });
    m_isClosed = true;

    RefPtr context = globalObject.scriptExecutionContext();
    if (!context)
        return promise.reject(Exception { ExceptionCode::InvalidStateError });

    m_closeDeferred = makeUnique<DOMPromiseDeferred<void>>(WTF::move(promise));

    // Defer the FIN so a racing abort() can suppress it and reset the stream instead.
    // FIXME: Validate this abort-after-close behavior. https://bugs.webkit.org/show_bug.cgi?id=320232
    protect(context->eventLoop())->queueTask(TaskSource::Networking, [protectedThis = Ref { *this }, context = Ref { *context }] {
        protectedThis->sendFin(context.get());
    });
}

void WebTransportSendStreamSink::sendFin(ScriptExecutionContext& context)
{
    if (m_isCancelled || !m_closeDeferred)
        return;

    RefPtr transport = m_transport.get();
    RefPtr session = transport ? transport->session().ptr() : nullptr;
    if (!transport || !session)
        return std::exchange(m_closeDeferred, nullptr)->reject(Exception { ExceptionCode::InvalidStateError });

    transport->sendStreamClosed(m_identifier);
    context.enqueueTaskWhenSettled(session->streamSendBytes(m_identifier, { }, true), TaskSource::Networking, [protectedThis = Ref { *this }] (auto&& exception) mutable {
        auto deferred = std::exchange(protectedThis->m_closeDeferred, nullptr);
        if (!deferred)
            return;
        if (!exception)
            return deferred->reject(Exception { ExceptionCode::NetworkError });
        if (*exception)
            return deferred->reject(WTF::move(**exception));
        deferred->resolve();
    });
}

void WebTransportSendStreamSink::abort(JSDOMGlobalObject&, JSC::JSValue reason, DOMPromiseDeferred<void>&& promise)
{
    cancel(reason);
    promise.resolve();
}

void WebTransportSendStreamSink::cancel(JSC::JSValue reason)
{
    if (m_isCancelled)
        return;
    m_isCancelled = true;

    // Reject an in-flight close() so its promise rejects rather than resolving after the FIN.
    if (auto deferred = std::exchange(m_closeDeferred, nullptr)) {
        deferred->rejectWithCallback([reason](JSDOMGlobalObject&) {
            return reason;
        });
    }

    RefPtr transport = m_transport.get();
    if (!transport)
        return;

    transport->sendStreamClosed(m_identifier);

    std::optional<uint64_t> errorCode;
    if (auto* jsWebTransportError = dynamicDowncast<JSWebTransportError>(reason)) {
        auto& webTransportError = jsWebTransportError->wrapped();
        if (auto webTransportErrorCode = webTransportError.streamErrorCode())
            errorCode = static_cast<uint64_t>(*webTransportErrorCode);
    }
    transport->session()->cancelSendStream(m_identifier, errorCode);
}

}
