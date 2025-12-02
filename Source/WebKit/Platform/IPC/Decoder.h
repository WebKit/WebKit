/*
 * Copyright (C) 2010-2023 Apple Inc. All rights reserved.
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
#include "SyncRequestID.h"
#include <memory>
#include <span>
#include <wtf/ArgumentCoder.h>
#include <wtf/Function.h>
#include <wtf/HashSet.h>
#include <wtf/Markable.h>
#include <wtf/OptionSet.h>
#include <wtf/RetainPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/text/ASCIILiteral.h>

#if PLATFORM(COCOA)
#include "ClassStructPtr.h"
#endif

#if PLATFORM(MAC)
#include "ImportanceAssertion.h"
#endif

#if !USE(SYSTEM_MALLOC)
#include <bmalloc/TZoneHeap.h>
#include <bmalloc/bmalloc.h>
#endif

namespace IPC {

enum class MessageFlags : uint8_t;
enum class ShouldDispatchWhenWaitingForSyncReply : uint8_t;

template<typename> struct ArgumentCoder;

#ifdef __OBJC__
template<typename T> using IsObjCObject = std::enable_if_t<std::is_convertible<T *, id>::value, T *>;
template<typename T> using IsNotObjCObject = std::enable_if_t<!std::is_convertible<T *, id>::value, T *>;
template<typename T, typename = IsObjCObject<T>> std::optional<RetainPtr<T>> decodeRequiringAllowedClasses(Decoder&);

template<typename T, typename = IsObjCObject<T>> Class getClass()
{
    return [T class];
}
#endif

#if PLATFORM(COCOA)
using AllowedClassHashSet = HashSet<RetainPtr<ClassStructPtr>>;
#endif

class Decoder {
    WTF_MAKE_TZONE_ALLOCATED(Decoder);
public:
    static std::unique_ptr<Decoder> create(std::span<const uint8_t> buffer, Vector<Attachment>&&);
    using BufferDeallocator = Function<void(std::span<const uint8_t>)>;
    static std::unique_ptr<Decoder> create(std::span<const uint8_t> buffer, BufferDeallocator&&, Vector<Attachment>&&);
    Decoder(std::span<const uint8_t> stream, uint64_t destinationID);

    ~Decoder();

    Decoder() = delete;
    Decoder(const Decoder&) = delete;
    Decoder(Decoder&&) = delete;
    Decoder& operator=(const Decoder&) = delete;
    Decoder& operator=(Decoder&&) = delete;

    ReceiverName messageReceiverName() const { return receiverName(m_messageName); }
    MessageName messageName() const { return m_messageName; }
    uint64_t destinationID() const { return m_destinationID; }
    SyncRequestID syncRequestID() const { ASSERT(m_syncRequestID); return *m_syncRequestID; }
    bool matches(const ReceiverMatcher& matcher) const { return matcher.matches(messageReceiverName(), destinationID()); }

    bool isSyncMessage() const { return messageIsSync(messageName()); }
    ShouldDispatchWhenWaitingForSyncReply shouldDispatchMessageWhenWaitingForSyncReply() const;
    bool isAllowedWhenWaitingForSyncReply() const { return messageAllowedWhenWaitingForSyncReply(messageName()) || m_isAllowedWhenWaitingForSyncReplyOverride; }
    bool isAllowedWhenWaitingForUnboundedSyncReply() const { return messageAllowedWhenWaitingForUnboundedSyncReply(messageName()); }
    bool shouldUseFullySynchronousModeForTesting() const;
    bool shouldMaintainOrderingWithAsyncMessages() const;
    void setIsAllowedWhenWaitingForSyncReplyOverride(bool value) { m_isAllowedWhenWaitingForSyncReplyOverride = value; }
    bool isAsyncReplyMessage() const { return isAsyncReply(messageName()); }

#if PLATFORM(MAC)
    void setImportanceAssertion(ImportanceAssertion&&);
#endif

#if ENABLE(IPC_TESTING_API)
    bool hasErrorString() const { return !m_errorString.isNull(); }
    void setErrorString(ASCIILiteral error)
    {
        if (!hasErrorString())
            m_errorString = error;
    }
    ASCIILiteral takeErrorString() { return std::exchange(m_errorString, ASCIILiteral { nullptr }); }
#endif

    static std::unique_ptr<Decoder> unwrapForTesting(Decoder&);

    std::span<const uint8_t> span() const { return m_buffer; }
    size_t currentBufferOffset() const { return std::distance(m_buffer.begin(), m_bufferPosition); }

    WARN_UNUSED_RETURN bool isValid() const { return !!m_buffer.data(); }
    void markInvalid()
    {
        auto buffer = std::exchange(m_buffer, { });
        if (m_bufferDeallocator && !buffer.empty())
            m_bufferDeallocator(WTFMove(buffer));
    }

    template<typename T>
    WARN_UNUSED_RETURN std::span<const T> decodeSpan(size_t);

    template<typename T>
    WARN_UNUSED_RETURN std::optional<T> decodeObject();

    template<typename T>
    Decoder& operator>>(std::optional<T>& t)
    {
        t = decode<T>();
        return *this;
    }

    template<typename T>
    std::optional<T> decode()
    {
        std::optional<T> t { ArgumentCoder<std::remove_cvref_t<T>>::decode(*this) };
        if (!t) [[unlikely]]
            markInvalid();
        return t;
    }

#ifdef __OBJC__
    template<typename T, typename = IsObjCObject<T>>
    std::optional<RetainPtr<T>> decodeWithAllowedClasses(const AllowedClassHashSet& allowedClasses = { getClass<T>() })
    {
#if HAVE(WK_SECURE_CODING_NSURLREQUEST)
        UNUSED_PARAM(allowedClasses);
#else
        m_allowedClasses = allowedClasses;
#endif
        return IPC::decodeRequiringAllowedClasses<T>(*this);
    }

    template<typename T, typename = IsNotObjCObject<T>>
    std::optional<T> decodeWithAllowedClasses(const AllowedClassHashSet& allowedClasses)
    {
#if HAVE(WK_SECURE_CODING_NSURLREQUEST)
        UNUSED_PARAM(allowedClasses);
#else
        m_allowedClasses = allowedClasses;
#endif
        return decode<T>();
    }

#if !HAVE(WK_SECURE_CODING_NSURLREQUEST)
    AllowedClassHashSet& allowedClasses() { return m_allowedClasses; }
#endif // !HAVE(WK_SECURE_CODING_NSURLREQUEST)
#endif // __OBJC__

    std::optional<Attachment> takeLastAttachment();

    void addIndexOfDecodingFailure(uint32_t indexOfObjectFailingDecoding) { m_indicesOfObjectsFailingDecoding.append(indexOfObjectFailingDecoding); }
    const Vector<uint32_t>& indicesOfObjectsFailingDecoding() const { return m_indicesOfObjectsFailingDecoding; }

private:
    Decoder(std::span<const uint8_t> buffer, BufferDeallocator&&, Vector<Attachment>&&);

    std::span<const uint8_t> m_buffer;
    std::span<const uint8_t>::iterator m_bufferPosition;
    BufferDeallocator m_bufferDeallocator;

    Vector<Attachment> m_attachments;

    OptionSet<MessageFlags> m_messageFlags;
    bool m_isAllowedWhenWaitingForSyncReplyOverride { false };
    MessageName m_messageName { MessageName::Invalid };

#if PLATFORM(MAC)
    ImportanceAssertion m_importanceAssertion;
#endif
#if PLATFORM(COCOA) && !HAVE(WK_SECURE_CODING_NSURLREQUEST)
    AllowedClassHashSet m_allowedClasses;
#endif

    uint64_t m_destinationID;
    Markable<SyncRequestID> m_syncRequestID;

    Vector<uint32_t> m_indicesOfObjectsFailingDecoding;

#if ENABLE(IPC_TESTING_API)
    ASCIILiteral m_errorString;
#endif
};

template<>
inline std::optional<Attachment> Decoder::decode<Attachment>()
{
    return takeLastAttachment();
}

inline bool alignedBufferIsLargeEnoughToContain(size_t bufferSize, const size_t alignedBufferPosition, size_t bytesNeeded)
{
    // When bytesNeeded == 0 for the last argument and it's a variable length byte array,
    // alignedBufferPosition == bufferSize, so checking (bufferSize >= alignedBufferPosition)
    // is not an off-by-one error since (bufferSize - alignedBufferPosition) >= bytesNeeded)
    // will catch issues when bytesNeeded > 0.
    return (bufferSize >= alignedBufferPosition) && ((bufferSize - alignedBufferPosition) >= bytesNeeded);
}

template<typename T>
inline std::span<const T> Decoder::decodeSpan(size_t size)
{
    if (size > std::numeric_limits<size_t>::max() / sizeof(T))
        return { };

    const size_t alignedBufferPosition = static_cast<size_t>(std::distance(m_buffer.data(), roundUpToMultipleOf<alignof(T)>(std::to_address(m_bufferPosition))));
    const size_t bytesNeeded = size * sizeof(T);
    if (!alignedBufferIsLargeEnoughToContain(m_buffer.size_bytes(), alignedBufferPosition, bytesNeeded)) [[unlikely]] {
        markInvalid();
        return { };
    }

    m_bufferPosition = m_buffer.begin() + alignedBufferPosition + bytesNeeded;
    return spanReinterpretCast<const T>(m_buffer.subspan(alignedBufferPosition, bytesNeeded));
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
