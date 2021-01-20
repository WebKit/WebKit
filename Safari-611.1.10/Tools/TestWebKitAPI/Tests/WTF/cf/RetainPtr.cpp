/*
 * Copyright (C) 2011-2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <wtf/RetainPtr.h>
#include <wtf/cf/TypeCastsCF.h>

namespace TestWebKitAPI {

TEST(RetainPtr, AdoptCF)
{
    RetainPtr<CFArrayRef> array1 = adoptCF(CFArrayCreate(kCFAllocatorDefault, nullptr, 0, nullptr));
    EXPECT_EQ(1, CFGetRetainCount(array1.get()));
    EXPECT_EQ(0, CFArrayGetCount(array1.get()));

    RetainPtr<CFTypeRef> array2 = adoptCF(CFArrayCreate(kCFAllocatorDefault, nullptr, 0, nullptr));
    EXPECT_EQ(1, CFGetRetainCount(array2.get()));
    EXPECT_EQ(0, CFArrayGetCount(checked_cf_cast<CFArrayRef>(array2.get())));
}

TEST(RetainPtr, ConstructionFromMutableCFType)
{
    CFMutableStringRef string = CFStringCreateMutableCopy(kCFAllocatorDefault, 4, CFSTR("foo"));

    // This should invoke RetainPtr's move constructor.
    // FIXME: This doesn't actually test that we moved the value. We should use a mock
    // CFTypeRef that logs -retain and -release calls.
    RetainPtr<CFStringRef> ptr = RetainPtr<CFMutableStringRef>(string);

    EXPECT_EQ(string, ptr);

    RetainPtr<CFMutableStringRef> temp = string;

    // This should invoke RetainPtr's move constructor.
    RetainPtr<CFStringRef> ptr2(WTFMove(temp));

    EXPECT_EQ(string, ptr2);
    EXPECT_EQ((CFStringRef)nullptr, temp);
}

TEST(RetainPtr, ConstructionFromSameCFType)
{
    CFStringRef string = CFSTR("foo");

    // This should invoke RetainPtr's move constructor.
    // FIXME: This doesn't actually test that we moved the value. We should use a mock
    // CFTypeRef that logs -retain and -release calls.
    RetainPtr<CFStringRef> ptr = RetainPtr<CFStringRef>(string);

    EXPECT_EQ(string, ptr);

    RetainPtr<CFStringRef> temp = string;

    // This should invoke RetainPtr's move constructor.
    RetainPtr<CFStringRef> ptr2(WTFMove(temp));

    EXPECT_EQ(string, ptr2);
    EXPECT_EQ((CFStringRef)nullptr, temp);
}

TEST(RetainPtr, MoveAssignmentFromMutableCFType)
{
    CFMutableStringRef string = CFStringCreateMutableCopy(kCFAllocatorDefault, 4, CFSTR("foo"));
    RetainPtr<CFStringRef> ptr;

    // This should invoke RetainPtr's move assignment operator.
    // FIXME: This doesn't actually test that we moved the value. We should use a mock
    // CFTypeRef that logs -retain and -release calls.
    ptr = RetainPtr<CFMutableStringRef>(string);

    EXPECT_EQ(string, ptr);

    ptr = nullptr;
    RetainPtr<CFMutableStringRef> temp = string;

    // This should invoke RetainPtr's move assignment operator.
    ptr = WTFMove(temp);

    EXPECT_EQ(string, ptr);
    EXPECT_EQ((CFStringRef)nullptr, temp);
}

TEST(RetainPtr, MoveAssignmentFromSameCFType)
{
    CFStringRef string = CFSTR("foo");
    RetainPtr<CFStringRef> ptr;

    // This should invoke RetainPtr's move assignment operator.
    // FIXME: This doesn't actually test that we moved the value. We should use a mock
    // CFTypeRef that logs -retain and -release calls.
    ptr = RetainPtr<CFStringRef>(string);

    EXPECT_EQ(string, ptr);

    ptr = nullptr;
    RetainPtr<CFStringRef> temp = string;

    // This should invoke RetainPtr's move assignment operator.
    ptr = WTFMove(temp);

    EXPECT_EQ(string, ptr);
    EXPECT_EQ((CFStringRef)nullptr, temp);
}

TEST(RetainPtr, OptionalRetainPtrCF)
{
    // Test assignment from adoptCF().
    float value = 3.1415926535;
    Optional<RetainPtr<CFNumberRef>> optionalObject1 = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &value));
    EXPECT_EQ(1, CFGetRetainCount(optionalObject1.value().get()));
    RetainPtr<CFNumberRef> object1 = optionalObject1.value();
    EXPECT_EQ(optionalObject1.value(), object1);

    // Test assignment from retainPtr().
    Optional<RetainPtr<CFNumberRef>> optionalObject2 = retainPtr(CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &value));
    CFRelease(optionalObject2.value().get());
    EXPECT_EQ(1, CFGetRetainCount(optionalObject2.value().get()));
    RetainPtr<CFNumberRef> object2 = optionalObject2.value();
    EXPECT_EQ(optionalObject2.value(), object2);

    EXPECT_NE(object1, object2);

    // Test assignment from Optional<RetainPtr<CFNumberRef>>.
    optionalObject1 = optionalObject2;
    EXPECT_TRUE(optionalObject1.value());
    EXPECT_TRUE(optionalObject1.value().get());
    EXPECT_EQ(optionalObject1.value(), object2);
    EXPECT_TRUE(optionalObject2.value());
    EXPECT_TRUE(optionalObject2.value().get());
    EXPECT_EQ(optionalObject2.value(), object2);

    // Reset after assignment test.
    optionalObject1 = object1;
    EXPECT_EQ(optionalObject1.value(), object1);
    EXPECT_EQ(optionalObject2.value(), object2);

    // Test move from Optional<RetainPtr<CFNumberRef>>.
    optionalObject1 = WTFMove(optionalObject2);
    EXPECT_TRUE(optionalObject1.value());
    EXPECT_TRUE(optionalObject1.value().get());
    EXPECT_EQ(optionalObject1.value(), object2);
    EXPECT_FALSE(optionalObject2);
}

TEST(RetainPtr, RetainPtrCF)
{
    float value = 3.1415926535;
    RetainPtr<CFNumberRef> object1 = retainPtr(CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &value));
    CFRelease(object1.get());
    EXPECT_EQ(1, CFGetRetainCount(object1.get()));

    RetainPtr<CFTypeRef> object2 = retainPtr(CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &value));
    CFRelease(object2.get());
    EXPECT_EQ(1, CFGetRetainCount(object2.get()));
}

} // namespace TestWebKitAPI
