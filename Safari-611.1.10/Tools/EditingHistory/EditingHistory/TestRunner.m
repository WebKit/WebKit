/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#import "TestRunner.h"

#import "TestUtil.h"
#import "WKWebViewAdditions.h"
#import <Carbon/Carbon.h>

static WKUserScript *injectedMessageEventHandlerScript(NSString *listener, NSString *event)
{
    NSString *source = [NSString stringWithFormat:@"%@.addEventListener('%@', () => {"
        "setTimeout(() => webkit.messageHandlers.eventHandler.postMessage('%@'), 0);"
        "});", listener, event, event];
    return [[WKUserScript alloc] initWithSource:source injectionTime:WKUserScriptInjectionTimeAtDocumentEnd forMainFrameOnly:YES];
}

@interface TestRunner () <WKScriptMessageHandler>

@property (nonatomic) NSMutableDictionary *pendingEventCounts;

@end

@implementation TestRunner

- (instancetype)init
{
    NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 800, 600) styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:NO];
    [window setFrameOrigin:NSMakePoint(0, 0)];
    [window setIsVisible:YES];

    WKWebView *webView = [[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)];
    [[window contentView] addSubview:webView];
    [window makeKeyAndOrderFront:webView];
    [webView becomeFirstResponder];

    _window = window;
    _webView = webView;
    _pendingEventCounts = nil;

    WKUserContentController *contentController = webView.configuration.userContentController;
    [contentController addScriptMessageHandler:self name:@"eventHandler"];
    [contentController addUserScript:injectedMessageEventHandlerScript(@"document.body", @"focus")];
    [contentController addUserScript:injectedMessageEventHandlerScript(@"document", @"input")];
    [contentController addUserScript:injectedMessageEventHandlerScript(@"document", @"selectionchange")];

    return self;
}

- (void)deleteBackwards:(NSInteger)times
{
    for (NSInteger i = 0; i < times; i++) {
        [self expectEvents:@{ @"input": @1, @"selectionchange": @1 } afterPerforming:^() {
            [_webView keyPressWithCharacters:nil keyCode:KeyCodeTypeDeleteBackward];
        }];
    }
}

- (void)typeString:(NSString *)string
{
    for (NSInteger charIndex = 0; charIndex < string.length; charIndex++) {
        [self expectEvents:@{ @"input": @1, @"selectionchange": @1 } afterPerforming:^() {
            [_webView typeCharacter:[string characterAtIndex:charIndex]];
        }];
    }
}

- (NSString *)bodyElementSubtree
{
    return [_webView stringByEvaluatingJavaScriptFromString:[NSString stringWithFormat:@"DOMUtil.subtreeAsString(document.body)"]];
}

- (NSString *)bodyTextContent
{
    return [_webView stringByEvaluatingJavaScriptFromString:[NSString stringWithFormat:@"document.body.textContent"]];
}

- (NSString *)editingHistoryJSON
{
    return [_webView stringByEvaluatingJavaScriptFromString:[NSString stringWithFormat:@"EditingHistory.getEditingHistoryAsJSONString(false)"]];
}

- (void)loadPlaybackTestHarnessWithJSON:(NSString *)json
{
    [_webView loadPageFromBundleNamed:@"PlaybackHarness"];
    waitUntil(CONDITION_BLOCK([[_webView stringByEvaluatingJavaScriptFromString:@"!!window.EditingHistory && !!EditingHistory.setupEditingHistory"] boolValue]));

    json = [[json stringByReplacingOccurrencesOfString:@"\\" withString:@"\\\\"] stringByReplacingOccurrencesOfString:@"`" withString:@"\\`"];
    [_webView stringByEvaluatingJavaScriptFromString:[NSString stringWithFormat:@"EditingHistory.setupEditingHistory(`%@`, false)", json]];
}

- (NSInteger)numberOfUpdates {
    return [[_webView stringByEvaluatingJavaScriptFromString:@"EditingHistory.context.updates().length"] integerValue];
}

- (void)jumpToUpdateIndex:(NSInteger)index
{
    [_webView stringByEvaluatingJavaScriptFromString:[NSString stringWithFormat:@"EditingHistory.context.jumpTo(%tu)", index]];
}

- (void)expectEvents:(NSDictionary<NSString *, NSNumber *> *)expectedEventCounts afterPerforming:(dispatch_block_t)action
{
    _pendingEventCounts = [expectedEventCounts mutableCopy];
    dispatch_async(dispatch_get_main_queue(), action);
    waitUntil(CONDITION_BLOCK(self.isDoneWaitingForPendingEvents));
    _pendingEventCounts = nil;
}

- (void)loadCaptureTestHarness
{
    [self expectEvents:@{ @"focus" : @1 } afterPerforming:^() {
        [_webView loadPageFromBundleNamed:@"CaptureHarness"];
    }];
}

- (void)setTextObfuscationEnabled:(BOOL)enabled
{
    [_webView stringByEvaluatingJavaScriptFromString:[NSString stringWithFormat:@"EditingHistory.Obfuscator.shared().enabled = %s", enabled ? "true" : "false"]];
}

- (BOOL)isDoneWaitingForPendingEvents
{
    NSInteger numberOfPendingEvents = 0;
    for (NSNumber *count in _pendingEventCounts.allValues)
        numberOfPendingEvents += [count integerValue];
    return _pendingEventCounts && !numberOfPendingEvents;
}

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    if ([message.name isEqualToString:@"eventHandler"] && _pendingEventCounts) {
        NSString *eventType = message.body;
        _pendingEventCounts[eventType] = @(MAX(0, [_pendingEventCounts[eventType] integerValue] - 1));
    }
}

@end
