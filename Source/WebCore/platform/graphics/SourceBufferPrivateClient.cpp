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

#include "config.h"
#include "SourceBufferPrivateClient.h"
#include <wtf/NeverDestroyed.h>

#if ENABLE(MEDIA_SOURCE)

namespace WebCore {

String convertEnumerationToString(SourceBufferPrivateClient::ReceiveResult enumerationValue)
{
    static const NeverDestroyed<String> values[] = {
        MAKE_STATIC_STRING_IMPL("RecieveSucceeded"),
        MAKE_STATIC_STRING_IMPL("AppendError"),
        MAKE_STATIC_STRING_IMPL("ClientDisconnected"),
        MAKE_STATIC_STRING_IMPL("BufferRemoved"),
        MAKE_STATIC_STRING_IMPL("IPCError"),
    };
    static_assert(static_cast<size_t>(SourceBufferPrivateClient::ReceiveResult::RecieveSucceeded) == 0, "ReceiveResult::RecieveSucceeded is not 0 as expected");
    static_assert(static_cast<size_t>(SourceBufferPrivateClient::ReceiveResult::AppendError) == 1, "ReceiveResult::AppendError is not 1 as expected");
    static_assert(static_cast<size_t>(SourceBufferPrivateClient::ReceiveResult::ClientDisconnected) == 2, "ReceiveResult::ClientDisconnected is not 2 as expected");
    static_assert(static_cast<size_t>(SourceBufferPrivateClient::ReceiveResult::BufferRemoved) == 3, "ReceiveResult::BufferRemoved is not 3 as expected");
    static_assert(static_cast<size_t>(SourceBufferPrivateClient::ReceiveResult::IPCError) == 4, "ReceiveResult::IPCError is not 4 as expected");
    ASSERT(static_cast<size_t>(enumerationValue) < WTF_ARRAY_LENGTH(values));
    return values[static_cast<size_t>(enumerationValue)];
}

} // namespace WebCore

#endif // ENABLE(MEDIA_SOURCE)
