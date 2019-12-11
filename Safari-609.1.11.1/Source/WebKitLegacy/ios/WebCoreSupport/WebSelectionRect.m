/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#import "WebSelectionRect.h"

@implementation WebSelectionRect {
    CGRect m_rect;
    WKWritingDirection m_writingDirection;
    BOOL m_isLineBreak;
    BOOL m_isFirstOnLine;
    BOOL m_isLastOnLine;
    BOOL m_containsStart;
    BOOL m_containsEnd;
    BOOL m_isInFixedPosition;
    BOOL m_isHorizontal;
}

@synthesize rect = m_rect;
@synthesize writingDirection = m_writingDirection;
@synthesize isLineBreak = m_isLineBreak;
@synthesize isFirstOnLine = m_isFirstOnLine;
@synthesize isLastOnLine = m_isLastOnLine;
@synthesize containsStart = m_containsStart;
@synthesize containsEnd = m_containsEnd;
@synthesize isInFixedPosition = m_isInFixedPosition;
@synthesize isHorizontal = m_isHorizontal;

+ (WebSelectionRect *)selectionRect
{
    return [[(WebSelectionRect *)[self alloc] init] autorelease];
}

+ (CGRect)startEdge:(NSArray *)rects
{
    if (rects.count == 0)
        return CGRectZero;

    WebSelectionRect *selectionRect = nil;
    for (WebSelectionRect *srect in rects) {
        if (srect.containsStart) {
            selectionRect = srect;
            break;
        }
    }
    if (!selectionRect)
        selectionRect = [rects objectAtIndex:0];
    
    CGRect rect = selectionRect.rect;
    
    if (!selectionRect.isHorizontal) {
        switch (selectionRect.writingDirection) {
            case WKWritingDirectionNatural:
            case WKWritingDirectionLeftToRight:
                // collapse to top edge
                rect.size.height = 1;
                break;
            case WKWritingDirectionRightToLeft:
                // collapse to bottom edge
                rect.origin.y += (rect.size.height - 1);
                rect.size.height = 1;
                break;
        }
        return rect;
    }
    
    switch (selectionRect.writingDirection) {
        case WKWritingDirectionNatural:
        case WKWritingDirectionLeftToRight:
            // collapse to left edge
            rect.size.width = 1;
            break;
        case WKWritingDirectionRightToLeft:
            // collapse to right edge
            rect.origin.x += (rect.size.width - 1);
            rect.size.width = 1;
            break;
    }
    return rect;
}

+ (CGRect)endEdge:(NSArray *)rects
{
    if (rects.count == 0)
        return CGRectZero;

    WebSelectionRect *selectionRect = nil;
    for (WebSelectionRect *srect in rects) {
        if (srect.containsEnd) {
            selectionRect = srect;
            break;
        }
    }
    if (!selectionRect)
        selectionRect = [rects lastObject];

    CGRect rect = selectionRect.rect;
    
    if (!selectionRect.isHorizontal) {
        switch (selectionRect.writingDirection) {
            case WKWritingDirectionNatural:
            case WKWritingDirectionLeftToRight:
                // collapse to bottom edge
                rect.origin.y += (rect.size.height - 1);
                rect.size.height = 1;
                break;
            case WKWritingDirectionRightToLeft:
                // collapse to top edge
                rect.size.height = 1;
                break;
        }
        return rect;
    }
    
    switch (selectionRect.writingDirection) {
        case WKWritingDirectionNatural:
        case WKWritingDirectionLeftToRight:
            // collapse to right edge
            rect.origin.x += (rect.size.width - 1);
            rect.size.width = 1;
            break;
        case WKWritingDirectionRightToLeft:
            // collapse to left edge
            rect.size.width = 1;
            break;
    }
    return rect;
}

- (id)init
{
    self = [super init];
    if (!self)
        return nil;
    
    self.rect = CGRectZero;
    self.writingDirection = WKWritingDirectionLeftToRight;
    self.isLineBreak = NO;
    self.isFirstOnLine = NO;
    self.isLastOnLine = NO;
    self.containsStart = NO;
    self.containsEnd = NO;
    self.isInFixedPosition = NO;
    self.isHorizontal = NO;
    
    return self;
}

- (id)copyWithZone:(NSZone *)zone
{
    WebSelectionRect *copy = [[WebSelectionRect selectionRect] retain];
    copy.rect = self.rect;
    copy.writingDirection = self.writingDirection;
    copy.isLineBreak = self.isLineBreak;
    copy.isFirstOnLine = self.isFirstOnLine;
    copy.isLastOnLine = self.isLastOnLine;
    copy.containsStart = self.containsStart;
    copy.containsEnd = self.containsEnd;
    copy.isInFixedPosition = self.isInFixedPosition;
    copy.isHorizontal = self.isHorizontal;
    return copy;
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<WebSelectionRect: %p> : { %.1f,%.1f,%.1f,%.1f } %@%@%@%@%@%@%@%@",
        self, 
        self.rect.origin.x, self.rect.origin.y, self.rect.size.width, self.rect.size.height,
        self.writingDirection == WKWritingDirectionLeftToRight ? @"[LTR]" : @"[RTL]",
        self.isLineBreak ? @" [BR]" : @"",
        self.isFirstOnLine ? @" [FIRST]" : @"",
        self.isLastOnLine ? @" [LAST]" : @"",
        self.containsStart ? @" [START]" : @"",
        self.containsEnd ? @" [END]" : @"",
        self.isInFixedPosition ? @" [FIXED]" : @"",
        !self.isHorizontal ? @" [VERTICAL]" : @""];
}

@end

#endif  // PLATFORM(IOS_FAMILY)
