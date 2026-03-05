/*
 * Copyright (C) 2021-2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import <wtf/darwin/TypeCastsOSObject.h>

#import "WTFTestUtilities.h"
#import <wtf/StdLibExtras.h>
#import <wtf/darwin/DispatchExtras.h>
#import <wtf/darwin/DispatchOSObject.h>

#ifdef __OBJC__
#error This tests TypeCastsOSObject.h in non-Cocoa source.
#endif

namespace TestWebKitAPI {

using namespace WTF;

TEST(TypeCastsOSObjectCF, osObjectCast)
{
    // Null cast.
    EXPECT_EQ(NULL, osObjectCast<dispatch_object_t>(NULL));

    // Same cast.
    OSObjectPtr<dispatch_group_t> group = adoptOSObject(dispatch_group_create());
    CFTypeRef groupPtr = group.get();
    EXPECT_EQ(group.get(), osObjectCast<dispatch_group_t>(groupPtr));
    EXPECT_EQ(1L, CFGetRetainCount(groupPtr));

    // Up cast.
    EXPECT_EQ(group.get(), osObjectCast<dispatch_object_t>(group.get()));
    EXPECT_EQ(1L, CFGetRetainCount(groupPtr));

    // Down cast.
    auto object = adoptOSObject<dispatch_object_t>(dispatch_group_create());
    uintptr_t objectPtr = reinterpret_cast<uintptr_t>(object.get());
    EXPECT_EQ(object.get(), osObjectCast<dispatch_group_t>(object.get()));
    EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectPtr));
}

TEST(TypeCastsOSObjectCF, dynamicOSObjectCast)
{
    // Null cast.
    EXPECT_EQ(NULL, dynamicOSObjectCast<dispatch_object_t>(NULL));

    // Same cast / up cast bad cast from CFTypeRef.
    {
        auto objectCF = adoptCF<CFTypeRef>(dispatch_group_create());
        uintptr_t objectCFPtr = reinterpret_cast<uintptr_t>(objectCF.get());
        EXPECT_EQ(objectCF.get(), dynamicOSObjectCast<dispatch_group_t>(objectCF.get()));
        EXPECT_EQ(objectCF.get(), dynamicOSObjectCast<dispatch_object_t>(objectCF.get()));
        EXPECT_EQ(NULL, dynamicOSObjectCast<dispatch_source_t>(objectCF.get()));
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectCFPtr));
    }

    // Down cast / bad cast.
    {
        auto object = adoptOSObject<dispatch_object_t>(dispatch_group_create());
        uintptr_t objectPtr = reinterpret_cast<uintptr_t>(object.get());
        EXPECT_EQ(object.get(), dynamicOSObjectCast<dispatch_group_t>(object.get()));
        EXPECT_EQ(NULL, dynamicOSObjectCast<dispatch_source_t>(object.get()));
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectPtr));
    }

    // Up cast / bad cast.
    {
        auto object = adoptOSObject(dispatch_group_create());
        uintptr_t objectPtr = reinterpret_cast<uintptr_t>(object.get());
        EXPECT_EQ(object.get(), dynamicOSObjectCast<dispatch_object_t>(object.get()));
        EXPECT_EQ(NULL, dynamicOSObjectCast<dispatch_source_t>(object.get()));
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectPtr));
    }

    // Up cast (excluding dispatch_object_t).
    {
        OSObjectPtr object = globalDispatchQueueSingleton(QOS_CLASS_BACKGROUND, 0);
        uintptr_t objectPtr = reinterpret_cast<uintptr_t>(object.get());
        EXPECT_EQ(object.get(), dynamicOSObjectCast<dispatch_queue_t>(object.get()));
        EXPECT_EQ(-1L, CFGetRetainCount((CFTypeRef)objectPtr)); // Immortal object.
    }

    // Bad down cast.
    {
        auto object = adoptOSObject(dispatch_queue_create("testQueue", NULL));
        uintptr_t objectPtr = reinterpret_cast<uintptr_t>(object.get());
        EXPECT_EQ(NULL, dynamicOSObjectCast<dispatch_queue_global_t>(object.get()));
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectPtr));
    }
}

TEST(TypeCastsOSObjectCF, dynamicOSObjectCast_OSObjectPtr)
{
    // Null cast.
    {
        OSObjectPtr<dispatch_object_t> object;
        auto objectCast = dynamicOSObjectCast<dispatch_group_t>(WTF::move(object));
        SUPPRESS_USE_AFTER_MOVE EXPECT_EQ(NULL, object.get());
        EXPECT_EQ(NULL, objectCast.get());
    }

    // Down cast / bad cast.
    {
        auto object = adoptOSObject<dispatch_object_t>(dispatch_group_create());
        uintptr_t objectPtr = reinterpret_cast<uintptr_t>(object.get());
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectPtr));

        OSObjectPtr<dispatch_group_t> objectCast = dynamicOSObjectCast<dispatch_group_t>(WTF::move(object));
        uintptr_t objectCastPtr = reinterpret_cast<uintptr_t>(objectCast.get());
        SUPPRESS_USE_AFTER_MOVE EXPECT_EQ(NULL, object.get());
        EXPECT_EQ(objectPtr, objectCastPtr);
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectCastPtr));

        object = adoptOSObject<dispatch_object_t>(dispatch_group_create());
        objectPtr = reinterpret_cast<uintptr_t>(object.get());
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectPtr));

        OSObjectPtr<dispatch_source_t> objectCastBad = dynamicOSObjectCast<dispatch_source_t>(WTF::move(object));
        uintptr_t objectPtr2 = reinterpret_cast<uintptr_t>(object.get());
        EXPECT_EQ(objectPtr, objectPtr2);
        EXPECT_EQ(NULL, objectCastBad.get());
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectPtr2));
    }

    // Up cast.
    {
        auto object = adoptOSObject(dispatch_group_create());
        uintptr_t objectPtr = reinterpret_cast<uintptr_t>(object.get());
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectPtr));

        auto objectCast = dynamicOSObjectCast<dispatch_object_t>(WTF::move(object));
        uintptr_t objectCastPtr = reinterpret_cast<uintptr_t>(objectCast.get());
        SUPPRESS_USE_AFTER_MOVE EXPECT_EQ(NULL, object.get());
        EXPECT_EQ(objectPtr, objectCastPtr);
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectCastPtr));
    }

    // Bad up cast (excluding dispatch_object_t).
    {
        auto object = adoptOSObject(dispatch_queue_create("testQueue", NULL));
        uintptr_t objectPtr = reinterpret_cast<uintptr_t>(object.get());
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectPtr));

        OSObjectPtr<dispatch_queue_global_t> objectCastBad = dynamicOSObjectCast<dispatch_queue_global_t>(WTF::move(object));
        EXPECT_EQ(NULL, objectCastBad.get());
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectPtr));
    }
}

} // namespace TestWebKitAPI
