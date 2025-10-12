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

#include "config.h"
#include "WebTransport.h"

#include "ContextDestructionObserverInlines.h"
#include "ContentSecurityPolicy.h"
#include "DatagramSink.h"
#include "DatagramSource.h"
#include "ExceptionOr.h"
#include "JSDOMException.h"
#include "JSDOMPromise.h"
#include "JSDOMPromiseDeferred.h"
#include "JSWebTransportBidirectionalStream.h"
#include "JSWebTransportCloseInfo.h"
#include "JSWebTransportConnectionStats.h"
#include "JSWebTransportSendStream.h"
#include "ReadableStream.h"
#include "ScriptExecutionContextInlines.h"
#include "SocketProvider.h"
#include "WebTransportBidirectionalStreamSource.h"
#include "WebTransportCloseInfo.h"
#include "WebTransportCongestionControl.h"
#include "WebTransportConnectionStats.h"
#include "WebTransportDatagramDuplexStream.h"
#include "WebTransportError.h"
#include "WebTransportOptions.h"
#include "WebTransportReceiveStream.h"
#include "WebTransportReceiveStreamSource.h"
#include "WebTransportReliabilityMode.h"
#include "WebTransportSendStreamSink.h"
#include "WebTransportSession.h"
#include "WorkerGlobalScope.h"
#include "WritableStream.h"
#include <JavaScriptCore/JSGlobalObject.h>
#include <wtf/CompletionHandler.h>
#include <wtf/RunLoop.h>

namespace WebCore {

ExceptionOr<Ref<WebTransport>> WebTransport::create(ScriptExecutionContext& context, String&& url, WebTransportOptions&& options)
{
    URL parsedURL(WTFMove(url));
    if (!parsedURL.isValid() || !parsedURL.protocolIs("https"_s) || parsedURL.hasFragmentIdentifier())
        return Exception { ExceptionCode::SyntaxError };

    bool dedicated = !options.allowPooling;
    if (!dedicated && !options.serverCertificateHashes.isEmpty())
        return Exception { ExceptionCode::NotSupportedError };

    auto* globalObject = context.globalObject();
    if (!globalObject) {
        ASSERT_NOT_REACHED();
        return Exception { ExceptionCode::InvalidStateError };
    }
    auto& domGlobalObject = *JSC::jsCast<JSDOMGlobalObject*>(globalObject);

    auto bidirectionalStreamSource = WebTransportBidirectionalStreamSource::create();
    auto incomingBidirectionalStreams = ReadableStream::create(domGlobalObject, bidirectionalStreamSource.copyRef());
    if (incomingBidirectionalStreams.hasException())
        return incomingBidirectionalStreams.releaseException();

    auto receiveStreamSource = WebTransportReceiveStreamSource::createIncomingStreamsSource();
    auto incomingUnidirectionalStreams = ReadableStream::create(domGlobalObject, receiveStreamSource.copyRef());
    if (incomingUnidirectionalStreams.hasException())
        return incomingUnidirectionalStreams.releaseException();

    auto datagramSource = DatagramSource::create();
    auto incomingDatagrams = ReadableStream::create(domGlobalObject, datagramSource.copyRef());
    if (incomingDatagrams.hasException())
        return incomingDatagrams.releaseException();

    auto datagramSink = DatagramSink::create();
    auto outgoingDatagrams = WritableStream::create(domGlobalObject, datagramSink.copyRef());
    if (outgoingDatagrams.hasException())
        return outgoingDatagrams.releaseException();

    RefPtr socketProvider = context.socketProvider();
    if (!socketProvider) {
        ASSERT_NOT_REACHED();
        return Exception { ExceptionCode::InvalidStateError };
    }

    auto datagrams = WebTransportDatagramDuplexStream::create(incomingDatagrams.releaseReturnValue(), outgoingDatagrams.releaseReturnValue());

    auto transport = adoptRef(*new WebTransport(context, domGlobalObject, incomingBidirectionalStreams.releaseReturnValue(), incomingUnidirectionalStreams.releaseReturnValue(), options.congestionControl, WTFMove(datagrams), WTFMove(datagramSource), WTFMove(receiveStreamSource), WTFMove(bidirectionalStreamSource)));
    datagramSink->attachTo(transport);
    transport->suspendIfNeeded();
    transport->initializeOverHTTP(*socketProvider, context, WTFMove(parsedURL), WTFMove(options));
    return transport;
}

void WebTransport::initializeOverHTTP(SocketProvider& provider, ScriptExecutionContext& context, URL&& url, WebTransportOptions&& options)
{
    if (CheckedPtr csp = context.contentSecurityPolicy(); !csp || !csp->allowConnectToSource(url))
        return cleanupWithSessionError();

    // FIXME: Rename SocketProvider to NetworkProvider or something to reflect that it provides a little more than just simple sockets. SocketAndTransportProvider?
    auto [session, promise] = provider.initializeWebTransportSession(context, *this, url, options);
    m_session = WTFMove(session);

    context.enqueueTaskWhenSettled(WTFMove(promise), TaskSource::Networking, [this, protectedThis = Ref { *this }] (auto&& result) mutable {
        if (!result) {
            m_state = State::Failed;
            m_ready.second->reject();
            m_closed.second->reject();
            return;
        }
        m_state = State::Connected;
        m_ready.second->resolve();
    });
}

WebTransport::WebTransport(ScriptExecutionContext& context, JSDOMGlobalObject& globalObject, Ref<ReadableStream>&& incomingBidirectionalStreams, Ref<ReadableStream>&& incomingUnidirectionalStreams, WebTransportCongestionControl congestionControl, Ref<WebTransportDatagramDuplexStream>&& datagrams, Ref<DatagramSource>&& datagramSource, Ref<WebTransportReceiveStreamSource>&& receiveStreamSource, Ref<WebTransportBidirectionalStreamSource>&& bidirectionalStreamSource)
    : ActiveDOMObject(&context)
    , m_incomingBidirectionalStreams(WTFMove(incomingBidirectionalStreams))
    , m_incomingUnidirectionalStreams(WTFMove(incomingUnidirectionalStreams))
    , m_ready(createPromiseAndWrapper(globalObject))
    , m_congestionControl(congestionControl)
    , m_closed(createPromiseAndWrapper(globalObject))
    , m_draining(createPromiseAndWrapper(globalObject))
    , m_datagrams(WTFMove(datagrams))
    , m_datagramSource(WTFMove(datagramSource))
    , m_receiveStreamSource(WTFMove(receiveStreamSource))
    , m_bidirectionalStreamSource(WTFMove(bidirectionalStreamSource))
{
}

WebTransport::~WebTransport() = default;

RefPtr<WebTransportSession> WebTransport::session()
{
    return m_session;
}

bool WebTransport::virtualHasPendingActivity() const
{
    // https://www.w3.org/TR/webtransport/#web-transport-gc
    return m_state == State::Connecting || m_state == State::Connected;
}

void WebTransport::receiveDatagram(std::span<const uint8_t> datagram, bool withFin, std::optional<Exception>&& exception)
{
    m_datagramSource->receiveDatagram(datagram, withFin, WTFMove(exception));
}

void WebTransport::receiveIncomingUnidirectionalStream(WebTransportStreamIdentifier identifier)
{
    RefPtr context = scriptExecutionContext();
    if (!context)
        return;
    auto* globalObject = context->globalObject();
    if (!globalObject)
        return;
    RefPtr session = m_session;
    if (!session)
        return;

    auto& jsDOMGlobalObject = *JSC::jsCast<JSDOMGlobalObject*>(globalObject);
    Ref incomingStream = WebTransportReceiveStreamSource::createIncomingDataSource(*this, identifier);
    auto stream = [&] {
        Locker<JSC::JSLock> locker(jsDOMGlobalObject.vm().apiLock());
        return WebTransportReceiveStream::create(identifier, *session, jsDOMGlobalObject, incomingStream.copyRef());
    } ();
    if (stream.hasException())
        return;
    Ref receiveStream = stream.releaseReturnValue();
    bool received = m_receiveStreamSource->receiveIncomingStream(jsDOMGlobalObject, receiveStream);
    if (received) {
        m_receiveStreams.add(receiveStream);
        ASSERT(!m_readStreamSources.contains(identifier));
        m_readStreamSources.add(identifier, WTFMove(incomingStream));
    } else
        protectedSession()->destroyStream(identifier, std::nullopt);
}

static ExceptionOr<Ref<WebTransportBidirectionalStream>> createBidirectionalStream(WebTransportSession& session, JSDOMGlobalObject& globalObject, Ref<WebTransportSendStreamSink>&& sink, Ref<WebTransportReceiveStreamSource>&& source)
{
    auto identifier = sink->identifier();
    auto sendStream = [&] {
        Locker<JSC::JSLock> locker(globalObject.vm().apiLock());
        return WebTransportSendStream::create(session, globalObject, WTFMove(sink));
    } ();
    if (sendStream.hasException())
        return sendStream.releaseException();
    auto receiveStream = [&] {
        Locker<JSC::JSLock> locker(globalObject.vm().apiLock());
        return WebTransportReceiveStream::create(identifier, session, globalObject, WTFMove(source));
    } ();
    if (receiveStream.hasException())
        return receiveStream.releaseException();
    return WebTransportBidirectionalStream::create(receiveStream.releaseReturnValue(), sendStream.releaseReturnValue());
}

void WebTransport::receiveBidirectionalStream(Ref<WebTransportSendStreamSink>&& sink)
{
    RefPtr context = scriptExecutionContext();
    if (!context)
        return;
    auto* globalObject = context->globalObject();
    if (!globalObject)
        return;
    RefPtr session = m_session;
    if (!session)
        return;

    auto identifier = sink->identifier();
    auto& jsDOMGlobalObject = *JSC::jsCast<JSDOMGlobalObject*>(globalObject);
    Ref incomingStream = WebTransportReceiveStreamSource::createIncomingDataSource(*this, identifier);
    auto stream = WebCore::createBidirectionalStream(*session, jsDOMGlobalObject, WTFMove(sink), incomingStream.copyRef());
    if (stream.hasException())
        return;
    Ref bidiStream = stream.releaseReturnValue();
    bool received = m_bidirectionalStreamSource->receiveIncomingStream(jsDOMGlobalObject, bidiStream);
    if (received) {
        m_sendStreams.add(bidiStream->writable());
        m_receiveStreams.add(bidiStream->readable());
        ASSERT(!m_readStreamSources.contains(identifier));
        m_readStreamSources.add(identifier, WTFMove(incomingStream));
    } else
        protectedSession()->destroyStream(identifier, std::nullopt);
}

void WebTransport::streamReceiveBytes(WebTransportStreamIdentifier identifier, std::span<const uint8_t> span, bool withFin, std::optional<Exception>&& exception)
{
    ASSERT(m_readStreamSources.contains(identifier));
    if (RefPtr source = m_readStreamSources.get(identifier))
        source->receiveBytes(span, withFin, WTFMove(exception));
}

void WebTransport::getStats(ScriptExecutionContext& context, Ref<DeferredPromise>&& promise)
{
    RefPtr session = m_session;
    if (!session)
        return promise->reject(ExceptionCode::InvalidStateError);

    context.enqueueTaskWhenSettled(session->getStats(), WebCore::TaskSource::Networking, [promise = WTFMove(promise)] (auto&& stats) mutable {
        if (!stats)
            return promise->reject(ExceptionCode::InvalidStateError);
        promise->resolve<IDLDictionary<WebTransportConnectionStats>>(*stats);
    });
}

DOMPromise& WebTransport::ready()
{
    return m_ready.first.get();
}

WebTransportReliabilityMode WebTransport::reliability()
{
    return m_reliability;
}

WebTransportCongestionControl WebTransport::congestionControl()
{
    return m_congestionControl;
}

DOMPromise& WebTransport::closed()
{
    return m_closed.first.get();
}

DOMPromise& WebTransport::draining()
{
    return m_draining.first.get();
}

static CString trimToValidUTF8Length1024(CString&& string)
{
    if (string.length() > 1024)
        string = string.span().first(1024);
    else
        return WTFMove(string);

    while (true) {
        if (!string.length())
            return WTFMove(string);
        auto decoded = String::fromUTF8(string.span());
        if (!decoded)
            string = string.span().first(string.length() - 1);
        else
            return WTFMove(string);
    }
}

void WebTransport::cleanupWithSessionError()
{
    cleanup(WebTransportError::create(String(emptyString()), WebTransportErrorOptions {
        WebTransportErrorSource::Session,
        std::nullopt
    }), std::nullopt);
}

void WebTransport::close(WebTransportCloseInfo&& closeInfo)
{
    // https://www.w3.org/TR/webtransport/#dom-webtransport-close
    if (m_state == State::Closed || m_state == State::Failed)
        return;
    if (m_state == State::Connecting)
        return cleanupWithSessionError();
    if (auto session = std::exchange(m_session, nullptr))
        session->terminate(closeInfo.closeCode, trimToValidUTF8Length1024(closeInfo.reason.utf8()));
    cleanup(DOMException::create(ExceptionCode::AbortError), WTFMove(closeInfo));
}

void WebTransport::cleanup(Ref<DOMException>&& exception, std::optional<WebTransportCloseInfo>&& closeInfo)
{
    // https://www.w3.org/TR/webtransport/#webtransport-cleanup
    for (auto& stream : std::exchange(m_sendStreams, { }))
        stream->closeIfPossible();
    for (auto& stream : std::exchange(m_receiveStreams, { }))
        stream->cancel(Exception { ExceptionCode::NetworkError });
    m_datagrams->readable().cancel(Exception { ExceptionCode::NetworkError });
    m_datagrams->writable().closeIfPossible();
    m_incomingBidirectionalStreams->cancel(Exception { ExceptionCode::NetworkError });
    m_incomingUnidirectionalStreams->cancel(Exception { ExceptionCode::NetworkError });
    if (closeInfo) {
        m_state = State::Closed;
        // FIXME: The six Safer CPP warnings here and elsewhere in this file are due to the lack of
        // support for const std::pair holding const smart pointers: rdar://155857105.
        m_closed.second->resolve<IDLDictionary<WebTransportCloseInfo>>(*closeInfo);
    } else {
        m_state = State::Failed;
        m_closed.second->reject<IDLInterface<DOMException>>(exception);
        m_ready.second->reject<IDLInterface<DOMException>>(exception);
    }

    m_session = nullptr;
}

WebTransportDatagramDuplexStream& WebTransport::datagrams()
{
    return m_datagrams.get();
}

void WebTransport::createBidirectionalStream(ScriptExecutionContext& context, WebTransportSendStreamOptions&&, Ref<DeferredPromise>&& promise)
{
    // https://www.w3.org/TR/webtransport/#dom-webtransport-createbidirectionalstream
    RefPtr session = m_session;
    if (m_state == State::Closed || m_state == State::Failed || !session)
        return promise->reject(ExceptionCode::InvalidStateError);

    context.enqueueTaskWhenSettled(session->createBidirectionalStream(), WebCore::TaskSource::Networking, [
        promise = WTFMove(promise),
        context = WeakPtr { context },
        protectedThis = Ref { *this },
        session
    ] (auto&& sink) mutable {
        if (!sink)
            return promise->reject(ExceptionCode::InvalidStateError);
        if (!context)
            return promise->reject(ExceptionCode::InvalidStateError);
        auto* globalObject = context->globalObject();
        if (!globalObject)
            return promise->reject(ExceptionCode::InvalidStateError);

        auto identifier = (*sink)->identifier();
        auto& jsDOMGlobalObject = *JSC::jsCast<JSDOMGlobalObject*>(globalObject);
        Ref incomingStream = WebTransportReceiveStreamSource::createIncomingDataSource(protectedThis.get(), identifier);
        auto stream = WebCore::createBidirectionalStream(*session, jsDOMGlobalObject, WTFMove(*sink), incomingStream.copyRef());
        if (stream.hasException())
            return promise->reject(stream.releaseException());
        Ref bidiStream = stream.releaseReturnValue();
        protectedThis->m_sendStreams.add(bidiStream->writable());
        protectedThis->m_receiveStreams.add(bidiStream->readable());
        ASSERT(!protectedThis->m_readStreamSources.get(identifier));
        protectedThis->m_readStreamSources.add(identifier, WTFMove(incomingStream));
        promise->resolveWithNewlyCreated<IDLInterface<WebTransportBidirectionalStream>>(WTFMove(bidiStream));
    });
}

ReadableStream& WebTransport::incomingBidirectionalStreams()
{
    return m_incomingBidirectionalStreams.get();
}

void WebTransport::createUnidirectionalStream(ScriptExecutionContext& context, WebTransportSendStreamOptions&&, Ref<DeferredPromise>&& promise)
{
    // https://www.w3.org/TR/webtransport/#dom-webtransport-createunidirectionalstream
    RefPtr session = m_session;
    if (m_state == State::Closed || m_state == State::Failed || !session)
        return promise->reject(ExceptionCode::InvalidStateError);

    context.enqueueTaskWhenSettled(session->createOutgoingUnidirectionalStream(), WebCore::TaskSource::Networking, [
        promise = WTFMove(promise),
        context = WeakPtr { context },
        protectedThis = Ref { *this },
        session
    ] (auto&& sink) mutable {
        if (!sink)
            return promise->reject(ExceptionCode::InvalidStateError);
        if (!context)
            return promise->reject(ExceptionCode::InvalidStateError);
        auto* globalObject = context->globalObject();
        if (!globalObject)
            return promise->reject(ExceptionCode::InvalidStateError);
        auto& jsDOMGlobalObject = *JSC::jsCast<JSDOMGlobalObject*>(globalObject);
        auto stream = [&] {
            Locker<JSC::JSLock> locker(jsDOMGlobalObject.vm().apiLock());
            return WebTransportSendStream::create(*session, jsDOMGlobalObject, WTFMove(*sink));
        } ();
        if (stream.hasException())
            return promise->reject(stream.releaseException());
        auto sendStream = stream.releaseReturnValue();
        protectedThis->m_sendStreams.add(sendStream);
        promise->resolveWithNewlyCreated<IDLInterface<WebTransportSendStream>>(sendStream);
    });
}

ReadableStream& WebTransport::incomingUnidirectionalStreams()
{
    return m_incomingUnidirectionalStreams.get();
}

void WebTransport::didFail()
{
    cleanup(DOMException::create(ExceptionCode::AbortError), std::nullopt);
}

RefPtr<WebTransportSession> WebTransport::protectedSession()
{
    return m_session;
}

} // namespace WebCore
