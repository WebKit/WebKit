/*
 * Copyright (C) 2022 Igalia S.L.
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "config.h"

#include "ArgumentCoders.h"
#include "Decoder.h"
#include "Encoder.h"
#include "StreamConnectionEncoder.h"
#include "Test.h"

namespace TestWebKitAPI {

// ArgumentCoderEncoderDecoderTest internally constructs the encoder object specified
// as the template parameter, into which each test can encode the desired objects.
// It then uses that Encoder's internal buffer to create a Decoder object, intended for
// the test to decode those objects.

struct EncoderDecoderTest {
    static constexpr IPC::MessageName name()  { return static_cast<IPC::MessageName>(123); }
};

struct EncoderTypeNames {
    template<typename T>
    static std::string GetName(int)
    {
        if (std::is_same_v<T, IPC::Encoder>)
            return "Encoder";
        if (std::is_same_v<T, IPC::StreamConnectionEncoder>)
            return "StreamConnectionEncoder";
        return "<unknown>";
    }
};

using EncoderTypes = ::testing::Types<IPC::Encoder, IPC::StreamConnectionEncoder>;

template<typename T> class ArgumentCoderEncoderDecoderTest;

template<>
class ArgumentCoderEncoderDecoderTest<IPC::Encoder> : public ::testing::Test {
public:
    void SetUp() override
    {
        m_encoder = makeUnique<IPC::Encoder>(EncoderDecoderTest::name(), 0);
        ASSERT_EQ(m_encoder->bufferSize(), headerSize());
    }

    IPC::Encoder& encoder() const { return *m_encoder; }
    size_t headerSize() const { return 16; }
    size_t encoderSize() const { return m_encoder->bufferSize(); }

    std::unique_ptr<IPC::Decoder> createDecoder() const
    {
        return IPC::Decoder::create({ m_encoder->buffer(), m_encoder->bufferSize() }, { });
    }

private:
    std::unique_ptr<IPC::Encoder> m_encoder;
};

template<>
class ArgumentCoderEncoderDecoderTest<IPC::StreamConnectionEncoder> : public ::testing::Test {
public:
    void SetUp() override
    {
        m_impl = makeUnique<Impl>();
        ASSERT_EQ(m_impl->encoder.size(), headerSize());
    }

    IPC::StreamConnectionEncoder& encoder() const { return m_impl->encoder; }
    size_t headerSize() const { return 2; }
    size_t encoderSize() const { return m_impl->encoder.size(); }

    std::unique_ptr<IPC::Decoder> createDecoder() const
    {
        auto decoder = makeUnique<IPC::Decoder>(IPC::DataReference { m_impl->buffer.data(), m_impl->buffer.size() }, 0);
        return decoder;
    }

private:
    struct Impl {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;

        Impl()
            : buffer(1024, static_cast<uint8_t>(0))
            , encoder(EncoderDecoderTest::name(), buffer.data(), buffer.size())
        { }

        Vector<uint8_t> buffer;
        IPC::StreamConnectionEncoder encoder;
    };
    std::unique_ptr<Impl> m_impl;
};


struct EncodingCounter {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    struct CounterValues {
        CounterValues() = default;
        CounterValues(unsigned encodingLValue, unsigned encodingRValue)
            : encodingLValue(encodingLValue)
            , encodingRValue(encodingRValue)
        { }

        unsigned encodingLValue { 0 };
        unsigned encodingRValue { 0 };

        bool operator==(const CounterValues& other) const
        {
            return encodingLValue == other.encodingLValue && encodingRValue == other.encodingRValue;
        }
    };

    EncodingCounter(CounterValues& counterValues)
        : m_counterValues(counterValues)
    { }

    void encode(IPC::Encoder& encoder) const & {
        encoder << static_cast<uint32_t>(0);
        ++m_counterValues.encodingLValue;
    }

    void encode(IPC::Encoder& encoder) && {
        encoder << static_cast<uint32_t>(0);
        ++m_counterValues.encodingRValue;
    }

    static std::optional<EncodingCounter> decode(IPC::Decoder&) { return std::nullopt; }

    CounterValues& m_counterValues;
};

enum class EncodingCounterTestType {
    LValue,
    RValue,
    MovedRValue,
};

void PrintTo(EncodingCounterTestType value, ::std::ostream* o)
{
    switch (value) {
    case EncodingCounterTestType::LValue:
        *o << "LValue";
        return;
    case EncodingCounterTestType::RValue:
        *o << "RValue";
        return;
    case EncodingCounterTestType::MovedRValue:
        *o << "MovedRValue";
        return;
    default:
        break;
    }

    *o << "Unknown";
}

class ArgumentCoderEncodingCounterTest : public ::testing::TestWithParam<std::tuple<EncodingCounterTestType>> {
public:
    ArgumentCoderEncodingCounterTest()
        : m_encoder(static_cast<IPC::MessageName>(0), 42)
    { }

    template<typename F>
    void testEncoding(unsigned expectedEncodingCount, const F& createFunctor)
    {
        EncodingCounter::CounterValues counterValues;

        switch (std::get<0>(GetParam())) {
        case EncodingCounterTestType::LValue: {
            auto object = createFunctor(counterValues);
            m_encoder << object;

            ASSERT_EQ(counterValues, EncodingCounter::CounterValues(expectedEncodingCount, 0));
            break;
        }
        case EncodingCounterTestType::RValue: {
            m_encoder << createFunctor(counterValues);

            ASSERT_EQ(counterValues, EncodingCounter::CounterValues(0, expectedEncodingCount));
            break;
        }
        case EncodingCounterTestType::MovedRValue: {
            auto object = createFunctor(counterValues);
            m_encoder << WTFMove(object);

            ASSERT_EQ(counterValues, EncodingCounter::CounterValues(0, expectedEncodingCount));
            break;
        }
        }
    }

private:
    IPC::Encoder m_encoder;
};

TEST_P(ArgumentCoderEncodingCounterTest, EncodeRawObject)
{
    testEncoding(1,
        [](auto& counterValues)
        {
            return EncodingCounter { counterValues };
        });
}

TEST_P(ArgumentCoderEncodingCounterTest, EncodeOptional)
{
    testEncoding(1,
        [](auto& counterValues)
        {
            return std::optional<EncodingCounter> { EncodingCounter { counterValues } };
        });
}

TEST_P(ArgumentCoderEncodingCounterTest, EncodePair)
{
    testEncoding(1,
        [](auto& counterValues)
        {
            return std::pair<unsigned, EncodingCounter> { 0, EncodingCounter { counterValues } };
        });
}

TEST_P(ArgumentCoderEncodingCounterTest, EncodeTuple)
{
    testEncoding(2,
        [](auto& counterValues)
        {
            return std::tuple<EncodingCounter, unsigned, EncodingCounter> {
                EncodingCounter { counterValues }, 0,
                EncodingCounter { counterValues },
            };
        });
}

TEST_P(ArgumentCoderEncodingCounterTest, EncodeArray)
{
    testEncoding(4,
        [](auto& counterValues)
        {
            return std::array<EncodingCounter, 4> {
                EncodingCounter { counterValues },
                EncodingCounter { counterValues },
                EncodingCounter { counterValues },
                EncodingCounter { counterValues },
            };
        });
}

TEST_P(ArgumentCoderEncodingCounterTest, EncodeVector)
{
    testEncoding(16,
        [](auto& counterValues)
        {
            Vector<EncodingCounter> counters;
            for (unsigned i = 0; i < 16; ++i)
                counters.append(EncodingCounter { counterValues });
            return counters;
        });
}

TEST_P(ArgumentCoderEncodingCounterTest, EncodeVariant)
{
    testEncoding(1,
        [](auto& counterValues)
        {
            return std::variant<EncodingCounter, unsigned> { EncodingCounter { counterValues } };
        });
}

TEST_P(ArgumentCoderEncodingCounterTest, EncodeUniquePtr)
{
    testEncoding(1,
        [](auto& counterValues)
        {
            return makeUnique<EncodingCounter>(counterValues);
        });
}

INSTANTIATE_TEST_SUITE_P(ArgumentCoderTest,
    ArgumentCoderEncodingCounterTest,
    testing::Values(EncodingCounterTestType::LValue, EncodingCounterTestType::RValue, EncodingCounterTestType::MovedRValue),
    TestParametersToStringFormatter());


template<typename T> class ArgumentCoderDecodingMoveCounterTest : public ArgumentCoderEncoderDecoderTest<T> { };
TYPED_TEST_SUITE_P(ArgumentCoderDecodingMoveCounterTest);

// DecodingMoveCounter is a move-only class whose move constructor and
// move assignment operator increase the moved-in object's move counter.

struct DecodingMoveCounter {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    DecodingMoveCounter() = default;

    DecodingMoveCounter(const DecodingMoveCounter&) = delete;
    DecodingMoveCounter& operator=(const DecodingMoveCounter&) = delete;

    DecodingMoveCounter(DecodingMoveCounter&& o)
    {
        moveCounter = o.moveCounter + 1;
    }

    DecodingMoveCounter& operator=(DecodingMoveCounter&& o)
    {
        moveCounter = o.moveCounter + 1;
        return *this;
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << static_cast<uint64_t>(42);
    }

    template<typename Decoder>
    static std::optional<DecodingMoveCounter> decode(Decoder& decoder)
    {
        auto value = decoder.template decode<uint64_t>();
        if (!value || *value != 42)
            return std::nullopt;
        return std::make_optional<DecodingMoveCounter>();
    }

    unsigned moveCounter { 0 };
};

TYPED_TEST_P(ArgumentCoderDecodingMoveCounterTest, DecodeDirectly)
{
    TestFixture::encoder() << DecodingMoveCounter { };

    auto decoder = TestFixture::createDecoder();
    auto counter = DecodingMoveCounter::decode(*decoder);
    ASSERT_TRUE(!!counter);
    ASSERT_EQ(counter->moveCounter, 0u);
}

TYPED_TEST_P(ArgumentCoderDecodingMoveCounterTest, DecodeValue)
{
    TestFixture::encoder() << DecodingMoveCounter { } << DecodingMoveCounter { };

    auto decoder = TestFixture::createDecoder();
    {
        std::optional<DecodingMoveCounter> counter;
        *decoder >> counter;
        ASSERT_TRUE(!!counter);
        ASSERT_EQ(counter->moveCounter, 1u);
    }
    {
        auto counter = decoder->template decode<DecodingMoveCounter>();
        ASSERT_TRUE(!!counter);
        ASSERT_EQ(counter->moveCounter, 0u);
    }
}

TYPED_TEST_P(ArgumentCoderDecodingMoveCounterTest, DecodeOptional)
{
    TestFixture::encoder() << std::make_optional<DecodingMoveCounter>();
    TestFixture::encoder() << std::make_optional<DecodingMoveCounter>();

    auto decoder = TestFixture::createDecoder();
    {
        std::optional<std::optional<DecodingMoveCounter>> optional;
        *decoder >> optional;
        ASSERT_TRUE(!!optional);

        auto& counter = *optional;
        ASSERT_TRUE(!!counter);
        ASSERT_EQ(counter->moveCounter, 2u);
    }
    {
        auto optional = decoder->template decode<std::optional<DecodingMoveCounter>>();
        ASSERT_TRUE(!!optional);

        auto& counter = *optional;
        ASSERT_TRUE(!!counter);
        ASSERT_EQ(counter->moveCounter, 1u);
    }
}

TYPED_TEST_P(ArgumentCoderDecodingMoveCounterTest, DecodePair)
{
    TestFixture::encoder() << std::pair { static_cast<uint64_t>(0), DecodingMoveCounter { } };
    TestFixture::encoder() << std::pair { static_cast<uint64_t>(0), DecodingMoveCounter { } };

    auto decoder = TestFixture::createDecoder();
    {
        std::optional<std::pair<uint64_t, DecodingMoveCounter>> pair;
        *decoder >> pair;
        ASSERT_TRUE(!!pair);
        ASSERT_EQ(pair->second.moveCounter, 2u);
    }
    {
        auto pair = decoder->template decode<std::pair<uint64_t, DecodingMoveCounter>>();
        ASSERT_TRUE(!!pair);
        ASSERT_EQ(pair->second.moveCounter, 1u);
    }
}

TYPED_TEST_P(ArgumentCoderDecodingMoveCounterTest, DecodeTuple)
{
    using TupleType = std::tuple<DecodingMoveCounter, uint64_t, DecodingMoveCounter>;
    TestFixture::encoder() << TupleType { DecodingMoveCounter { }, 42, DecodingMoveCounter { } };
    TestFixture::encoder() << TupleType { DecodingMoveCounter { }, 42, DecodingMoveCounter { } };

    auto decoder = TestFixture::createDecoder();
    {
        std::optional<TupleType> tuple;
        *decoder >> tuple;
        ASSERT_TRUE(!!tuple);
        ASSERT_EQ(std::get<0>(*tuple).moveCounter, 2u);
        ASSERT_EQ(std::get<2>(*tuple).moveCounter, 2u);
    }
    {
        auto tuple = decoder->template decode<TupleType>();
        ASSERT_TRUE(!!tuple);
        ASSERT_EQ(std::get<0>(*tuple).moveCounter, 1u);
        ASSERT_EQ(std::get<2>(*tuple).moveCounter, 1u);
    }
}

TYPED_TEST_P(ArgumentCoderDecodingMoveCounterTest, DecodeArray)
{
    TestFixture::encoder() << std::array<DecodingMoveCounter, 2> { DecodingMoveCounter { }, DecodingMoveCounter { } };
    TestFixture::encoder() << std::array<DecodingMoveCounter, 2> { DecodingMoveCounter { }, DecodingMoveCounter { } };

    auto decoder = TestFixture::createDecoder();
    {
        std::optional<std::array<DecodingMoveCounter, 2>> array;
        *decoder >> array;
        ASSERT_TRUE(!!array);
        for (auto&& entry : *array)
            ASSERT_EQ(entry.moveCounter, 3u);
    }
    {
        auto array = decoder->template decode<std::array<DecodingMoveCounter, 2>>();
        ASSERT_TRUE(!!array);
        for (auto&& entry : *array)
            ASSERT_EQ(entry.moveCounter, 2u);
    }
}

TYPED_TEST_P(ArgumentCoderDecodingMoveCounterTest, DecodeVector)
{
    TestFixture::encoder() << Vector<DecodingMoveCounter>::from(DecodingMoveCounter { }, DecodingMoveCounter { });
    TestFixture::encoder() << Vector<DecodingMoveCounter>::from(DecodingMoveCounter { }, DecodingMoveCounter { });

    auto decoder = TestFixture::createDecoder();
    {
        std::optional<Vector<DecodingMoveCounter>> vector;
        *decoder >> vector;
        ASSERT_TRUE(!!vector);
        ASSERT_EQ(vector->size(), 2u);
        for (auto&& entry : *vector)
            ASSERT_EQ(entry.moveCounter, 1u);
    }
    {
        auto vector = decoder->template decode<Vector<DecodingMoveCounter>>();
        ASSERT_TRUE(!!vector);
        ASSERT_EQ(vector->size(), 2u);
        for (auto&& entry : *vector)
            ASSERT_EQ(entry.moveCounter, 1u);
    }
}

TYPED_TEST_P(ArgumentCoderDecodingMoveCounterTest, DecodeVariant)
{
    using VariantType = std::variant<DecodingMoveCounter, uint64_t>;
    TestFixture::encoder() << VariantType { DecodingMoveCounter { } };
    TestFixture::encoder() << VariantType { DecodingMoveCounter { } };

    auto decoder = TestFixture::createDecoder();
    {
        std::optional<VariantType> variant;
        *decoder >> variant;
        ASSERT_TRUE(!!variant);
        ASSERT_EQ(variant->index(), 0u);
        ASSERT_EQ(std::get<0>(*variant).moveCounter, 2u);
    }
    {
        auto variant = decoder->template decode<VariantType>();
        ASSERT_TRUE(!!variant);
        ASSERT_EQ(variant->index(), 0u);
        ASSERT_EQ(std::get<0>(*variant).moveCounter, 1u);
    }
}

TYPED_TEST_P(ArgumentCoderDecodingMoveCounterTest, DecodeUniquePtr)
{
    TestFixture::encoder() << makeUnique<DecodingMoveCounter>() << makeUnique<DecodingMoveCounter>();

    auto decoder = TestFixture::createDecoder();
    {
        std::optional<std::unique_ptr<DecodingMoveCounter>> pointer;
        *decoder >> pointer;
        ASSERT_TRUE(!!pointer);

        auto& counter = *pointer;
        ASSERT_TRUE(!!counter);
        ASSERT_EQ(counter->moveCounter, 1u);
    }
    {
        auto pointer = decoder->template decode<std::unique_ptr<DecodingMoveCounter>>();
        ASSERT_TRUE(!!pointer);

        auto& counter = *pointer;
        ASSERT_TRUE(!!counter);
        ASSERT_EQ(counter->moveCounter, 1u);
    }
}

REGISTER_TYPED_TEST_SUITE_P(ArgumentCoderDecodingMoveCounterTest,
    DecodeDirectly, DecodeValue, DecodeOptional, DecodePair, DecodeTuple,
    DecodeArray, DecodeVector, DecodeVariant, DecodeUniquePtr);
INSTANTIATE_TYPED_TEST_SUITE_P(ArgumentCoderTest, ArgumentCoderDecodingMoveCounterTest, EncoderTypes, EncoderTypeNames);


template<typename T> class ArgumentCoderSpanTest : public ArgumentCoderEncoderDecoderTest<T> { };
TYPED_TEST_SUITE_P(ArgumentCoderSpanTest);

TYPED_TEST_P(ArgumentCoderSpanTest, SimpleSpan)
{
    std::array<uint8_t, 16> data8 { 0, 0, 0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233 };
    std::array<uint32_t, 16> data32 { 0, 0, 0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233 };
    TestFixture::encoder() << std::span<const uint8_t> { data8 } << std::span<const uint32_t> { data32 } << std::span<const uint64_t> { };

    auto decoder = TestFixture::createDecoder();
    {
        auto span = decoder->template decode<std::span<const uint8_t>>();
        ASSERT_TRUE(!!span);
        ASSERT_EQ(data8.size(), span->size());
        ASSERT_EQ(data8.size() * sizeof(uint8_t), span->size_bytes());
        ASSERT_EQ(memcmp(data8.data(), span->data(), span->size_bytes()), 0);
    }
    {
        auto span = decoder->template decode<std::span<const uint32_t>>();
        ASSERT_TRUE(!!span);
        ASSERT_EQ(data32.size(), span->size());
        ASSERT_EQ(data32.size() * sizeof(uint32_t), span->size_bytes());
        ASSERT_EQ(memcmp(data32.data(), span->data(), span->size_bytes()), 0);
    }
    {
        auto span = decoder->template decode<std::span<const uint64_t>>();
        ASSERT_TRUE(!!span);
        ASSERT_EQ(span->data(), nullptr);
        ASSERT_EQ(span->size(), 0u);
    }
}

template<typename T, size_t S>
struct EncodedValue {
    using Type = T;
    static constexpr size_t Size = S;
};

template<typename EncodedValueType, typename... EncodedValueTypes>
static size_t calculateEncodedSize(size_t value, EncodedValueType, EncodedValueTypes...)
{
    value = roundUpToMultipleOf<alignof(typename EncodedValueType::Type)>(value) + EncodedValueType::Size * sizeof(typename EncodedValueType::Type);
    if constexpr (sizeof...(EncodedValueTypes) > 0)
        return calculateEncodedSize(value, EncodedValueTypes { }...);
    else
        return value;
}

struct alignas(16) AlignedStructure { };

TYPED_TEST_P(ArgumentCoderSpanTest, AlignedSpan)
{
    ASSERT_EQ(alignof(AlignedStructure), 16u);

    auto& encoder = TestFixture::encoder();
    {
        // This one byte will misalign the encoded data, making proper aligning of AlignedStructure properly testable.
        encoder << static_cast<uint8_t>(42);
        ASSERT_EQ(TestFixture::encoderSize(), calculateEncodedSize(TestFixture::headerSize(), EncodedValue<uint8_t, 1> { }));
        ASSERT_TRUE(!!(TestFixture::encoderSize() % alignof(AlignedStructure)));
    }
    {
        // Span over the array data is encoded. Encoded data now includes the header, the previous byte, the span size, and array data.
        std::array<AlignedStructure, 2> alignedData { AlignedStructure { }, AlignedStructure { } };
        encoder << std::span<const AlignedStructure> { alignedData };
        ASSERT_EQ(TestFixture::encoderSize(), calculateEncodedSize(TestFixture::headerSize(),
            EncodedValue<uint8_t, 1> { }, EncodedValue<uint64_t, 1> { }, EncodedValue<AlignedStructure, 2> { }));
    }

    auto decoder = TestFixture::createDecoder();
    ASSERT_EQ(decoder->currentBufferOffset(), TestFixture::headerSize());
    {
        auto byte = decoder->template decode<uint8_t>();
        ASSERT_TRUE(!!byte);
        ASSERT_EQ(*byte, 42);
        ASSERT_EQ(decoder->currentBufferOffset(), calculateEncodedSize(TestFixture::headerSize(), EncodedValue<uint8_t, 1> { }));
    }
    {
        auto alignedData = decoder->template decode<std::span<const AlignedStructure>>();
        ASSERT_TRUE(!!alignedData);
        ASSERT_NE(alignedData->data(), nullptr);
        ASSERT_EQ(alignedData->size(), 2u);
        ASSERT_EQ(decoder->currentBufferOffset(), calculateEncodedSize(TestFixture::headerSize(),
            EncodedValue<uint8_t, 1> { }, EncodedValue<uint64_t, 1> { }, EncodedValue<AlignedStructure, 2> { }));
    }
}

TYPED_TEST_P(ArgumentCoderSpanTest, AlignedEmptySpan)
{
    ASSERT_EQ(alignof(AlignedStructure), 16u);

    auto& encoder = TestFixture::encoder();
    {
        // Only data about the empty span that's encoded is the 64-bit size value, and nothing more.
        encoder << std::span<const AlignedStructure> { };
        ASSERT_EQ(TestFixture::encoderSize(), calculateEncodedSize(TestFixture::headerSize(), EncodedValue<uint64_t, 1> { }));
    }
    {
        // Another byte is encoded tightly after the span size.
        encoder << static_cast<uint8_t>(42);
        ASSERT_EQ(TestFixture::encoderSize(), calculateEncodedSize(TestFixture::headerSize(), EncodedValue<uint64_t, 1> { }, EncodedValue<uint8_t, 1> { }));
    }

    auto decoder = TestFixture::createDecoder();
    ASSERT_EQ(decoder->currentBufferOffset(), TestFixture::headerSize());
    {
        // A valid but empty span should be decoded, meaning a null data pointer and 0 size.
        auto alignedData = decoder->template decode<std::span<const AlignedStructure>>();
        ASSERT_TRUE(!!alignedData);
        ASSERT_EQ(alignedData->data(), nullptr);
        ASSERT_EQ(alignedData->size(), 0u);
        ASSERT_EQ(decoder->currentBufferOffset(), calculateEncodedSize(TestFixture::headerSize(), EncodedValue<uint64_t, 1> { }));
    }
    {
        auto byte = decoder->template decode<uint8_t>();
        ASSERT_TRUE(!!byte);
        ASSERT_EQ(*byte, 42);
        ASSERT_EQ(decoder->currentBufferOffset(), calculateEncodedSize(TestFixture::headerSize(), EncodedValue<uint64_t, 1> { }, EncodedValue<uint8_t, 1> { }));
    }
}

REGISTER_TYPED_TEST_SUITE_P(ArgumentCoderSpanTest,
    SimpleSpan, AlignedSpan, AlignedEmptySpan);
INSTANTIATE_TYPED_TEST_SUITE_P(ArgumentCoderTest, ArgumentCoderSpanTest, EncoderTypes, EncoderTypeNames);

template<typename T> class ArgumentCoderVectorTest : public ArgumentCoderEncoderDecoderTest<T> { };
TYPED_TEST_SUITE_P(ArgumentCoderVectorTest);

TYPED_TEST_P(ArgumentCoderVectorTest, VectorTooBig)
{
    std::array<uint8_t, 9> bytes { 255, 255, 255, 255, 255, 255, 255, 255, 255 };
    TestFixture::encoder() << std::span<uint8_t, 9>(bytes);
    auto optionalVector = TestFixture::createDecoder()->template decode<Vector<String>>();
    ASSERT_FALSE(optionalVector);
}

REGISTER_TYPED_TEST_SUITE_P(ArgumentCoderVectorTest,
    VectorTooBig);
INSTANTIATE_TYPED_TEST_SUITE_P(ArgumentCoderTest, ArgumentCoderVectorTest, EncoderTypes, EncoderTypeNames);

} // namespace TestWebKitAPI
