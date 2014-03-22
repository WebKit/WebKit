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

#import "WebNSStringDrawing.h"

// FIXME: Delete this file after testing to be sure it doesn't break any apps or frameworks.

#if PLATFORM(IOS)

@implementation NSString (WebStringDrawing)

+ (void)_web_setWordRoundingEnabled:(BOOL)flag
{
}

+ (BOOL)_web_wordRoundingEnabled
{
    return NO;
}

+ (void)_web_setWordRoundingAllowed:(BOOL)flag
{
}

+ (BOOL)_web_wordRoundingAllowed
{
    return YES;
}

+ (void)_web_setAscentRoundingEnabled:(BOOL)flag
{
}

+ (BOOL)_web_ascentRoundingEnabled
{
    return NO;
}

- (CGSize)_web_drawAtPoint:(CGPoint)point withFont:(GSFontRef)font
{
    return CGSizeZero;
}

- (CGSize)_web_sizeWithFont:(GSFontRef)font
{
    return CGSizeZero;
}

- (CGSize)_web_sizeWithFont:(GSFontRef)font forWidth:(float)width ellipsis:(WebEllipsisStyle)ellipsisStyle
{
    return CGSizeZero;
}

- (CGSize)_web_sizeWithFont:(GSFontRef)font forWidth:(float)width ellipsis:(WebEllipsisStyle)ellipsisStyle letterSpacing:(float)letterSpacing
{
    return CGSizeZero;
}

- (CGSize)_web_sizeWithFont:(GSFontRef)font forWidth:(float)width ellipsis:(WebEllipsisStyle)ellipsisStyle letterSpacing:(float)letterSpacing resultRange:(NSRange *)resultRangeOut
{
    return CGSizeZero;
}

- (CGSize)_web_drawAtPoint:(CGPoint)point forWidth:(float)width withFont:(GSFontRef)font ellipsis:(WebEllipsisStyle)ellipsisStyle
{
    return CGSizeZero;
}

- (CGSize)_web_drawAtPoint:(CGPoint)point forWidth:(float)width withFont:(GSFontRef)font ellipsis:(WebEllipsisStyle)ellipsisStyle letterSpacing:(float)letterSpacing
{
    return CGSizeZero;
}

- (CGSize)_web_drawAtPoint:(CGPoint)point forWidth:(float)width withFont:(GSFontRef)font ellipsis:(WebEllipsisStyle)ellipsisStyle letterSpacing:(float)letterSpacing includeEmoji:(BOOL)includeEmoji
{
    return CGSizeZero;
}

- (CGSize)_web_drawInRect:(CGRect)rect withFont:(GSFontRef)font ellipsis:(WebEllipsisStyle)ellipsisStyle alignment:(WebTextAlignment)alignment lineSpacing:(int)lineSpacing includeEmoji:(BOOL)includeEmoji truncationRect:(CGRect *)truncationRect measureOnly:(BOOL)measureOnly
{
    if (truncationRect)
        *truncationRect = CGRectZero;
    return CGSizeZero;
}

- (CGSize)_web_drawInRect:(CGRect)rect withFont:(GSFontRef)font ellipsis:(WebEllipsisStyle)ellipsisStyle alignment:(WebTextAlignment)alignment lineSpacing:(int)lineSpacing includeEmoji:(BOOL)includeEmoji truncationRect:(CGRect *)truncationRect
{
    if (truncationRect)
        *truncationRect = CGRectZero;
    return CGSizeZero;
}

- (CGSize)_web_drawInRect:(CGRect)rect withFont:(GSFontRef)font ellipsis:(WebEllipsisStyle)ellipsisStyle alignment:(WebTextAlignment)alignment lineSpacing:(int)lineSpacing
{
    return CGSizeZero;
}

- (CGSize)_web_drawInRect:(CGRect)rect withFont:(GSFontRef)font ellipsis:(WebEllipsisStyle)ellipsisStyle alignment:(WebTextAlignment)alignment
{
    return CGSizeZero;
}

- (CGSize)_web_sizeInRect:(CGRect)rect withFont:(GSFontRef)font ellipsis:(WebEllipsisStyle)ellipsisStyle lineSpacing:(int)lineSpacing
{
    return CGSizeZero;
}

- (CGSize)_web_sizeInRect:(CGRect)rect withFont:(GSFontRef)font ellipsis:(WebEllipsisStyle)ellipsisStyle
{
    return CGSizeZero;
}

- (NSString *)_web_stringForWidth:(float)width withFont:(GSFontRef)font ellipsis:(WebEllipsisStyle)ellipsisStyle letterSpacing:(float)letterSpacing includeEmoji:(BOOL)includeEmoji
{
    return @"";
}

- (CGSize)_web_sizeForWidth:(CGFloat)width withAttributes:(id <WebTextRenderingAttributes>)attributes
{
    return CGSizeZero;
}

- (CGSize)_web_drawAtPoint:(CGPoint)point forWidth:(CGFloat)width withAttributes:(id <WebTextRenderingAttributes>)attributes
{
    return CGSizeZero;
}

- (CGSize)_web_sizeInRect:(CGRect)rect withAttributes:(id <WebTextRenderingAttributes>)attributes
{
    return CGSizeZero;
}

- (CGSize)_web_drawInRect:(CGRect)rect withAttributes:(id <WebTextRenderingAttributes>)attributes
{
    return CGSizeZero;
}

@end

#endif // PLATFORM(IOS)
