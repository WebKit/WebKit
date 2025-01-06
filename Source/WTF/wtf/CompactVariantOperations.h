/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <algorithm>
#include <bit>
#include <concepts>
#include <functional>
#include <type_traits>
#include <variant>
#include <wtf/GetPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/VariantExtras.h>

namespace WTF {

template<typename T> struct CompactVariantTraits {
   static constexpr bool hasAlternativeRepresentation = false;

   /* 
       If `hasAlternativeRepresentation` is set to `true`, you must also implement the following functions.

           static constexpr uint64_t encodeFromArguments(...) { ... }
           static constexpr uint64_t encode(const T&) { ... }
           static constexpr uint64_t encode(T&&) { ... }
           static constexpr T decode(uint64_t) { ... }
   */
};

template<typename T> concept CompactVariantAlternativeSmallEnough = sizeof(T) <= 4;
template<typename T> concept CompactVariantAlternativePointer =
       std::is_pointer_v<T>
   ||  IsSmartPtr<T>::value;

template<typename T> concept  CompactVariantAlternative =
       CompactVariantAlternativePointer<T>
    || CompactVariantAlternativeSmallEnough<T>
    || CompactVariantTraits<T>::hasAlternativeRepresentation;

template<CompactVariantAlternative... Ts> struct CompactVariantOperations {
    using StdVariant = std::variant<Ts...>;
    using Index = uint8_t;
    using Storage = uint64_t;
    static constexpr Storage movedFromDataValue = std::numeric_limits<Storage>::max();
    static constexpr Storage totalSize = sizeof(Storage) * 8;
    static constexpr Storage indexSize = sizeof(Index) * 8;
    static constexpr Storage indexShift = totalSize - indexSize;
    static constexpr Storage payloadSize = totalSize - indexSize;
    static constexpr Storage payloadMask = (1ULL << payloadSize) - 1;
    static_assert(payloadSize + indexSize <= totalSize);

    static constexpr Storage encodedIndex(Index index)
    {
        return static_cast<Storage>(index) << indexShift;
    }

    static constexpr Index decodedIndex(Storage value)
    {
        return static_cast<Index>(static_cast<uint8_t>(value >> indexShift));
    }

    template<typename T, typename U> static constexpr Storage encodedPayload(U&& payload)
    {
        Storage data = 0;

        if constexpr (CompactVariantTraits<T>::hasAlternativeRepresentation)
            data = CompactVariantTraits<T>::encode(std::forward<U>(payload));
        else
            new (NotNull, &data) T(std::forward<U>(payload));

        return data;
    }

    template<typename T, typename... Args> static constexpr Storage encodedPayloadFromArguments(Args&&... arguments)
    {
        Storage data = 0;

        if constexpr (CompactVariantTraits<T>::hasAlternativeRepresentation)
            data = CompactVariantTraits<T>::encodeFromArguments(std::forward<Args>(arguments)...);
        else
            new (NotNull, &data) T(std::forward<Args>(arguments)...);

        return data;
    }

    template<typename T, typename F> static constexpr decltype(auto) decodedPayload(Storage value, NOESCAPE F&& f)
    {
        Storage maskedData = value & payloadMask;

        if constexpr (CompactVariantTraits<T>::hasAlternativeRepresentation) {
            T decodedData = CompactVariantTraits<T>::decode(maskedData);
            return f(decodedData);
        } else {
            T& decodedData = *std::launder(reinterpret_cast<T*>(&maskedData));
            return f(decodedData);
        }
    }

    template<typename T, typename F> static constexpr decltype(auto) decodedConstPayload(Storage value, NOESCAPE F&& f)
    {
        Storage maskedData = value & payloadMask;

        if constexpr (CompactVariantTraits<T>::hasAlternativeRepresentation) {
            T decodedData = CompactVariantTraits<T>::decode(maskedData);
            return f(std::as_const(decodedData));
        } else {
            T& decodedData = *std::launder(reinterpret_cast<T*>(&maskedData));
            return f(std::as_const(decodedData));
        }
    }

    template<typename T, typename U> static Storage encode(U&& argument)
    {
        return encodedPayload<T>(std::forward<U>(argument)) | encodedIndex(alternativeIndexV<T, StdVariant>);
    }

    template<typename T, typename... Args> static Storage encodeFromArguments(Args&&... arguments)
    {
        return encodedPayloadFromArguments<T>(std::forward<Args>(arguments)...) | encodedIndex(alternativeIndexV<T, StdVariant>);
    }

    template<typename... F> static decltype(auto) payloadForData(Storage data, F&&... f)
    {
        auto visitor = makeVisitor(std::forward<F>(f)...);
        return typeForIndex<StdVariant>(decodedIndex(data), [&]<typename T>() {
            return decodedPayload<T>(data, visitor);
        });
    }

    template<typename... F> static decltype(auto) constPayloadForData(Storage data, F&&... f)
    {
        auto visitor = makeVisitor(std::forward<F>(f)...);
        return typeForIndex<StdVariant>(decodedIndex(data), [&]<typename T>() {
            return decodedConstPayload<T>(data, visitor);
        });
    }

    static void destruct(Storage data)
    {
        if (data == movedFromDataValue)
            return;

        payloadForData(data, [&]<typename T>(T& value) {
            if constexpr (!std::is_trivially_destructible_v<T>)
                value.~T();
        });
    }

    static void copy(Storage& to, Storage from)
    {
        if (from == movedFromDataValue) {
            to = from;
            return;
        }

        payloadForData(from, [&]<typename T>(T& value) {
            to = encodedPayload<T>(value) | encodedIndex(alternativeIndexV<T, StdVariant>);
        });
    }

    static void move(Storage& to, Storage from)
    {
        if (from == movedFromDataValue) {
            to = from;
            return;
        }

        payloadForData(from, [&]<typename T>(T& value) {
            to = encodedPayload<T>(WTFMove(value)) | encodedIndex(alternativeIndexV<T, StdVariant>);
        });
    }

    template<typename T> static bool equal(Storage a, Storage b)
    {
        Storage maskedA = a & payloadMask;
        Storage maskedB = b & payloadMask;

        if constexpr (CompactVariantTraits<T>::hasAlternativeRepresentation)
            return CompactVariantTraits<T>::decode(maskedA) == CompactVariantTraits<T>::decode(maskedB);
        else
            return *std::launder(reinterpret_cast<T*>(&maskedA)) == *std::launder(reinterpret_cast<T*>(&maskedB));
    }
};

} // namespace WTF
