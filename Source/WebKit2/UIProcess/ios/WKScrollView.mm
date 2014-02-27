/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#import "WKScrollView.h"

#if PLATFORM(IOS)

@interface WKScrollViewDelegateForwarder : NSObject <UIScrollViewDelegate>

- (instancetype)initWithInternalDelegate:(id <UIScrollViewDelegate>)internalDelegate externalDelegate:(id <UIScrollViewDelegate>)externalDelegate;

@end

@implementation WKScrollViewDelegateForwarder {
    id <UIScrollViewDelegate> _internalDelegate;
    id <UIScrollViewDelegate> _externalDelegate;
}

- (instancetype)initWithInternalDelegate:(id <UIScrollViewDelegate>)internalDelegate externalDelegate:(id <UIScrollViewDelegate>)externalDelegate
{
    self = [super init];
    if (!self)
        return nil;
    _internalDelegate = internalDelegate;
    _externalDelegate = externalDelegate;
    return self;
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector
{
    NSMethodSignature *signature = [super methodSignatureForSelector:aSelector];
    if (!signature)
        signature = [(NSObject *)_internalDelegate methodSignatureForSelector:aSelector];
    if (!signature)
        signature = [(NSObject *)_externalDelegate methodSignatureForSelector:aSelector];
    return signature;
}

- (BOOL)respondsToSelector:(SEL)aSelector
{
    return [super respondsToSelector:aSelector] || [_internalDelegate respondsToSelector:aSelector] || [_externalDelegate respondsToSelector:aSelector];
}

- (void)forwardInvocation:(NSInvocation *)anInvocation
{
    BOOL messageHandled = NO;
    if ([_internalDelegate respondsToSelector:[anInvocation selector]]) {
        [anInvocation invokeWithTarget:_internalDelegate];
        messageHandled = YES;
    }
    if ([_externalDelegate respondsToSelector:[anInvocation selector]]) {
        [anInvocation invokeWithTarget:_externalDelegate];
        messageHandled = YES;
    }
    if (!messageHandled)
        [super forwardInvocation:anInvocation];
}

@end

@implementation WKScrollView {
    id <UIScrollViewDelegate> _externalDelegate;
    WKScrollViewDelegateForwarder *_delegateForwarder;
}

- (BOOL)touchesShouldCancelInContentView:(UIView *)contentView
{
    return NO;
}

- (void)setInternalDelegate:(id <UIScrollViewDelegate>)internalDelegate
{
    if (internalDelegate == _internalDelegate)
        return;
    _internalDelegate = internalDelegate;
    [self _updateDelegate];
}

- (void)setDelegate:(id <UIScrollViewDelegate>)delegate
{
    if (_externalDelegate == delegate)
        return;
    _externalDelegate = delegate;
    [self _updateDelegate];
}

- (id <UIScrollViewDelegate>)delegate
{
    return _externalDelegate;
}

- (void)_updateDelegate
{
    WKScrollViewDelegateForwarder *oldForwarder = _delegateForwarder;
    _delegateForwarder = nil;
    if (!_externalDelegate)
        [super setDelegate:_internalDelegate];
    else if (!_internalDelegate)
        [super setDelegate:_externalDelegate];
    else {
        _delegateForwarder = [[WKScrollViewDelegateForwarder alloc] initWithInternalDelegate:_internalDelegate externalDelegate:_externalDelegate];
        [super setDelegate:_delegateForwarder];
    }
    [oldForwarder release];
}

- (void)dealloc
{
    [_delegateForwarder release];
    [super dealloc];
}

@end

#endif // PLATFORM(IOS)
