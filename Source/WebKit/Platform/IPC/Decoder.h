/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
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

#include "Attachment.h"
#include "MessageNames.h"
#include "ReceiverMatcher.h"
#include <wtf/ArgumentCoder.h>
#include <wtf/Function.h>
#include <wtf/OptionSet.h>
#include <wtf/Vector.h>

#if PLATFORM(MAC)
#include "ImportanceAssertion.h"
#endif

namespace IPC {

enum class MessageFlags : uint8_t;
enum class ShouldDispatchWhenWaitingForSyncReply : uint8_t;

template<typename, typename> struct ArgumentCoder;
template<typename, typename, typename> struct HasLegacyDecoder;
template<typename, typename, typename> struct HasModernDecoder;

class Decoder {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<Decoder> create(const uint8_t* buffer, size_t bufferSize, Vector<Attachment>&&);
    using BufferDeallocator = Function<void(const uint8_t*, size_t)>;
    static std::unique_ptr<Decoder> create(const uint8_t* buffer, size_t bufferSize, BufferDeallocator&&, Vector<Attachment>&&);
    Decoder(const uint8_t* stream, size_t streamSize, uint64_t destinationID);

    ~Decoder();

    Decoder(const Decoder&) = delete;
    Decoder(Decoder&&) = delete;
    Decoder& operator=(const Decoder&) = delete;
    Decoder& operator=(Decoder&&) = delete;

    ReceiverName messageReceiverName() const { return receiverName(m_messageName); }
    MessageName messageName() const { return m_messageName; }
    uint64_t destinationID() const { return m_destinationID; }
    bool matches(const ReceiverMatcher& matcher) const { return matcher.matches(messageReceiverName(), destinationID()); }

    bool isSyncMessage() const { return messageIsSync(messageName()); }
    ShouldDispatchWhenWaitingForSyncReply shouldDispatchMessageWhenWaitingForSyncReply() const;
    bool isAllowedWhenWaitingForSyncReply() const { return messageAllowedWhenWaitingForSyncReply(messageName()) || m_isAllowedWhenWaitingForSyncReplyOverride; }
    bool isAllowedWhenWaitingForUnboundedSyncReply() const { return messageAllowedWhenWaitingForUnboundedSyncReply(messageName()); }
    bool shouldUseFullySynchronousModeForTesting() const;
    bool shouldMaintainOrderingWithAsyncMessages() const;
    void setIsAllowedWhenWaitingForSyncReplyOverride(bool value) { m_isAllowedWhenWaitingForSyncReplyOverride = value; }

#if PLATFORM(MAC)
    void setImportanceAssertion(ImportanceAssertion&&);
#endif

    static std::unique_ptr<Decoder> unwrapForTesting(Decoder&);

    const uint8_t* buffer() const { return m_buffer; }
    size_t currentBufferPosition() const { return m_bufferPos - m_buffer; }
    size_t length() const { return m_bufferEnd - m_buffer; }

    WARN_UNUSED_RETURN bool isValid() const { return m_bufferPos != nullptr; }
    void markInvalid() { m_bufferPos = nullptr; }

    template<typename T>
    WARN_UNUSED_RETURN Span<const T> decodeSpan(size_t);

    template<typename T>
    WARN_UNUSED_RETURN std::optional<T> decodeObject();

    template<typename T>
    WARN_UNUSED_RETURN bool decode(T& t)
    {
        using Impl = ArgumentCoder<std::remove_cvref_t<T>, void>;
        if constexpr(HasLegacyDecoder<T, Impl>::value) {
            if (UNLIKELY(!Impl::decode(*this, t))) {
                markInvalid();
                return false;
            }
        } else {
            std::optional<T> optional { decode<T>() };
            if (UNLIKELY(!optional)) {
                markInvalid();
                return false;
            }
            t = WTFMove(*optional);
        }
        return true;
    }

    template<typename T>
    Decoder& operator>>(std::optional<T>& t)
    {
        t = decode<T>();
        return *this;
    }

    // The preferred decode() function. Can decode T which is not default constructible when T
    // has a  modern decoder, e.g decoding function that returns std::optional.
    template<typename T>
    std::optional<T> decode()
    {
        using Impl = ArgumentCoder<std::remove_cvref_t<T>, void>;
        if constexpr(HasModernDecoder<T, Impl>::value) {
            std::optional<T> t { Impl::decode(*this) };
            if (UNLIKELY(!t))
                markInvalid();
            return t;
        } else {
            std::optional<T> t { T { } };
            if (LIKELY(Impl::decode(*this, *t)))
                return t;
            markInvalid();
            return std::nullopt;
        }
    }

    std::optional<Attachment> takeLastAttachment();

    static constexpr bool isIPCDecoder = true;

private:
    Decoder(const uint8_t* buffer, size_t bufferSize, BufferDeallocator&&, Vector<Attachment>&&);

    const uint8_t* m_buffer;
    const uint8_t* m_bufferPos;
    const uint8_t* m_bufferEnd;
    BufferDeallocator m_bufferDeallocator;

    Vector<Attachment> m_attachments;

    OptionSet<MessageFlags> m_messageFlags;
    MessageName m_messageName;

    uint64_t m_destinationID;
    bool m_isAllowedWhenWaitingForSyncReplyOverride { false };

#if PLATFORM(MAC)
    ImportanceAssertion m_importanceAssertion;
#endif
};

inline bool alignedBufferIsLargeEnoughToContain(const uint8_t* alignedPosition, const uint8_t* bufferStart, const uint8_t* bufferEnd, size_t size)
{
    // When size == 0 for the last argument and it's a variable length byte array,
    // bufferStart == alignedPosition == bufferEnd, so checking (bufferEnd >= alignedPosition)
    // is not an off-by-one error since (static_cast<size_t>(bufferEnd - alignedPosition) >= size)
    // will catch issues when size != 0.
    return bufferEnd >= alignedPosition && bufferStart <= alignedPosition && static_cast<size_t>(bufferEnd - alignedPosition) >= size;
}

template<typename T>
inline Span<const T> Decoder::decodeSpan(size_t size)
{
    if (size > std::numeric_limits<size_t>::max() / sizeof(T))
        return { };

    const uint8_t* alignedPosition = roundUpToMultipleOf<alignof(T)>(m_bufferPos);
    if (UNLIKELY(!alignedBufferIsLargeEnoughToContain(alignedPosition, m_buffer, m_bufferEnd, size * sizeof(T)))) {
        markInvalid();
        return { };
    }

    m_bufferPos = alignedPosition + size * sizeof(T);
    return { reinterpret_cast<const T*>(alignedPosition), size };
}

template<typename T>
inline std::optional<T> Decoder::decodeObject()
{
    static_assert(std::is_trivially_copyable_v<T>);

    auto data = decodeSpan<T>(1);
    if (!data.data())
        return std::nullopt;

    return data[0];
}

} // namespace IPC
