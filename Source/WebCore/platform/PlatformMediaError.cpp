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
#include "PlatformMediaError.h"

#include <wtf/NeverDestroyed.h>

namespace WebCore {

String convertEnumerationToString(PlatformMediaError enumerationValue)
{
    static const NeverDestroyed<String> values[] = {
        MAKE_STATIC_STRING_IMPL("AppendError"),
        MAKE_STATIC_STRING_IMPL("ClientDisconnected"),
        MAKE_STATIC_STRING_IMPL("BufferRemoved"),
        MAKE_STATIC_STRING_IMPL("SourceRemoved"),
        MAKE_STATIC_STRING_IMPL("IPCError"),
        MAKE_STATIC_STRING_IMPL("ParsingError"),
        MAKE_STATIC_STRING_IMPL("MemoryError"),
        MAKE_STATIC_STRING_IMPL("Cancelled"),
        MAKE_STATIC_STRING_IMPL("LogicError"),
        MAKE_STATIC_STRING_IMPL("DecoderCreationError"),
        MAKE_STATIC_STRING_IMPL("NotSupportedError"),
    };
    static_assert(!static_cast<size_t>(PlatformMediaError::AppendError), "PlatformMediaError::AppendError is not 0 as expected");
    static_assert(static_cast<size_t>(PlatformMediaError::ClientDisconnected) == 1, "PlatformMediaError::ClientDisconnected is not 1 as expected");
    static_assert(static_cast<size_t>(PlatformMediaError::BufferRemoved) == 2, "PlatformMediaError::BufferRemoved is not 2 as expected");
    static_assert(static_cast<size_t>(PlatformMediaError::SourceRemoved) == 3, "PlatformMediaError::SourceRemoved is not 3 as expected");
    static_assert(static_cast<size_t>(PlatformMediaError::IPCError) == 4, "PlatformMediaError::IPCError is not 4 as expected");
    static_assert(static_cast<size_t>(PlatformMediaError::ParsingError) == 5, "PlatformMediaError::ParsingError is not 5 as expected");
    static_assert(static_cast<size_t>(PlatformMediaError::MemoryError) == 6, "PlatformMediaError::MemoryError is not 6 as expected");
    static_assert(static_cast<size_t>(PlatformMediaError::Cancelled) == 7, "PlatformMediaError::Cancelled is not 7 as expected");
    static_assert(static_cast<size_t>(PlatformMediaError::LogicError) == 8, "PlatformMediaError::LogicError is not 8 as expected");
    static_assert(static_cast<size_t>(PlatformMediaError::DecoderCreationError) == 9, "PlatformMediaError::DecoderCreationError is not 9 as expected");
    static_assert(static_cast<size_t>(PlatformMediaError::NotSupportedError) == 10, "PlatformMediaError::NotSupportedError is not 10 as expected");
    ASSERT(static_cast<size_t>(enumerationValue) < std::size(values));
    return values[static_cast<size_t>(enumerationValue)];
}

} // namespace WebCore
