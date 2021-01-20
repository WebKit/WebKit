/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#import <wtf/WeakObjCPtr.h>

TEST(WebKit2_WeakObjCPtr, Construction)
{
    NSObject *object = [[NSObject alloc] init];

    WeakObjCPtr<NSObject> weak(object);

    EXPECT_EQ(weak.get(), object);

    [object release];

    EXPECT_EQ(weak.get(), (void*)nil);
}

TEST(WebKit2_WeakObjCPtr, Assignment)
{
    NSObject *object1 = [[NSObject alloc] init];

    WeakObjCPtr<NSObject> weak(object1);

    EXPECT_EQ(weak.get(), object1);

    NSObject *object2 = [[NSObject alloc] init];

    weak = object2;
    EXPECT_EQ(weak.get(), object2);

    [object1 release];
    EXPECT_EQ(weak.get(), object2);

    [object2 release];
    EXPECT_EQ(weak.get(), (void*)nil);
}

TEST(WebKit2_WeakObjCPtr, ObjectOutlivesItsWeakPointer)
{
    NSObject *object = [[NSObject alloc] init];

    {
        WeakObjCPtr<NSObject> weak(object);

        EXPECT_EQ(weak.get(), object);
    }

    [object release];
}

TEST(WebKit2_WeakObjCPtr, GetAutoreleased)
{
    WeakObjCPtr<NSObject> weak;

    @autoreleasepool {
        NSObject *object = [[NSObject alloc] init];

        weak = object;

        EXPECT_EQ(weak.getAutoreleased(), object);
        
        [object release];

        // The object is still in the autorelease pool.
        EXPECT_EQ(weak.getAutoreleased(), object);
    }

    EXPECT_EQ(weak.getAutoreleased(), (id)nil);
}

TEST(WebKit2_WeakObjCPtr, Id)
{
    id object = [[NSObject alloc] init];
    WeakObjCPtr<id> weak(object);

    EXPECT_EQ(weak.get(), object);

    [object release];

    EXPECT_EQ(weak.get(), (void*)nil);
}

TEST(WebKit2_WeakObjCPtr, LogicalNegation)
{
    id object = [[NSObject alloc] init];
    WeakObjCPtr<id> weak(object);

    EXPECT_FALSE(!weak);

    [object release];

    EXPECT_TRUE(!weak);
}

TEST(WebKit2_WeakObjCPtr, CopyConstructor)
{
    id object = [[NSObject alloc] init];
    WeakObjCPtr<id> weak1(object);
    WeakObjCPtr<id> weak2(weak1);

    EXPECT_EQ(weak1.get(), object);
    EXPECT_EQ(weak2.get(), object);

    {
        WeakObjCPtr<id> weak3(weak2);
    }

    [object release];

    EXPECT_EQ(weak1.get(), (void*)nil);
    EXPECT_EQ(weak2.get(), (void*)nil);
}

TEST(WebKit2_WeakObjCPtr, MoveConstructor)
{
    id object = [[NSObject alloc] init];
    WeakObjCPtr<id> weak1(object);
    WeakObjCPtr<id> weak2(WTFMove(weak1));

    EXPECT_EQ(weak1.get(), (void*)nil);
    EXPECT_EQ(weak2.get(), object);

    [object release];

    EXPECT_EQ(weak1.get(), (void*)nil);
    EXPECT_EQ(weak2.get(), (void*)nil);
}
