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

- (BOOL)searchFor: (NSString *)string direction: (BOOL)forward caseSensitive: (BOOL)caseFlag
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

        range = [textContents findString:string selectedRange:[self selectedRange] options:options wrap:YES];
        if (range.length) {
            [self setSelectedRange:range];
            [self scrollRangeToVisible:range];
            lastFindWasSuccessful = YES;
        }
    }

    return lastFindWasSuccessful;
}

@end

@implementation NSString (_Web_StringTextFinding)

- (NSRange)findString:(NSString *)string selectedRange:(NSRange)selectedRange options:(unsigned)options wrap:(BOOL)wrap
{
    BOOL forwards = (options & NSBackwardsSearch) == 0;
    unsigned length = [self length];
    NSRange searchRange, range;

    if (forwards) {
        searchRange.location = NSMaxRange(selectedRange);
        searchRange.length = length - searchRange.location;
        range = [self rangeOfString:string options:options range:searchRange];
        if ((range.length == 0) && wrap) {	/* If not found look at the first part of the string */
            searchRange.location = 0;
            searchRange.length = selectedRange.location;
            range = [self rangeOfString:string options:options range:searchRange];
        }
    } else {
        searchRange.location = 0;
        searchRange.length = selectedRange.location;
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

