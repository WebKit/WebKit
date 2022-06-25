/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#import <WebKit/WKFoundation.h>

#if WK_HAVE_C_SPI

#import <WebKit/WKNumber.h>
#import <WebKit/WKRetainPtr.h>

namespace TestWebKitAPI {

TEST(WebKit, WKNSNumber)
{
    auto booleanRef = adoptWK(WKBooleanCreate(true));
    auto uint64Ref = adoptWK(WKUInt64Create(39));
    auto doubleRef = adoptWK(WKDoubleCreate(-16.2));

    NSNumber *booleanNumber = (NSNumber *)booleanRef.get();
    NSNumber *uint64Number = (NSNumber *)uint64Ref.get();
    NSNumber *doubleNumber = (NSNumber *)doubleRef.get();

    EXPECT_EQ(YES, booleanNumber.boolValue);
    EXPECT_EQ(YES, booleanNumber.charValue);
    EXPECT_EQ(39UL, uint64Number.unsignedLongLongValue);
    EXPECT_EQ(39, uint64Number.intValue);
    EXPECT_EQ(-16.2, doubleNumber.doubleValue);
}

} // namespace TestWebKitAPI

#endif
