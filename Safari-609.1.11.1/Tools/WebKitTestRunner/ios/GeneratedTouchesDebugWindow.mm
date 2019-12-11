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

#import "config.h"
#import "GeneratedTouchesDebugWindow.h"

#import "HIDEventGenerator.h"
#import "UIKitSPI.h"
#import <wtf/RetainPtr.h>

static const CGFloat debugTouchDotRadius = 5;
static const CGFloat debugTouchDotSize = debugTouchDotRadius * 2;

@interface GeneratedTouchesDebugWindow ()
@property (nonatomic, strong) NSArray<UIView *> *debugTouchViews;
@property (nonatomic, strong) UIApplicationRotationFollowingWindow *debugTouchWindow;
@end

@implementation GeneratedTouchesDebugWindow

+ (GeneratedTouchesDebugWindow *)sharedGeneratedTouchesDebugWindow
{
    static dispatch_once_t onceToken;
    static GeneratedTouchesDebugWindow *touchWindow = nil;
    dispatch_once(&onceToken, ^{
        touchWindow = [[GeneratedTouchesDebugWindow alloc] init];
    });
    return touchWindow;
}

- (void)dealloc
{
    _debugTouchWindow.hidden = YES;
    [_debugTouchWindow release];
    [_debugTouchViews release];
    
    [super dealloc];
}

- (void)updateDebugIndicatorForTouch:(NSUInteger)index withPointInWindowCoordinates:(CGPoint)point isTouching:(BOOL)isTouching
{
    [self initDebugViewsIfNeeded];
    
    if (index < self.debugTouchViews.count) {
        self.debugTouchViews[index].hidden = !isTouching;
        self.debugTouchViews[index].center = point;
    }
}

- (void)initDebugViewsIfNeeded
{
    if (!self.shouldShowTouches)
        return;
    
    static dispatch_once_t onceToken = 0;
    dispatch_once(&onceToken, ^{
        auto touchWindow = adoptNS([[UIApplicationRotationFollowingWindow alloc] init]);
        auto viewController = adoptNS([[UIApplicationRotationFollowingControllerNoTouches alloc] init]);
        [touchWindow setRootViewController:viewController.get()];
        [touchWindow setHidden:NO];
        [touchWindow setBackgroundColor:[UIColor clearColor]];
        [touchWindow setUserInteractionEnabled:NO];
        self.debugTouchWindow = touchWindow.get();
        
        NSMutableArray *debugViews = [NSMutableArray arrayWithCapacity:HIDMaxTouchCount];
        
        for (NSUInteger i = 0; i < HIDMaxTouchCount; ++i) {
            auto newView = adoptNS([[UIView alloc] initWithFrame:CGRectMake(10, 10, debugTouchDotSize, debugTouchDotSize)]);
            [newView setUserInteractionEnabled:NO];
            [newView layer].cornerRadius = debugTouchDotRadius;
            [newView setBackgroundColor:[UIColor colorWithRed:1.0 - i * .05 green:0.0 blue:1.0 - i * .05 alpha:0.5]];
            [newView setHidden:YES];
            debugViews[i] = newView.get();
            [[touchWindow rootViewController].view addSubview:debugViews[i]];
        }
        self.debugTouchViews = [NSArray arrayWithArray:debugViews];
    });
}

@end
