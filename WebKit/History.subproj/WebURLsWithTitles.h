//
//  WebURLsWithTitles.h
//  WebKit
//
//  Created by John Sullivan on Wed Jun 12 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//
// Convenience class for getting URLs and associated titles on and off an NSPasteboard

#import <Cocoa/Cocoa.h>

#define WebURLsWithTitlesPboardType	@"WebURLsWithTitlesPboardType"

@interface WebURLsWithTitles : NSObject

// Writes parallel arrays of URLs and titles to the pasteboard. These items can be retrieved by
// calling URLsFromPasteboard and titlesFromPasteboard. URLs must consist of NSURL objects.
// titles must consist of NSStrings, or be nil. If titles is nil, or if titles is a different
// length than URLs, empty strings will be used for all titles. If URLs is nil, this method
// returns without doing anything. You must declare an WebURLsWithTitlesPboardType data type
// for pasteboard before invoking this method, or it will return without doing anything.
+ (void)writeURLs:(NSArray *)URLs andTitles:(NSArray *)titles toPasteboard:(NSPasteboard *)pasteboard;

// Reads an array of NSURLs off the pasteboard. Returns nil if pasteboard does not contain
// data of type WebURLsWithTitlesPboardType. This array consists of the URLs that correspond to
// the titles returned from titlesFromPasteboard.
+ (NSArray *)URLsFromPasteboard:(NSPasteboard *)pasteboard;

// Reads an array of NSStrings off the pasteboard. Returns nil if pasteboard does not contain
// data of type WebURLsWithTitlesPboardType. This array consists of the titles that correspond to
// the URLs returned from URLsFromPasteboard.
+ (NSArray *)titlesFromPasteboard:(NSPasteboard *)pasteboard;

@end
