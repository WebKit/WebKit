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
#include "StringReference.h"
#include <WebCore/SharedBuffer.h>
#include <wtf/OptionSet.h>
#include <wtf/Vector.h>

#if HAVE(QOS_CLASSES)
#include <pthread/qos.h>
#endif

namespace IPC {

class ImportanceAssertion;
enum class MessageFlags : uint8_t;
enum class ShouldDispatchWhenWaitingForSyncReply : uint8_t;

template<typename, typename> struct ArgumentCoder;
template<typename, typename> struct HasLegacyDecoder;
template<typename, typename> struct HasModernDecoder;

class Decoder {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<Decoder> create(const uint8_t* buffer, size_t bufferSize, void (*bufferDeallocator)(const uint8_t*, size_t), Vector<Attachment>&&);
    Decoder(const uint8_t* stream, size_t streamSize, uint64_t destinationID);

    ~Decoder();

    Decoder(const Decoder&) = delete;
    Decoder(Decoder&&) = delete;

    ReceiverName messageReceiverName() const { return receiverName(m_messageName); }
    MessageName messageName() const { return m_messageName; }
    uint64_t destinationID() const { return m_destinationID; }

    bool isSyncMessage() const { return messageIsSync(messageName()); }
    ShouldDispatchWhenWaitingForSyncReply shouldDispatchMessageWhenWaitingForSyncReply() const;
    bool shouldUseFullySynchronousModeForTesting() const;

#if PLATFORM(MAC)
    void setImportanceAssertion(std::unique_ptr<ImportanceAssertion>);
#endif

    static std::unique_ptr<Decoder> unwrapForTesting(Decoder&);

    const uint8_t* buffer() const { return m_buffer; }
    size_t currentBufferPosition() const { return m_bufferPos - m_buffer; }
    size_t length() const { return m_bufferEnd - m_buffer; }

    WARN_UNUSED_RETURN bool isValid() const { return m_bufferPos != nullptr; }
    void markInvalid() { m_bufferPos = nullptr; }

    WARN_UNUSED_RETURN bool decodeFixedLengthData(uint8_t* data, size_t, size_t alignment);

    // The data in the returned pointer here will only be valid for the lifetime of the Decoder object.
    // Returns nullptr on failure.
    WARN_UNUSED_RETURN const uint8_t* decodeFixedLengthReference(size_t, size_t alignment);

    template<typename T>
    WARN_UNUSED_RETURN bool decode(T& t)
    {
        using Impl = ArgumentCoder<std::remove_const_t<std::remove_reference_t<T>>, void>;
        if constexpr(HasLegacyDecoder<T, Impl>::value)
            return Impl::decode(*this, t);
        else {
            Optional<T> optional;
            *this >> optional;
            if (!optional)
                return false;
            t = WTFMove(*optional);
            return true;
        }
    }

    template<typename T>
    Decoder& operator>>(Optional<T>& t)
    {
        using Impl = ArgumentCoder<std::remove_const_t<std::remove_reference_t<T>>, void>;
        if constexpr(HasModernDecoder<T, Impl>::value)
            t = Impl::decode(*this);
        else {
            T v;
            if (Impl::decode(*this, v))
                t = WTFMove(v);
        }
        return *this;
    }

    template<typename T>
    bool bufferIsLargeEnoughToContain(size_t numElements) const
    {
        static_assert(std::is_arithmetic<T>::value, "Type T must have a fixed, known encoded size!");

        if (numElements > std::numeric_limits<size_t>::max() / sizeof(T))
            return false;

        return bufferIsLargeEnoughToContain(alignof(T), numElements * sizeof(T));
    }

    bool removeAttachment(Attachment&);

    static const bool isIPCDecoder = true;

    template <typename T>
    static Optional<T> decodeSingleObject(const uint8_t* source, size_t numberOfBytes)
    {
        Optional<T> result;
        Decoder decoder(source, numberOfBytes, ConstructWithoutHeader);
        if (!decoder.isValid())
            return WTF::nullopt;

        decoder >> result;
        return result;
    }

private:
    Decoder(const uint8_t* buffer, size_t bufferSize, void (*bufferDeallocator)(const uint8_t*, size_t), Vector<Attachment>&&);

    enum ConstructWithoutHeaderTag { ConstructWithoutHeader };
    Decoder(const uint8_t* buffer, size_t bufferSize, ConstructWithoutHeaderTag);

    bool alignBufferPosition(size_t alignment, size_t);
    bool bufferIsLargeEnoughToContain(size_t alignment, size_t) const;

    const uint8_t* m_buffer;
    const uint8_t* m_bufferPos;
    const uint8_t* m_bufferEnd;
    void (*m_bufferDeallocator)(const uint8_t*, size_t);

    Vector<Attachment> m_attachments;

    OptionSet<MessageFlags> m_messageFlags;
    MessageName m_messageName;

    uint64_t m_destinationID;

#if PLATFORM(MAC)
    std::unique_ptr<ImportanceAssertion> m_importanceAssertion;
#endif
};

} // namespace IPC
