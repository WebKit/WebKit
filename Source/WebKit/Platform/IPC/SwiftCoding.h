/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "Connection.h"
#include "Decoder.h"
#include "Encoder.h"
#include <wtf/NonEscapableBorrow.h>
#include <wtf/SwiftBridging.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/UniqueRef.h>

namespace IPC {

// The primitive decoders of the Decoder of a message whose Swift receiver takes Swift types, lent to
// generated Swift. A failed decode returns false and invalidates the decoder. Values are returned
// through out parameters because Swift would import std::optional<uint64_t> as Optional<UInt> on Linux.
class SwiftDecoder : public NonEscapableBorrow<Decoder> {
public:
    [[nodiscard]] bool decodeBool(bool& value) { return decode(value); }
    [[nodiscard]] bool decodeUInt8(uint8_t& value) { return decode(value); }
    [[nodiscard]] bool decodeUInt16(uint16_t& value) { return decode(value); }
    [[nodiscard]] bool decodeUInt32(uint32_t& value) { return decode(value); }
    [[nodiscard]] bool decodeUInt64(uint64_t& value) { return decode(value); }
    [[nodiscard]] bool decodeInt8(int8_t& value) { return decode(value); }
    [[nodiscard]] bool decodeInt16(int16_t& value) { return decode(value); }
    [[nodiscard]] bool decodeInt32(int32_t& value) { return decode(value); }
    [[nodiscard]] bool decodeInt64(int64_t& value) { return decode(value); }
    [[nodiscard]] bool decodeFloat(float& value) { return decode(value); }
    [[nodiscard]] bool decodeDouble(double& value) { return decode(value); }

    // Aligns the decoder as Decoder::decodeSpan aligns elementCount contiguous integers of elementSize
    // (1, 2, 4 or 8) bytes, even when elementCount is 0, and checks they are all there, so they can be
    // decoded one at a time. No span crosses into Swift because not every Swift toolchain imports one
    // as a safe Span.
    [[nodiscard]] bool beginArray(size_t elementCount, size_t elementSize)
    {
        auto& decoder = borrowed();
        bool aligned = [&] {
            switch (elementSize) {
            case 1:
                return !!decoder.decodeSpan<uint8_t>(0).data();
            case 2:
                return !!decoder.decodeSpan<uint16_t>(0).data();
            case 4:
                return !!decoder.decodeSpan<uint32_t>(0).data();
            case 8:
                return !!decoder.decodeSpan<uint64_t>(0).data();
            default:
                return false;
            }
        }();
        if (!aligned || elementCount > remainingByteCount() / elementSize) [[unlikely]] {
            decoder.markInvalid();
            return false;
        }
        return true;
    }

    size_t remainingByteCount() const
    {
        auto& decoder = borrowed();
        auto span = decoder.span();
        size_t offset = decoder.currentBufferOffset();
        return offset < span.size() ? span.size() - offset : 0;
    }

    bool isValid() const { return borrowed().isValid(); }
    void markInvalid() { borrowed().markInvalid(); }

private:
    friend class NonEscapableBorrowLender<SwiftDecoder>;

    explicit SwiftDecoder(NonEscapableBorrowLease<Decoder>& lease LIFETIME_BOUND)
        : NonEscapableBorrow(lease)
    {
    }

    template<typename T> [[nodiscard]] bool decode(T& value)
    {
        auto decoded = borrowed().decode<T>();
        if (!decoded)
            return false;
        value = *decoded;
        return true;
    }
} SWIFT_NONESCAPABLE;

// The reply to a message whose Swift receiver takes Swift types, which generated Swift encodes and sends.
// Cancelling it, or dropping it unsent, sends the default reply that cancelling a C++ handler's
// completion handler would.
class SwiftReplyEncoder final : public ThreadSafeRefCounted<SwiftReplyEncoder> {
public:
    using DefaultReplyEncoder = void (*)(Encoder&);

    static Ref<SwiftReplyEncoder> create(Connection& connection, UniqueRef<Encoder>&& encoder, DefaultReplyEncoder encodeDefaultReply)
    {
        return adoptRef(*new SwiftReplyEncoder(connection, WTF::move(encoder), encodeDefaultReply));
    }

    ~SwiftReplyEncoder()
    {
        if (m_encoder)
            cancel();
    }

#ifdef __swift__
    // FIXME: rdar://165684636 means we have to define these at this level of the
    // type hierarchy.
    void ref() const { ThreadSafeRefCounted<SwiftReplyEncoder>::ref(); }
    void deref() const { ThreadSafeRefCounted<SwiftReplyEncoder>::deref(); }
#endif

    void encode(bool value) { encodeValue(value); }
    void encode(uint8_t value) { encodeValue(value); }
    void encode(uint16_t value) { encodeValue(value); }
    void encode(uint32_t value) { encodeValue(value); }
    void encode(uint64_t value) { encodeValue(value); }
    void encode(int8_t value) { encodeValue(value); }
    void encode(int16_t value) { encodeValue(value); }
    void encode(int32_t value) { encodeValue(value); }
    void encode(int64_t value) { encodeValue(value); }
    void encode(float value) { encodeValue(value); }
    void encode(double value) { encodeValue(value); }

    void send()
    {
        if (auto encoder = std::exchange(m_encoder, nullptr))
            m_connection->sendSyncReply(makeUniqueRefFromNonNullUniquePtr(WTF::move(encoder)));
    }

    void cancel()
    {
        auto encoder = std::exchange(m_encoder, nullptr);
        if (!encoder)
            return;
        m_encodeDefaultReply(*encoder);
        m_connection->sendSyncReply(makeUniqueRefFromNonNullUniquePtr(WTF::move(encoder)));
    }

    // Gives back the unsent encoder of a message that failed to decode, so IPC replies as for any
    // unhandled message.
    std::unique_ptr<Encoder> discard() { return std::exchange(m_encoder, nullptr); }

private:
    SwiftReplyEncoder(Connection& connection, UniqueRef<Encoder>&& encoder, DefaultReplyEncoder encodeDefaultReply)
        : m_connection(connection)
        , m_encoder(encoder.moveToUniquePtr())
        , m_encodeDefaultReply(encodeDefaultReply)
    {
    }

    template<typename T> void encodeValue(T value)
    {
        RELEASE_ASSERT(m_encoder);
        *m_encoder << value;
    }

    const Ref<Connection> m_connection;
    std::unique_ptr<Encoder> m_encoder;
    DefaultReplyEncoder m_encodeDefaultReply;
} DERIVED_CLASS_SWIFT_SHARED_REFERENCE(.ref, .deref);

} // namespace IPC
