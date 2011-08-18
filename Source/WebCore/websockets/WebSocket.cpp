/*
 * Copyright (C) 2009 Google Inc.  All rights reserved.
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

#if ENABLE(WEB_SOCKETS)

#include "WebSocket.h"

#include "CloseEvent.h"
#include "DOMWindow.h"
#include "Event.h"
#include "EventException.h"
#include "EventListener.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "Logging.h"
#include "MessageEvent.h"
#include "ScriptCallStack.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "ThreadableWebSocketChannel.h"
#include "WebSocketChannel.h"
#include <wtf/HashSet.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

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

static bool isValidProtocolString(const String& protocol)
{
    if (protocol.isEmpty())
        return false;
    for (size_t i = 0; i < protocol.length(); ++i) {
        if (!isValidProtocolCharacter(protocol[i]))
            return false;
    }
    return true;
}

static bool isValidProtocolStringHixie76(const String& protocol)
{
    if (protocol.isNull())
        return true;
    if (protocol.isEmpty())
        return false;
    const UChar* characters = protocol.characters();
    for (size_t i = 0; i < protocol.length(); i++) {
        if (characters[i] < 0x20 || characters[i] > 0x7E)
            return false;
    }
    return true;
}

static String encodeProtocolString(const String& protocol)
{
    StringBuilder builder;
    for (size_t i = 0; i < protocol.length(); i++) {
        if (protocol[i] < 0x20 || protocol[i] > 0x7E)
            builder.append(String::format("\\u%04X", protocol[i]));
        else if (protocol[i] == 0x5c)
            builder.append("\\\\");
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

static bool webSocketsAvailable = false;

void WebSocket::setIsAvailable(bool available)
{
    webSocketsAvailable = available;
}

bool WebSocket::isAvailable()
{
    return webSocketsAvailable;
}

WebSocket::WebSocket(ScriptExecutionContext* context)
    : ActiveDOMObject(context, this)
    , m_state(CONNECTING)
    , m_bufferedAmountAfterClose(0)
    , m_binaryType(BinaryTypeBlob)
    , m_useHixie76Protocol(true)
    , m_subprotocol("")
{
}

WebSocket::~WebSocket()
{
    if (m_channel)
        m_channel->disconnect();
}

void WebSocket::connect(const String& url, ExceptionCode& ec)
{
    Vector<String> protocols;
    connect(url, protocols, ec);
}

void WebSocket::connect(const String& url, const String& protocol, ExceptionCode& ec)
{
    Vector<String> protocols;
    protocols.append(protocol);
    connect(url, protocols, ec);
}

void WebSocket::connect(const String& url, const Vector<String>& protocols, ExceptionCode& ec)
{
    LOG(Network, "WebSocket %p connect to %s", this, url.utf8().data());
    m_url = KURL(KURL(), url);

    if (!m_url.isValid()) {
        scriptExecutionContext()->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "Invalid url for WebSocket " + m_url.string(), 0, scriptExecutionContext()->securityOrigin()->toString(), 0);
        m_state = CLOSED;
        ec = SYNTAX_ERR;
        return;
    }

    if (!m_url.protocolIs("ws") && !m_url.protocolIs("wss")) {
        scriptExecutionContext()->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "Wrong url scheme for WebSocket " + m_url.string(), 0, scriptExecutionContext()->securityOrigin()->toString(), 0);
        m_state = CLOSED;
        ec = SYNTAX_ERR;
        return;
    }
    if (m_url.hasFragmentIdentifier()) {
        scriptExecutionContext()->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "URL has fragment component " + m_url.string(), 0, scriptExecutionContext()->securityOrigin()->toString(), 0);
        m_state = CLOSED;
        ec = SYNTAX_ERR;
        return;
    }
    if (!portAllowed(m_url)) {
        scriptExecutionContext()->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "WebSocket port " + String::number(m_url.port()) + " blocked", 0, scriptExecutionContext()->securityOrigin()->toString(), 0);
        m_state = CLOSED;
        ec = SECURITY_ERR;
        return;
    }

    m_channel = ThreadableWebSocketChannel::create(scriptExecutionContext(), this);
    m_useHixie76Protocol = m_channel->useHixie76Protocol();

    String protocolString;
    if (m_useHixie76Protocol) {
        if (!protocols.isEmpty()) {
            // Emulate JavaScript's Array.toString() behavior.
            protocolString = joinStrings(protocols, ",");
        }
        if (!isValidProtocolStringHixie76(protocolString)) {
            scriptExecutionContext()->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "Wrong protocol for WebSocket '" + encodeProtocolString(protocolString) + "'", 0, scriptExecutionContext()->securityOrigin()->toString(), 0);
            m_state = CLOSED;
            ec = SYNTAX_ERR;
            return;
        }
    } else {
        // FIXME: There is a disagreement about restriction of subprotocols between WebSocket API and hybi-10 protocol
        // draft. The former simply says "only characters in the range U+0021 to U+007E are allowed," while the latter
        // imposes a stricter rule: "the elements MUST be non-empty strings with characters as defined in [RFC2616],
        // and MUST all be unique strings."
        //
        // Here, we throw SYNTAX_ERR if the given protocols do not meet the latter criteria. This behavior does not
        // comply with WebSocket API specification, but it seems to be the only reasonable way to handle this conflict.
        for (size_t i = 0; i < protocols.size(); ++i) {
            if (!isValidProtocolString(protocols[i])) {
                scriptExecutionContext()->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "Wrong protocol for WebSocket '" + encodeProtocolString(protocols[i]) + "'", 0, scriptExecutionContext()->securityOrigin()->toString(), 0);
                m_state = CLOSED;
                ec = SYNTAX_ERR;
                return;
            }
        }
        HashSet<String> visited;
        for (size_t i = 0; i < protocols.size(); ++i) {
            if (visited.contains(protocols[i])) {
                scriptExecutionContext()->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "WebSocket protocols contain duplicates: '" + encodeProtocolString(protocols[i]) + "'", 0, scriptExecutionContext()->securityOrigin()->toString(), 0);
                m_state = CLOSED;
                ec = SYNTAX_ERR;
                return;
            }
            visited.add(protocols[i]);
        }

        if (!protocols.isEmpty())
            protocolString = joinStrings(protocols, ", ");
    }

    m_channel->connect(m_url, protocolString);
    ActiveDOMObject::setPendingActivity(this);
}

bool WebSocket::send(const String& message, ExceptionCode& ec)
{
    LOG(Network, "WebSocket %p send %s", this, message.utf8().data());
    if (m_state == CONNECTING) {
        ec = INVALID_STATE_ERR;
        return false;
    }
    // No exception is raised if the connection was once established but has subsequently been closed.
    if (m_state == CLOSING || m_state == CLOSED) {
        m_bufferedAmountAfterClose += message.utf8().length() + 2; // 2 for frameing
        return false;
    }
    // FIXME: check message is valid utf8.
    ASSERT(m_channel);
    return m_channel->send(message);
}

void WebSocket::close()
{
    LOG(Network, "WebSocket %p close", this);
    if (m_state == CLOSING || m_state == CLOSED)
        return;
    if (m_state == CONNECTING) {
        m_state = CLOSING;
        m_channel->fail("WebSocket is closed before the connection is established.");
        return;
    }
    m_state = CLOSING;
    m_bufferedAmountAfterClose = m_channel->bufferedAmount();
    // didClose notification may be already queued, which we will inadvertently process while waiting for bufferedAmount() to return.
    // In this case m_channel will be set to null during didClose() call, thus we need to test validness of m_channel here.
    if (m_channel)
        m_channel->close();
}

const KURL& WebSocket::url() const
{
    return m_url;
}

WebSocket::State WebSocket::readyState() const
{
    return m_state;
}

unsigned long WebSocket::bufferedAmount() const
{
    if (m_state == OPEN)
        return m_channel->bufferedAmount();
    else if (m_state == CLOSING)
        return m_channel->bufferedAmount() + m_bufferedAmountAfterClose;
    return m_bufferedAmountAfterClose;
}

String WebSocket::protocol() const
{
    if (m_useHixie76Protocol)
        return String();
    return m_subprotocol;
}

String WebSocket::binaryType() const
{
    if (m_useHixie76Protocol)
        return String();
    switch (m_binaryType) {
    case BinaryTypeBlob:
        return "blob";
    case BinaryTypeArrayBuffer:
        return "arraybuffer";
    }
    ASSERT_NOT_REACHED();
    return String();
}

void WebSocket::setBinaryType(const String& binaryType, ExceptionCode& ec)
{
    if (m_useHixie76Protocol)
        return;
    if (binaryType == "blob") {
        m_binaryType = BinaryTypeBlob;
        return;
    }
    if (binaryType == "arraybuffer") {
        m_binaryType = BinaryTypeArrayBuffer;
        return;
    }
    ec = SYNTAX_ERR;
    return;
}

ScriptExecutionContext* WebSocket::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

void WebSocket::contextDestroyed()
{
    LOG(Network, "WebSocket %p scriptExecutionContext destroyed", this);
    ASSERT(!m_channel);
    ASSERT(m_state == CLOSED);
    ActiveDOMObject::contextDestroyed();
}

bool WebSocket::canSuspend() const
{
    return !m_channel;
}

void WebSocket::suspend(ReasonForSuspension)
{
    if (m_channel)
        m_channel->suspend();
}

void WebSocket::resume()
{
    if (m_channel)
        m_channel->resume();
}

void WebSocket::stop()
{
    bool pending = hasPendingActivity();
    if (m_channel)
        m_channel->disconnect();
    m_channel = 0;
    m_state = CLOSED;
    ActiveDOMObject::stop();
    if (pending)
        ActiveDOMObject::unsetPendingActivity(this);
}

void WebSocket::didConnect()
{
    LOG(Network, "WebSocket %p didConnect", this);
    if (m_state != CONNECTING) {
        didClose(0, ClosingHandshakeIncomplete);
        return;
    }
    ASSERT(scriptExecutionContext());
    m_state = OPEN;
    m_subprotocol = m_channel->subprotocol();
    dispatchEvent(Event::create(eventNames().openEvent, false, false));
}

void WebSocket::didReceiveMessage(const String& msg)
{
    LOG(Network, "WebSocket %p didReceiveMessage %s", this, msg.utf8().data());
    if (m_state != OPEN && m_state != CLOSING)
        return;
    ASSERT(scriptExecutionContext());
    RefPtr<MessageEvent> evt = MessageEvent::create();
    evt->initMessageEvent(eventNames().messageEvent, false, false, SerializedScriptValue::create(msg), "", "", 0, 0);
    dispatchEvent(evt);
}

void WebSocket::didReceiveMessageError()
{
    LOG(Network, "WebSocket %p didReceiveErrorMessage", this);
    if (m_state != OPEN && m_state != CLOSING)
        return;
    ASSERT(scriptExecutionContext());
    dispatchEvent(Event::create(eventNames().errorEvent, false, false));
}

void WebSocket::didStartClosingHandshake()
{
    LOG(Network, "WebSocket %p didStartClosingHandshake", this);
    m_state = CLOSING;
}

void WebSocket::didClose(unsigned long unhandledBufferedAmount, ClosingHandshakeCompletionStatus closingHandshakeCompletion)
{
    LOG(Network, "WebSocket %p didClose", this);
    if (!m_channel)
        return;
    bool wasClean = m_state == CLOSING && !unhandledBufferedAmount && closingHandshakeCompletion == ClosingHandshakeComplete;
    m_state = CLOSED;
    m_bufferedAmountAfterClose += unhandledBufferedAmount;
    ASSERT(scriptExecutionContext());
    RefPtr<CloseEvent> event = CloseEvent::create();
    event->initCloseEvent(eventNames().closeEvent, false, false, wasClean);
    dispatchEvent(event);
    if (m_channel) {
        m_channel->disconnect();
        m_channel = 0;
    }
    if (hasPendingActivity())
        ActiveDOMObject::unsetPendingActivity(this);
}

EventTargetData* WebSocket::eventTargetData()
{
    return &m_eventTargetData;
}

EventTargetData* WebSocket::ensureEventTargetData()
{
    return &m_eventTargetData;
}

}  // namespace WebCore

#endif
