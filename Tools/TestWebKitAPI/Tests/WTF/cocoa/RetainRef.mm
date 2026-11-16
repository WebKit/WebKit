/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#import "config.h"
#import <wtf/HashMap.h>
#import <wtf/RetainRef.h>

#if __has_feature(objc_arc)
#ifndef RETAIN_REF_TEST_NAME
#error This tests RetainRef.h with ARC disabled.
#endif
#endif

#ifndef RETAIN_REF_TEST_NAME
#define RETAIN_REF_TEST_NAME RetainRef
#endif

#if __has_feature(objc_arc) && !defined(NDEBUG)
#define AUTORELEASEPOOL_FOR_ARC_DEBUG @autoreleasepool
#else
#define AUTORELEASEPOOL_FOR_ARC_DEBUG
#endif

namespace TestWebKitAPI {

TEST(RETAIN_REF_TEST_NAME, AdoptNSRef)
{
    RetainRef<NSObject> object = adoptNSRef([[NSObject alloc] init]);
    uintptr_t objectPtr = 0;
    AUTORELEASEPOOL_FOR_ARC_DEBUG {
        objectPtr = reinterpret_cast<uintptr_t>(object.get());
    }
    EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectPtr));
}

TEST(RETAIN_REF_TEST_NAME, RetainRefFromValueNS)
{
    RetainPtr<NSObject> source = adoptNS([[NSObject alloc] init]);
    uintptr_t sourcePtr = 0;
    AUTORELEASEPOOL_FOR_ARC_DEBUG {
        sourcePtr = reinterpret_cast<uintptr_t>(source.get());
    }
    EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)sourcePtr));
    AUTORELEASEPOOL_FOR_ARC_DEBUG {
        RetainRef<NSObject> ref = retainRef(source.get());
        EXPECT_EQ(sourcePtr, reinterpret_cast<uintptr_t>(ref.get()));
    }
    EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)sourcePtr));
}

TEST(RETAIN_REF_TEST_NAME, CopyConstructorNS)
{
    RetainRef<NSObject> a = adoptNSRef([[NSObject alloc] init]);
    uintptr_t rawPtr = 0;
    AUTORELEASEPOOL_FOR_ARC_DEBUG {
        rawPtr = reinterpret_cast<uintptr_t>(a.get());
    }
    EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)rawPtr));
    {
        RetainRef<NSObject> b = a;
        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            EXPECT_EQ(rawPtr, reinterpret_cast<uintptr_t>(b.get()));
        }
        EXPECT_EQ(2L, CFGetRetainCount((CFTypeRef)rawPtr));
    }
    EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)rawPtr));
}

TEST(RETAIN_REF_TEST_NAME, MoveConstructorNS)
{
    RetainRef<NSObject> a = adoptNSRef([[NSObject alloc] init]);
    uintptr_t rawPtr = 0;
    AUTORELEASEPOOL_FOR_ARC_DEBUG {
        rawPtr = reinterpret_cast<uintptr_t>(a.get());
    }
    EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)rawPtr));

    RetainRef<NSObject> b = WTF::move(a);
    AUTORELEASEPOOL_FOR_ARC_DEBUG {
        EXPECT_EQ(rawPtr, reinterpret_cast<uintptr_t>(b.get()));
    }
    EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)rawPtr));
}

TEST(RETAIN_REF_TEST_NAME, FromRetainPtrReleaseNonNullNS)
{
    RetainPtr<NSObject> ptr = adoptNS([[NSObject alloc] init]);
    uintptr_t rawPtr = 0;
    AUTORELEASEPOOL_FOR_ARC_DEBUG {
        rawPtr = reinterpret_cast<uintptr_t>(ptr.get());
    }
    EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)rawPtr));

    RetainRef<NSObject> ref = ptr.releaseNonNull();
    AUTORELEASEPOOL_FOR_ARC_DEBUG {
        EXPECT_EQ(rawPtr, reinterpret_cast<uintptr_t>(ref.get()));
        EXPECT_EQ(nullptr, ptr.get());
    }
    EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)rawPtr));
}

TEST(RETAIN_REF_TEST_NAME, MoveToRetainPtrNS)
{
    RetainRef<NSObject> ref = adoptNSRef([[NSObject alloc] init]);
    uintptr_t rawPtr = 0;
    AUTORELEASEPOOL_FOR_ARC_DEBUG {
        rawPtr = reinterpret_cast<uintptr_t>(ref.get());
    }
    EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)rawPtr));

    {
        RetainPtr<NSObject> ptr { WTF::move(ref) };
        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            EXPECT_EQ(rawPtr, reinterpret_cast<uintptr_t>(ptr.get()));
        }
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)rawPtr));
    }
}

TEST(RETAIN_REF_TEST_NAME, AssignToRetainPtrNS)
{
    RetainPtr<NSObject> ptr = adoptNS([[NSObject alloc] init]);
    RetainRef<NSObject> ref = adoptNSRef([[NSObject alloc] init]);
    uintptr_t rawNew = 0;
    AUTORELEASEPOOL_FOR_ARC_DEBUG {
        rawNew = reinterpret_cast<uintptr_t>(ref.get());
    }
    EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)rawNew));

    ptr = WTF::move(ref);
    AUTORELEASEPOOL_FOR_ARC_DEBUG {
        EXPECT_EQ(rawNew, reinterpret_cast<uintptr_t>(ptr.get()));
    }
    EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)rawNew));
}

TEST(RETAIN_REF_TEST_NAME, CtadFromRetainRefNS)
{
    RetainRef<NSObject> ref = adoptNSRef([[NSObject alloc] init]);
    uintptr_t rawPtr = 0;
    AUTORELEASEPOOL_FOR_ARC_DEBUG {
        rawPtr = reinterpret_cast<uintptr_t>(ref.get());
    }
    EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)rawPtr));

    RetainPtr ptr { WTF::move(ref) };
    static_assert(std::is_same_v<decltype(ptr), RetainPtr<NSObject>>);
    AUTORELEASEPOOL_FOR_ARC_DEBUG {
        EXPECT_EQ(rawPtr, reinterpret_cast<uintptr_t>(ptr.get()));
    }
    EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)rawPtr));
}

TEST(RETAIN_REF_TEST_NAME, ExplicitFromRetainPtrNS)
{
    RetainPtr<NSObject> ptr = adoptNS([[NSObject alloc] init]);
    uintptr_t rawPtr = 0;
    AUTORELEASEPOOL_FOR_ARC_DEBUG {
        rawPtr = reinterpret_cast<uintptr_t>(ptr.get());
    }
    EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)rawPtr));
    AUTORELEASEPOOL_FOR_ARC_DEBUG {
        RetainRef<NSObject> ref { ptr.get() };
        EXPECT_EQ(rawPtr, reinterpret_cast<uintptr_t>(ref.get()));
    }
    EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)rawPtr));
}

TEST(RETAIN_REF_TEST_NAME, HashMapNS)
{
    HashMap<RetainRef<NSObject>, int> map;
    RetainRef<NSObject> key = adoptNSRef([[NSObject alloc] init]);
    map.add(key, 7);
    EXPECT_EQ(1u, map.size());
    EXPECT_EQ(7, map.get(key));
}

} // namespace TestWebKitAPI
