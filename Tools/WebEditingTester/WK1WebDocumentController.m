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

#import "WK1WebDocumentController.h"

#import <WebKit/WebKit.h>
#import <WebKit/WebPreferences.h>
#import <WebKit/WebPreferencesPrivate.h>
#import <WebKit/WebPreferenceKeysPrivate.h>
#import <WebKit/WebViewPrivate.h>

@interface WK1WebDocumentController () <WebUIDelegate>
@property (nonatomic, strong) WebView *webView;
@end

@implementation WK1WebDocumentController

- (void)awakeFromNib
{
    self.webView = [[WebView alloc] initWithFrame:[containerView bounds] frameName:nil groupName:@"WebEditingTester"];
    _webView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    
    _webView.editable = YES;
    _webView.UIDelegate = self;
    
    WebPreferences *preferences = [WebPreferences standardPreferences];
    preferences.fullScreenEnabled = YES;
    preferences.developerExtrasEnabled = YES;
    preferences.imageControlsEnabled = YES;
    preferences.serviceControlsEnabled = YES;
    
    self.window.title = @"WebEditor [WK1]";
    [containerView addSubview:_webView];
}

- (void)loadHTMLString:(NSString *)content
{
    [_webView.mainFrame loadHTMLString:content baseURL:nil];
}

- (IBAction)pasteAsMarkup:(id)sender
{
    // FIXME: This is probably incorrect, should use WebArchive
    NSString *markup = [[NSPasteboard generalPasteboard] stringForType:NSStringPboardType];
    [_webView replaceSelectionWithMarkupString:markup ? markup : @""];
}

@end
