/*
 *  WebIconDatabasePrivate.h
 *  
 *
 *  Created by Chris Blumenberg on Tue Aug 27 2002.
 *  Copyright (c) 2002 Apple Computer Inc. All rights reserved.
 *
 */

#import <Cocoa/Cocoa.h>

@class WebFileDatabase;

@interface WebIconDatabasePrivate : NSObject {

@public
    WebFileDatabase *fileDatabase;

    NSMutableDictionary *iconURLToIcons;
    NSMutableDictionary *iconURLToRetainCount;
    NSMutableDictionary *iconURLToSiteURLs;
    NSMutableDictionary *siteURLToIconURL;    
    NSMutableDictionary *futureSiteURLToRetainCount;
    NSMutableDictionary *hostToSiteURLs;
    NSMutableDictionary *hostToBuiltItIcons;
    
    NSMutableSet *iconsOnDiskWithURLs;
    NSMutableSet *iconsToEraseWithURLs;
    NSMutableSet *iconsToSaveWithURLs;
    
    int cleanupCount;

    BOOL didCleanup;
    BOOL waitingToCleanup;

    NSMutableArray *htmlIcons;
}

@end

@interface WebIconDatabase (WebPrivate)

// Called by WebIconLoader to determine if a load is necessary.
- (NSImage *)_iconForIconURL:(NSURL *)iconURL;

// Called by WebIconLoader after loading an icon.
- (void)_setIcon:(NSImage *)icon forIconURL:(NSURL *)iconURL;

// Called by WebDataSource to bind a web site URL to a icon URL and icon image.
- (void)_setIconURL:(NSURL *)iconURL forSiteURL:(NSURL *)siteURL;

- (void)_setBuiltInIcon:(NSImage *)icon forHost:(NSString *)host;

@end