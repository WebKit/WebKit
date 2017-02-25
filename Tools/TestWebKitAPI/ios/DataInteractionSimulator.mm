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
    _phase = DataInteractionBeginning;
    _currentProgress = 0;
    _isDoneWithCurrentRun = false;
    _observedEventNames = adoptNS([[NSMutableArray alloc] init]);
    _finalSelectionRects = @[ ];
    _dataInteractionSession = nil;
    _dataInteractionInfo = nil;
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

    if (self.externalItemProvider) {
        _dataInteractionInfo = adoptNS([[MockDataInteractionInfo alloc] initWithProvider:self.externalItemProvider location:_startLocation window:[_webView window]]);
        _phase = DataInteractionBegan;
        [self _advanceProgress];
    } else {
        _dataInteractionSession = adoptNS([[MockDataInteractionSession alloc] initWithWindow:[_webView window]]);
        [_dataInteractionSession setMockLocationInWindow:_startLocation];
        [_webView _simulatePrepareForDataInteractionSession:_dataInteractionSession.get() completion:^() {
            DataInteractionSimulator *weakSelf = strongSelf.get();
            weakSelf->_phase = DataInteractionBeginning;
            [weakSelf _advanceProgress];
        }];
    }

    Util::run(&_isDoneWithCurrentRun);
    [_webView clearMessageHandlers:dataInteractionEventNames()];
    _finalSelectionRects = [_webView selectionRectsAfterPresentationUpdate];
}

- (NSArray *)finalSelectionRects
{
    return _finalSelectionRects.get();
}

- (void)_advanceProgress
{
    _currentProgress += progressIncrementStep;
    CGPoint locationInWindow = self._currentLocation;
    [_dataInteractionSession setMockLocationInWindow:locationInWindow];
    [_dataInteractionInfo setMockLocationInWindow:locationInWindow];

    if (_currentProgress >= 1) {
        [_webView _simulateDataInteractionPerformOperation:_dataInteractionInfo.get()];
        [_webView _simulateDataInteractionEnded:_dataInteractionInfo.get()];
        if (_dataInteractionSession)
            [_webView _simulateDataInteractionSessionDidEnd:_dataInteractionSession.get()];

        _phase = DataInteractionPerforming;
        _currentProgress = 1;
        return;
    }

    switch (_phase) {
    case DataInteractionBeginning: {
        NSMutableArray<UIItemProvider *> *itemProviders = [NSMutableArray array];
        NSArray<WKDataInteractionItem *> *items = [_webView _simulatedItemsForSession:_dataInteractionSession.get()];
        if (!items.count) {
            _phase = DataInteractionCancelled;
            _currentProgress = 1;
            _isDoneWithCurrentRun = true;
            return;
        }

        for (WKDataInteractionItem *item in items)
            [itemProviders addObject:item.itemProvider];

        _dataInteractionInfo = adoptNS([[MockDataInteractionInfo alloc] initWithProvider:itemProviders.firstObject location:self._currentLocation window:[_webView window]]);
        [_dataInteractionSession setItems:items];
        [_webView _simulateWillBeginDataInteractionWithSession:_dataInteractionSession.get()];
        _phase = DataInteractionBegan;
        break;
    }
    case DataInteractionBegan:
        [_webView _simulateDataInteractionEntered:_dataInteractionInfo.get()];
        _phase = DataInteractionEntered;
        break;
    case DataInteractionEntered:
        [_webView _simulateDataInteractionUpdated:_dataInteractionInfo.get()];
        break;
    default:
        break;
    }

    [self _scheduleAdvanceProgress];
}

- (CGPoint)_currentLocation
{
    CGFloat distanceX = _endLocation.x - _startLocation.x;
    CGFloat distanceY = _endLocation.y - _startLocation.y;
    return CGPointMake(_startLocation.x + _currentProgress * distanceX, _startLocation.y + _currentProgress * distanceY);
}

- (void)_scheduleAdvanceProgress
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(_advanceProgress) object:nil];
    [self performSelector:@selector(_advanceProgress) withObject:nil afterDelay:progressTimeStep];
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

- (void)webViewDidPerformDataInteractionControllerOperation:(WKWebView *)webView
{
    _isDoneWithCurrentRun = true;
}

@end

#endif // ENABLE(DATA_INTERACTION)
