/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#pragma once

#import "config.h"
#import "WKRevealItemPresenter.h"

#if PLATFORM(MAC) && ENABLE(REVEAL)

#import "WebViewImpl.h"
#import <wtf/RetainPtr.h>
#import <wtf/WeakPtr.h>
#import <pal/cocoa/RevealSoftLink.h>

@interface WKRevealItemPresenter () <RVPresenterHighlightDelegate>
@end

@implementation WKRevealItemPresenter {
    WeakPtr<WebKit::WebViewImpl> _impl;
    RetainPtr<RVPresenter> _presenter;
    RetainPtr<RVPresentingContext> _presentingContext;
    RetainPtr<RVItem> _item;
    CGRect _frameInView;
    CGPoint _menuLocationInView;
    BOOL _isHighlightingItem;
}

- (instancetype)initWithWebViewImpl:(const WebKit::WebViewImpl&)impl item:(RVItem *)item frame:(CGRect)frameInView menuLocation:(CGPoint)menuLocationInView
{
    if (!(self = [super init]))
        return nil;

    _impl = impl;
    _presenter = adoptNS([PAL::allocRVPresenterInstance() init]);
    _presentingContext = adoptNS([PAL::allocRVPresentingContextInstance() initWithPointerLocationInView:menuLocationInView inView:impl.view() highlightDelegate:self]);
    _item = item;
    _frameInView = frameInView;
    _menuLocationInView = menuLocationInView;
    return self;
}

- (void)showContextMenu
{
    if (!_impl)
        return;

    auto view = _impl->view();
    if (!view)
        return;

    auto menuItems = retainPtr([_presenter menuItemsForItem:_item.get() documentContext:nil presentingContext:_presentingContext.get() options:nil]);
    if (![menuItems count])
        return;

    auto menu = adoptNS([[NSMenu alloc] initWithTitle:emptyString()]);
    [menu setAutoenablesItems:NO];
    [menu setItemArray:menuItems.get()];

    auto clickLocationInWindow = [view convertPoint:_menuLocationInView toView:nil];
    NSEvent *event = [NSEvent mouseEventWithType:NSEventTypeLeftMouseDown location:clickLocationInWindow modifierFlags:0 timestamp:0 windowNumber:view.window.windowNumber context:0 eventNumber:0 clickCount:1 pressure:1];
    [NSMenu popUpContextMenu:menu.get() withEvent:event forView:view];

    [self _callDidFinishPresentationIfNeeded];
}

- (void)_callDidFinishPresentationIfNeeded
{
    if (!_impl || _isHighlightingItem)
        return;

    _impl->didFinishPresentation(self);
}

#pragma mark - RVPresenterHighlightDelegate

- (NSArray<NSValue *> *)revealContext:(RVPresentingContext *)context rectsForItem:(RVItem *)item
{
    return @[ [NSValue valueWithRect:_frameInView] ];
}

- (BOOL)revealContext:(RVPresentingContext *)context shouldUseDefaultHighlightForItem:(RVItem *)item
{
    return self.shouldUseDefaultHighlight;
}

- (void)revealContext:(RVPresentingContext *)context startHighlightingItem:(RVItem *)item
{
    _isHighlightingItem = YES;
}

- (void)revealContext:(RVPresentingContext *)context stopHighlightingItem:(RVItem *)item
{
    _isHighlightingItem = NO;

    [self _callDidFinishPresentationIfNeeded];
}

@end

#endif // PLATFORM(MAC) && ENABLE(REVEAL)
