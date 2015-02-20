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

@implementation WK1WebDocumentController {
    WebView *_webView;
}

- (void)awakeFromNib
{
    _webView = [[WebView alloc] initWithFrame:[containerView bounds] frameName:nil groupName:@"WebEditingTester"];
    [_webView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
    
    [_webView setEditable:YES];
    [_webView setUIDelegate:self];
    
    [[WebPreferences standardPreferences] setFullScreenEnabled:YES];
    [[WebPreferences standardPreferences] setDeveloperExtrasEnabled:YES];
    [[WebPreferences standardPreferences] setImageControlsEnabled:YES];
    [[WebPreferences standardPreferences] setServiceControlsEnabled:YES];
    
    [self.window setTitle:@"WebEditor [WK1]"];
    [containerView addSubview:_webView];
}

- (void)loadHTMLString:(NSString *)content
{
    [[_webView mainFrame] loadHTMLString:content baseURL:nil];
}

- (IBAction)pasteAsMarkup:(id)sender
{
    // FIXME: This is probably incorrect, should use WebArchive
    NSString *markup = [[NSPasteboard generalPasteboard] stringForType:NSStringPboardType];
    [_webView replaceSelectionWithMarkupString:markup ? markup : @""];
}

@end
