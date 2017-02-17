/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "config.h"
#include "DataInteractionSimulator.h"

#if ENABLE(DATA_INTERACTION)

#import "PlatformUtilities.h"
#import <UIKit/UIItemProvider_Private.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

using namespace TestWebKitAPI;

#if USE(APPLE_INTERNAL_SDK) && __has_include(<WebKitAdditions/DataInteractionSimulatorAdditions.mm>)
#include <WebKitAdditions/DataInteractionSimulatorAdditions.mm>
#endif

@interface MockLongPressGestureRecognizer : UILongPressGestureRecognizer {
    RetainPtr<UIWindow> _window;
}

@property (nonatomic) CGPoint mockLocationInWindow;
@property (nonatomic) UIGestureRecognizerState mockState;
@property (nonatomic) NSInteger mockNumberOfTouches;

@end

@implementation MockLongPressGestureRecognizer

- (instancetype)initWithWindow:(UIWindow *)window
{
    if (self = [super init]) {
        _window = window;
        _mockState = UIGestureRecognizerStatePossible;
        _mockNumberOfTouches = 0;
        _mockLocationInWindow = CGPointZero;
    }
    return self;
}

- (CGPoint)locationInView:(UIView *)view
{
    return [view convertPoint:_mockLocationInWindow fromView:_window.get()];
}

- (UIGestureRecognizerState)state
{
    return _mockState;
}

- (NSUInteger)numberOfTouches
{
    return _mockNumberOfTouches;
}

@end

static double progressIncrementStep = 0.033;
static double progressTimeStep = 0.016;

static NSArray *dataInteractionEventNames()
{
    static NSArray *eventNames = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^() {
        eventNames = @[ DataInteractionEnterEventName, DataInteractionOverEventName, DataInteractionPerformOperationEventName, DataInteractionLeaveEventName, DataInteractionStartEventName ];
    });
    return eventNames;
}

@implementation DataInteractionSimulator

- (instancetype)initWithWebView:(TestWKWebView *)webView
{
    if (self = [super init]) {
        _webView = webView;
        _gestureRecognizer = adoptNS([[MockLongPressGestureRecognizer alloc] initWithWindow:webView.window]);

        [_gestureRecognizer setMockNumberOfTouches:0];
        [_webView _setTestingDelegate:self];
    }
    return self;
}

- (void)dealloc
{
    if ([_webView _testingDelegate] == self)
        [_webView _setTestingDelegate:nil];

    [super dealloc];
}

- (void)_resetSimulatedState
{
    _gestureProgress = 0;
    _phase = DataInteractionUnrecognized;
    _isDoneWithCurrentRun = false;
    _didTryToBeginDataInteraction = NO;
    _observedEventNames = adoptNS([[NSMutableArray alloc] init]);
}

- (NSArray *)observedEventNames
{
    return _observedEventNames.get();
}

- (void)runFrom:(CGPoint)startLocation to:(CGPoint)endLocation
{
    [self _resetSimulatedState];

    RetainPtr<DataInteractionSimulator> strongSelf = self;
    for (NSString *eventName in dataInteractionEventNames()) {
        DataInteractionSimulator *weakSelf = strongSelf.get();
        [weakSelf->_webView performAfterReceivingMessage:eventName action:^() {
            [weakSelf->_observedEventNames addObject:eventName];
        }];
    }

    _startLocation = startLocation;
    _endLocation = endLocation;
    [_gestureRecognizer setMockNumberOfTouches:1];

    if (self.externalItemProvider) {
        _phase = DataInteractionBegan;
        _dataInteractionInfo = adoptNS([[MockDataInteractionInfo alloc] initWithProvider:self.externalItemProvider location:startLocation window:[_webView window]]);
    } else
        [self _recognizeGestureAtLocation:_startLocation withState:UIGestureRecognizerStateBegan];

    [self _scheduleAdvanceProgress];

    Util::run(&_isDoneWithCurrentRun);
    [_gestureRecognizer setMockNumberOfTouches:0];
    [_webView clearMessageHandlers:dataInteractionEventNames()];
}

- (void)_advanceProgress
{
    _gestureProgress = std::min(1.0, std::max(0.0, progressIncrementStep + _gestureProgress));
    [_dataInteractionInfo setMockLocationInWindow:self._currentLocation];
    if (_gestureProgress >= 1) {
        [self _finishDataInteraction];
        return;
    }

    switch (_phase) {
    case DataInteractionUnrecognized:
        [self _recognizeGestureAtLocation:self._currentLocation withState:UIGestureRecognizerStateChanged];
        [self _scheduleAdvanceProgress];
        break;
    case DataInteractionBegan:
        [_webView _simulateDataInteractionEntered:_dataInteractionInfo.get()];
        _phase = DataInteractionEntered;
        [self _scheduleAdvanceProgress];
        break;
    case DataInteractionEntered:
        [_webView _simulateDataInteractionUpdated:_dataInteractionInfo.get()];
        [self _scheduleAdvanceProgress];
        break;
    default:
        break;
    }
}

- (void)_finishDataInteraction
{
    if (_phase == DataInteractionUnrecognized) {
        [self _recognizeGestureAtLocation:self._currentLocation withState:UIGestureRecognizerStateEnded];
        _isDoneWithCurrentRun = true;
        return;
    }

    _phase = DataInteractionPerforming;
    [_webView _simulateDataInteractionPerformOperation:_dataInteractionInfo.get()];
    [_webView _simulateDataInteractionEnded:_dataInteractionInfo.get()];
    [_webView _simulateDataInteractionSessionDidEnd:nil withOperation:0];
}

- (CGPoint)_currentLocation
{
    CGFloat distanceX = _endLocation.x - _startLocation.x;
    CGFloat distanceY = _endLocation.y - _startLocation.y;
    return CGPointMake(_startLocation.x + _gestureProgress * distanceX, _startLocation.y + _gestureProgress * distanceY);
}

- (void)_scheduleAdvanceProgress
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(_advanceProgress) object:nil];
    [self performSelector:@selector(_advanceProgress) withObject:nil afterDelay:progressTimeStep];
}

- (void)_recognizeGestureAtLocation:(CGPoint)locationInWindow withState:(UIGestureRecognizerState)state
{
    [_gestureRecognizer setMockState:state];
    [_gestureRecognizer setMockLocationInWindow:locationInWindow];
    [_webView _simulateDataInteractionGestureRecognized];
}

- (UIItemProvider *)externalItemProvider
{
    return _externalItemProvider.get();
}

- (void)setExternalItemProvider:(UIItemProvider *)externalItemProvider
{
    _externalItemProvider = externalItemProvider;
}

#pragma mark - _WKTestingDelegate

- (UILongPressGestureRecognizer *)dataInteractionGestureRecognizer
{
    return _gestureRecognizer.get();
}

- (void)webViewDidPerformDataInteractionControllerOperation:(WKWebView *)webView
{
    _isDoneWithCurrentRun = true;
}

- (void)webView:(WKWebView *)webView beginDataInteractionWithSourceIndex:(NSInteger)sourceIndex gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer
{
    _didTryToBeginDataInteraction = YES;

    if (self.forceRequestToFail) {
        [_webView _simulateFailedDataInteractionWithIndex:sourceIndex];
        return;
    }

    _phase = DataInteractionBegan;

    // End the data interaction gesture recognizer.
    auto location = self._currentLocation;
    [self _recognizeGestureAtLocation:location withState:UIGestureRecognizerStateEnded];

    // Officially begin data interaction by initializing the info.
    NSArray *items = [_webView _simulatedItemsForDataInteractionWithIndex:sourceIndex];
    _dataInteractionInfo = adoptNS([[MockDataInteractionInfo alloc] initWithItems:items location:location window:[_webView window]]);
    [_webView _simulateWillBeginDataInteractionWithIndex:sourceIndex withSession:nil];
}

- (void)webViewDidSendDataInteractionStartRequest:(WKWebView *)webView
{
    // This addresses a race condition in the testing harness wherein the web process might take longer than usual to process the
    // request to start data interaction, and in the meantime, DataInteractionSimulator would end up completing the rest of the test
    // before a response from the web process is received. We instead defer test progress until after the web process has indicated
    // whether or not data interaction should commence.
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(_advanceProgress) object:nil];
}

- (void)webView:(WKWebView *)webView didReceiveDataInteractionStartResponse:(BOOL)started
{
    [self _scheduleAdvanceProgress];
}

@end

#endif // ENABLE(DATA_INTERACTION)
