//
//  WebIconDatabase.m
//  WebKit
//
//  Created by Chris Blumenberg on Tue Aug 27 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

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

NSString *WebIconDatabaseDidAddIconNotification =   @"WebIconDatabaseDidAddIconNotification";
NSString *WebIconNotificationUserInfoURLKey =       @"WebIconNotificationUserInfoURLKey";

NSString *WebIconDatabaseDirectoryDefaultsKey = @"WebIconDatabaseDirectoryDefaultsKey";
NSString *WebIconDatabaseEnabledDefaultsKey =   @"WebIconDatabaseEnabled";

NSSize WebIconSmallSize = {16, 16};
NSSize WebIconMediumSize = {32, 32};
NSSize WebIconLargeSize = {128, 128};

@interface NSMutableDictionary (WebIconDatabase)
- (void)_web_setObjectUsingSetIfNecessary:(id)object forKey:(id)key;
@end

@interface WebIconDatabase (WebInternal)
- (void)_createFileDatabase;
- (void)_loadIconDictionaries;
- (void)_updateFileDatabase;
- (NSMutableDictionary *)_iconsForIconURLString:(NSString *)iconURL;
- (NSImage *)_iconForFileURL:(NSString *)fileURL withSize:(NSSize)size;
- (void)_retainIconForIconURLString:(NSString *)iconURL;
- (void)_releaseIconForIconURLString:(NSString *)iconURL;
- (void)_retainFutureIconForURL:(NSString *)URL;
- (void)_releaseFutureIconForURL:(NSString *)URL;
- (void)_retainOriginalIconsOnDisk;
- (void)_releaseOriginalIconsOnDisk;
- (void)_sendNotificationForURL:(NSString *)URL;
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
            [database->_private->URLToIconURL count], [database->_private->iconURLToURLs count], (CFAbsoluteTimeGetCurrent() - start));
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
    _private->iconURLToRetainCount = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
    _private->futureURLToRetainCount = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
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
    
    NSString *iconURLString = [_private->URLToIconURL objectForKey:URL];
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
    return URL ? [_private->URLToIconURL objectForKey:URL] : nil;
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
    
    NSString *iconURLString = [_private->URLToIconURL objectForKey:URL];
    
    if(iconURLString){
        [self _retainIconForIconURLString:iconURLString];
    }else{
        [self _retainFutureIconForURL:URL];
    }
}

- (void)releaseIconForURL:(NSString *)URL
{
    ASSERT(URL);
    
    if (![self _isEnabled]) {
        return;
    }    
    
    NSString *iconURLString = [_private->URLToIconURL objectForKey:URL];
    
    if(iconURLString){
        [self _releaseIconForIconURLString:iconURLString];
    }else{
        [self _releaseFutureIconForURL:URL];        
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
    
    if ([[_private->URLToIconURL objectForKey:URL] isEqualToString:iconURL] &&
        [_private->iconsOnDiskWithURLs containsObject:iconURL]) {
        // Don't do any work if the icon URL is already bound to the site URL
        return;
    }
    
    [_private->URLToIconURL setObject:iconURL forKey:URL];
    [_private->iconURLToURLs _web_setObjectUsingSetIfNecessary:URL forKey:iconURL];
    
    int futureRetainCount = (int)(void *)CFDictionaryGetValue(_private->futureURLToRetainCount, URL);
    
    if (futureRetainCount != 0) {
        int retainCount = (int)(void *)CFDictionaryGetValue(_private->iconURLToRetainCount, iconURL);
        int newRetainCount = retainCount + futureRetainCount;
        CFDictionarySetValue(_private->iconURLToRetainCount, iconURL, (void *)newRetainCount);
        CFDictionaryRemoveValue(_private->futureURLToRetainCount, URL);
    }
    
    [self _sendNotificationForURL:URL];
    [self _updateFileDatabase];
}

- (BOOL)_hasIconForIconURL:(NSString *)iconURL;
{
    ASSERT([self _isEnabled]);
    
    return (([_private->iconURLToIcons objectForKey:iconURL] ||
	     [_private->iconURLsWithNoIcons containsObject:iconURL] ||
             [_private->iconsOnDiskWithURLs containsObject:iconURL]) &&
            CFDictionaryGetValue(_private->iconURLToRetainCount,iconURL));
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
    [_private->URLToIconURL release];
    [_private->iconURLToURLs release];
    [_private->iconsOnDiskWithURLs release];
    [_private->originalIconsOnDiskWithURLs release];
    _private->URLToIconURL = [[NSMutableDictionary alloc] init];
    _private->iconURLToURLs = [[NSMutableDictionary alloc] init];
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
    NSMutableDictionary *URLToIconURL = nil;
    if (v <= WebIconDatabaseCurrentVersion) {
        URLToIconURL = [[fileDB objectForKey:WebURLToIconURLKey] retain];
        // Remove the old unnecessary mapping files.
        if (v < WebIconDatabaseCurrentVersion) {
            [fileDB removeObjectForKey:ObsoleteIconsOnDiskKey];
            [fileDB removeObjectForKey:ObsoleteIconURLToURLsKey];
        }        
    }
    
    if (![URLToIconURL isKindOfClass:[NSMutableDictionary class]]) {
        [self _clearDictionaries];
        return;
    }

    // Keep a set of icon URLs on disk so we know what we need to write out or remove.
    NSMutableSet *iconsOnDiskWithURLs = [[NSMutableSet alloc] initWithArray:[URLToIconURL allValues]];

    // Reverse URLToIconURL so we have an icon URL to site URLs dictionary. 
    NSMutableDictionary *iconURLToURLs = [[NSMutableDictionary alloc] initWithCapacity:[_private->iconsOnDiskWithURLs count]];
    NSEnumerator *enumerator = [URLToIconURL keyEnumerator];
    NSString *URL;
    while ((URL = [enumerator nextObject])) {
        NSString *iconURL = (NSString *)[URLToIconURL objectForKey:URL];
        if (![URL isKindOfClass:[NSString class]] || ![iconURL isKindOfClass:[NSString class]]) {
            [self _clearDictionaries];
            return;
        }
        [iconURLToURLs _web_setObjectUsingSetIfNecessary:URL forKey:iconURL];
    }

    _private->URLToIconURL = URLToIconURL;
    _private->iconURLToURLs = iconURLToURLs;
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
    NSMutableDictionary *URLToIconURLCopy = [_private->URLToIconURL mutableCopy];
    [fileDB setObject:URLToIconURLCopy forKey:WebURLToIconURLKey];
    [URLToIconURLCopy release];
}

- (void)_applicationWillTerminate:(NSNotification *)notification
{
    // Should only cause a write if user quit before 3 seconds after the last _updateFileDatabase
    [_private->fileDatabase sync];
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
    
    int retainCount = (int)(void *)CFDictionaryGetValue(_private->iconURLToRetainCount, iconURLString);
    int newRetainCount = retainCount + 1;

    CFDictionarySetValue(_private->iconURLToRetainCount, iconURLString, (void *)newRetainCount);

    if (newRetainCount == 1 && ![_private->iconsOnDiskWithURLs containsObject:iconURLString]){
        [_private->iconsToSaveWithURLs addObject:iconURLString];
        [_private->iconsToEraseWithURLs removeObject:iconURLString];
    }
}

- (void)_releaseIconForIconURLString:(NSString *)iconURLString
{
    ASSERT(iconURLString);
    
    int retainCount = (int)(void *)CFDictionaryGetValue(_private->iconURLToRetainCount, iconURLString);

    if (!retainCount) {
        ASSERT_NOT_REACHED();
        return;
    }
    
    int newRetainCount = retainCount - 1;
    CFDictionarySetValue(_private->iconURLToRetainCount, iconURLString, (void *)newRetainCount);

    if (newRetainCount == 0) {
        if([_private->iconsOnDiskWithURLs containsObject:iconURLString]){
            [_private->iconsToEraseWithURLs addObject:iconURLString];
            [_private->iconsToSaveWithURLs removeObject:iconURLString];
        }

        // Remove the icon's images
        [_private->iconURLToIcons removeObjectForKey:iconURLString];

        // Remove negative cache item for icon, if any
        [_private->iconURLsWithNoIcons removeObject:iconURLString];

        // Remove the icon's retain count
        CFDictionaryRemoveValue(_private->iconURLToRetainCount, iconURLString);

        // Remove the icon's associated site URLs
        [iconURLString retain];
        id URLs = [_private->iconURLToURLs objectForKey:iconURLString];
        if ([URLs isKindOfClass:[NSMutableSet class]]) {
            [_private->URLToIconURL removeObjectsForKeys:[URLs allObjects]];
        } else {
            ASSERT([URLs isKindOfClass:[NSString class]]);
            [_private->URLToIconURL removeObjectForKey:URLs];
        }
        [_private->iconURLToURLs removeObjectForKey:iconURLString];
        [iconURLString release];
    }
}

- (void)_retainFutureIconForURL:(NSString *)URL
{
    ASSERT(URL);
    
    int retainCount = (int)(void *)CFDictionaryGetValue(_private->futureURLToRetainCount, URL);
    CFDictionarySetValue(_private->futureURLToRetainCount, URL, (void *)(retainCount + 1));
}

- (void)_releaseFutureIconForURL:(NSString *)URL
{
    ASSERT(URL);
    
    int retainCount = (int)(void *)CFDictionaryGetValue(_private->futureURLToRetainCount, URL);

    if (!retainCount) {
        ERROR("The future icon for %@ was released before it was retained.", URL);
        return;
    }

    int newRetainCount = retainCount - 1;

    if (newRetainCount == 0){
        CFDictionaryRemoveValue(_private->futureURLToRetainCount, URL);
    } else {
        CFDictionarySetValue(_private->futureURLToRetainCount, URL, (void *)newRetainCount);
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
