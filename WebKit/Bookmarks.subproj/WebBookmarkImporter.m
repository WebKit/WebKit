//
//  WebBookmarkImporter.m
//  WebKit
//
//  Created by Ken Kocienda on Sun Nov 17 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "WebBookmarkImporter.h"

#import <WebFoundation/WebError.h>
#import <WebFoundation/WebLocalizableStrings.h>
#import <WebKit/WebBookmark.h>
#import <WebKit/WebBookmarkPrivate.h>
#import <WebKit/WebBookmarkGroup.h>
#import <WebKit/WebKitErrors.h>

static NSMutableArray *_breakStringIntoLines(NSString *string)
{
    unsigned length;
    unsigned index;
    unsigned lineStart;
    unsigned lineEnd;
    unichar c;
    BOOL crlf;
    BOOL blankLine;
    NSString *line;
    NSCharacterSet *whitespaceCharacterSet;

    NSMutableArray *lines = [NSMutableArray array];

    length = [string length];
    index = 0;
    lineStart = 0;
    lineEnd = 0;
    crlf = NO;
    blankLine = YES;
    whitespaceCharacterSet = [NSCharacterSet whitespaceCharacterSet];
    
    while (index < length) {
        crlf = NO;
        c = [string characterAtIndex:index];
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
                    line = [string substringWithRange:NSMakeRange(lineStart, lineEnd - lineStart + 1)];
                    line = [line stringByTrimmingCharactersInSet:whitespaceCharacterSet];
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

static NSRange _HREFRangeFromSpec(NSString *spec)
{
    NSRange result;
    unsigned startIndex;
    unsigned endIndex;
    int length;
    int i;
    unichar c;
    NSRange range;

    result.location = NSNotFound;
    result.length = -1;
    length = [spec length];
    startIndex = 0;
    endIndex = 0;

    range = [spec rangeOfString:@"HREF="];
    if (range.location == NSNotFound) {
        return result;
    }
    startIndex = range.location + range.length;
    // account for quote
    startIndex++;

    endIndex = length - startIndex;
    for (i = startIndex; i < length; i++) {
        c = [spec characterAtIndex:i];
        if (c == '"') {
            endIndex = i;
            break;
        }
    }

    result.location = startIndex;
    result.length = endIndex - startIndex;

    return result;
}

static NSString *_HREFTextFromSpec(NSString *spec)
{
    NSString *result = nil;

    NSRange range = _HREFRangeFromSpec(spec);
    
    if (range.location != NSNotFound) {
        result = [spec substringWithRange:range];
    }
    
    return result;
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

-(id)initWithPath:(NSString *)path group:(WebBookmarkGroup *)group
{
    self = [super init];
    if (!self) {
        return nil;
    }
    
    NSData *data = [[NSData alloc] initWithContentsOfFile:path];
    if (!data) {
        error = [WebError errorWithCode:WebErrorCannotOpenFile inDomain:WebErrorDomainWebKit failingURL:path];
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
    [topBookmark setTitle:UI_STRING("Imported IE Favorites", "Imported IE Favorites menu item")];
    [topBookmark _setGroup:group];
    [bookmarkLists addObject:topBookmark];
    
    for (i = 0; i < lineCount; i++) {
        NSString *line = [lines objectAtIndex:i];
        
        if ([line hasPrefix:@"<DL>"]) {
            // ignore this line
            // we recognize new lists by parsing "<DT><H" lines
        }
        else if ([line hasPrefix:@"<DT><H"]) {
            // a bookmark folder specifier
            bookmark = [WebBookmark bookmarkOfType:WebBookmarkTypeList];
            currentBookmarkList = [bookmarkLists lastObject];
            // find the folder name
            NSString *title = _linkTextFromSpec(line);
            if (!title) {
                continue;
            }
            [bookmark setTitle:title];
            numberOfChildren = [currentBookmarkList numberOfChildren];
            [currentBookmarkList insertChild:bookmark atIndex:numberOfChildren];
            [bookmarkLists addObject:bookmark];
        }
        else if ([line hasPrefix:@"<DT><A"]) {
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
        else if ([line hasPrefix:@"</DL>"]) {
            // ends a bookmark list
            [bookmarkLists removeLastObject];
        }
        else {
            // ignore this line
        }
    }

    bookmark = [group topBookmark];
    numberOfChildren = [bookmark numberOfChildren];
    [bookmark insertChild:topBookmark atIndex:numberOfChildren];
    
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

