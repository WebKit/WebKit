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

#include "ArgumentCoder.h"
#include "ArrayReference.h"
#include <utility>
#include <wtf/Box.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/Forward.h>
#include <wtf/MonotonicTime.h>
#include <wtf/OptionSet.h>
#include <wtf/SHA1.h>
#include <wtf/Unexpected.h>
#include <wtf/Variant.h>
#include <wtf/WallTime.h>

namespace IPC {

// An argument coder works on POD types
template<typename T> struct SimpleArgumentCoder {
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

template<typename T, size_t Extent> struct ArgumentCoder<ArrayReference<T, Extent>> {
    using ArrayReferenceType = ArrayReference<T, Extent>;
    template<typename Encoder>
    static void encode(Encoder& encoder, const ArrayReferenceType& arrayReference)
    {
        if (!Extent)
            return;
        encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(arrayReference.data()), arrayReference.size() * sizeof(T), alignof(T));
    }
    static std::optional<ArrayReferenceType> decode(Decoder& decoder)
    {
        if (!Extent)
            return ArrayReferenceType();
        const uint8_t* data = decoder.decodeFixedLengthReference(Extent * sizeof(T), alignof(T));
        if (!data)
            return std::nullopt;
        return ArrayReferenceType(reinterpret_cast<const T*>(data), Extent);
    }
};

template<typename T> struct ArgumentCoder<ArrayReference<T, arrayReferenceDynamicExtent>> {
    using ArrayReferenceType = ArrayReference<T, arrayReferenceDynamicExtent>;
    template<typename Encoder>
    static void encode(Encoder& encoder, const ArrayReferenceType& arrayReference)
    {
        encoder << static_cast<uint64_t>(arrayReference.size());
        if (!arrayReference.size())
            return;
        encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(arrayReference.data()), arrayReference.size() * sizeof(T), alignof(T));
    }
    static std::optional<ArrayReferenceType> decode(Decoder& decoder)
    {
        uint64_t size;
        if (!decoder.decode(size))
            return std::nullopt;
        if (!size)
            return ArrayReferenceType();

        auto dataSize = CheckedSize { size } * sizeof(T);
        if (UNLIKELY(dataSize.hasOverflowed()))
            return std::nullopt;

        const uint8_t* data = decoder.decodeFixedLengthReference(dataSize, alignof(T));
        if (!data)
            return std::nullopt;
        return ArrayReferenceType(reinterpret_cast<const T*>(data), static_cast<size_t>(size));
    }
};

template<typename T> struct ArgumentCoder<OptionSet<T>> {
    static void encode(Encoder& encoder, const OptionSet<T>& optionSet)
    {
        ASSERT(WTF::isValidOptionSet(optionSet));
        encoder << optionSet.toRaw();
    }

    static WARN_UNUSED_RETURN bool decode(Decoder& decoder, OptionSet<T>& optionSet)
    {
        typename OptionSet<T>::StorageType value;
        if (!decoder.decode(value))
            return false;
        optionSet = OptionSet<T>::fromRaw(value);
        if (!WTF::isValidOptionSet(optionSet))
            return false;
        return true;
    }

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

    template<typename Decoder> static WARN_UNUSED_RETURN bool decode(Decoder& decoder, std::optional<T>& optional)
    {
        bool isEngaged;
        if (!decoder.decode(isEngaged))
            return false;

        if (!isEngaged) {
            optional = std::nullopt;
            return true;
        }

        T value;
        if (!decoder.decode(value))
            return false;

        optional = WTFMove(value);
        return true;
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
    static void encode(Encoder& encoder, const Box<T>& box)
    {
        if (!box) {
            encoder << false;
            return;
        }

        encoder << true;
        encoder << *box.get();
    }

    static WARN_UNUSED_RETURN bool decode(Decoder& decoder, Box<T>& box)
    {
        bool isEngaged;
        if (!decoder.decode(isEngaged))
            return false;

        if (!isEngaged) {
            box = nullptr;
            return true;
        }

        Box<T> value = Box<T>::create();
        if (!decoder.decode(*value))
            return false;

        box = WTFMove(value);
        return true;
    }

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
    static void encode(Encoder& encoder, const std::pair<T, U>& pair)
    {
        encoder << pair.first << pair.second;
    }

    static WARN_UNUSED_RETURN bool decode(Decoder& decoder, std::pair<T, U>& pair)
    {
        T first;
        if (!decoder.decode(first))
            return false;

        U second;
        if (!decoder.decode(second))
            return false;

        pair.first = first;
        pair.second = second;
        return true;
    }

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

template<size_t index, typename... Elements>
struct TupleEncoder {
    template<typename Encoder>
    static void encode(Encoder& encoder, const std::tuple<Elements...>& tuple)
    {
        encoder << std::get<sizeof...(Elements) - index>(tuple);
        TupleEncoder<index - 1, Elements...>::encode(encoder, tuple);
    }
};

template<typename... Elements>
struct TupleEncoder<0, Elements...> {
    template<typename Encoder>
    static void encode(Encoder&, const std::tuple<Elements...>&)
    {
    }
};

template <typename T, typename... Elements, size_t... Indices>
auto tupleFromTupleAndObject(T&& object, std::tuple<Elements...>&& tuple, std::index_sequence<Indices...>)
{
    return std::make_tuple(WTFMove(object), WTFMove(std::get<Indices>(tuple))...);
}

template <typename T, typename... Elements>
auto tupleFromTupleAndObject(T&& object, std::tuple<Elements...>&& tuple)
{
    return tupleFromTupleAndObject(WTFMove(object), WTFMove(tuple), std::index_sequence_for<Elements...>());
}

template<typename Type, typename... Types>
struct TupleDecoderImpl {
    static std::optional<std::tuple<Type, Types...>> decode(Decoder& decoder)
    {
        std::optional<Type> optional;
        decoder >> optional;
        if (!optional)
            return std::nullopt;

        std::optional<std::tuple<Types...>> subTuple = TupleDecoderImpl<Types...>::decode(decoder);
        if (!subTuple)
            return std::nullopt;

        return tupleFromTupleAndObject(WTFMove(*optional), WTFMove(*subTuple));
    }
};

template<typename Type>
struct TupleDecoderImpl<Type> {
    static std::optional<std::tuple<Type>> decode(Decoder& decoder)
    {
        std::optional<Type> optional;
        decoder >> optional;
        if (!optional)
            return std::nullopt;
        return std::make_tuple(WTFMove(*optional));
    }
};

template<size_t size, typename... Elements>
struct TupleDecoder {
    static std::optional<std::tuple<Elements...>> decode(Decoder& decoder)
    {
        return TupleDecoderImpl<Elements...>::decode(decoder);
    }
};

template<>
struct TupleDecoder<0> {
    static std::optional<std::tuple<>> decode(Decoder&)
    {
        return std::make_tuple();
    }
};

template<typename... Elements> struct ArgumentCoder<std::tuple<Elements...>> {
    template<typename Encoder>
    static void encode(Encoder& encoder, const std::tuple<Elements...>& tuple)
    {
        TupleEncoder<sizeof...(Elements), Elements...>::encode(encoder, tuple);
    }

    static std::optional<std::tuple<Elements...>> decode(Decoder& decoder)
    {
        return TupleDecoder<sizeof...(Elements), Elements...>::decode(decoder);
    }
};

template<typename KeyType, typename ValueType> struct ArgumentCoder<WTF::KeyValuePair<KeyType, ValueType>> {
    static void encode(Encoder& encoder, const WTF::KeyValuePair<KeyType, ValueType>& pair)
    {
        encoder << pair.key << pair.value;
    }

    static WARN_UNUSED_RETURN bool decode(Decoder& decoder, WTF::KeyValuePair<KeyType, ValueType>& pair)
    {
        KeyType key;
        if (!decoder.decode(key))
            return false;

        ValueType value;
        if (!decoder.decode(value))
            return false;

        pair.key = key;
        pair.value = value;
        return true;
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

    static WARN_UNUSED_RETURN bool decode(Decoder& decoder, Vector<T, inlineCapacity, OverflowHandler, minCapacity>& vector)
    {
        std::optional<Vector<T, inlineCapacity, OverflowHandler, minCapacity>> optional;
        decoder >> optional;
        if (!optional)
            return false;
        vector = WTFMove(*optional);
        return true;
    }

    static std::optional<Vector<T, inlineCapacity, OverflowHandler, minCapacity>> decode(Decoder& decoder)
    {
        uint64_t size;
        if (!decoder.decode(size))
            return std::nullopt;

        Vector<T, inlineCapacity, OverflowHandler, minCapacity> vector;
        for (size_t i = 0; i < size; ++i) {
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
    
    static WARN_UNUSED_RETURN bool decode(Decoder& decoder, Vector<T, inlineCapacity, OverflowHandler, minCapacity>& vector)
    {
        uint64_t decodedSize;
        if (!decoder.decode(decodedSize))
            return false;

        if (!isInBounds<size_t>(decodedSize))
            return false;

        auto size = static_cast<size_t>(decodedSize);

        // Since we know the total size of the elements, we can allocate the vector in
        // one fell swoop. Before allocating we must however make sure that the decoder buffer
        // is big enough.
        if (!decoder.bufferIsLargeEnoughToContain<T>(size))
            return false;

        Vector<T, inlineCapacity, OverflowHandler, minCapacity> temp;
        temp.grow(size);

        if (!decoder.decodeFixedLengthData(reinterpret_cast<uint8_t*>(temp.data()), size * sizeof(T), alignof(T)))
            return false;

        vector.swap(temp);
        return true;
    }
    
    static std::optional<Vector<T, inlineCapacity, OverflowHandler, minCapacity>> decode(Decoder& decoder)
    {
        uint64_t decodedSize;
        if (!decoder.decode(decodedSize))
            return std::nullopt;

        if (!isInBounds<size_t>(decodedSize))
            return std::nullopt;

        auto size = static_cast<size_t>(decodedSize);

        // Since we know the total size of the elements, we can allocate the vector in
        // one fell swoop. Before allocating we must however make sure that the decoder buffer
        // is big enough.
        if (!decoder.bufferIsLargeEnoughToContain<T>(size))
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

    static void encode(Encoder& encoder, const HashMapType& hashMap)
    {
        encoder << static_cast<uint32_t>(hashMap.size());
        for (typename HashMapType::const_iterator it = hashMap.begin(), end = hashMap.end(); it != end; ++it)
            encoder << *it;
    }

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

    static WARN_UNUSED_RETURN bool decode(Decoder& decoder, HashMapType& hashMap)
    {
        std::optional<HashMapType> tempHashMap;
        decoder >> tempHashMap;
        if (!tempHashMap)
            return false;
        hashMap.swap(*tempHashMap);
        return true;
    }
};

template<typename KeyArg, typename HashArg, typename KeyTraitsArg, typename HashTableTraits> struct ArgumentCoder<HashSet<KeyArg, HashArg, KeyTraitsArg, HashTableTraits>> {
    typedef HashSet<KeyArg, HashArg, KeyTraitsArg, HashTableTraits> HashSetType;

    static void encode(Encoder& encoder, const HashSetType& hashSet)
    {
        encoder << static_cast<uint64_t>(hashSet.size());
        for (typename HashSetType::const_iterator it = hashSet.begin(), end = hashSet.end(); it != end; ++it)
            encoder << *it;
    }

    static WARN_UNUSED_RETURN bool decode(Decoder& decoder, HashSetType& hashSet)
    {
        std::optional<HashSetType> tempHashSet;
        decoder >> tempHashSet;
        if (!tempHashSet)
            return false;

        hashSet.swap(tempHashSet.value());
        return true;
    }

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
    
    static void encode(Encoder& encoder, const HashCountedSetType& hashCountedSet)
    {
        encoder << static_cast<uint64_t>(hashCountedSet.size());
        
        for (auto entry : hashCountedSet) {
            encoder << entry.key;
            encoder << entry.value;
        }
    }
    
    static WARN_UNUSED_RETURN bool decode(Decoder& decoder, HashCountedSetType& hashCountedSet)
    {
        uint64_t hashCountedSetSize;
        if (!decoder.decode(hashCountedSetSize))
            return false;
        
        HashCountedSetType tempHashCountedSet;
        for (uint64_t i = 0; i < hashCountedSetSize; ++i) {
            KeyArg key;
            if (!decoder.decode(key))
                return false;
            
            unsigned count;
            if (!decoder.decode(count))
                return false;

            if (UNLIKELY(!HashCountedSetType::isValidValue(key)))
                return false;

            if (UNLIKELY(!tempHashCountedSet.add(key, count).isNewEntry)) {
                // The hash counted set already has the specified key, bail.
                return false;
            }
        }
        
        hashCountedSet.swap(tempHashCountedSet);
        return true;
    }
};

template<typename ValueType, typename ErrorType> struct ArgumentCoder<Expected<ValueType, ErrorType>> {
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

template<size_t index, typename... Types>
struct VariantCoder {
    static void encode(Encoder& encoder, const WTF::Variant<Types...>& variant, unsigned i)
    {
        if (i == index) {
            encoder << WTF::get<index>(variant);
            return;
        }
        VariantCoder<index - 1, Types...>::encode(encoder, variant, i);
    }
    
    static std::optional<WTF::Variant<Types...>> decode(Decoder& decoder, unsigned i)
    {
        if (i == index) {
            std::optional<typename WTF::variant_alternative<index, WTF::Variant<Types...>>::type> optional;
            decoder >> optional;
            if (!optional)
                return std::nullopt;
            return { WTFMove(*optional) };
        }
        return VariantCoder<index - 1, Types...>::decode(decoder, i);
    }
};

template<typename... Types>
struct VariantCoder<0, Types...> {
    static void encode(Encoder& encoder, const WTF::Variant<Types...>& variant, unsigned i)
    {
        ASSERT_UNUSED(i, !i);
        encoder << WTF::get<0>(variant);
    }
    
    static std::optional<WTF::Variant<Types...>> decode(Decoder& decoder, unsigned i)
    {
        ASSERT_UNUSED(i, !i);
        std::optional<typename WTF::variant_alternative<0, WTF::Variant<Types...>>::type> optional;
        decoder >> optional;
        if (!optional)
            return std::nullopt;
        return { WTFMove(*optional) };
    }
};

template<typename... Types> struct ArgumentCoder<WTF::Variant<Types...>> {
    static void encode(Encoder& encoder, const WTF::Variant<Types...>& variant)
    {
        unsigned i = variant.index();
        encoder << i;
        VariantCoder<sizeof...(Types) - 1, Types...>::encode(encoder, variant, i);
    }
    
    static std::optional<WTF::Variant<Types...>> decode(Decoder& decoder)
    {
        std::optional<unsigned> i;
        decoder >> i;
        if (!i)
            return std::nullopt;
        return VariantCoder<sizeof...(Types) - 1, Types...>::decode(decoder, *i);
    }
};
    
template<> struct ArgumentCoder<WallTime> {
    static void encode(Encoder&, const WallTime&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WallTime&);
    static std::optional<WallTime> decode(Decoder&);
};

template<> struct ArgumentCoder<AtomString> {
    static void encode(Encoder&, const AtomString&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, AtomString&);
};

template<> struct ArgumentCoder<CString> {
    static void encode(Encoder&, const CString&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, CString&);
};

template<> struct ArgumentCoder<String> {
    template<typename Encoder>
    static void encode(Encoder&, const String&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, String&);
    static std::optional<String> decode(Decoder&);
};

template<> struct ArgumentCoder<SHA1::Digest> {
    static void encode(Encoder&, const SHA1::Digest&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, SHA1::Digest&);
};

#if HAVE(AUDIT_TOKEN)
template<> struct ArgumentCoder<audit_token_t> {
    static void encode(Encoder&, const audit_token_t&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, audit_token_t&);
};
#endif

template<> struct ArgumentCoder<Monostate> {
    static void encode(Encoder&, const Monostate&);
    static std::optional<Monostate> decode(Decoder&);
};

} // namespace IPC
