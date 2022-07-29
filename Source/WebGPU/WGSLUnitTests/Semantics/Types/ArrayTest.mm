/*
 * Copyright (c) 2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "Array.h"

#import "Bool.h"
#import "F16.h"
#import "F32.h"
#import "I32.h"
#import "U32.h"
#import "Vector.h"
#import <XCTest/XCTest.h>

using namespace WGSL::Semantics;

@interface ArrayTest : XCTestCase

@end

@implementation ArrayTest

- (void)setUp {
    [super setUp];
    [self setContinueAfterFailure:NO];
}

- (void)testCreateScalarElement {
    WGSL::SourceSpan span { 0, 1, 2, 3 };

    auto boolArray = Types::Array::tryCreate(span, Types::Bool::create(span), 10);
    auto i32Array = Types::Array::tryCreate(span, Types::I32::create(span), 10);
    auto u32Array = Types::Array::tryCreate(span, Types::U32::create(span), 10);
    auto f32Array = Types::Array::tryCreate(span, Types::F32::create(span), 10);
    auto f16Array = Types::Array::tryCreate(span, Types::F16::create(span), 10);

    XCTAssert(boolArray);
    XCTAssert(i32Array);
    XCTAssert(u32Array);
    XCTAssert(f32Array);
    XCTAssert(f16Array);
}

- (void)testCreateVectorWithConcreteElements {
    WGSL::SourceSpan span { 0, 1, 2, 3 };

    {
        auto vectorOfI32 = Types::Vector::tryCreate(span, Types::I32::create(span), 4);
        XCTAssert(vectorOfI32);

        auto vectorOfI32Array = Types::Array::tryCreate(span, WTFMove(*vectorOfI32), 10);
        XCTAssert(vectorOfI32Array);
    }

    {
        auto vectorOfI32 = Types::Vector::tryCreate(span, Types::I32::create(span), 4);
        XCTAssert(vectorOfI32);

        auto vectorOfVectorOfI32 = Types::Vector::tryCreate(span, WTFMove(*vectorOfI32), 4);
        XCTAssert(vectorOfVectorOfI32);

        auto vectorOfI32Array = Types::Array::tryCreate(span, WTFMove(*vectorOfVectorOfI32), 10);
        XCTAssert(vectorOfI32Array);
    }
}

@end
