/*
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

#pragma once

#if USE(CF)

#import <CoreFoundation/CoreFoundation.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

namespace WebKit {

class CoreIPCNumber {
public:
    typedef std::variant<
        char,
        unsigned char,
        short,
        unsigned short,
        int,
        unsigned,
        long,
        unsigned long,
        long long,
        unsigned long long,
        float,
        double
    > NumberHolder;

    static NumberHolder numberHolderForNumber(CFNumberRef number)
    {
        CFNumberType numberType = CFNumberGetType(number);
        bool isNegative = [bridge_cast(number) compare:@(0)] == NSOrderedAscending;

        switch (numberType) {
        case kCFNumberSInt8Type:
            return [bridge_cast(number) charValue ];
        case kCFNumberSInt16Type:
            return [bridge_cast(number) shortValue ];
        case kCFNumberSInt32Type:
            return [bridge_cast(number) intValue ];
        case kCFNumberSInt64Type:
            if (isNegative)
                return [bridge_cast(number) longLongValue ];
            return [bridge_cast(number) unsignedLongLongValue ];
        case kCFNumberFloat32Type:
            return [bridge_cast(number) floatValue ];
        case kCFNumberFloat64Type:
            return [bridge_cast(number) doubleValue ];
        case kCFNumberCharType:
            if (isNegative)
                return [bridge_cast(number) charValue ];
            return [bridge_cast(number) unsignedCharValue ];
        case kCFNumberShortType:
            if (isNegative)
                return [bridge_cast(number) shortValue ];
            return [bridge_cast(number) unsignedShortValue ];
        case kCFNumberIntType:
            if (isNegative)
                return [bridge_cast(number) intValue ];
            return [bridge_cast(number) unsignedIntValue ];
        case kCFNumberLongType:
            if (isNegative)
                return [bridge_cast(number) longValue ];
            return [bridge_cast(number) unsignedLongValue ];
        case kCFNumberLongLongType:
            if (isNegative)
                return [bridge_cast(number) longLongValue ];
            return [bridge_cast(number) unsignedLongLongValue ];
        case kCFNumberFloatType:
            return [bridge_cast(number) floatValue ];
        case kCFNumberDoubleType:
            return [bridge_cast(number) doubleValue ];
        case kCFNumberCFIndexType:
            return [bridge_cast(number) longValue ];
        case kCFNumberNSIntegerType:
            return [bridge_cast(number) longValue ];
        case kCFNumberCGFloatType:
            return [bridge_cast(number) doubleValue ];
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    CoreIPCNumber(NSNumber *number)
        : CoreIPCNumber(bridge_cast(number))
    {
    }

    CoreIPCNumber(CFNumberRef number)
        : m_numberHolder(numberHolderForNumber(number))
    {
    }

    CoreIPCNumber(NumberHolder numberHolder)
        : m_numberHolder(numberHolder)
    {
    }

    CoreIPCNumber(const CoreIPCNumber& other) = default;

    RetainPtr<CFNumberRef> createCFNumber() const
    {
        return WTF::switchOn(m_numberHolder,
            [&] (const char& n) {
                return bridge_cast(adoptNS([[NSNumber alloc] initWithChar: n]));
            },
            [&] (const unsigned char& n) {
                return bridge_cast(adoptNS([[NSNumber alloc] initWithUnsignedChar: n]));
            },
            [&] (const short& n) {
                return bridge_cast(adoptNS([[NSNumber alloc] initWithShort: n]));
            },
            [&] (const unsigned short& n) {
                return bridge_cast(adoptNS([[NSNumber alloc] initWithUnsignedShort: n]));
            },
            [&] (const int& n) {
                return bridge_cast(adoptNS([[NSNumber alloc] initWithInt: n]));
            },
            [&] (const unsigned& n) {
                return bridge_cast(adoptNS([[NSNumber alloc] initWithUnsignedInt: n]));
            },
            [&] (const long& n) {
                return bridge_cast(adoptNS([[NSNumber alloc] initWithLong: n]));
            },
            [&] (const unsigned long& n) {
                return bridge_cast(adoptNS([[NSNumber alloc] initWithUnsignedLong: n]));
            },
            [&] (const long long& n) {
                return bridge_cast(adoptNS([[NSNumber alloc] initWithLongLong: n]));
            },
            [&] (const unsigned long long& n) {
                return bridge_cast(adoptNS([[NSNumber alloc] initWithUnsignedLongLong: n]));
            },
            [&] (const float& n) {
                return bridge_cast(adoptNS([[NSNumber alloc] initWithFloat: n]));
            },
            [&] (const double& n) {
                return bridge_cast(adoptNS([[NSNumber alloc] initWithDouble: n]));
            }
        );
    }

    CoreIPCNumber::NumberHolder get() const
    {
        return m_numberHolder;
    }

    RetainPtr<id> toID() const { return bridge_cast(createCFNumber().get()); }

private:
    NumberHolder m_numberHolder;
};

}

#endif // USE(CF)
