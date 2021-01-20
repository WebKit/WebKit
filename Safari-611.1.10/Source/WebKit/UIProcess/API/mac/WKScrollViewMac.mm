/*
* Copyright (C) 2020 Apple Inc. All rights reserved.
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
#import "WKScrollViewMac.h"

#if PLATFORM(MAC)

#import <wtf/WeakObjCPtr.h>

@implementation WKScrollView {
    WeakObjCPtr<id <WKScrollViewDelegate>> _delegate;
}

+ (BOOL)isCompatibleWithResponsiveScrolling
{
    return NO;
}

- (instancetype)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (!self)
        return nil;

    self.allowsMagnification = YES;
    self.minMagnification = 1; // Matches ViewGestureController.
    self.maxMagnification = 3;

    [self.contentView setPostsBoundsChangedNotifications:YES];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(boundsDidChange:) name:NSViewBoundsDidChangeNotification object:self.contentView];
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [super dealloc];
}

- (id <WKScrollViewDelegate>)delegate
{
    return _delegate.getAutoreleased();
}

- (void)setDelegate:(id <WKScrollViewDelegate>)delegate
{
    _delegate = delegate;
}

- (CGPoint)contentOffset
{
    return self.contentView.bounds.origin;
}

- (void)boundsDidChange:(NSNotification *)notification
{
    auto retainedDelegate = _delegate.get();
    if (retainedDelegate)
        [retainedDelegate scrollViewDidScroll:self];
}

- (void)setContentInsets:(NSEdgeInsets)insets
{
    BOOL insetsChanged = !NSEdgeInsetsEqual(insets, self.contentInsets);
    [super setContentInsets:insets];
    if (insetsChanged) {
        auto retainedDelegate = _delegate.get();
        if (retainedDelegate)
            [retainedDelegate scrollViewContentInsetsDidChange:self];
    }
}

@end

#endif // PLATFORM(MAC)
