/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <WebCore/SharedMemory.h>
#include <limits>
#include <wtf/text/WTFString.h>

namespace IPC {

// String type for efficient holding of read-only strings that are transfered multiple times,
// possibly to different processes.
// Originators are able to optionally create from custom memory mappings.
// A small string is held without copy and transferred inline.
// A large string is held as single shared memory object.
// Receive side, release()d Strings refer to the possible shared memory object.
class TransferString {
public:
    struct SharedSpan8 {
        WebCore::SharedMemoryHandle dataHandle;
    };
    struct SharedSpan16 {
        WebCore::SharedMemoryHandle dataHandle;
    };
    using IPCData = Variant<std::monostate, std::span<const Latin1Character>, std::span<const char16_t>, SharedSpan8, SharedSpan16>;

    static std::optional<TransferString> create(const String&);
    static std::optional<TransferString> create(StringView);
#if USE(CF)
    static std::optional<TransferString> create(CFStringRef);
#endif
#if USE(FOUNDATION) && defined(__OBJC__)
    static std::optional<TransferString> create(NSString *);
    static std::optional<TransferString> createCached(NSString *);
#endif

    TransferString() = default;
    // Constructor for custom memory mapping of Latin1 (String::span8()) string.
    TransferString(SharedSpan8&&);
    // Constructor for custom memory mapping of char16_t (String::span16()) string.
    TransferString(SharedSpan16&&);

    TransferString(IPCData&&);
    TransferString(TransferString&&) = default;
    TransferString& operator=(TransferString&&) = default;

    static constexpr size_t transferAsMappingSize = 16384;

    // Release the string.
    // Pass maxCopySizeInBytes = transferAsMappingSize - 1 to release without copy, possibly holding the underlying virtual memory mapping.
    // Pass maxCopySizeInBytes = std::numeric_limits<size_t>::max() to release with copy always, avoiding potential virtual memory fragmentation.
    // Fails on out-of-memory (if mapping fails).
    std::optional<String> release(size_t maxCopySizeInBytes = transferAsMappingSize - 1) &&;

    // Release the string via copy.
    std::optional<String> releaseToCopy() && { return WTF::move(*this).release(std::numeric_limits<size_t>::max()); };

    IPCData toIPCData() const LIFETIME_BOUND;

private:
    static std::optional<TransferString> createCopy(std::span<const Latin1Character>);
    static std::optional<TransferString> createCopy(std::span<const char16_t>);
    TransferString(String&&);
#if USE(CF)
    TransferString(CFStringRef);
#endif

#if USE(CF)
    using Storage = Variant<String, RetainPtr<CFStringRef>, SharedSpan8, SharedSpan16>;
#else
    using Storage = Variant<String, SharedSpan8, SharedSpan16>;
#endif
    Storage m_storage;
};

inline std::optional<TransferString> TransferString::create(const String& string)
{
    if (string.sizeInBytes() >= transferAsMappingSize) {
        if (string.is8Bit())
            return createCopy(string.span8());
        return createCopy(string.span16());
    }
    return TransferString { String { string } };
}

inline std::optional<TransferString> TransferString::create(StringView string)
{
    if (string.sizeInBytes() >= transferAsMappingSize) {
        if (string.is8Bit())
            return createCopy(string.span8());
        return createCopy(string.span16());
    }
    return TransferString { string.toString() };
}

inline TransferString::TransferString(String&& string)
    : m_storage(WTF::move(string))
{
}

#if USE(FOUNDATION) && defined(__OBJC__)
inline std::optional<TransferString> TransferString::create(NSString *string)
{
    return create((__bridge CFStringRef)string);
}
#endif

inline TransferString::TransferString(SharedSpan8&& handle)
    : m_storage(WTF::move(handle))
{
}

inline TransferString::TransferString(SharedSpan16&& handle)
    : m_storage(WTF::move(handle))
{
}

inline TransferString::TransferString(IPCData&& data)
{
    WTF::switchOn(WTF::move(data),
        [&](std::monostate) {
            m_storage = String { };
        },
        [&](std::span<const Latin1Character> characters) {
            m_storage = String { characters };
        },
        [&](std::span<const char16_t> characters) {
            m_storage = String { characters };
        },
        [&](SharedSpan8 handle) {
            m_storage = WTF::move(handle);
        },
        [&](SharedSpan16 handle) {
            m_storage = WTF::move(handle);
        }
    );
}

}
