//
//  WebNSPasteboardExtras.m
//  WebKit
//
//  Created by John Sullivan on Thu Sep 19 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "WebNSPasteboardExtras.h"

#import "WebController.h"

#import <WebFoundation/WebNSStringExtras.h>
#import <WebFoundation/WebNSURLExtras.h>

@implementation NSPasteboard (WebExtras)

+ (NSArray *)_web_dragTypesForURL
{
    return [NSArray arrayWithObjects:NSURLPboardType, NSStringPboardType, NSFilenamesPboardType, nil];
}

-(NSURL *)_web_bestURL
{
    NSArray *types = [self types];

    if ([types containsObject:NSURLPboardType]) {
        NSURL *URLFromPasteboard;
        NSString *scheme;

        URLFromPasteboard = [NSURL URLFromPasteboard:self];
        scheme = [URLFromPasteboard scheme];
        if ([scheme isEqualToString:@"http"] || [scheme isEqualToString:@"https"]) {
            return URLFromPasteboard;
        }
    }

    if ([types containsObject:NSStringPboardType]) {
        NSString *URLString;

        URLString = [[self stringForType:NSStringPboardType] _web_stringByTrimmingWhitespace];
        if ([URLString _web_looksLikeAbsoluteURL]) {
            return [NSURL _web_URLWithString:URLString];
        }
    }

    if ([types containsObject:NSFilenamesPboardType]) {
        NSArray *files;

        files = [self propertyListForType:NSFilenamesPboardType];
        if ([files count] == 1) {
            NSString *file;

            file = [files objectAtIndex:0];
            if ([WebController canShowFile:file]) {
                return [NSURL fileURLWithPath:file];
            }
        }
    }

    return nil;
}

@end
