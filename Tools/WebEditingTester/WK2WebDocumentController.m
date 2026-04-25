/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#import "WK2WebDocumentController.h"

#import <WebKit/WKFrameInfo.h>
#import <WebKit/WKNavigationDelegate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKUIDelegate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKWebsiteDataStore.h>

@interface WK2WebDocumentController () <WKUIDelegate, NSTextFinderBarContainer>
@property (nonatomic, strong) WKWebView *webView;
@end

@implementation WK2WebDocumentController {
    NSTextFinder *_textFinder;
    NSView *_textFindBarView;
    BOOL _findBarVisible;
}

static WKWebViewConfiguration *defaultConfiguration()
{
    static WKWebViewConfiguration *configuration;

    if (!configuration) {
        configuration = [[WKWebViewConfiguration alloc] init];
        configuration.preferences._fullScreenEnabled = YES;
        configuration.preferences._developerExtrasEnabled = YES;
    }

    return configuration;
}

- (IBAction)pasteAsMarkup:(id)sender
{
    NSLog(@"To be implemented");
}

- (void)awakeFromNib
{
    self.webView = [[WKWebView alloc] initWithFrame:[containerView bounds] configuration:defaultConfiguration()];
    _webView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    _webView._editable = YES;
    _webView.UIDelegate = self;
    
    [containerView addSubview:_webView];
    self.window.title = @"WebEditor [WK2]";

    _textFinder = [[NSTextFinder alloc] init];
    _textFinder.incrementalSearchingEnabled = YES;
    _textFinder.incrementalSearchingShouldDimContentView = YES;
    _textFinder.client = _webView;
    _textFinder.findBarContainer = self;
}

- (void)loadHTMLString:(NSString *)content
{
    NSStringEncoding encoding = NSUnicodeStringEncoding;

    NSData *data = [content dataUsingEncoding:encoding];
    CFStringEncoding cfEncoding = CFStringConvertNSStringEncodingToEncoding(encoding);
    NSString *textEncodingName = (__bridge NSString *)CFStringConvertEncodingToIANACharSetName(cfEncoding);

    [_webView _loadData:data MIMEType:@"text/html" characterEncodingName:textEncodingName baseURL:[NSURL URLWithString:@"x-webdoc:/klsadfgjlfsdj/"] userData:nil];
}

- (void)performTextFinderAction:(id)sender
{
    [_textFinder performAction:[sender tag]];
}

- (NSView *)findBarView
{
    return _textFindBarView;
}

- (void)setFindBarView:(NSView *)findBarView
{
    if (_textFindBarView)
        [_textFindBarView removeFromSuperview];
    _textFindBarView = findBarView;
    _findBarVisible = YES;
    [containerView addSubview:_textFindBarView];
    [self layout];
}

- (NSView *)contentView
{
    return _webView;
}

- (BOOL)isFindBarVisible
{
    return _findBarVisible;
}

- (void)setFindBarVisible:(BOOL)findBarVisible
{
    _findBarVisible = findBarVisible;
    if (findBarVisible)
        [containerView addSubview:_textFindBarView];
    else
        [_textFindBarView removeFromSuperview];

    [self layout];
}

- (void)findBarViewDidChangeHeight
{
    [self layout];
}

- (void)layout
{
    CGRect containerBounds = [containerView bounds];

    if (!_findBarVisible) {
        _webView.frame = containerBounds;
    } else {
        _textFindBarView.frame = CGRectMake(containerBounds.origin.x, containerBounds.origin.y + containerBounds.size.height - _textFindBarView.frame.size.height, containerBounds.size.width, _textFindBarView.frame.size.height);
        _webView.frame = CGRectMake(containerBounds.origin.x, containerBounds.origin.y, containerBounds.size.width, containerBounds.size.height - _textFindBarView.frame.size.height);
    }
}

@end
