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

#include "ArgumentCoder.h"
#include "Attachment.h"
#include "MessageNames.h"
#include "StringReference.h"
#include <wtf/EnumTraits.h>
#include <wtf/Vector.h>

namespace IPC {

class DataReference;
enum class MessageFlags : uint8_t;
enum class MessageName : uint16_t;
enum class ShouldDispatchWhenWaitingForSyncReply : uint8_t;

class Encoder final {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Encoder(MessageName, uint64_t destinationID);
    ~Encoder();

    ReceiverName messageReceiverName() const { return receiverName(m_messageName); }
    MessageName messageName() const { return m_messageName; }
    uint64_t destinationID() const { return m_destinationID; }

    void setIsSyncMessage(bool);
    bool isSyncMessage() const;

    void setShouldDispatchMessageWhenWaitingForSyncReply(ShouldDispatchWhenWaitingForSyncReply);
    ShouldDispatchWhenWaitingForSyncReply shouldDispatchMessageWhenWaitingForSyncReply() const;

    void setFullySynchronousModeForTesting();

    void wrapForTesting(std::unique_ptr<Encoder>);

    void encodeFixedLengthData(const uint8_t* data, size_t, size_t alignment);
    void encodeVariableLengthByteArray(const DataReference&);

    template<typename E>
    void encodeEnum(E enumValue)
    {
        // FIXME: Remove this after migrating all uses of this function to encode() or operator<<() with WTF::isValidEnum check.
        encode(static_cast<typename std::underlying_type<E>::type>(enumValue));
    }

    template<typename T, std::enable_if_t<!std::is_enum<typename std::remove_const_t<std::remove_reference_t<T>>>::value && !std::is_arithmetic<typename std::remove_const_t<std::remove_reference_t<T>>>::value>* = nullptr>
    void encode(T&& t)
    {
        ArgumentCoder<typename std::remove_const<typename std::remove_reference<T>::type>::type>::encode(*this, std::forward<T>(t));
    }

    template<typename E, std::enable_if_t<std::is_enum<E>::value>* = nullptr>
    Encoder& operator<<(E&& enumValue)
    {
        ASSERT(WTF::isValidEnum<E>(static_cast<typename std::underlying_type<E>::type>(enumValue)));
        encode(static_cast<typename std::underlying_type<E>::type>(enumValue));
        return *this;
    }

    template<typename T, std::enable_if_t<!std::is_enum<T>::value>* = nullptr>
    Encoder& operator<<(T&& t)
    {
        encode(std::forward<T>(t));
        return *this;
    }

    template<typename T, std::enable_if_t<std::is_arithmetic<T>::value>* = nullptr>
    void encode(T value)
    {
        encodeFixedLengthData(reinterpret_cast<const uint8_t*>(&value), sizeof(T), alignof(T));
    }

    uint8_t* buffer() const { return m_buffer; }
    size_t bufferSize() const { return m_bufferSize; }

    void addAttachment(Attachment&&);
    Vector<Attachment> releaseAttachments();

    static const bool isIPCEncoder = true;

private:
    void reserve(size_t);

    uint8_t* grow(size_t alignment, size_t);

    template<typename E, std::enable_if_t<std::is_enum<E>::value>* = nullptr>
    void encode(E enumValue)
    {
        ASSERT(WTF::isValidEnum<E>(static_cast<typename std::underlying_type<E>::type>(enumValue)));
        encode(static_cast<typename std::underlying_type<E>::type>(enumValue));
    }

    void encodeHeader();
    const OptionSet<MessageFlags>& messageFlags() const;
    OptionSet<MessageFlags>& messageFlags();

    MessageName m_messageName;
    uint64_t m_destinationID;

    uint8_t m_inlineBuffer[512];

    uint8_t* m_buffer;
    uint8_t* m_bufferPointer;
    
    size_t m_bufferSize;
    size_t m_bufferCapacity;

    Vector<Attachment> m_attachments;
};

} // namespace IPC
