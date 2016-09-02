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

#import <XCTest/XCTest.h>
#import <XCTest/XCUIRemote.h>

@interface MobileMiniBrowserUITests : XCTestCase

@end

@implementation MobileMiniBrowserUITests

- (void)setUp
{
    [super setUp];

    self.continueAfterFailure = NO;

    XCUIApplication *app = [[XCUIApplication alloc] init];
    [app launch];
}

- (void)tearDown
{
    [super tearDown];
}

- (void)testBasicVideoPlayback
{
    XCUIApplication *app = [[XCUIApplication alloc] init];
    NSPredicate *exists = [NSPredicate predicateWithFormat:@"exists == 1"];

    XCUIElement *urlField = app.textFields[@"URL Field"];
    [self expectationForPredicate:exists evaluatedWithObject:urlField handler:nil];
    [self waitForExpectationsWithTimeout:5.0 handler:nil];
    [urlField tap];

    XCUIElement *clearButton = urlField.buttons[@"Clear text"];
    if (clearButton.exists)
        [clearButton tap];
    [urlField typeText:@"bundle:/looping.html"];
    [app.buttons[@"Go To URL"] tap];

    XCUIElement *startPlayback = app.buttons[@"Start Playback"];
    [self expectationForPredicate:exists evaluatedWithObject:startPlayback handler:nil];
    [self waitForExpectationsWithTimeout:5.0 handler:nil];
    [startPlayback tap];

    UIUserInterfaceIdiom idiom = UI_USER_INTERFACE_IDIOM();
    if (idiom == UIUserInterfaceIdiomPhone) {
        XCUIElement *pauseButton = app.buttons[@"PauseButton"];
        [self expectationForPredicate:exists evaluatedWithObject:pauseButton handler:nil];
        [self waitForExpectationsWithTimeout:5.0 handler:nil];
        [pauseButton tap];

        XCUIElement *doneButton = app.buttons[@"Done"];
        [self expectationForPredicate:exists evaluatedWithObject:doneButton handler:nil];
        [self waitForExpectationsWithTimeout:5.0 handler:nil];
        [doneButton tap];
    } else if (idiom == UIUserInterfaceIdiomPad) {
        XCUIElement *pauseButton = app.buttons[@"Pause"];
        [self expectationForPredicate:exists evaluatedWithObject:pauseButton handler:nil];
        [self waitForExpectationsWithTimeout:5.0 handler:nil];
        [pauseButton tap];
    }
}

@end
