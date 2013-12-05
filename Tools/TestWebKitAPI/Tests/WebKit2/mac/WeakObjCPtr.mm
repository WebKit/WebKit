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

#import <WebKit2/WeakObjCPtr.h>

using WebKit::WeakObjCPtr;

TEST(WebKit2, WeakObjCPtr)
{
    // Test construction.
    {
        NSObject *object = [[NSObject alloc] init];

        WeakObjCPtr<NSObject> weak(object);

        EXPECT_EQ(weak.get(), object);

        [object release];

        EXPECT_EQ(weak.get(), (void*)nil);
    }

    // Test assignment.
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

    // Test having an object that outlives its weak pointer.
    {
        NSObject *object = [[NSObject alloc] init];

        {
            WeakObjCPtr<NSObject> weak(object);

            EXPECT_EQ(weak.get(), object);
        }

        [object release];
    }

}
