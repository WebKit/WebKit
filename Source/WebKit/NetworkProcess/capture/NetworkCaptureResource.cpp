/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "NetworkCaptureResource.h"

#if ENABLE(NETWORK_CAPTURE)

#include "NetworkCaptureEvent.h"
#include "NetworkCaptureLogging.h"
#include "NetworkCaptureManager.h"
#include "NetworkCaptureRecorder.h"

namespace WebKit {
namespace NetworkCapture {

Resource::Resource(const String& eventFilePath)
    : m_eventFilePath(eventFilePath)
{
}

const URL& Resource::url()
{
    if (!m_url.isValid()) {
        auto events = eventStream();
        auto event = events.nextEvent();
        if (!event)
            DEBUG_LOG_ERROR("Event stream does not contain events: file = " STRING_SPECIFIER, DEBUG_STR(m_eventFilePath));
        else if (!WTF::holds_alternative<RequestSentEvent>(*event))
            DEBUG_LOG_ERROR("Event stream does not have a requestSent event: file = " STRING_SPECIFIER, DEBUG_STR(m_eventFilePath));
        else {
            auto requestSentEvent = WTF::get<RequestSentEvent>(*event);
            m_url = URL({ }, requestSentEvent.request.url);
        }
    }

    return m_url;
}

const String& Resource::urlIdentifyingCommonDomain()
{
    if (m_urlIdentifyingCommonDomain.isNull())
        m_urlIdentifyingCommonDomain = Manager::urlIdentifyingCommonDomain(url());

    return m_urlIdentifyingCommonDomain;
}

WTF::URLParser::URLEncodedForm Resource::queryParameters()
{
    if (!m_queryParameters)
        m_queryParameters = WTF::URLParser::parseURLEncodedForm(url().query());

    return *m_queryParameters;
}

Resource::EventStream Resource::eventStream()
{
    return EventStream(m_eventFilePath);
}

Resource::EventStream::EventStream(const String& eventFilePath)
    : m_eventFilePath(eventFilePath)
    , m_mappedEventFile(m_eventFilePath, m_haveMappedEventFile)
{
}

OptionalCaptureEvent Resource::EventStream::nextEvent()
{
    if (m_offset == m_mappedEventFile.size()) {
        DEBUG_LOG_ERROR("Unable to return event - at end of file: " STRING_SPECIFIER, DEBUG_STR(m_eventFilePath));
        return std::nullopt;
    }

    const char* charBuffer = static_cast<const char*>(m_mappedEventFile.data());
    const char* current = charBuffer + m_offset;

    while (m_offset < m_mappedEventFile.size() && charBuffer[m_offset])
        ++m_offset;

    if (m_offset == m_mappedEventFile.size()) {
        DEBUG_LOG_ERROR("Unable to return event - no terminating NUL: " STRING_SPECIFIER, DEBUG_STR(m_eventFilePath));
        return std::nullopt;
    }

    ++m_offset;

    return stringToEvent(current);
}

} // namespace NetworkCapture
} // namespace WebKit

#endif // ENABLE(NETWORK_CAPTURE)
