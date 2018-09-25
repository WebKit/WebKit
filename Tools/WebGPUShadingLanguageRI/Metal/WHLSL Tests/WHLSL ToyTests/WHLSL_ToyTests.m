/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#import <XCTest/XCTest.h>

#import "TestFamily.h"
#import "TestFamilyRunner.h"

/// The actual implementation of tests is in Test.h/m
@interface WHLSLShaderTest : XCTestCase

@end

@implementation WHLSLShaderTest

+ (WHLSLShaderTest*)testCaseInstanceFromImplementation:(TestFamily*)test
{
    NSMethodSignature *testSignature = [WHLSLShaderTest instanceMethodSignatureForSelector:@selector(testShaderCompilationAndExecution:)];
    NSInvocation *testInvocation = [NSInvocation invocationWithMethodSignature:testSignature];
    testInvocation.selector = @selector(testShaderCompilationAndExecution:);
    [testInvocation setArgument:&test atIndex:2];
    [testInvocation retainArguments];

    WHLSLShaderTest *testCase = [[WHLSLShaderTest alloc] initWithInvocation:testInvocation];
    return testCase;
}

- (void)testShaderCompilationAndExecution:(TestFamily*)test {
    TestFamilyRunner* runner = [[TestFamilyRunner alloc] initWithTestFamily:test];
    BOOL result = [runner executeAllTests];
    if (result)
        NSLog(@"\u2705 %@", test.name);
    XCTAssert(result, @"%@", test.name);
    NSLog(@"Total test case count: %lu", [TestFamilyRunner executionCount]);
}

@end

@interface WHLSL_ToyTests : XCTestCase

@end

@implementation WHLSL_ToyTests

+ (XCTestSuite*)defaultTestSuite
{
    XCTestSuite* suite = [XCTestSuite testSuiteWithName:@"WHLSL tests"];
    NSArray<TestFamily*>* testDescriptions = [TestFamily allTests];
    NSRegularExpression* testFilter = [NSRegularExpression regularExpressionWithPattern:@".*" options:0 error:nil];
    NSUInteger testCaseCount = 0;
    for (TestFamily* test in testDescriptions) {
        if ([testFilter firstMatchInString:test.name options:0 range:NSMakeRange(0, test.name.length)]) {
            [suite addTest:[WHLSLShaderTest testCaseInstanceFromImplementation:test]];
            testCaseCount += test.totalTestCaseCount;
        }
    }

    NSLog(@"Will execute %ld tests (%ld test cases)", suite.tests.count, testCaseCount);

    return suite;
}

@end
