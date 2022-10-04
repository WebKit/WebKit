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

template<typename T0, typename T1> struct ArgumentCoder<ArrayReferenceTuple<T0, T1>> {
    using ArrayReferenceTupleType = ArrayReferenceTuple<T0, T1>;
    template<typename Encoder>
    static void encode(Encoder& encoder, const ArrayReferenceTupleType& arrayReference)
    {
        encoder << static_cast<uint64_t>(arrayReference.size());
        auto size = arrayReference.size();
        if (UNLIKELY(!size))
            return;
        encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(arrayReference.template data<0>()), size * sizeof(T0), alignof(T0));
        encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(arrayReference.template data<1>()), size * sizeof(T1), alignof(T1));
    }
    static std::optional<ArrayReferenceTupleType> decode(Decoder& decoder)
    {
        uint64_t size;
        if (UNLIKELY(!decoder.decode(size)))
            return std::nullopt;
        if (UNLIKELY(!size))
            return ArrayReferenceTupleType { };

        auto dataSize = CheckedSize { size } * sizeof(T0);
        if (UNLIKELY(dataSize.hasOverflowed()))
            return std::nullopt;
        const uint8_t* data0 = decoder.decodeFixedLengthReference(dataSize, alignof(T0));
        if (UNLIKELY(!data0))
            return std::nullopt;
        dataSize = CheckedSize { size } * sizeof(T1);
        if (UNLIKELY(dataSize.hasOverflowed()))
            return std::nullopt;
        const uint8_t* data1 = decoder.decodeFixedLengthReference(dataSize, alignof(T1));
        if (UNLIKELY(!data1))
            return std::nullopt;
        return ArrayReferenceTupleType { reinterpret_cast<const T0*>(data0), reinterpret_cast<const T1*>(data1), static_cast<size_t>(size) };
    }
};

template<typename T0, typename T1, typename T2> struct ArgumentCoder<ArrayReferenceTuple<T0, T1, T2>> {
    using ArrayReferenceTupleType = ArrayReferenceTuple<T0, T1, T2>;
    template<typename Encoder>
    static void encode(Encoder& encoder, const ArrayReferenceTupleType& arrayReference)
    {
        encoder << static_cast<uint64_t>(arrayReference.size());
        auto size = arrayReference.size();
        if (UNLIKELY(!size))
            return;
        encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(arrayReference.template data<0>()), size * sizeof(T0), alignof(T0));
        encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(arrayReference.template data<1>()), size * sizeof(T1), alignof(T1));
        encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(arrayReference.template data<2>()), size * sizeof(T2), alignof(T2));
    }
    static std::optional<ArrayReferenceTupleType> decode(Decoder& decoder)
    {
        uint64_t size;
        if (UNLIKELY(!decoder.decode(size)))
            return std::nullopt;
        if (UNLIKELY(!size))
            return ArrayReferenceTupleType { };

        auto dataSize = CheckedSize { size } * sizeof(T0);
        if (UNLIKELY(dataSize.hasOverflowed()))
            return std::nullopt;
        const uint8_t* data0 = decoder.decodeFixedLengthReference(dataSize, alignof(T0));
        if (UNLIKELY(!data0))
            return std::nullopt;
        dataSize = CheckedSize { size } * sizeof(T1);
        if (UNLIKELY(dataSize.hasOverflowed()))
            return std::nullopt;
        const uint8_t* data1 = decoder.decodeFixedLengthReference(dataSize, alignof(T1));
        if (UNLIKELY(!data1))
            return std::nullopt;
        dataSize = CheckedSize { size } * sizeof(T2);
        if (UNLIKELY(dataSize.hasOverflowed()))
            return std::nullopt;
        const uint8_t* data2 = decoder.decodeFixedLengthReference(dataSize, alignof(T2));
        if (UNLIKELY(!data2))
            return std::nullopt;
        return ArrayReferenceTupleType { reinterpret_cast<const T0*>(data0), reinterpret_cast<const T1*>(data1), reinterpret_cast<const T2*>(data2), static_cast<size_t>(size) };
    }
};

template<typename T0, typename T1, typename T2, typename T3> struct ArgumentCoder<ArrayReferenceTuple<T0, T1, T2, T3>> {
    using ArrayReferenceTupleType = ArrayReferenceTuple<T0, T1, T2, T3>;
    template<typename Encoder>
    static void encode(Encoder& encoder, const ArrayReferenceTupleType& arrayReference)
    {
        encoder << static_cast<uint64_t>(arrayReference.size());
        auto size = arrayReference.size();
        if (UNLIKELY(!size))
            return;
        encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(arrayReference.template data<0>()), size * sizeof(T0), alignof(T0));
        encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(arrayReference.template data<1>()), size * sizeof(T1), alignof(T1));
        encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(arrayReference.template data<2>()), size * sizeof(T2), alignof(T2));
        encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(arrayReference.template data<3>()), size * sizeof(T3), alignof(T3));
    }
    static std::optional<ArrayReferenceTupleType> decode(Decoder& decoder)
    {
        uint64_t size;
        if (UNLIKELY(!decoder.decode(size)))
            return std::nullopt;
        if (UNLIKELY(!size))
            return ArrayReferenceTupleType { };

        auto dataSize = CheckedSize { size } * sizeof(T0);
        if (UNLIKELY(dataSize.hasOverflowed()))
            return std::nullopt;
        const uint8_t* data0 = decoder.decodeFixedLengthReference(dataSize, alignof(T0));
        if (UNLIKELY(!data0))
            return std::nullopt;
        dataSize = CheckedSize { size } * sizeof(T1);
        if (UNLIKELY(dataSize.hasOverflowed()))
            return std::nullopt;
        const uint8_t* data1 = decoder.decodeFixedLengthReference(dataSize, alignof(T1));
        if (UNLIKELY(!data1))
            return std::nullopt;
        dataSize = CheckedSize { size } * sizeof(T2);
        if (UNLIKELY(dataSize.hasOverflowed()))
            return std::nullopt;
        const uint8_t* data2 = decoder.decodeFixedLengthReference(dataSize, alignof(T2));
        if (UNLIKELY(!data2))
            return std::nullopt;
        dataSize = CheckedSize { size } * sizeof(T3);
        if (UNLIKELY(dataSize.hasOverflowed()))
            return std::nullopt;
        const uint8_t* data3 = decoder.decodeFixedLengthReference(dataSize, alignof(T3));
        if (UNLIKELY(!data3))
            return std::nullopt;
        return ArrayReferenceTupleType { reinterpret_cast<const T0*>(data0), reinterpret_cast<const T1*>(data1), reinterpret_cast<const T2*>(data2), reinterpret_cast<const T3*>(data3), static_cast<size_t>(size) };
    }
};

template<typename T0, typename T1, typename T2, typename T3, typename T4> struct ArgumentCoder<ArrayReferenceTuple<T0, T1, T2, T3, T4>> {
    using ArrayReferenceTupleType = ArrayReferenceTuple<T0, T1, T2, T3, T4>;
    template<typename Encoder>
    static void encode(Encoder& encoder, const ArrayReferenceTupleType& arrayReference)
    {
        encoder << static_cast<uint64_t>(arrayReference.size());
        auto size = arrayReference.size();
        if (UNLIKELY(!size))
            return;
        encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(arrayReference.template data<0>()), size * sizeof(T0), alignof(T0));
        encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(arrayReference.template data<1>()), size * sizeof(T1), alignof(T1));
        encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(arrayReference.template data<2>()), size * sizeof(T2), alignof(T2));
        encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(arrayReference.template data<3>()), size * sizeof(T3), alignof(T3));
        encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(arrayReference.template data<4>()), size * sizeof(T4), alignof(T4));
    }
    static std::optional<ArrayReferenceTupleType> decode(Decoder& decoder)
    {
        uint64_t size;
        if (UNLIKELY(!decoder.decode(size)))
            return std::nullopt;
        if (UNLIKELY(!size))
            return ArrayReferenceTupleType { };

        auto dataSize = CheckedSize { size } * sizeof(T0);
        if (UNLIKELY(dataSize.hasOverflowed()))
            return std::nullopt;
        const uint8_t* data0 = decoder.decodeFixedLengthReference(dataSize, alignof(T0));
        if (UNLIKELY(!data0))
            return std::nullopt;
        dataSize = CheckedSize { size } * sizeof(T1);
        if (UNLIKELY(dataSize.hasOverflowed()))
            return std::nullopt;
        const uint8_t* data1 = decoder.decodeFixedLengthReference(dataSize, alignof(T1));
        if (UNLIKELY(!data1))
            return std::nullopt;
        dataSize = CheckedSize { size } * sizeof(T2);
        if (UNLIKELY(dataSize.hasOverflowed()))
            return std::nullopt;
        const uint8_t* data2 = decoder.decodeFixedLengthReference(dataSize, alignof(T2));
        if (UNLIKELY(!data2))
            return std::nullopt;
        dataSize = CheckedSize { size } * sizeof(T3);
        if (UNLIKELY(dataSize.hasOverflowed()))
            return std::nullopt;
        const uint8_t* data3 = decoder.decodeFixedLengthReference(dataSize, alignof(T3));
        if (UNLIKELY(!data3))
            return std::nullopt;
        dataSize = CheckedSize { size } * sizeof(T4);
        if (UNLIKELY(dataSize.hasOverflowed()))
            return std::nullopt;
        const uint8_t* data4 = decoder.decodeFixedLengthReference(dataSize, alignof(T4));
        if (UNLIKELY(!data4))
            return std::nullopt;
        return ArrayReferenceTupleType { reinterpret_cast<const T0*>(data0), reinterpret_cast<const T1*>(data1), reinterpret_cast<const T2*>(data2), reinterpret_cast<const T3*>(data3), reinterpret_cast<const T4*>(data4), static_cast<size_t>(size) };
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

template<typename... Elements>
struct TupleEncoder {
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
};

template<typename... Elements> struct TupleDecoder;

template<typename Type, typename... Types>
struct TupleDecoder<Type, Types...> {
    template<typename Decoder>
    static std::optional<std::tuple<Type, Types...>> decode(Decoder& decoder)
    {
        std::optional<Type> optional;
        decoder >> optional;
        if (!optional)
            return std::nullopt;

        std::optional<std::tuple<Types...>> remainder = TupleDecoder<Types...>::decode(decoder);
        if (!remainder)
            return std::nullopt;

        return std::tuple_cat(std::make_tuple(WTFMove(*optional)), WTFMove(*remainder));
    }
};

template<>
struct TupleDecoder<> {
    template<typename Decoder>
    static std::optional<std::tuple<>> decode(Decoder&)
    {
        return std::make_tuple();
    }
};

template<typename... Elements> struct ArgumentCoder<std::tuple<Elements...>> {
    template<typename Encoder>
    static void encode(Encoder& encoder, const std::tuple<Elements...>& tuple)
    {
        TupleEncoder<Elements...>::encode(encoder, tuple);
    }

    template<typename Decoder>
    static std::optional<std::tuple<Elements...>> decode(Decoder& decoder)
    {
        return TupleDecoder<Elements...>::decode(decoder);
    }
};

template<typename KeyType, typename ValueType> struct ArgumentCoder<WTF::KeyValuePair<KeyType, ValueType>> {
    template<typename Encoder>
    static void encode(Encoder& encoder, const WTF::KeyValuePair<KeyType, ValueType>& pair)
    {
        encoder << pair.key << pair.value;
    }

    template<typename Decoder>
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

    template<typename Decoder>
    static WARN_UNUSED_RETURN bool decode(Decoder& decoder, Vector<T, inlineCapacity, OverflowHandler, minCapacity>& vector)
    {
        std::optional<Vector<T, inlineCapacity, OverflowHandler, minCapacity>> optional;
        decoder >> optional;
        if (!optional)
            return false;
        vector = WTFMove(*optional);
        return true;
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
        if (!decoder.template bufferIsLargeEnoughToContain<T>(size))
            return false;

        Vector<T, inlineCapacity, OverflowHandler, minCapacity> temp;
        temp.grow(size);

        if (!decoder.decodeFixedLengthData(reinterpret_cast<uint8_t*>(temp.data()), size * sizeof(T), alignof(T)))
            return false;

        vector.swap(temp);
        return true;
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

    template<typename Decoder>
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

    template<typename Encoder>
    static void encode(Encoder& encoder, const HashSetType& hashSet)
    {
        encoder << static_cast<uint64_t>(hashSet.size());
        for (typename HashSetType::const_iterator it = hashSet.begin(), end = hashSet.end(); it != end; ++it)
            encoder << *it;
    }

    template<typename Decoder>
    static WARN_UNUSED_RETURN bool decode(Decoder& decoder, HashSetType& hashSet)
    {
        std::optional<HashSetType> tempHashSet;
        decoder >> tempHashSet;
        if (!tempHashSet)
            return false;

        hashSet.swap(tempHashSet.value());
        return true;
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

template<> struct ArgumentCoder<WallTime> {
    template<typename Encoder>
    static void encode(Encoder&, const WallTime&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, WallTime&);
    template<typename Decoder>
    static std::optional<WallTime> decode(Decoder&);
};

template<> struct ArgumentCoder<AtomString> {
    static void encode(Encoder&, const AtomString&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, AtomString&);
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
    static WARN_UNUSED_RETURN bool decode(Decoder&, String&);
    template<typename Decoder>
    static std::optional<String> decode(Decoder&);
};

template<> struct ArgumentCoder<StringView> {
    template<typename Encoder>
    static void encode(Encoder&, StringView);
};

template<> struct ArgumentCoder<SHA1::Digest> {
    static void encode(Encoder&, const SHA1::Digest&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, SHA1::Digest&);
};

template<> struct ArgumentCoder<std::monostate> {
    template<typename Encoder>
    static void encode(Encoder&, const std::monostate&) { }

    template<typename Decoder>
    static std::optional<std::monostate> decode(Decoder&)
    {
        return std::monostate { };
    }
};

template<> struct ArgumentCoder<std::nullptr_t> {
    static void encode(Encoder&, const std::nullptr_t&) { }
    static WARN_UNUSED_RETURN bool decode(Decoder&, std::nullptr_t&) { return true; }
};

} // namespace IPC
