/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "ArgumentCoders.h"
#include "PushMessageForTesting.h"
#include <WebCore/PushSubscriptionIdentifier.h>
#include <optional>
#include <wtf/Forward.h>
#include <wtf/URL.h>
#include <wtf/UUID.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
struct ExceptionData;
class CertificateInfo;
struct PushSubscriptionData;
class PrivateClickMeasurement;
struct SecurityOriginData;
class RegistrableDomain;
namespace PCM {
struct AttributionTimeToSendData;
struct AttributionTriggerData;
struct EphemeralNonce;
}
}

namespace WebKit {

struct WebPushMessage;

namespace WebPushD {
struct PushMessageForTesting;
struct WebPushDaemonConnectionConfiguration;
}

namespace Daemon {

class Encoder;
class Decoder;

template<typename T, typename = void> struct Coder;

template<typename T> struct Coder<T, typename std::enable_if_t<std::is_arithmetic_v<T>>> {
    template<typename Encoder> static void encode(Encoder& encoder, T value)
    {
        encoder.encodeFixedLengthData({ reinterpret_cast<const uint8_t*>(&value), sizeof(T) });
    }
    template<typename Decoder> static std::optional<T> decode(Decoder& decoder)
    {
        if (T result; decoder.decodeFixedLengthData(reinterpret_cast<uint8_t*>(&result), sizeof(T)))
            return result;
        return std::nullopt;
    }
};

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity> struct Coder<Vector<T, inlineCapacity, OverflowHandler, minCapacity>> {
    template<typename Encoder> static void encode(Encoder& encoder, const Vector<T, inlineCapacity, OverflowHandler, minCapacity>& vector)
    {
        encoder << static_cast<uint64_t>(vector.size());
        for (size_t i = 0; i < vector.size(); ++i)
            encoder << vector[i];
    }
    template<typename Decoder> static std::optional<Vector<T, inlineCapacity, OverflowHandler, minCapacity>> decode(Decoder& decoder)
    {
        std::optional<uint64_t> size;
        decoder >> size;
        if (!size)
            return std::nullopt;

        Vector<T, inlineCapacity, OverflowHandler, minCapacity> vector;
        for (size_t i = 0; i < *size; ++i) {
            std::optional<T> element;
            decoder >> element;
            if (!element)
                return std::nullopt;
            vector.append(WTFMove(*element));
        }
        vector.shrinkToFit();
        return vector;
    }
};

template<typename T> struct Coder<T, typename std::enable_if_t<std::is_enum_v<T>>> : public IPC::ArgumentCoder<T> { };
template<typename T> struct Coder<std::optional<T>> : public IPC::ArgumentCoder<std::optional<T>> { };
template<typename ValueType, typename ErrorType> struct Coder<Expected<ValueType, ErrorType>> : IPC::ArgumentCoder<Expected<ValueType, ErrorType>> { };
template<typename... Elements> struct Coder<std::tuple<Elements...>> : public IPC::ArgumentCoder<std::tuple<Elements...>> { };

#define DECLARE_CODER(class) \
template<> struct Coder<class> { \
    static void encode(Encoder&, const class&); \
    static std::optional<class> decode(Decoder&); \
}

DECLARE_CODER(WebCore::CertificateInfo);
DECLARE_CODER(WebCore::ExceptionData);
DECLARE_CODER(WebCore::PrivateClickMeasurement);
DECLARE_CODER(WebCore::PCM::AttributionTriggerData);
DECLARE_CODER(WebCore::PushSubscriptionData);
DECLARE_CODER(WebCore::PushSubscriptionIdentifier);
DECLARE_CODER(WebCore::RegistrableDomain);
DECLARE_CODER(WebCore::SecurityOriginData);
DECLARE_CODER(WebKit::WebPushMessage);
DECLARE_CODER(WebPushD::WebPushDaemonConnectionConfiguration);
DECLARE_CODER(WTF::WallTime);

#undef DECLARE_CODER

template<> struct Coder<WebPushD::PushMessageForTesting> {
    template<typename Encoder>
    static void encode(Encoder& encoder, const WebPushD::PushMessageForTesting& instance) { instance.encode(encoder); }
    template<typename Decoder>
    static std::optional<WebPushD::PushMessageForTesting> decode(Decoder& decoder) { return WebPushD::PushMessageForTesting::decode(decoder); }
};

template<> struct Coder<WTF::URL> {
    template<typename Encoder>
    static void encode(Encoder& encoder, const WTF::URL& instance)
    {
        encoder << instance.string();
    }
    template<typename Decoder>
    static std::optional<WTF::URL> decode(Decoder& decoder)
    {
        std::optional<String> string;
        decoder >> string;
        if (!string)
            return std::nullopt;
        return { WTF::URL(WTFMove(*string)) };
    }
};

template<> struct Coder<WTF::UUID> {
    template<typename Encoder>
    static void encode(Encoder& encoder, const WTF::UUID& instance)
    {
        instance.encode(encoder);
    }
    template<typename Decoder>
    static std::optional<WTF::UUID> decode(Decoder& decoder)
    {
        return UUID::decode(decoder);
    }
};

template<> struct Coder<WTF::String> {
    template<typename Encoder>
    static void encode(Encoder& encoder, const WTF::String& string)
    {
        // Special case the null string.
        if (string.isNull()) {
            encoder << std::numeric_limits<uint32_t>::max();
            return;
        }

        uint32_t length = string.length();
        bool is8Bit = string.is8Bit();

        encoder << length << is8Bit;

        if (is8Bit)
            encoder.encodeFixedLengthData({ string.characters8(), length * sizeof(LChar) });
        else
            encoder.encodeFixedLengthData({ reinterpret_cast<const uint8_t*>(string.characters16()), length * sizeof(UChar) });
    }

    template<typename CharacterType, typename Decoder>
    static std::optional<String> decodeStringText(Decoder& decoder, uint32_t length)
    {
        // Before allocating the string, make sure that the decoder buffer is big enough.
        if (!decoder.template bufferIsLargeEnoughToContain<CharacterType>(length))
            return std::nullopt;

        CharacterType* buffer;
        String string = String::createUninitialized(length, buffer);
        if (!decoder.decodeFixedLengthData(reinterpret_cast<uint8_t*>(buffer), length * sizeof(CharacterType)))
            return std::nullopt;

        return string;
    }

    template<typename Decoder>
    static std::optional<WTF::String> decode(Decoder& decoder)
    {
        std::optional<uint32_t> length;
        decoder >> length;
        if (!length)
            return std::nullopt;

        if (*length == std::numeric_limits<uint32_t>::max()) {
            // This is the null string.
            return String();
        }

        std::optional<bool> is8Bit;
        decoder >> is8Bit;
        if (!is8Bit)
            return std::nullopt;

        if (*is8Bit)
            return decodeStringText<LChar>(decoder, *length);
        return decodeStringText<UChar>(decoder, *length);
    }
};

} // namespace Daemon
} // namespace WebKit
