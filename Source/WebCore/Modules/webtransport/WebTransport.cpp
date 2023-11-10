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

#include "DatagramSink.h"
#include "DatagramSource.h"
#include "JSDOMPromise.h"
#include "JSDOMPromiseDeferred.h"
#include "JSWebTransportBidirectionalStream.h"
#include "JSWebTransportSendStream.h"
#include "ReadableStream.h"
#include "SocketProvider.h"
#include "WebTransportBidirectionalStreamConstructionParameters.h"
#include "WebTransportBidirectionalStreamSource.h"
#include "WebTransportCloseInfo.h"
#include "WebTransportCongestionControl.h"
#include "WebTransportDatagramDuplexStream.h"
#include "WebTransportError.h"
#include "WebTransportReceiveStream.h"
#include "WebTransportReceiveStreamSource.h"
#include "WebTransportReliabilityMode.h"
#include "WebTransportSession.h"
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

    auto receiveStreamSource = WebTransportReceiveStreamSource::create();
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

    auto socketProvider = context.socketProvider();
    if (!socketProvider) {
        ASSERT_NOT_REACHED();
        return Exception { ExceptionCode::InvalidStateError };
    }

    auto datagrams = WebTransportDatagramDuplexStream::create(incomingDatagrams.releaseReturnValue(), outgoingDatagrams.releaseReturnValue());

    auto transport = adoptRef(*new WebTransport(context, domGlobalObject, incomingBidirectionalStreams.releaseReturnValue(), incomingUnidirectionalStreams.releaseReturnValue(), options.congestionControl, WTFMove(datagrams), WTFMove(datagramSource), WTFMove(receiveStreamSource), WTFMove(bidirectionalStreamSource)));
    datagramSink->attachTo(transport);
    transport->suspendIfNeeded();
    transport->initializeOverHTTP(*socketProvider, context, WTFMove(parsedURL), dedicated, options.requireUnreliable, options.congestionControl, WTFMove(options.serverCertificateHashes));
    return transport;
}

void WebTransport::initializeOverHTTP(SocketProvider& provider, ScriptExecutionContext& context, URL&& url, bool, bool, WebTransportCongestionControl, Vector<WebTransportHash>&&)
{
    // FIXME: Do origin checks as outlined in https://www.w3.org/TR/webtransport/#initialize-webtransport-over-http

    // FIXME: Rename SocketProvider to NetworkProvider or something to reflect that it provides a little more than just simple sockets. SocketAndTransportProvider?
    provider.initializeWebTransportSession(context, url, [this, protectedThis = Ref { *this }] (RefPtr<WebTransportSession>&& session) {
        m_session = WTFMove(session);
        if (!m_session) {
            m_state = State::Failed;
            m_ready.second->reject();
            return;
        }
        m_session->attachClient(*this);
        m_state = State::Connected;
        m_ready.second->resolve();
    });
}

static std::pair<Ref<DOMPromise>, Ref<DeferredPromise>> createPromiseAndWrapper(JSDOMGlobalObject& globalObject)
{
    JSC::JSLockHolder lock(globalObject.vm());
    auto deferredPromise = DeferredPromise::create(globalObject);
    auto domPromise = DOMPromise::create(globalObject, *JSC::jsCast<JSC::JSPromise*>(deferredPromise->promise()));
    return { WTFMove(domPromise), deferredPromise.releaseNonNull() };
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

const char* WebTransport::activeDOMObjectName() const
{
    return "WebTransport";
}

bool WebTransport::virtualHasPendingActivity() const
{
    // https://www.w3.org/TR/webtransport/#web-transport-gc
    return m_state == State::Connecting || m_state == State::Connected;
}

void WebTransport::receiveDatagram(std::span<const uint8_t> datagram)
{
    m_datagramSource->receiveDatagram(datagram);
}

void WebTransport::receiveIncomingUnidirectionalStream()
{
    m_receiveStreamSource->receiveIncomingStream();
}

void WebTransport::receiveBidirectionalStream()
{
    m_bidirectionalStreamSource->receiveIncomingStream();
}

void WebTransport::getStats(Ref<DeferredPromise>&& promise)
{
    promise->reject(ExceptionCode::NotSupportedError);
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
        string = CString(string.data(), 1024);
    else
        return WTFMove(string);

    while (true) {
        if (!string.length())
            return WTFMove(string);
        auto decoded = String::fromUTF8(string.data(), string.length());
        if (!decoded)
            string = CString(string.data(), string.length() - 1);
        else
            return WTFMove(string);
    }
}

void WebTransport::close(WebTransportCloseInfo&& closeInfo)
{
    // https://www.w3.org/TR/webtransport/#dom-webtransport-close
    if (m_state == State::Closed || m_state == State::Failed)
        return;
    if (m_state == State::Connecting) {
        auto error = WebTransportError::create(String(emptyString()), WebTransportErrorOptions {
            WebTransportErrorSource::Session,
            std::nullopt
        });
        return cleanup(WTFMove(error), std::nullopt);
    }
    if (auto session = std::exchange(m_session, nullptr))
        session->terminate(closeInfo.closeCode, trimToValidUTF8Length1024(closeInfo.reason.utf8()));
    cleanup(DOMException::create(ExceptionCode::AbortError), WTFMove(closeInfo));
}

void WebTransport::cleanup(Ref<DOMException>&&, std::optional<WebTransportCloseInfo>&& closeInfo)
{
    // https://www.w3.org/TR/webtransport/#webtransport-cleanup

    m_sendStreams = { }; // FIXME: Error each send stream.
    // FIXME: The use of cancel and the use of NetworkError aren't quite correct in this function.
    for (auto& stream : std::exchange(m_receiveStreams, { }))
        stream->cancel(Exception { ExceptionCode::NetworkError });

    if (closeInfo) {
        // FIXME: Resolve with closeInfo.
        m_closed.second->resolve();
        m_incomingBidirectionalStreams->cancel(Exception { ExceptionCode::NetworkError });
        m_incomingUnidirectionalStreams->cancel(Exception { ExceptionCode::NetworkError });
        m_state = State::Closed;
    } else {
        m_closed.second->reject(Exception { ExceptionCode::NetworkError });
        m_ready.second->reject(Exception { ExceptionCode::NetworkError });
        m_incomingBidirectionalStreams->cancel(Exception { ExceptionCode::NetworkError });
        m_incomingUnidirectionalStreams->cancel(Exception { ExceptionCode::NetworkError });
        m_state = State::Failed;
    }
}

WebTransportDatagramDuplexStream& WebTransport::datagrams()
{
    return m_datagrams.get();
}

void WebTransport::createBidirectionalStream(ScriptExecutionContext& context, WebTransportSendStreamOptions&&, Ref<DeferredPromise>&& promise)
{
    // https://www.w3.org/TR/webtransport/#dom-webtransport-createbidirectionalstream
    if (m_state == State::Closed || m_state == State::Failed || !m_session)
        return promise->reject(ExceptionCode::InvalidStateError);
    m_session->createBidirectionalStream([promise = WTFMove(promise), context = WeakPtr { context }] (std::optional<WebTransportBidirectionalStreamConstructionParameters>&& parameters) {
        if (!parameters)
            return promise->reject(nullptr);
        if (!context)
            return promise->reject(nullptr);
        auto* globalObject = context->globalObject();
        if (!globalObject)
            return promise->reject(nullptr);
        auto& jsDOMGlobalObject = *JSC::jsCast<JSDOMGlobalObject*>(globalObject);
        auto sendStream = [&] {
            Locker<JSC::JSLock> locker(jsDOMGlobalObject.vm().apiLock());
            return WebTransportSendStream::create(jsDOMGlobalObject, WTFMove(parameters->sink));
        } ();
        if (sendStream.hasException())
            return promise->reject(sendStream.releaseException());
        auto receiveStream = [&] {
            Locker<JSC::JSLock> locker(jsDOMGlobalObject.vm().apiLock());
            return WebTransportReceiveStream::create(jsDOMGlobalObject, WTFMove(parameters->source));
        } ();
        if (receiveStream.hasException())
            return promise->reject(receiveStream.releaseException());
        auto stream = WebTransportBidirectionalStream::create(receiveStream.releaseReturnValue(), sendStream.releaseReturnValue());
        promise->resolveWithNewlyCreated<IDLInterface<WebTransportBidirectionalStream>>(WTFMove(stream));
    });
}

ReadableStream& WebTransport::incomingBidirectionalStreams()
{
    return m_incomingBidirectionalStreams.get();
}

void WebTransport::createUnidirectionalStream(ScriptExecutionContext& context, WebTransportSendStreamOptions&&, Ref<DeferredPromise>&& promise)
{
    // https://www.w3.org/TR/webtransport/#dom-webtransport-createunidirectionalstream
    if (m_state == State::Closed || m_state == State::Failed || !m_session)
        return promise->reject(ExceptionCode::InvalidStateError);
    m_session->createOutgoingUnidirectionalStream([promise = WTFMove(promise), context = WeakPtr { context }] (RefPtr<WritableStreamSink>&& sink) {
        if (!sink)
            return promise->reject(nullptr);
        if (!context)
            return promise->reject(nullptr);
        auto* globalObject = context->globalObject();
        if (!globalObject)
            return promise->reject(nullptr);
        auto& jsDOMGlobalObject = *JSC::jsCast<JSDOMGlobalObject*>(globalObject);
        auto stream = [&] {
            Locker<JSC::JSLock> locker(jsDOMGlobalObject.vm().apiLock());
            return WebTransportSendStream::create(jsDOMGlobalObject, sink.releaseNonNull());
        } ();
        if (stream.hasException())
            return promise->reject(stream.releaseException());
        promise->resolveWithNewlyCreated<IDLInterface<WebTransportSendStream>>(stream.releaseReturnValue());
    });
}

ReadableStream& WebTransport::incomingUnidirectionalStreams()
{
    return m_incomingUnidirectionalStreams.get();
}

}
