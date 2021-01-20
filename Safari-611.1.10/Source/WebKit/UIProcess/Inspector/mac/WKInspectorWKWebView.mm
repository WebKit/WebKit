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
#import "WKInspectorWKWebView.h"

#if PLATFORM(MAC)

#import "WKInspectorPrivateMac.h"
#import <wtf/WeakObjCPtr.h>

@implementation WKInspectorWKWebView {
    WeakObjCPtr<id <WKInspectorWKWebViewDelegate>> _inspectorWKWebViewDelegate;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSWindowDidBecomeKeyNotification object:nil];

    [super dealloc];
}

- (NSRect)_opaqueRectForWindowMoveWhenInTitlebar
{
    // This convinces AppKit to allow window moves when clicking anywhere in the titlebar (top 22pt)
    // when this view's contents cover the entire window's contents, including the titlebar.
    return NSZeroRect;
}

- (NSInteger)tag
{
    return WKInspectorViewTag;
}

- (id <WKInspectorWKWebViewDelegate>)inspectorWKWebViewDelegate
{
    return _inspectorWKWebViewDelegate.getAutoreleased();
}

- (void)setInspectorWKWebViewDelegate:(id <WKInspectorWKWebViewDelegate>)delegate
{
    if (!!_inspectorWKWebViewDelegate)
        [[NSNotificationCenter defaultCenter] removeObserver:self name:NSWindowDidBecomeKeyNotification object:nil];

    _inspectorWKWebViewDelegate = delegate;

    if (!!_inspectorWKWebViewDelegate)
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_handleWindowDidBecomeKey:) name:NSWindowDidBecomeKeyNotification object:nil];
}

- (IBAction)reload:(id)sender
{
    [self.inspectorWKWebViewDelegate inspectorWKWebViewReload:self];
}

- (IBAction)reloadFromOrigin:(id)sender
{
    [self.inspectorWKWebViewDelegate inspectorWKWebViewReloadFromOrigin:self];
}

- (void)viewWillMoveToWindow:(NSWindow *)newWindow
{
    [super viewWillMoveToWindow:newWindow];
    [self.inspectorWKWebViewDelegate inspectorWKWebView:self willMoveToWindow:newWindow];
}

- (void)viewDidMoveToWindow
{
    [super viewDidMoveToWindow];
    [self.inspectorWKWebViewDelegate inspectorWKWebViewDidMoveToWindow:self];
}

- (BOOL)becomeFirstResponder
{
    BOOL result = [super becomeFirstResponder];
    [self.inspectorWKWebViewDelegate inspectorWKWebViewDidBecomeActive:self];
    return result;
}

- (void)_handleWindowDidBecomeKey:(NSNotification *)notification
{
    if (notification.object == self.window)
        [self.inspectorWKWebViewDelegate inspectorWKWebViewDidBecomeActive:self];
}

@end

#endif
