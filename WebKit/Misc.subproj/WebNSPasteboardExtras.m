//
//  WebNSPasteboardExtras.m
//  WebKit
//
//  Created by John Sullivan on Thu Sep 19 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WebController.h>
#import <WebKit/WebNSPasteboardExtras.h>
#import <WebKit/WebURLsWithTitles.h>

#import <WebFoundation/WebAssertions.h>
#import <WebFoundation/WebNSStringExtras.h>
#import <WebFoundation/WebNSURLExtras.h>

#import <ApplicationServices/ApplicationServicesPriv.h>

NSString *WebURLPboardType = nil;
NSString *WebURLNamePboardType = nil;

@implementation NSPasteboard (WebExtras)

+ (void)initialize
{
    CreatePasteboardFlavorTypeName('url ', (CFStringRef*)&WebURLPboardType);
    CreatePasteboardFlavorTypeName('urln', (CFStringRef*)&WebURLNamePboardType);
}

+ (NSArray *)_web_dragTypesForURL
{
    return [NSArray arrayWithObjects:NSURLPboardType, NSStringPboardType, NSFilenamesPboardType, nil];
}

-(NSURL *)_web_bestURL
{
    NSArray *types = [self types];

    if ([types containsObject:NSURLPboardType]) {
        NSURL *URLFromPasteboard = [NSURL URLFromPasteboard:self];
        NSString *scheme = [URLFromPasteboard scheme];
        if ([scheme isEqualToString:@"http"] || [scheme isEqualToString:@"https"]) {
            return [URLFromPasteboard _web_canonicalize];
        }
    }

    if ([types containsObject:NSStringPboardType]) {
        NSString *URLString = [[self stringForType:NSStringPboardType] _web_stringByTrimmingWhitespace];
        if ([URLString _web_looksLikeAbsoluteURL]) {
            return [[NSURL _web_URLWithString:URLString] _web_canonicalize];
        }
    }

    if ([types containsObject:NSFilenamesPboardType]) {
        NSArray *files = [self propertyListForType:NSFilenamesPboardType];
        if ([files count] == 1) {
            NSString *file = [files objectAtIndex:0];
            BOOL isDirectory;
            if([[NSFileManager defaultManager] fileExistsAtPath:file isDirectory:&isDirectory] && !isDirectory){
                if ([WebController canShowFile:file]) {
                    return [[NSURL fileURLWithPath:file] _web_canonicalize];
                }
            }
        }
    }

    return nil;
}

- (void)_web_writeURL:(NSURL *)URL andTitle:(NSString *)title withOwner:(id)owner
{
    ASSERT(URL);
    
    NSArray *types = [NSArray arrayWithObjects:WebURLsWithTitlesPboardType, NSURLPboardType, NSStringPboardType, nil];
    [self declareTypes:types owner:owner];

    [URL writeToPasteboard:self];
    [self setString:[URL absoluteString] forType:NSStringPboardType];

    NSArray *titles = nil;
    if(title && ![title isEqualToString:@""]){
        titles = [NSArray arrayWithObject:title];
        [self setString:title forType:WebURLNamePboardType];
    }
    
    [WebURLsWithTitles writeURLs:[NSArray arrayWithObject:URL] andTitles:titles toPasteboard:self];
    [self setString:[URL absoluteString] forType:WebURLPboardType];
}

@end
