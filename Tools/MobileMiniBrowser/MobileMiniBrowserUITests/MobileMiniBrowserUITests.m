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

#import <QuartzCore/CARenderServer.h>
#import <XCTest/XCTest.h>
#import <XCTest/XCUIRemote.h>

@interface XCUIApplication ()
-(instancetype)initPrivateWithPath:(NSString *)path bundleID:(NSString *)bundleID;
@end

@interface MobileMiniBrowserUITests : XCTestCase

@end

@implementation MobileMiniBrowserUITests {
    XCUIApplication *app;
    NSPredicate *exists;
}

- (void)setUp
{
    [super setUp];

    self.continueAfterFailure = NO;

    exists = [NSPredicate predicateWithFormat:@"exists == 1"];
    app = [[XCUIApplication alloc] init];
    [app launch];
}

- (void)tearDown
{
    [super tearDown];
}

- (void)waitToTapButtonNamed:(NSString *)name forApp:(XCUIApplication *)targetApp {
    XCUIElement *button = targetApp.buttons[name];
    [self expectationForPredicate:exists evaluatedWithObject:button handler:nil];
    [self waitForExpectationsWithTimeout:5.0 handler:nil];
    [button tap];
}

- (void)loadURL:(NSString *)url {
    XCUIElement *urlField = app.textFields[@"URL Field"];
    [self expectationForPredicate:exists evaluatedWithObject:urlField handler:nil];
    [self waitForExpectationsWithTimeout:5.0 handler:nil];
    [urlField tap];
    
    XCUIElement *clearButton = urlField.buttons[@"Clear text"];
    if (clearButton.exists)
        [clearButton tap];
    [urlField typeText:url];
    [app.buttons[@"Go To URL"] tap];
}

- (void)requireMinFPS:(NSTimeInterval)minFPS sampleDurationSeconds:(NSTimeInterval)sampleSeconds message:(NSString *)message {
    NSTimeInterval startTime = [NSDate timeIntervalSinceReferenceDate];
    uint32_t initialFrameCount = CARenderServerGetFrameCounter(0);
    usleep(sampleSeconds*1000000);
    uint32_t finalFrameCount = CARenderServerGetFrameCounter(0);
    NSTimeInterval endTime = [NSDate timeIntervalSinceReferenceDate];
    NSTimeInterval framesPerSecond = (finalFrameCount - initialFrameCount) / (endTime - startTime);
    
    XCTAssert(framesPerSecond > 30, @"framesPerSecond < %f: %@", framesPerSecond, message);
}

- (void)ensureFullscreenControls {
    if (!app.buttons[@"Done"].exists) {
        XCUIElement* window = [app.windows allElementsBoundByIndex][0];
        XCUICoordinate* center = [window coordinateWithNormalizedOffset:CGVectorMake(0.5, 0.5)];
        [center tap];
    }
}

- (NSTimeInterval)timeForTimeString:(NSString *)time {
    NSArray *components = [time componentsSeparatedByString:@":"];
    if (components.count == 3)
        return [components[0] doubleValue]*60*60 + [components[1] doubleValue]*60 + [components[2] doubleValue];

    return [components[0] doubleValue]*60 + [components[1] doubleValue];
}

- (void)testBasicVideoPlayback
{
    [self loadURL:@"bundle:/looping.html"];
    [self waitToTapButtonNamed:@"Start Playback" forApp:app];

    UIUserInterfaceIdiom idiom = UI_USER_INTERFACE_IDIOM();
    if (idiom == UIUserInterfaceIdiomPhone) {
        [self waitToTapButtonNamed:@"PauseButton" forApp:app];
        [self waitToTapButtonNamed:@"Done" forApp:app];
    } else if (idiom == UIUserInterfaceIdiomPad)
        [self waitToTapButtonNamed:@"Pause" forApp:app];
}

- (void)testBasicVideoFullscreen
{
    [self loadURL:@"bundle:/looping.html"];
    [self waitToTapButtonNamed:@"Start Playback" forApp:app];
    
    UIUserInterfaceIdiom idiom = UI_USER_INTERFACE_IDIOM();
    if (idiom == UIUserInterfaceIdiomPad)
        [self waitToTapButtonNamed:@"Display Full Screen" forApp:app];
    
    [self waitToTapButtonNamed:@"PauseButton" forApp:app];
    [self waitToTapButtonNamed:@"Done" forApp:app];
}

- (void)testVideoFullscreenAndRotationAnimation
{
    XCUIDevice *device = [XCUIDevice sharedDevice];
    device.orientation = UIDeviceOrientationPortrait;
    
    [self loadURL:@"bundle:/looping.html"];
    [self waitToTapButtonNamed:@"Start Playback" forApp:app];
    
    UIUserInterfaceIdiom idiom = UI_USER_INTERFACE_IDIOM();
    if (idiom == UIUserInterfaceIdiomPad)
        [self waitToTapButtonNamed:@"Display Full Screen" forApp:app];
    
    [self requireMinFPS:30 sampleDurationSeconds:1 message:@"Framerate during enter fullscreen animation."];
    device.orientation = UIDeviceOrientationLandscapeLeft;
    [self requireMinFPS:30 sampleDurationSeconds:1 message:@"Framerate during rotation animation."];
    [self waitToTapButtonNamed:@"Done" forApp:app];
    [self requireMinFPS:30 sampleDurationSeconds:1 message:@"Framerate during exit fullscreen  animation."];
}

- (void)testVideoFullscreenControlCenter
{
    [self loadURL:@"bundle:/looping.html"];
    [self waitToTapButtonNamed:@"Start Playback" forApp:app];
    
    UIUserInterfaceIdiom idiom = UI_USER_INTERFACE_IDIOM();
    if (idiom == UIUserInterfaceIdiomPad)
        [self waitToTapButtonNamed:@"Display Full Screen" forApp:app];
    
    XCUIElement* window = [app.windows allElementsBoundByIndex][0];
    XCUICoordinate* top = [window coordinateWithNormalizedOffset:CGVectorMake(0.5, 0.5)];
    XCUICoordinate* bottom = [window coordinateWithNormalizedOffset:CGVectorMake(0.5, 1)];
    [bottom pressForDuration:0 thenDragToCoordinate:top];
    [bottom pressForDuration:0 thenDragToCoordinate:top];
    
    XCTAssert(app.buttons[@"PlayButton"].exists, @"Control center should have interrupted");
    
    XCUIApplication* springboard = [[XCUIApplication alloc] initPrivateWithPath:nil bundleID:@"com.apple.springboard"];
    XCUICoordinate* right = [window coordinateWithNormalizedOffset:CGVectorMake(1, 0.9)];
    XCUICoordinate* left = [window coordinateWithNormalizedOffset:CGVectorMake(0, 0.9)];

    if (springboard.switches[@"Continue"].exists)
        [springboard.switches[@"Continue"] tap];
    
    if (springboard.switches[@"Airplane Mode"].exists)
        [right pressForDuration:0 thenDragToCoordinate:left];

    XCTAssert(springboard.buttons[@"Play"].exists, @"Control center should have interrupted");
    
    NSString* appElapsedString = [app.staticTexts elementBoundByIndex:0].label;
    NSTimeInterval appElapsed = [self timeForTimeString:appElapsedString];
    
    NSString* controlCenterTrackString = springboard.otherElements[@"Track position"].value;
    NSArray* components = [controlCenterTrackString componentsSeparatedByString:@" of "];
    NSTimeInterval controlCenterElapsed = [self timeForTimeString:components[0]];
    
    XCTAssert(appElapsed == round(controlCenterElapsed), "Elapsed times match");
}

// rdar://problem/27685077
- (void)testLoopingFullscreenLockup
{
    [self loadURL:@"bundle:/looping2s.html"];
    
    XCUIElement* window = [app.windows allElementsBoundByIndex][0];
    XCUICoordinate* gifLoc = [window coordinateWithNormalizedOffset:CGVectorMake(0.5, 0.75)];
    [gifLoc pressForDuration:0];
    
    [self waitToTapButtonNamed:@"Start Playback" forApp:app];
    
    [self waitToTapButtonNamed:@"ExitFullScreenButton" forApp:app];
    [self waitToTapButtonNamed:@"ExitFullScreenButton" forApp:app];
    [self waitToTapButtonNamed:@"ExitFullScreenButton" forApp:app];
    [self waitToTapButtonNamed:@"ExitFullScreenButton" forApp:app];
    [self waitToTapButtonNamed:@"ExitFullScreenButton" forApp:app];
}

@end
