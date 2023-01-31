/*
 * Copyright (C) 2010-2021 Apple Inc. All rights reserved.
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

#include "ArrayReference.h"
#include "ArrayReferenceTuple.h"
#include "Decoder.h"
#include "Encoder.h"
#include "GeneratedSerializers.h"
#include <utility>
#include <variant>
#include <wtf/ArgumentCoder.h>
#include <wtf/Box.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/Expected.h>
#include <wtf/Forward.h>
#include <wtf/MonotonicTime.h>
#include <wtf/OptionSet.h>
#include <wtf/SHA1.h>
#include <wtf/Unexpected.h>
#include <wtf/WallTime.h>

#if OS(DARWIN)
#include "ArgumentCodersDarwin.h"
#endif
#if OS(WINDOWS)
#include "ArgumentCodersWin.h"
#endif
#if USE(UNIX_DOMAIN_SOCKETS)
#include "ArgumentCodersUnix.h"
#endif

namespace IPC {

// An argument coder works on POD types
template<typename T> struct SimpleArgumentCoder {
    static_assert(std::is_trivially_copyable_v<T>);

    template<typename Encoder>
    static void encode(Encoder& encoder, const T& t)
    {
        encoder.encodeObject(t);
    }

    static std::optional<T> decode(Decoder& decoder)
    {
        return decoder.decodeObject<T>();
    }
};

template<typename T, size_t Extent> struct ArgumentCoder<Span<T, Extent>> {
    template<typename Encoder>
    static void encode(Encoder& encoder, const Span<T, Extent>& span)
    {
        static_assert(Extent, "Can't encode a fixed size of 0");

        if constexpr (Extent == WTF::dynamic_extent) {
            size_t size = span.size();
            encoder << size;
            if (!size)
                return;
        }
        encoder.encodeSpan(span);
    }

    template<typename Decoder>
    static std::optional<Span<T, Extent>> decode(Decoder& decoder)
    {
        static_assert(Extent, "Can't decode a fixed size of 0");

        size_t size = Extent;
        if constexpr (Extent == WTF::dynamic_extent) {
            auto decodedSize = decoder.template decode<size_t>();
            if (!decodedSize)
                return std::nullopt;

            size = *decodedSize;
            if (!size)
                return Span<T, Extent> { };
        }

        auto data = decoder.template decodeSpan<T>(size);
        if (!data.data() || data.size() != size)
            return std::nullopt;

        if constexpr (Extent == WTF::dynamic_extent)
            return data;
        else
            return Span<T, Extent> { data.data(), Extent };
    }
};

template<typename... Types>
struct ArgumentCoder<ArrayReferenceTuple<Types...>> {
    template<typename Encoder>
    static void encode(Encoder& encoder, const ArrayReferenceTuple<Types...>& arrayReference)
    {
        encode(encoder, arrayReference, std::index_sequence_for<Types...> { });
    }

    template<typename Encoder, size_t... Indices>
    static void encode(Encoder& encoder, const ArrayReferenceTuple<Types...>& arrayReference, std::index_sequence<Indices...>)
    {
        size_t size = arrayReference.size();
        encoder << size;
        if (UNLIKELY(!size))
            return;

        (..., encoder.encodeSpan(Span { arrayReference.template data<Indices>(), size }));
    }

    template<typename Decoder>
    static std::optional<ArrayReferenceTuple<Types...>> decode(Decoder& decoder)
    {
        auto decodedSize = decoder.template decode<size_t>();
        if (UNLIKELY(!decodedSize))
            return std::nullopt;
        if (UNLIKELY(!*decodedSize))
            return ArrayReferenceTuple<Types...> { };

        CheckedSize size { *decodedSize };
        bool anyOverflow = (... || (size * sizeof(Types)).hasOverflowed());
        if (UNLIKELY(anyOverflow))
            return std::nullopt;

        return decode(decoder, size);
    }

    template<typename Decoder, typename... DataPointerTypes>
    static std::optional<ArrayReferenceTuple<Types...>> decode(Decoder& decoder, size_t size, DataPointerTypes&&... dataPointers)
    {
        constexpr size_t Index = sizeof...(DataPointerTypes);
        static_assert(Index <= sizeof...(Types));

        if constexpr (Index < sizeof...(Types)) {
            using ElementType = std::tuple_element_t<Index, std::tuple<Types...>>;
            auto data = decoder.template decodeSpan<ElementType>(size);
            if (!data.data())
                return std::nullopt;
            return decode(decoder, size, std::forward<DataPointerTypes>(dataPointers)..., data.data());
        } else
            return ArrayReferenceTuple<Types...> { std::forward<DataPointerTypes>(dataPointers)..., size };
    }
};

template<typename T> struct ArgumentCoder<OptionSet<T>> {
    template<typename Encoder>
    static void encode(Encoder& encoder, const OptionSet<T>& optionSet)
    {
        ASSERT(WTF::isValidOptionSet(optionSet));
        encoder << optionSet.toRaw();
    }

    template<typename Decoder>
    static std::optional<OptionSet<T>> decode(Decoder& decoder)
    {
        auto value = decoder.template decode<typename OptionSet<T>::StorageType>();
        if (!value)
            return std::nullopt;
        auto optionSet = OptionSet<T>::fromRaw(*value);
        if (!WTF::isValidOptionSet(optionSet))
            return std::nullopt;
        return optionSet;
    }
};

template<typename T> struct ArgumentCoder<std::optional<T>> {
    template<typename Encoder, typename U>
    static void encode(Encoder& encoder, U&& optional)
    {
        static_assert(std::is_same_v<std::remove_cvref_t<U>, std::optional<T>>);

        if (!optional) {
            encoder << false;
            return;
        }

        encoder << true;
        encoder << std::forward<U>(optional).value();
    }

    template<typename Decoder>
    static std::optional<std::optional<T>> decode(Decoder& decoder)
    {
        auto isEngaged = decoder.template decode<bool>();
        if (!isEngaged)
            return std::nullopt;
        if (*isEngaged) {
            auto value = decoder.template decode<T>();
            if (!value)
                return std::nullopt;
            return std::optional<std::optional<T>>(WTFMove(*value));
        }
        return std::optional<std::optional<T>>(std::optional<T>(std::nullopt));
    }
};

template<typename T> struct ArgumentCoder<Box<T>> {
    template<typename Encoder>
    static void encode(Encoder& encoder, const Box<T>& box)
    {
        if (!box) {
            encoder << false;
            return;
        }

        encoder << true;
        encoder << *box.get();
    }

    template<typename Decoder>
    static std::optional<Box<T>> decode(Decoder& decoder)
    {
        auto isEngaged = decoder.template decode<bool>();
        if (!isEngaged)
            return std::nullopt;
        if (*isEngaged) {
            auto value = decoder.template decode<T>();
            if (!value)
                return std::nullopt;
            return std::optional<Box<T>>(Box<T>::create(WTFMove(*value)));
        }
        return std::optional<Box<T>>(Box<T>(nullptr));
    }
};

template<typename T, typename U> struct ArgumentCoder<std::pair<T, U>> {
    template<typename Encoder, typename V>
    static void encode(Encoder& encoder, V&& pair)
    {
        static_assert(std::is_same_v<std::remove_cvref_t<V>, std::pair<T, U>>);
        encoder << std::get<0>(std::forward<V>(pair)) << std::get<1>(std::forward<V>(pair));
    }

    template<typename Decoder>
    static std::optional<std::pair<T, U>> decode(Decoder& decoder)
    {
        auto first = decoder.template decode<T>();
        if (!first)
            return std::nullopt;

        auto second = decoder.template decode<U>();
        if (!second)
            return std::nullopt;

        return std::make_optional<std::pair<T, U>>(WTFMove(*first), WTFMove(*second));
    }
};

template<typename T> struct ArgumentCoder<RefPtr<T>> {
    template<typename Encoder, typename U = T>
    static void encode(Encoder& encoder, const RefPtr<U>& object)
    {
        if (object)
            encoder << true << *object;
        else
            encoder << false;
    }

    template<typename Decoder, typename U = T>
    static std::optional<RefPtr<U>> decode(Decoder& decoder)
    {
        auto hasObject = decoder.template decode<bool>();
        if (!hasObject)
            return std::nullopt;
        if (!*hasObject)
            return RefPtr<U> { };
        // Decoders of U held with RefPtr do not return std::optional<U> but
        // std::optional<RefPtr<U>>. We cannot use `decoder.template decode<U>()`
        // Currently expect "modern decoder" -like decode function.
        return ArgumentCoder<U>::decode(decoder);
    }
};

template<typename T> struct ArgumentCoder<Ref<T>> {
    template<typename Encoder, typename U = T>
    static void encode(Encoder& encoder, const Ref<U>& object)
    {
        encoder << object.get();
    }

    template<typename Decoder, typename U = T>
    static std::optional<Ref<U>> decode(Decoder& decoder)
    {
        // Decoders of U held with Ref do not return std::optional<U> but
        // std::optional<Ref<U>>.
        // We cannot use `decoder.template decode<U>()`
        // Currently expect "modern decoder" -like decode function.
        return ArgumentCoder<U>::decode(decoder);
    }
};

template<typename T> struct ArgumentCoder<std::unique_ptr<T>> {
    template<typename Encoder, typename U>
    static void encode(Encoder& encoder, U&& object)
    {
        static_assert(std::is_same_v<std::remove_cvref_t<U>, std::unique_ptr<T>>);

        if (object)
            encoder << true << std::forward_like<U>(*object);
        else
            encoder << false;
    }

    template<typename Decoder>
    static std::optional<std::unique_ptr<T>> decode(Decoder& decoder)
    {
        auto isEngaged = decoder.template decode<bool>();
        if (!isEngaged)
            return std::nullopt;

        if (*isEngaged) {
            auto object = decoder.template decode<T>();
            if (!object)
                return std::nullopt;
            return std::make_optional<std::unique_ptr<T>>(makeUnique<T>(WTFMove(*object)));
        }
        return std::make_optional<std::unique_ptr<T>>();
    }
};

template<typename... Elements> struct ArgumentCoder<std::tuple<Elements...>> {
    template<typename Encoder, typename T>
    static void encode(Encoder& encoder, T&& tuple)
    {
        static_assert(std::is_same_v<std::remove_cvref_t<T>, std::tuple<Elements...>>);
        encode(encoder, std::forward<T>(tuple), std::index_sequence_for<Elements...> { });
    }

    template<typename Encoder, typename T, size_t... Indices>
    static void encode(Encoder& encoder, T&& tuple, std::index_sequence<Indices...>)
    {
        if constexpr (sizeof...(Indices) > 0)
            (encoder << ... << std::get<Indices>(std::forward<T>(tuple)));
    }

    template<typename Decoder, typename... DecodedTypes>
    static std::optional<std::tuple<Elements...>> decode(Decoder& decoder, std::optional<DecodedTypes>&&... decodedObjects)
    {
        constexpr size_t index = sizeof...(DecodedTypes);
        static_assert(index <= sizeof...(Elements));
        constexpr bool shouldHandleElement = index < sizeof...(Elements); // MSVC++ workaround (https://webkit.org/b/247226)

        if constexpr (shouldHandleElement) {
            auto optional = decoder.template decode<std::tuple_element_t<index, std::tuple<Elements...>>>();
            if (!optional)
                return std::nullopt;
            return decode(decoder, WTFMove(decodedObjects)..., WTFMove(optional));
        } else {
            static_assert((std::is_same_v<DecodedTypes, Elements> && ...));
            return std::make_optional<std::tuple<Elements...>>(*WTFMove(decodedObjects)...);
        }
    }
};

template<typename KeyType, typename ValueType> struct ArgumentCoder<WTF::KeyValuePair<KeyType, ValueType>> {
    template<typename Encoder, typename T>
    static void encode(Encoder& encoder, T&& pair)
    {
        static_assert(std::is_same_v<std::remove_cvref_t<T>, WTF::KeyValuePair<KeyType, ValueType>>);
        encoder << std::forward_like<T>(pair.key) << std::forward_like<T>(pair.value);
    }

    template<typename Decoder>
    static std::optional<WTF::KeyValuePair<KeyType, ValueType>> decode(Decoder& decoder)
    {
        auto key = decoder.template decode<KeyType>();
        if (!key)
            return std::nullopt;

        auto value = decoder.template decode<ValueType>();
        if (!value)
            return std::nullopt;

        return std::make_optional<WTF::KeyValuePair<KeyType, ValueType>>(WTFMove(*key), WTFMove(*value));
    }
};

template<typename T, size_t size> struct ArgumentCoder<std::array<T, size>> {
    template<typename Encoder, typename U>
    static void encode(Encoder& encoder, U&& array)
    {
        static_assert(std::is_same_v<std::remove_cvref_t<U>, std::array<T, size>>);

        for (auto&& item : array)
            encoder << std::forward_like<U>(item);
    }

    template<typename Decoder, typename... DecodedTypes>
    static std::optional<std::array<T, size>> decode(Decoder& decoder, std::optional<DecodedTypes>&&... decodedObjects)
    {
        constexpr size_t index = sizeof...(DecodedTypes);
        static_assert(index <= size);
        constexpr bool shouldHandleElement = index < size; // MSVC++ workaround (https://webkit.org/b/247226)

        if constexpr (shouldHandleElement) {
            auto optional = decoder.template decode<T>();
            if (!optional)
                return std::nullopt;
            return decode(decoder, WTFMove(decodedObjects)..., WTFMove(optional));
        } else {
            static_assert((std::is_same_v<DecodedTypes, T> && ...));
            return std::array<T, size> { *WTFMove(decodedObjects)... };
        }
    }
};

template<typename Key, typename T, Key lastValue> struct ArgumentCoder<EnumeratedArray<Key, T, lastValue>> {
    template<typename Encoder>
    static void encode(Encoder& encoder, const EnumeratedArray<Key, T, lastValue>& array)
    {
        for (auto& item : array)
            encoder << item;
    }

    template<typename Decoder>
    static std::optional<EnumeratedArray<Key, T, lastValue>> decode(Decoder& decoder)
    {
        auto array = decoder.template decode<typename EnumeratedArray<Key, T, lastValue>::UnderlyingType>();
        if (!array)
            return std::nullopt;

        return std::make_optional<EnumeratedArray<Key, T, lastValue>>(WTFMove(*array));
    }
};

template<bool fixedSizeElements, typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity> struct VectorArgumentCoder;

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity> struct VectorArgumentCoder<false, T, inlineCapacity, OverflowHandler, minCapacity> {
    template<typename Encoder, typename U>
    static void encode(Encoder& encoder, U&& vector)
    {
        static_assert(std::is_same_v<std::remove_cvref_t<U>, Vector<T, inlineCapacity, OverflowHandler, minCapacity>>);

        encoder << static_cast<size_t>(vector.size());
        for (auto&& item : vector)
            encoder << std::forward_like<U>(item);
    }

    template<typename Decoder>
    static std::optional<Vector<T, inlineCapacity, OverflowHandler, minCapacity>> decode(Decoder& decoder)
    {
        auto size = decoder.template decode<size_t>();
        if (!size)
            return std::nullopt;

        Vector<T, inlineCapacity, OverflowHandler, minCapacity> vector;
        vector.reserveInitialCapacity(*size);
        for (size_t i = 0; i < *size; ++i) {
            auto element = decoder.template decode<T>();
            if (!element)
                return std::nullopt;
            vector.uncheckedAppend(WTFMove(*element));
        }
        return vector;
    }
};

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity> struct VectorArgumentCoder<true, T, inlineCapacity, OverflowHandler, minCapacity> {
    template<typename Encoder>
    static void encode(Encoder& encoder, const Vector<T, inlineCapacity, OverflowHandler, minCapacity>& vector)
    {
        encoder << vector.span();
    }

    template<typename Decoder>
    static std::optional<Vector<T, inlineCapacity, OverflowHandler, minCapacity>> decode(Decoder& decoder)
    {
        auto data = decoder.template decode<Span<const T>>();
        if (!data)
            return std::nullopt;
        return std::make_optional<Vector<T, inlineCapacity, OverflowHandler, minCapacity>>(*data);
    }
};

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity> struct ArgumentCoder<Vector<T, inlineCapacity, OverflowHandler, minCapacity>> : VectorArgumentCoder<std::is_arithmetic<T>::value, T, inlineCapacity, OverflowHandler, minCapacity> { };

template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg, typename HashTableTraits> struct ArgumentCoder<HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg, HashTableTraits>> {
    typedef HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg, HashTableTraits> HashMapType;

    template<typename Encoder, typename T>
    static void encode(Encoder& encoder, T&& hashMap)
    {
        static_assert(std::is_same_v<std::remove_cvref_t<T>, HashMapType>);

        encoder << static_cast<unsigned>(hashMap.size());
        for (auto&& entry : hashMap)
            encoder << std::forward_like<T>(entry);
    }

    template<typename Decoder>
    static std::optional<HashMapType> decode(Decoder& decoder)
    {
        unsigned hashMapSize;
        if (!decoder.decode(hashMapSize))
            return std::nullopt;

        HashMapType hashMap;
        for (unsigned i = 0; i < hashMapSize; ++i) {
            auto key = decoder.template decode<KeyArg>();
            if (UNLIKELY(!key))
                return std::nullopt;

            auto value = decoder.template decode<MappedArg>();
            if (UNLIKELY(!value))
                return std::nullopt;

            if (UNLIKELY(!HashMapType::isValidKey(*key)))
                return std::nullopt;

            if (UNLIKELY(!hashMap.add(WTFMove(*key), WTFMove(*value)).isNewEntry)) {
                // The hash map already has the specified key, bail.
                return std::nullopt;
            }
        }

        return hashMap;
    }
};

template<typename KeyArg, typename HashArg, typename KeyTraitsArg, typename HashTableTraits> struct ArgumentCoder<HashSet<KeyArg, HashArg, KeyTraitsArg, HashTableTraits>> {
    typedef HashSet<KeyArg, HashArg, KeyTraitsArg, HashTableTraits> HashSetType;

    template<typename Encoder>
    static void encode(Encoder& encoder, const HashSetType& hashSet)
    {
        encoder << static_cast<unsigned>(hashSet.size());
        for (typename HashSetType::const_iterator it = hashSet.begin(), end = hashSet.end(); it != end; ++it)
            encoder << *it;
    }

    template<typename Decoder>
    static std::optional<HashSetType> decode(Decoder& decoder)
    {
        unsigned hashSetSize;
        if (!decoder.decode(hashSetSize))
            return std::nullopt;

        HashSetType hashSet;
        for (unsigned i = 0; i < hashSetSize; ++i) {
            auto key = decoder.template decode<KeyArg>();
            if (!key)
                return std::nullopt;

            if (UNLIKELY(!HashSetType::isValidValue(*key)))
                return std::nullopt;

            if (UNLIKELY(!hashSet.add(WTFMove(*key)).isNewEntry)) {
                // The hash set already has the specified key, bail.
                return std::nullopt;
            }
        }

        return hashSet;
    }
};

template<typename KeyArg, typename HashArg, typename KeyTraitsArg> struct ArgumentCoder<HashCountedSet<KeyArg, HashArg, KeyTraitsArg>> {
    typedef HashCountedSet<KeyArg, HashArg, KeyTraitsArg> HashCountedSetType;

    template<typename Encoder, typename T>
    static void encode(Encoder& encoder, T&& hashCountedSet)
    {
        static_assert(std::is_same_v<std::remove_cvref_t<T>, HashCountedSetType>);

        encoder << static_cast<unsigned>(hashCountedSet.size());
        for (auto&& entry : hashCountedSet)
            encoder << std::forward_like<T>(entry);
    }

    template<typename Decoder>
    static std::optional<HashCountedSetType> decode(Decoder& decoder)
    {
        unsigned hashCountedSetSize;
        if (!decoder.decode(hashCountedSetSize))
            return false;

        HashCountedSetType tempHashCountedSet;
        for (unsigned i = 0; i < hashCountedSetSize; ++i) {
            KeyArg key;
            if (!decoder.decode(key))
                return std::nullopt;

            unsigned count;
            if (!decoder.decode(count))
                return std::nullopt;

            if (UNLIKELY(!HashCountedSetType::isValidValue(key)))
                return std::nullopt;

            if (UNLIKELY(!tempHashCountedSet.add(key, count).isNewEntry)) {
                // The hash counted set already has the specified key, bail.
                return std::nullopt;
            }
        }

        return WTFMove(tempHashCountedSet);
    }
};

template<typename ValueType, typename ErrorType> struct ArgumentCoder<Expected<ValueType, ErrorType>> {
    template<typename Encoder>
    static void encode(Encoder& encoder, const Expected<ValueType, ErrorType>& expected)
    {
        if (!expected.has_value()) {
            encoder << false;
            encoder << expected.error();
            return;
        }
        encoder << true;
        encoder << expected.value();
    }

    template<typename Decoder>
    static std::optional<Expected<ValueType, ErrorType>> decode(Decoder& decoder)
    {
        auto hasValue = decoder.template decode<bool>();
        if (!hasValue)
            return std::nullopt;

        if (*hasValue) {
            auto value = decoder.template decode<ValueType>();
            if (!value)
                return std::nullopt;

            return std::make_optional<Expected<ValueType, ErrorType>>(WTFMove(*value));
        }

        auto error = decoder.template decode<ErrorType>();
        if (!error)
            return std::nullopt;
        return std::make_optional<Expected<ValueType, ErrorType>>(makeUnexpected(WTFMove(*error)));
    }
};

template<typename ErrorType> struct ArgumentCoder<Expected<void, ErrorType>> {
    template<typename Encoder> static void encode(Encoder& encoder, const Expected<void, ErrorType>& expected)
    {
        if (!expected.has_value()) {
            encoder << false;
            encoder << expected.error();
            return;
        }

        encoder << true;
    }

    template<typename Decoder> static std::optional<Expected<void, ErrorType>> decode(Decoder& decoder)
    {
        auto hasValue = decoder.template decode<bool>();
        if (!hasValue)
            return std::nullopt;

        if (*hasValue)
            return std::make_optional<Expected<void, ErrorType>>();

        auto error = decoder.template decode<ErrorType>();
        if (!error)
            return std::nullopt;
        return std::make_optional<Expected<void, ErrorType>>(makeUnexpected(WTFMove(*error)));
    }
};

template<typename... Types> struct ArgumentCoder<std::variant<Types...>> {
    template<typename Encoder, typename T>
    static void encode(Encoder& encoder, T&& variant)
    {
        static_assert(std::is_same_v<std::remove_cvref_t<T>, std::variant<Types...>>);

        size_t i = variant.index();
        encoder << i;
        encode(encoder, std::forward<T>(variant), std::index_sequence<> { }, i);
    }

    template<typename Encoder, typename T, size_t... Indices>
    static void encode(Encoder& encoder, T&& variant, std::index_sequence<Indices...>, size_t i)
    {
        constexpr size_t index = sizeof...(Indices);
        if constexpr (index < sizeof...(Types)) {
            if (index == i) {
                encoder << std::get<index>(std::forward<T>(variant));
                return;
            }
            encode(encoder, std::forward<T>(variant), std::make_index_sequence<index + 1> { }, i);
        }
    }

    template<typename Decoder>
    static std::optional<std::variant<Types...>> decode(Decoder& decoder)
    {
        auto i = decoder.template decode<size_t>();
        if (!i || *i >= sizeof...(Types))
            return std::nullopt;
        return decode(decoder, std::index_sequence<> { }, *i);
    }

    template<typename Decoder, size_t... Indices>
    static std::optional<std::variant<Types...>> decode(Decoder& decoder, std::index_sequence<Indices...>, size_t i)
    {
        constexpr size_t index = sizeof...(Indices);
        if constexpr (index < sizeof...(Types)) {
            if (index == i) {
                auto optional = decoder.template decode<typename std::variant_alternative_t<index, std::variant<Types...>>>();
                if (!optional)
                    return std::nullopt;
                return std::make_optional<std::variant<Types...>>(WTFMove(*optional));
            }
            return decode(decoder, std::make_index_sequence<index + 1> { }, i);
        } else
            return std::nullopt;
    }
};

template<> struct ArgumentCoder<CString> {
    template<typename Encoder>
    static void encode(Encoder&, const CString&);
    template<typename Decoder>
    static std::optional<CString> decode(Decoder&);
};

template<> struct ArgumentCoder<String> {
    template<typename Encoder>
    static void encode(Encoder&, const String&);
    template<typename Decoder>
    static std::optional<String> decode(Decoder&);
};

template<> struct ArgumentCoder<StringView> {
    template<typename Encoder>
    static void encode(Encoder&, StringView);
};

template<> struct ArgumentCoder<SHA1::Digest> {
    static void encode(Encoder& encoder, const SHA1::Digest& digest)
    {
        encoder.encodeSpan(Span { digest.data(), digest.size() });
    }

    static std::optional<SHA1::Digest> decode(Decoder& decoder)
    {
        constexpr size_t size = std::tuple_size_v<SHA1::Digest>;
        auto data = decoder.template decodeSpan<uint8_t>(size);
        if (!data.data())
            return std::nullopt;

        SHA1::Digest digest;
        static_assert(sizeof(typename decltype(data)::element_type) == 1);
        memcpy(digest.data(), data.data(), data.size_bytes());
        return digest;
    }
};

template<> struct ArgumentCoder<std::monostate> {
    template<typename Encoder>
    static void encode(Encoder&, const std::monostate&) { }
    template<typename Decoder>
    static std::optional<std::monostate> decode(Decoder&) { return std::monostate { }; }
};

template<> struct ArgumentCoder<std::nullptr_t> {
    static void encode(Encoder&, const std::nullptr_t&) { }
    static std::optional<std::nullptr_t> decode(Decoder&) { return nullptr; }
};

} // namespace IPC
