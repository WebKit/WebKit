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

/*
    This class is a dumb text storage implementation.  It is optimized for speed,
    not flexibility.  It only support read-only text with font and color attributes.
    
    It is used, along with KWQTextContainer to spoof NSLayoutManager.  KWQTextStorage
    and KWQTextContainer are both used as singletons.  They are set/reset to contain the
    correct attributes (font and color only) during layout and rendering.
*/

@implementation KWQTextStorage

static KWQTextStorage *sharedInstance = nil;

+ (KWQTextStorage *)sharedInstance
{
    if (sharedInstance == nil)
        sharedInstance = [[KWQTextStorage alloc] init];
    return sharedInstance;
}

+ setString:(NSString *)str attributes:(NSDictionary *)attrs 
{
    [[KWQTextStorage sharedInstance] setString: str attributes: attrs];
}

- (id)initWithString:(NSString *)str attributes:(NSDictionary *)attrs 
{
    attrString = [str retain];
    attributes = [attrs retain];
    return self;
}

- (void)dealloc 
{
    [attributes release];
    [attrString release];
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
    return [attrString length];
}

- (void)setString: (NSString *)aString attributes: (NSDictionary *)at
{
    if (aString != attrString){
        [attrString release];
        attrString = [aString retain];
    }

    if (at != attributes){
        [attributes release];
        attributes = [at retain];
    }
}


- (NSString *)string 
{
    return attrString;
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

