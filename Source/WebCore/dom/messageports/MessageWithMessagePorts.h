/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#pragma once

#include "MessagePortIdentifier.h"
#include "SerializedScriptValue.h"
#include <wtf/RefPtr.h>

namespace WebCore {

// When a message port is transferred, it is represented by a pair of identifiers.
// The first identifier is the port being transferred and the second is its remote port.
typedef Vector<std::pair<WebCore::MessagePortIdentifier, WebCore::MessagePortIdentifier>> TransferredMessagePortArray;

struct MessageWithMessagePorts {
    RefPtr<SerializedScriptValue> message;
    TransferredMessagePortArray transferredPorts;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<MessageWithMessagePorts> decode(Decoder&);
};


template<class Encoder>
void MessageWithMessagePorts::encode(Encoder& encoder) const
{
    ASSERT(message);
    encoder << *message << transferredPorts;
}

template<class Decoder>
Optional<MessageWithMessagePorts> MessageWithMessagePorts::decode(Decoder& decoder)
{
    MessageWithMessagePorts result;

    result.message = SerializedScriptValue::decode(decoder);
    if (!result.message)
        return WTF::nullopt;

    if (!decoder.decode(result.transferredPorts))
        return WTF::nullopt;

    return result;
}

} // namespace WebCore
