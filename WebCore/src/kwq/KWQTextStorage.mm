/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import <Cocoa/Cocoa.h> 

#include <kwqdebug.h>

#import <KWQTextStorage.h>
#import <KWQTextContainer.h>

/*
    This class is a dumb text storage implementation.  It is optimized for speed,
    not flexibility.  It only support read-only text with font and color attributes.
    
    It is used, along with KWQTextContainer to spoof NSLayoutManager.  KWQTextStorage
    and KWQTextContainer are both used as singletons.  They are set/reset to contain the
    correct attributes (font and color only) during layout and rendering.
*/

#ifndef UINT16_MAX
#define UINT16_MAX        65535
#endif

@implementation KWQTextStorage

- (id <KWQLayoutFragment>)_buildFragmentForString: (NSString *)measureString
{
    id <KWQLayoutFragment> fragment;
    NSRange range = NSMakeRange (0, [measureString length]);
    NSRange glyphRange;
    NSRect boundingRect;

    if (fragmentCache == nil)
        fragmentCache = [[NSMutableDictionary alloc] init];
    
    [self setString: measureString];
    
    glyphRange = [_layoutManager glyphRangeForCharacterRange:range actualCharacterRange:nil];
    boundingRect = [_layoutManager boundingRectForGlyphRange: glyphRange inTextContainer: [KWQTextContainer sharedInstance]];
    
    bool useLargeFragment = NO;

    if (boundingRect.origin.x != 0 || boundingRect.origin.y != 0 ||
            glyphRange.location != 0 ||
            glyphRange.length > UINT16_MAX || 
            boundingRect.size.width > (float)UINT16_MAX ||
            boundingRect.size.height > (float)UINT16_MAX){
        useLargeFragment = YES;
    }

    if (useLargeFragment)
        fragment = [[KWQLargeLayoutFragment alloc] init];
    else
        fragment = [[KWQSmallLayoutFragment alloc] init];

    [fragment setGlyphRange: glyphRange];
    [fragment setBoundingRect: boundingRect];

    [fragmentCache setObject: fragment forKey: [self string]];
    [fragment release];

    return fragment;
}


- (id <KWQLayoutFragment>)getFragmentForString: (NSString *)fString
{
    id <KWQLayoutFragment> fragment;
    
#ifdef SPACE_OPTIMIZATION
    bool hasLeadingSpace = NO, hasTrailingSpace = NO;
    const UniChar *clippedStart = 0;
    unsigned int clippedLength = 0, fragStringLength;
    CFStringRef clippedString, fragString = (CFStringRef)fString;

    fragStringLength = CFStringGetLength (fragString);
    if (fragStringLength > 1){
        const UniChar *internalBuffer = CFStringGetCharactersPtr((CFStringRef)fragString);
        
        if (internalBuffer){
            clippedStart = internalBuffer;
            clippedLength = fragStringLength;
            if (*internalBuffer == ' '){
                hasLeadingSpace = YES;
                clippedStart++;
                clippedLength--;
            }
            if (internalBuffer[fragStringLength-1] == ' '){
                hasTrailingSpace = YES;
                clippedLength--;
            }

            if (hasLeadingSpace || hasTrailingSpace){
                NSRect boundingRect;
                NSRange glyphRange;
                
                clippedString = CFStringCreateWithCharactersNoCopy (kCFAllocatorDefault, clippedStart, clippedLength, kCFAllocatorNull);
                fragment = [fragmentCache objectForKey: (NSString *)clippedString];
                if (!fragment)
                    fragment = [self _buildFragmentForString: (NSString *)clippedString];
                CFRelease (clippedString);
                boundingRect = [fragment boundingRect];
                glyphRange = [fragment glyphRange];
                if (hasLeadingSpace){
                    glyphRange.length++;
                    boundingRect.size.width += [spaceFragment boundingRect].size.width;
                }
                if (hasTrailingSpace){
                    glyphRange.length++;
                    boundingRect.size.width += [spaceFragment boundingRect].size.width;
                }
                [expandedFragment setBoundingRect: boundingRect];
                [expandedFragment setGlyphRange: glyphRange];
                
                return expandedFragment;
            }
        }
    }
#endif

    fragment = [fragmentCache objectForKey: fString];
    if (!fragment)
        fragment = [self _buildFragmentForString: fString];
        
    return fragment;
}

#ifdef DEBUG_SPACE_OPTIMIZATION
static int totalMeasurements = 0;
static int leadingSpace = 0;
static int trailingSpace = 0;
#endif

- (id)initWithFontAttribute:(NSDictionary *)attrs 
{
    attributes = [attrs retain];
#ifdef SPACE_OPTIMIZATION
    expandedFragment = [[KWQLargeLayoutFragment alloc] init];
#endif
    return self;
}

- (void)dealloc 
{
    [string release];
    [fragmentCache release];
    [attributes release];
    [super dealloc];
}

#ifdef _DEBUG_LAYOUT_FRAGMENT
- (NSDictionary *)fragmentCache { return fragmentCache; }
#endif

- (void)addLayoutManager:(id)obj {
    _layoutManager = [obj retain];
    [obj setTextStorage:self];
#ifdef SPACE_OPTIMIZATION
    spaceFragment = [self _buildFragmentForString: @" "];
#endif
}

- (void)removeLayoutManager:(id)obj {
    if (obj == _layoutManager){
        [obj setTextStorage:nil];
        [obj autorelease];
    }
}

- (NSArray *)layoutManagers {
    return [NSArray arrayWithObjects: _layoutManager, nil];
}

- (void)edited:(unsigned)mask range:(NSRange)oldRange changeInLength:(int)lengthChange
{
    KWQDEBUG ("not implemented");
}

- (void)ensureAttributesAreFixedInRange:(NSRange)range
{
    KWQDEBUG ("not implemented");
}

- (void)invalidateAttributesInRange:(NSRange)range
{
    KWQDEBUG ("not implemented");
}

- (unsigned)length 
{
    return [string length];
}

- (void)setAttributes: (NSDictionary *)at
{
    if (at != attributes){
        [attributes release];
        attributes = [at retain];
    }
}

- (void)setString: (NSString *)newString
{
    if (newString != string){
        int oldLength = [self length];
        int newLength = [newString length];
        
        [string release];
        
        // Make an immutable copy.
        string = [[NSString stringWithString: newString] retain];
        
        [_layoutManager textStorage: self 
                edited: NSTextStorageEditedCharacters
                range: NSMakeRange (0, newLength)
                changeInLength: newLength - oldLength
                invalidatedRange:NSMakeRange (0, newLength)];
    }
}

- (NSString *)string 
{
    return string;
}


- (NSDictionary *)attributesAtIndex:(unsigned)location effectiveRange:(NSRangePointer)range 
{
    if (range != 0){
        range->location = 0;
        range->length = [self length];
    }
    return attributes;
}


- (NSDictionary *)attributesAtIndex:(unsigned)location longestEffectiveRange:(NSRangePointer)range inRange:(NSRange)rangeLimit
{
    if (range != 0){
        range->location = rangeLimit.location;
        range->length = rangeLimit.length;
    }
    return attributes;
}


- (id)attribute:(NSString *)attrName atIndex:(unsigned)location effectiveRange:(NSRangePointer)range 
{
    if (range != 0){
        range->location = 0;
        range->length = [self length];
    }
    return [attributes objectForKey: attrName];
}


- (void)setAttributes:(NSDictionary *)attrs range:(NSRange)range 
{
    [NSException raise:@"NOT IMPLEMENTED" format:@"- (void)setAttributes:(NSDictionary *)attrs range:(NSRange)range"];
}


- (void)replaceCharactersInRange:(NSRange)range withAttributedString:(NSAttributedString *)str 
{
    [NSException raise:@"NOT IMPLEMENTED" format:@"- (void)replaceCharactersInRange:(NSRange)range withAttributedString:(NSAttributedString *)str"];
}


- (void)addAttribute:(NSString *)name value:value range:(NSRange)range
{
    [NSException raise:@"NOT IMPLEMENTED" format:@"- (void)addAttribute:(NSString *)name value:value range:(NSRange)range"];
}


- (NSAttributedString *)attributedSubstringFromRange:(NSRange)aRange
{
    [NSException raise:@"NOT IMPLEMENTED" format:@"- (NSAttributedString *)attributedSubstringFromRange:(NSRange)aRange"];
    return nil;
}

@end

