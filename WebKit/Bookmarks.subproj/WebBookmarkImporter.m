//
//  WebBookmarkImporter.m
//  WebKit
//
//  Created by Ken Kocienda on Sun Nov 17 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "WebBookmarkImporter.h"

#import <WebFoundation/WebAssertions.h>
#import <WebFoundation/WebError.h>
#import <WebFoundation/WebLocalizableStrings.h>
#import <WebFoundation/WebNSStringExtras.h>

#import <WebKit/WebBookmark.h>
#import <WebKit/WebBookmarkPrivate.h>
#import <WebKit/WebKitErrors.h>

static NSMutableArray *_breakStringIntoLines(NSString *string)
{
    NSMutableArray *lines = [NSMutableArray array];

    unsigned length = [string length];
    unsigned index = 0;
    unsigned lineStart = 0;
    unsigned lineEnd = 0;
    BOOL blankLine = YES;
    
    while (index < length) {
        BOOL crlf = NO;
        unichar c = [string characterAtIndex:index];
        switch (c) {
            case '\r':
                if (index < length - 1) {
                    if ([string characterAtIndex:index + 1] == '\n') {
                        index++;
                        crlf = YES;
                    }
                }
                // fall through
            case '\n':
            case 0x2028: // unicode line break character
            case 0x2029: // ditto
                if (!blankLine) {
                    NSString *line = [[string substringWithRange:NSMakeRange(lineStart, lineEnd - lineStart + 1)]
                        _web_stringByTrimmingWhitespace];
                    
                    // Simple hack to make parsing work better: Break lines before any <DT> or </DL>.
                    while (1) {
                        unsigned lineBreakLocation = 0;

                        // Break before the first <DT> or </DL>.
                        NSRange rangeOfDT = [line rangeOfString:@"<DT>" options:(NSCaseInsensitiveSearch | NSLiteralSearch)];
                        if (rangeOfDT.location != NSNotFound) {
                            lineBreakLocation = rangeOfDT.location;
                        }
                        NSRange rangeOfDL = [line rangeOfString:@"</DL>" options:(NSCaseInsensitiveSearch | NSLiteralSearch)];
                        if (rangeOfDL.location != NSNotFound && rangeOfDL.location < lineBreakLocation) {
                            lineBreakLocation = rangeOfDL.location;
                        }
                        
                        if (lineBreakLocation == 0) {
                            break;
                        }
                        
                        // Turn the part before the break into a line.
                        [lines addObject:[[line substringToIndex:lineBreakLocation] _web_stringByTrimmingWhitespace]];
                        
                        // Keep going with the part after the break.
                        line = [[line substringFromIndex:lineBreakLocation] _web_stringByTrimmingWhitespace];
                    }
                    
                    [lines addObject:line];
                    blankLine = YES;
                }
                lineStart = index + 1;
                if (crlf) {
                    lineStart++;
                }
                break;
            default:
                blankLine = NO;
                lineEnd = index;
                break;
        }
        
        index++;
    }
    
    return lines;
}

static NSString *_HREFTextFromSpec(NSString *spec)
{
    NSRange startRange = [spec rangeOfString:@"HREF=\"" options:(NSCaseInsensitiveSearch | NSLiteralSearch)];
    if (startRange.location == NSNotFound) {
        return nil;
    }
    
    NSRange remainder;
    remainder.location = startRange.location + startRange.length;
    remainder.length = [spec length] - remainder.location;
    
    NSRange endRange = [spec rangeOfString:@"\"" options:NSLiteralSearch range:remainder];
    if (endRange.location == NSNotFound) {
        return nil;
    }
    
    return [spec substringWithRange:NSMakeRange(remainder.location, endRange.location - remainder.location)];
}

static NSRange _linkTextRangeFromSpec(NSString *spec)
{
    NSRange result;
    unsigned startIndex;
    unsigned endIndex;
    int closeBrackets;
    int openBrackets;
    int length;
    int i;
    unichar c;
    
    result.location = NSNotFound;
    result.length = -1;
    length = [spec length];
    startIndex = 0;
    endIndex = 0;

    closeBrackets = 0;
    for (i = 0; i < length; i++) {
        c = [spec characterAtIndex:i];
        if (c == '>') {
            closeBrackets++;
        }
        else if (closeBrackets == 2 && startIndex == 0) {
            startIndex = i;
            break;
        }
    }

    openBrackets = 0;
    for (i = length - 1; i > 0; i--) {
        c = [spec characterAtIndex:i];
        if (c == '<') {
            openBrackets = 1;
        }
        else if (openBrackets == 1) {
            endIndex = i + 1;
            break;
        }
    }

    if (startIndex > 0 && endIndex > startIndex) {
        result.location = startIndex;
        result.length = endIndex - startIndex;
    }
    
    return result;
}

static NSString *_linkTextFromSpec(NSString *spec)
{
    NSString *result = nil;

    NSRange range = _linkTextRangeFromSpec(spec);
    
    if (range.location != NSNotFound) {
        result = [spec substringWithRange:range];
    }
    
    return result;
}


@implementation WebBookmarkImporter

-(id)initWithPath:(NSString *)path
{
    self = [super init];
    if (!self) {
        return nil;
    }
    
    NSData *data = [[NSData alloc] initWithContentsOfFile:path];
    if (!data) {
        error = [[WebError alloc] initWithErrorCode:WebErrorCannotOpenFile inDomain:WebErrorDomainWebKit failingURL:path];
    }

    NSString *string = [[NSString alloc] initWithData:data encoding:NSISOLatin1StringEncoding];
    NSArray *lines = _breakStringIntoLines(string);
    int lineCount = [lines count];
    int i;
    
    WebBookmark *bookmark;
    WebBookmark *currentBookmarkList;
    NSMutableArray *bookmarkLists = [NSMutableArray array];
    unsigned numberOfChildren;
    
    // create the top-level folder
    topBookmark = [[WebBookmark bookmarkOfType:WebBookmarkTypeList] retain];
    [bookmarkLists addObject:topBookmark];
    
    for (i = 0; i < lineCount; i++) {
        NSString *line = [lines objectAtIndex:i];
        
        if ([line _web_hasCaseInsensitivePrefix:@"<DT><H"]) {
            // a bookmark folder specifier
            bookmark = [WebBookmark bookmarkOfType:WebBookmarkTypeList];
            currentBookmarkList = [bookmarkLists lastObject];
            // find the folder name
            NSString *title = _linkTextFromSpec(line);
            if (!title) {
                // Rather than skipping the folder altogether, make one so we don't get unbalanced with </DL>.
                title = @"?";
            }
            [bookmark setTitle:title];
            numberOfChildren = [currentBookmarkList numberOfChildren];
            [currentBookmarkList insertChild:bookmark atIndex:numberOfChildren];
            [bookmarkLists addObject:bookmark];
        }
        else if ([line _web_hasCaseInsensitivePrefix:@"<DT><A"]) {
            // a bookmark or folder specifier
            bookmark = [WebBookmark bookmarkOfType:WebBookmarkTypeLeaf];
            currentBookmarkList = [bookmarkLists lastObject];
            // find the folder name
            NSString *title = _linkTextFromSpec(line);
            if (!title) {
                continue;
            }
            if ([title isEqualToString:@"-"]) {
                // skip dividers
                continue;
            }
            [bookmark setTitle:title];
            NSString *href = _HREFTextFromSpec(line);
            if (!href) {
                continue;
            }
            [bookmark setURLString:href];
            numberOfChildren = [currentBookmarkList numberOfChildren];
            [currentBookmarkList insertChild:bookmark atIndex:numberOfChildren];
        }
        else if ([line _web_hasCaseInsensitivePrefix:@"</DL>"]) {
            // ends a bookmark list
            if ([bookmarkLists count] == 0) {
                ERROR("unbalanced </DL>, doesn't match the number of <DT><H we saw");
            } else {
                [bookmarkLists removeLastObject];
            }
        }
    }
    
    return self;
}

-(WebBookmark *)topBookmark
{
    return topBookmark;
}

-(WebError *)error
{
    return error;
}

-(void)dealloc
{
    [topBookmark release];
    [error release];
    
    [super dealloc];
}

@end

