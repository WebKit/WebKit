/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#import <wtf/cocoa/TypeCastsCocoa.h>

#import <wtf/StdLibExtras.h>

#if __has_feature(objc_arc) && !defined(TypeCastsCocoa)
#error This tests TypeCastsCocoa.h with ARC disabled.
#endif

#if __has_feature(objc_arc) && !defined(NDEBUG)
// Debug builds with ARC enabled cause objects to be autoreleased
// when assigning adoptNS() result to a different RetainPtr<> type,
// and when calling RetainPtr<>::get().
#define AUTORELEASEPOOL_FOR_ARC_DEBUG @autoreleasepool
#else
#define AUTORELEASEPOOL_FOR_ARC_DEBUG
#endif

@interface MyObjectSubtype : NSObject
@end

@implementation MyObjectSubtype
@end

namespace TestWebKitAPI {

using namespace WTF;

static const char* helloWorldCString = "Hello world";
static size_t helloWorldCStringLength()
{
    static auto length = strlen(helloWorldCString);
    return length;
}

TEST(TypeCastsCocoa, bridge_cast)
{
    @autoreleasepool {
        auto objectNS = adoptNS([[NSString alloc] initWithFormat:@"%s", helloWorldCString]);
        auto objectNSPtr = reinterpret_cast<uintptr_t>(objectNS.get());
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectNSPtr));

        auto objectCF = bridge_cast(WTFMove(objectNS));
        auto objectCFPtr = reinterpret_cast<uintptr_t>(objectCF.get());
        SUPPRESS_USE_AFTER_MOVE EXPECT_EQ(nil, objectNS.get());
        EXPECT_EQ(objectNSPtr, objectCFPtr);
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectCFPtr));

        objectNS = bridge_cast(WTFMove(objectCF));
        objectNSPtr = reinterpret_cast<uintptr_t>(objectNS.get());
        SUPPRESS_USE_AFTER_MOVE EXPECT_EQ(NULL, objectCF.get());
        EXPECT_EQ(objectCFPtr, objectNSPtr);
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectNSPtr));
    }

    @autoreleasepool {
        auto objectCF = adoptCF(CFStringCreateWithBytes(NULL, (const UInt8*)helloWorldCString, helloWorldCStringLength(), kCFStringEncodingUTF8, false));
        auto objectCFPtr = reinterpret_cast<uintptr_t>(objectCF.get());
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectCFPtr));

        auto objectNS = bridge_cast(WTFMove(objectCF));
        auto objectNSPtr = reinterpret_cast<uintptr_t>(objectNS.get());
        SUPPRESS_USE_AFTER_MOVE EXPECT_EQ(NULL, objectCF.get());
        EXPECT_EQ(objectCFPtr, objectNSPtr);
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectNSPtr));

        objectCF = bridge_cast(WTFMove(objectNS));
        objectCFPtr = reinterpret_cast<uintptr_t>(objectCF.get());
        SUPPRESS_USE_AFTER_MOVE EXPECT_EQ(nil, objectNS.get());
        EXPECT_EQ(objectNSPtr, objectCFPtr);
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectCFPtr));
    }
}

TEST(TypeCastsCocoa, bridge_id_cast)
{
    @autoreleasepool {
        RetainPtr<CFTypeRef> objectCF;
        auto objectID = bridge_id_cast(WTFMove(objectCF));
        SUPPRESS_USE_AFTER_MOVE EXPECT_EQ(NULL, objectCF.get());
        EXPECT_EQ(nil, objectID.get());
    }

    @autoreleasepool {
        auto objectCF = adoptCF(CFStringCreateWithBytes(NULL, (const UInt8*)helloWorldCString, helloWorldCStringLength(), kCFStringEncodingUTF8, false));
        auto objectCFPtr = reinterpret_cast<uintptr_t>(objectCF.get());
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectCFPtr));

        auto objectID = bridge_id_cast(WTFMove(objectCF));
        uintptr_t objectIDPtr;
        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            objectIDPtr = reinterpret_cast<uintptr_t>(objectID.get());
        }
        SUPPRESS_USE_AFTER_MOVE EXPECT_EQ(NULL, objectCF.get());
        EXPECT_EQ(objectCFPtr, objectIDPtr);
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectIDPtr));
    }
}

TEST(TypeCastsCocoa, checked_objc_cast)
{
    EXPECT_EQ(nil, checked_objc_cast<NSString>(nil));

    @autoreleasepool {
        RetainPtr<id> objectNS;
        uintptr_t objectNSPtr;
        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            objectNS = adoptNS<id>([[NSString alloc] initWithFormat:@"%s", helloWorldCString]);
            objectNSPtr = reinterpret_cast<uintptr_t>(objectNS.get());
            EXPECT_EQ(objectNS.get(), checked_objc_cast<NSString>(objectNS.get()));
            EXPECT_EQ(objectNS.get(), checked_objc_cast<NSObject>(objectNS.get()));
        }
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectNSPtr));
    }

    @autoreleasepool {
        RetainPtr<NSObject *> objectNS;
        uintptr_t objectNSPtr;
        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            objectNS = adoptNS<NSObject *>([[NSString alloc] initWithFormat:@"%s", helloWorldCString]);
            objectNSPtr = reinterpret_cast<uintptr_t>(objectNS.get());
            EXPECT_EQ(objectNS.get(), checked_objc_cast<NSString>(objectNS.get()));
        }
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectNSPtr));
    }

    @autoreleasepool {
        RetainPtr<NSString> objectNS;
        uintptr_t objectNSPtr;
        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            objectNS = adoptNS([[NSString alloc] initWithFormat:@"%s", helloWorldCString]);
            objectNSPtr = reinterpret_cast<uintptr_t>(objectNS.get());
            EXPECT_EQ(objectNS.get(), checked_objc_cast<NSObject>(objectNS.get()));
        }
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectNSPtr));
    }
}

TEST(TypeCastsCocoa, dynamic_objc_cast)
{
    EXPECT_EQ(nil, dynamic_objc_cast<NSString>(nil));

    @autoreleasepool {
        auto objectNS = adoptNS<id>([[NSString alloc] initWithFormat:@"%s", helloWorldCString]);
        uintptr_t objectNSPtr;
        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            objectNSPtr = reinterpret_cast<uintptr_t>(objectNS.get());
            EXPECT_EQ(objectNS.get(), dynamic_objc_cast<NSString>(objectNS.get()));
            EXPECT_EQ(objectNS.get(), dynamic_objc_cast<NSObject>(objectNS.get()));
            EXPECT_EQ(nil, dynamic_objc_cast<NSArray>(objectNS.get()));
        }
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectNSPtr));
    }

    @autoreleasepool {
        auto objectNS = adoptNS<NSObject *>([[NSString alloc] initWithFormat:@"%s", helloWorldCString]);
        uintptr_t objectNSPtr;
        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            objectNSPtr = reinterpret_cast<uintptr_t>(objectNS.get());
            EXPECT_EQ(objectNS.get(), dynamic_objc_cast<NSString>(objectNS.get()));
            EXPECT_EQ(nil, dynamic_objc_cast<NSArray>(objectNS.get()));
        }
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectNSPtr));
    }

    @autoreleasepool {
        auto objectNS = adoptNS([[NSString alloc] initWithFormat:@"%s", helloWorldCString]);
        uintptr_t objectNSPtr;
        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            objectNSPtr = reinterpret_cast<uintptr_t>(objectNS.get());
            EXPECT_EQ(objectNS.get(), dynamic_objc_cast<NSObject>(objectNS.get()));
            EXPECT_EQ(nil, dynamic_objc_cast<NSArray>(objectNS.get()));
        }
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectNSPtr));
    }

    @autoreleasepool {
        auto objectID = adoptNS<id>([[NSObject alloc] init]);
        uintptr_t objectIDPtr;
        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            objectIDPtr = reinterpret_cast<uintptr_t>(objectID.get());
            EXPECT_EQ(objectID.get(), dynamic_objc_cast<NSObject>(objectID.get()));
            EXPECT_EQ(nil, dynamic_objc_cast<MyObjectSubtype>(objectID.get()));
        }
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectIDPtr));
    }

    @autoreleasepool {
        auto objectNS = adoptNS([[NSObject alloc] init]);
        uintptr_t objectNSPtr;
        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            objectNSPtr = reinterpret_cast<uintptr_t>(objectNS.get());
            EXPECT_EQ(nil, dynamic_objc_cast<MyObjectSubtype>(objectNS.get()));
        }
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectNSPtr));
    }
}

TEST(TypeCastsCocoa, dynamic_objc_cast_RetainPtr)
{
    @autoreleasepool {
        RetainPtr<NSString> object;
        auto objectCast = dynamic_objc_cast<NSString>(WTFMove(object));
        SUPPRESS_USE_AFTER_MOVE EXPECT_EQ(nil, object.get());
        EXPECT_EQ(nil, objectCast.get());
    }

    @autoreleasepool {
        auto object = adoptNS<id>([[NSString alloc] initWithFormat:@"%s", helloWorldCString]);
        uintptr_t objectPtr;
        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            objectPtr = reinterpret_cast<uintptr_t>(object.get());
        }
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectPtr));

        RetainPtr<NSString> objectCast;
        uintptr_t objectCastPtr;
        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            objectCast = dynamic_objc_cast<NSString>(WTFMove(object));
            objectCastPtr = reinterpret_cast<uintptr_t>(objectCast.get());
        }
        SUPPRESS_USE_AFTER_MOVE EXPECT_EQ(nil, object.get());
        EXPECT_EQ(objectPtr, objectCastPtr);
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectCastPtr));

        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            object = adoptNS<id>([[NSString alloc] initWithFormat:@"%s", helloWorldCString]);
            objectPtr = reinterpret_cast<uintptr_t>(object.get());
        }
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectPtr));

        RetainPtr<NSObject> objectCast2;
        uintptr_t objectCastPtr2;
        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            objectCast2 = dynamic_objc_cast<NSObject>(WTFMove(object));
            objectCastPtr2 = reinterpret_cast<uintptr_t>(objectCast2.get());
        }
        SUPPRESS_USE_AFTER_MOVE EXPECT_EQ(nil, object.get());
        EXPECT_EQ(objectPtr, objectCastPtr2);
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectCastPtr2));

        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            object = adoptNS<id>([[NSString alloc] initWithFormat:@"%s", helloWorldCString]);
            objectPtr = reinterpret_cast<uintptr_t>(object.get());
        }
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectPtr));

        RetainPtr<NSArray> objectCastBad;
        uintptr_t objectPtr2;
        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            objectCastBad = dynamic_objc_cast<NSArray>(WTFMove(object));
            objectPtr2 = reinterpret_cast<uintptr_t>(object.get());
        }
        EXPECT_EQ(objectPtr, objectPtr2);
        EXPECT_EQ(nil, objectCastBad.get());
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectPtr2));
    }

    @autoreleasepool {
        auto object = adoptNS([[NSString alloc] initWithFormat:@"%s", helloWorldCString]);
        uintptr_t objectPtr;
        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            objectPtr = reinterpret_cast<uintptr_t>(object.get());
        }
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectPtr));

        auto objectCast = dynamic_objc_cast<NSObject>(WTFMove(object));
        uintptr_t objectCastPtr;
        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            objectCastPtr = reinterpret_cast<uintptr_t>(objectCast.get());
        }
        SUPPRESS_USE_AFTER_MOVE EXPECT_EQ(nil, object.get());
        EXPECT_EQ(objectPtr, objectCastPtr);
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectCastPtr));
    }

    @autoreleasepool {
        RetainPtr<id> object;
        uintptr_t objectPtr;
        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            object = adoptNS<id>([[NSObject alloc] init]);
            objectPtr = reinterpret_cast<uintptr_t>(object.get());
        }
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectPtr));

        RetainPtr<NSObject> objectCast;
        uintptr_t objectCastPtr;
        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            objectCast = dynamic_objc_cast<NSObject>(WTFMove(object));
            objectCastPtr = reinterpret_cast<uintptr_t>(objectCast.get());
        }
        SUPPRESS_USE_AFTER_MOVE EXPECT_EQ(nil, object.get());
        EXPECT_EQ(objectPtr, objectCastPtr);
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectCastPtr));

        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            object = adoptNS<id>([[NSObject alloc] init]);
            objectPtr = reinterpret_cast<uintptr_t>(object.get());
        }
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectPtr));

        RetainPtr<MyObjectSubtype> objectCastBad;
        AUTORELEASEPOOL_FOR_ARC_DEBUG {
            objectCastBad = dynamic_objc_cast<MyObjectSubtype>(WTFMove(object));
        }
        EXPECT_EQ(nil, objectCastBad.get());
        EXPECT_EQ(1L, CFGetRetainCount((CFTypeRef)objectPtr));
    }
}

} // namespace TestWebKitAPI
