/*
 * Copyright (C) 2005, 2007, 2016 Apple Inc.  All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#import "config.h"
#import "TextInputController.h"

#if PLATFORM(MAC)

#import "DumpRenderTreeMac.h"
#import <AppKit/NSInputManager.h>
#import <AppKit/NSTextAlternatives.h>
#import <WebKit/WebDocument.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebScriptObject.h>
#import <WebKit/WebView.h>
#import <WebKit/WebViewPrivate.h>
#import <pal/spi/mac/NSTextInputContextSPI.h>

@interface TextInputController (DumpRenderTreeInputMethodHandler)
- (BOOL)interpretKeyEvents:(NSArray *)eventArray withSender:(WebHTMLView *)sender;
@end

@interface WebHTMLView (DumpRenderTreeInputMethodHandler)
- (void)interpretKeyEvents:(NSArray *)eventArray;
@end

@interface WebHTMLView (WebKitSecretsTextInputControllerIsAwareOf)
- (WebFrame *)_frame;
- (NSAttributedString *)_attributedStringFromDOMRange:(DOMRange *)range;
- (NSAttributedString *)_legacyAttributedStringFrom:(DOMNode*)startContainer offset:(int)startOffset to:(DOMNode*)endContainer offset:(int)endOffset;
@end

@implementation WebHTMLView (DumpRenderTreeInputMethodHandler)
- (void)interpretKeyEvents:(NSArray *)eventArray
{
    WebScriptObject *obj = [[self _frame] windowObject];
    TextInputController *tic = [obj valueForKey:@"textInputController"];
    if (![tic interpretKeyEvents:eventArray withSender:self])
        [super interpretKeyEvents:eventArray];
}
@end

@interface WebNSRange : NSObject {
@private
    NSRange _range;
}
- (id)initWithNSRange:(NSRange)range;
- (unsigned)location;
- (unsigned)length;
@end

@implementation WebNSRange

- (id)initWithNSRange:(NSRange)range
{
    self = [super init];
    if (!self)
        return self;

    _range = range;
    return self;
}

- (unsigned)location
{
    return _range.location;
}

- (unsigned)length
{
    return _range.length;
}

+ (BOOL)isSelectorExcludedFromWebScript:(SEL)selector
{
    return !(selector == @selector(location) || selector == @selector(length));
}

@end

@implementation NSMutableAttributedString (TextInputController)

+ (BOOL)isSelectorExcludedFromWebScript:(SEL)aSelector
{
    if (aSelector == @selector(string)
        || aSelector == @selector(getLength)
        || aSelector == @selector(ranges)
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
    if (aSelector == @selector(ranges))
        return @"ranges";
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

- (NSArray *)ranges
{
    NSMutableArray *array = [NSMutableArray array];
    [self enumerateAttributesInRange:NSMakeRange(0, [self length]) options:0 usingBlock:^(NSDictionary *attributes, NSRange range, BOOL *stop) {
        WebNSRange *webRange = [[WebNSRange alloc] initWithNSRange:range];
        [array addObject:webRange];
        [webRange release];
    }];
    return array;
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
        || aSelector == @selector(setMarkedText:selectedFrom:length:suppressUnderline:highlights:)
        || aSelector == @selector(unmarkText)
        || aSelector == @selector(hasMarkedText)
        || aSelector == @selector(conversationIdentifier)
        || aSelector == @selector(substringFrom:length:)
        || aSelector == @selector(attributedSubstringFrom:length:)
        || aSelector == @selector(legacyAttributedString:offset:to:offset:)
        || aSelector == @selector(markedRange)
        || aSelector == @selector(selectedRange)
        || aSelector == @selector(firstRectForCharactersFrom:length:)
        || aSelector == @selector(characterIndexForPointX:Y:)
        || aSelector == @selector(validAttributesForMarkedText)
        || aSelector == @selector(attributedStringForTyping)
        || aSelector == @selector(attributedStringWithString:)
        || aSelector == @selector(setInputMethodHandler:)
        || aSelector == @selector(dictatedStringWithPrimaryString:alternative:alternativeOffset:alternativeLength:)
        || aSelector == @selector(stringWithUndoGroupingInsertion:))
        return NO;
    return YES;
}

+ (NSString *)webScriptNameForSelector:(SEL)aSelector
{
    if (aSelector == @selector(insertText:))
        return @"insertText";
    if (aSelector == @selector(doCommand:))
        return @"doCommand";
    if (aSelector == @selector(setMarkedText:selectedFrom:length:suppressUnderline:highlights:))
        return @"setMarkedText";
    if (aSelector == @selector(substringFrom:length:))
        return @"substringFromRange";
    if (aSelector == @selector(attributedSubstringFrom:length:))
        return @"attributedSubstringFromRange";
    if (aSelector == @selector(legacyAttributedString:offset:to:offset:))
        return @"legacyAttributedString";
    if (aSelector == @selector(firstRectForCharactersFrom:length:))
        return @"firstRectForCharacterRange";
    if (aSelector == @selector(characterIndexForPointX:Y:))
        return @"characterIndexForPoint";
    if (aSelector == @selector(attributedStringWithString:))
        return @"makeAttributedString"; // just a factory method, doesn't call into NSTextInput
    if (aSelector == @selector(setInputMethodHandler:))
        return @"setInputMethodHandler";
    if (aSelector == @selector(dictatedStringWithPrimaryString:alternative:alternativeOffset:alternativeLength:))
        return @"makeDictatedString";
    if (aSelector == @selector(stringWithUndoGroupingInsertion:))
        return @"makeUndoGroupingInsertionString";

    return nil;
}

- (id)initWithWebView:(WebView *)wv
{
    self = [super init];
    webView = wv;
    inputMethodView = nil;
    inputMethodHandler = nil;
    return self;
}

- (void)dealloc
{
    [inputMethodHandler release];
    inputMethodHandler = nil;
    
    [super dealloc];
}

- (WebFrame *)selectedOrMainFrame
{
    return webView.selectedFrame ?: webView.mainFrame;
}

- (NSObject <NSTextInput> *)textInput
{
    NSView <NSTextInput> *view = inputMethodView ?: (id)self.selectedOrMainFrame.frameView.documentView;
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

- (void)setMarkedText:(NSString *)aString selectedFrom:(int)from length:(int)length suppressUnderline:(BOOL)suppressUnderline highlights:(NSArray<NSDictionary *> *)highlights
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

- (NSAttributedString *)legacyAttributedString:(DOMNode*)startContainer offset:(int)startOffset to:(DOMNode*)endContainer offset:(int)endOffset
{
    id documentView = [[[webView mainFrame] frameView] documentView];
    if (![documentView isKindOfClass:[WebHTMLView class]])
        return nil;

    return [(WebHTMLView *)documentView _legacyAttributedStringFrom:startContainer offset:startOffset to:endContainer offset:endOffset];
}

- (NSArray *)markedRange
{
    NSObject <NSTextInput> *textInput = [self textInput];

    if (textInput) {
        NSRange range = [textInput markedRange];
        return @[@(range.location), @(range.length)];
    }

    return nil;
}

- (NSArray *)selectedRange
{
    NSObject <NSTextInput> *textInput = [self textInput];

    if (textInput) {
        NSRange range = [textInput selectedRange];
        return @[@(range.location), @(range.length)];
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
        return @[ @(rect.origin.x), @(rect.origin.y), @(rect.size.width), @(rect.size.height) ];
    }

    return nil;
}

- (NSInteger)characterIndexForPointX:(float)x Y:(float)y
{
    NSObject <NSTextInput> *textInput = [self textInput];

    if (textInput) {
        NSPoint point = NSMakePoint(x, y);
        point = [webView convertPoint:point toView:nil];
        point = [[webView window] convertBaseToScreen:point];
        NSInteger index = [textInput characterIndexForPoint:point];
        if (index == NSNotFound)
            return -1;

        return index;
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

- (NSMutableAttributedString *)attributedStringForTyping
{
    // The string has to be non-empty.
    return [[[NSMutableAttributedString alloc] initWithString:@" " attributes:[webView typingAttributes]] autorelease];
}

- (NSMutableAttributedString *)attributedStringWithString:(NSString *)string
{
    return [[[NSMutableAttributedString alloc] initWithString:string] autorelease];
}

- (NSMutableAttributedString *)stringWithUndoGroupingInsertion:(NSString *)string
{
    NSMutableAttributedString* attributedString = [self dictatedStringWithPrimaryString:string alternative:@"test" alternativeOffset:0 alternativeLength:1];
    [attributedString addAttribute:NSTextInsertionUndoableAttributeName value:@YES range:NSMakeRange(0, [attributedString length])];
    return attributedString;
}

- (NSMutableAttributedString*)dictatedStringWithPrimaryString:(NSString*)aString alternative:(NSString*)alternative alternativeOffset:(int)offset alternativeLength:(int)length
{
    NSMutableAttributedString* dictatedString = [self attributedStringWithString:aString];
    NSRange rangeWithAlternative = NSMakeRange((NSUInteger)offset, (NSUInteger)length);
    NSString* subStringWithAlternative = [aString substringWithRange:rangeWithAlternative];
    if (!subStringWithAlternative)
        return nil;

    NSTextAlternatives* alternativeObject = [[[NSTextAlternatives alloc] initWithPrimaryString:subStringWithAlternative alternativeStrings:@[alternative]] autorelease];
    if (!alternativeObject)
        return nil;

    [dictatedString addAttribute:NSTextAlternativesAttributeName value:alternativeObject range:rangeWithAlternative];

    return dictatedString;
}

- (void)setInputMethodHandler:(WebScriptObject *)handler
{
    if (inputMethodHandler == handler)
        return;
    [handler retain];
    [inputMethodHandler release];
    inputMethodHandler = handler;
}

- (BOOL)interpretKeyEvents:(NSArray *)eventArray withSender:(WebHTMLView *)sender
{
    if (!inputMethodHandler)
        return NO;
    
    inputMethodView = sender;
    
    NSEvent *event = [eventArray objectAtIndex:0];
    unsigned modifierFlags = [event modifierFlags]; 
    NSMutableArray *modifiers = [[NSMutableArray alloc] init];
    if (modifierFlags & NSEventModifierFlagCapsLock)
        [modifiers addObject:@"NSAlphaShiftKeyMask"];
    if (modifierFlags & NSEventModifierFlagShift)
        [modifiers addObject:@"NSShiftKeyMask"];
    if (modifierFlags & NSEventModifierFlagControl)
        [modifiers addObject:@"NSControlKeyMask"];
    if (modifierFlags & NSEventModifierFlagOption)
        [modifiers addObject:@"NSAlternateKeyMask"];
    if (modifierFlags & NSEventModifierFlagCommand)
        [modifiers addObject:@"NSCommandKeyMask"];
    if (modifierFlags & NSEventModifierFlagNumericPad)
        [modifiers addObject:@"NSNumericPadKeyMask"];
    if (modifierFlags & NSEventModifierFlagHelp)
        [modifiers addObject:@"NSHelpKeyMask"];
    if (modifierFlags & NSEventModifierFlagFunction)
        [modifiers addObject:@"NSFunctionKeyMask"];
    
    WebScriptObject* eventParam = [inputMethodHandler evaluateWebScript:@"new Object();"];
    [eventParam setValue:[event characters] forKey:@"characters"];
    [eventParam setValue:[event charactersIgnoringModifiers] forKey:@"charactersIgnoringModifiers"];
    [eventParam setValue:[NSNumber numberWithBool:[event isARepeat]] forKey:@"isARepeat"];
    [eventParam setValue:[NSNumber numberWithUnsignedShort:[event keyCode]] forKey:@"keyCode"];
    [eventParam setValue:modifiers forKey:@"modifierFlags"];

    [modifiers release];
    
    id result = [inputMethodHandler callWebScriptMethod:@"call" withArguments:@[inputMethodHandler, eventParam]];
    if (![result respondsToSelector:@selector(boolValue)] || ![result boolValue]) {
        IGNORE_WARNINGS_BEGIN("undeclared-selector")
        [sender doCommandBySelector:@selector(noop:)]; // AppKit sends noop: if the ime does not handle an event
        IGNORE_WARNINGS_END
    }

    inputMethodView = nil;
    return YES;
}

@end

#endif // PLATFORM(MAC)
