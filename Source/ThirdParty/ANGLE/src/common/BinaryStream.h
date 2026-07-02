//
// Copyright 2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// BinaryStream.h: Provides binary serialization of simple types.

#ifndef COMMON_BINARYSTREAM_H_
#define COMMON_BINARYSTREAM_H_

#include <stdint.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "common/PackedEnums.h"
#include "common/angleutils.h"
#include "common/span.h"
#include "common/span_util.h"

namespace gl
{
template <typename IntT>
struct PromotedIntegerType
{
    using type = typename std::conditional<
        std::is_signed<IntT>::value,
        typename std::conditional<sizeof(IntT) <= 4, int32_t, int64_t>::type,
        typename std::conditional<sizeof(IntT) <= 4, uint32_t, uint64_t>::type>::type;
};

class BinaryInputStream : angle::NonCopyable
{
  public:
    BinaryInputStream(angle::Span<const uint8_t> data) : mData(data) {}

    // readInt will generate an error for bool types
    template <class IntT>
    IntT readInt()
    {
        static_assert(!std::is_same<bool, std::remove_cv<IntT>()>(), "Use readBool");
        using PromotedIntT = typename PromotedIntegerType<IntT>::type;
        PromotedIntT value = 0;
        read(angle::byte_span_from_ref(value));
        ASSERT(angle::IsValueInRangeForNumericType<IntT>(value));
        return static_cast<IntT>(value);
    }

    template <class IntT>
    void readInt(IntT *outValue)
    {
        *outValue = readInt<IntT>();
    }

    template <class T>
    void readVector(std::vector<T> *param)
    {
        static_assert(std::is_trivially_copyable<T>(), "must be memcpy-able");
        ASSERT(param->empty());
        size_t size = readInt<size_t>();
        if (size > 0)
        {
            param->resize(size);
            readBytes(angle::as_writable_byte_span(*param));
        }
    }

    template <typename E, typename T>
    void readPackedEnumMap(angle::PackedEnumMap<E, T> *param)
    {
        static_assert(std::is_trivially_copyable<T>(), "must be memcpy-able");
        readBytes(angle::as_writable_byte_span(*param));
    }

    template <class T>
    void readStruct(T *param)
    {
        static_assert(std::is_trivially_copyable<T>(), "must be memcpy-able");
        readBytes(angle::byte_span_from_ref(*param));
    }

    template <class EnumT>
    EnumT readEnum()
    {
        using UnderlyingType = typename std::underlying_type<EnumT>::type;
        return static_cast<EnumT>(readInt<UnderlyingType>());
    }

    template <class EnumT>
    void readEnum(EnumT *outValue)
    {
        *outValue = readEnum<EnumT>();
    }

    bool readBool()
    {
        int value = 0;
        read(angle::byte_span_from_ref(value));
        return value > 0;
    }

    void readBool(bool *outValue) { *outValue = readBool(); }

    void readBytes(angle::Span<uint8_t> outArray) { read(outArray); }

    std::string readString()
    {
        std::string outString;
        readString(&outString);
        return outString;
    }

    void readString(std::string *v)
    {
        size_t length;
        readInt(&length);

        if (mError)
        {
            return;
        }

        angle::CheckedNumeric<size_t> checkedOffset(mOffset);
        checkedOffset += length;

        if (!checkedOffset.IsValid() || checkedOffset.ValueOrDie() > mData.size())
        {
            mError = true;
            return;
        }
        auto char_span = angle::as_chars(mData).subspan(mOffset, length);
        v->assign(char_span.data(), char_span.size());
        mOffset = checkedOffset.ValueOrDie();
    }

    float readFloat()
    {
        float f = 0.0f;
        read(angle::byte_span_from_ref(f));
        return f;
    }

    void skip(size_t length)
    {
        angle::CheckedNumeric<size_t> checkedOffset(mOffset);
        checkedOffset += length;

        if (!checkedOffset.IsValid() || checkedOffset.ValueOrDie() > mData.size())
        {
            mError = true;
            return;
        }

        mOffset = checkedOffset.ValueOrDie();
    }

    bool error() const { return mError; }
    bool endOfStream() const { return mOffset == mData.size(); }

    // data() and size() methods allow implicit conversion to span.
    const uint8_t *data() const { return mData.data(); }
    size_t size() const { return mData.size(); }

    angle::Span<const uint8_t> remainingSpan() const { return mData.subspan(mOffset); }

  private:
    void read(angle::Span<uint8_t> dstSpan)
    {
        angle::CheckedNumeric<size_t> checkedOffset(mOffset);
        checkedOffset += dstSpan.size();

        if (!checkedOffset.IsValid() || checkedOffset.ValueOrDie() > mData.size())
        {
            mError = true;
            return;
        }

        angle::Span<const uint8_t> srcSpan = mData.subspan(mOffset, dstSpan.size());
        angle::SpanMemcpy(dstSpan, srcSpan);
        mOffset = checkedOffset.ValueOrDie();
    }

    bool mError    = false;
    size_t mOffset = 0;
    angle::Span<const uint8_t> mData;
};

class BinaryOutputStream : angle::NonCopyable
{
  public:
    BinaryOutputStream()  = default;
    ~BinaryOutputStream() = default;

    // writeInt also handles bool types
    template <class IntT>
    void writeInt(IntT param)
    {
        static_assert(std::is_integral<IntT>::value, "Not an integral type");
        static_assert(!std::is_same<bool, std::remove_cv<IntT>()>(), "Use writeBool");
        using PromotedIntT = typename PromotedIntegerType<IntT>::type;
        ASSERT(angle::IsValueInRangeForNumericType<PromotedIntT>(param));
        PromotedIntT intValue = static_cast<PromotedIntT>(param);
        write(angle::byte_span_from_ref(intValue));
    }

    // Specialized writeInt for values that can also be exactly -1.
    template <class UintT>
    void writeIntOrNegOne(UintT param)
    {
        if (param == static_cast<UintT>(-1))
        {
            writeInt(-1);
        }
        else
        {
            writeInt(param);
        }
    }

    template <class T>
    void writeVector(const std::vector<T> &param)
    {
        static_assert(std::is_trivially_copyable<T>(), "must be memcpy-able");
        writeInt(param.size());
        if (param.size() > 0)
        {
            writeBytes(angle::as_byte_span(param));
        }
    }

    template <typename E, typename T>
    void writePackedEnumMap(const angle::PackedEnumMap<E, T> &param)
    {
        static_assert(std::is_trivially_copyable<T>(), "must be memcpy-able");
        writeBytes(angle::as_byte_span(param));
    }

    template <class T>
    void writeStruct(const T &param)
    {
        static_assert(!std::is_pointer<T>::value,
                      "Must pass in a struct, not the pointer to struct");
        static_assert(std::is_trivially_copyable<T>(), "must be memcpy-able");
        writeBytes(angle::byte_span_from_ref(param));
    }

    template <class EnumT>
    void writeEnum(EnumT param)
    {
        using UnderlyingType = typename std::underlying_type<EnumT>::type;
        writeInt<UnderlyingType>(static_cast<UnderlyingType>(param));
    }

    void writeString(std::string_view v)
    {
        writeInt(v.size());
        write(angle::as_byte_span(v));
    }

    void writeBytes(angle::Span<const uint8_t> bytes) { write(bytes); }

    void writeBool(bool value)
    {
        const int intValue = value ? 1 : 0;
        write(angle::byte_span_from_ref(intValue));
    }

    void writeFloat(float value) { write(angle::byte_span_from_ref(value)); }

    // data() and size() methods allow implicit conversion to span.
    const uint8_t *data() const { return mData.data(); }
    size_t size() const { return mData.size(); }

    // No further use of this stream allowed after data is taken.
    std::vector<uint8_t> takeData() { return std::move(mData); }

  private:
    void write(angle::Span<const uint8_t> srcSpan)
    {
        mData.insert(mData.end(), srcSpan.begin(), srcSpan.end());
    }

    std::vector<uint8_t> mData;
};

}  // namespace gl

#endif  // COMMON_BINARYSTREAM_H_
