/*
 * Copyright (C) 2011, 2015 Apple Inc. All rights reserved.
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
#include <wtf/CheckedArithmetic.h>
#include <wtf/Int128.h>

namespace TestWebKitAPI {

class OverflowCrashLogger {
protected:
    void overflowed()
    {
        m_overflowCount++;
    }
    
    void clearOverflow()
    {
        m_overflowCount = 0;
    }
    
    static void crash()
    {
        s_didCrash = true;
    }
    
public:
    void reset()
    {
        m_overflowCount = 0;
        s_didCrash = false;
    }
    
    bool hasOverflowed() const { return m_overflowCount > 0; }
    int overflowCount() const { return m_overflowCount; }

    bool didCrash() const { return s_didCrash; }
    
private:
    int m_overflowCount { 0 };
    static bool s_didCrash;
};

bool OverflowCrashLogger::s_didCrash = false;

template <typename type>
static void resetOverflow(Checked<type, OverflowCrashLogger>& value)
{
    value.reset();
    value = 100;
    value *= std::numeric_limits<type>::max();
}

#define CheckedArithmeticTest(type, Coercer, MixedSignednessTester) \
    TEST(WTF, Checked_##type) \
    { \
        typedef Coercer<type> CoercerType; \
        typedef MixedSignednessTester<type, CoercerType> MixedSignednessTesterType; \
        CheckedArithmeticTester<type, CoercerType, MixedSignednessTesterType>::run(); \
    }
    
#define coerceLiteral(x) Coercer::coerce(x)
    
template <typename type, typename Coercer, typename MixedSignednessTester>
class CheckedArithmeticTester {
public:
    static void run()
    {
        Checked<type, RecordOverflow> value;
        EXPECT_EQ(coerceLiteral(0), value.value());
        EXPECT_EQ(std::numeric_limits<type>::max(), (value + std::numeric_limits<type>::max()).value());
        EXPECT_EQ(std::numeric_limits<type>::max(), (std::numeric_limits<type>::max() + value).value());
        EXPECT_EQ(std::numeric_limits<type>::min(), (value + std::numeric_limits<type>::min()).value());
        EXPECT_EQ(std::numeric_limits<type>::min(), (std::numeric_limits<type>::min() + value).value());

        EXPECT_EQ(coerceLiteral(0), (value * coerceLiteral(0)).value());
        EXPECT_EQ(coerceLiteral(0), (coerceLiteral(0) * value).value());
        EXPECT_EQ(coerceLiteral(0), (value * value).value());
        EXPECT_EQ(coerceLiteral(0), (value - coerceLiteral(0)).value());
        EXPECT_EQ(coerceLiteral(0), (coerceLiteral(0) - value).value());
        EXPECT_EQ(coerceLiteral(0), (value - value).value());
        EXPECT_EQ(coerceLiteral(0), (value++).value());
        EXPECT_EQ(coerceLiteral(1), (value--).value());
        EXPECT_EQ(coerceLiteral(1), (++value).value());
        EXPECT_EQ(coerceLiteral(0), (--value).value());
        EXPECT_EQ(coerceLiteral(10), (value += coerceLiteral(10)).value());
        EXPECT_EQ(coerceLiteral(10), value.value());
        EXPECT_EQ(coerceLiteral(100), (value *= coerceLiteral(10)).value());
        EXPECT_EQ(coerceLiteral(100), value.value());
        EXPECT_EQ(coerceLiteral(0), (value -= coerceLiteral(100)).value());
        EXPECT_EQ(coerceLiteral(0), value.value());
        value = 10;
        EXPECT_EQ(coerceLiteral(10), value.value());
        EXPECT_EQ(coerceLiteral(0), (value - coerceLiteral(10)).value());
        EXPECT_EQ(coerceLiteral(10), value.value());

        value = std::numeric_limits<type>::min();
        EXPECT_EQ(true, (Checked<type, RecordOverflow>(value - coerceLiteral(1))).hasOverflowed());
        EXPECT_EQ(true, !((value--).hasOverflowed()));
        EXPECT_EQ(true, value.hasOverflowed());
        value = std::numeric_limits<type>::max();
        EXPECT_EQ(true, !value.hasOverflowed());
        EXPECT_EQ(true, (Checked<type, RecordOverflow>(value + coerceLiteral(1))).hasOverflowed());
        EXPECT_EQ(true, !(value++).hasOverflowed());
        EXPECT_EQ(true, value.hasOverflowed());
        value = std::numeric_limits<type>::max();
        EXPECT_EQ(true, (value += coerceLiteral(1)).hasOverflowed());
        EXPECT_EQ(true, value.hasOverflowed());

        value = 10;
        EXPECT_FALSE((value * Checked<type, RecordOverflow>(0)).hasOverflowed());
        EXPECT_FALSE((Checked<type, RecordOverflow>(0) * value).hasOverflowed());
        EXPECT_TRUE((value * Checked<type, RecordOverflow>(std::numeric_limits<type>::max())).hasOverflowed());
        EXPECT_TRUE((Checked<type, RecordOverflow>(std::numeric_limits<type>::max()) * value).hasOverflowed());
        value = 0;
        EXPECT_FALSE((value * Checked<type, RecordOverflow>(std::numeric_limits<type>::max())).hasOverflowed());
        EXPECT_FALSE((Checked<type, RecordOverflow>(std::numeric_limits<type>::max()) * value).hasOverflowed());
        value = 1;
        EXPECT_FALSE((value * Checked<type, RecordOverflow>(std::numeric_limits<type>::max())).hasOverflowed());
        EXPECT_FALSE((Checked<type, RecordOverflow>(std::numeric_limits<type>::max()) * value).hasOverflowed());
        EXPECT_FALSE((value * Checked<type, RecordOverflow>(std::numeric_limits<type>::max())).hasOverflowed());
        EXPECT_FALSE((Checked<type, RecordOverflow>(std::numeric_limits<type>::max()) * (type)0).hasOverflowed());
        value = 1;
        EXPECT_FALSE((value * Checked<type, RecordOverflow>(std::numeric_limits<type>::max())).hasOverflowed());
        EXPECT_FALSE((Checked<type, RecordOverflow>(std::numeric_limits<type>::max()) * (type)1).hasOverflowed());
        value = 2;
        EXPECT_TRUE((value * Checked<type, RecordOverflow>(std::numeric_limits<type>::max())).hasOverflowed());
        EXPECT_TRUE((Checked<type, RecordOverflow>(std::numeric_limits<type>::max()) * (type)2).hasOverflowed());
        value = 10;
        EXPECT_TRUE((value * Checked<type, RecordOverflow>(std::numeric_limits<type>::max())).hasOverflowed());


        Checked<type, OverflowCrashLogger> nvalue; // to hold a not overflowed value.
        Checked<type, OverflowCrashLogger> ovalue; // to hold an overflowed value.

        bool unused;
        UNUSED_PARAM(unused);

        type _value = 75;
        type _largeValue = 100;
        type _smallValue = 50;

        value = _smallValue;
        nvalue = _value;
        ovalue = _value;

        // Make sure the OverflowCrashLogger is working as expected.
        EXPECT_EQ(false, (ovalue.hasOverflowed()));
        EXPECT_EQ(true, (resetOverflow(ovalue), ovalue.hasOverflowed()));
        EXPECT_EQ(false, (resetOverflow(ovalue), ovalue.didCrash()));
        EXPECT_EQ(true, (unused = (ovalue == ovalue), ovalue.didCrash()));
        EXPECT_EQ(false, (resetOverflow(ovalue), ovalue.didCrash()));

        EXPECT_EQ(false, nvalue.hasOverflowed());
        EXPECT_EQ(false, nvalue.didCrash());

        // Test operator== that should not overflow nor crash.
        EXPECT_EQ(true, (nvalue == nvalue));
        EXPECT_EQ(true, (nvalue == Checked<type, OverflowCrashLogger>(_value)));
        EXPECT_EQ(false, (nvalue == value));
        EXPECT_EQ(true, (nvalue == _value));
        EXPECT_EQ(false, (nvalue == Checked<type, OverflowCrashLogger>(std::numeric_limits<type>::max())));
        EXPECT_EQ(false, (nvalue == std::numeric_limits<type>::max()));

        EXPECT_EQ(false, nvalue.hasOverflowed());
        EXPECT_EQ(false, nvalue.didCrash());

        // Test operator!= that should not overflow nor crash.
        EXPECT_EQ(false, (nvalue != nvalue));
        EXPECT_EQ(false, (nvalue != Checked<type, OverflowCrashLogger>(_value)));
        EXPECT_EQ(true, (nvalue != value));
        EXPECT_EQ(false, (nvalue != _value));
        EXPECT_EQ(true, (nvalue != Checked<type, OverflowCrashLogger>(std::numeric_limits<type>::max())));
        EXPECT_EQ(true, (nvalue != std::numeric_limits<type>::max()));

        EXPECT_EQ(false, nvalue.hasOverflowed());
        EXPECT_EQ(false, nvalue.didCrash());

        // Test operator< that should not overflow nor crash.
        EXPECT_EQ(false, (nvalue < nvalue));
        EXPECT_EQ(false, (nvalue < value));
        EXPECT_EQ(true, (nvalue < Checked<type, OverflowCrashLogger>(_largeValue)));
        EXPECT_EQ(false, (nvalue < Checked<type, OverflowCrashLogger>(_value)));
        EXPECT_EQ(false, (nvalue < Checked<type, OverflowCrashLogger>(_smallValue)));
        EXPECT_EQ(true, (nvalue < _largeValue));
        EXPECT_EQ(false, (nvalue < _value));
        EXPECT_EQ(false, (nvalue < _smallValue));
        EXPECT_EQ(true, (nvalue < Checked<type, OverflowCrashLogger>(std::numeric_limits<type>::max())));
        EXPECT_EQ(true, (nvalue < std::numeric_limits<type>::max()));

        EXPECT_EQ(false, nvalue.hasOverflowed());
        EXPECT_EQ(false, nvalue.didCrash());

        // Test operator<= that should not overflow nor crash.
        EXPECT_EQ(true, (nvalue <= nvalue));
        EXPECT_EQ(false, (nvalue <= value));
        EXPECT_EQ(true, (nvalue <= Checked<type, OverflowCrashLogger>(_largeValue)));
        EXPECT_EQ(true, (nvalue <= Checked<type, OverflowCrashLogger>(_value)));
        EXPECT_EQ(false, (nvalue <= Checked<type, OverflowCrashLogger>(_smallValue)));
        EXPECT_EQ(true, (nvalue <= _largeValue));
        EXPECT_EQ(true, (nvalue <= _value));
        EXPECT_EQ(false, (nvalue <= _smallValue));
        EXPECT_EQ(true, (nvalue <= Checked<type, OverflowCrashLogger>(std::numeric_limits<type>::max())));
        EXPECT_EQ(true, (nvalue <= std::numeric_limits<type>::max()));

        EXPECT_EQ(false, nvalue.hasOverflowed());
        EXPECT_EQ(false, nvalue.didCrash());

        // Test operator> that should not overflow nor crash.
        EXPECT_EQ(false, (nvalue > nvalue));
        EXPECT_EQ(true, (nvalue > value));
        EXPECT_EQ(false, (nvalue > Checked<type, OverflowCrashLogger>(_largeValue)));
        EXPECT_EQ(false, (nvalue > Checked<type, OverflowCrashLogger>(_value)));
        EXPECT_EQ(true, (nvalue > Checked<type, OverflowCrashLogger>(_smallValue)));
        EXPECT_EQ(false, (nvalue > _largeValue));
        EXPECT_EQ(false, (nvalue > _value));
        EXPECT_EQ(true, (nvalue > _smallValue));
        EXPECT_EQ(false, (nvalue > Checked<type, OverflowCrashLogger>(std::numeric_limits<type>::max())));
        EXPECT_EQ(false, (nvalue > std::numeric_limits<type>::max()));

        EXPECT_EQ(false, nvalue.hasOverflowed());
        EXPECT_EQ(false, nvalue.didCrash());

        // Test operator>= that should not overflow nor crash.
        EXPECT_EQ(true, (nvalue >= nvalue));
        EXPECT_EQ(true, (nvalue >= value));
        EXPECT_EQ(false, (nvalue >= Checked<type, OverflowCrashLogger>(_largeValue)));
        EXPECT_EQ(true, (nvalue >= Checked<type, OverflowCrashLogger>(_value)));
        EXPECT_EQ(true, (nvalue >= Checked<type, OverflowCrashLogger>(_smallValue)));
        EXPECT_EQ(false, (nvalue >= _largeValue));
        EXPECT_EQ(true, (nvalue >= _value));
        EXPECT_EQ(true, (nvalue >= _smallValue));
        EXPECT_EQ(false, (nvalue >= Checked<type, OverflowCrashLogger>(std::numeric_limits<type>::max())));
        EXPECT_EQ(false, (nvalue >= std::numeric_limits<type>::max()));

        EXPECT_EQ(false, nvalue.hasOverflowed());
        EXPECT_EQ(false, nvalue.didCrash());

        // Test operator== with an overflowed value.
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue == ovalue), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue == Checked<type, OverflowCrashLogger>(_value)), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue == value), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue == _value), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue == _value * std::numeric_limits<type>::max()), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue == Checked<type, OverflowCrashLogger>(std::numeric_limits<type>::max())), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue == std::numeric_limits<type>::max()), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue == nvalue), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (nvalue == ovalue), ovalue.didCrash()));

        EXPECT_EQ(false, nvalue.hasOverflowed());

        // Test operator!= with an overflowed value.
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue != ovalue), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue != Checked<type, OverflowCrashLogger>(_value)), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue != value), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue != _value), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue != _value * std::numeric_limits<type>::max()), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue != Checked<type, OverflowCrashLogger>(std::numeric_limits<type>::max())), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue != std::numeric_limits<type>::max()), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue != nvalue), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (nvalue != ovalue), ovalue.didCrash()));

        EXPECT_EQ(false, nvalue.hasOverflowed());

        // Test operator< with an overflowed value.
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue < ovalue), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue < value), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue < Checked<type, OverflowCrashLogger>(_largeValue)), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue < Checked<type, OverflowCrashLogger>(_value)), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue < Checked<type, OverflowCrashLogger>(_smallValue)), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue < _largeValue), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue < _value), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue < _smallValue), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue < Checked<type, OverflowCrashLogger>(std::numeric_limits<type>::max())), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue < std::numeric_limits<type>::max()), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue < nvalue), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (nvalue < ovalue), ovalue.didCrash()));

        EXPECT_EQ(false, nvalue.hasOverflowed());

        // Test operator<= with an overflowed value.
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue <= ovalue), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue <= value), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue <= Checked<type, OverflowCrashLogger>(_largeValue)), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue <= Checked<type, OverflowCrashLogger>(_value)), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue <= Checked<type, OverflowCrashLogger>(_smallValue)), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue <= _largeValue), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue <= _value), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue <= _smallValue), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue <= Checked<type, OverflowCrashLogger>(std::numeric_limits<type>::max())), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue <= std::numeric_limits<type>::max()), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue <= nvalue), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (nvalue <= ovalue), ovalue.didCrash()));

        EXPECT_EQ(false, nvalue.hasOverflowed());

        // Test operator> with an overflowed value.
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue > ovalue), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue > value), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue > Checked<type, OverflowCrashLogger>(_largeValue)), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue > Checked<type, OverflowCrashLogger>(_value)), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue > Checked<type, OverflowCrashLogger>(_smallValue)), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue > _largeValue), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue > _value), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue > _smallValue), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue > Checked<type, OverflowCrashLogger>(std::numeric_limits<type>::max())), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue > std::numeric_limits<type>::max()), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue > nvalue), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (nvalue > ovalue), ovalue.didCrash()));

        EXPECT_EQ(false, nvalue.hasOverflowed());

        // Test operator>= with an overflowed value.
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue >= ovalue), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue >= value), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue >= Checked<type, OverflowCrashLogger>(_largeValue)), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue >= Checked<type, OverflowCrashLogger>(_value)), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue >= Checked<type, OverflowCrashLogger>(_smallValue)), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue >= _largeValue), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue >= _value), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue >= _smallValue), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue >= Checked<type, OverflowCrashLogger>(std::numeric_limits<type>::max())), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue >= std::numeric_limits<type>::max()), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (ovalue >= nvalue), ovalue.didCrash()));
        EXPECT_EQ(true, (resetOverflow(ovalue), unused = (nvalue >= ovalue), ovalue.didCrash()));

        EXPECT_EQ(false, nvalue.hasOverflowed());

        MixedSignednessTester::run();
    }
};

template <typename type, typename Coercer>
class AllowMixedSignednessTest {
public:
    static void run()
    {
        Checked<type, RecordOverflow> value;
        value = 10;

        EXPECT_EQ(coerceLiteral(0), (value + -10).value());
        EXPECT_EQ(0U, (value - 10U).value());
        EXPECT_EQ(coerceLiteral(0), (-10 + value).value());
        EXPECT_EQ(0U, (10U - value).value());
        value = std::numeric_limits<type>::min();
        EXPECT_EQ(true, (Checked<type, RecordOverflow>(value - 1)).hasOverflowed());
        EXPECT_EQ(true, !(value--).hasOverflowed());
        EXPECT_EQ(true, value.hasOverflowed());
        value = std::numeric_limits<type>::max();
        EXPECT_EQ(true, !value.hasOverflowed());
        EXPECT_EQ(true, (Checked<type, RecordOverflow>(value + 1)).hasOverflowed());
        EXPECT_EQ(true, !(value++).hasOverflowed());
        EXPECT_EQ(true, value.hasOverflowed());
        value = std::numeric_limits<type>::max();
        EXPECT_EQ(true, (value += 1).hasOverflowed());
        EXPECT_EQ(true, value.hasOverflowed());
        value = std::numeric_limits<type>::min();
        EXPECT_EQ(true, (value - 1U).hasOverflowed());
        EXPECT_EQ(true, !(value--).hasOverflowed());
        EXPECT_EQ(true, value.hasOverflowed());
        value = std::numeric_limits<type>::max();
        EXPECT_EQ(true, !value.hasOverflowed());
        EXPECT_EQ(true, (Checked<type, RecordOverflow>(value + 1U)).hasOverflowed());
        EXPECT_EQ(true, !(value++).hasOverflowed());
        EXPECT_EQ(true, value.hasOverflowed());
        value = std::numeric_limits<type>::max();
        EXPECT_EQ(true, (value += 1U).hasOverflowed());
        EXPECT_EQ(true, value.hasOverflowed());
        value = std::numeric_limits<type>::max() - 1;
        EXPECT_FALSE((value + 1).hasOverflowed());
        value = std::numeric_limits<type>::min() + 1;
        EXPECT_FALSE((value - 1).hasOverflowed());
    }
};

template <typename type, typename Coercer>
class IgnoreMixedSignednessTest {
public:
    static void run() { }
};

template <typename type> class CoerceLiteralToUnsigned {
public:
    static unsigned coerce(type x) { return static_cast<unsigned>(x); }
};
    
template <typename type> class CoerceLiteralNop {
public:
    static type coerce(type x) { return x; }
};

CheckedArithmeticTest(int8_t, CoerceLiteralNop, IgnoreMixedSignednessTest)
CheckedArithmeticTest(int16_t, CoerceLiteralNop, IgnoreMixedSignednessTest)
CheckedArithmeticTest(int32_t, CoerceLiteralNop, AllowMixedSignednessTest)
CheckedArithmeticTest(uint32_t, CoerceLiteralToUnsigned, AllowMixedSignednessTest)
CheckedArithmeticTest(int64_t, CoerceLiteralNop, IgnoreMixedSignednessTest)
CheckedArithmeticTest(uint64_t, CoerceLiteralToUnsigned, IgnoreMixedSignednessTest)

TEST(CheckedArithmeticTest, IsInBounds)
{
    // bigger precision, signed, signed
    EXPECT_TRUE(isInBounds<int32_t>(std::numeric_limits<int16_t>::max()));
    EXPECT_TRUE(isInBounds<int32_t>(std::numeric_limits<int16_t>::min()));

    // bigger precision, unsigned, signed
    EXPECT_TRUE(isInBounds<uint32_t>(std::numeric_limits<int32_t>::max()));
    EXPECT_FALSE(isInBounds<uint32_t>(std::numeric_limits<int16_t>::min()));

    EXPECT_FALSE(isInBounds<uint32_t>((int32_t)-1));
    EXPECT_FALSE(isInBounds<uint16_t>((int32_t)-1));
    EXPECT_FALSE(isInBounds<unsigned long>((int)-1));

    EXPECT_TRUE(isInBounds<uint32_t>((int32_t)1));
    EXPECT_TRUE(isInBounds<uint32_t>((int16_t)1));
    EXPECT_TRUE(isInBounds<unsigned>((int)1));

    EXPECT_TRUE(isInBounds<uint32_t>((int32_t)0));
    EXPECT_TRUE(isInBounds<uint16_t>((int32_t)0));
    EXPECT_TRUE(isInBounds<uint32_t>((int16_t)0));
    EXPECT_TRUE(isInBounds<unsigned>((int)0));

    EXPECT_TRUE(isInBounds<uint32_t>(std::numeric_limits<int32_t>::max()));
    EXPECT_TRUE(isInBounds<uint32_t>(std::numeric_limits<int16_t>::max()));
    EXPECT_TRUE(isInBounds<unsigned>(std::numeric_limits<int>::max()));

    // bigger precision, signed, unsigned
    EXPECT_TRUE(isInBounds<int32_t>(std::numeric_limits<uint16_t>::max()));
    EXPECT_FALSE(isInBounds<int32_t>(std::numeric_limits<uint32_t>::max()));
    EXPECT_TRUE(isInBounds<int32_t>((uint32_t)0));

    // bigger precision, unsigned, unsigned
    EXPECT_TRUE(isInBounds<uint32_t>(std::numeric_limits<uint16_t>::max()));
    EXPECT_TRUE(isInBounds<uint32_t>(std::numeric_limits<uint16_t>::min()));

    // lower precision, signed signed
    EXPECT_FALSE(isInBounds<int16_t>(std::numeric_limits<int32_t>::max()));
    EXPECT_FALSE(isInBounds<int16_t>(std::numeric_limits<int32_t>::min()));
    EXPECT_TRUE(isInBounds<int16_t>((int32_t)-1));
    EXPECT_TRUE(isInBounds<int16_t>((int32_t)0));
    EXPECT_TRUE(isInBounds<int16_t>((int32_t)1));
    // lower precision, unsigned, signed
    EXPECT_FALSE(isInBounds<uint16_t>(std::numeric_limits<int32_t>::max()));
    EXPECT_FALSE(isInBounds<uint16_t>(std::numeric_limits<int32_t>::min()));
    EXPECT_FALSE(isInBounds<uint16_t>((int32_t)-1));
    EXPECT_TRUE(isInBounds<uint16_t>((int32_t)0));
    EXPECT_TRUE(isInBounds<uint16_t>((int32_t)1));
    // lower precision, signed, unsigned
    EXPECT_FALSE(isInBounds<int16_t>(std::numeric_limits<uint32_t>::max()));
    EXPECT_TRUE(isInBounds<int16_t>((uint32_t)0));
    EXPECT_TRUE(isInBounds<int16_t>((uint32_t)1));
    // lower precision, unsigned, unsigned
    EXPECT_FALSE(isInBounds<uint16_t>(std::numeric_limits<uint32_t>::max()));
    EXPECT_TRUE(isInBounds<uint16_t>((uint32_t)0));
    EXPECT_TRUE(isInBounds<uint16_t>((uint32_t)1));
}

TEST(CheckedArithmeticTest, Division)
{
    CheckedSize size = 100;
    EXPECT_EQ(100U, size.value());
    size /= 10U;
    EXPECT_FALSE(size.hasOverflowed());
    EXPECT_EQ(10U, size.value());

    size /= 11U;
    EXPECT_FALSE(size.hasOverflowed());
    EXPECT_EQ(0U, size.value());

    size = 100;
    EXPECT_FALSE(size.hasOverflowed());

    size /= 0U;
    EXPECT_TRUE(size.hasOverflowed());

    size = 100;
    EXPECT_FALSE(size.hasOverflowed());
    EXPECT_EQ(100U, size.value());
}

// Bug 1: Signed division of MIN / -1 should report overflow.
// INT_MIN / -1 mathematically equals INT_MAX + 1, which doesn't fit in the
// signed type. The divide() function for signed types only checks for a zero
// divisor but misses this case entirely. For int16_t, the C++ compiler promotes
// both operands to int, computes 32768, then the implicit narrowing conversion
// back to int16_t wraps to -32768 — so we get a silently wrong result instead
// of an overflow indication. (For int32_t/int64_t this is worse: the hardware
// idiv instruction traps, causing SIGFPE.)
TEST(CheckedArithmeticTest, SignedDivisionMinByNegativeOneShouldOverflow)
{
    // int8_t: INT8_MIN / -1 = 128, doesn't fit in int8_t (max 127).
    {
        Checked<int8_t, RecordOverflow> a = std::numeric_limits<int8_t>::min();
        Checked<int8_t, RecordOverflow> b = -1;
        auto result = a / b;
        EXPECT_TRUE(result.hasOverflowed());
    }

    // int16_t: INT16_MIN / -1 = 32768, doesn't fit in int16_t (max 32767).
    {
        Checked<int16_t, RecordOverflow> a = std::numeric_limits<int16_t>::min();
        Checked<int16_t, RecordOverflow> b = -1;
        auto result = a / b;
        EXPECT_TRUE(result.hasOverflowed());
    }

    // Also test via operator/=.
    {
        Checked<int16_t, RecordOverflow> a = std::numeric_limits<int16_t>::min();
        a /= static_cast<int16_t>(-1);
        EXPECT_TRUE(a.hasOverflowed());
    }

    // Verify that normal signed divisions still work.
    {
        Checked<int16_t, RecordOverflow> a = 100;
        auto result = a / Checked<int16_t, RecordOverflow>(-1);
        EXPECT_FALSE(result.hasOverflowed());
        EXPECT_EQ(static_cast<int16_t>(-100), result.value());
    }
}

// Bug 2: Mixed-signedness division (int / unsigned) doesn't bounds-check the
// result against the ResultType. The ResultType for <int, unsigned> is unsigned
// (chosen by SignednessSelector). When a negative int is divided by a positive
// unsigned, the int64_t quotient is negative, which cannot be represented in
// unsigned — but the code does a raw static_cast without checking, producing a
// silently wrong value (e.g. UINT_MAX) instead of signaling overflow.
TEST(CheckedArithmeticTest, MixedSignDivisionShouldOverflowWhenResultIsNegative)
{
    // -1 / 1u: mathematically -1, which doesn't fit in unsigned.
    {
        Checked<int, RecordOverflow> a = -1;
        Checked<unsigned, RecordOverflow> b = 1;
        auto result = a / b;
        EXPECT_TRUE(result.hasOverflowed());
    }

    // INT_MIN / 1u: mathematically INT_MIN (negative), doesn't fit in unsigned.
    {
        Checked<int, RecordOverflow> a = std::numeric_limits<int>::min();
        Checked<unsigned, RecordOverflow> b = 1;
        auto result = a / b;
        EXPECT_TRUE(result.hasOverflowed());
    }

    // -10 / 3u: mathematically -3 (truncated toward zero), doesn't fit in unsigned.
    {
        Checked<int, RecordOverflow> a = -10;
        Checked<unsigned, RecordOverflow> b = 3;
        auto result = a / b;
        EXPECT_TRUE(result.hasOverflowed());
    }

    // Verify that non-negative results still work.
    {
        Checked<int, RecordOverflow> a = 10;
        Checked<unsigned, RecordOverflow> b = 3;
        auto result = a / b;
        EXPECT_FALSE(result.hasOverflowed());
        EXPECT_EQ(3U, result.value());
    }

    // 0 / 5u = 0, should succeed.
    {
        Checked<int, RecordOverflow> a = 0;
        Checked<unsigned, RecordOverflow> b = 5;
        auto result = a / b;
        EXPECT_FALSE(result.hasOverflowed());
        EXPECT_EQ(0U, result.value());
    }
}

template<typename T, typename OverflowHandler>
static constexpr Checked<T, OverflowHandler> compoundAssignments(T initial)
{
    Checked<T, OverflowHandler> value = initial;
    value += static_cast<T>(7);
    value -= static_cast<T>(2);
    value *= static_cast<T>(3);
    value /= static_cast<T>(5);
    value |= static_cast<T>(8);
    value &= static_cast<T>(13);
    ++value;
    value++;
    --value;
    value--;
    return value;
}

template<typename T>
static constexpr Checked<T, RecordOverflow> maxChecked()
{
    return std::numeric_limits<T>::max();
}

TEST(CheckedArithmeticTest, Constexpr)
{
    // Construction and conversion.
    static_assert(!Checked<int, RecordOverflow>().value());
    static_assert(Checked<int, RecordOverflow>(WTF::ResultOverflowed).hasOverflowed());
    static_assert(Checked<uint8_t, RecordOverflow>(255).value() == 255);
    static_assert(Checked<uint8_t, RecordOverflow>(256).hasOverflowed());
    static_assert(Checked<uint8_t, RecordOverflow>(-1).hasOverflowed());
    static_assert(Checked<int8_t, RecordOverflow>(Checked<int, RecordOverflow>(-128)).value() == -128);
    static_assert(Checked<int8_t, RecordOverflow>(Checked<int, RecordOverflow>(128)).hasOverflowed());
    static_assert(Checked<int8_t, RecordOverflow>(Checked<int, CrashOnOverflow>(128)).hasOverflowed());
    static_assert(static_cast<int>(Checked<int>(42)) == 42);
    static_assert(Checked<int>(1) && !Checked<int>(0));
    static_assert(!!Checked<int>(1));

    // Compound assignment, increment and decrement, for each overflow handler.
    static_assert(compoundAssignments<int, RecordOverflow>(3).value() == 12);
    static_assert(compoundAssignments<int, CrashOnOverflow>(3).value() == 12);
    static_assert(compoundAssignments<int, AssertNoOverflow>(3).value() == 12);
    static_assert(compoundAssignments<unsigned, RecordOverflow>(3).value() == 12);
    static_assert(compoundAssignments<size_t, CrashOnOverflow>(3).value() == 12);

    // Overflow is recorded by each mutating operation.
    static_assert((maxChecked<int>() += 1).hasOverflowed());
    static_assert((maxChecked<int>() -= -1).hasOverflowed());
    static_assert((maxChecked<int>() *= 2).hasOverflowed());
    static_assert((maxChecked<int>() /= 0).hasOverflowed());
    static_assert((maxChecked<int>() += Checked<int, RecordOverflow>(WTF::ResultOverflowed)).hasOverflowed());
    static_assert((maxChecked<unsigned>() += 1u).hasOverflowed());
    static_assert((maxChecked<unsigned>() *= 2u).hasOverflowed());
    static_assert((++maxChecked<unsigned>()).hasOverflowed());
    static_assert([] {
        auto value = maxChecked<unsigned>();
        value++;
        return value.hasOverflowed();
    }());
    static_assert(!(maxChecked<int>() -= 1).hasOverflowed());
    static_assert(!(maxChecked<int>() /= 2).hasOverflowed());
    static_assert(!(maxChecked<unsigned>() &= 1u).hasOverflowed());

    // Assignment clears the overflow.
    static_assert([] {
        Checked<int, RecordOverflow> value = std::numeric_limits<int>::max();
        value += 1;
        bool overflowed = value.hasOverflowed();
        value = 5;
        return overflowed && !value.hasOverflowed() && value.value() == 5;
    }());

    // Binary operators.
    static_assert((Checked<int, RecordOverflow>(6) + Checked<int, RecordOverflow>(7)).value() == 13);
    static_assert((Checked<int, RecordOverflow>(6) - 7).value() == -1);
    static_assert((6 * Checked<int, RecordOverflow>(7)).value() == 42);
    static_assert((Checked<int, RecordOverflow>(42) / 6).value() == 7);
    static_assert((Checked<size_t>(4) * 5u + 1u).value() == 21);
    static_assert((Checked<int, RecordOverflow>(std::numeric_limits<int>::max()) + 1).hasOverflowed());
    static_assert((Checked<int, RecordOverflow>(std::numeric_limits<int>::min()) - 1).hasOverflowed());
    static_assert((Checked<int, RecordOverflow>(std::numeric_limits<int>::min()) / -1).hasOverflowed());
    static_assert((Checked<uint32_t, RecordOverflow>(1u << 20) * (1u << 12)).hasOverflowed());
    static_assert((Checked<uint32_t, RecordOverflow>(0u) - 1u).hasOverflowed());
    static_assert((Checked<int, RecordOverflow>(WTF::ResultOverflowed) + 1).hasOverflowed());

    // Mixed signedness.
    static_assert(Checked<int, RecordOverflow>(-1) + 2u == 1u);
    static_assert(Checked<unsigned, RecordOverflow>(3u) - 1 == 2u);
    static_assert((Checked<int, RecordOverflow>(-3) * 2u).hasOverflowed());
    static_assert((Checked<int, RecordOverflow>(-10) / Checked<unsigned, RecordOverflow>(3u)).hasOverflowed());
    static_assert((Checked<int, RecordOverflow>(10) / Checked<unsigned, RecordOverflow>(3u)).value() == 3u);

    // 64-bit and 128-bit.
    static_assert((Checked<int64_t, RecordOverflow>(std::numeric_limits<int64_t>::max()) * 2).hasOverflowed());
    static_assert((Checked<uint64_t, RecordOverflow>(1ull << 32) * (1ull << 31)).value() == 1ull << 63);
#if HAVE(INT128_T)
    static_assert((Checked<Int128, RecordOverflow>(5) * 7).value() == 35);
    static_assert(!productOverflows<UInt128>(static_cast<UInt128>(1) << 64, 2u));
    static_assert(productOverflows<UInt128>(static_cast<UInt128>(1) << 64, static_cast<UInt128>(1) << 64));
#endif

    // Comparisons.
    static_assert(Checked<int, RecordOverflow>(3) == Checked<int, CrashOnOverflow>(3));
    static_assert(Checked<int>(3) == 3u);
    static_assert(Checked<int>(-1) != 0xffffffffu);
    static_assert(Checked<uint8_t>(3) == Checked<int>(3));
    static_assert(Checked<int, RecordOverflow>(3) < Checked<int, CrashOnOverflow>(4));

    // Helpers.
    static_assert(checkedSum<size_t>(1, 2, 3).value() == 6);
    static_assert(checkedSum<uint8_t>(200, 100).hasOverflowed());
    static_assert(checkedDifference<int>(3, 5).value() == -2);
    static_assert(checkedDifference<unsigned>(3, 5).hasOverflowed());
    static_assert(checkedProduct<size_t>(2, 3, 4).value() == 24);
    static_assert(sumOverflows<int8_t>(100, 100));
    static_assert(!sumOverflows<int16_t>(100, 100));
    static_assert(differenceOverflows<uint8_t>(0, 1));
    static_assert(productOverflows<uint32_t>(1u << 20, 1u << 12));
    static_assert(!productOverflows<uint64_t>(1u << 20, 1u << 12));
    static_assert(WTF::sumIfNoOverflowOrFirstValue<uint8_t>(200, 100) == 200);
    static_assert(WTF::sumIfNoOverflowOrFirstValue<uint8_t>(100, 100) == 200);
    static_assert(isSumSmallerThanOrEqual<uint32_t>(1, 2, 3));
    static_assert(!isSumSmallerThanOrEqual<uint32_t>(std::numeric_limits<uint32_t>::max(), 1, std::numeric_limits<uint32_t>::max()));
    static_assert(safeCast<uint8_t>(200) == 200);

    constexpr auto product = CheckedSize(1000) * 1000u;
    EXPECT_FALSE(product.hasOverflowed());
    EXPECT_EQ(1000000u, product.value());
    constexpr auto overflowedProduct = Checked<uint16_t, RecordOverflow>(1000) * static_cast<uint16_t>(1000);
    EXPECT_TRUE(overflowedProduct.hasOverflowed());
}

} // namespace TestWebKitAPI
