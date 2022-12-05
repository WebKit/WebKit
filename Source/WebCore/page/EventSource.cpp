/*
 * Copyright (C) 2009, 2012 Ericsson AB. All rights reserved.
 * Copyright (C) 2010, 2016 Apple Inc. All rights reserved.
 * Copyright (C) 2011, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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
#include "EventSource.h"

#include "CachedResourceRequestInitiatorTypes.h"
#include "ContentSecurityPolicy.h"
#include "EventNames.h"
#include "MessageEvent.h"
#include "ResourceError.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "SharedBuffer.h"
#include "TextResourceDecoder.h"
#include "ThreadableLoader.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/SetForScope.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(EventSource);

const uint64_t EventSource::defaultReconnectDelay = 3000;

inline EventSource::EventSource(ScriptExecutionContext& context, const URL& url, const Init& eventSourceInit)
    : ActiveDOMObject(&context)
    , m_url(url)
    , m_withCredentials(eventSourceInit.withCredentials)
    , m_decoder(TextResourceDecoder::create("text/plain"_s, "UTF-8"))
    , m_connectTimer(&context, *this, &EventSource::connect)
{
    m_connectTimer.suspendIfNeeded();
}

ExceptionOr<Ref<EventSource>> EventSource::create(ScriptExecutionContext& context, const String& url, const Init& eventSourceInit)
{
    URL fullURL = context.completeURL(url);
    if (!fullURL.isValid())
        return Exception { SyntaxError };

    // FIXME: Convert this to check the isolated world's Content Security Policy once webkit.org/b/104520 is resolved.
    if (!context.shouldBypassMainWorldContentSecurityPolicy() && !context.contentSecurityPolicy()->allowConnectToSource(fullURL)) {
        // FIXME: Should this be throwing an exception?
        return Exception { SecurityError };
    }

    auto source = adoptRef(*new EventSource(context, fullURL, eventSourceInit));
    source->scheduleInitialConnect();
    source->suspendIfNeeded();
    return source;
}

EventSource::~EventSource()
{
    ASSERT(m_state == CLOSED);
    ASSERT(!m_requestInFlight);
}

void EventSource::connect()
{
    ASSERT(m_state == CONNECTING);
    ASSERT(!m_requestInFlight);

    ResourceRequest request { m_url };
    request.setRequester(ResourceRequestRequester::EventSource);
    request.setHTTPMethod("GET"_s);
    request.setHTTPHeaderField(HTTPHeaderName::Accept, "text/event-stream"_s);
    request.setHTTPHeaderField(HTTPHeaderName::CacheControl, "no-cache"_s);
    if (!m_lastEventId.isEmpty())
        request.setHTTPHeaderField(HTTPHeaderName::LastEventID, m_lastEventId);

    ThreadableLoaderOptions options;
    options.sendLoadCallbacks = SendCallbackPolicy::SendCallbacks;
    options.credentials = m_withCredentials ? FetchOptions::Credentials::Include : FetchOptions::Credentials::SameOrigin;
    options.preflightPolicy = PreflightPolicy::Prevent;
    options.mode = FetchOptions::Mode::Cors;
    options.cache = FetchOptions::Cache::NoStore;
    options.dataBufferingPolicy = DataBufferingPolicy::DoNotBufferData;
    options.contentSecurityPolicyEnforcement = scriptExecutionContext()->shouldBypassMainWorldContentSecurityPolicy() ? ContentSecurityPolicyEnforcement::DoNotEnforce : ContentSecurityPolicyEnforcement::EnforceConnectSrcDirective;
    options.initiatorType = cachedResourceRequestInitiatorTypes().eventsource;

    ASSERT(scriptExecutionContext());
    m_loader = ThreadableLoader::create(*scriptExecutionContext(), *this, WTFMove(request), options);

    // FIXME: Can we just use m_loader for this, null it out when it's no longer in flight, and eliminate the m_requestInFlight member?
    if (m_loader)
        m_requestInFlight = true;
}

void EventSource::networkRequestEnded()
{
    ASSERT(m_requestInFlight);

    m_requestInFlight = false;

    if (m_state != CLOSED)
        scheduleReconnect();
}

void EventSource::scheduleInitialConnect()
{
    ASSERT(m_state == CONNECTING);
    ASSERT(!m_requestInFlight);

    m_connectTimer.startOneShot(0_s);
}

void EventSource::scheduleReconnect()
{
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!m_isSuspendedForBackForwardCache);
    m_state = CONNECTING;
    m_connectTimer.startOneShot(1_ms * m_reconnectDelay);
    dispatchErrorEvent();
}

void EventSource::close()
{
    if (m_state == CLOSED) {
        ASSERT(!m_requestInFlight);
        return;
    }

    // Stop trying to connect/reconnect if EventSource was explicitly closed or if ActiveDOMObject::stop() was called.
    if (m_connectTimer.isActive())
        m_connectTimer.cancel();

    if (m_requestInFlight)
        doExplicitLoadCancellation();
    else
        m_state = CLOSED;
}

bool EventSource::responseIsValid(const ResourceResponse& response) const
{
    // Logs to the console as a side effect.

    // To keep the signal-to-noise ratio low, we don't log anything if the status code is not 200.
    if (response.httpStatusCode() != 200)
        return false;

    if (!equalLettersIgnoringASCIICase(response.mimeType(), "text/event-stream"_s)) {
        auto message = makeString("EventSource's response has a MIME type (\"", response.mimeType(), "\") that is not \"text/event-stream\". Aborting the connection.");
        // FIXME: Console message would be better with a source code location; where would we get that?
        scriptExecutionContext()->addConsoleMessage(MessageSource::JS, MessageLevel::Error, WTFMove(message));
        return false;
    }

    // The specification states we should always decode as UTF-8. If there is a provided charset and it is not UTF-8, then log a warning
    // message but keep going anyway.
    auto& charset = response.textEncodingName();
    if (!charset.isEmpty() && !equalLettersIgnoringASCIICase(charset, "utf-8"_s)) {
        auto message = makeString("EventSource's response has a charset (\"", charset, "\") that is not UTF-8. The response will be decoded as UTF-8.");
        // FIXME: Console message would be better with a source code location; where would we get that?
        scriptExecutionContext()->addConsoleMessage(MessageSource::JS, MessageLevel::Error, WTFMove(message));
    }

    return true;
}

void EventSource::didReceiveResponse(ResourceLoaderIdentifier, const ResourceResponse& response)
{
    ASSERT(m_state == CONNECTING);
    ASSERT(m_requestInFlight);
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!m_isSuspendedForBackForwardCache);

    if (!responseIsValid(response)) {
        doExplicitLoadCancellation();
        dispatchErrorEvent();
        return;
    }

    m_eventStreamOrigin = SecurityOriginData::fromURL(response.url()).toString();
    m_state = OPEN;
    dispatchEvent(Event::create(eventNames().openEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

void EventSource::dispatchErrorEvent()
{
    dispatchEvent(Event::create(eventNames().errorEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

void EventSource::didReceiveData(const SharedBuffer& buffer)
{
    ASSERT(m_state == OPEN);
    ASSERT(m_requestInFlight);
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!m_isSuspendedForBackForwardCache);

    append(m_receiveBuffer, m_decoder->decode(buffer.data(), buffer.size()));
    parseEventStream();
}

void EventSource::didFinishLoading(ResourceLoaderIdentifier, const NetworkLoadMetrics&)
{
    ASSERT(m_state == OPEN);
    ASSERT(m_requestInFlight);
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!m_isSuspendedForBackForwardCache);

    append(m_receiveBuffer, m_decoder->flush());
    parseEventStream();

    // Discard everything that has not been dispatched by now.
    // FIXME: Why does this need to be done?
    // If this is important, why isn't it important to clear other data members: m_decoder, m_lastEventId, m_loader?
    m_receiveBuffer.clear();
    m_data.clear();
    m_eventName = { };
    m_currentlyParsedEventId = { };

    networkRequestEnded();
}

void EventSource::didFail(const ResourceError& error)
{
    ASSERT(m_state != CLOSED);

    if (error.isAccessControl()) {
        abortConnectionAttempt();
        return;
    }

    ASSERT(m_requestInFlight);

    // This is the case where the load gets cancelled on navigating away. We only fire an error event and attempt to reconnect
    // if we end up getting resumed from back/forward cache.
    if (error.isCancellation() && !m_isDoingExplicitCancellation) {
        m_shouldReconnectOnResume = true;
        m_requestInFlight = false;
        return;
    }

    if (error.isCancellation())
        m_state = CLOSED;

    // FIXME: Why don't we need to clear data members here as in didFinishLoading?

    networkRequestEnded();
}

void EventSource::abortConnectionAttempt()
{
    ASSERT(m_state == CONNECTING);
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!m_isSuspendedForBackForwardCache);

    auto jsWrapperProtector = makePendingActivity(*this);
    if (m_requestInFlight)
        doExplicitLoadCancellation();
    else
        m_state = CLOSED;

    ASSERT(m_state == CLOSED);
    dispatchEvent(Event::create(eventNames().errorEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

bool EventSource::virtualHasPendingActivity() const
{
    return m_state != CLOSED;
}

void EventSource::doExplicitLoadCancellation()
{
    ASSERT(m_requestInFlight);
    SetForScope explicitLoadCancellation(m_isDoingExplicitCancellation, true);
    m_loader->cancel();
}

void EventSource::parseEventStream()
{
    unsigned position = 0;
    unsigned size = m_receiveBuffer.size();
    while (position < size) {
        if (m_discardTrailingNewline) {
            if (m_receiveBuffer[position] == '\n')
                ++position;
            m_discardTrailingNewline = false;
        }

        std::optional<unsigned> lineLength;
        std::optional<unsigned> fieldLength;
        for (unsigned i = position; !lineLength && i < size; ++i) {
            switch (m_receiveBuffer[i]) {
            case ':':
                if (!fieldLength)
                    fieldLength = i - position;
                break;
            case '\r':
                m_discardTrailingNewline = true;
                FALLTHROUGH;
            case '\n':
                lineLength = i - position;
                break;
            }
        }

        if (!lineLength)
            break;

        parseEventStreamLine(position, fieldLength, lineLength.value());
        position += lineLength.value() + 1;

        // EventSource.close() might've been called by one of the message event handlers.
        // Per spec, no further messages should be fired after that.
        if (m_state == CLOSED)
            break;
    }

    // FIXME: The following operation makes it clear that m_receiveBuffer should be some other type,
    // perhaps a Deque or a circular buffer of some sort.
    if (position == size)
        m_receiveBuffer.clear();
    else if (position)
        m_receiveBuffer.remove(0, position);
}

void EventSource::parseEventStreamLine(unsigned position, std::optional<unsigned> fieldLength, unsigned lineLength)
{
    if (!lineLength) {
        if (!m_data.isEmpty())
            dispatchMessageEvent();
        m_eventName = { };
        return;
    }

    if (fieldLength && !fieldLength.value())
        return;

    StringView field { &m_receiveBuffer[position], fieldLength ? fieldLength.value() : lineLength };

    unsigned step;
    if (!fieldLength)
        step = lineLength;
    else if (m_receiveBuffer[position + fieldLength.value() + 1] != ' ')
        step = fieldLength.value() + 1;
    else
        step = fieldLength.value() + 2;
    position += step;
    unsigned valueLength = lineLength - step;

    if (field == "data"_s) {
        m_data.append(&m_receiveBuffer[position], valueLength);
        m_data.append('\n');
    } else if (field == "event"_s)
        m_eventName = { &m_receiveBuffer[position], valueLength };
    else if (field == "id"_s) {
        StringView parsedEventId = { &m_receiveBuffer[position], valueLength };
        constexpr UChar nullCharacter = '\0';
        if (!parsedEventId.contains(nullCharacter))
            m_currentlyParsedEventId = parsedEventId.toString();
    } else if (field == "retry"_s) {
        if (!valueLength)
            m_reconnectDelay = defaultReconnectDelay;
        else {
            if (auto reconnectDelay = parseInteger<uint64_t>({ &m_receiveBuffer[position], valueLength }))
                m_reconnectDelay = *reconnectDelay;
        }
    }
}

void EventSource::stop()
{
    close();
}

const char* EventSource::activeDOMObjectName() const
{
    return "EventSource";
}

void EventSource::suspend(ReasonForSuspension reason)
{
    if (reason != ReasonForSuspension::BackForwardCache)
        return;

    m_isSuspendedForBackForwardCache = true;
    RELEASE_ASSERT_WITH_MESSAGE(!m_requestInFlight, "Loads get cancelled before entering the BackForwardCache.");
}

void EventSource::resume()
{
    if (!m_isSuspendedForBackForwardCache)
        return;

    m_isSuspendedForBackForwardCache = false;
    if (std::exchange(m_shouldReconnectOnResume, false)) {
        scriptExecutionContext()->postTask([this, pendingActivity = makePendingActivity(*this)](ScriptExecutionContext&) {
            if (!isContextStopped())
                scheduleReconnect();
        });
    }
}

void EventSource::dispatchMessageEvent()
{
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!m_isSuspendedForBackForwardCache);

    if (!m_currentlyParsedEventId.isNull())
        m_lastEventId = WTFMove(m_currentlyParsedEventId);

    auto& name = m_eventName.isEmpty() ? eventNames().messageEvent : m_eventName;

    ASSERT(!m_data.isEmpty());

    // Omit the trailing "\n" character.
    String data(m_data.data(), m_data.size() - 1);
    m_data = { };

    dispatchEvent(MessageEvent::create(name, WTFMove(data), m_eventStreamOrigin, m_lastEventId));
}

} // namespace WebCore
