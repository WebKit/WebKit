/*
 * Copyright (C) 2019 Sony Interactive Entertainment Inc.
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
#include "RemoteInspectorMessageParser.h"

#if ENABLE(REMOTE_INSPECTOR)

namespace Inspector {

/*
| <--- one message for send / didReceiveData ---> |
+--------------+----------------------------------+--------------
|    size      |               data               | (next message)
|    4byte     |          variable length         |
+--------------+----------------------------------+--------------
               | <------------ size ------------> |
*/

MessageParser::MessageParser(ClientID clientID, size_t bufferSize)
    : m_clientID(clientID)
{
    m_buffer.reserveCapacity(bufferSize);
}

Vector<uint8_t> MessageParser::createMessage(const uint8_t* data, size_t size)
{
    if (!data || !size || size > UINT_MAX)
        return Vector<uint8_t>();

    auto messageBuffer = Vector<uint8_t>(size + sizeof(uint32_t));
    uint32_t uintSize = static_cast<uint32_t>(size);
    memcpy(&messageBuffer[0], &uintSize, sizeof(uint32_t));
    memcpy(&messageBuffer[sizeof(uint32_t)], data, uintSize);
    return messageBuffer;
}

void MessageParser::pushReceivedData(const uint8_t* data, size_t size)
{
    if (!data || !size)
        return;

    m_buffer.reserveCapacity(m_buffer.size() + size);
    m_buffer.append(data, size);

    if (!parse())
        clearReceivedData();
}

void MessageParser::clearReceivedData()
{
    m_buffer.clear();
}

bool MessageParser::parse()
{
    while (!m_buffer.isEmpty()) {
        if (m_buffer.size() < sizeof(uint32_t)) {
            // Wait for more data.
            return true;
        }

        uint32_t dataSize = 0;
        memcpy(&dataSize, &m_buffer[0], sizeof(uint32_t));
        if (!dataSize) {
            LOG_ERROR("Message Parser received an invalid message size");
            return false;
        }

        size_t messageSize = (sizeof(uint32_t) + dataSize);
        if (m_buffer.size() < messageSize) {
            // Wait for more data.
            return true;
        }

        // FIXME: This should avoid re-creating a new data Vector.
        auto dataBuffer = Vector<uint8_t>(dataSize);
        memcpy(&dataBuffer[0], &m_buffer[sizeof(uint32_t)], dataSize);

        if (m_didParseMessageListener)
            m_didParseMessageListener(m_clientID, WTFMove(dataBuffer));

        m_buffer.remove(0, messageSize);
    }

    return true;
}

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)
