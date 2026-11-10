/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "WebSocketExtensionDispatcher.h"
#include "WebSocketExtensionParser.h"
#include <wtf/ASCIICType.h>
#include <wtf/HashMap.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

void WebSocketExtensionDispatcher::reset()
{
    m_processors.clear();
}

void WebSocketExtensionDispatcher::addProcessor(std::unique_ptr<WebSocketExtensionProcessor> processor)
{
    for (auto& extensionProcessor : m_processors) {
        if (equal(extensionProcessor->extensionToken(), processor->extensionToken()))
            return;
    }
    ASSERT(length(processor->handshakeString()));
    ASSERT(!contains(processor->handshakeString(), '\n'));
    ASSERT(!contains(processor->handshakeString(), static_cast<char16_t>('\0')));
    m_processors.append(WTF::move(processor));
}

const RefPtr<StringImpl> WebSocketExtensionDispatcher::createHeaderValue() const
{
    size_t numProcessors = m_processors.size();
    if (!numProcessors)
        return nullptr;

    return makeString(interleave(m_processors, [](auto& processor) { return processor->handshakeString(); }, ", "_s));
}

void WebSocketExtensionDispatcher::appendAcceptedExtension(StringImpl* extensionToken, HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>>& extensionParameters)
{
    m_acceptedExtensionsBuilder.append(m_acceptedExtensionsBuilder.isEmpty() ? ""_s : ", "_s, extensionToken);
    // FIXME: Should use ListHashSet to keep the order of the parameters.
    for (auto& parameter : extensionParameters) {
        m_acceptedExtensionsBuilder.append("; "_s, parameter.key);
        if (parameter.value)
            m_acceptedExtensionsBuilder.append('=', parameter.value);
    }
}

void WebSocketExtensionDispatcher::fail(StringImpl* reason)
{
    m_failureReason = reason;
    m_acceptedExtensionsBuilder.clear();
}

bool WebSocketExtensionDispatcher::processHeaderValue(StringImpl* headerValue)
{
    if (!length(headerValue))
        return true;

    // If we don't send Sec-WebSocket-Extensions header, the server should not return the header.
    if (!m_processors.size()) {
        fail("Received unexpected Sec-WebSocket-Extensions header"_si);
        return false;
    }

    const auto headerValueData = utf8(headerValue);
    // FIXME: Is UTF-8 the encoding that WebSocketExtensionParser expects? It doesn't specify.
    WebSocketExtensionParser parser(byteCast<uint8_t>(headerValueData.span()));
    while (!parser.finished()) {
        RefPtr<StringImpl> extensionToken;
        HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>> extensionParameters;
        if (!parser.parseExtension(extensionToken, extensionParameters)) {
            fail("Sec-WebSocket-Extensions header is invalid"_si);
            return false;
        }

        size_t index = 0;
        for (auto& processor : m_processors) {
            if (equal(extensionToken, processor->extensionToken())) {
                if (processor->processResponse(extensionParameters)) {
                    appendAcceptedExtension(extensionToken, extensionParameters);
                    break;
                }
                fail(processor->failureReason());
                return false;
            }
            ++index;
        }
        // There is no extension which can process the response.
        if (index == m_processors.size()) {
            fail(makeString("Received unexpected extension: "_s, extensionToken));
            return false;
        }
    }
    return parser.parsedSuccessfully();
}

RefPtr<StringImpl> WebSocketExtensionDispatcher::acceptedExtensions() const
{
    if (m_acceptedExtensionsBuilder.isEmpty())
        return nullptr;
    return m_acceptedExtensionsBuilder.toStringPreserveCapacity();
}

RefPtr<StringImpl> WebSocketExtensionDispatcher::failureReason() const
{
    return m_failureReason;
}

} // namespace WebCore
