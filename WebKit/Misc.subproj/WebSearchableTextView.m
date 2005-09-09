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

#import "WebSearchableTextView.h"
#import "WebDocumentPrivate.h"

@interface NSString (_Web_StringTextFinding)
- (NSRange)findString:(NSString *)string selectedRange:(NSRange)selectedRange options:(unsigned)mask wrap:(BOOL)wrapFlag;
@end

@implementation WebSearchableTextView

- (BOOL)searchFor: (NSString *)string direction: (BOOL)forward caseSensitive: (BOOL)caseFlag wrap: (BOOL)wrapFlag;
{
    BOOL lastFindWasSuccessful = NO;
    NSString *textContents = [self string];
    unsigned textLength;

    if (textContents && (textLength = [textContents length])) {
        NSRange range;
        unsigned options = 0;

        if (!forward)
            options |= NSBackwardsSearch;

        if (!caseFlag)
            options |= NSCaseInsensitiveSearch;

        range = [textContents findString:string selectedRange:[self selectedRange] options:options wrap:wrapFlag];
        if (range.length) {
            [self setSelectedRange:range];
            [self scrollRangeToVisible:range];
            lastFindWasSuccessful = YES;
        }
    }

    return lastFindWasSuccessful;
}

- (void)copy:(id)sender
{
    if ([self isRichText]) {
        [super copy:sender];
    }else{
        //Convert CRLF to LF to workaround: 3105538 - Carbon doesn't convert text with CRLF to LF
        NSMutableString *string = [[[self string] substringWithRange:[self selectedRange]] mutableCopy];
        [string replaceOccurrencesOfString:@"\r\n" withString:@"\n" options:0 range:NSMakeRange(0, [string length])];

        NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
        [pasteboard declareTypes:[NSArray arrayWithObject:NSStringPboardType] owner:self];
        [pasteboard setString:string forType:NSStringPboardType];
    }
}

- (NSRect)selectionRect
{
    // Note that this method would work for any NSTextView; some day we might want to use it
    // for an NSTextView that isn't a WebTextView.
    NSRect result = NSZeroRect;
    
    // iterate over multiple selected ranges
    NSEnumerator *rangeEnumerator = [[self selectedRanges] objectEnumerator];
    NSValue *rangeAsValue;
    while ((rangeAsValue = [rangeEnumerator nextObject]) != nil) {
        NSRange range = [rangeAsValue rangeValue];
        unsigned rectCount;
        NSRectArray rectArray = [[self layoutManager] rectArrayForCharacterRange:range 
                                                    withinSelectedCharacterRange:range 
                                                                 inTextContainer:[self textContainer] 
                                                                       rectCount:&rectCount];
        unsigned i;
        // iterate over multiple rects in each selected range
        for (i = 0; i < rectCount; ++i) {
            NSRect rect = rectArray[i];
            if (NSEqualRects(result, NSZeroRect)) {
                result = rect;
            } else {
                result = NSUnionRect(result, rect);
            }
        }
    }
    
    return result;
}

- (NSArray *)pasteboardTypesForSelection
{
    return [self writablePasteboardTypes];
}

- (void)writeSelectionWithPasteboardTypes:(NSArray *)types toPasteboard:(NSPasteboard *)pasteboard
{
    [self writeSelectionToPasteboard:pasteboard types:types];
}

@end

@implementation NSString (_Web_StringTextFinding)

- (NSRange)findString:(NSString *)string selectedRange:(NSRange)selectedRange options:(unsigned)options wrap:(BOOL)wrap
{
    BOOL forwards = (options & NSBackwardsSearch) == 0;
    unsigned length = [self length];
    NSRange searchRange, range;

    // Our search algorithm, used in WebCore also, is to search in the selection first. If the found text is the
    // entire selection, then we search again from just past the selection.
    
    if (forwards) {
        // FIXME: If selectedRange has length of 0, we ignore it, which is appropriate for non-editable text (since
        // a zero-length selection in non-editable is invisible). We might want to change this someday to only ignore the
        // selection if its location is NSNotFound when the text is editable (and similarly for the backwards case).
        searchRange.location = selectedRange.length > 0 ? selectedRange.location : 0;
        searchRange.length = length - searchRange.location;
        range = [self rangeOfString:string options:options range:searchRange];
        
        // If found range matches (non-empty) selection, search again from just past selection
        if (range.location != NSNotFound && NSEqualRanges(range, selectedRange)) {
            searchRange.location = NSMaxRange(selectedRange);
            searchRange.length = length - searchRange.location;
            range = [self rangeOfString:string options:options range:searchRange];
        }
        
        // If not found, search again from the beginning. Make search range large enough that
        // we'll find a match even if it partially overlapped the existing selection (including the
        // case where it exactly matches the existing selection).
        if ((range.length == 0) && wrap) {	
            searchRange.location = 0;
            searchRange.length = selectedRange.location + selectedRange.length + [string length];
            if (searchRange.length > length) {
                searchRange.length = length;
            }
            range = [self rangeOfString:string options:options range:searchRange];
        }
    } else {
        searchRange.location = 0;
        searchRange.length = selectedRange.length > 0 ? NSMaxRange(selectedRange) : length;
        range = [self rangeOfString:string options:options range:searchRange];
        
        // If found range matches (non-empty) selection, search again from just before selection
        if (range.location != NSNotFound && NSEqualRanges(range, selectedRange)) {
            searchRange.location = 0;
            searchRange.length = selectedRange.location;
            range = [self rangeOfString:string options:options range:searchRange];
        }
        
        // If not found, search again from the end. Make search range large enough that
        // we'll find a match even if it partially overlapped the existing selection (including the
        // case where it exactly matches the existing selection).
        if ((range.length == 0) && wrap) {
            unsigned stringLength = [string length];
            if (selectedRange.location > stringLength) {
                searchRange.location = selectedRange.location - stringLength;
            } else {
                searchRange.location = 0;
            }
            searchRange.length = length - searchRange.location;
            range = [self rangeOfString:string options:options range:searchRange];
        }
}
return range;
}

@end
