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
#import <WebKit/WebKitNSStringExtras.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKit/WebPreferences.h>

#import <WebKit/WebIconDatabaseBridge.h>

#import "WebTypesInternal.h"

NSString * const WebIconDatabaseVersionKey =    @"WebIconDatabaseVersion";
NSString * const WebURLToIconURLKey =           @"WebSiteURLToIconURLKey";

NSString * const ObsoleteIconsOnDiskKey =       @"WebIconsOnDisk";
NSString * const ObsoleteIconURLToURLsKey =     @"WebIconURLToSiteURLs";

static const int WebIconDatabaseCurrentVersion = 2;

NSString *WebIconDatabaseDidAddIconNotification =          @"WebIconDatabaseDidAddIconNotification";
NSString *WebIconNotificationUserInfoURLKey =              @"WebIconNotificationUserInfoURLKey";
NSString *WebIconDatabaseDidRemoveAllIconsNotification =   @"WebIconDatabaseDidRemoveAllIconsNotification";

NSString *WebIconDatabaseDirectoryDefaultsKey = @"WebIconDatabaseDirectoryDefaultsKey";
NSString *WebIconDatabaseEnabledDefaultsKey =   @"WebIconDatabaseEnabled";

NSString *WebIconDatabasePath = @"~/Library/Icons";

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
- (NSImage *)_iconForFileURL:(NSString *)fileURL withSize:(NSSize)size;
- (void)_resetCachedWebPreferences:(NSNotification *)notification;
- (NSImage *)_largestIconFromDictionary:(NSMutableDictionary *)icons;
- (NSMutableDictionary *)_iconsBySplittingRepresentationsOfIcon:(NSImage *)icon;
- (NSImage *)_iconFromDictionary:(NSMutableDictionary *)icons forSize:(NSSize)size cache:(BOOL)cache;
- (void)_scaleIcon:(NSImage *)icon toSize:(NSSize)size;
- (NSData *)_iconDataForIconURL:(NSString *)iconURLString;
- (void)_convertToWebCoreFormat; 
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

#ifdef ICONDEBUG
    _private->databaseBridge = [WebIconDatabaseBridge sharedBridgeInstance];
    if (_private->databaseBridge) {
        NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
        NSString *databaseDirectory = [defaults objectForKey:WebIconDatabaseDirectoryDefaultsKey];

        if (!databaseDirectory) {
            databaseDirectory = WebIconDatabasePath;
            [defaults setObject:databaseDirectory forKey:WebIconDatabaseDirectoryDefaultsKey];
        }
        databaseDirectory = [databaseDirectory stringByExpandingTildeInPath];
        [_private->databaseBridge openSharedDatabaseWithPath:databaseDirectory];
    }
    
    [self _convertToWebCoreFormat];
#endif

    _private->iconURLToIcons = [[NSMutableDictionary alloc] init];
    _private->iconURLToExtraRetainCount = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
    _private->pageURLToRetainCount = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
    _private->iconsToEraseWithURLs = [[NSMutableSet alloc] init];
    _private->iconsToSaveWithURLs = [[NSMutableSet alloc] init];
    _private->iconURLsWithNoIcons = [[NSMutableSet alloc] init];
    _private->iconURLsBoundDuringPrivateBrowsing = [[NSMutableSet alloc] init];
    _private->pageURLsBoundDuringPrivateBrowsing = [[NSMutableSet alloc] init];
    _private->privateBrowsingEnabled = [[WebPreferences standardPreferences] privateBrowsingEnabled];
    
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(_applicationWillTerminate:)
                                                 name:NSApplicationWillTerminateNotification
                                               object:NSApp];
    [[NSNotificationCenter defaultCenter] 
            addObserver:self selector:@selector(_resetCachedWebPreferences:) 
                   name:WebPreferencesChangedNotification object:nil];
                   
    // FIXME - Once the new iconDB is the only game in town, we need to remove any of the WebFileDatabase code
    // that is threaded and expects certain files to exist - certain files we rip right out from underneath it
    // in the _convertToWebCoreFormat method

    return self;
}

- (NSImage *)iconForURL:(NSString *)URL withSize:(NSSize)size cache:(BOOL)cache
{
    ASSERT(size.width);
    ASSERT(size.height);

    if (!URL || ![self _isEnabled])
        return [self defaultIconWithSize:size];

    // <rdar://problem/4697934> - Move the handling of FileURLs to WebCore and implement in ObjC++
    if ([URL _webkit_isFileURL])
        return [self _iconForFileURL:URL withSize:size];

#ifdef ICONDEBUG        
    NSImage* image = [_private->databaseBridge iconForPageURL:URL withSize:size];
    return image ? image : [self defaultIconWithSize:size];
#endif
}

- (NSImage *)iconForURL:(NSString *)URL withSize:(NSSize)size
{
    return [self iconForURL:URL withSize:size cache:YES];
}

- (NSString *)iconURLForURL:(NSString *)URL
{
    if (![self _isEnabled])
        return nil;
        
#ifdef ICONDEBUG
    NSString* iconurl = [_private->databaseBridge iconURLForPageURL:URL];
    return iconurl;
#endif
}

- (NSImage *)defaultIconWithSize:(NSSize)size
{
    ASSERT(size.width);
    ASSERT(size.height);
    
#ifdef ICONDEBUG
    return [_private->databaseBridge defaultIconWithSize:size];
#endif
}

- (void)retainIconForURL:(NSString *)URL
{
    ASSERT(URL);
    if (![self _isEnabled])
        return;

#ifdef ICONDEBUG
    [_private->databaseBridge retainIconForURL:URL];
    return;
#endif
}

- (void)releaseIconForURL:(NSString *)pageURL
{
    ASSERT(pageURL);
    if (![self _isEnabled])
        return;

#ifdef ICONDEBUG
    [_private->databaseBridge releaseIconForURL:pageURL];
    return;
#endif
}
@end


@implementation WebIconDatabase (WebPendingPublic)

- (void)removeAllIcons
{
    // FIXME - <rdar://problem/4678414>
    // Need to create a bridge method that calls through to WebCore and performs a wipe of the DB there
}

- (BOOL)isIconExpiredForIconURL:(NSString *)iconURL
{
    return [_private->databaseBridge isIconExpiredForIconURL:iconURL];
}

@end

@implementation WebIconDatabase (WebPrivate)

- (BOOL)_isEnabled
{
    return (_private->fileDatabase != nil);
}

- (void)_setIconData:(NSData *)data forIconURL:(NSString *)iconURL
{
    ASSERT(data);
    ASSERT(iconURL);
    ASSERT([self _isEnabled]);   
    
    [_private->databaseBridge _setIconData:data forIconURL:iconURL];
}

- (void)_setHaveNoIconForIconURL:(NSString *)iconURL
{
    ASSERT(iconURL);
    ASSERT([self _isEnabled]);

#ifdef ICONDEBUG
    [_private->databaseBridge _setHaveNoIconForIconURL:iconURL];
    return;
#endif
}


- (void)_setIconURL:(NSString *)iconURL forURL:(NSString *)URL
{
    ASSERT(iconURL);
    ASSERT(URL);
    ASSERT([self _isEnabled]);
    
#ifdef ICONDEBUG
    // FIXME - The following comment is a holdover from the old icon DB, which handled missing icons
    // differently than the new db.  It would return early if the icon is in the negative cache,
    // avoiding the notification.  We should explore and see if a similar optimization can take place-
        // If the icon is in the negative cache (ie, there is no icon), avoid the
        // work of delivering a notification for it or saving it to disk. This is a significant
        // win on the iBench HTML test.
        
    // FIXME - The following comment is also a holdover - if the iconURL->pageURL mapping was already the
    // same, the notification would again be avoided - we should try to do this, too.
        // Don't do any work if the icon URL is already bound to the site URL
    
    // A possible solution for both of these is to have the bridge method return a BOOL saying "Yes, notify" or
    // "no, don't bother notifying"
    
    [_private->databaseBridge _setIconURL:iconURL forPageURL:URL];
    [self _sendNotificationForURL:URL];
    return;
#endif
}

- (BOOL)_hasEntryForIconURL:(NSString *)iconURL;
{
    ASSERT([self _isEnabled]);

#ifdef ICONDEBUG
    return [_private->databaseBridge _hasEntryForIconURL:iconURL];
#endif
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

- (void)loadIconFromURL:(NSString *)iconURL
{
    [_private->databaseBridge loadIconFromURL:iconURL];
}

@end

@implementation WebIconDatabase (WebInternal)

- (void)_createFileDatabase
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSString *databaseDirectory = [defaults objectForKey:WebIconDatabaseDirectoryDefaultsKey];

    if (!databaseDirectory) {
        databaseDirectory = WebIconDatabasePath;
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
    [_private->iconURLsBoundDuringPrivateBrowsing release];
    [_private->pageURLsBoundDuringPrivateBrowsing release];
    _private->pageURLToIconURL = [[NSMutableDictionary alloc] init];
    _private->iconURLToPageURLs = [[NSMutableDictionary alloc] init];
    _private->iconsOnDiskWithURLs = [[NSMutableSet alloc] init];
    _private->originalIconsOnDiskWithURLs = [[NSMutableSet alloc] init];
    _private->iconURLsBoundDuringPrivateBrowsing = [[NSMutableSet alloc] init];
    _private->pageURLsBoundDuringPrivateBrowsing = [[NSMutableSet alloc] init];
}

- (void)_loadIconDictionaries
{
    WebFileDatabase *fileDB = _private->fileDatabase;
    
    // fileDB should be non-nil here because it should have been created by _createFileDatabase 
    if (!fileDB) {
        LOG_ERROR("Couldn't load icon dictionaries because file database didn't exist");
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
        pageURLToIconURL = [fileDB objectForKey:WebURLToIconURLKey];
        // Remove the old unnecessary mapping files.
        if (v < WebIconDatabaseCurrentVersion) {
            [fileDB removeObjectForKey:ObsoleteIconsOnDiskKey];
            [fileDB removeObjectForKey:ObsoleteIconURLToURLsKey];
        }        
    }
    
    // Must double-check all values read from disk. If any are bogus, we just throw out the whole icon cache.
    // We expect this to be nil if the icon cache has been cleared, so we shouldn't whine in that case.
    if (![pageURLToIconURL isKindOfClass:[NSMutableDictionary class]]) {
        if (pageURLToIconURL)
            LOG_ERROR("Clearing icon cache because bad value %@ was found on disk, expected an NSMutableDictionary", pageURLToIconURL);
        [self _clearDictionaries];
        return;
    }

    // Keep a set of icon URLs on disk so we know what we need to write out or remove.
    NSMutableSet *iconsOnDiskWithURLs = [NSMutableSet setWithArray:[pageURLToIconURL allValues]];

    // Reverse pageURLToIconURL so we have an icon URL to page URLs dictionary. 
    NSMutableDictionary *iconURLToPageURLs = [NSMutableDictionary dictionaryWithCapacity:[_private->iconsOnDiskWithURLs count]];
    NSEnumerator *enumerator = [pageURLToIconURL keyEnumerator];
    NSString *URL;
    while ((URL = [enumerator nextObject])) {
        NSString *iconURL = (NSString *)[pageURLToIconURL objectForKey:URL];
        // Must double-check all values read from disk. If any are bogus, we just throw out the whole icon cache.
        if (![URL isKindOfClass:[NSString class]] || ![iconURL isKindOfClass:[NSString class]]) {
            LOG_ERROR("Clearing icon cache because either %@ or %@ was a bad value on disk, expected both to be NSStrings", URL, iconURL);
            [self _clearDictionaries];
            return;
        }
        [iconURLToPageURLs _web_setObjectUsingSetIfNecessary:URL forKey:iconURL];
    }

    ASSERT(!_private->pageURLToIconURL);
    ASSERT(!_private->iconURLToPageURLs);
    ASSERT(!_private->iconsOnDiskWithURLs);
    ASSERT(!_private->originalIconsOnDiskWithURLs);
    
    _private->pageURLToIconURL = [pageURLToIconURL retain];
    _private->iconURLToPageURLs = [iconURLToPageURLs retain];
    _private->iconsOnDiskWithURLs = [iconsOnDiskWithURLs retain];
    _private->originalIconsOnDiskWithURLs = [iconsOnDiskWithURLs copy];
}



- (void)_applicationWillTerminate:(NSNotification *)notification
{
#ifdef ICONDEBUG
    [_private->databaseBridge closeSharedDatabase];
    [_private->databaseBridge release];
    _private->databaseBridge = nil;
#endif
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
            icon = [workspace iconForFileType:NSFileTypeForHFSTypeCode(kGenericDocumentIcon)];
        } else {
            icon = [workspace iconForFile:path];
        }
        [self _scaleIcon:icon toSize:size];
    }

    return icon;
}

- (void)_resetCachedWebPreferences:(NSNotification *)notification
{
    BOOL privateBrowsingEnabledNow = [[WebPreferences standardPreferences] privateBrowsingEnabled];

#ifdef ICONDEBUG
    [_private->databaseBridge setPrivateBrowsingEnabled:privateBrowsingEnabledNow];
    return;
#endif
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

    if([icons count] > 0)
        return icons;

    LOG_ERROR("icon has no representations");
    
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

- (NSData *)_iconDataForIconURL:(NSString *)iconURLString
{
    ASSERT(iconURLString);
    
    NSData *iconData = [_private->fileDatabase objectForKey:iconURLString];
    
    if ((id)iconData == (id)[NSNull null]) 
        return nil;
        
    return iconData;
}

- (void)_convertToWebCoreFormat
{
    ASSERT(_private);
    ASSERT(_private->databaseBridge);
    
    // If the WebCore Icon Database is not empty, we assume that this conversion has already
    // taken place and skip the rest of the steps 
    if (![_private->databaseBridge _isEmpty])
        return;
                
    NSEnumerator *enumerator = [_private->pageURLToIconURL keyEnumerator];
    NSString *url, *iconURL;
    
    // First, we'll iterate through the PageURL->IconURL map
    while ((url = [enumerator nextObject]) != nil) {
        iconURL = [_private->pageURLToIconURL objectForKey:url];
        if (!iconURL)
            continue;
        [_private->databaseBridge _setIconURL:iconURL forPageURL:url];
    }    
    
    // Second, we'll iterate through the icon data we do have
    enumerator = [_private->iconsOnDiskWithURLs objectEnumerator];
    NSData *iconData;
    
    while ((url = [enumerator nextObject]) != nil) {
        iconData = [self _iconDataForIconURL:url];
        if (iconData)
            [_private->databaseBridge _setIconData:iconData forIconURL:url];
        else {
            // This really *shouldn't* happen, so it'd be good to track down why it might happen in a debug build
            // however, we do know how to handle it gracefully in release
            LOG_ERROR("%@ is marked as having an icon on disk, but we couldn't get the data for it", url);
            [_private->databaseBridge _setHaveNoIconForIconURL:url];
        }
    }
    
    // Finally, we'll iterate through the negative cache we have
    enumerator = [_private->iconURLsWithNoIcons objectEnumerator];
    while ((url = [enumerator nextObject]) != nil) 
        [_private->databaseBridge _setHaveNoIconForIconURL:url];
   
    // After we're done converting old style icons over to webcore icons, we delete the entire directory hierarchy 
    // for the old icon DB
    NSString *iconPath = [[NSUserDefaults standardUserDefaults] objectForKey:WebIconDatabaseDirectoryDefaultsKey];
    if (!iconPath)
        iconPath = WebIconDatabasePath;
    
    NSString *fullIconPath = [iconPath stringByExpandingTildeInPath];    
    NSFileManager *fileManager = [NSFileManager defaultManager];
    enumerator = [[fileManager directoryContentsAtPath:fullIconPath] objectEnumerator];
    
    NSString *databaseFilename = [_private->databaseBridge defaultDatabaseFilename];

    NSString *file;
    while ((file = [enumerator nextObject]) != nil) {
        if ([file isEqualTo:databaseFilename])
            continue;
        NSString *filePath = [fullIconPath stringByAppendingPathComponent:file];
        if (![fileManager  removeFileAtPath:filePath handler:nil])
            LOG_ERROR("Failed to delete %@ from old icon directory", filePath);
    }
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
