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

#import <Cocoa/Cocoa.h>
#import <WebKit/WebIconDatabase.h>

@class WebFileDatabase;

@interface WebIconDatabasePrivate : NSObject {

@public
    WebFileDatabase *fileDatabase;

    NSMutableDictionary *iconURLToIcons;
    NSMutableDictionary *iconURLToPageURLs;
    NSMutableDictionary *pageURLToIconURL;    
    CFMutableDictionaryRef pageURLToRetainCount;
    CFMutableDictionaryRef iconURLToExtraRetainCount;
    
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

// Sent when all icons are removed from the databse. The object of the notification is 
// the icon database. There is no userInfo. Clients should react by removing any cached
// icon images from the user interface. Clients need not and should not call 
// releaseIconForURL: in response to this notification.
extern NSString *WebIconDatabaseDidRemoveAllIconsNotification;

@interface WebIconDatabase (WebPendingPublic)
/*!
   @method removeAllIcons:
   @discussion Causes the icon database to delete all of the images that it has stored,
   and to send out the notification WebIconDatabaseDidRemoveAllIconsNotification.
*/
- (void)removeAllIcons;
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
