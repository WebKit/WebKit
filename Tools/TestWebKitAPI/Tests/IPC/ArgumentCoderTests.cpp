/*
 * Copyright (C) 2022 Igalia S.L.
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
#include "Test.h"

namespace TestWebKitAPI {

struct EncodingCounter {
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
        encoder << uint32_t(0);
        ++m_counterValues.encodingLValue;
    }

    void encode(IPC::Encoder& encoder) && {
        encoder << uint32_t(0);
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

INSTANTIATE_TEST_SUITE_P(ArgumentCoderTest,
    ArgumentCoderEncodingCounterTest,
    testing::Values(EncodingCounterTestType::LValue, EncodingCounterTestType::RValue, EncodingCounterTestType::MovedRValue),
    TestParametersToStringFormatter());

} // namespace TestWebKitAPI
