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

namespace IPC {

// An argument coder works on POD types
template<typename T> struct SimpleArgumentCoder {
    static_assert(std::is_trivially_copyable_v<T>);

    template<typename Encoder>
    static void encode(Encoder& encoder, const T& t)
    {
        encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(&t), sizeof(T), alignof(T));
    }

    static WARN_UNUSED_RETURN bool decode(Decoder& decoder, T& t)
    {
        return decoder.decodeFixedLengthData(reinterpret_cast<uint8_t*>(&t), sizeof(T), alignof(T));
    }
};

template<typename T, size_t Extent> struct ArgumentCoder<Span<T, Extent>> {
    template<typename Encoder>
    static void encode(Encoder& encoder, const Span<T, Extent>& span)
    {
        if constexpr (Extent == WTF::dynamic_extent)
            encoder << static_cast<uint64_t>(span.size());
        encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(span.data()), span.size() * sizeof(T), alignof(T));
    }
    template<typename Decoder>
    static std::optional<Span<T, Extent>> decode(Decoder& decoder)
    {
        std::optional<uint64_t> size;
        if constexpr (Extent == WTF::dynamic_extent) {
            decoder >> size;
            if (!size)
                return std::nullopt;
            if (!*size)
                return Span<T, Extent>();
        } else {
            size = Extent;
            static_assert(Extent, "Can't decode a fixed size of 0");
        }

        auto dataSize = CheckedSize { *size } * sizeof(T);
        if (UNLIKELY(dataSize.hasOverflowed()))
            return std::nullopt;

        const uint8_t* data = decoder.decodeFixedLengthReference(dataSize, alignof(T));
        if (!data)
            return std::nullopt;
        return Span<T, Extent>(reinterpret_cast<const T*>(data), static_cast<size_t>(*size));
    }
};

template<typename... Types>
struct ArgumentCoder<ArrayReferenceTuple<Types...>> {
    template<size_t Indices>
    using ArrayReferenceTupleElementType = std::tuple_element_t<Indices, std::tuple<Types...>>;

    template<typename Encoder>
    static void encode(Encoder& encoder, const ArrayReferenceTuple<Types...>& arrayReference)
    {
        encode(encoder, arrayReference, std::index_sequence_for<Types...> { });
    }

    template<typename Encoder, size_t... Indices>
    static void encode(Encoder& encoder, const ArrayReferenceTuple<Types...>& arrayReference, std::index_sequence<Indices...>)
    {
        auto size = arrayReference.size();
        encoder << static_cast<uint64_t>(size);
        if (UNLIKELY(!size))
            return;

        (encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(arrayReference.template data<Indices>()),
            size * sizeof(ArrayReferenceTupleElementType<Indices>), alignof(ArrayReferenceTupleElementType<Indices>)) , ...);
    }

    template<typename Decoder>
    static std::optional<ArrayReferenceTuple<Types...>> decode(Decoder& decoder)
    {
        auto size = decoder.template decode<uint64_t>();
        if (UNLIKELY(!size))
            return std::nullopt;
        if (UNLIKELY(!*size))
            return ArrayReferenceTuple<Types...> { };

        return decode(decoder, *size);
    }

    template<typename Decoder, typename... DataPointerTypes>
    static std::optional<ArrayReferenceTuple<Types...>> decode(Decoder& decoder, uint64_t size, DataPointerTypes&&... dataPointers)
    {
        constexpr size_t Index = sizeof...(DataPointerTypes);
        static_assert(Index <= sizeof...(Types));

        if constexpr (Index < sizeof...(Types)) {
            using ElementType = ArrayReferenceTupleElementType<Index>;
            auto dataSize = CheckedSize { size } * sizeof(ElementType);
            if (UNLIKELY(dataSize.hasOverflowed()))
                return std::nullopt;
            const uint8_t* data = decoder.decodeFixedLengthReference(dataSize, alignof(ElementType));
            if (UNLIKELY(!data))
                return std::nullopt;
            return decode(decoder, size, std::forward<DataPointerTypes>(dataPointers)..., reinterpret_cast<const ElementType*>(data) );
        } else
            return ArrayReferenceTuple<Types...> { std::forward<DataPointerTypes>(dataPointers)..., static_cast<size_t>(size) };
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
        std::optional<typename OptionSet<T>::StorageType> value;
        decoder >> value;
        if (!value)
            return std::nullopt;
        auto optionSet = OptionSet<T>::fromRaw(*value);
        if (!WTF::isValidOptionSet(optionSet))
            return std::nullopt;
        return optionSet;
    }
};

template<typename T> struct ArgumentCoder<std::optional<T>> {
    template<typename Encoder> static void encode(Encoder& encoder, const std::optional<T>& optional)
    {
        if (!optional) {
            encoder << false;
            return;
        }

        encoder << true;
        encoder << optional.value();
    }
    template<typename Decoder> static std::optional<std::optional<T>> decode(Decoder& decoder)
    {
        std::optional<bool> isEngaged;
        decoder >> isEngaged;
        if (!isEngaged)
            return std::nullopt;
        if (*isEngaged) {
            std::optional<T> value;
            decoder >> value;
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
        std::optional<bool> isEngaged;
        decoder >> isEngaged;
        if (!isEngaged)
            return std::nullopt;
        if (*isEngaged) {
            std::optional<T> value;
            decoder >> value;
            if (!value)
                return std::nullopt;
            return std::optional<Box<T>>(Box<T>::create(WTFMove(*value)));
        }
        return std::optional<Box<T>>(Box<T>(nullptr));
    }
};

template<typename T, typename U> struct ArgumentCoder<std::pair<T, U>> {
    template<typename Encoder>
    static void encode(Encoder& encoder, const std::pair<T, U>& pair)
    {
        encoder << pair.first << pair.second;
    }

    template<typename Decoder>
    static std::optional<std::pair<T, U>> decode(Decoder& decoder)
    {
        std::optional<T> first;
        decoder >> first;
        if (!first)
            return std::nullopt;

        std::optional<U> second;
        decoder >> second;
        if (!second)
            return std::nullopt;

        return {{ WTFMove(*first), WTFMove(*second) }};
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

template<typename... Elements> struct ArgumentCoder<std::tuple<Elements...>> {
    template<typename Encoder>
    static void encode(Encoder& encoder, const std::tuple<Elements...>& tuple)
    {
        encode(encoder, tuple, std::index_sequence_for<Elements...> { });
    }

    template<typename Encoder, size_t... Indices>
    static void encode(Encoder& encoder, const std::tuple<Elements...>& tuple, std::index_sequence<Indices...>)
    {
        if constexpr (sizeof...(Indices) > 0)
            (encoder << ... << std::get<Indices>(tuple));
    }

    template<typename Decoder>
    static std::optional<std::tuple<Elements...>> decode(Decoder& decoder)
    {
        return decode(decoder, std::tuple<> { }, std::index_sequence_for<> { });
    }

    template<typename Decoder, typename OptionalTuple, size_t... Indices>
    static std::optional<std::tuple<Elements...>> decode(Decoder& decoder, OptionalTuple&& optionalTuple, std::index_sequence<Indices...>)
    {
        constexpr size_t Index = sizeof...(Indices);
        static_assert(Index == std::tuple_size_v<OptionalTuple>);

        if constexpr (Index < sizeof...(Elements)) {
            auto optional = decoder.template decode<std::tuple_element_t<Index, std::tuple<Elements...>>>();
            if (!optional)
                return std::nullopt;
            return decode(decoder, std::forward_as_tuple(std::get<Indices>(WTFMove(optionalTuple))..., WTFMove(optional)), std::make_index_sequence<Index + 1> { });
        } else {
            static_assert(Index == sizeof...(Elements));
            return std::make_tuple(*std::get<Indices>(WTFMove(optionalTuple))...);
        }
    }
};

template<typename KeyType, typename ValueType> struct ArgumentCoder<WTF::KeyValuePair<KeyType, ValueType>> {
    template<typename Encoder>
    static void encode(Encoder& encoder, const WTF::KeyValuePair<KeyType, ValueType>& pair)
    {
        encoder << pair.key << pair.value;
    }

    template<typename Decoder>
    static std::optional<WTF::KeyValuePair<KeyType, ValueType>> decode(Decoder& decoder)
    {
        std::optional<KeyType> key;
        decoder >> key;
        if (!key)
            return std::nullopt;

        std::optional<ValueType> value;
        decoder >> value;
        if (!value)
            return std::nullopt;

        return { { WTFMove(*key), WTFMove(*value) } };
    }
};

template<typename T, size_t size> struct ArgumentCoder<std::array<T, size>> {
    template<typename Encoder>
    static void encode(Encoder& encoder, const std::array<T, size>& array)
    {
        for (auto& item : array)
            encoder << item;
    }

    template<typename Decoder>
    static std::optional<std::array<T, size>> decode(Decoder& decoder)
    {
        std::array<std::optional<T>, size> items;

        for (auto& item : items) {
            decoder >> item;
            if (!item)
                return std::nullopt;
        }

        return unwrapArray(WTFMove(items));
    }

private:
    static std::array<T, size> unwrapArray(std::array<std::optional<T>, size>&& array)
    {
        return unwrapArrayHelper(WTFMove(array), std::make_index_sequence<size>());
    }

    template<size_t... index>
    static std::array<T, size> unwrapArrayHelper(std::array<std::optional<T>, size>&& array, std::index_sequence<index...>)
    {
        return { WTFMove(*array[index])... };
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
        std::optional<typename EnumeratedArray<Key, T, lastValue>::UnderlyingType> array;
        decoder >> array;
        if (!array)
            return std::nullopt;

        return { WTFMove(*array) };
    }
};

template<bool fixedSizeElements, typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity> struct VectorArgumentCoder;

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity> struct VectorArgumentCoder<false, T, inlineCapacity, OverflowHandler, minCapacity> {
    template<typename Encoder>
    static void encode(Encoder& encoder, const Vector<T, inlineCapacity, OverflowHandler, minCapacity>& vector)
    {
        encoder << static_cast<uint64_t>(vector.size());
        for (size_t i = 0; i < vector.size(); ++i)
            encoder << vector[i];
    }

    template<typename Decoder>
    static std::optional<Vector<T, inlineCapacity, OverflowHandler, minCapacity>> decode(Decoder& decoder)
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

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity> struct VectorArgumentCoder<true, T, inlineCapacity, OverflowHandler, minCapacity> {
    template<typename Encoder>
    static void encode(Encoder& encoder, const Vector<T, inlineCapacity, OverflowHandler, minCapacity>& vector)
    {
        encoder << static_cast<uint64_t>(vector.size());
        encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(vector.data()), vector.size() * sizeof(T), alignof(T));
    }

    template<typename Decoder>
    static std::optional<Vector<T, inlineCapacity, OverflowHandler, minCapacity>> decode(Decoder& decoder)
    {
        std::optional<uint64_t> decodedSize;
        decoder >> decodedSize;
        if (!decodedSize)
            return std::nullopt;

        if (!isInBounds<size_t>(decodedSize))
            return std::nullopt;

        auto size = static_cast<size_t>(*decodedSize);

        // Since we know the total size of the elements, we can allocate the vector in
        // one fell swoop. Before allocating we must however make sure that the decoder buffer
        // is big enough.
        if (!decoder.template bufferIsLargeEnoughToContain<T>(size))
            return std::nullopt;

        Vector<T, inlineCapacity, OverflowHandler, minCapacity> vector;
        vector.grow(size);

        if (!decoder.decodeFixedLengthData(reinterpret_cast<uint8_t*>(vector.data()), size * sizeof(T), alignof(T)))
            return std::nullopt;

        return vector;
    }
};

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity> struct ArgumentCoder<Vector<T, inlineCapacity, OverflowHandler, minCapacity>> : VectorArgumentCoder<std::is_arithmetic<T>::value, T, inlineCapacity, OverflowHandler, minCapacity> { };

template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg, typename HashTableTraits> struct ArgumentCoder<HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg, HashTableTraits>> {
    typedef HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg, HashTableTraits> HashMapType;

    template<typename Encoder>
    static void encode(Encoder& encoder, const HashMapType& hashMap)
    {
        encoder << static_cast<uint32_t>(hashMap.size());
        for (typename HashMapType::const_iterator it = hashMap.begin(), end = hashMap.end(); it != end; ++it)
            encoder << *it;
    }

    template<typename Decoder>
    static std::optional<HashMapType> decode(Decoder& decoder)
    {
        uint32_t hashMapSize;
        if (!decoder.decode(hashMapSize))
            return std::nullopt;

        HashMapType hashMap;
        for (uint32_t i = 0; i < hashMapSize; ++i) {
            std::optional<KeyArg> key;
            decoder >> key;
            if (UNLIKELY(!key))
                return std::nullopt;

            std::optional<MappedArg> value;
            decoder >> value;
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
        encoder << static_cast<uint64_t>(hashSet.size());
        for (typename HashSetType::const_iterator it = hashSet.begin(), end = hashSet.end(); it != end; ++it)
            encoder << *it;
    }

    template<typename Decoder>
    static std::optional<HashSetType> decode(Decoder& decoder)
    {
        uint64_t hashSetSize;
        if (!decoder.decode(hashSetSize))
            return std::nullopt;

        HashSetType hashSet;
        for (uint64_t i = 0; i < hashSetSize; ++i) {
            std::optional<KeyArg> key;
            decoder >> key;
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

    template<typename Encoder>
    static void encode(Encoder& encoder, const HashCountedSetType& hashCountedSet)
    {
        encoder << static_cast<uint64_t>(hashCountedSet.size());

        for (auto entry : hashCountedSet) {
            encoder << entry.key;
            encoder << entry.value;
        }
    }

    template<typename Decoder>
    static std::optional<HashCountedSetType> decode(Decoder& decoder)
    {
        uint64_t hashCountedSetSize;
        if (!decoder.decode(hashCountedSetSize))
            return false;

        HashCountedSetType tempHashCountedSet;
        for (uint64_t i = 0; i < hashCountedSetSize; ++i) {
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
        std::optional<bool> hasValue;
        decoder >> hasValue;
        if (!hasValue)
            return std::nullopt;

        if (*hasValue) {
            std::optional<ValueType> value;
            decoder >> value;
            if (!value)
                return std::nullopt;

            Expected<ValueType, ErrorType> expected(WTFMove(*value));
            return expected;
        }
        std::optional<ErrorType> error;
        decoder >> error;
        if (!error)
            return std::nullopt;
        return { makeUnexpected(WTFMove(*error)) };
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
        std::optional<bool> hasValue;
        decoder >> hasValue;
        if (!hasValue)
            return std::nullopt;

        if (*hasValue)
            return {{ }};

        std::optional<ErrorType> error;
        decoder >> error;
        if (!error)
            return std::nullopt;
        return { makeUnexpected(WTFMove(*error)) };
    }
};

template<typename... Types> struct ArgumentCoder<std::variant<Types...>> {
    template<typename Encoder>
    static void encode(Encoder& encoder, const std::variant<Types...>& variant)
    {
        unsigned i = variant.index();
        encoder << i;
        encode(encoder, variant, std::index_sequence_for<Types...> { }, i);
    }

    template<typename Encoder, size_t... Indices>
    static void encode(Encoder& encoder, const std::variant<Types...>& variant, std::index_sequence<Indices...>, unsigned i)
    {
        constexpr size_t Index = sizeof...(Types) - sizeof...(Indices);
        static_assert(Index < sizeof...(Types));
        if (Index == i) {
            encoder << std::get<Index>(variant);
            return;
        }

        if constexpr (sizeof...(Indices) > 1)
            encode(encoder, variant, std::make_index_sequence<sizeof...(Indices) - 1> { }, i);
    }

    template<typename Decoder>
    static std::optional<std::variant<Types...>> decode(Decoder& decoder)
    {
        std::optional<unsigned> i;
        decoder >> i;
        if (!i || *i >= sizeof...(Types))
            return std::nullopt;
        return decode(decoder, std::index_sequence_for<Types...> { }, *i);
    }

    template<typename Decoder, size_t... Indices>
    static std::optional<std::variant<Types...>> decode(Decoder& decoder, std::index_sequence<Indices...>, unsigned i)
    {
        constexpr size_t Index = sizeof...(Types) - sizeof...(Indices);
        static_assert(Index < sizeof...(Types));
        if (Index == i) {
            std::optional<typename std::variant_alternative_t<Index, std::variant<Types...>>> optional;
            decoder >> optional;
            if (!optional)
                return std::nullopt;
            return { WTFMove(*optional) };
        }

        if constexpr (sizeof...(Indices) > 1)
            return decode(decoder, std::make_index_sequence<sizeof...(Indices) - 1> { }, i);
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
    static void encode(Encoder&, const SHA1::Digest&);
    static std::optional<SHA1::Digest> decode(Decoder&);
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
