/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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
#include "HTTPParser.h"

namespace WebDriver {

HTTPParser::Phase HTTPParser::parse(Vector<uint8_t>&& data)
{
    if (!data.isEmpty()) {
        m_buffer.appendVector(WTFMove(data));

        while (true) {
            if (handlePhase() == Process::Suspend)
                break;
        }
    }

    return m_phase;
}

HTTPParser::Process HTTPParser::handlePhase()
{
    switch (m_phase) {
    case Phase::Idle: {
        String line;
        if (!readLine(line))
            return Process::Suspend;

        if (!parseFirstLine(WTFMove(line)))
            return abortProcess("Client error: invalid request line.");

        ASSERT(!m_message.method.isEmpty());
        ASSERT(!m_message.path.isEmpty());
        ASSERT(!m_message.version.isEmpty());
        m_phase = Phase::Header;

        return Process::Continue;
    }

    case Phase::Header: {
        String line;
        if (!readLine(line))
            return Process::Suspend;

        if (!line.isEmpty())
            m_message.requestHeaders.append(WTFMove(line));
        else {
            m_bodyLength = expectedBodyLength();
            m_phase = Phase::Body;
        }

        return Process::Continue;
    }

    case Phase::Body:
        if (m_buffer.size() > m_bodyLength)
            return abortProcess("Client error: don't send data after request and before response.");

        if (m_buffer.size() < m_bodyLength)
            return Process::Suspend;

        m_message.requestBody = WTFMove(m_buffer);
        m_phase = Phase::Complete;

        return Process::Suspend;

    case Phase::Complete:
        return abortProcess("Client error: don't send data after request and before response.");

    case Phase::Error:
        return abortProcess();
    }
}

HTTPParser::Process HTTPParser::abortProcess(const char* message)
{
    if (message)
        LOG_ERROR(message);

    m_phase = Phase::Error;

    if (!m_buffer.isEmpty())
        m_buffer.resize(0);

    return Process::Suspend;
}

bool HTTPParser::parseFirstLine(String&& line)
{
    auto components = line.split(' ');
    if (components.size() != 3)
        return false;

    m_message.method = WTFMove(components[0]);
    m_message.path = WTFMove(components[1]);
    m_message.version = WTFMove(components[2]);
    return true;
}

bool HTTPParser::readLine(String& line)
{
    auto length = m_buffer.size();
    auto position = m_buffer.find(0x0d);
    if (position == notFound || position + 1 == length || m_buffer[position + 1] != 0x0a)
        return false;

    line = String::fromUTF8(reinterpret_cast<char*>(m_buffer.data()), position);
    if (line.isNull())
        LOG_ERROR("Client error: invalid encoding in HTTP header.");

    m_buffer.remove(0, position + 2);
    return true;
}

size_t HTTPParser::expectedBodyLength() const
{
    if (m_message.method == "HEAD")
        return 0;

    const char* name = "content-length:";
    const size_t nameLength = std::strlen(name);

    for (const auto& header : m_message.requestHeaders) {
        if (header.startsWithIgnoringASCIICase(name)) {
            auto value = header.substringSharingImpl(nameLength).stripWhiteSpace();
            return value.toInt();
        }
    }

    return 0;
}

} // namespace WebDriver
