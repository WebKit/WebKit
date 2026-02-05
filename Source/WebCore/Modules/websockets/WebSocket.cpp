/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2015-2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebSocket.h"

#include "Blob.h"
#include "CloseEvent.h"
#include "ContentSecurityPolicy.h"
#include "ContextDestructionObserverInlines.h"
#include "DNS.h"
#include "Document.h"
#include "Event.h"
#include "EventListener.h"
#include "EventNames.h"
#include "EventTargetInterfaces.h"
#include "FrameDestructionObserverInlines.h"
#include "FrameLoader.h"
#include "InspectorInstrumentation.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "LocalFrameLoaderClient.h"
#include "Logging.h"
#include "MessageEvent.h"
#include "MixedContentChecker.h"
#include "ResourceLoadObserver.h"
#include "ScriptController.h"
#include "ScriptExecutionContext.h"
#include "SocketProvider.h"
#include "ThreadableWebSocketChannel.h"
#include "WebSocketChannelInspector.h"
#include "WorkerGlobalScope.h"
#include "WorkerLoaderProxy.h"
#include "WorkerThread.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <JavaScriptCore/ArrayBufferView.h>
#include <JavaScriptCore/ScriptCallStack.h>
#include <wtf/HashSet.h>
#include <wtf/HexNumber.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

#if USE(WEB_THREAD)
#include "WebCoreThreadRun.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebSocket);

Lock WebSocket::s_allActiveWebSocketsLock;

const size_t maxReasonSizeInBytes = 123;

static inline bool isValidProtocolCharacter(char16_t character)
{
    // Hybi-10 says "(Subprotocol string must consist of) characters in the range U+0021 to U+007E not including
    // separator characters as defined in [RFC2616]."
    const char16_t minimumProtocolCharacter = '!'; // U+0021.
    const char16_t maximumProtocolCharacter = '~'; // U+007E.
    return character >= minimumProtocolCharacter && character <= maximumProtocolCharacter
        && character != '"' && character != '(' && character != ')' && character != ',' && character != '/'
        && !(character >= ':' && character <= '@') // U+003A - U+0040 (':', ';', '<', '=', '>', '?', '@').
        && !(character >= '[' && character <= ']') // U+005B - U+005D ('[', '\\', ']').
        && character != '{' && character != '}';
}

static bool isValidProtocolString(StringView protocol)
{
    if (protocol.isEmpty())
        return false;
    for (auto codeUnit : protocol.codeUnits()) {
        if (!isValidProtocolCharacter(codeUnit))
            return false;
    }
    return true;
}

static String encodeProtocolString(const String& protocol)
{
    StringBuilder builder;
    for (size_t i = 0; i < protocol.length(); i++) {
        if (protocol[i] < 0x20 || protocol[i] > 0x7E)
            builder.append("\\u"_s, hex(protocol[i], 4));
        else if (protocol[i] == 0x5c)
            builder.append("\\\\"_s);
        else
            builder.append(protocol[i]);
    }
    return builder.toString();
}

static String joinStrings(const Vector<String>& strings, ASCIILiteral separator)
{
    StringBuilder builder;
    for (size_t i = 0; i < strings.size(); ++i) {
        if (i)
            builder.append(separator);
        builder.append(strings[i]);
    }
    return builder.toString();
}

static unsigned saturateAdd(unsigned a, unsigned b)
{
    if (std::numeric_limits<unsigned>::max() - a < b)
        return std::numeric_limits<unsigned>::max();
    return a + b;
}

ASCIILiteral WebSocket::subprotocolSeparator()
{
    return ", "_s;
}

WebSocket::WebSocket(ScriptExecutionContext& context)
    : ActiveDOMObject(&context)
    , m_subprotocol(emptyString())
    , m_extensions(emptyString())
{
    Locker locker { allActiveWebSocketsLock() };
    allActiveWebSockets().add(this);
}

WebSocket::~WebSocket()
{
    {
        Locker locker { allActiveWebSocketsLock() };
        allActiveWebSockets().remove(this);
    }

    if (RefPtr channel = m_channel)
        channel->disconnect();
}

ExceptionOr<Ref<WebSocket>> WebSocket::create(ScriptExecutionContext& context, const String& url)
{
    return create(context, url, Vector<String> { });
}

ExceptionOr<Ref<WebSocket>> WebSocket::create(ScriptExecutionContext& context, const String& url, const Vector<String>& protocols)
{
    if (url.isNull())
        return Exception { ExceptionCode::SyntaxError };

    auto socket = adoptRef(*new WebSocket(context));
    socket->suspendIfNeeded();

    auto result = socket->connect(context.completeURL(url, ScriptExecutionContext::ForceUTF8::Yes).string(), protocols);
    if (result.hasException())
        return result.releaseException();

    return socket;
}

ExceptionOr<Ref<WebSocket>> WebSocket::create(ScriptExecutionContext& context, const String& url, const String& protocol)
{
    return create(context, url, Vector<String> { 1, protocol });
}

HashSet<CheckedPtr<WebSocket>>& WebSocket::allActiveWebSockets()
{
    static NeverDestroyed<HashSet<CheckedPtr<WebSocket>>> activeWebSockets;
    return activeWebSockets;
}

Lock& WebSocket::allActiveWebSocketsLock()
{
    return s_allActiveWebSocketsLock;
}

ExceptionOr<void> WebSocket::connect(const String& url)
{
    return connect(url, Vector<String> { });
}

ExceptionOr<void> WebSocket::connect(const String& url, const String& protocol)
{
    return connect(url, Vector<String> { 1, protocol });
}

void WebSocket::failAsynchronously()
{
    queueTaskKeepingObjectAlive(*this, TaskSource::WebSocket, [](auto& socket) {
        // We must block this connection. Instead of throwing an exception, we indicate this
        // using the error event. But since this code executes as part of the WebSocket's
        // constructor, we have to wait until the constructor has completed before firing the
        // event; otherwise, users can't connect to the event.

        socket.dispatchErrorEventIfNeeded();
        socket.stop();
    });
}

ExceptionOr<void> WebSocket::connect(const String& url, const Vector<String>& protocols)
{
    LOG(Network, "WebSocket %p connect() url='%s'", this, url.utf8().data());
    m_url = URL { url };

    Ref context = *scriptExecutionContext();

    if (!m_url.isValid()) {
        context->addConsoleMessage(MessageSource::JS, MessageLevel::Error, makeString("Invalid url for WebSocket "_s, m_url.stringCenterEllipsizedToLength()));
        m_state = CLOSED;
        return Exception { ExceptionCode::SyntaxError };
    }

    if (m_url.protocolIs("http"_s))
        m_url.setProtocol("ws"_s);
    else if (m_url.protocolIs("https"_s))
        m_url.setProtocol("wss"_s);

    if (!m_url.protocolIs("ws"_s) && !m_url.protocolIs("wss"_s)) {
        context->addConsoleMessage(MessageSource::JS, MessageLevel::Error, makeString("Wrong url scheme for WebSocket "_s, m_url.stringCenterEllipsizedToLength()));
        m_state = CLOSED;
        return Exception { ExceptionCode::SyntaxError };
    }
    if (m_url.hasFragmentIdentifier()) {
        context->addConsoleMessage(MessageSource::JS, MessageLevel::Error, makeString("URL has fragment component "_s, m_url.stringCenterEllipsizedToLength()));
        m_state = CLOSED;
        return Exception { ExceptionCode::SyntaxError };
    }

    ASSERT(context->contentSecurityPolicy());
    CheckedRef contentSecurityPolicy = *context->contentSecurityPolicy();

    contentSecurityPolicy->upgradeInsecureRequestIfNeeded(m_url, ContentSecurityPolicy::InsecureRequestType::Load);

    if (!portAllowed(m_url) || isIPAddressDisallowed(m_url)) {
        String message;
        if (isIPAddressDisallowed(m_url))
            message = makeString("WebSocket address "_s, m_url.host(), " blocked"_s);
        else if (m_url.port())
            message = makeString("WebSocket port "_s, m_url.port().value(), " blocked"_s);
        else
            message = "WebSocket without port blocked"_s;
        context->addConsoleMessage(MessageSource::JS, MessageLevel::Error, message);
        failAsynchronously();
        return { };
    }

    // FIXME: Convert this to check the isolated world's Content Security Policy once webkit.org/b/104520 is solved.
    if (!context->shouldBypassMainWorldContentSecurityPolicy() && !contentSecurityPolicy->allowConnectToSource(m_url)) {
        m_state = CLOSED;

        // FIXME: Should this be throwing an exception?
        return Exception { ExceptionCode::SecurityError };
    }

    if (RefPtr provider = context->socketProvider())
        m_channel = ThreadableWebSocketChannel::create(context.get(), *this, *provider);

    // Every ScriptExecutionContext should have a SocketProvider.
    RELEASE_ASSERT(m_channel);

    // FIXME: There is a disagreement about restriction of subprotocols between WebSocket API and hybi-10 protocol
    // draft. The former simply says "only characters in the range U+0021 to U+007E are allowed," while the latter
    // imposes a stricter rule: "the elements MUST be non-empty strings with characters as defined in [RFC2616],
    // and MUST all be unique strings."
    //
    // Here, we throw SyntaxError if the given protocols do not meet the latter criteria. This behavior does not
    // comply with WebSocket API specification, but it seems to be the only reasonable way to handle this conflict.
    for (auto& protocol : protocols) {
        if (!isValidProtocolString(protocol)) {
            context->addConsoleMessage(MessageSource::JS, MessageLevel::Error, makeString("Wrong protocol for WebSocket '"_s, encodeProtocolString(protocol), '\''));
            m_state = CLOSED;
            return Exception { ExceptionCode::SyntaxError };
        }
    }
    HashSet<String> visited;
    for (auto& protocol : protocols) {
        if (!visited.add(protocol).isNewEntry) {
            context->addConsoleMessage(MessageSource::JS, MessageLevel::Error, makeString("WebSocket protocols contain duplicates: '"_s, encodeProtocolString(protocol), '\''));
            m_state = CLOSED;
            return Exception { ExceptionCode::SyntaxError };
        }
    }

    RunLoop::mainSingleton().dispatch([targetURL = m_url.isolatedCopy(), mainFrameURL = context->url().isolatedCopy()]() {
        ResourceLoadObserver::singleton().logWebSocketLoading(targetURL, mainFrameURL);
    });

    if (RefPtr document = dynamicDowncast<Document>(context)) {
        RefPtr frame = document->frame();
        // FIXME: make the mixed content check equivalent to the non-document mixed content check currently in WorkerThreadableWebSocketChannel::Bridge::connect()
        // In particular we need to match the error messaging in the console and the inspector instrumentation. See WebSocketChannel::fail.
        if (!frame || MixedContentChecker::shouldBlockRequest(*frame, m_url)) {
            failAsynchronously();
            return { };
        }
    }

    String protocolString;
    if (!protocols.isEmpty())
        protocolString = joinStrings(protocols, subprotocolSeparator());

    if (channel()->connect(m_url, protocolString) == ThreadableWebSocketChannel::ConnectStatus::KO) {
        failAsynchronously();
        return { };
    }

    auto reportRegistrableDomain = [domain = RegistrableDomain(m_url).isolatedCopy()](auto& context) mutable {
        if (RefPtr frame = downcast<Document>(context).frame())
            frame->loader().client().didLoadFromRegistrableDomain(WTF::move(domain));
    };
    if (is<Document>(context))
        reportRegistrableDomain(context.get());
    else if (CheckedPtr workerLoaderProxy = downcast<WorkerGlobalScope>(context)->thread()->workerLoaderProxy())
        workerLoaderProxy->postTaskToLoader(WTF::move(reportRegistrableDomain));

    if (!m_origin)
        lazyInitialize(m_origin, SecurityOrigin::create(m_url));
    m_pendingActivity = makePendingActivity(*this);

    return { };
}

ExceptionOr<void> WebSocket::send(const String& message)
{
    LOG(Network, "WebSocket %p send() Sending String '%s'", this, message.utf8().data());
    if (m_state == CONNECTING)
        return Exception { ExceptionCode::InvalidStateError };
    auto utf8 = message.utf8(StrictConversionReplacingUnpairedSurrogatesWithFFFD);
    // No exception is raised if the connection was once established but has subsequently been closed.
    if (m_state == CLOSING || m_state == CLOSED) {
        size_t payloadSize = utf8.length();
        m_bufferedAmountAfterClose = saturateAdd(m_bufferedAmountAfterClose, payloadSize);
        m_bufferedAmountAfterClose = saturateAdd(m_bufferedAmountAfterClose, getFramingOverhead(payloadSize));
        return { };
    }
    // FIXME: WebSocketChannel also has a m_bufferedAmount. Remove that one. This one is the correct one accessed by JS.
    m_bufferedAmount = saturateAdd(m_bufferedAmount, utf8.length());
    channel()->send(WTF::move(utf8));
    return { };
}

ExceptionOr<void> WebSocket::send(ArrayBuffer& binaryData)
{
    LOG(Network, "WebSocket %p send() Sending ArrayBuffer %p", this, &binaryData);
    if (m_state == CONNECTING)
        return Exception { ExceptionCode::InvalidStateError };
    if (m_state == CLOSING || m_state == CLOSED) {
        unsigned payloadSize = binaryData.byteLength();
        m_bufferedAmountAfterClose = saturateAdd(m_bufferedAmountAfterClose, payloadSize);
        m_bufferedAmountAfterClose = saturateAdd(m_bufferedAmountAfterClose, getFramingOverhead(payloadSize));
        return { };
    }
    m_bufferedAmount = saturateAdd(m_bufferedAmount, binaryData.byteLength());
    channel()->send(binaryData, 0, binaryData.byteLength());
    return { };
}

ExceptionOr<void> WebSocket::send(ArrayBufferView& arrayBufferView)
{
    LOG(Network, "WebSocket %p send() Sending ArrayBufferView %p", this, &arrayBufferView);

    if (m_state == CONNECTING)
        return Exception { ExceptionCode::InvalidStateError };
    if (m_state == CLOSING || m_state == CLOSED) {
        unsigned payloadSize = arrayBufferView.byteLength();
        m_bufferedAmountAfterClose = saturateAdd(m_bufferedAmountAfterClose, payloadSize);
        m_bufferedAmountAfterClose = saturateAdd(m_bufferedAmountAfterClose, getFramingOverhead(payloadSize));
        return { };
    }
    m_bufferedAmount = saturateAdd(m_bufferedAmount, arrayBufferView.byteLength());
    channel()->send(*arrayBufferView.unsharedBuffer(), arrayBufferView.byteOffset(), arrayBufferView.byteLength());
    return { };
}

ExceptionOr<void> WebSocket::send(Blob& binaryData)
{
    LOG(Network, "WebSocket %p send() Sending Blob '%s'", this, binaryData.url().stringCenterEllipsizedToLength().utf8().data());
    if (m_state == CONNECTING)
        return Exception { ExceptionCode::InvalidStateError };
    if (m_state == CLOSING || m_state == CLOSED) {
        unsigned payloadSize = static_cast<unsigned>(binaryData.size());
        m_bufferedAmountAfterClose = saturateAdd(m_bufferedAmountAfterClose, payloadSize);
        m_bufferedAmountAfterClose = saturateAdd(m_bufferedAmountAfterClose, getFramingOverhead(payloadSize));
        return { };
    }
    m_bufferedAmount = saturateAdd(m_bufferedAmount, binaryData.size());
    channel()->send(binaryData);
    return { };
}

ExceptionOr<void> WebSocket::close(std::optional<unsigned short> optionalCode, const String& reason)
{
    int code = optionalCode ? optionalCode.value() : static_cast<int>(ThreadableWebSocketChannel::CloseEventCodeNotSpecified);
    if (code == ThreadableWebSocketChannel::CloseEventCodeNotSpecified)
        LOG(Network, "WebSocket %p close() without code and reason", this);
    else {
        LOG(Network, "WebSocket %p close() code=%d reason='%s'", this, code, reason.utf8().data());
        if (!(code == ThreadableWebSocketChannel::CloseEventCodeNormalClosure || (ThreadableWebSocketChannel::CloseEventCodeMinimumUserDefined <= code && code <= ThreadableWebSocketChannel::CloseEventCodeMaximumUserDefined)))
            return Exception { ExceptionCode::InvalidAccessError };
        CString utf8 = reason.utf8(StrictConversionReplacingUnpairedSurrogatesWithFFFD);
        if (utf8.length() > maxReasonSizeInBytes) {
            protectedScriptExecutionContext()->addConsoleMessage(MessageSource::JS, MessageLevel::Error, "WebSocket close message is too long."_s);
            return Exception { ExceptionCode::SyntaxError };
        }
    }

    if (m_state == CLOSING || m_state == CLOSED)
        return { };
    if (m_state == CONNECTING) {
        m_state = CLOSING;
        if (RefPtr channel = m_channel)
            channel->fail("WebSocket is closed before the connection is established."_s);
        return { };
    }
    m_state = CLOSING;
    if (RefPtr channel = m_channel)
        channel->close(code, reason);
    return { };
}

RefPtr<ThreadableWebSocketChannel> WebSocket::channel() const
{
    return m_channel;
}

const URL& WebSocket::url() const
{
    return m_url;
}

WebSocket::State WebSocket::readyState() const
{
    return m_state;
}

unsigned WebSocket::bufferedAmount() const
{
    return saturateAdd(m_bufferedAmount, m_bufferedAmountAfterClose);
}

String WebSocket::protocol() const
{
    return m_subprotocol;
}

String WebSocket::extensions() const
{
    return m_extensions;
}

void WebSocket::setBinaryType(BinaryType binaryType)
{
    m_binaryType = binaryType;
}

enum EventTargetInterfaceType WebSocket::eventTargetInterface() const
{
    return EventTargetInterfaceType::WebSocket;
}

ScriptExecutionContext* WebSocket::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

void WebSocket::contextDestroyed()
{
    LOG(Network, "WebSocket %p contextDestroyed()", this);
    ASSERT(!m_channel);
    ASSERT(m_state == CLOSED);
    ActiveDOMObject::contextDestroyed();
}

void WebSocket::suspend(ReasonForSuspension reason)
{
    RefPtr channel = m_channel;
    if (!channel || m_state == CLOSING || m_state == CLOSED)
        return;

    if (reason == ReasonForSuspension::BackForwardCache) {
        // This will cause didClose() to be called.
        channel->fail("WebSocket is closed due to suspension."_s);
        return;
    }

    channel->suspend();
}

void WebSocket::resume()
{
    if (RefPtr channel = m_channel)
        channel->resume();
}

void WebSocket::stop()
{
    if (RefPtr channel = m_channel)
        channel->disconnect();
    m_channel = nullptr;
    m_state = CLOSED;
    ActiveDOMObject::stop();
    m_pendingActivity = nullptr;
}

void WebSocket::didConnect()
{
    LOG(Network, "WebSocket %p didConnect()", this);
    queueTaskKeepingObjectAlive(*this, TaskSource::WebSocket, [](auto& socket) {
        if (socket.m_state == CLOSED)
            return;
        if (socket.m_state != CONNECTING) {
            socket.didClose(0, ClosingHandshakeIncomplete, ThreadableWebSocketChannel::CloseEventCodeAbnormalClosure, emptyString());
            return;
        }
        ASSERT(socket.scriptExecutionContext());
        socket.m_state = OPEN;
        socket.m_subprotocol = socket.m_channel->subprotocol();
        socket.m_extensions = socket.m_channel->extensions();
        socket.dispatchEvent(Event::create(eventNames().openEvent, Event::CanBubble::No, Event::IsCancelable::No));
    });
}

void WebSocket::didReceiveMessage(String&& message)
{
    LOG(Network, "WebSocket %p didReceiveMessage() Text message '%s'", this, message.utf8().data());
    queueTaskKeepingObjectAlive(*this, TaskSource::WebSocket, [message = WTF::move(message)](auto& socket) mutable {
        if (socket.m_state != OPEN)
            return;

        if (InspectorInstrumentation::hasFrontends()) [[unlikely]] {
            if (auto* inspector = socket.m_channel->channelInspector()) {
                auto utf8Message = message.utf8();
                inspector->didReceiveWebSocketFrame(WebSocketChannelInspector::createFrame(byteCast<uint8_t>(utf8Message.span()), WebSocketFrame::OpCode::OpCodeText));
            }
        }
        ASSERT(socket.scriptExecutionContext());
        socket.dispatchEvent(MessageEvent::create(WTF::move(message), socket.m_origin.copyRef()));
    });
}

void WebSocket::didReceiveBinaryData(Vector<uint8_t>&& binaryData)
{
    LOG(Network, "WebSocket %p didReceiveBinaryData() %u byte binary message", this, static_cast<unsigned>(binaryData.size()));
    queueTaskKeepingObjectAlive(*this, TaskSource::WebSocket, [binaryData = WTF::move(binaryData)](auto& socket) mutable {
        if (socket.m_state != OPEN)
            return;

        if (InspectorInstrumentation::hasFrontends()) [[unlikely]] {
            if (auto* inspector = socket.m_channel->channelInspector())
                inspector->didReceiveWebSocketFrame(WebSocketChannelInspector::createFrame(binaryData.span(), WebSocketFrame::OpCode::OpCodeBinary));
        }

        switch (socket.m_binaryType) {
        case BinaryType::Blob:
            // FIXME: We just received the data from NetworkProcess, and are sending it back. This is inefficient.
            socket.dispatchEvent(MessageEvent::create(Blob::create(socket.protectedScriptExecutionContext().get(), WTF::move(binaryData), emptyString()), socket.m_origin.copyRef()));
            break;
        case BinaryType::Arraybuffer:
            socket.dispatchEvent(MessageEvent::create(ArrayBuffer::create(binaryData), socket.m_origin.copyRef()));
            break;
        }
    });
}

void WebSocket::didReceiveMessageError(String&& reason)
{
    LOG(Network, "WebSocket %p didReceiveErrorMessage()", this);
    queueTaskKeepingObjectAlive(*this, TaskSource::WebSocket, [reason = WTF::move(reason)](auto& socket) {
        if (socket.m_state == CLOSED)
            return;
        socket.m_state = CLOSED;
        ASSERT(socket.scriptExecutionContext());

        if (InspectorInstrumentation::hasFrontends()) [[unlikely]] {
            if (auto* inspector = socket.m_channel->channelInspector())
                inspector->didReceiveWebSocketFrameError(reason);
        }

        // FIXME: As per https://html.spec.whatwg.org/multipage/web-sockets.html#feedback-from-the-protocol:concept-websocket-closed, we should synchronously fire a close event.
        socket.dispatchErrorEventIfNeeded();
    });
}

void WebSocket::didUpdateBufferedAmount(unsigned bufferedAmount)
{
    LOG(Network, "WebSocket %p didUpdateBufferedAmount() New bufferedAmount is %u", this, bufferedAmount);
    if (m_state == CLOSED)
        return;
    m_bufferedAmount = bufferedAmount;
}

void WebSocket::didStartClosingHandshake()
{
    LOG(Network, "WebSocket %p didStartClosingHandshake()", this);
    queueTaskKeepingObjectAlive(*this, TaskSource::WebSocket, [](auto& socket) {
        if (socket.m_state == CLOSED)
            return;
        socket.m_state = CLOSING;
    });
}

void WebSocket::didClose(unsigned unhandledBufferedAmount, ClosingHandshakeCompletionStatus closingHandshakeCompletion, unsigned short code, const String& reason)
{
    LOG(Network, "WebSocket %p didClose()", this);
    queueTaskKeepingObjectAlive(*this, TaskSource::WebSocket, [unhandledBufferedAmount, closingHandshakeCompletion, code, reason](auto& socket) {
        if (!socket.m_channel)
            return;

        if (InspectorInstrumentation::hasFrontends()) [[unlikely]] {
            if (auto* inspector = socket.m_channel->channelInspector()) {
                WebSocketFrame closingFrame(WebSocketFrame::OpCodeClose, true, false, false);
                inspector->didReceiveWebSocketFrame(closingFrame);
                inspector->didCloseWebSocket();
            }
        }

        bool wasClean = socket.m_state == CLOSING && !unhandledBufferedAmount && closingHandshakeCompletion == ClosingHandshakeComplete && code != ThreadableWebSocketChannel::CloseEventCodeAbnormalClosure;
        socket.m_state = CLOSED;
        socket.m_bufferedAmount = unhandledBufferedAmount;
        ASSERT(socket.scriptExecutionContext());

        socket.dispatchEvent(CloseEvent::create(wasClean, code, reason));

        if (socket.m_channel) {
            socket.m_channel->disconnect();
            socket.m_channel = nullptr;
        }
        socket.m_pendingActivity = nullptr;
    });
}

void WebSocket::didUpgradeURL()
{
    ASSERT(m_url.protocolIs("ws"_s));
    m_url.setProtocol("wss"_s);
}

size_t WebSocket::getFramingOverhead(size_t payloadSize)
{
    static const size_t hybiBaseFramingOverhead = 2; // Every frame has at least two-byte header.
    static const size_t hybiMaskingKeyLength = 4; // Every frame from client must have masking key.
    static const size_t minimumPayloadSizeWithTwoByteExtendedPayloadLength = 126;
    static const size_t minimumPayloadSizeWithEightByteExtendedPayloadLength = 0x10000;
    size_t overhead = hybiBaseFramingOverhead + hybiMaskingKeyLength;
    if (payloadSize >= minimumPayloadSizeWithEightByteExtendedPayloadLength)
        overhead += 8;
    else if (payloadSize >= minimumPayloadSizeWithTwoByteExtendedPayloadLength)
        overhead += 2;
    return overhead;
}

void WebSocket::dispatchErrorEventIfNeeded()
{
    if (m_dispatchedErrorEvent)
        return;

    m_dispatchedErrorEvent = true;
    dispatchEvent(Event::create(eventNames().errorEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

} // namespace WebCore
