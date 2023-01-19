/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "StreamConnectionEncoder.h"
#include "Test.h"
#include <memory>

namespace TestWebKitAPI {

TEST(StreamConnectionEncoderTest, MinimumMessageSizeCanEncodeExpectedMessageAtAnyOffset)
{
    uint8_t storage[200];
    // IPC::Decoder, IPC::StreamConnectionEncoder expect the buffers to be aligned in the way that
    // an aligned write is readable by an aligned read.
    // In other words: (encoder buffer % max used align) == (decoder buffer % max used align).
    constexpr size_t maxAlignment = alignof(std::max_align_t);
    // Currently the implementation selects (buffer % max used align) == 0 as the starting
    // point.
    uint8_t* buffer = roundUpToMultipleOf<maxAlignment>(storage);
    EXPECT_GT(sizeof(storage), 2 * maxAlignment + IPC::StreamConnectionEncoder::minimumMessageSize);

    // Currently stream connection expects to be able to encode at least the SetStreamDestinationID message.
    static_assert(2u == IPC::StreamConnectionEncoder::messageAlignment);
    static_assert(10u == sizeof(IPC::MessageName) + sizeof(uint64_t)); // SetStreamDestinationID message structure.
    static_assert(16u == IPC::StreamConnectionEncoder::minimumMessageSize); // SetStreamDestinationID fits here at any offset.

    size_t minimumMessageSize = IPC::StreamConnectionEncoder::minimumMessageSize;
    for (size_t i = 0; i < maxAlignment; i += IPC::StreamConnectionEncoder::messageAlignment) {
        IPC::StreamConnectionEncoder encoder { IPC::MessageName::SetStreamDestinationID, buffer + i,  minimumMessageSize };
        encoder << static_cast<int64_t>(0);
        EXPECT_TRUE(encoder.isValid()) << i;
    }
}

}
