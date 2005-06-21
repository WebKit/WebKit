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

#import <WebKit/WebIconDatabase.h>

#import <WebKit/WebIconDatabasePrivate.h>
#import <WebKit/WebFileDatabase.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKit/WebKitNSStringExtras.h>

NSString * const WebIconDatabaseVersionKey = 	@"WebIconDatabaseVersion";
NSString * const WebURLToIconURLKey = 		@"WebSiteURLToIconURLKey";

NSString * const ObsoleteIconsOnDiskKey =       @"WebIconsOnDisk";
NSString * const ObsoleteIconURLToURLsKey =     @"WebIconURLToSiteURLs";

static const int WebIconDatabaseCurrentVersion = 2;

NSString *WebIconDatabaseDidAddIconNotification =          @"WebIconDatabaseDidAddIconNotification";
NSString *WebIconNotificationUserInfoURLKey =              @"WebIconNotificationUserInfoURLKey";
NSString *WebIconDatabaseDidRemoveAllIconsNotification =   @"WebIconDatabaseDidRemoveAllIconsNotification";

NSString *WebIconDatabaseDirectoryDefaultsKey = @"WebIconDatabaseDirectoryDefaultsKey";
NSString *WebIconDatabaseEnabledDefaultsKey =   @"WebIconDatabaseEnabled";

NSSize WebIconSmallSize = {16, 16};
NSSize WebIconMediumSize = {32, 32};
NSSize WebIconLargeSize = {128, 128};

@interface NSMutableDictionary (WebIconDatabase)
- (void)_web_setObjectUsingSetIfNecessary:(id)object forKey:(id)key;
@end

@interface WebIconDatabase (WebInternal)
- (void)_clearDictionaries;
- (void)_createFileDatabase;
- (void)_loadIconDictionaries;
- (void)_updateFileDatabase;
- (void)_forgetIconForIconURLString:(NSString *)iconURLString;
- (NSMutableDictionary *)_iconsForIconURLString:(NSString *)iconURL;
- (NSImage *)_iconForFileURL:(NSString *)fileURL withSize:(NSSize)size;
- (void)_retainIconForIconURLString:(NSString *)iconURL;
- (void)_releaseIconForIconURLString:(NSString *)iconURL;
- (void)_retainOriginalIconsOnDisk;
- (void)_releaseOriginalIconsOnDisk;
- (void)_sendNotificationForURL:(NSString *)URL;
- (int)_totalRetainCountForIconURLString:(NSString *)iconURLString;
- (NSImage *)_largestIconFromDictionary:(NSMutableDictionary *)icons;
- (NSMutableDictionary *)_iconsBySplittingRepresentationsOfIcon:(NSImage *)icon;
- (NSImage *)_iconFromDictionary:(NSMutableDictionary *)icons forSize:(NSSize)size cache:(BOOL)cache;
- (void)_scaleIcon:(NSImage *)icon toSize:(NSSize)size;
@end


@implementation WebIconDatabase

+ (WebIconDatabase *)sharedIconDatabase
{
    static WebIconDatabase *database = nil;
    
    if (!database) {
#if !LOG_DISABLED
        double start = CFAbsoluteTimeGetCurrent();
#endif
        database = [[WebIconDatabase alloc] init];
#if !LOG_DISABLED
        LOG(Timing, "initializing icon database with %d sites and %d icons took %f", 
            [database->_private->pageURLToIconURL count], [database->_private->iconURLToPageURLs count], (CFAbsoluteTimeGetCurrent() - start));
#endif
    }
    return database;
}

- init
{
    [super init];
    
    _private = [[WebIconDatabasePrivate alloc] init];

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSDictionary *initialDefaults = [[NSDictionary alloc] initWithObjectsAndKeys:[NSNumber numberWithBool:YES], WebIconDatabaseEnabledDefaultsKey, nil];
    [defaults registerDefaults:initialDefaults];
    [initialDefaults release];
    if (![defaults boolForKey:WebIconDatabaseEnabledDefaultsKey]) {
        return self;
    }
    
    [self _createFileDatabase];
    [self _loadIconDictionaries];

    _private->iconURLToIcons = [[NSMutableDictionary alloc] init];
    _private->iconURLToExtraRetainCount = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
    _private->pageURLToRetainCount = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
    _private->iconsToEraseWithURLs = [[NSMutableSet alloc] init];
    _private->iconsToSaveWithURLs = [[NSMutableSet alloc] init];
    _private->iconURLsWithNoIcons = [[NSMutableSet alloc] init];
    
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(_applicationWillTerminate:)
                                                 name:NSApplicationWillTerminateNotification
                                               object:NSApp];

    // Retain icons on disk then release them once clean-up has begun.
    // This gives the client the opportunity to retain them before they are erased.
    [self _retainOriginalIconsOnDisk];
    [self performSelector:@selector(_releaseOriginalIconsOnDisk) withObject:nil afterDelay:0];
    
    return self;
}

- (NSImage *)iconForURL:(NSString *)URL withSize:(NSSize)size cache:(BOOL)cache
{
    ASSERT(size.width);
    ASSERT(size.height);
    
    if (!URL || ![self _isEnabled]) {
        return [self defaultIconWithSize:size];
    }

    if ([URL _webkit_isFileURL]) {
        return [self _iconForFileURL:URL withSize:size];
    }
    
    NSString *iconURLString = [_private->pageURLToIconURL objectForKey:URL];
    if (!iconURLString) {
        // Don't have it
        return [self defaultIconWithSize:size];
    }

    NSMutableDictionary *icons = [self _iconsForIconURLString:iconURLString];
    if (!icons) {
	if (![_private->iconURLsWithNoIcons containsObject:iconURLString]) {
	    ERROR("WebIconDatabase said it had %@, but it doesn't.", iconURLString);
	}
        return [self defaultIconWithSize:size];
    }        

    return [self _iconFromDictionary:icons forSize:size cache:cache];
}

- (NSImage *)iconForURL:(NSString *)URL withSize:(NSSize)size
{
    return [self iconForURL:URL withSize:size cache:YES];
}

- (NSString *)iconURLForURL:(NSString *)URL
{
    if (![self _isEnabled]) {
        return nil;
    }
    return URL ? [_private->pageURLToIconURL objectForKey:URL] : nil;
}

- (NSImage *)defaultIconWithSize:(NSSize)size
{
    ASSERT(size.width);
    ASSERT(size.height);
    
    if (!_private->defaultIcons) {
        NSString *path = [[NSBundle bundleForClass:[self class]] pathForResource:@"url_icon" ofType:@"tiff"];
        if (path) {
            NSImage *icon = [[NSImage alloc] initByReferencingFile:path];
            _private->defaultIcons = [[NSMutableDictionary dictionaryWithObject:icon
                                            forKey:[NSValue valueWithSize:[icon size]]] retain];
            [icon release];
        }
    }

    return [self _iconFromDictionary:_private->defaultIcons forSize:size cache:YES];
}

- (void)retainIconForURL:(NSString *)URL
{
    ASSERT(URL);
    
    if (![self _isEnabled]) {
        return;
    }
    
    int retainCount = (int)(void *)CFDictionaryGetValue(_private->pageURLToRetainCount, URL);
    CFDictionarySetValue(_private->pageURLToRetainCount, URL, (void *)(retainCount + 1));
}

- (void)releaseIconForURL:(NSString *)pageURL
{
    ASSERT(pageURL);
    
    if (![self _isEnabled]) {
        return;
    }    
    
    int retainCount = (int)(void *)CFDictionaryGetValue(_private->pageURLToRetainCount, pageURL);
    
    if (retainCount <= 0) {
        ERROR("The icon for %@ was released more times than it was retained.", pageURL);
        return;
    }
    
    int newRetainCount = retainCount - 1;

    if (newRetainCount == 0) {
        // Forget association between this page URL and a retain count
        CFDictionaryRemoveValue(_private->pageURLToRetainCount, pageURL);

        // If there's a known iconURL for this page URL, we need to do more cleanup
        NSString *iconURL = [_private->pageURLToIconURL objectForKey:pageURL];
        if (iconURL != nil) {
            // If there are no other retainers of this icon, forget it entirely
            if ([self _totalRetainCountForIconURLString:iconURL] == 0) {
                [self _forgetIconForIconURLString:iconURL];
            } else {
                // There's at least one other retainer of this icon, so we need to forget the
                // two-way links between this page URL and the icon URL, without blowing away
                // the icon entirely.                
                [_private->pageURLToIconURL removeObjectForKey:pageURL];
            
                id pageURLs = [_private->iconURLToPageURLs objectForKey:iconURL];
                if ([pageURLs isKindOfClass:[NSMutableSet class]]) {
                    ASSERT([pageURLs containsObject:pageURL]);
                    [pageURLs removeObject:pageURL];
                    
                    // Maybe this was the last page URL mapped to this icon URL
                    if ([pageURLs count] == 0) {
                        [_private->iconURLToPageURLs removeObjectForKey:iconURL];
                    }
                } else {
                    // Only one page URL was associated with this icon URL; it must have been us
                    ASSERT([pageURLs isKindOfClass:[NSString class]]);
                    ASSERT([pageURLs isEqualToString:pageURL]);
                    [_private->iconURLToPageURLs removeObjectForKey:pageURL];
                }
            }
        }
    } else {
        CFDictionarySetValue(_private->pageURLToRetainCount, pageURL, (void *)newRetainCount);
    }
}

- (void)delayDatabaseCleanup
{
    if (![self _isEnabled]) {
        return;
    }
    
    if(_private->didCleanup){
        ERROR("delayDatabaseCleanup cannot be called after cleanup has begun");
        return;
    }
    
    _private->cleanupCount++;
}

- (void)allowDatabaseCleanup
{
    if (![self _isEnabled]) {
        return;
    }
    
    if(_private->didCleanup){
        ERROR("allowDatabaseCleanup cannot be called after cleanup has begun");
        return;
    }
    
    _private->cleanupCount--;

    if(_private->cleanupCount == 0 && _private->waitingToCleanup){
        [self _releaseOriginalIconsOnDisk];
    }
}

@end


@implementation WebIconDatabase (WebPendingPublic)

- (void)removeAllIcons
{
    NSEnumerator *keyEnumerator = [(NSDictionary *)_private->iconURLToPageURLs keyEnumerator];
    NSString *iconURLString;
    while ((iconURLString = [keyEnumerator nextObject]) != nil) {
        // Note that _forgetIconForIconURLString does not affect retain counts, so the current clients
        // need not do anything about retaining/releasing icons here. (However, the current clients should
        // respond to WebIconDatabaseDidRemoveAllIconsNotification by refetching any icons that are 
        // displayed in the UI.) 
        [self _forgetIconForIconURLString:iconURLString];
    }
    
    // Delete entire file database immediately. This has at least three advantages over waiting for
    // _updateFileDatabase to execute:
    // (1) _updateFileDatabase won't execute until an icon has been added
    // (2) this is faster
    // (3) this deletes all the on-disk hierarchy (especially useful if due to past problems there are
    // some stale files in that hierarchy)
    [_private->fileDatabase removeAllObjects];
    [_private->iconsToEraseWithURLs removeAllObjects];
    [_private->iconsToSaveWithURLs removeAllObjects];
    [self _clearDictionaries];
    [[NSNotificationCenter defaultCenter] postNotificationName:WebIconDatabaseDidRemoveAllIconsNotification
                                                        object:self
                                                      userInfo:nil];
}
@end

@implementation WebIconDatabase (WebPrivate)

- (BOOL)_isEnabled
{
    return (_private->fileDatabase != nil);
}

- (void)_setIcon:(NSImage *)icon forIconURL:(NSString *)iconURL
{
    ASSERT(icon);
    ASSERT(iconURL);
    ASSERT([self _isEnabled]);
    
    NSMutableDictionary *icons = [self _iconsBySplittingRepresentationsOfIcon:icon];
    
    if (!icons) {
        return;
    }
    
    [_private->iconURLToIcons setObject:icons forKey:iconURL];
    
    [self _retainIconForIconURLString:iconURL];
    
    // Release the newly created icon much like an autorelease.
    // This gives the client enough time to retain it.
    // FIXME: Should use an actual autorelease here using a proxy object instead.
    [self performSelector:@selector(_releaseIconForIconURLString:) withObject:iconURL afterDelay:0];
}

- (void)_setHaveNoIconForIconURL:(NSString *)iconURL
{
    ASSERT(iconURL);
    ASSERT([self _isEnabled]);

    [_private->iconURLsWithNoIcons addObject:iconURL];
    
    [self _retainIconForIconURLString:iconURL];
    
    // Release the newly created icon much like an autorelease.
    // This gives the client enough time to retain it.
    // FIXME: Should use an actual autorelease here using a proxy object instead.
    [self performSelector:@selector(_releaseIconForIconURLString:) withObject:iconURL afterDelay:0];
}


- (void)_setIconURL:(NSString *)iconURL forURL:(NSString *)URL
{
    ASSERT(iconURL);
    ASSERT(URL);
    ASSERT([self _isEnabled]);
    ASSERT([self _hasIconForIconURL:iconURL]);
 
    if ([[_private->pageURLToIconURL objectForKey:URL] isEqualToString:iconURL] &&
        [_private->iconsOnDiskWithURLs containsObject:iconURL]) {
        // Don't do any work if the icon URL is already bound to the site URL
        return;
    }
    
    [_private->pageURLToIconURL setObject:iconURL forKey:URL];
    [_private->iconURLToPageURLs _web_setObjectUsingSetIfNecessary:URL forKey:iconURL];
        
    [self _sendNotificationForURL:URL];
    [self _updateFileDatabase];
}

- (BOOL)_hasIconForIconURL:(NSString *)iconURL;
{
    ASSERT([self _isEnabled]);
    
    return (([_private->iconURLToIcons objectForKey:iconURL] ||
	     [_private->iconURLsWithNoIcons containsObject:iconURL] ||
             [_private->iconsOnDiskWithURLs containsObject:iconURL]) &&
             [self _totalRetainCountForIconURLString:iconURL] > 0);
}

@end

@implementation WebIconDatabase (WebInternal)

- (void)_createFileDatabase
{
    // FIXME: Make defaults key public somehow
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSString *databaseDirectory = [defaults objectForKey:WebIconDatabaseDirectoryDefaultsKey];

    if (!databaseDirectory) {
        databaseDirectory = @"~/Library/Icons";
        [defaults setObject:databaseDirectory forKey:WebIconDatabaseDirectoryDefaultsKey];
    }
    databaseDirectory = [databaseDirectory stringByExpandingTildeInPath];
    
    _private->fileDatabase = [[WebFileDatabase alloc] initWithPath:databaseDirectory];
    [_private->fileDatabase setSizeLimit:20000000];
    [_private->fileDatabase open];
}

- (void)_clearDictionaries
{
    [_private->pageURLToIconURL release];
    [_private->iconURLToPageURLs release];
    [_private->iconsOnDiskWithURLs release];
    [_private->originalIconsOnDiskWithURLs release];
    _private->pageURLToIconURL = [[NSMutableDictionary alloc] init];
    _private->iconURLToPageURLs = [[NSMutableDictionary alloc] init];
    _private->iconsOnDiskWithURLs = [[NSMutableSet alloc] init];
    _private->originalIconsOnDiskWithURLs = [[NSMutableSet alloc] init];
}

- (void)_loadIconDictionaries
{
    WebFileDatabase *fileDB = _private->fileDatabase;
    if (!fileDB) {
        return;
    }
    
    NSNumber *version = [fileDB objectForKey:WebIconDatabaseVersionKey];
    int v = 0;
    // no version means first version
    if (version == nil) {
	v = 1;
    } else if ([version isKindOfClass:[NSNumber class]]) {
	v = [version intValue];
    }
    
    // Get the site URL to icon URL dictionary from the file DB.
    NSMutableDictionary *pageURLToIconURL = nil;
    if (v <= WebIconDatabaseCurrentVersion) {
        pageURLToIconURL = [[fileDB objectForKey:WebURLToIconURLKey] retain];
        // Remove the old unnecessary mapping files.
        if (v < WebIconDatabaseCurrentVersion) {
            [fileDB removeObjectForKey:ObsoleteIconsOnDiskKey];
            [fileDB removeObjectForKey:ObsoleteIconURLToURLsKey];
        }        
    }
    
    if (![pageURLToIconURL isKindOfClass:[NSMutableDictionary class]]) {
        [self _clearDictionaries];
        return;
    }

    // Keep a set of icon URLs on disk so we know what we need to write out or remove.
    NSMutableSet *iconsOnDiskWithURLs = [[NSMutableSet alloc] initWithArray:[pageURLToIconURL allValues]];

    // Reverse pageURLToIconURL so we have an icon URL to page URLs dictionary. 
    NSMutableDictionary *iconURLToPageURLs = [[NSMutableDictionary alloc] initWithCapacity:[_private->iconsOnDiskWithURLs count]];
    NSEnumerator *enumerator = [pageURLToIconURL keyEnumerator];
    NSString *URL;
    while ((URL = [enumerator nextObject])) {
        NSString *iconURL = (NSString *)[pageURLToIconURL objectForKey:URL];
        if (![URL isKindOfClass:[NSString class]] || ![iconURL isKindOfClass:[NSString class]]) {
            [self _clearDictionaries];
            return;
        }
        [iconURLToPageURLs _web_setObjectUsingSetIfNecessary:URL forKey:iconURL];
    }

    _private->pageURLToIconURL = pageURLToIconURL;
    _private->iconURLToPageURLs = iconURLToPageURLs;
    _private->iconsOnDiskWithURLs = iconsOnDiskWithURLs;
    _private->originalIconsOnDiskWithURLs = [iconsOnDiskWithURLs copy];
}

// Only called by _setIconURL:forKey:
- (void)_updateFileDatabase
{
    if(_private->cleanupCount != 0){
        return;
    }

    WebFileDatabase *fileDB = _private->fileDatabase;
    if (!fileDB) {
        return;
    }

    [fileDB setObject:[NSNumber numberWithInt:WebIconDatabaseCurrentVersion] forKey:WebIconDatabaseVersionKey];

    // Erase icons that have been released that are on disk.
    // Must remove icons before writing them to disk or else we could potentially remove the newly written ones.
    NSEnumerator *enumerator = [_private->iconsToEraseWithURLs objectEnumerator];
    NSString *iconURLString;
    
    while ((iconURLString = [enumerator nextObject]) != nil) {
        [fileDB removeObjectForKey:iconURLString];
        [_private->iconsOnDiskWithURLs removeObject:iconURLString];
    }

    // Save icons that have been retained that are not already on disk
    enumerator = [_private->iconsToSaveWithURLs objectEnumerator];

    while ((iconURLString = [enumerator nextObject]) != nil) {
        NSMutableDictionary *icons = [_private->iconURLToIcons objectForKey:iconURLString];
        if (icons) {
            // Save the 16 x 16 size icons as this is the only size that Safari uses.
            // If we ever use larger sizes, we should save the largest size so icons look better when scaling up.
            // This also works around the problem with cnet's blank 32x32 icon (3105486).
            NSImage *icon = [icons objectForKey:[NSValue valueWithSize:NSMakeSize(16,16)]];
            if (!icon) {
                // In case there is no 16 x 16 size.
                icon = [self _largestIconFromDictionary:icons];
            }
            NSData *iconData = [icon TIFFRepresentation];
            if (iconData) {
                //NSLog(@"Writing icon: %@", iconURLString);
                [fileDB setObject:iconData forKey:iconURLString];
                [_private->iconsOnDiskWithURLs addObject:iconURLString];
            }
        } else if ([_private->iconURLsWithNoIcons containsObject:iconURLString]) {
            [fileDB setObject:[NSNull null] forKey:iconURLString];
            [_private->iconsOnDiskWithURLs addObject:iconURLString];
        }
    }
    
    [_private->iconsToEraseWithURLs removeAllObjects];
    [_private->iconsToSaveWithURLs removeAllObjects];

    // Save the icon dictionaries to disk. Save them as mutable copies otherwise WebFileDatabase may access the 
    // same dictionaries on a separate thread as it's being modified. We think this fixes 3566336.
    NSMutableDictionary *pageURLToIconURLCopy = [_private->pageURLToIconURL mutableCopy];
    [fileDB setObject:pageURLToIconURLCopy forKey:WebURLToIconURLKey];
    [pageURLToIconURLCopy release];
}

- (void)_applicationWillTerminate:(NSNotification *)notification
{
    // Should only cause a write if user quit before 3 seconds after the last _updateFileDatabase
    [_private->fileDatabase sync];
}

- (int)_totalRetainCountForIconURLString:(NSString *)iconURLString
{
    // Add up the retain counts for each associated page, plus the retain counts not associated
    // with any page, which are stored in _private->iconURLToExtraRetainCount
    int result = (int)(void *)CFDictionaryGetValue(_private->iconURLToExtraRetainCount, iconURLString);
    
    id URLStrings = [_private->iconURLToPageURLs objectForKey:iconURLString];
    if (URLStrings != nil) {
        if ([URLStrings isKindOfClass:[NSMutableSet class]]) {
            NSEnumerator *e = [(NSMutableSet *)URLStrings objectEnumerator];
            NSString *URLString;
            while ((URLString = [e nextObject]) != nil) {
                ASSERT([URLString isKindOfClass:[NSString class]]);
                result += (int)(void *)CFDictionaryGetValue(_private->pageURLToRetainCount, URLString);
            }
        } else {
            ASSERT([URLStrings isKindOfClass:[NSString class]]);
            result += (int)(void *)CFDictionaryGetValue(_private->pageURLToRetainCount, URLStrings);
        }
    }

    return result;
}

- (NSMutableDictionary *)_iconsForIconURLString:(NSString *)iconURLString
{
    ASSERT(iconURLString);

    if ([_private->iconURLsWithNoIcons containsObject:iconURLString]) {
	return nil;
    }
    
    NSMutableDictionary *icons = [_private->iconURLToIcons objectForKey:iconURLString];

    if (icons) {
	return icons;
    }
	
    // Not in memory, check disk
    if(![_private->iconsOnDiskWithURLs containsObject:iconURLString]){
        return nil;
    }

    
#if !LOG_DISABLED         
    double start = CFAbsoluteTimeGetCurrent();
#endif
    NSData *iconData = [_private->fileDatabase objectForKey:iconURLString];

    if ([iconData isKindOfClass:[NSNull class]]) {
	[_private->iconURLsWithNoIcons addObject:iconURLString];
	return nil;
    }
    
    if (iconData) {
        NS_DURING
            NSImage *icon = [[NSImage alloc] initWithData:iconData];
            icons = [self _iconsBySplittingRepresentationsOfIcon:icon];
            if (icons) {
#if !LOG_DISABLED 
                double duration = CFAbsoluteTimeGetCurrent() - start;
                LOG(Timing, "loading and creating icon %@ took %f seconds", iconURLString, duration);
#endif
                [_private->iconURLToIcons setObject:icons forKey:iconURLString];
            }
        NS_HANDLER
            icons = nil;
        NS_ENDHANDLER
    }
    
    return icons;
}

- (NSImage *)_iconForFileURL:(NSString *)file withSize:(NSSize)size
{
    ASSERT(size.width);
    ASSERT(size.height);

    NSWorkspace *workspace = [NSWorkspace sharedWorkspace];
    NSString *path = [[NSURL _web_URLWithDataAsString:file] path];
    NSString *suffix = [path pathExtension];
    NSImage *icon = nil;
    
    if ([suffix _webkit_isCaseInsensitiveEqualToString:@"htm"] || [suffix _webkit_isCaseInsensitiveEqualToString:@"html"]) {
        if (!_private->htmlIcons) {
            icon = [workspace iconForFileType:@"html"];
            _private->htmlIcons = [[self _iconsBySplittingRepresentationsOfIcon:icon] retain];
        }
        icon = [self _iconFromDictionary:_private->htmlIcons forSize:size cache:YES];
    } else {
        if (!path || ![path isAbsolutePath]) {
            // Return the generic icon when there is no path.
            icon = [workspace iconForFileType:@"????"];
        } else {
            icon = [workspace iconForFile:path];
        }
        [self _scaleIcon:icon toSize:size];
    }

    return icon;
}

- (void)_retainIconForIconURLString:(NSString *)iconURLString
{
    ASSERT(iconURLString);
    
    int retainCount = (int)(void *)CFDictionaryGetValue(_private->iconURLToExtraRetainCount, iconURLString);
    int newRetainCount = retainCount + 1;

    CFDictionarySetValue(_private->iconURLToExtraRetainCount, iconURLString, (void *)newRetainCount);

    if (newRetainCount == 1 && ![_private->iconsOnDiskWithURLs containsObject:iconURLString]){
        [_private->iconsToSaveWithURLs addObject:iconURLString];
        [_private->iconsToEraseWithURLs removeObject:iconURLString];
    }
}

- (void)_forgetIconForIconURLString:(NSString *)iconURLString
{
    ASSERT_ARG(iconURLString, iconURLString != nil);
    if([_private->iconsOnDiskWithURLs containsObject:iconURLString]){
        [_private->iconsToEraseWithURLs addObject:iconURLString];
        [_private->iconsToSaveWithURLs removeObject:iconURLString];
    }
    
    // Remove the icon's images
    [_private->iconURLToIcons removeObjectForKey:iconURLString];
    
    // Remove negative cache item for icon, if any
    [_private->iconURLsWithNoIcons removeObject:iconURLString];
    
    // Remove the icon's associated site URLs, if any
    [iconURLString retain];
    id URLs = [_private->iconURLToPageURLs objectForKey:iconURLString];
    if (URLs != nil) {
        if ([URLs isKindOfClass:[NSMutableSet class]]) {
            [_private->pageURLToIconURL removeObjectsForKeys:[URLs allObjects]];
        } else {
            ASSERT([URLs isKindOfClass:[NSString class]]);
            [_private->pageURLToIconURL removeObjectForKey:URLs];
        }
    }
    [_private->iconURLToPageURLs removeObjectForKey:iconURLString];
    [iconURLString release];
}

- (void)_releaseIconForIconURLString:(NSString *)iconURLString
{
    ASSERT(iconURLString);
    
    int retainCount = (int)(void *)CFDictionaryGetValue(_private->iconURLToExtraRetainCount, iconURLString);

    if (retainCount <= 0) {
        ASSERT_NOT_REACHED();
        return;
    }
    
    int newRetainCount = retainCount - 1;
    if (newRetainCount == 0) {
        CFDictionaryRemoveValue(_private->iconURLToExtraRetainCount, iconURLString);
        if ([self _totalRetainCountForIconURLString:iconURLString] == 0) {
            [self _forgetIconForIconURLString:iconURLString];
        }
    } else {
        CFDictionarySetValue(_private->iconURLToExtraRetainCount, iconURLString, (void *)newRetainCount);
    }
}

- (void)_retainOriginalIconsOnDisk
{
    NSEnumerator *enumerator = [_private->originalIconsOnDiskWithURLs objectEnumerator];;
    NSString *iconURLString;
    while ((iconURLString = [enumerator nextObject]) != nil) {
        [self _retainIconForIconURLString:iconURLString];
    }
}

- (void)_releaseOriginalIconsOnDisk
{
    if (_private->cleanupCount > 0) {
        _private->waitingToCleanup = YES;
        return;
    }

    NSEnumerator *enumerator = [_private->originalIconsOnDiskWithURLs objectEnumerator];
    NSString *iconURLString;
    while ((iconURLString = [enumerator nextObject]) != nil) {
        [self _releaseIconForIconURLString:iconURLString];
    }
    
    [_private->originalIconsOnDiskWithURLs release];
    _private->originalIconsOnDiskWithURLs = nil;

    _private->didCleanup = YES;
}

- (void)_sendNotificationForURL:(NSString *)URL
{
    ASSERT(URL);
    
    NSDictionary *userInfo = [NSDictionary dictionaryWithObject:URL
                                                         forKey:WebIconNotificationUserInfoURLKey];
    [[NSNotificationCenter defaultCenter] postNotificationName:WebIconDatabaseDidAddIconNotification
                                                        object:self
                                                      userInfo:userInfo];
}

- (NSImage *)_largestIconFromDictionary:(NSMutableDictionary *)icons
{
    ASSERT(icons);
    
    NSEnumerator *enumerator = [icons keyEnumerator];
    NSValue *currentSize, *largestSize=nil;
    float largestSizeArea=0;

    while ((currentSize = [enumerator nextObject]) != nil) {
        NSSize currentSizeSize = [currentSize sizeValue];
        float currentSizeArea = currentSizeSize.width * currentSizeSize.height;
        if(!largestSizeArea || (currentSizeArea > largestSizeArea)){
            largestSize = currentSize;
            largestSizeArea = currentSizeArea;
        }
    }

    return [icons objectForKey:largestSize];
}

- (NSMutableDictionary *)_iconsBySplittingRepresentationsOfIcon:(NSImage *)icon
{
    ASSERT(icon);

    NSMutableDictionary *icons = [NSMutableDictionary dictionary];
    NSEnumerator *enumerator = [[icon representations] objectEnumerator];
    NSImageRep *rep;

    while ((rep = [enumerator nextObject]) != nil) {
        NSSize size = [rep size];
        NSImage *subIcon = [[NSImage alloc] initWithSize:size];
        [subIcon addRepresentation:rep];
        [icons setObject:subIcon forKey:[NSValue valueWithSize:size]];
        [subIcon release];
    }

    if([icons count] > 0){
        return icons;
    }

    ERROR("icon has no representations");
    
    return nil;
}

- (NSImage *)_iconFromDictionary:(NSMutableDictionary *)icons forSize:(NSSize)size cache:(BOOL)cache
{
    ASSERT(size.width);
    ASSERT(size.height);

    NSImage *icon = [icons objectForKey:[NSValue valueWithSize:size]];

    if(!icon){
        icon = [[[self _largestIconFromDictionary:icons] copy] autorelease];
        [self _scaleIcon:icon toSize:size];

        if(cache){
            [icons setObject:icon forKey:[NSValue valueWithSize:size]];
        }
    }

    return icon;
}

- (void)_scaleIcon:(NSImage *)icon toSize:(NSSize)size
{
    ASSERT(size.width);
    ASSERT(size.height);
    
#if !LOG_DISABLED        
    double start = CFAbsoluteTimeGetCurrent();
#endif
    
    [icon setScalesWhenResized:YES];
    [icon setSize:size];
    
#if !LOG_DISABLED
    double duration = CFAbsoluteTimeGetCurrent() - start;
    LOG(Timing, "scaling icon took %f seconds.", duration);
#endif
}

@end

@implementation WebIconDatabasePrivate

@end

@implementation NSMutableDictionary (WebIconDatabase)

- (void)_web_setObjectUsingSetIfNecessary:(id)object forKey:(id)key
{
    id previousObject = [self objectForKey:key];
    if (previousObject == nil) {
        [self setObject:object forKey:key];
    } else if ([previousObject isKindOfClass:[NSMutableSet class]]) {
        [previousObject addObject:object];
    } else {
        NSMutableSet *objects = [[NSMutableSet alloc] initWithObjects:previousObject, object, nil];
        [self setObject:objects forKey:key];
        [objects release];
    }
}

@end
