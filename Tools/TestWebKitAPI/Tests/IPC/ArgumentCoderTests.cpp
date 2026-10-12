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
#include "EditingRange.h"
#include "Encoder.h"
#include "GeneratedSerializers.h"
#include "Helpers/Test.h"
#include "SecurityFlags.h"
#include "StreamConnectionEncoder.h"
#include "UnsafeSpan.h"
#include <WebCore/ShareableResource.h>
#include <WebCore/TransformationMatrix.h>
#include <limits>
#include <wtf/StdLibExtras.h>

namespace TestWebKitAPI {

// ArgumentCoderEncoderDecoderTest internally constructs the encoder object specified
// as the template parameter, into which each test can encode the desired objects.
// It then uses that Encoder's internal buffer to create a Decoder object, intended for
// the test to decode those objects.

struct EncoderDecoderTest {
    static constexpr IPC::MessageName name()  { return IPC::MessageName::IPCTester_EmptyMessage; }
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
        ASSERT_EQ(m_encoder->span().size(), headerSize());
    }

    IPC::Encoder& encoder() const { return *m_encoder; }
    size_t headerSize() const { return 16; }
    size_t encoderSize() const { return m_encoder->span().size(); }

    std::unique_ptr<IPC::Decoder> createDecoder() const
    {
        return IPC::Decoder::create(m_encoder->span(), { });
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
        auto decoder = makeUnique<IPC::Decoder>(m_impl->buffer.span(), 0);
        return decoder;
    }

private:
    struct Impl {
        WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(Impl);

        Impl()
            : buffer(FillWith { }, 1024, static_cast<uint8_t>(0))
            , encoder(EncoderDecoderTest::name(), buffer.mutableSpan())
        { }

        Vector<uint8_t> buffer;
        IPC::StreamConnectionEncoder encoder;
    };
    std::unique_ptr<Impl> m_impl;
};


struct EncodingCounter {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(EncodingCounter);

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

} // namespace TestWebKitAPI

namespace IPC {

template<> struct ArgumentCoder<TestWebKitAPI::EncodingCounter> {
    static void encode(Encoder& encoder, const TestWebKitAPI::EncodingCounter& counter)
    {
        counter.encode(encoder);
    }
    static void encode(Encoder& encoder, TestWebKitAPI::EncodingCounter&& counter)
    {
        WTF::move(counter).encode(encoder);
    }
};

} // namespace IPC

namespace TestWebKitAPI {

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
            m_encoder << WTF::move(object);

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
            return Variant<EncodingCounter, unsigned> { EncodingCounter { counterValues } };
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
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(DecodingMoveCounter);

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

} // namespace TestWebKitAPI

namespace IPC {

template<> struct ArgumentCoder<TestWebKitAPI::DecodingMoveCounter> {
    template<typename Encoder>
    static void encode(Encoder& encoder, TestWebKitAPI::DecodingMoveCounter&& counter)
    {
        WTF::move(counter).encode(encoder);
    }
    static std::optional<TestWebKitAPI::DecodingMoveCounter> decode(Decoder& decoder)
    {
        return TestWebKitAPI::DecodingMoveCounter::decode(decoder);
    }
};

} // namespace IPC

namespace TestWebKitAPI {

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
    using VariantType = Variant<DecodingMoveCounter, uint64_t>;
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
        ASSERT_TRUE(equalSpans(std::span { data8 }, *span));
    }
    {
        auto span = decoder->template decode<std::span<const uint32_t>>();
        ASSERT_TRUE(!!span);
        ASSERT_EQ(data32.size(), span->size());
        ASSERT_EQ(data32.size() * sizeof(uint32_t), span->size_bytes());
        ASSERT_TRUE(equalSpans(std::span { data32 }, *span));
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

TEST(ArgumentCoderEditingRange, OverflowingRangeIsRejectedNotCrashed)
{
    IPC::Encoder encoder(IPC::MessageName::IPCTester_EmptyMessage, 0);
    encoder << WebKit::EditingRange { std::numeric_limits<uint64_t>::max(), 1 };

    auto decoder = IPC::Decoder::create(encoder.span(), { });
    ASSERT_TRUE(decoder);

    auto editingRange = decoder->decode<WebKit::EditingRange>();
    EXPECT_FALSE(editingRange.has_value());
    EXPECT_FALSE(decoder->isValid());
}

TEST(EditingRangeFirstLineClamp, NeverProducesOverflowingRange)
{
    constexpr auto max = std::numeric_limits<uint64_t>::max();

    EXPECT_TRUE(WebKit::EditingRange::clampedFirstLineRange({ 0, 5 }, { max, 0 }).isValid());
    EXPECT_TRUE(WebKit::EditingRange::clampedFirstLineRange({ 100, 10 }, { max - 2, 0 }).isValid());
    EXPECT_TRUE(WebKit::EditingRange::clampedFirstLineRange({ max, max }, { max, max }).isValid());
    EXPECT_TRUE(WebKit::EditingRange::clampedFirstLineRange({ max - 3, 10 }, { 5, max }).isValid());

    auto unchanged = WebKit::EditingRange::clampedFirstLineRange({ 5, 3 }, { 2, 20 });
    EXPECT_EQ(unchanged.location, 5u);
    EXPECT_EQ(unchanged.length, 3u);

    auto trimmed = WebKit::EditingRange::clampedFirstLineRange({ 5, 100 }, { 2, 10 });
    EXPECT_TRUE(trimmed.isValid());
    EXPECT_EQ(trimmed.location, 5u);
    EXPECT_EQ(trimmed.location + trimmed.length, 12u);
}

#if ENABLE(SHAREABLE_RESOURCE)
TEST(ArgumentCoderShareableResourceHandle, OverflowingOffsetAndSizeIsRejectedNotCrashed)
{
    auto sharedMemory = WebCore::SharedMemory::allocate(4096);
    ASSERT_TRUE(sharedMemory);
    auto memoryHandle = sharedMemory->createHandle(WebCore::SharedMemory::Protection::ReadOnly);
    ASSERT_TRUE(memoryHandle);

    WebCore::ShareableResourceHandle handle { WTF::move(*memoryHandle), std::numeric_limits<unsigned>::max(), 1 };

    IPC::Encoder encoder(IPC::MessageName::IPCTester_EmptyMessage, 0);
    encoder << WTF::move(handle);

    auto decoder = IPC::Decoder::create(encoder.span(), encoder.releaseAttachments());
    ASSERT_TRUE(decoder);

    auto result = decoder->decode<WebCore::ShareableResourceHandle>();
    EXPECT_FALSE(result.has_value());
}
#endif // ENABLE(SHAREABLE_RESOURCE)

TEST(ArgumentCoderUnsafeArguments, UnwrapToDeclaredTypes)
{
    std::array<uint32_t, 4> data { 1, 2, 3, 4 };
    std::array<float, 4> floats { 5, 6, 7, 8 };
    std::tuple<IPC::UnsafeSpan<uint32_t>, std::optional<IPC::UnsafeSpan<uint32_t>>, IPC::UnsafeArrayReferenceTuple<uint32_t, float>, int> decoded {
        std::span<const uint32_t> { data },
        IPC::UnsafeSpan<uint32_t> { std::span<const uint32_t> { data }.first(2) },
        IPC::UnsafeArrayReferenceTuple<uint32_t, float> { data.data(), floats.data(), data.size() },
        9
    };

    auto unwrapped = IPC::unwrapUnsafeArguments(WTF::move(decoded));
    static_assert(std::is_same_v<decltype(unwrapped), std::tuple<std::span<const uint32_t>, std::optional<std::span<const uint32_t>>, IPC::ArrayReferenceTuple<uint32_t, float>, int>>);
    EXPECT_EQ(std::get<0>(unwrapped).data(), data.data());
    EXPECT_EQ(std::get<0>(unwrapped).size(), 4u);
    ASSERT_TRUE(std::get<1>(unwrapped));
    EXPECT_EQ(std::get<1>(unwrapped)->size(), 2u);
    EXPECT_EQ(std::get<2>(unwrapped).data<1>(), floats.data());
    EXPECT_EQ(std::get<2>(unwrapped).size(), 4u);
    EXPECT_EQ(std::get<3>(unwrapped), 9);
}

TEST(ArgumentCoderSecurityFlags, SecureDefaultRoundTrips)
{
    IPC::Encoder encoder(IPC::MessageName::IPCTester_EmptyMessage, 0);
    encoder << WebKit::SecurityFlags { };

    auto decoder = IPC::Decoder::create(encoder.span(), { });
    ASSERT_TRUE(decoder);

    auto flags = decoder->decode<WebKit::SecurityFlags>();
    ASSERT_TRUE(flags.has_value());
    EXPECT_TRUE(*flags == WebKit::SecurityFlags { });
}

TEST(ArgumentCoderSecurityFlags, DisabledFlagRoundTrips)
{
    std::array<uint64_t, WebKit::SecurityFlags::ipcWordCount> words { };
    words[0] |= 1ULL;
    auto original = WebKit::SecurityFlags::fromDisabledFlagsForIPC(words);
    ASSERT_TRUE(original.has_value());
    EXPECT_FALSE(*original == WebKit::SecurityFlags { });

    IPC::Encoder encoder(IPC::MessageName::IPCTester_EmptyMessage, 0);
    encoder << *original;

    auto decoder = IPC::Decoder::create(encoder.span(), { });
    ASSERT_TRUE(decoder);

    auto flags = decoder->decode<WebKit::SecurityFlags>();
    ASSERT_TRUE(flags.has_value());
    EXPECT_TRUE(*flags == *original);
    EXPECT_EQ(flags->disabledFlagsForIPC(), words);
}

TEST(ArgumentCoderSecurityFlags, BitBelongingToNoFlagIsRejectedNotCrashed)
{
    // A flag count that fills the last word leaves no bit for this test to set, and flagCount / 64 would then
    // index one past the end.
    if constexpr (!(WebKit::SecurityFlags::flagCount % 64))
        GTEST_SKIP() << "every bit belongs to a flag at this flag count";
    else {
        std::array<uint64_t, WebKit::SecurityFlags::ipcWordCount> words { };
        words[WebKit::SecurityFlags::flagCount / 64] |= 1ULL << (WebKit::SecurityFlags::flagCount % 64);

        IPC::Encoder encoder(IPC::MessageName::IPCTester_EmptyMessage, 0);
        encoder << words;

        auto decoder = IPC::Decoder::create(encoder.span(), { });
        ASSERT_TRUE(decoder);

        auto flags = decoder->decode<WebKit::SecurityFlags>();
        EXPECT_FALSE(flags.has_value());
    }
}

template<size_t expectedDoubleCount>
static void testTransformationMatrixCoding(const WebCore::TransformationMatrix& matrix)
{
    IPC::Encoder encoder(IPC::MessageName::IPCTester_EmptyMessage, 0);
    encoder << matrix;

    constexpr size_t messageHeaderSize = 16;
    size_t expectedSize = calculateEncodedSize(messageHeaderSize, EncodedValue<uint8_t, 1> { });
    if constexpr (expectedDoubleCount)
        expectedSize = calculateEncodedSize(expectedSize, EncodedValue<double, expectedDoubleCount> { });
    EXPECT_EQ(expectedSize, encoder.span().size());

    auto decoder = IPC::Decoder::create(encoder.span(), { });
    ASSERT_TRUE(decoder);
    auto decoded = decoder->decode<WebCore::TransformationMatrix>();
    ASSERT_TRUE(decoded.has_value());

    EXPECT_TRUE(matrix == *decoded);
}

TEST(ArgumentCoderTransformationMatrix, IdentityCostsOnlyTheVariantTag)
{
    testTransformationMatrixCoding<0>(WebCore::TransformationMatrix { });
}

TEST(ArgumentCoderTransformationMatrix, Translation2DCostsTwoDoubles)
{
    testTransformationMatrixCoding<2>(WebCore::TransformationMatrix { 10.5, -20.25 });

    WebCore::TransformationMatrix zeroZ;
    zeroZ.translate3d(1.0, 2.0, 0.0);
    testTransformationMatrixCoding<2>(zeroZ);
}

TEST(ArgumentCoderTransformationMatrix, Translation3DCostsThreeDoubles)
{
    WebCore::TransformationMatrix translated3D;
    translated3D.translate3d(1.0, 2.0, 3.0);
    testTransformationMatrixCoding<3>(translated3D);
}

TEST(ArgumentCoderTransformationMatrix, AffineCostsSixDoubles)
{
    testTransformationMatrixCoding<6>(WebCore::TransformationMatrix { 6.0, 5.0, 4.0, 3.0, 2.0, 1.0 });
}

TEST(ArgumentCoderTransformationMatrix, NonAffineCostsSixteenDoubles)
{
    testTransformationMatrixCoding<16>(WebCore::TransformationMatrix {
        16.0, 15.0, 14.0, 13.0,
        12.0, 11.0, 10.0, 9.0,
        8.0, 7.0, 6.0, 5.0,
        4.0, 3.0, 2.0, 1.0
    });
}

static bool spanAliasesBuffer(std::span<const uint8_t> buffer, const void* data)
{
    auto* bytes = static_cast<const uint8_t*>(data);
    return bytes >= buffer.data() && bytes < std::to_address(buffer.end());
}

TEST(ArgumentCoderStreamSpanCopy, PlainSpanIsCopiedOffStreamBuffer)
{
    Vector<uint8_t> buffer(FillWith { }, 1024, static_cast<uint8_t>(0));
    IPC::StreamConnectionEncoder encoder(EncoderDecoderTest::name(), buffer.mutableSpan());
    std::array<uint32_t, 4> data { 1, 2, 3, 4 };
    encoder << std::span<const uint32_t> { data };

    auto decoder = makeUnique<IPC::Decoder>(buffer.span(), 0);
    ASSERT_TRUE(decoder->shouldCopySpans());
    auto span = decoder->decode<std::span<const uint32_t>>();
    ASSERT_TRUE(!!span);
    ASSERT_EQ(span->size(), 4u);

    EXPECT_FALSE(spanAliasesBuffer(decoder->span(), span->data()));

    for (auto& byte : buffer.mutableSpan())
        byte = 0xff;
    EXPECT_EQ((*span)[0], 1u);
    EXPECT_EQ((*span)[1], 2u);
    EXPECT_EQ((*span)[2], 3u);
    EXPECT_EQ((*span)[3], 4u);
}

TEST(ArgumentCoderStreamSpanCopy, UnsafeSpanViewsStreamBuffer)
{
    Vector<uint8_t> buffer(FillWith { }, 1024, static_cast<uint8_t>(0));
    IPC::StreamConnectionEncoder encoder(EncoderDecoderTest::name(), buffer.mutableSpan());
    std::array<uint32_t, 4> data { 1, 2, 3, 4 };
    encoder << IPC::UnsafeSpan<uint32_t> { std::span<const uint32_t> { data } };

    auto decoder = makeUnique<IPC::Decoder>(buffer.span(), 0);
    ASSERT_TRUE(decoder->shouldCopySpans());
    auto span = decoder->decode<IPC::UnsafeSpan<uint32_t>>();
    ASSERT_TRUE(!!span);
    ASSERT_EQ(span->size(), 4u);

    EXPECT_TRUE(spanAliasesBuffer(decoder->span(), span->data()));

    for (auto& byte : buffer.mutableSpan())
        byte = 0xff;
    EXPECT_EQ((*span)[0], 0xffffffffu);
    EXPECT_EQ((*span)[3], 0xffffffffu);
}

TEST(ArgumentCoderStreamSpanCopy, NonStreamPlainSpanIsNotCopied)
{
    IPC::Encoder encoder(EncoderDecoderTest::name(), 0);
    std::array<uint32_t, 4> data { 1, 2, 3, 4 };
    encoder << std::span<const uint32_t> { data };

    auto decoder = IPC::Decoder::create(encoder.span(), { });
    ASSERT_TRUE(decoder);
    ASSERT_FALSE(decoder->shouldCopySpans());
    auto span = decoder->decode<std::span<const uint32_t>>();
    ASSERT_TRUE(!!span);
    ASSERT_EQ(span->size(), 4u);

    EXPECT_TRUE(spanAliasesBuffer(decoder->span(), span->data()));
    EXPECT_EQ((*span)[0], 1u);
    EXPECT_EQ((*span)[3], 4u);
}

TEST(ArgumentCoderStreamSpanCopy, SpansWithSmallElementAlignmentAreCopied)
{
    Vector<uint8_t> buffer(FillWith { }, 1024, static_cast<uint8_t>(0));
    IPC::StreamConnectionEncoder encoder(EncoderDecoderTest::name(), buffer.mutableSpan());
    std::array<uint8_t, 3> bytes { 1, 2, 3 };
    std::array<float, 3> floats { 1.5f, 2.5f, 3.5f };
    std::array<uint8_t, 3> tupleBytes { 4, 5, 6 };
    std::array<float, 3> tupleFloats { 4.5f, 5.5f, 6.5f };
    encoder << std::span<const uint8_t> { bytes } << std::span<const float> { floats } << IPC::ArrayReferenceTuple<uint8_t, float> { tupleBytes.data(), tupleFloats.data(), tupleBytes.size() };

    auto decoder = makeUnique<IPC::Decoder>(buffer.span(), 0);
    ASSERT_TRUE(decoder->shouldCopySpans());

    auto byteSpan = decoder->decode<std::span<const uint8_t>>();
    ASSERT_TRUE(!!byteSpan);
    ASSERT_EQ(byteSpan->size(), 3u);
    EXPECT_FALSE(spanAliasesBuffer(decoder->span(), byteSpan->data()));
    EXPECT_EQ((*byteSpan)[0], 1u);
    EXPECT_EQ((*byteSpan)[2], 3u);

    auto floatSpan = decoder->decode<std::span<const float>>();
    ASSERT_TRUE(!!floatSpan);
    ASSERT_EQ(floatSpan->size(), 3u);
    EXPECT_FALSE(spanAliasesBuffer(decoder->span(), floatSpan->data()));
    EXPECT_EQ(reinterpret_cast<uintptr_t>(floatSpan->data()) % alignof(float), 0u);
    EXPECT_EQ((*floatSpan)[0], 1.5f);
    EXPECT_EQ((*floatSpan)[2], 3.5f);

    auto tuple = decoder->decode<IPC::ArrayReferenceTuple<uint8_t, float>>();
    ASSERT_TRUE(!!tuple);
    ASSERT_EQ(tuple->size(), 3u);
    auto tupleByteSpan = tuple->span<0>();
    auto tupleFloatSpan = tuple->span<1>();
    EXPECT_FALSE(spanAliasesBuffer(decoder->span(), tupleByteSpan.data()));
    EXPECT_FALSE(spanAliasesBuffer(decoder->span(), tupleFloatSpan.data()));
    EXPECT_EQ(reinterpret_cast<uintptr_t>(tupleFloatSpan.data()) % alignof(float), 0u);
    EXPECT_EQ(tupleByteSpan[0], 4u);
    EXPECT_EQ(tupleByteSpan[2], 6u);
    EXPECT_EQ(tupleFloatSpan[0], 4.5f);
    EXPECT_EQ(tupleFloatSpan[2], 6.5f);
}

} // namespace TestWebKitAPI
