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

@implementation KWQTextStorage

- (KWQLayoutFragment *)getFragmentForString: (NSString *)fragString
{
    return [fragmentCache objectForKey: fragString];
}

- (KWQLayoutFragment *)addFragmentForString: (NSString *)fragString
{
    KWQLayoutFragment *fragment;
    
    if (fragmentCache == nil)
        fragmentCache = [[NSMutableDictionary alloc] init];

    int fragStringLength = [fragString length];

    fragment = [[KWQLayoutFragment alloc] init];

    [fragmentCache setObject: fragment forKey: fragString];
    
    [self setString: fragString];

    NSRange range = NSMakeRange (0, fragStringLength);
    NSRange glyphRange = [_layoutManager glyphRangeForCharacterRange:range actualCharacterRange:nil];
    
    if (glyphRange.location != 0){
        [NSException raise:@"OPTIMIZATION ASSUMPTION VIOLATED" format:@"glyphRange.location != 0"];
    }

    [fragment setGlyphRangeLength: glyphRange.length];
    
    NSRect boundingRect = [_layoutManager boundingRectForGlyphRange: glyphRange inTextContainer: [KWQTextContainer sharedInstance]];
    
    if (boundingRect.origin.x != 0 || boundingRect.origin.y != 0){
        [NSException raise:@"OPTIMIZATION ASSUMPTION VIOLATED" format:@"bounding rect origin not 0,0"];
    }
    
    [fragment setBoundingRectSize: boundingRect.size];

    [fragment release];
    
    return fragment;
}

- (id)initWithFontAttribute:(NSDictionary *)attrs 
{
    attributes = [attrs retain];
    return self;
}

- (void)dealloc 
{
    [string release];
    [fragmentCache release];
    [attributes release];
    [super dealloc];
}

- (void)addLayoutManager:(id)obj {
    _layoutManager = [obj retain];
    [obj setTextStorage:self];
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
        string = [newString retain];
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

