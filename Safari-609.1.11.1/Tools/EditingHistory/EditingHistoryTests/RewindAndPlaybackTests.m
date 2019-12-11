/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#import "TestRunner.h"
#import "TestUtil.h"
#import "WKWebViewAdditions.h"
#import <XCTest/XCTest.h>

@interface RewindAndPlaybackTests : XCTestCase

@end

@implementation RewindAndPlaybackTests {
    TestRunner *testRunner;
}

- (void)setUp
{
    testRunner = [[TestRunner alloc] init];
    [testRunner loadCaptureTestHarness];
    [testRunner setTextObfuscationEnabled:NO];
}

- (void)tearDown
{
    testRunner = nil;
}

- (void)testTypingSingleLineOfText
{
    [testRunner typeString:@"hello world"];
    NSString *originalSubtree = testRunner.bodyElementSubtree;

    [self rewindAndPlaybackEditingInPlaybackTestHarness];
    XCTAssertTrue([self originalBodySubtree:originalSubtree isEqualToFinalSubtree:testRunner.bodyElementSubtree]);
    XCTAssertEqualObjects(testRunner.bodyTextContent, @"hello world");
}

- (void)testTypingMultipleLinesOfText
{
    [testRunner typeString:@"foo"];
    [testRunner typeString:@"\n"];
    [testRunner typeString:@"bar"];
    NSString *originalSubtree = testRunner.bodyElementSubtree;

    [self rewindAndPlaybackEditingInPlaybackTestHarness];
    XCTAssertTrue([self originalBodySubtree:originalSubtree isEqualToFinalSubtree:testRunner.bodyElementSubtree]);
    XCTAssertEqualObjects(testRunner.bodyTextContent, @"foobar");
}

- (void)testTypingAndDeletingText
{
    [testRunner typeString:@"apple"];
    [testRunner deleteBackwards:3];

    NSString *originalSubtree = testRunner.bodyElementSubtree;

    [self rewindAndPlaybackEditingInPlaybackTestHarness];
    XCTAssertTrue([self originalBodySubtree:originalSubtree isEqualToFinalSubtree:testRunner.bodyElementSubtree]);
    XCTAssertEqualObjects(testRunner.bodyTextContent, @"ap");
}

- (void)rewindAndPlaybackEditingInPlaybackTestHarness
{
    // This assumes that the test runner has loaded the capture test harness.
    [testRunner loadPlaybackTestHarnessWithJSON:testRunner.editingHistoryJSON];

    // Rewind to the very beginning, then play back all editing again.
    [testRunner jumpToUpdateIndex:0];
    [testRunner jumpToUpdateIndex:testRunner.numberOfUpdates];
}

- (BOOL)originalBodySubtree:(NSString *)originalSubtree isEqualToFinalSubtree:(NSString *)finalSubtree
{
    if ([originalSubtree isEqualToString:finalSubtree])
        return YES;

    NSLog(@">>>>>>>");
    NSLog(@"The original subtree is:\n%@", originalSubtree);
    NSLog(@"=======");
    NSLog(@"The final subtree is:\n%@", finalSubtree);
    NSLog(@"<<<<<<<");

    return NO;
}

@end
