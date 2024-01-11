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

#pragma once

#include <wtf/Forward.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

enum class PlatformMediaError : uint8_t {
    AppendError,
    ClientDisconnected,
    BufferRemoved,
    SourceRemoved,
    IPCError,
    ParsingError,
    MemoryError,
    Cancelled,
    LogicError,
    DecoderCreationError,
};

using MediaPromise = NativePromise<void, PlatformMediaError>;

String convertEnumerationToString(PlatformMediaError);

} // namespace WebCore

namespace WTF {

template<typename Type>
struct LogArgument;

template <>
struct LogArgument<WebCore::PlatformMediaError> {
    static String toString(const WebCore::PlatformMediaError error)
    {
        return convertEnumerationToString(error);
    }
};

template<> struct EnumTraits<WebCore::PlatformMediaError> {
    using values = EnumValues<
        WebCore::PlatformMediaError,
        WebCore::PlatformMediaError::AppendError,
        WebCore::PlatformMediaError::ClientDisconnected,
        WebCore::PlatformMediaError::BufferRemoved,
        WebCore::PlatformMediaError::SourceRemoved,
        WebCore::PlatformMediaError::IPCError,
        WebCore::PlatformMediaError::ParsingError,
        WebCore::PlatformMediaError::MemoryError,
        WebCore::PlatformMediaError::Cancelled,
        WebCore::PlatformMediaError::LogicError
    >;
};

} // namespace WTF
