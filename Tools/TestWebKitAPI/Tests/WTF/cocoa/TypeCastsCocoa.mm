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

@interface MyObjectSubtype : NSObject
@end

@implementation MyObjectSubtype
@end

namespace TestWebKitAPI {

using namespace WTF;

TEST(TypeCastsCocoa, checked_objc_cast)
{
    EXPECT_EQ(nil, checked_objc_cast<NSString>(nil));

    {
        auto object = adoptNS(reinterpret_cast<id>([[NSString alloc] initWithFormat:@"%s", "Hello world"]));
        EXPECT_EQ(object.get(), checked_objc_cast<NSString>(object.get()));
        EXPECT_EQ(object.get(), checked_objc_cast<NSObject>(object.get()));
    }

    {
        auto object = adoptNS(reinterpret_cast<NSObject *>([[NSString alloc] initWithFormat:@"%s", "Hello world"]));
        EXPECT_EQ(object.get(), checked_objc_cast<NSString>(object.get()));
    }

    {
        auto object = adoptNS([[NSString alloc] initWithFormat:@"%s", "Hello world"]);
        EXPECT_EQ(object.get(), checked_objc_cast<NSObject>(object.get()));
    }
}

TEST(TypeCastsCocoa, dynamic_objc_cast)
{
    EXPECT_EQ(nil, dynamic_objc_cast<NSString>(nil));

    {
        auto object = adoptNS(reinterpret_cast<id>([[NSString alloc] initWithFormat:@"%s", "Hello world"]));
        EXPECT_EQ(object.get(), dynamic_objc_cast<NSString>(object.get()));
        EXPECT_EQ(object.get(), dynamic_objc_cast<NSObject>(object.get()));
        EXPECT_EQ(nil, dynamic_objc_cast<NSArray>(object.get()));
    }

    {
        auto object = adoptNS(reinterpret_cast<NSObject *>([[NSString alloc] initWithFormat:@"%s", "Hello world"]));
        EXPECT_EQ(object.get(), dynamic_objc_cast<NSString>(object.get()));
        EXPECT_EQ(nil, dynamic_objc_cast<NSArray>(object.get()));
    }

    {
        auto object = adoptNS([[NSString alloc] initWithFormat:@"%s", "Hello world"]);
        EXPECT_EQ(object.get(), dynamic_objc_cast<NSObject>(object.get()));
        EXPECT_EQ(nil, dynamic_objc_cast<NSArray>(object.get()));
    }

    {
        auto object = adoptNS(reinterpret_cast<id>([[NSObject alloc] init]));
        EXPECT_EQ(object.get(), dynamic_objc_cast<NSObject>(object.get()));
        EXPECT_EQ(nil, dynamic_objc_cast<MyObjectSubtype>(object.get()));
    }

    {
        auto object = adoptNS([[NSObject alloc] init]);
        EXPECT_EQ(nil, dynamic_objc_cast<MyObjectSubtype>(object.get()));
    }
}

TEST(TypeCastsCocoa, dynamic_ns_cast_RetainPtr)
{
    {
        RetainPtr<NSString> object;
        auto objectCast = dynamic_objc_cast<NSString>(WTFMove(object));
        EXPECT_EQ(nil, object.get());
        EXPECT_EQ(nil, objectCast.get());
    }

    {
        auto object = adoptNS(reinterpret_cast<id>([[NSString alloc] initWithFormat:@"%s", "Hello world"]));
        id objectPtr = object.get();
        auto objectCast = dynamic_objc_cast<NSString>(WTFMove(object));
        EXPECT_EQ(nil, object.get());
        EXPECT_EQ(objectPtr, objectCast.get());
        objectPtr = nil; // For ARC.
        EXPECT_EQ(1U, [objectCast retainCount]);

        object = adoptNS(reinterpret_cast<id>([[NSString alloc] initWithFormat:@"%s", "Hello world"]));
        objectPtr = object.get();
        auto objectCast2 = dynamic_objc_cast<NSObject>(WTFMove(object));
        EXPECT_EQ(nil, object.get());
        EXPECT_EQ(objectPtr, objectCast2.get());
        objectPtr = nil; // For ARC.
        EXPECT_EQ(1U, [objectCast2 retainCount]);

        object = adoptNS(reinterpret_cast<id>([[NSString alloc] initWithFormat:@"%s", "Hello world"]));
        objectPtr = object.get();
        auto objectCast3 = dynamic_objc_cast<NSArray>(WTFMove(object));
        EXPECT_EQ(objectPtr, object.get());
        EXPECT_EQ(nil, objectCast3.get());
        objectPtr = nil; // For ARC.
        EXPECT_EQ(1U, [object retainCount]);
    }

    {
        auto object = adoptNS(reinterpret_cast<NSObject *>([[NSString alloc] initWithFormat:@"%s", "Hello world"]));
        id objectPtr = object.get();
        auto objectCast = dynamic_objc_cast<NSString>(WTFMove(object));
        EXPECT_EQ(nil, object.get());
        EXPECT_EQ(objectPtr, objectCast.get());
        objectPtr = nil;
        EXPECT_EQ(1U, [objectCast retainCount]);

        object = adoptNS(reinterpret_cast<NSObject *>([[NSString alloc] initWithFormat:@"%s", "Hello world"]));
        objectPtr = object.get();
        auto objectCast2 = dynamic_objc_cast<NSArray>(WTFMove(object));
        EXPECT_EQ(objectPtr, object.get());
        EXPECT_EQ(nil, objectCast2.get());
        objectPtr = nil;
        EXPECT_EQ(1U, [object retainCount]);
    }

    {
        auto object = adoptNS([[NSString alloc] initWithFormat:@"%s", "Hello world"]);
        id objectPtr = object.get();
        auto objectCast = dynamic_objc_cast<NSObject>(WTFMove(object));
        EXPECT_EQ(nil, object.get());
        EXPECT_EQ(objectPtr, objectCast.get());
        objectPtr = nil;
        EXPECT_EQ(1U, [objectCast retainCount]);

        object = adoptNS([[NSString alloc] initWithFormat:@"%s", "Hello world"]);
        objectPtr = object.get();
        auto objectCast2 = dynamic_objc_cast<NSArray>(WTFMove(object));
        EXPECT_EQ(objectPtr, object.get());
        EXPECT_EQ(nil, objectCast2.get());
        objectPtr = nil;
        EXPECT_EQ(1U, [object retainCount]);
    }

    {
        auto object = adoptNS(reinterpret_cast<id>([[NSObject alloc] init]));
        id objectPtr = object.get();
        auto objectCast = dynamic_objc_cast<NSObject>(WTFMove(object));
        EXPECT_EQ(nil, object.get());
        EXPECT_EQ(objectPtr, objectCast.get());
        objectPtr = nil; // For ARC.
        EXPECT_EQ(1U, [objectCast retainCount]);

        object = adoptNS(reinterpret_cast<id>([[NSObject alloc] init]));
        objectPtr = object.get();
        auto objectCast2 = dynamic_objc_cast<MyObjectSubtype>(WTFMove(object));
        EXPECT_EQ(objectPtr, object.get());
        EXPECT_EQ(nil, objectCast2.get());
        objectPtr = nil; // For ARC.
        EXPECT_EQ(1U, [object retainCount]);
    }

    {
        auto object = adoptNS([[NSObject alloc] init]);
        id objectPtr = object.get();
        auto objectCast = dynamic_objc_cast<MyObjectSubtype>(WTFMove(object));
        EXPECT_EQ(objectPtr, object.get());
        EXPECT_EQ(nil, objectCast.get());
        objectPtr = nil; // For ARC.
        EXPECT_EQ(1U, [object retainCount]);
    }
}

} // namespace TestWebKitAPI
