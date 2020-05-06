/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "LegacySessionStateCoding.h"

#include "APIData.h"
#include "DataReference.h"
#include "Decoder.h"
#include "Encoder.h"
#include "MessageNames.h"
#include "SessionState.h"
#include "WebCoreArgumentCoders.h"

namespace WebKit {

RefPtr<API::Data> encodeLegacySessionState(const SessionState& sessionState)
{
    // FIXME: This should use WTF::Persistence::Encoder instead.
    IPC::Encoder encoder(IPC::MessageName::LegacySessionState, 0);
    encoder << sessionState.backForwardListState;
    encoder << sessionState.renderTreeSize;
    encoder << sessionState.provisionalURL;
    return API::Data::create(encoder.buffer(), encoder.bufferSize());
}

bool decodeLegacySessionState(const uint8_t* data, size_t dataSize, SessionState& sessionState)
{
    IPC::Decoder decoder(data, dataSize, nullptr, Vector<IPC::Attachment>());

    Optional<BackForwardListState> backForwardListState;
    decoder >> backForwardListState;
    if (!backForwardListState)
        return false;
    sessionState.backForwardListState = WTFMove(*backForwardListState);

    Optional<uint64_t> renderTreeSize;
    decoder >> renderTreeSize;
    if (!renderTreeSize)
        return false;
    sessionState.renderTreeSize = *renderTreeSize;

    Optional<URL> provisionalURL;
    decoder >> provisionalURL;
    if (!provisionalURL)
        return false;
    sessionState.provisionalURL = WTFMove(*provisionalURL);

    return true;
}

} // namespace WebKit
