/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#import "config.h"
#import "NetworkTransportReceiveStream.h"

#import "NetworkTransportSession.h"

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WebKit {

NetworkTransportReceiveStream::NetworkTransportReceiveStream(NetworkTransportSession& session, nw_connection_t connection)
    : m_identifier(WebTransportStreamIdentifier::generate())
    , m_session(session)
    , m_connection(connection)
{
    ASSERT(m_connection);
    receiveLoop();
}

void NetworkTransportReceiveStream::receiveLoop()
{
    nw_connection_receive(m_connection.get(), 1, std::numeric_limits<uint32_t>::max(), makeBlockPtr([weakThis = WeakPtr { *this }] (dispatch_data_t content, nw_content_context_t, bool withFin, nw_error_t error) {

        // FIXME: Not only is this an unnecessary string copy, but it's also something that should probably be in WTF or FragmentedSharedBuffer.
        auto vectorFromData = [](dispatch_data_t content) {
            ASSERT(content);
            __block Vector<uint8_t> request;
            dispatch_data_apply(content, ^bool(dispatch_data_t, size_t, const void* buffer, size_t size) {
                request.append(std::span { static_cast<const uint8_t*>(buffer), size });
                return true;
            });
            return request;
        };

        RefPtr strongThis = weakThis.get();
        if (!strongThis)
            return;
        if (error)
            return; // FIXME: Pipe this error to JS.
        if (strongThis->m_session)
            strongThis->m_session->streamReceiveBytes(strongThis->m_identifier, vectorFromData(content).span(), withFin);
        strongThis->receiveLoop();
    }).get());
}
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
