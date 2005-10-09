/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "TextInputController.h"

#import <AppKit/NSInputManager.h>
#import <WebKit/WebDocument.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebView.h>

@implementation TextInputController

// FIXME: need to support attributed strings

+ (BOOL)isSelectorExcludedFromWebScript:(SEL)aSelector
{
    if (aSelector == @selector(insertText:)
            || aSelector == @selector(doCommand:)
            || aSelector == @selector(setMarkedText:selectedFrom:length:)
            || aSelector == @selector(unmarkText)
            || aSelector == @selector(hasMarkedText)
            || aSelector == @selector(conversationIdentifier)
            || aSelector == @selector(substringFrom:length:)
            || aSelector == @selector(markedRange)
            || aSelector == @selector(selectedRange)
            || aSelector == @selector(firstRectForCharactersFrom:length:)
            || aSelector == @selector(characterIndexForPointX:Y:)
            || aSelector == @selector(validAttributesForMarkedText))
        return NO;
    return YES;
}

+ (NSString *)webScriptNameForSelector:(SEL)aSelector
{
    if (aSelector == @selector(insertText:))
        return @"insertText";
    else if (aSelector == @selector(doCommand:))
        return @"doCommand";
    else if (aSelector == @selector(setMarkedText:selectedFrom:length:))
        return @"setMarkedText";
    else if (aSelector == @selector(substringFrom:length:))
        return @"substringFromRange";
    else if (aSelector == @selector(firstRectForCharactersFrom:length:))
        return @"firstRectForCharacterRange";
    else if (aSelector == @selector(characterIndexForPointX:Y:))
        return @"characterIndexForPoint";

    return nil;
}

- (id)initWithWebView:(WebView *)wv
{
    self = [super init];
    webView = wv;
    return self;
}

- (NSObject <NSTextInput> *)textInput
{
    NSView <WebDocumentView> *view = [[[webView mainFrame] frameView] documentView];
    return [view conformsToProtocol:@protocol(NSTextInput)] ? view : nil;
}

- (void)insertText:(NSString *)aString
{
    NSObject <NSTextInput> *textInput = [self textInput];

    if (textInput)
        [textInput insertText:aString];
}

- (void)doCommand:(NSString *)aCommand
{
    NSObject <NSTextInput> *textInput = [self textInput];

    if (textInput)
        [textInput doCommandBySelector:NSSelectorFromString(aCommand)];
}

- (void)setMarkedText:(NSString *)aString selectedFrom:(int)from length:(int)length
{
    NSObject <NSTextInput> *textInput = [self textInput];
 
    if (textInput)
        [textInput setMarkedText:aString selectedRange:NSMakeRange(from, length)];
}

- (void)unmarkText
{
    NSObject <NSTextInput> *textInput = [self textInput];

    if (textInput)
        [textInput unmarkText];
}

- (BOOL)hasMarkedText
{
    NSObject <NSTextInput> *textInput = [self textInput];

    if (textInput)
        return [textInput hasMarkedText];

    return FALSE;
}

- (long)conversationIdentifier
{
    NSObject <NSTextInput> *textInput = [self textInput];

    if (textInput)
        return [textInput conversationIdentifier];

    return 0;
}

- (NSString *)substringFrom:(int)from length:(int)length
{
    NSObject <NSTextInput> *textInput = [self textInput];

    if (textInput)
        return [[textInput attributedSubstringFromRange:NSMakeRange(from, length)] string];
    
    return @"";
}

- (NSArray *)markedRange
{
    NSObject <NSTextInput> *textInput = [self textInput];

    if (textInput) {
        NSRange range = [textInput markedRange];
        return [NSArray arrayWithObjects:[NSNumber numberWithUnsignedInt:range.location], [NSNumber numberWithUnsignedInt:range.length], nil];
    }

    return nil;
}

- (NSArray *)selectedRange
{
    NSObject <NSTextInput> *textInput = [self textInput];

    if (textInput) {
        NSRange range = [textInput selectedRange];
        return [NSArray arrayWithObjects:[NSNumber numberWithUnsignedInt:range.location], [NSNumber numberWithUnsignedInt:range.length], nil];
    }

    return nil;
}
  

- (NSArray *)firstRectForCharactersFrom:(int)from length:(int)length
{
    NSObject <NSTextInput> *textInput = [self textInput];

    if (textInput) {
        NSRect rect = [textInput firstRectForCharacterRange:NSMakeRange(from, length)];
        return [NSArray arrayWithObjects:
                    [NSNumber numberWithFloat:rect.origin.x],
                    [NSNumber numberWithFloat:rect.origin.y],
                    [NSNumber numberWithFloat:rect.size.width],
                    [NSNumber numberWithFloat:rect.size.height],
                    nil];
    }

    return nil;
}

- (int)characterIndexForPointX:(float)x Y:(float)y
{
    NSObject <NSTextInput> *textInput = [self textInput];

    if (textInput)
        return [textInput characterIndexForPoint:NSMakePoint(x, y)];

    return 0;
}

- (NSArray *)validAttributesForMarkedText
{
    NSObject <NSTextInput> *textInput = [self textInput];

    if (textInput)
        return [textInput validAttributesForMarkedText];

    return nil;
}

@end
