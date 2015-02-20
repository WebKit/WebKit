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

@interface WK2WebDocumentController () <WKUIDelegate>
@property (nonatomic, strong) WKWebView *webView;
@end

@implementation WK2WebDocumentController

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
}

- (void)loadHTMLString:(NSString *)content
{
    [_webView loadHTMLString:content baseURL:nil];
}

@end
