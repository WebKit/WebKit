/*
 * Copyright (C) 2011-2025 Apple Inc. All rights reserved.
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

#import "config.h"

#import "WTFTestUtilities.h"
#import <wtf/darwin/DispatchOSObject.h>

#if __has_feature(objc_arc)
#ifndef TYPE_CASTS_OSOBJECT_PTR_TEST_NAME
#error This tests TypeCastsOSObject.h with ARC disabled.
#endif
#define autorelease self
#endif

#ifndef TYPE_CASTS_OSOBJECT_PTR_TEST_NAME
#define TYPE_CASTS_OSOBJECT_PTR_TEST_NAME TypeCastsOSObjectCocoa
#endif

#if __has_feature(objc_arc) && !defined(NDEBUG)
// Debug builds with ARC enabled cause objects to be autoreleased
// when assigning adoptOSObject() result to a different OSObjectPtr<> type,
// and when calling OSObjectPtr<>::get().
#define AUTORELEASEPOOL_FOR_ARC_DEBUG @autoreleasepool
#else
#define AUTORELEASEPOOL_FOR_ARC_DEBUG
#endif

namespace TestWebKitAPI {

using namespace WTF;

TEST(TYPE_CASTS_OSOBJECT_PTR_TEST_NAME, osObjectCast_from_id)
{
    // Null cast.
    EXPECT_EQ(NULL, osObjectCast<dispatch_object_t>(static_cast<id>(NULL)));

    // Same cast.
    @autoreleasepool {
        OSObjectPtr<id> group;
        uintptr_t groupPtr = 0;
        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            group = adoptOSObject<id>(dispatch_group_create());
            groupPtr = reinterpret_cast<uintptr_t>(group.get());
            EXPECT_EQ(group.get(), osObjectCast<dispatch_group_t>(group.get()));
        }
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)groupPtr));
    }

    // Up cast.
    @autoreleasepool {
        OSObjectPtr<id> group;
        uintptr_t groupPtr = 0;
        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            group = adoptOSObject<id>(dispatch_group_create());
            groupPtr = reinterpret_cast<uintptr_t>(group.get());
            EXPECT_EQ(group.get(), osObjectCast<dispatch_object_t>(group.get()));
        }
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)groupPtr));
    }

    // Down cast.
    @autoreleasepool {
        OSObjectPtr<id> group;
        uintptr_t groupPtr = 0;
        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            group = adoptOSObject<id>(dispatch_group_create());
            groupPtr = reinterpret_cast<uintptr_t>(group.get());
            EXPECT_EQ(group.get(), osObjectCast<dispatch_group_t>(group.get()));
        }
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)groupPtr));
    }
}

TEST(TYPE_CASTS_OSOBJECT_PTR_TEST_NAME, dynamicOSObjectCast_from_OSObjectPtr)
{
    // Null cast.
    @autoreleasepool {
        OSObjectPtr<dispatch_object_t> object;
        auto objectCast = dynamicOSObjectCast<dispatch_group_t>(WTF::move(object));
        SUPPRESS_USE_AFTER_MOVE EXPECT_EQ(NULL, object.get());
        EXPECT_EQ(NULL, objectCast.get());
    }

    // Down cast.
    @autoreleasepool {
        OSObjectPtr<dispatch_object_t> object;
        uintptr_t objectPtr = 0;
        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            object = adoptOSObject<dispatch_object_t>(dispatch_group_create());
            objectPtr = reinterpret_cast<uintptr_t>(object.get());
        }
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectPtr));

        OSObjectPtr<dispatch_group_t> objectCast;
        uintptr_t objectCastPtr = 0;
        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            objectCast = dynamicOSObjectCast<dispatch_group_t>(WTF::move(object));
            objectCastPtr = reinterpret_cast<uintptr_t>(objectCast.get());
            SUPPRESS_USE_AFTER_MOVE EXPECT_EQ(NULL, object.get());
            EXPECT_EQ(objectPtr, objectCastPtr);
        }
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectCastPtr));
    }

    // Invalid down cast.
    @autoreleasepool {
        OSObjectPtr<dispatch_object_t> object;
        uintptr_t objectPtr = 0;
        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            object = adoptOSObject<dispatch_object_t>(dispatch_group_create());
            objectPtr = reinterpret_cast<uintptr_t>(object.get());
        }
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectPtr));

        OSObjectPtr<dispatch_source_t> objectCastBad;
        uintptr_t objectPtr2 = 0;
        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            objectCastBad = dynamicOSObjectCast<dispatch_source_t>(WTF::move(object));
            EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectPtr));
            objectPtr2 = reinterpret_cast<uintptr_t>(object.get());
            EXPECT_EQ(objectPtr, objectPtr2);
            EXPECT_EQ(NULL, objectCastBad.get());
        }
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectPtr));
    }

    // Up cast.
    @autoreleasepool {
        OSObjectPtr<dispatch_group_t> object;
        uintptr_t objectPtr = 0;
        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            object = adoptOSObject(dispatch_group_create());
            objectPtr = reinterpret_cast<uintptr_t>(object.get());
        }
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectPtr));

        OSObjectPtr<dispatch_object_t> objectCast;
        uintptr_t objectCastPtr = 0;
        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            objectCast = dynamicOSObjectCast<dispatch_object_t>(WTF::move(object));
            objectCastPtr = reinterpret_cast<uintptr_t>(objectCast.get());
            SUPPRESS_USE_AFTER_MOVE EXPECT_EQ(NULL, object.get());
            EXPECT_EQ(objectPtr, objectCastPtr);
        }
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectCastPtr));
    }

    // Bad up cast (excluding dispatch_object_t).
    @autoreleasepool {
        OSObjectPtr<dispatch_queue_t> object;
        uintptr_t objectPtr = 0;
        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            object = adoptOSObject(dispatch_queue_create("testQueue", NULL));
            objectPtr = reinterpret_cast<uintptr_t>(object.get());
        }
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectPtr));

        OSObjectPtr<dispatch_queue_global_t> objectCastBad;
        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            objectCastBad = dynamicOSObjectCast<dispatch_queue_global_t>(WTF::move(object));
            EXPECT_EQ(NULL, objectCastBad.get());
        }
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectPtr));
    }
}

TEST(TYPE_CASTS_OSOBJECT_PTR_TEST_NAME, dynamicOSObjectCast_from_id)
{
    // Null cast.
    EXPECT_EQ(NULL, dynamicOSObjectCast<dispatch_object_t>(static_cast<id>(nil)));

    // Same cast.
    @autoreleasepool {
        OSObjectPtr<id> object;
        uintptr_t objectPtr = 0;
        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            object = adoptOSObject<id>(dispatch_group_create());
            objectPtr = reinterpret_cast<uintptr_t>(object.get());
            EXPECT_EQ(object.get(), dynamicOSObjectCast<dispatch_group_t>(object.get()));
        }
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectPtr));
    }

    // Down cast.
    @autoreleasepool {
        OSObjectPtr<id> object;
        uintptr_t objectPtr = 0;
        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            object = adoptOSObject<id>(dispatch_group_create());
            objectPtr = reinterpret_cast<uintptr_t>(object.get());
        }
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectPtr));

        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            EXPECT_EQ(objectPtr, reinterpret_cast<uintptr_t>(dynamicOSObjectCast<dispatch_group_t>(object.get())));
        }
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectPtr));
    }

    // Invalid down cast.
    @autoreleasepool {
        OSObjectPtr<id> object;
        uintptr_t objectPtr = 0;
        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            object = adoptOSObject<id>(dispatch_group_create());
            objectPtr = reinterpret_cast<uintptr_t>(object.get());
        }
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectPtr));

        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            EXPECT_EQ(NULL, dynamicOSObjectCast<dispatch_source_t>(object.get()));
        }
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectPtr));
    }

    // Up cast.
    @autoreleasepool {
        OSObjectPtr<id> object;
        uintptr_t objectPtr = 0;
        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            object = adoptOSObject<id>(dispatch_group_create());
            objectPtr = reinterpret_cast<uintptr_t>(object.get());
        }
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectPtr));

        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            EXPECT_EQ(objectPtr, reinterpret_cast<uintptr_t>(dynamicOSObjectCast<dispatch_object_t>(object.get())));
        }
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectPtr));
    }

    // Bad up cast (excluding dispatch_object_t).
    @autoreleasepool {
        OSObjectPtr<id> object;
        uintptr_t objectPtr = 0;
        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            object = adoptOSObject<id>(dispatch_queue_create("testQueue", NULL));
            objectPtr = reinterpret_cast<uintptr_t>(object.get());
        }
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectPtr));

        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            EXPECT_EQ(NULL, dynamicOSObjectCast<dispatch_queue_global_t>(object.get()));
        }
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectPtr));
    }
}

} // namespace TestWebKitAPI
