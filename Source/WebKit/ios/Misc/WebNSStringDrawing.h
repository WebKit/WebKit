/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef WebNSStringDrawing_h
#define WebNSStringDrawing_h

// FIXME: Delete this header after testing to be sure it doesn't break any apps or frameworks.

#if TARGET_OS_IPHONE

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <GraphicsServices/GraphicsServices.h>

typedef enum {
    // The order of the enum items is important, and it is used for >= comparisions
    WebEllipsisStyleNone = 0, // Old style, no truncation. Doesn't respect the "width" passed to it. Left in for compatability.
    WebEllipsisStyleHead = 1,
    WebEllipsisStyleTail = 2,
    WebEllipsisStyleCenter = 3,
    WebEllipsisStyleClip = 4, // Doesn't really clip, but instad truncates at the last character.
    WebEllipsisStyleWordWrap = 5, // Truncates based on the width/height passed to it.
    WebEllipsisStyleCharacterWrap = 6, // For "drawAtPoint", it is just like WebEllipsisStyleClip, since it doesn't really clip, but truncates at the last character
} WebEllipsisStyle;

typedef enum {
    WebTextAlignmentLeft = 0,
    WebTextAlignmentCenter = 1,
    WebTextAlignmentRight = 2,
} WebTextAlignment;

@protocol WebTextRenderingAttributes

@property(nonatomic, readonly)          GSFontRef font; // font the text should be rendered with (defaults to nil)
@property(nonatomic, readonly)          CGFloat lineSpacing; // set to specify the line spacing (defaults to 0.0 which indicates the default line spacing)

@property(nonatomic, readonly)          WebEllipsisStyle ellipsisStyle; // text will be wrapped and truncated according to the line break mode (defaults to UILineBreakModeWordWrap)
@property(nonatomic, readonly)          CGFloat letterSpacing; // number of extra pixels to be added or subtracted between each pair of characters  (defalts to 0)
@property(nonatomic, readonly)          WebTextAlignment alignment; // specifies the horizontal alignment that should be used when rendering the text (defaults to UITextAlignmentLeft)
@property(nonatomic, readonly)          BOOL includeEmoji; // if yes, the text can include Emoji characters.
@property(nonatomic, readwrite)         CGRect truncationRect; // the truncation rect argument, if non-nil, will be used instead of an ellipsis character for truncation sizing. if no truncation occurs, the truncationRect will be changed to CGRectNull. If truncation occurs, the rect will be updated with its placement.

@property(nonatomic, readonly)          NSString **renderString; // An out-parameter for the actual rendered string. Defaults to nil.
@property(nonatomic, readonly)          BOOL drawUnderline; // if yes, the text will be painted with underline.

@end

@interface NSString (WebStringDrawing)

+ (void)_web_setWordRoundingEnabled:(BOOL)flag;
+ (BOOL)_web_wordRoundingEnabled;

+ (void)_web_setWordRoundingAllowed:(BOOL)flag;
+ (BOOL)_web_wordRoundingAllowed;

+ (void)_web_setAscentRoundingEnabled:(BOOL)flag;
+ (BOOL)_web_ascentRoundingEnabled;

- (CGSize)_web_drawAtPoint:(CGPoint)point withFont:(GSFontRef)font;

- (CGSize)_web_sizeWithFont:(GSFontRef)font;

// Size after applying ellipsis style and clipping to width.
- (CGSize)_web_sizeWithFont:(GSFontRef)font forWidth:(float)width ellipsis:(WebEllipsisStyle)ellipsisStyle;
- (CGSize)_web_sizeWithFont:(GSFontRef)font forWidth:(float)width ellipsis:(WebEllipsisStyle)ellipsisStyle letterSpacing:(float)letterSpacing;
- (CGSize)_web_sizeWithFont:(GSFontRef)font forWidth:(float)width ellipsis:(WebEllipsisStyle)ellipsisStyle letterSpacing:(float)letterSpacing resultRange:(NSRange *)resultRangeOut;

// Draw text to fit width. Clip or apply ellipsis according to style.
- (CGSize)_web_drawAtPoint:(CGPoint)point forWidth:(float)width withFont:(GSFontRef)font ellipsis:(WebEllipsisStyle)ellipsisStyle;
- (CGSize)_web_drawAtPoint:(CGPoint)point forWidth:(float)width withFont:(GSFontRef)font ellipsis:(WebEllipsisStyle)ellipsisStyle letterSpacing:(float)letterSpacing;
- (CGSize)_web_drawAtPoint:(CGPoint)point forWidth:(float)width withFont:(GSFontRef)font ellipsis:(WebEllipsisStyle)ellipsisStyle letterSpacing:(float)letterSpacing includeEmoji:(BOOL)includeEmoji;

// Wrap and clip to rect.
- (CGSize)_web_drawInRect:(CGRect)rect withFont:(GSFontRef)font ellipsis:(WebEllipsisStyle)ellipsisStyle alignment:(WebTextAlignment)alignment;
- (CGSize)_web_drawInRect:(CGRect)rect withFont:(GSFontRef)font ellipsis:(WebEllipsisStyle)ellipsisStyle alignment:(WebTextAlignment)alignment lineSpacing:(int)lineSpacing;
- (CGSize)_web_drawInRect:(CGRect)rect withFont:(GSFontRef)font ellipsis:(WebEllipsisStyle)ellipsisStyle alignment:(WebTextAlignment)alignment lineSpacing:(int)lineSpacing includeEmoji:(BOOL)includeEmoj truncationRect:(CGRect *)truncationRect;
- (CGSize)_web_sizeInRect:(CGRect)rect withFont:(GSFontRef)font ellipsis:(WebEllipsisStyle)ellipsisStyle;
- (CGSize)_web_sizeInRect:(CGRect)rect withFont:(GSFontRef)font ellipsis:(WebEllipsisStyle)ellipsisStyle lineSpacing:(int)lineSpacing;

// Clip or apply ellipsis according to style. Return the string which results.
- (NSString *)_web_stringForWidth:(float)width withFont:(GSFontRef)font ellipsis:(WebEllipsisStyle)ellipsisStyle letterSpacing:(float)letterSpacing includeEmoji:(BOOL)includeEmoji;

// These methods should eventually replace all string drawing/sizing methods above

// Sizing/drawing a single line of text
- (CGSize)_web_sizeForWidth:(CGFloat)width withAttributes:(id <WebTextRenderingAttributes>)attributes;
- (CGSize)_web_drawAtPoint:(CGPoint)point forWidth:(CGFloat)width withAttributes:(id <WebTextRenderingAttributes>)attributes;

// Sizing/drawing multiline text
- (CGSize)_web_sizeInRect:(CGRect)rect withAttributes:(id <WebTextRenderingAttributes>)attributes;
- (CGSize)_web_drawInRect:(CGRect)rect withAttributes:(id <WebTextRenderingAttributes>)attributes;

@end

#endif // TARGET_OS_IPHONE

#endif // WebNSStringDrawing_h
