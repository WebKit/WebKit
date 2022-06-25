/*
 * Copyright (C) 2014-2021 Apple Inc. All rights reserved.
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

#include <wtf/OSObjectPtr.h>

#include <CoreFoundation/CoreFoundation.h>
#include <dispatch/dispatch.h>

#if __has_feature(objc_arc) && !defined(NDEBUG)
// Debug builds with ARC enabled cause objects to be autoreleased
// when assigning adoptNS() result to a different RetainPtr<> type,
// and when calling RetainPtr<>::get().
#define AUTORELEASEPOOL_FOR_ARC_DEBUG @autoreleasepool
#else
#define AUTORELEASEPOOL_FOR_ARC_DEBUG
#endif

#ifndef OS_OBJECT_PTR_TEST_NAME
#define OS_OBJECT_PTR_TEST_NAME OSObjectPtr
#endif

namespace TestWebKitAPI {

TEST(OS_OBJECT_PTR_TEST_NAME, AdoptOSObject)
{
    OSObjectPtr<dispatch_queue_t> foo = adoptOSObject(dispatch_queue_create(0, DISPATCH_QUEUE_SERIAL));
    uintptr_t fooPtr;
    AUTORELEASEPOOL_FOR_ARC_DEBUG {
        fooPtr = reinterpret_cast<uintptr_t>(foo.get());
    }
    EXPECT_EQ(1, CFGetRetainCount((CFTypeRef)fooPtr));
}

TEST(OS_OBJECT_PTR_TEST_NAME, RetainRelease)
{
    dispatch_queue_t foo = dispatch_queue_create(0, DISPATCH_QUEUE_SERIAL);
    auto fooPtr = reinterpret_cast<uintptr_t>(foo);
    EXPECT_EQ(1, CFGetRetainCount((CFTypeRef)fooPtr));

    WTF::retainOSObject(foo); // Does nothing under ARC.
#if __has_feature(objc_arc)
    EXPECT_EQ(1, CFGetRetainCount((CFTypeRef)fooPtr));
#else
    EXPECT_EQ(2, CFGetRetainCount((CFTypeRef)fooPtr));
#endif

    WTF::releaseOSObject(foo); // Does nothing under ARC.
    EXPECT_EQ(1, CFGetRetainCount((CFTypeRef)fooPtr));

    WTF::releaseOSObject(foo); // Balance dispatch_queue_create() without ARC.
}

TEST(OS_OBJECT_PTR_TEST_NAME, LeakRef)
{
    OSObjectPtr<dispatch_queue_t> foo = adoptOSObject(dispatch_queue_create(0, DISPATCH_QUEUE_SERIAL));
    uintptr_t fooPtr;
    AUTORELEASEPOOL_FOR_ARC_DEBUG {
        fooPtr = reinterpret_cast<uintptr_t>(foo.get());
    }
    EXPECT_EQ(1, CFGetRetainCount((CFTypeRef)fooPtr));

    dispatch_queue_t queue;
    AUTORELEASEPOOL_FOR_ARC_DEBUG {
        queue = foo.leakRef();
    }
    EXPECT_EQ(nullptr, foo.get());
    EXPECT_EQ(1, CFGetRetainCount((CFTypeRef)fooPtr));

    WTF::releaseOSObject(queue);
}

} // namespace TestWebKitAPI
