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
#include "DatagramByteSource.h"
#include "DatagramSink.h"
#include "DatagramSource.h"
#include "ExceptionOr.h"
#include "HTTPParsers.h"
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
#include "WebTransportDatagramsWritable.h"
#include "WebTransportError.h"
#include "WebTransportOptions.h"
#include "WebTransportReceiveStream.h"
#include "WebTransportReceiveStreamSource.h"
#include "WebTransportReliabilityMode.h"
#include "WebTransportSendGroup.h"
#include "WebTransportSendStream.h"
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

    HashSet<String> uniqueProtocols;
    for (auto& protocol : options.protocols) {
        if (!isValidHTTPToken(protocol))
            return Exception { ExceptionCode::SyntaxError };

        auto utf8 = protocol.utf8();
        if (utf8.isEmpty() || utf8.length() > 512)
            return Exception { ExceptionCode::SyntaxError };

        if (!uniqueProtocols.add(protocol).isNewEntry)
            return Exception { ExceptionCode::SyntaxError };
    }

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

    RefPtr<DatagramSource> datagramSource;
    RefPtr<ReadableStream> incomingDatagrams;
    if (options.datagramsReadableMode) {
        Ref datagramByteSource = DatagramByteSource::create();
        ReadableByteStreamController::PullAlgorithm pullAlgorithm = [datagramByteSource](auto& globalObject, auto&& controller) {
            auto [promise, deferred] = createPromiseAndWrapper(globalObject);
            datagramByteSource->pull(globalObject, controller, WTFMove(deferred));
            return promise;
        };

        ReadableByteStreamController::CancelAlgorithm cancelAlgorithm = [datagramByteSource](auto& globalObject, auto&&, auto&&) {
            auto [promise, deferred] = createPromiseAndWrapper(globalObject);
            datagramByteSource->cancel(WTFMove(deferred));
            return promise;
        };

        incomingDatagrams = ReadableStream::createReadableByteStream(domGlobalObject, WTFMove(pullAlgorithm), WTFMove(cancelAlgorithm), {
            .isReachableFromOpaqueRootIfPulling = ReadableStream::IsReachableFromOpaqueRootIfPulling::Yes
        });
        datagramSource = WTFMove(datagramByteSource);
    } else {
        Ref datagramDefaultSource = DatagramDefaultSource::create();
        auto readableOrException = ReadableStream::create(domGlobalObject, datagramDefaultSource.copyRef());
        if (readableOrException.hasException())
            return readableOrException.releaseException();
        incomingDatagrams = readableOrException.releaseReturnValue();
        datagramSource = WTFMove(datagramDefaultSource);
    }

    RefPtr socketProvider = context.socketProvider();
    if (!socketProvider) {
        ASSERT_NOT_REACHED();
        return Exception { ExceptionCode::InvalidStateError };
    }

    auto datagrams = WebTransportDatagramDuplexStream::create(incomingDatagrams.releaseNonNull());

    auto transport = adoptRef(*new WebTransport(context, domGlobalObject, incomingBidirectionalStreams.releaseReturnValue(), incomingUnidirectionalStreams.releaseReturnValue(), options, WTFMove(datagrams), datagramSource.releaseNonNull(), WTFMove(receiveStreamSource), WTFMove(bidirectionalStreamSource)));
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
    m_datagrams->attachTo(*this);

    context.enqueueTaskWhenSettled(WTFMove(promise), TaskSource::Networking, [this, protectedThis = Ref { *this }] (auto&& result) mutable {
        if (!result) {
            return cleanupWithSessionError();
        }
        auto& connectionInfo = result.value();
        m_protocol = WTFMove(connectionInfo.protocol);
        m_reliability = connectionInfo.reliabilityMode;
        m_state = State::Connected;
        m_ready.second->resolve();
    });
}

WebTransport::WebTransport(ScriptExecutionContext& context, JSDOMGlobalObject& globalObject, Ref<ReadableStream>&& incomingBidirectionalStreams, Ref<ReadableStream>&& incomingUnidirectionalStreams, const WebTransportOptions& options, Ref<WebTransportDatagramDuplexStream>&& datagrams, Ref<DatagramSource>&& datagramSource, Ref<WebTransportReceiveStreamSource>&& receiveStreamSource, Ref<WebTransportBidirectionalStreamSource>&& bidirectionalStreamSource)
    : ActiveDOMObject(&context)
    , m_incomingBidirectionalStreams(WTFMove(incomingBidirectionalStreams))
    , m_incomingUnidirectionalStreams(WTFMove(incomingUnidirectionalStreams))
    , m_ready(createPromiseAndWrapper(globalObject))
    , m_congestionControl(options.congestionControl)
    , m_anticipatedConcurrentIncomingUnidirectionalStreams(options.anticipatedConcurrentIncomingUnidirectionalStreams)
    , m_anticipatedConcurrentIncomingBidirectionalStreams(options.anticipatedConcurrentIncomingBidirectionalStreams)
    , m_closed(createPromiseAndWrapper(globalObject))
    , m_draining(createPromiseAndWrapper(globalObject))
    , m_datagrams(WTFMove(datagrams))
    , m_datagramSource(WTFMove(datagramSource))
    , m_receiveStreamSource(WTFMove(receiveStreamSource))
    , m_bidirectionalStreamSource(WTFMove(bidirectionalStreamSource))
{
    context.createdWebTransport(*this);
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

static ExceptionOr<Ref<WebTransportBidirectionalStream>> createBidirectionalStream(WebTransport& transport, WebTransportSession& session, JSDOMGlobalObject& globalObject, Ref<WebTransportSendStreamSink>&& sink, Ref<WebTransportReceiveStreamSource>&& source)
{
    auto identifier = sink->identifier();
    auto sendStream = [&] {
        Locker<JSC::JSLock> locker(globalObject.vm().apiLock());
        return WebTransportSendStream::create(transport, globalObject, WTFMove(sink));
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
    auto stream = WebCore::createBidirectionalStream(*this, *session, jsDOMGlobalObject, WTFMove(sink), incomingStream.copyRef());
    if (stream.hasException())
        return;
    Ref bidiStream = stream.releaseReturnValue();
    bool received = m_bidirectionalStreamSource->receiveIncomingStream(jsDOMGlobalObject, bidiStream);
    if (received) {
        m_sendStreams.add(bidiStream->writable());
        m_receiveStreams.add(bidiStream->readable());
        ASSERT(!m_readStreamSources.contains(identifier));
        m_readStreamSources.add(identifier, WTFMove(incomingStream));
        ASSERT(!m_writeStreams.contains(identifier));
        m_writeStreams.add(identifier, bidiStream->writable());
    } else
        protectedSession()->destroyStream(identifier, std::nullopt);
}

void WebTransport::streamReceiveBytes(WebTransportStreamIdentifier identifier, std::span<const uint8_t> span, bool withFin, std::optional<Exception>&& exception)
{
    ASSERT(m_readStreamSources.contains(identifier));
    if (RefPtr source = m_readStreamSources.get(identifier))
        source->receiveBytes(span, withFin, WTFMove(exception));
}

void WebTransport::streamReceiveError(WebTransportStreamIdentifier identifier, uint64_t errorCode)
{
    RefPtr context = scriptExecutionContext();
    if (!context)
        return;
    auto* globalObject = context->globalObject();
    if (!globalObject)
        return;
    if (!m_session)
        return;

    ASSERT(m_readStreamSources.contains(identifier));
    if (RefPtr source = m_readStreamSources.get(identifier))
        source->receiveError(*globalObject, errorCode);
}

void WebTransport::streamSendError(WebTransportStreamIdentifier identifier, uint64_t errorCode)
{
    RefPtr context = scriptExecutionContext();
    if (!context)
        return;
    auto* globalObject = context->globalObject();
    if (!globalObject)
        return;
    if (!m_session)
        return;

    ASSERT(m_writeStreams.contains(identifier));
    if (RefPtr stream = m_writeStreams.get(identifier)) {
        auto& jsDOMGlobalObject = *JSC::jsCast<JSDOMGlobalObject*>(globalObject);
        Locker<JSC::JSLock> locker(jsDOMGlobalObject.vm().apiLock());

        auto error = WebTransportError::create(String(emptyString()), WebTransportErrorOptions {
            WebTransportErrorSource::Stream,
            static_cast<unsigned>(errorCode)
        });
        auto jsError = toJS(globalObject, &jsDOMGlobalObject, error.get());
        stream->errorIfPossible(jsDOMGlobalObject, jsError);
    }
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

std::optional<uint16_t> WebTransport::anticipatedConcurrentIncomingUnidirectionalStreams()
{
    return m_anticipatedConcurrentIncomingUnidirectionalStreams;
}

void WebTransport::setAnticipatedConcurrentIncomingUnidirectionalStreams(std::optional<uint16_t> value)
{
    m_anticipatedConcurrentIncomingUnidirectionalStreams = value;
}

std::optional<uint16_t> WebTransport::anticipatedConcurrentIncomingBidirectionalStreams()
{
    return m_anticipatedConcurrentIncomingBidirectionalStreams;
}

void WebTransport::setAnticipatedConcurrentIncomingBidirectionalStreams(std::optional<uint16_t> value)
{
    m_anticipatedConcurrentIncomingBidirectionalStreams = value;
}

String& WebTransport::protocol()
{
    return m_protocol;
}

bool WebTransport::supportsReliableOnly()
{
    return true;
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

void WebTransport::cleanupContext(ScriptExecutionContext& context)
{
    // https://www.w3.org/TR/webtransport/#web-transport-context-cleanup-steps
    if (m_state == State::Connected) {
        m_state = State::Failed;
        if (auto session = std::exchange(m_session, nullptr))
            session->terminate(0, { });
        context.postTask([protectedThis = Ref { *this }] (auto&) {
            protectedThis->cleanupWithSessionError();
        });
    }
    if (m_state == State::Connecting)
        m_state = State::Failed;
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
    RefPtr context = scriptExecutionContext();
    if (!context)
        return;
    auto* globalObject = context->globalObject();
    if (!globalObject)
        return;

    auto& jsDOMGlobalObject = *jsDynamicCast<JSDOMGlobalObject*>(globalObject);

    auto jsException = [&] {
        Locker<JSC::JSLock> locker(jsDOMGlobalObject.vm().apiLock());
        return toJS(&jsDOMGlobalObject, &jsDOMGlobalObject, exception.get());
    } ();

    // https://www.w3.org/TR/webtransport/#webtransport-cleanup
    for (auto& stream : std::exchange(m_sendStreams, { }))
        stream->errorIfPossible(jsDOMGlobalObject, jsException);

    m_writeStreams = { };

    auto readStreamSources = std::exchange(m_readStreamSources, { });
    for (auto& source : readStreamSources.values())
        source->error(jsDOMGlobalObject, jsException);

    std::exchange(m_receiveStreams, { });

    if (closeInfo) {
        m_state = State::Closed;
        // FIXME: The six Safer CPP warnings here and elsewhere in this file are due to the lack of
        // support for const std::pair holding const smart pointers: rdar://155857105.
        m_closed.second->resolve<IDLDictionary<WebTransportCloseInfo>>(*closeInfo);
        m_incomingBidirectionalStreams->close();
        m_incomingUnidirectionalStreams->close();
        m_datagrams->readable().close();
        for (Ref datagramsWritable : std::exchange(m_datagramsWritables, { }))
            datagramsWritable->closeIfPossible();
    } else {
        m_state = State::Failed;
        m_closed.second->reject<IDLInterface<DOMException>>(exception);
        m_ready.second->reject<IDLInterface<DOMException>>(exception);
        m_bidirectionalStreamSource->error(jsDOMGlobalObject, jsException);
        m_receiveStreamSource->error(jsDOMGlobalObject, jsException);
        m_datagramSource->error(jsDOMGlobalObject, jsException);
        for (Ref datagramsWritable : std::exchange(m_datagramsWritables, { }))
            datagramsWritable->errorIfPossible(jsDOMGlobalObject, jsException);
    }

    m_session = nullptr;
}

void WebTransport::datagramsWritableCreated(WebTransportDatagramsWritable& writable)
{
    m_datagramsWritables.add(writable);
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
        auto stream = WebCore::createBidirectionalStream(protectedThis, *session, jsDOMGlobalObject, WTFMove(*sink), incomingStream.copyRef());
        if (stream.hasException())
            return promise->reject(stream.releaseException());
        Ref bidiStream = stream.releaseReturnValue();
        protectedThis->m_sendStreams.add(bidiStream->writable());
        protectedThis->m_receiveStreams.add(bidiStream->readable());
        ASSERT(!protectedThis->m_readStreamSources.get(identifier));
        protectedThis->m_readStreamSources.add(identifier, WTFMove(incomingStream));
        ASSERT(!protectedThis->m_writeStreams.contains(identifier));
        protectedThis->m_writeStreams.add(identifier, bidiStream->writable());
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
        protectedThis = Ref { *this }
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
        auto stream = [&] {
            Locker<JSC::JSLock> locker(jsDOMGlobalObject.vm().apiLock());
            return WebTransportSendStream::create(protectedThis, jsDOMGlobalObject, WTFMove(*sink));
        } ();
        if (stream.hasException())
            return promise->reject(stream.releaseException());
        auto sendStream = stream.releaseReturnValue();
        protectedThis->m_sendStreams.add(sendStream);
        ASSERT(!protectedThis->m_writeStreams.contains(identifier));
        protectedThis->m_writeStreams.add(identifier, sendStream);
        promise->resolveWithNewlyCreated<IDLInterface<WebTransportSendStream>>(sendStream);
    });
}

ReadableStream& WebTransport::incomingUnidirectionalStreams()
{
    return m_incomingUnidirectionalStreams.get();
}

Ref<WebTransportSendGroup> WebTransport::createSendGroup()
{
    return WebTransportSendGroup::create(*this);
}

void WebTransport::didFail(std::optional<uint32_t>&& code, String&& message)
{
    if (code) {
        WebTransportCloseInfo closeInfo {
            .closeCode = code.value_or(0),
            .reason = message
        };
        cleanup(WebTransportError::create(String(emptyString()), WebTransportErrorOptions {
            WebTransportErrorSource::Session,
            code
        }), WTFMove(closeInfo));
    } else
        cleanupWithSessionError();
}

void WebTransport::didDrain()
{
    m_state = State::Draining;
    m_draining.second->resolve();
}

RefPtr<WebTransportSession> WebTransport::protectedSession()
{
    return m_session;
}

} // namespace WebCore
