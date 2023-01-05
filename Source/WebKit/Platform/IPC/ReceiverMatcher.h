/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include <optional>
#include <wtf/Int128.h>

namespace IPC {

struct ReceiverMatcher {
    // Matches all messages.
    ReceiverMatcher() = default;

    // Matches message to specific receiver, any destination ID.
    ReceiverMatcher(ReceiverName receiverName)
        : receiverName(receiverName)
    {
    }

    // Matches message to specific receiver, specific destination ID.
    // Note: destinationID == 0 matches only 0 ids.
    ReceiverMatcher(ReceiverName receiverName, UInt128 destinationID)
        : receiverName(receiverName)
        , destinationID(destinationID)
    {
    }

    // Creates a matcher from parameters where destinationID == 0 means any destintation ID. Deprecated.
    static ReceiverMatcher createWithZeroAsAnyDestination(ReceiverName receiverName, UInt128 destinationID)
    {
        if (destinationID)
            return ReceiverMatcher { receiverName, destinationID };
        return ReceiverMatcher { receiverName };
    }

    bool matches(ReceiverName matchReceiverName, UInt128 matchDestinationID) const
    {
        return !receiverName || (*receiverName == matchReceiverName && (!destinationID || *destinationID == matchDestinationID));
    }

    std::optional<ReceiverName> receiverName;
    std::optional<UInt128> destinationID;
};

}
