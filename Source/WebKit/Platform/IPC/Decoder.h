/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "ArgumentCoder.h"
#include "Attachment.h"
#include "StringReference.h"
#include <wtf/EnumTraits.h>
#include <wtf/Vector.h>

#if HAVE(QOS_CLASSES)
#include <pthread/qos.h>
#endif

namespace IPC {

class DataReference;
class ImportanceAssertion;
enum class ShouldDispatchWhenWaitingForSyncReply;

class Decoder {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Decoder(const uint8_t* buffer, size_t bufferSize, void (*bufferDeallocator)(const uint8_t*, size_t), Vector<Attachment>&&);
    ~Decoder();

    Decoder(const Decoder&) = delete;
    Decoder(Decoder&&) = delete;

    StringReference messageReceiverName() const { return m_messageReceiverName; }
    StringReference messageName() const { return m_messageName; }
    uint64_t destinationID() const { return m_destinationID; }

    bool isSyncMessage() const;
    ShouldDispatchWhenWaitingForSyncReply shouldDispatchMessageWhenWaitingForSyncReply() const;
    bool shouldUseFullySynchronousModeForTesting() const;

#if PLATFORM(MAC)
    void setImportanceAssertion(std::unique_ptr<ImportanceAssertion>);
#endif

#if HAVE(QOS_CLASSES)
    void setQOSClassOverride(pthread_override_t override) { m_qosClassOverride = override; }
#endif

    static std::unique_ptr<Decoder> unwrapForTesting(Decoder&);

    size_t length() const { return m_bufferEnd - m_buffer; }

    bool isInvalid() const { return m_bufferPos > m_bufferEnd; }
    void markInvalid() { m_bufferPos = m_bufferEnd + 1; }

    bool decodeFixedLengthData(uint8_t*, size_t, unsigned alignment);

    // The data in the data reference here will only be valid for the lifetime of the ArgumentDecoder object.
    bool decodeVariableLengthByteArray(DataReference&);

    bool decode(bool&);
    Decoder& operator>>(Optional<bool>&);
    bool decode(uint8_t&);
    Decoder& operator>>(Optional<uint8_t>&);
    bool decode(uint16_t&);
    Decoder& operator>>(Optional<uint16_t>&);
    bool decode(uint32_t&);
    Decoder& operator>>(Optional<uint32_t>&);
    bool decode(uint64_t&);
    Decoder& operator>>(Optional<uint64_t>&);
    bool decode(int16_t&);
    Decoder& operator>>(Optional<int16_t>&);
    bool decode(int32_t&);
    Decoder& operator>>(Optional<int32_t>&);
    bool decode(int64_t&);
    Decoder& operator>>(Optional<int64_t>&);
    bool decode(float&);
    Decoder& operator>>(Optional<float>&);
    bool decode(double&);
    Decoder& operator>>(Optional<double>&);

    template<typename E>
    auto decode(E& e) -> std::enable_if_t<std::is_enum<E>::value, bool>
    {
        uint64_t value;
        if (!decode(value))
            return false;
        if (!isValidEnum<E>(value))
            return false;

        e = static_cast<E>(value);
        return true;
    }

    template<typename E, std::enable_if_t<std::is_enum<E>::value>* = nullptr>
    Decoder& operator>>(Optional<E>& optional)
    {
        Optional<uint64_t> value;
        *this >> value;
        if (value && isValidEnum<E>(*value))
            optional = static_cast<E>(*value);
        return *this;
    }

    template<typename T> bool decodeEnum(T& result)
    {
        static_assert(sizeof(T) <= 8, "Enum type T must not be larger than 64 bits!");

        uint64_t value;
        if (!decode(value))
            return false;
        
        result = static_cast<T>(value);
        return true;
    }

    template<typename T>
    bool bufferIsLargeEnoughToContain(size_t numElements) const
    {
        static_assert(std::is_arithmetic<T>::value, "Type T must have a fixed, known encoded size!");

        if (numElements > std::numeric_limits<size_t>::max() / sizeof(T))
            return false;

        return bufferIsLargeEnoughToContain(alignof(T), numElements * sizeof(T));
    }

    template<typename T, std::enable_if_t<!std::is_enum<T>::value && UsesLegacyDecoder<T>::value>* = nullptr>
    bool decode(T& t)
    {
        return ArgumentCoder<T>::decode(*this, t);
    }

    template<typename T, std::enable_if_t<!std::is_enum<T>::value && !UsesLegacyDecoder<T>::value>* = nullptr>
    bool decode(T& t)
    {
        Optional<T> optional;
        *this >> optional;
        if (!optional)
            return false;
        t = WTFMove(*optional);
        return true;
    }

    template<typename T, std::enable_if_t<UsesModernDecoder<T>::value>* = nullptr>
    Decoder& operator>>(Optional<T>& t)
    {
        t = ArgumentCoder<T>::decode(*this);
        return *this;
    }
    
    template<typename T, std::enable_if_t<!std::is_enum<T>::value && !UsesModernDecoder<T>::value>* = nullptr>
    Decoder& operator>>(Optional<T>& optional)
    {
        T t;
        if (ArgumentCoder<T>::decode(*this, t)) {
            optional = WTFMove(t);
            return *this;
        }
        optional = WTF::nullopt;
        return *this;
    }

    bool removeAttachment(Attachment&);

    static const bool isIPCDecoder = true;

private:
    bool alignBufferPosition(unsigned alignment, size_t);
    bool bufferIsLargeEnoughToContain(unsigned alignment, size_t) const;
    template<typename Type> Decoder& getOptional(Optional<Type>&);

    const uint8_t* m_buffer;
    const uint8_t* m_bufferPos;
    const uint8_t* m_bufferEnd;
    void (*m_bufferDeallocator)(const uint8_t*, size_t);

    Vector<Attachment> m_attachments;

    uint8_t m_messageFlags;
    StringReference m_messageReceiverName;
    StringReference m_messageName;

    uint64_t m_destinationID;

#if PLATFORM(MAC)
    std::unique_ptr<ImportanceAssertion> m_importanceAssertion;
#endif

#if HAVE(QOS_CLASSES)
    pthread_override_t m_qosClassOverride { nullptr };
#endif
};

} // namespace IPC
