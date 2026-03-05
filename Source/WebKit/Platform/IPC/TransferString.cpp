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

#include "config.h"
#include "TransferString.h"

#include <wtf/text/ExternalStringImpl.h>

#if USE(CF)
#include <wtf/cf/VectorCF.h>
#endif

namespace IPC {

#if USE(CF)
std::optional<TransferString> TransferString::create(CFStringRef string)
{
    if (!string)
        return TransferString { String { } };
    CFIndex size = CFStringGetLength(string);
    if (!size)
        return TransferString { ""_s };
    // The transferAsMappingSize checks are inside the `if`s because we want to make sure
    // that the getter will return the data. We need that during ::toIPCData.
    if (auto span8 = CFStringGetLatin1CStringSpan(string); !span8.empty()) {
        if (span8.size_bytes() < transferAsMappingSize)
            return TransferString { string };
        return createCopy(span8);
    }
    if (auto span16 = CFStringGetCharactersSpan(string); !span16.empty()) {
        if (span16.size_bytes() < transferAsMappingSize)
            return TransferString { string };
        return createCopy(span16);
    }
    // Not native Latin1 or UTF-16 string, needs a copy from the CFString
    // to get UTF-16. Copy small ones to be held and sent as String.
    auto dataSize = static_cast<size_t>(size) * sizeof(char16_t);
    if (dataSize < transferAsMappingSize)
        return TransferString { String { string } };
    RefPtr buffer = WebCore::SharedMemory::allocate(dataSize);
    if (!buffer)
        return std::nullopt;
    auto bufferSpan = spanReinterpretCast<char16_t>(buffer->mutableSpan());
    CFStringCopyCharactersSpan(string, bufferSpan);
    auto handle = buffer->createHandle(WebCore::SharedMemoryProtection::ReadOnly);
    if (!handle)
        return std::nullopt;
    return std::optional<TransferString> { std::in_place, SharedSpan16 { WTF::move(*handle) } };
}

TransferString::TransferString(CFStringRef string)
    : m_storage(RetainPtr { string })
{
}
#endif

std::optional<TransferString> TransferString::createCopy(std::span<const Latin1Character> span8)
{
    auto handle = WebCore::SharedMemoryHandle::createCopy(span8, WebCore::SharedMemoryProtection::ReadOnly);
    if (!handle)
        return std::nullopt;
    return std::optional<TransferString> { std::in_place, SharedSpan8 { WTF::move(*handle) } };
}

std::optional<TransferString> TransferString::createCopy(std::span<const char16_t> span16)
{
    auto handle = WebCore::SharedMemoryHandle::createCopy(asBytes(span16), WebCore::SharedMemoryProtection::ReadOnly);
    if (!handle)
        return std::nullopt;
    return std::optional<TransferString> { std::in_place, SharedSpan16 { WTF::move(*handle) } };
}

std::optional<String> TransferString::release(size_t maxCopySizeInBytes) && // NOLINT
{ // NOLINT
    return WTF::switchOn(WTF::move(m_storage),
        [](String string) {
            return std::optional<String> { std::in_place, WTF::move(string) };
        },
#if USE(CF)
        [](RetainPtr<CFStringRef> string) {
            return std::optional<String> { std::in_place, string.get() };
        },
#endif
        [maxCopySizeInBytes](SharedSpan8 handle) -> std::optional<String> {
            RefPtr memory = WebCore::SharedMemory::map(WTF::move(handle.dataHandle), WebCore::SharedMemoryProtection::ReadOnly);
            if (!memory)
                return std::nullopt;
            if (memory->size() > maxCopySizeInBytes) {
                Ref<StringImpl> impl = ExternalStringImpl::create(byteCast<Latin1Character>(memory->span()), [memory = memory.releaseNonNull()] (auto...) mutable { });
                return std::optional<String> { std::in_place, String { WTF::move(impl) } };
            }
            return std::optional<String> { std::in_place, String { byteCast<Latin1Character>(memory->span()) } };
        },
        [maxCopySizeInBytes](SharedSpan16 handle) -> std::optional<String> {
            RefPtr memory = WebCore::SharedMemory::map(WTF::move(handle.dataHandle), WebCore::SharedMemoryProtection::ReadOnly);
            if (!memory || (memory->size() % sizeof(char16_t)))
                return std::nullopt;
            if (memory->size() > maxCopySizeInBytes) {
                Ref<StringImpl> impl = ExternalStringImpl::create(spanReinterpretCast<const char16_t>(memory->span()), [memory = memory.releaseNonNull()] (auto...) mutable { });
                return std::optional<String> { std::in_place, String { WTF::move(impl) } };
            }
            return std::optional<String> { std::in_place, String { spanReinterpretCast<const char16_t>(memory->span()) } };
        }
    );
}

TransferString::IPCData TransferString::toIPCData() const LIFETIME_BOUND
{
    return WTF::switchOn(m_storage,
        [](const String& string) {
            if (string.isNull())
                return IPCData { std::monostate { } };
            if (string.is8Bit())
                return IPCData { string.span8() };
            return IPCData { string.span16() };
        },
#if USE(CF)
        [](const RetainPtr<CFStringRef>& string) {
            if (auto span8 = CFStringGetLatin1CStringSpan(string.get()); !span8.empty())
                return IPCData { span8 };
            return IPCData { CFStringGetCharactersSpan(string.get()) };
        },
#endif
        [](const SharedSpan8& handle) {
            return IPCData { SharedSpan8 { WebCore::SharedMemoryHandle { handle.dataHandle } } };
        },
        [](const SharedSpan16& handle) -> IPCData {
            return IPCData { SharedSpan16 { WebCore::SharedMemoryHandle { handle.dataHandle } } };
        }
    );
}

}
