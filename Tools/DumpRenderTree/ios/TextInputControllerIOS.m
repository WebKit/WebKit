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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "TextInputController.h"

#if PLATFORM(IOS_FAMILY)

// FIXME: <rdar://problem/5106287> Only partial support for TextInputController has been implemented on iOS so far. We need to finish the implementation
// here to bring it up to parity with the Mac version (see TextInputControllerMac.m), and then reenable skipped iOS tests that use TextInputController.

#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebKitLegacy.h>

@implementation TextInputController

+ (BOOL)isSelectorExcludedFromWebScript:(SEL)aSelector
{
    if (aSelector == @selector(insertText:)
        || aSelector == @selector(setMarkedText:selectedFrom:length:suppressUnderline:highlights:)
        || aSelector == @selector(markedRange))
        return NO;

    return YES;
}

+ (NSString *)webScriptNameForSelector:(SEL)aSelector
{
    if (aSelector == @selector(insertText:))
        return @"insertText";
    if (aSelector == @selector(setMarkedText:selectedFrom:length:suppressUnderline:highlights:))
        return @"setMarkedText";
    if (aSelector == @selector(markedRange))
        return @"markedRange";

    return nil;
}

- (id)initWithWebView:(WebView *)wv
{
    if (self = [super init]) {
        webView = wv;
        inputMethodView = nil;
        inputMethodHandler = nil;
    }

    return self;
}

- (NSArray *)markedRange
{
    DOMRange *range = [[webView mainFrame] markedTextDOMRange];
    if (!range)
        return nil;

    return @[@([range startOffset]), @([range endOffset])];
}

- (void)insertText:(NSString *)text
{
    [[webView mainFrame] confirmMarkedText:text];
}

- (void)setMarkedText:(NSString *)text selectedFrom:(NSInteger)selectionStart length:(NSInteger)selectionLength suppressUnderline:(BOOL)suppressUnderline highlights:(NSArray<NSDictionary *> *)highlights
{
    if (selectionStart == -1)
        selectionStart = NSNotFound;

    [[webView mainFrame] setMarkedText:text selectedRange:NSMakeRange(selectionStart, selectionLength)];
}

@end

#endif
