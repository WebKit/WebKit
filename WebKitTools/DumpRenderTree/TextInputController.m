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

@implementation NSMutableAttributedString (TextInputController)

+ (BOOL)isSelectorExcludedFromWebScript:(SEL)aSelector
{
    if (aSelector == @selector(string)
            || aSelector == @selector(getLength)
            || aSelector == @selector(attributeNamesAtIndex:)
            || aSelector == @selector(valueOfAttribute:atIndex:)
            || aSelector == @selector(addAttribute:value:)
            || aSelector == @selector(addAttribute:value:from:length:)
            || aSelector == @selector(addColorAttribute:red:green:blue:alpha:)
            || aSelector == @selector(addColorAttribute:red:green:blue:alpha:from:length:)
            || aSelector == @selector(addFontAttribute:fontName:size:)
            || aSelector == @selector(addFontAttribute:fontName:size:from:length:))
        return NO;
    return YES;
}

+ (NSString *)webScriptNameForSelector:(SEL)aSelector
{
    if (aSelector == @selector(getLength))
        return @"length";
    if (aSelector == @selector(attributeNamesAtIndex:))
        return @"getAttributeNamesAtIndex";
    if (aSelector == @selector(valueOfAttribute:atIndex:))
        return @"getAttributeValueAtIndex";
    if (aSelector == @selector(addAttribute:value:))
        return @"addAttribute";
    if (aSelector == @selector(addAttribute:value:from:length:))
        return @"addAttributeForRange";
    if (aSelector == @selector(addColorAttribute:red:green:blue:alpha:))
        return @"addColorAttribute";
    if (aSelector == @selector(addColorAttribute:red:green:blue:alpha:from:length:))
        return @"addColorAttributeForRange";
    if (aSelector == @selector(addFontAttribute:fontName:size:))
        return @"addFontAttribute";
    if (aSelector == @selector(addFontAttribute:fontName:size:from:length:))
        return @"addFontAttributeForRange";

    return nil;
}

- (int)getLength
{
    return (int)[self length];
}

- (NSArray *)attributeNamesAtIndex:(int)index
{
    NSDictionary *attributes = [self attributesAtIndex:(unsigned)index effectiveRange:nil];
    return [attributes allKeys];
}

- (id)valueOfAttribute:(NSString *)attrName atIndex:(int)index
{
    return [self attribute:attrName atIndex:(unsigned)index effectiveRange:nil];
}

- (void)addAttribute:(NSString *)attrName value:(id)value
{
    [self addAttribute:attrName value:value range:NSMakeRange(0, [self length])];
}

- (void)addAttribute:(NSString *)attrName value:(id)value from:(int)from length:(int)length
{
    [self addAttribute:attrName value:value range:NSMakeRange((unsigned)from, (unsigned)length)];
}

- (void)addColorAttribute:(NSString *)attrName red:(float)red green:(float)green blue:(float)blue alpha:(float)alpha
{
    [self addAttribute:attrName value:[NSColor colorWithDeviceRed:red green:green blue:blue alpha:alpha] range:NSMakeRange(0, [self length])];
}

- (void)addColorAttribute:(NSString *)attrName red:(float)red green:(float)green blue:(float)blue alpha:(float)alpha from:(int)from length:(int)length
{
    [self addAttribute:attrName value:[NSColor colorWithDeviceRed:red green:green blue:blue alpha:alpha] range:NSMakeRange((unsigned)from, (unsigned)length)];
}

- (void)addFontAttribute:(NSString *)attrName fontName:(NSString *)fontName size:(float)fontSize
{
    [self addAttribute:attrName value:[NSFont fontWithName:fontName size:fontSize] range:NSMakeRange(0, [self length])];
}

- (void)addFontAttribute:(NSString *)attrName fontName:(NSString *)fontName size:(float)fontSize from:(int)from length:(int)length
{
    [self addAttribute:attrName value:[NSFont fontWithName:fontName size:fontSize] range:NSMakeRange((unsigned)from, (unsigned)length)];
}

@end

@implementation TextInputController

+ (BOOL)isSelectorExcludedFromWebScript:(SEL)aSelector
{
    if (aSelector == @selector(insertText:)
            || aSelector == @selector(doCommand:)
            || aSelector == @selector(setMarkedText:selectedFrom:length:)
            || aSelector == @selector(unmarkText)
            || aSelector == @selector(hasMarkedText)
            || aSelector == @selector(conversationIdentifier)
            || aSelector == @selector(substringFrom:length:)
            || aSelector == @selector(attributedSubstringFrom:length:)
            || aSelector == @selector(markedRange)
            || aSelector == @selector(selectedRange)
            || aSelector == @selector(firstRectForCharactersFrom:length:)
            || aSelector == @selector(characterIndexForPointX:Y:)
            || aSelector == @selector(validAttributesForMarkedText)
            || aSelector == @selector(attributedStringWithString:))
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
    else if (aSelector == @selector(attributedSubstringFrom:length:))
        return @"attributedSubstringFromRange";
    else if (aSelector == @selector(firstRectForCharactersFrom:length:))
        return @"firstRectForCharacterRange";
    else if (aSelector == @selector(characterIndexForPointX:Y:))
        return @"characterIndexForPoint";
    else if (aSelector == @selector(attributedStringWithString:))
        return @"makeAttributedString"; // just a factory method, doesn't call into NSTextInput

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
    NSView <NSTextInput> *view = [[[webView mainFrame] frameView] documentView];
    return [view conformsToProtocol:@protocol(NSTextInput)] ? view : nil;
}

- (void)insertText:(id)aString
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

- (NSMutableAttributedString *)attributedSubstringFrom:(int)from length:(int)length
{
    NSObject <NSTextInput> *textInput = [self textInput];

    NSMutableAttributedString *ret = [[[NSMutableAttributedString alloc] init] autorelease];

    if (textInput)
        [ret setAttributedString:[textInput attributedSubstringFromRange:NSMakeRange(from, length)]];
    
    return ret;
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
        if (rect.origin.x || rect.origin.y || rect.size.width || rect.size.height) {
            rect.origin = [[webView window] convertScreenToBase:rect.origin];
            rect = [webView convertRect:rect fromView:nil];
        }
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

    if (textInput) {
        NSPoint point = NSMakePoint(x, y);
        point = [webView convertPoint:point toView:nil];
        point = [[webView window] convertBaseToScreen:point];
        return [textInput characterIndexForPoint:point];
    }

    return 0;
}

- (NSArray *)validAttributesForMarkedText
{
    NSObject <NSTextInput> *textInput = [self textInput];

    if (textInput)
        return [textInput validAttributesForMarkedText];

    return nil;
}

- (NSMutableAttributedString *)attributedStringWithString:(NSString *)aString
{
    NSMutableAttributedString *ret = [[[NSMutableAttributedString alloc] init] autorelease];
    [ret setAttributedString:[[[NSAttributedString alloc] initWithString:aString] autorelease]];
    return ret;
}

@end
