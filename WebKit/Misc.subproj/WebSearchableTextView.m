//
//  WebSearchableTextView.m
//  WebKit
//
//  Created by John Sullivan on Fri Aug 16 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "WebSearchableTextView.h"
#import "WebDocument.h"

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

@end

@implementation NSString (_Web_StringTextFinding)

- (NSRange)findString:(NSString *)string selectedRange:(NSRange)selectedRange options:(unsigned)options wrap:(BOOL)wrap
{
    BOOL forwards = (options & NSBackwardsSearch) == 0;
    unsigned length = [self length];
    NSRange searchRange, range;

    if (forwards) {
        searchRange.location = selectedRange.length > 0 ? NSMaxRange(selectedRange) : 0;
        searchRange.length = length - searchRange.location;
        range = [self rangeOfString:string options:options range:searchRange];
        if ((range.length == 0) && wrap) {	/* If not found look at the first part of the string */
            searchRange.location = 0;
            searchRange.length = selectedRange.location;
            range = [self rangeOfString:string options:options range:searchRange];
        }
    } else {
        searchRange.location = 0;
        searchRange.length = selectedRange.length > 0 ? selectedRange.location : length;
        range = [self rangeOfString:string options:options range:searchRange];
        if ((range.length == 0) && wrap) {
            searchRange.location = NSMaxRange(selectedRange);
            searchRange.length = length - searchRange.location;
            range = [self rangeOfString:string options:options range:searchRange];
        }
}
return range;
}

@end

