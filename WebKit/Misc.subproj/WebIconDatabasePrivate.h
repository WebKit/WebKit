/*
 *  WebIconDatabasePrivate.h
 *  
 *
 *  Created by Chris Blumenberg on Tue Aug 27 2002.
 *  Copyright (c) 2002 Apple Computer Inc. All rights reserved.
 *
 */

#import <Cocoa/Cocoa.h>
#import <WebKit/WebIconDatabase.h>

@class WebFileDatabase;

@interface WebIconDatabasePrivate : NSObject {

@public
    WebFileDatabase *fileDatabase;

    NSMutableDictionary *iconURLToIcons;
    CFMutableDictionaryRef iconURLToRetainCount;
    NSMutableDictionary *iconURLToURLs;
    NSMutableDictionary *URLToIconURL;    
    CFMutableDictionaryRef futureURLToRetainCount;
    
    NSMutableSet *iconsOnDiskWithURLs;
    NSMutableSet *iconsToEraseWithURLs;
    NSMutableSet *iconsToSaveWithURLs;
    NSMutableSet *iconURLsWithNoIcons;
    NSMutableSet *originalIconsOnDiskWithURLs;
    
    int cleanupCount;

    BOOL didCleanup;
    BOOL waitingToCleanup;

    NSMutableDictionary *htmlIcons;
    NSMutableDictionary *defaultIcons;
}

@end

@interface WebIconDatabase (WebPrivate)

- (BOOL)_isEnabled;

// Called by WebIconLoader after loading an icon.
- (void)_setIcon:(NSImage *)icon forIconURL:(NSString *)iconURL;
- (void)_setHaveNoIconForIconURL:(NSString *)iconURL;

// Called by WebDataSource to bind a web site URL to a icon URL and icon image.
- (void)_setIconURL:(NSString *)iconURL forURL:(NSString *)URL;

- (BOOL)_hasIconForIconURL:(NSString *)iconURL;

@end
