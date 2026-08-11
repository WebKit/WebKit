/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#import "config.h"
#import "CoreIPCNumber.h"

#import <wtf/cocoa/TypeCastsCocoa.h>

namespace WebKit {

auto CoreIPCNumber::numberHolderForNumber(CFNumberRef cfNumber) -> NumberHolder
{
    NSNumber *number = bridge_cast(cfNumber);
    bool isNegative = [number compare:@(0)] == NSOrderedAscending;

    switch (CFNumberGetType(cfNumber)) {
    case kCFNumberSInt8Type:
        return number.charValue;
    case kCFNumberSInt16Type:
        return number.shortValue;
    case kCFNumberSInt32Type:
        return number.intValue;
    case kCFNumberSInt64Type:
        if (isNegative)
            return number.longLongValue;
        return number.unsignedLongLongValue;
    case kCFNumberFloat32Type:
        return number.floatValue;
    case kCFNumberFloat64Type:
        return number.doubleValue;
    case kCFNumberCharType:
        if (isNegative)
            return number.charValue;
        return number.unsignedCharValue;
    case kCFNumberShortType:
        if (isNegative)
            return number.shortValue;
        return number.unsignedShortValue;
    case kCFNumberIntType:
        if (isNegative)
            return number.intValue;
        return number.unsignedIntValue;
    case kCFNumberLongType:
        if (isNegative)
            return Long { number.longValue };
        return UnsignedLong { number.unsignedLongValue };
    case kCFNumberLongLongType:
        if (isNegative)
            return number.longLongValue;
        return number.unsignedLongLongValue;
    case kCFNumberFloatType:
        return number.floatValue;
    case kCFNumberDoubleType:
        return number.doubleValue;
    case kCFNumberCFIndexType:
        return Long { number.longValue };
    case kCFNumberNSIntegerType:
        return Long { number.longValue };
    case kCFNumberCGFloatType:
        return number.doubleValue;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

CoreIPCNumber::CoreIPCNumber(NSNumber *number)
    : CoreIPCNumber(bridge_cast(number)) { }

CoreIPCNumber::CoreIPCNumber(CFNumberRef number)
    : m_numberHolder(numberHolderForNumber(number)) { }

CoreIPCNumber::CoreIPCNumber(NumberHolder numberHolder)
    : m_numberHolder(numberHolder) { }

RetainPtr<CFNumberRef> CoreIPCNumber::createCFNumber() const
{
    return bridge_cast(WTF::switchOn(m_numberHolder,
        [] (const char n) {
            return adoptNS([[NSNumber alloc] initWithChar:n]);
        }, [] (const unsigned char n) {
            return adoptNS([[NSNumber alloc] initWithUnsignedChar:n]);
        }, [] (const short n) {
            return adoptNS([[NSNumber alloc] initWithShort:n]);
        }, [] (const unsigned short n) {
            return adoptNS([[NSNumber alloc] initWithUnsignedShort:n]);
        }, [] (const int n) {
            return adoptNS([[NSNumber alloc] initWithInt:n]);
        }, [] (const unsigned n) {
            return adoptNS([[NSNumber alloc] initWithUnsignedInt:n]);
        }, [] (const Long& n) {
            return adoptNS([[NSNumber alloc] initWithLong:n.value]);
        }, [] (const UnsignedLong& n) {
            return adoptNS([[NSNumber alloc] initWithUnsignedLong:n.value]);
        }, [] (const long long n) {
            return adoptNS([[NSNumber alloc] initWithLongLong:n]);
        }, [] (const unsigned long long n) {
            return adoptNS([[NSNumber alloc] initWithUnsignedLongLong:n]);
        }, [] (const float n) {
            return adoptNS([[NSNumber alloc] initWithFloat:n]);
        }, [] (const double n) {
            return adoptNS([[NSNumber alloc] initWithDouble:n]);
        }
    ));
}

CoreIPCNumber::NumberHolder CoreIPCNumber::get() const
{
    return m_numberHolder;
}

RetainPtr<id> CoreIPCNumber::toID() const
{
    return bridge_cast(createCFNumber().get());
}

}
