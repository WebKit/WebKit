/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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
#include "DOMWindow.h"
#include "Document.h"
#include "Event.h"
#include "EventListener.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "Logging.h"
#include "MessageEvent.h"
#include "ResourceLoadObserver.h"
#include "ScriptController.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "SocketProvider.h"
#include "ThreadableWebSocketChannel.h"
#include "WebSocketChannel.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <JavaScriptCore/ArrayBufferView.h>
#include <JavaScriptCore/ScriptCallStack.h>
#include <wtf/HashSet.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

#if USE(WEB_THREAD)
#include "WebCoreThreadRun.h"
#endif

namespace WebCore {

const size_t maxReasonSizeInBytes = 123;

static inline bool isValidProtocolCharacter(UChar character)
{
    // Hybi-10 says "(Subprotocol string must consist of) characters in the range U+0021 to U+007E not including
    // separator characters as defined in [RFC2616]."
    const UChar minimumProtocolCharacter = '!'; // U+0021.
    const UChar maximumProtocolCharacter = '~'; // U+007E.
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
        if (protocol[i] < 0x20 || protocol[i] > 0x7E) {
            builder.appendLiteral("\\u");
            appendUnsignedAsHexFixedSize(protocol[i], builder, 4);
        } else if (protocol[i] == 0x5c)
            builder.appendLiteral("\\\\");
        else
            builder.append(protocol[i]);
    }
    return builder.toString();
}

static String joinStrings(const Vector<String>& strings, const char* separator)
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

const char* WebSocket::subprotocolSeparator()
{
    return ", ";
}

WebSocket::WebSocket(ScriptExecutionContext& context)
    : ActiveDOMObject(&context)
    , m_subprotocol(emptyString())
    , m_extensions(emptyString())
    , m_resumeTimer(*this, &WebSocket::resumeTimerFired)
{
    LockHolder lock(allActiveWebSocketsMutex());

    allActiveWebSockets(lock).add(this);
}

WebSocket::~WebSocket()
{
    {
        LockHolder lock(allActiveWebSocketsMutex());

        allActiveWebSockets(lock).remove(this);
    }

    if (m_channel)
        m_channel->disconnect();
}

ExceptionOr<Ref<WebSocket>> WebSocket::create(ScriptExecutionContext& context, const String& url)
{
    return create(context, url, Vector<String> { });
}

ExceptionOr<Ref<WebSocket>> WebSocket::create(ScriptExecutionContext& context, const String& url, const Vector<String>& protocols)
{
    if (url.isNull())
        return Exception { SyntaxError };

    auto socket = adoptRef(*new WebSocket(context));
    socket->suspendIfNeeded();

    auto result = socket->connect(context.completeURL(url), protocols);
    if (result.hasException())
        return result.releaseException();

    return WTFMove(socket);
}

ExceptionOr<Ref<WebSocket>> WebSocket::create(ScriptExecutionContext& context, const String& url, const String& protocol)
{
    return create(context, url, Vector<String> { 1, protocol });
}

HashSet<WebSocket*>& WebSocket::allActiveWebSockets(const LockHolder&)
{
    static NeverDestroyed<HashSet<WebSocket*>> activeWebSockets;
    return activeWebSockets;
}

Lock& WebSocket::allActiveWebSocketsMutex()
{
    static Lock mutex;
    return mutex;
}

ExceptionOr<void> WebSocket::connect(const String& url)
{
    return connect(url, Vector<String> { });
}

ExceptionOr<void> WebSocket::connect(const String& url, const String& protocol)
{
    return connect(url, Vector<String> { 1, protocol });
}

ExceptionOr<void> WebSocket::connect(const String& url, const Vector<String>& protocols)
{
    LOG(Network, "WebSocket %p connect() url='%s'", this, url.utf8().data());
    m_url = URL(URL(), url);

    ASSERT(scriptExecutionContext());
    auto& context = *scriptExecutionContext();

    if (!m_url.isValid()) {
        context.addConsoleMessage(MessageSource::JS, MessageLevel::Error, "Invalid url for WebSocket " + m_url.stringCenterEllipsizedToLength());
        m_state = CLOSED;
        return Exception { SyntaxError };
    }

    if (!m_url.protocolIs("ws") && !m_url.protocolIs("wss")) {
        context.addConsoleMessage(MessageSource::JS, MessageLevel::Error, "Wrong url scheme for WebSocket " + m_url.stringCenterEllipsizedToLength());
        m_state = CLOSED;
        return Exception { SyntaxError };
    }
    if (m_url.hasFragmentIdentifier()) {
        context.addConsoleMessage(MessageSource::JS, MessageLevel::Error, "URL has fragment component " + m_url.stringCenterEllipsizedToLength());
        m_state = CLOSED;
        return Exception { SyntaxError };
    }

    ASSERT(context.contentSecurityPolicy());
    auto& contentSecurityPolicy = *context.contentSecurityPolicy();

    contentSecurityPolicy.upgradeInsecureRequestIfNeeded(m_url, ContentSecurityPolicy::InsecureRequestType::Load);

    if (!portAllowed(m_url)) {
        String message;
        if (m_url.port())
            message = makeString("WebSocket port ", static_cast<unsigned>(m_url.port().value()), " blocked");
        else
            message = "WebSocket without port blocked"_s;
        context.addConsoleMessage(MessageSource::JS, MessageLevel::Error, message);
        m_state = CLOSED;
        return Exception { SecurityError };
    }

    // FIXME: Convert this to check the isolated world's Content Security Policy once webkit.org/b/104520 is solved.
    if (!context.shouldBypassMainWorldContentSecurityPolicy() && !contentSecurityPolicy.allowConnectToSource(m_url)) {
        m_state = CLOSED;

        // FIXME: Should this be throwing an exception?
        return Exception { SecurityError };
    }

    if (auto* provider = context.socketProvider())
        m_channel = ThreadableWebSocketChannel::create(*scriptExecutionContext(), *this, *provider);

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
            context.addConsoleMessage(MessageSource::JS, MessageLevel::Error, "Wrong protocol for WebSocket '" + encodeProtocolString(protocol) + "'");
            m_state = CLOSED;
            return Exception { SyntaxError };
        }
    }
    HashSet<String> visited;
    for (auto& protocol : protocols) {
        if (!visited.add(protocol).isNewEntry) {
            context.addConsoleMessage(MessageSource::JS, MessageLevel::Error, "WebSocket protocols contain duplicates: '" + encodeProtocolString(protocol) + "'");
            m_state = CLOSED;
            return Exception { SyntaxError };
        }
    }

    RunLoop::main().dispatch([targetURL = m_url.isolatedCopy(), mainFrameURL = context.url().isolatedCopy(), sessionID = context.sessionID()]() {
        ResourceLoadObserver::shared().logWebSocketLoading(targetURL, mainFrameURL, sessionID);
    });

    if (is<Document>(context)) {
        Document& document = downcast<Document>(context);
        RefPtr<Frame> frame = document.frame();
        if (!frame || !frame->loader().mixedContentChecker().canRunInsecureContent(document.securityOrigin(), m_url)) {
            m_pendingActivity = makePendingActivity(*this);

            // We must block this connection. Instead of throwing an exception, we indicate this
            // using the error event. But since this code executes as part of the WebSocket's
            // constructor, we have to wait until the constructor has completed before firing the
            // event; otherwise, users can't connect to the event.

            document.postTask([this, protectedThis = makeRef(*this)](auto&) {
                this->dispatchOrQueueErrorEvent();
                this->stop();
            });

            return { };
        }
    }

    String protocolString;
    if (!protocols.isEmpty())
        protocolString = joinStrings(protocols, subprotocolSeparator());

    m_channel->connect(m_url, protocolString);
    m_pendingActivity = makePendingActivity(*this);

    return { };
}

ExceptionOr<void> WebSocket::send(const String& message)
{
    LOG(Network, "WebSocket %p send() Sending String '%s'", this, message.utf8().data());
    if (m_state == CONNECTING)
        return Exception { InvalidStateError };
    // No exception is raised if the connection was once established but has subsequently been closed.
    if (m_state == CLOSING || m_state == CLOSED) {
        size_t payloadSize = message.utf8().length();
        m_bufferedAmountAfterClose = saturateAdd(m_bufferedAmountAfterClose, payloadSize);
        m_bufferedAmountAfterClose = saturateAdd(m_bufferedAmountAfterClose, getFramingOverhead(payloadSize));
        return { };
    }
    ASSERT(m_channel);
    m_channel->send(message);
    return { };
}

ExceptionOr<void> WebSocket::send(ArrayBuffer& binaryData)
{
    LOG(Network, "WebSocket %p send() Sending ArrayBuffer %p", this, &binaryData);
    if (m_state == CONNECTING)
        return Exception { InvalidStateError };
    if (m_state == CLOSING || m_state == CLOSED) {
        unsigned payloadSize = binaryData.byteLength();
        m_bufferedAmountAfterClose = saturateAdd(m_bufferedAmountAfterClose, payloadSize);
        m_bufferedAmountAfterClose = saturateAdd(m_bufferedAmountAfterClose, getFramingOverhead(payloadSize));
        return { };
    }
    ASSERT(m_channel);
    m_channel->send(binaryData, 0, binaryData.byteLength());
    return { };
}

ExceptionOr<void> WebSocket::send(ArrayBufferView& arrayBufferView)
{
    LOG(Network, "WebSocket %p send() Sending ArrayBufferView %p", this, &arrayBufferView);

    if (m_state == CONNECTING)
        return Exception { InvalidStateError };
    if (m_state == CLOSING || m_state == CLOSED) {
        unsigned payloadSize = arrayBufferView.byteLength();
        m_bufferedAmountAfterClose = saturateAdd(m_bufferedAmountAfterClose, payloadSize);
        m_bufferedAmountAfterClose = saturateAdd(m_bufferedAmountAfterClose, getFramingOverhead(payloadSize));
        return { };
    }
    ASSERT(m_channel);
    m_channel->send(*arrayBufferView.unsharedBuffer(), arrayBufferView.byteOffset(), arrayBufferView.byteLength());
    return { };
}

ExceptionOr<void> WebSocket::send(Blob& binaryData)
{
    LOG(Network, "WebSocket %p send() Sending Blob '%s'", this, binaryData.url().stringCenterEllipsizedToLength().utf8().data());
    if (m_state == CONNECTING)
        return Exception { InvalidStateError };
    if (m_state == CLOSING || m_state == CLOSED) {
        unsigned payloadSize = static_cast<unsigned>(binaryData.size());
        m_bufferedAmountAfterClose = saturateAdd(m_bufferedAmountAfterClose, payloadSize);
        m_bufferedAmountAfterClose = saturateAdd(m_bufferedAmountAfterClose, getFramingOverhead(payloadSize));
        return { };
    }
    ASSERT(m_channel);
    m_channel->send(binaryData);
    return { };
}

ExceptionOr<void> WebSocket::close(Optional<unsigned short> optionalCode, const String& reason)
{
    int code = optionalCode ? optionalCode.value() : static_cast<int>(WebSocketChannel::CloseEventCodeNotSpecified);
    if (code == WebSocketChannel::CloseEventCodeNotSpecified)
        LOG(Network, "WebSocket %p close() without code and reason", this);
    else {
        LOG(Network, "WebSocket %p close() code=%d reason='%s'", this, code, reason.utf8().data());
        if (!(code == WebSocketChannel::CloseEventCodeNormalClosure || (WebSocketChannel::CloseEventCodeMinimumUserDefined <= code && code <= WebSocketChannel::CloseEventCodeMaximumUserDefined)))
            return Exception { InvalidAccessError };
        CString utf8 = reason.utf8(StrictConversionReplacingUnpairedSurrogatesWithFFFD);
        if (utf8.length() > maxReasonSizeInBytes) {
            scriptExecutionContext()->addConsoleMessage(MessageSource::JS, MessageLevel::Error, "WebSocket close message is too long."_s);
            return Exception { SyntaxError };
        }
    }

    if (m_state == CLOSING || m_state == CLOSED)
        return { };
    if (m_state == CONNECTING) {
        m_state = CLOSING;
        m_channel->fail("WebSocket is closed before the connection is established.");
        return { };
    }
    m_state = CLOSING;
    if (m_channel)
        m_channel->close(code, reason);
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

String WebSocket::binaryType() const
{
    switch (m_binaryType) {
    case BinaryType::Blob:
        return "blob"_s;
    case BinaryType::ArrayBuffer:
        return "arraybuffer"_s;
    }
    ASSERT_NOT_REACHED();
    return String();
}

ExceptionOr<void> WebSocket::setBinaryType(const String& binaryType)
{
    if (binaryType == "blob") {
        m_binaryType = BinaryType::Blob;
        return { };
    }
    if (binaryType == "arraybuffer") {
        m_binaryType = BinaryType::ArrayBuffer;
        return { };
    }
    scriptExecutionContext()->addConsoleMessage(MessageSource::JS, MessageLevel::Error, "'" + binaryType + "' is not a valid value for binaryType; binaryType remains unchanged.");
    return Exception { SyntaxError };
}

EventTargetInterface WebSocket::eventTargetInterface() const
{
    return WebSocketEventTargetInterfaceType;
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

bool WebSocket::canSuspendForDocumentSuspension() const
{
    return true;
}

void WebSocket::suspend(ReasonForSuspension reason)
{
    if (m_resumeTimer.isActive())
        m_resumeTimer.stop();

    m_shouldDelayEventFiring = true;

    if (m_channel) {
        if (reason == ReasonForSuspension::PageCache) {
            // This will cause didClose() to be called.
            m_channel->fail("WebSocket is closed due to suspension.");
        } else
            m_channel->suspend();
    }
}

void WebSocket::resume()
{
    if (m_channel)
        m_channel->resume();
    else if (!m_pendingEvents.isEmpty() && !m_resumeTimer.isActive()) {
        // Fire the pending events in a timer as we are not allowed to execute arbitrary JS from resume().
        m_resumeTimer.startOneShot(0_s);
    }

    m_shouldDelayEventFiring = false;
}

void WebSocket::resumeTimerFired()
{
    Ref<WebSocket> protectedThis(*this);

    ASSERT(!m_pendingEvents.isEmpty());

    // Check m_shouldDelayEventFiring when iterating in case firing an event causes
    // suspend() to be called.
    while (!m_pendingEvents.isEmpty() && !m_shouldDelayEventFiring)
        dispatchEvent(m_pendingEvents.takeFirst());
}

void WebSocket::stop()
{
    if (m_channel)
        m_channel->disconnect();
    m_channel = nullptr;
    m_state = CLOSED;
    m_pendingEvents.clear();
    ActiveDOMObject::stop();
    m_pendingActivity = nullptr;
}

const char* WebSocket::activeDOMObjectName() const
{
    return "WebSocket";
}

void WebSocket::didConnect()
{
    LOG(Network, "WebSocket %p didConnect()", this);
    if (m_state != CONNECTING) {
        didClose(0, ClosingHandshakeIncomplete, WebSocketChannel::CloseEventCodeAbnormalClosure, emptyString());
        return;
    }
    ASSERT(scriptExecutionContext());
    m_state = OPEN;
    m_subprotocol = m_channel->subprotocol();
    m_extensions = m_channel->extensions();
    dispatchEvent(Event::create(eventNames().openEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

void WebSocket::didReceiveMessage(const String& msg)
{
    LOG(Network, "WebSocket %p didReceiveMessage() Text message '%s'", this, msg.utf8().data());
    if (m_state != OPEN)
        return;
    ASSERT(scriptExecutionContext());
    dispatchEvent(MessageEvent::create(msg, SecurityOrigin::create(m_url)->toString()));
}

void WebSocket::didReceiveBinaryData(Vector<uint8_t>&& binaryData)
{
    LOG(Network, "WebSocket %p didReceiveBinaryData() %u byte binary message", this, static_cast<unsigned>(binaryData.size()));
    switch (m_binaryType) {
    case BinaryType::Blob:
        // FIXME: We just received the data from NetworkProcess, and are sending it back. This is inefficient.
        dispatchEvent(MessageEvent::create(Blob::create(WTFMove(binaryData), emptyString()), SecurityOrigin::create(m_url)->toString()));
        break;
    case BinaryType::ArrayBuffer:
        dispatchEvent(MessageEvent::create(ArrayBuffer::create(binaryData.data(), binaryData.size()), SecurityOrigin::create(m_url)->toString()));
        break;
    }
}

void WebSocket::didReceiveMessageError()
{
    LOG(Network, "WebSocket %p didReceiveErrorMessage()", this);
    m_state = CLOSED;
    ASSERT(scriptExecutionContext());
    dispatchOrQueueErrorEvent();
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
    m_state = CLOSING;
}

void WebSocket::didClose(unsigned unhandledBufferedAmount, ClosingHandshakeCompletionStatus closingHandshakeCompletion, unsigned short code, const String& reason)
{
    LOG(Network, "WebSocket %p didClose()", this);
    if (!m_channel)
        return;
    bool wasClean = m_state == CLOSING && !unhandledBufferedAmount && closingHandshakeCompletion == ClosingHandshakeComplete && code != WebSocketChannel::CloseEventCodeAbnormalClosure;
    m_state = CLOSED;
    m_bufferedAmount = unhandledBufferedAmount;
    ASSERT(scriptExecutionContext());

    dispatchOrQueueEvent(CloseEvent::create(wasClean, code, reason));

    if (m_channel) {
        m_channel->disconnect();
        m_channel = nullptr;
    }
    m_pendingActivity = nullptr;
}

void WebSocket::didUpgradeURL()
{
    ASSERT(m_url.protocolIs("ws"));
    m_url.setProtocol("wss");
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

void WebSocket::dispatchOrQueueErrorEvent()
{
    if (m_dispatchedErrorEvent)
        return;

    m_dispatchedErrorEvent = true;
    dispatchOrQueueEvent(Event::create(eventNames().errorEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

void WebSocket::dispatchOrQueueEvent(Ref<Event>&& event)
{
    if (m_shouldDelayEventFiring)
        m_pendingEvents.append(WTFMove(event));
    else
        dispatchEvent(event);
}

} // namespace WebCore
