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

#if ENABLE(DATA_INTERACTION)

#import <UIKit/UIItemProvider.h>
#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKTestingDelegate.h>
#import <wtf/BlockPtr.h>

@class MockLongPressGestureRecognizer;
@class MockDataInteractionInfo;

extern NSString * const DataInteractionEnterEventName;
extern NSString * const DataInteractionOverEventName;
extern NSString * const DataInteractionPerformOperationEventName;

typedef NS_ENUM(NSInteger, DataInteractionPhase) {
    DataInteractionUnrecognized = 1,
    DataInteractionBegan = 2,
    DataInteractionEntered = 3,
    DataInteractionPerforming = 4
};

@interface DataInteractionSimulator : NSObject<_WKTestingDelegate> {
    RetainPtr<WKWebView> _webView;
    RetainPtr<MockLongPressGestureRecognizer> _gestureRecognizer;
    RetainPtr<MockDataInteractionInfo> _dataInteractionInfo;
    CGPoint _startLocation;
    CGPoint _endLocation;

    double _gestureProgress;
    bool _isDoneWithDataInteraction;
    DataInteractionPhase _phase;
}

- (instancetype)initWithWebView:(WKWebView *)webView startLocation:(CGPoint)startLocation endLocation:(CGPoint)endLocation;
- (void)run;

@end

#endif // ENABLE(DATA_INTERACTION)
