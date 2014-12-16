/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import "WKPagePreviewViewController.h"

#import "WKWebViewInternal.h"

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000 && WK_API_ENABLED

static const CGFloat previewViewInset = 3;
static const CGFloat previewViewTitleHeight = 34;

@implementation WKPagePreviewViewController

- (instancetype)initWithPageURL:(NSURL *)URL mainViewSize:(NSSize)size popoverToViewScale:(CGFloat)scale
{
    if (!(self = [super init]))
        return nil;

    _url = URL;
    _mainViewSize = size;
    _popoverToViewScale = scale;

    return self;
}

- (NSString *)previewTitle
{
    return _previewTitle.get();
}

- (void)setPreviewTitle:(NSString *)previewTitle
{
    if ([_previewTitle isEqualToString:previewTitle])
        return;

    // Keep a separate copy around in case this is received before the view hierarchy is created.
    _previewTitle = adoptNS([previewTitle copy]);
    [_titleTextField setStringValue:previewTitle ? previewTitle : @""];
}

+ (NSSize)previewPadding
{
    return NSMakeSize(2 * previewViewInset, previewViewTitleHeight + 2 * previewViewInset);
}

- (void)loadView
{
    NSRect defaultFrame = NSMakeRect(0, 0, _mainViewSize.width, _mainViewSize.height);
    _previewView = [_delegate pagePreviewViewController:self viewForPreviewingURL:_url.get() initialFrameSize:defaultFrame.size];
    if (!_previewView) {
        RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:defaultFrame]);
        [webView _setIgnoresNonWheelEvents:YES];
        if (_url) {
            NSURLRequest *request = [NSURLRequest requestWithURL:_url.get()];
            [webView loadRequest:request];
        }
        _previewView = webView;
    }

    RetainPtr<NSClickGestureRecognizer> clickRecognizer = adoptNS([[NSClickGestureRecognizer alloc] initWithTarget:self action:@selector(_clickRecognized:)]);
    [_previewView addGestureRecognizer:clickRecognizer.get()];

    NSRect previewFrame = [_previewView frame];
    NSRect containerFrame = previewFrame;
    NSSize totalPadding = [[self class] previewPadding];
    containerFrame.size.width += totalPadding.width;
    containerFrame.size.height += totalPadding.height;
    previewFrame = NSOffsetRect(previewFrame, previewViewInset, previewViewInset);

    RetainPtr<NSView> containerView = adoptNS([[NSView alloc] initWithFrame:containerFrame]);
    [containerView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [containerView addSubview:_previewView.get()];
    [_previewView setFrame:previewFrame];
    [_previewView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

    _titleTextField = adoptNS([[NSTextField alloc] init]);
    [_titleTextField setWantsLayer:YES];
    [_titleTextField setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
    [_titleTextField setEditable:NO];
    [_titleTextField setBezeled:NO];
    [_titleTextField setDrawsBackground:NO];
    [_titleTextField setAlignment:NSCenterTextAlignment];
    [_titleTextField setUsesSingleLineMode:YES];
    [_titleTextField setLineBreakMode:NSLineBreakByTruncatingTail];
    [_titleTextField setTextColor:[NSColor labelColor]];

    NSString *title = _previewTitle.get();
    if (!title)
        title = [_delegate pagePreviewViewController:self titleForPreviewOfURL:_url.get()];
    if (!title)
        title = [_url absoluteString];

    [_titleTextField setStringValue:title ? title : @""];

    [_titleTextField sizeToFit];
    NSSize titleFittingSize = [_titleTextField frame].size;
    CGFloat textFieldCenteringOffset = (NSMaxY(containerFrame) - NSMaxY(previewFrame) - titleFittingSize.height) / 2;

    NSRect titleFrame = previewFrame;
    titleFrame.size.height = titleFittingSize.height;
    titleFrame.origin.y = NSMaxY(previewFrame) + textFieldCenteringOffset;
    [_titleTextField setFrame:titleFrame];
    [containerView addSubview:_titleTextField.get()];

    // Setting the webView bounds will scale it to 75% of the _mainViewSize.
    [_previewView setBounds:NSMakeRect(0, 0, _mainViewSize.width / _popoverToViewScale, _mainViewSize.height / _popoverToViewScale)];

    self.view = containerView.get();
}

- (void)_clickRecognized:(NSGestureRecognizer *)gestureRecognizer
{
    if (gestureRecognizer.state == NSGestureRecognizerStateBegan)
        [_delegate pagePreviewViewControllerWasClicked:self];
}

@end

#endif // PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000 && WK_API_ENABLED
