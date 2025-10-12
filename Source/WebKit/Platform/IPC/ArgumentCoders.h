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

#include "ArrayReferenceTuple.h"
#include "Decoder.h"
#include "Encoder.h"
#include "GeneratedSerializers.h"
#include <utility>
#include <wtf/ArgumentCoder.h>
#include <wtf/Box.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/Expected.h>
#include <wtf/Forward.h>
#include <wtf/MonotonicTime.h>
#include <wtf/OptionSet.h>
#include <wtf/SHA1.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Unexpected.h>
#include <wtf/WallTime.h>

#if OS(ANDROID)
#include "ArgumentCodersAndroid.h"
#endif
#if USE(GLIB)
#include "ArgumentCodersGlib.h"
#include "RendererBufferFormat.h"
#endif
#if USE(UNIX_DOMAIN_SOCKETS)
#include "ArgumentCodersUnix.h"
#endif

namespace IPC {

template<typename T, size_t Extent> struct ArgumentCoder<std::span<T, Extent>> {
    template<typename Encoder>
    static void encode(Encoder& encoder, std::span<T, Extent> span)
    {
        static_assert(Extent, "Can't encode a fixed size of 0");

        if constexpr (Extent == std::dynamic_extent) {
            uint64_t size = span.size();
            encoder << size;
            if (!size)
                return;
        }
        encoder.encodeSpan(span);
    }

    template<typename Decoder>
    static std::optional<std::span<T, Extent>> decode(Decoder& decoder)
    {
        static_assert(Extent, "Can't decode a fixed size of 0");

        size_t size = Extent;
        if constexpr (Extent == std::dynamic_extent) {
            auto decodedSize = decoder.template decode<uint64_t>();
            if (!decodedSize)
                return std::nullopt;

            if (*decodedSize > std::numeric_limits<size_t>::max())
                return std::nullopt;

            size = *decodedSize;
            if (!size)
                return std::span<T, Extent> { };
        }

        auto data = decoder.template decodeSpan<T>(size);
        if (!data.data() || data.size() != size)
            return std::nullopt;

        if constexpr (Extent == std::dynamic_extent)
            return data;
        else
            return data.template subspan<0, Extent>();
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
        uint64_t size = arrayReference.size();
        encoder << size;
        if (!size) [[unlikely]]
            return;

        (..., encoder.encodeSpan(arrayReference.template span<Indices>()));
    }

    template<typename Decoder>
    static std::optional<ArrayReferenceTuple<Types...>> decode(Decoder& decoder)
    {
        auto decodedSize = decoder.template decode<uint64_t>();
        if (!decodedSize) [[unlikely]]
            return std::nullopt;
        if (!*decodedSize) [[unlikely]]
            return ArrayReferenceTuple<Types...> { };

        CheckedSize size { *decodedSize };
        bool anyOverflow = (... || (size * sizeof(Types)).hasOverflowed());
        if (anyOverflow) [[unlikely]]
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
        SUPPRESS_UNCHECKED_ARG encoder << std::forward<U>(optional).value();
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
            SUPPRESS_UNCHECKED_ARG return std::optional<std::optional<T>>(WTFMove(*value));
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
        SUPPRESS_UNCHECKED_ARG encoder << std::get<0>(std::forward<V>(pair)) << std::get<1>(std::forward<V>(pair));
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

        SUPPRESS_UNCHECKED_ARG return std::make_optional<std::pair<T, U>>(WTFMove(*first), WTFMove(*second));
    }
};

template<typename T> struct ArgumentCoder<RefPtr<T>> {
    template<typename Encoder, typename U = T>
    static void encode(Encoder& encoder, const RefPtr<U>& object)
    {
        if (object)
            SUPPRESS_FORWARD_DECL_ARG encoder << true << *object;
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
        return ArgumentCoder<std::remove_const_t<U>>::decode(decoder);
    }
};

template<typename T> struct ArgumentCoder<Ref<T>> {
    template<typename Encoder, typename U = T>
    static void encode(Encoder& encoder, const Ref<U>& object)
    {
        SUPPRESS_FORWARD_DECL_ARG encoder << object.get();
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
            SUPPRESS_UNCHECKED_ARG encoder << true << WTF::forward_like<U>(*object);
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
            SUPPRESS_UNCHECKED_ARG return std::make_optional<std::unique_ptr<T>>(makeUnique<T>(WTFMove(*object)));
        }
        return std::make_optional<std::unique_ptr<T>>();
    }
};

template<typename T> struct ArgumentCoder<UniqueRef<T>> {
    template<typename Encoder, typename U>
    static void encode(Encoder& encoder, U&& object)
    {
        static_assert(std::is_same_v<std::remove_cvref_t<U>, UniqueRef<T>>);
        encoder << WTF::forward_like<U>(object.get());
    }

    template<typename Decoder>
    static std::optional<UniqueRef<T>> decode(Decoder& decoder)
    {
        auto object = decoder.template decode<T>();
        if (!object)
            return std::nullopt;
        return makeUniqueRef<T>(WTFMove(*object));
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
            SUPPRESS_UNCHECKED_ARG return std::make_optional<std::tuple<Elements...>>(*WTFMove(decodedObjects)...);
        }
    }
};

template<typename KeyType, typename ValueType> struct ArgumentCoder<WTF::KeyValuePair<KeyType, ValueType>> {
    template<typename Encoder, typename T>
    static void encode(Encoder& encoder, T&& pair)
    {
        static_assert(std::is_same_v<std::remove_cvref_t<T>, WTF::KeyValuePair<KeyType, ValueType>>);
        encoder << WTF::forward_like<T>(pair.key) << WTF::forward_like<T>(pair.value);
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
            encoder << WTF::forward_like<U>(item);
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

        encoder << static_cast<uint64_t>(vector.size());
        SUPPRESS_UNCHECKED_LOCAL for (auto&& item : vector)
            SUPPRESS_UNCHECKED_ARG encoder << WTF::forward_like<U>(item);
    }

    template<typename Decoder>
    static std::optional<Vector<T, inlineCapacity, OverflowHandler, minCapacity>> decode(Decoder& decoder)
    {
        auto size = decoder.template decode<uint64_t>();
        if (!size)
            return std::nullopt;

        Vector<T, inlineCapacity, OverflowHandler, minCapacity> vector;

        // Calls to reserveInitialCapacity with untrusted large sizes can cause allocator crashes.
        // Limit allocations from untrusted sources to 1MB.
        if (*size < 1024 * 1024 / sizeof(T)) [[likely]] {
            vector.reserveInitialCapacity(*size);
            for (size_t i = 0; i < *size; ++i) {
                auto element = decoder.template decode<T>();
                if (!element)
                    return std::nullopt;
                vector.append(WTFMove(*element));
            }
            return vector;
        }

        for (size_t i = 0; i < *size; ++i) {
            auto element = decoder.template decode<T>();
            if (!element)
                return std::nullopt;
            SUPPRESS_UNCHECKED_ARG vector.append(WTFMove(*element));
        }
        vector.shrinkToFit();
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
        auto data = decoder.template decode<std::span<const T>>();
        if (!data)
            return std::nullopt;
        return std::make_optional<Vector<T, inlineCapacity, OverflowHandler, minCapacity>>(*data);
    }
};

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity> struct ArgumentCoder<Vector<T, inlineCapacity, OverflowHandler, minCapacity>> : VectorArgumentCoder<std::is_arithmetic<T>::value, T, inlineCapacity, OverflowHandler, minCapacity> { };


template<bool fixedSizeElements, typename T> struct FixedVectorArgumentCoder;

template<typename T> struct FixedVectorArgumentCoder<false, T> {
    template<typename Encoder, typename U>
    static void encode(Encoder& encoder, U&& vector)
    {
        static_assert(std::is_same_v<std::remove_cvref_t<U>, FixedVector<T>>);

        encoder << static_cast<uint64_t>(vector.size());
        for (auto&& item : vector)
            encoder << WTF::forward_like<U>(item);
    }

    template<typename Decoder>
    static std::optional<FixedVector<T>> decode(Decoder& decoder)
    {
        auto size = decoder.template decode<uint64_t>();
        if (!size)
            return std::nullopt;

        // Limit direct allocations from untrusted sources to 1MB.
        if (*size < 1024 * 1024 / sizeof(T)) [[likely]] {
            auto vector = FixedVector<T>::createWithSizeFromFailableGenerator(*size, [&](auto i) {
                return decoder.template decode<T>();
            });
            if (vector.size() != *size)
                return std::nullopt;
            return vector;
        }

        Vector<T> mutableVector;
        for (size_t i = 0; i < *size; ++i) {
            auto element = decoder.template decode<T>();
            if (!element)
                return std::nullopt;
            mutableVector.append(WTFMove(*element));
        }
        return std::make_optional<FixedVector<T>>(WTFMove(mutableVector));
    }
};

template<typename T> struct FixedVectorArgumentCoder<true, T> {
    template<typename Encoder>
    static void encode(Encoder& encoder, const FixedVector<T>& vector)
    {
        encoder << vector.span();
    }

    template<typename Decoder>
    static std::optional<FixedVector<T>> decode(Decoder& decoder)
    {
        auto data = decoder.template decode<std::span<const T>>();
        if (!data)
            return std::nullopt;
        return std::make_optional<FixedVector<T>>(*data);
    }
};

template<typename T> struct ArgumentCoder<FixedVector<T>> : FixedVectorArgumentCoder<std::is_arithmetic<T>::value, T> { };

template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg, typename HashTableTraits> struct ArgumentCoder<HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg, HashTableTraits>> {
    typedef HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg, HashTableTraits> HashMapType;

    template<typename Encoder, typename T>
    static void encode(Encoder& encoder, T&& hashMap)
    {
        static_assert(std::is_same_v<std::remove_cvref_t<T>, HashMapType>);

        encoder << static_cast<unsigned>(hashMap.size());
        for (auto&& entry : hashMap)
            encoder << WTF::forward_like<T>(entry);
    }

    template<typename Decoder>
    static std::optional<HashMapType> decode(Decoder& decoder)
    {
        auto hashMapSize = decoder.template decode<unsigned>();
        if (!hashMapSize)
            return std::nullopt;

        HashMapType hashMap;
        for (unsigned i = 0; i < *hashMapSize; ++i) {
            auto key = decoder.template decode<KeyArg>();
            if (!key) [[unlikely]]
                return std::nullopt;

            auto value = decoder.template decode<MappedArg>();
            if (!value) [[unlikely]]
                return std::nullopt;

            if (!HashMapType::isValidKey(*key)) [[unlikely]]
                return std::nullopt;

            if (!hashMap.add(WTFMove(*key), WTFMove(*value)).isNewEntry) [[unlikely]] {
                // The hash map already has the specified key, bail.
                return std::nullopt;
            }
        }

        return hashMap;
    }
};

template<typename KeyArg, typename HashArg, typename KeyTraitsArg, typename HashTableTraits, WTF::ShouldValidateKey shouldValidateKey> struct ArgumentCoder<HashSet<KeyArg, HashArg, KeyTraitsArg, HashTableTraits, shouldValidateKey>> {
    typedef HashSet<KeyArg, HashArg, KeyTraitsArg, HashTableTraits, shouldValidateKey> HashSetType;

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
        auto hashSetSize = decoder.template decode<unsigned>();
        if (!hashSetSize)
            return std::nullopt;

        HashSetType hashSet;
        for (unsigned i = 0; i < *hashSetSize; ++i) {
            auto key = decoder.template decode<KeyArg>();
            if (!key)
                return std::nullopt;

            if (!HashSetType::isValidValue(*key)) [[unlikely]]
                return std::nullopt;

            if (!hashSet.add(WTFMove(*key)).isNewEntry) [[unlikely]] {
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
            encoder << WTF::forward_like<T>(entry);
    }

    template<typename Decoder>
    static std::optional<HashCountedSetType> decode(Decoder& decoder)
    {
        auto hashCountedSetSize = decoder.template decode<unsigned>();
        if (!hashCountedSetSize)
            return std::nullopt;

        HashCountedSetType tempHashCountedSet;
        for (unsigned i = 0; i < *hashCountedSetSize; ++i) {
            auto key = decoder.template decode<KeyArg>();
            if (!key)
                return std::nullopt;

            auto count = decoder.template decode<unsigned>();
            if (!count)
                return std::nullopt;

            if (!HashCountedSetType::isValidValue(*key)) [[unlikely]]
                return std::nullopt;

            if (!tempHashCountedSet.add(*key, *count).isNewEntry) [[unlikely]] {
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

    template<typename Encoder>
    static void encode(Encoder& encoder, Expected<ValueType, ErrorType>&& expected)
    {
        if (!expected.has_value()) {
            encoder << false;
            encoder << WTFMove(expected.error());
            return;
        }
        encoder << true;
        encoder << WTFMove(expected.value());
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

using EncodedVariantIndex = uint8_t;

template<typename... Types> struct ArgumentCoder<Variant<Types...>> {
    template<typename Encoder, typename T>
    static void encode(Encoder& encoder, T&& variant)
    {
        static_assert(std::is_same_v<std::remove_cvref_t<T>, Variant<Types...>>);
        static_assert(sizeof...(Types) <= static_cast<size_t>(std::numeric_limits<EncodedVariantIndex>::max()));

        EncodedVariantIndex i = variant.index();
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
    static std::optional<Variant<Types...>> decode(Decoder& decoder)
    {
        auto i = decoder.template decode<EncodedVariantIndex>();
        if (!i || *i >= sizeof...(Types))
            return std::nullopt;
        return decode(decoder, std::index_sequence<> { }, *i);
    }

    template<typename Decoder, size_t... Indices>
    static std::optional<Variant<Types...>> decode(Decoder& decoder, std::index_sequence<Indices...>, size_t i)
    {
        constexpr size_t index = sizeof...(Indices);
        if constexpr (index < sizeof...(Types)) {
            if (index == i) {
                auto optional = decoder.template decode<typename WTF::VariantAlternativeT<index, Variant<Types...>>>();
                if (!optional)
                    return std::nullopt;
                return std::make_optional<Variant<Types...>>(WTF::InPlaceIndex<index>, WTFMove(*optional));
            }
            return decode(decoder, std::make_index_sequence<index + 1> { }, i);
        } else
            return std::nullopt;
    }
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

template<> struct ArgumentCoder<std::nullptr_t> {
    template<typename Encoder>
    static void encode(Encoder&, const std::nullptr_t&) { }
    static std::optional<std::nullptr_t> decode(Decoder&) { return nullptr; }
};

template<typename T, typename Traits> struct ArgumentCoder<WTF::Markable<T, Traits>> {
    template<typename Encoder, typename U>
    static void encode(Encoder& encoder, U&& markable)
    {
        bool isEmpty = !markable;
        encoder << isEmpty;
        if (!isEmpty)
            encoder << *markable;
    }

    template<typename Decoder>
    static std::optional<WTF::Markable<T, Traits>> decode(Decoder& decoder)
    {
        auto isEmpty = decoder.template decode<bool>();
        if (!isEmpty) [[unlikely]]
            return std::nullopt;

        if (*isEmpty)
            return WTF::Markable<T, Traits> { };

        auto value = decoder.template decode<T>();
        if (!value) [[unlikely]]
            return std::nullopt;

        return WTF::Markable<T, Traits>(WTFMove(*value));
    }
};

} // namespace IPC
