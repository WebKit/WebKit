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

namespace IPC {

class DataReference;
enum class ShouldDispatchWhenWaitingForSyncReply;

class Encoder final {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Encoder(StringReference messageReceiverName, StringReference messageName, uint64_t destinationID);
    ~Encoder();

    StringReference messageReceiverName() const { return m_messageReceiverName; }
    StringReference messageName() const { return m_messageName; }
    uint64_t destinationID() const { return m_destinationID; }

    void setIsSyncMessage(bool);
    bool isSyncMessage() const;

    void setShouldDispatchMessageWhenWaitingForSyncReply(ShouldDispatchWhenWaitingForSyncReply);
    ShouldDispatchWhenWaitingForSyncReply shouldDispatchMessageWhenWaitingForSyncReply() const;

    void setFullySynchronousModeForTesting();

    void wrapForTesting(std::unique_ptr<Encoder>);

    void encodeFixedLengthData(const uint8_t*, size_t, unsigned alignment);
    void encodeVariableLengthByteArray(const DataReference&);

    template<typename T> void encodeEnum(T t)
    {
        COMPILE_ASSERT(sizeof(T) <= sizeof(uint64_t), enum_type_must_not_be_larger_than_64_bits);

        encode(static_cast<uint64_t>(t));
    }

    template<typename T, std::enable_if_t<!std::is_enum<typename std::remove_const_t<std::remove_reference_t<T>>>::value>* = nullptr>
    void encode(T&& t)
    {
        ArgumentCoder<typename std::remove_const<typename std::remove_reference<T>::type>::type>::encode(*this, std::forward<T>(t));
    }

    template<typename T, std::enable_if_t<std::is_enum<T>::value>* = nullptr>
    Encoder& operator<<(T&& t)
    {
        encode(static_cast<uint64_t>(t));
        return *this;
    }

    template<typename T, std::enable_if_t<!std::is_enum<T>::value>* = nullptr>
    Encoder& operator<<(T&& t)
    {
        encode(std::forward<T>(t));
        return *this;
    }

    uint8_t* buffer() const { return m_buffer; }
    size_t bufferSize() const { return m_bufferSize; }

    void addAttachment(Attachment&&);
    Vector<Attachment> releaseAttachments();
    void reserve(size_t);

    static const bool isIPCEncoder = true;

    void encode(uint64_t);

private:
    uint8_t* grow(unsigned alignment, size_t);

    void encode(bool);
    void encode(uint8_t);
    void encode(uint16_t);
    void encode(uint32_t);
    void encode(int16_t);
    void encode(int32_t);
    void encode(int64_t);
    void encode(float);
    void encode(double);

    template<typename E>
    auto encode(E value) -> std::enable_if_t<std::is_enum<E>::value>
    {
        static_assert(sizeof(E) <= sizeof(uint64_t), "Enum type must not be larger than 64 bits.");

        ASSERT(isValidEnum<E>(static_cast<uint64_t>(value)));
        encode(static_cast<uint64_t>(value));
    }

    void encodeHeader();

    StringReference m_messageReceiverName;
    StringReference m_messageName;
    uint64_t m_destinationID;

    uint8_t m_inlineBuffer[512];

    uint8_t* m_buffer;
    uint8_t* m_bufferPointer;
    
    size_t m_bufferSize;
    size_t m_bufferCapacity;

    Vector<Attachment> m_attachments;
};

} // namespace IPC
