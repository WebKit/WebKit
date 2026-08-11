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
#import "WKWebViewAdditions.h"

#import "TestUtil.h"

#import <Carbon/Carbon.h>

@implementation WKWebView (EditingHistoryTest)

- (void)loadPageFromBundleNamed:(NSString *)name
{
    NSURL *pathToPage = [[NSBundle mainBundle] URLForResource:name withExtension:@"html" subdirectory:nil];
    [self loadRequest:[NSURLRequest requestWithURL:pathToPage]];
}

- (void)typeCharacter:(char)character
{
    [self keyPressWithCharacters:[NSString stringWithFormat:@"%c", character] keyCode:character];
}

- (void)keyPressWithCharacters:(nullable NSString *)characters keyCode:(char)keyCode
{
    NSEventType keyDownEventType = NSEventTypeKeyDown;
    NSEventType keyUpEventType = NSEventTypeKeyUp;
    [self keyDown:[NSEvent keyEventWithType:keyDownEventType location:NSZeroPoint modifierFlags:0 timestamp:GetCurrentEventTime() windowNumber:self.window.windowNumber context:nil characters:characters charactersIgnoringModifiers:characters isARepeat:NO keyCode:keyCode]];
    [self keyUp:[NSEvent keyEventWithType:keyUpEventType location:NSZeroPoint modifierFlags:0 timestamp:GetCurrentEventTime() windowNumber:self.window.windowNumber context:nil characters:characters charactersIgnoringModifiers:characters isARepeat:NO keyCode:keyCode]];
}

- (nullable NSString *)stringByEvaluatingJavaScriptFromString:(NSString *)script
{
    __block bool doneEvaluatingJavaScript = false;
    __block NSString *returnResult = nil;
    [self evaluateJavaScript:script completionHandler:^(id returnValue, NSError *error) {
        if (error)
            NSLog(@"Error evaluating JavaScript: %@", error);

        returnResult = error || !returnValue ? nil : [NSString stringWithFormat:@"%@", returnValue];
        doneEvaluatingJavaScript = true;
    }];
    waitUntil(CONDITION_BLOCK(doneEvaluatingJavaScript));
    return returnResult;
}

@end
