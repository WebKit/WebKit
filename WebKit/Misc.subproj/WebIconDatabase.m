//
//  WebIconDatabase.m
//  WebKit
//
//  Created by Chris Blumenberg on Tue Aug 27 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WebIconDatabase.h>

#import <WebKit/WebIconDatabasePrivate.h>
#import <WebKit/WebKitLogging.h>

#import <WebFoundation/WebNSStringExtras.h>
#import <WebFoundation/WebNSURLExtras.h>
#import <WebFoundation/WebFileDatabase.h>

#define WebIconDatabaseDefaultDirectory ([NSString stringWithFormat:@"%@/%@", NSHomeDirectory(), @"Library/Caches/com.apple.WebKit/Icons"])

#define WebIconsOnDiskKey @"WebIconsOnDisk"
#define WebSiteURLToIconURLKey 	@"WebSiteURLToIconURLKey"
#define WebIconURLToSiteURLsKey @"WebIconURLToSiteURLs"
#define WebHostToSiteURLsKey @"WebHostToSiteURLs"

NSString *WebIconDidChangeNotification = @"WebIconDidChangeNotification";
NSString *WebIconNotificationUserInfoSiteURLKey = @"WebIconNotificationUserInfoSiteURLKey";

NSSize WebIconSmallSize = {16, 16};
NSSize WebIconMediumSize = {32, 32};
NSSize WebIconLargeSize = {128, 128};

@interface NSEnumerator (WebIconDatabase)
- (BOOL)_web_isAllStrings;
@end

@interface WebIconDatabase (WebInternal)

- (void)_createFileDatabase;
- (void)_loadIconDictionaries;
- (void)_updateFileDatabase;
- (NSMutableDictionary *)_iconsForIconURLString:(NSString *)iconURL;
- (NSImage *)_iconForFileURL:(NSURL *)fileURL withSize:(NSSize)size;
- (NSString *)_pathForBuiltInIconForHost:(NSString *)host;
- (NSMutableDictionary *)_builtInIconsForHost:(NSString *)host;
- (void)_retainIconForIconURLString:(NSString *)iconURL;
- (void)_releaseIconForIconURLString:(NSString *)iconURL;
- (void)_retainFutureIconForSiteURL:(NSURL *)siteURL;
- (void)_releaseFutureIconForSiteURL:(NSURL *)siteURL;
- (void)_retainOriginalIconsOnDisk;
- (void)_releaseOriginalIconsOnDisk;
- (void)_sendNotificationForSiteURL:(NSURL *)siteURL;
- (void)_addObject:(id)object toSetForKey:(id)key inDictionary:(NSMutableDictionary *)dictionary;
- (NSURL *)_uniqueIconURL;
- (NSImage *)_largestIconFromDictionary:(NSMutableDictionary *)icons;
- (NSMutableDictionary *)_iconsBySplittingRepresentationsOfIcon:(NSImage *)icon;
- (NSImage *)_iconFromDictionary:(NSMutableDictionary *)icons forSize:(NSSize)size cache:(BOOL)cache;
- (NSImage *)_iconByScalingIcon:(NSImage *)icon toSize:(NSSize)size;

@end


@implementation WebIconDatabase

+ (WebIconDatabase *)sharedIconDatabase
{
    static WebIconDatabase *database = nil;
    
    if (!database) {
        database = [[WebIconDatabase alloc] init];
    }
    return database;
}

- init
{
    [super init];
    
    _private = [[WebIconDatabasePrivate alloc] init];
    
    [self _createFileDatabase];
    [self _loadIconDictionaries];

    _private->iconURLToIcons = [[NSMutableDictionary dictionary] retain];
    _private->iconURLToRetainCount = [[NSMutableDictionary dictionary] retain];
    _private->futureSiteURLToRetainCount = [[NSMutableDictionary dictionary] retain];
    _private->pathToBuiltInIcons = [[NSMutableDictionary dictionary] retain];
    _private->hostToBuiltInIconPath = [[NSMutableDictionary dictionary] retain];

    _private->iconsToEraseWithURLs = [[NSMutableSet set] retain];
    _private->iconsToSaveWithURLs = [[NSMutableSet set] retain];
    
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(applicationWillTerminate:)
                                                 name:NSApplicationWillTerminateNotification
                                               object:NSApp];

    // Retain icons on disk then release them once clean-up has begun.
    // This gives the client the opportunity to retain them before they are erased.
    [self _retainOriginalIconsOnDisk];
    [self performSelector:@selector(_releaseOriginalIconsOnDisk) withObject:nil afterDelay:0];
    
    return self;
}

- (NSImage *)iconForSiteURL:(NSURL *)siteURL withSize:(NSSize)size
{
    return [self iconForSiteURL:siteURL withSize:size cache:YES];
}

- (NSImage *)iconForSiteURL:(NSURL *)siteURL withSize:(NSSize)size cache:(BOOL)cache
{
    ASSERT(size.width);
    ASSERT(size.height);
    
    if (!siteURL) {
        return [self defaultIconWithSize:size];
    }
    
    if ([siteURL isFileURL]) {
        return [self _iconForFileURL:siteURL withSize:size];
    }

    NSMutableDictionary *icons = [self _builtInIconsForHost:[siteURL host]];
    
    if (!icons) {
        NSString *iconURLString = [_private->siteURLToIconURL objectForKey:[siteURL absoluteString]];
        if (!iconURLString) {
            // Don't have it
            return [self defaultIconWithSize:size];
        }

        icons = [self _iconsForIconURLString:iconURLString];
        if (!icons) {
            // This should not happen
            return [self defaultIconWithSize:size];
        }        
    }

    return [self _iconFromDictionary:icons forSize:size cache:cache];
}

- (NSImage *)defaultIconWithSize:(NSSize)size
{
    ASSERT(size.width);
    ASSERT(size.height);
    
    if (!_private->defaultIcons) {
        NSString *path = [[NSBundle bundleForClass:[self class]] pathForResource:@"url_icon" ofType:@"tiff"];
        if (path) {
            NSImage *icon = [[NSImage alloc] initByReferencingFile:path];
            _private->defaultIcons = [[NSMutableDictionary dictionaryWithObject:icon forKey:[NSValue valueWithSize:[icon size]]] retain];
            [icon release];
        }
    }

    return [self _iconFromDictionary:_private->defaultIcons forSize:size cache:YES];
}

- (void)setIcon:(NSImage *)icon forSiteURL:(NSURL *)siteURL
{
    if(!icon || !siteURL){
        return;
    }

    NSURL *iconURL = [self _uniqueIconURL];
    
    [self _setIcon:icon forIconURL:iconURL];
    [self _setIconURL:iconURL forSiteURL:siteURL];
}

- (void)setIcon:(NSImage *)icon forHost:(NSString *)host
{
    if(!icon || !host){
        return;
    }

    NSMutableSet *siteURLs = [_private->hostToSiteURLs objectForKey:host];
    NSURL *iconURL = [self _uniqueIconURL];
    NSEnumerator *enumerator;
    NSURL *siteURL;
    
    [self _setIcon:icon forIconURL:iconURL];

    if(siteURLs){
        enumerator = [siteURLs objectEnumerator];
    
        while ((siteURL = [enumerator nextObject]) != nil) {
            [self releaseIconForSiteURL:siteURL];
            [self _setIconURL:iconURL forSiteURL:siteURL];
            [self retainIconForSiteURL:siteURL];
        }
    }
}

- (void)retainIconForSiteURL:(NSURL *)siteURL
{
    NSString *iconURLString = [_private->siteURLToIconURL objectForKey:[siteURL absoluteString]];
    
    if(iconURLString){
        [self _retainIconForIconURLString:iconURLString];
    }else{
        // Retaining an icon not in the DB. This is OK. Just remember this.
        [self _retainFutureIconForSiteURL:siteURL];
    }
}

- (void)releaseIconForSiteURL:(NSURL *)siteURL
{
    NSString *iconURLString = [_private->siteURLToIconURL objectForKey:[siteURL absoluteString]];
    
    if(iconURLString){
        [self _releaseIconForIconURLString:iconURLString];
    }else{
        // Releasing an icon not in the DB. This is OK. Just remember this.
        [self _releaseFutureIconForSiteURL:siteURL];        
    }
}

- (void)delayDatabaseCleanup
{
    if(_private->didCleanup){
        [NSException raise:NSGenericException format:@"delayDatabaseCleanup cannot be called after cleanup has begun"];
    }
    
    _private->cleanupCount++;
    //NSLog(@"delayDatabaseCleanup %d", _private->cleanupCount);
}

- (void)allowDatabaseCleanup
{
    if(_private->didCleanup){
        [NSException raise:NSGenericException format:@"allowDatabaseCleanup cannot be called after cleanup has begun"];
    }
    
    _private->cleanupCount--;
    //NSLog(@"allowDatabaseCleanup %d", _private->cleanupCount);

    if(_private->cleanupCount == 0 && _private->waitingToCleanup){
        [self _releaseOriginalIconsOnDisk];
    }
}

- (void)applicationWillTerminate:(NSNotification *)notification
{
    // Should only cause a write if user quit before 3 seconds after the last _updateFileDatabase
    [_private->fileDatabase sync];
}

@end


@implementation WebIconDatabase (WebPrivate)

- (void)_createFileDatabase
{
    // FIXME: Make defaults key public somehow
    NSString *databaseDirectory = [[NSUserDefaults standardUserDefaults] objectForKey:@"WebIconDatabaseDirectory"];

    if (!databaseDirectory) {
        databaseDirectory = WebIconDatabaseDefaultDirectory;
    }

    _private->fileDatabase = [[WebFileDatabase alloc] initWithPath:databaseDirectory];
    [_private->fileDatabase setSizeLimit:20000000];
    [_private->fileDatabase open];
}

- (BOOL)_iconDictionariesAreGood
{
    NSEnumerator *e;
    id o;

    if (![_private->iconsOnDiskWithURLs isKindOfClass:[NSMutableSet class]]) {
        return NO;
    }
    if (![[_private->iconsOnDiskWithURLs objectEnumerator] _web_isAllStrings]) {
        return NO;
    }

    if (![_private->siteURLToIconURL isKindOfClass:[NSMutableDictionary class]]) {
        return NO;
    }
    if (![[_private->siteURLToIconURL keyEnumerator] _web_isAllStrings]) {
        return NO;
    }
    if (![[_private->siteURLToIconURL objectEnumerator] _web_isAllStrings]) {
        return NO;
    }

    if (![_private->iconURLToSiteURLs isKindOfClass:[NSMutableDictionary class]]) {
        return NO;
    }
    if (![[_private->iconURLToSiteURLs keyEnumerator] _web_isAllStrings]) {
        return NO;
    }
    e = [_private->iconURLToSiteURLs objectEnumerator];
    while ((o = [e nextObject])) {
        if (![o isKindOfClass:[NSMutableSet class]]) {
            return NO;
        }
        if (![[o objectEnumerator] _web_isAllStrings]) {
            return NO;
        }
    }

    if (![_private->hostToSiteURLs isKindOfClass:[NSMutableDictionary class]]) {
        return NO;
    }
    if (![[_private->hostToSiteURLs keyEnumerator] _web_isAllStrings]) {
        return NO;
    }
    e = [_private->hostToSiteURLs objectEnumerator];
    while ((o = [e nextObject])) {
        if (![o isKindOfClass:[NSMutableSet class]]) {
            return NO;
        }
        if (![[o objectEnumerator] _web_isAllStrings]) {
            return NO;
        }
    }
    
    return YES;
}

- (void)_loadIconDictionaries
{
    WebFileDatabase *fileDB = _private->fileDatabase;

    _private->iconsOnDiskWithURLs = [fileDB objectForKey:WebIconsOnDiskKey];
    _private->siteURLToIconURL = [fileDB objectForKey:WebSiteURLToIconURLKey];
    _private->iconURLToSiteURLs = [fileDB objectForKey:WebIconURLToSiteURLsKey];
    _private->hostToSiteURLs = [fileDB objectForKey:WebHostToSiteURLsKey];

    if (![self _iconDictionariesAreGood]) {
        _private->iconsOnDiskWithURLs = [NSMutableSet set];
        _private->siteURLToIconURL = [NSMutableDictionary dictionary];
        _private->iconURLToSiteURLs = [NSMutableDictionary dictionary];
        _private->hostToSiteURLs = [NSMutableDictionary dictionary];
    }

    [_private->iconsOnDiskWithURLs retain];
    [_private->iconURLToSiteURLs retain];
    [_private->siteURLToIconURL retain];
    [_private->hostToSiteURLs retain];
}

// Only called by _setIconURL:forKey:
- (void)_updateFileDatabase
{
    if(_private->cleanupCount != 0){
        return;
    }

    //NSLog(@"_updateFileDatabase");

    WebFileDatabase *fileDB = _private->fileDatabase;

    NSEnumerator *enumerator;
    NSData *iconData;
    NSString *iconURLString;

    // Erase icons that have been released that are on disk
    enumerator = [_private->iconsToEraseWithURLs objectEnumerator];

    while ((iconURLString = [enumerator nextObject]) != nil) {
        //NSLog(@"removing %@", iconURL);
        [fileDB removeObjectForKey:iconURLString];
        [_private->iconsOnDiskWithURLs removeObject:iconURLString];
    }

    // Save icons that have been retained that are not already on disk
    enumerator = [_private->iconsToSaveWithURLs objectEnumerator];

    while ((iconURLString = [enumerator nextObject]) != nil) {
        //NSLog(@"writing %@", iconURL);
        iconData = [[self _largestIconFromDictionary:[_private->iconURLToIcons objectForKey:iconURLString]] TIFFRepresentation];
        if(iconData){
            [fileDB setObject:iconData forKey:iconURLString];
            [_private->iconsOnDiskWithURLs addObject:iconURLString];
        }
    }
    
    [_private->iconsToEraseWithURLs removeAllObjects];
    [_private->iconsToSaveWithURLs removeAllObjects];

    // Save the icon dictionaries to disk
    [fileDB setObject:_private->iconsOnDiskWithURLs forKey:WebIconsOnDiskKey];
    [fileDB setObject:_private->siteURLToIconURL forKey:WebSiteURLToIconURLKey];
    [fileDB setObject:_private->iconURLToSiteURLs forKey:WebIconURLToSiteURLsKey];
    [fileDB setObject:_private->hostToSiteURLs forKey:WebHostToSiteURLsKey];
}

- (BOOL)_hasIconForSiteURL:(NSURL *)siteURL
{
    if ([siteURL isFileURL] ||
        [self _pathForBuiltInIconForHost:[siteURL host]] ||
        [_private->siteURLToIconURL objectForKey:[siteURL absoluteString]]){
        return YES;
    }
    
    return NO;
}

- (NSMutableDictionary *)_iconsForIconURLString:(NSString *)iconURLString
{
    if(!iconURLString){
        return nil;
    }
    
    NSMutableDictionary *icons = [_private->iconURLToIcons objectForKey:iconURLString];
    double start, duration;
    
    if(!icons){
        // Not in memory, check disk
        if([_private->iconsOnDiskWithURLs containsObject:iconURLString]){
            
            start = CFAbsoluteTimeGetCurrent();
            NSData *iconData = [_private->fileDatabase objectForKey:iconURLString];
            
            if(iconData){
                NSImage *icon = [[NSImage alloc] initWithData:iconData];
                icons = [self _iconsBySplittingRepresentationsOfIcon:icon];

                if(icons){
                    duration = CFAbsoluteTimeGetCurrent() - start;
                    LOG(Timing, "loading and creating icon %@ took %f seconds", iconURLString, duration);
                    [_private->iconURLToIcons setObject:icons forKey:iconURLString];
                }
            }
        }
    }
    
    return icons;
}


- (NSImage *)_iconForFileURL:(NSURL *)fileURL withSize:(NSSize)size
{
    ASSERT(size.width);
    ASSERT(size.height);
    
    NSWorkspace *workspace = [NSWorkspace sharedWorkspace];
    NSImage *icon;
    
    if([[[fileURL path] pathExtension] _web_hasCaseInsensitivePrefix:@"htm"]){
        if(!_private->htmlIcons){
            icon = [workspace iconForFileType:@"html"];
            _private->htmlIcons = [[self _iconsBySplittingRepresentationsOfIcon:icon] retain];
        }
        return [self _iconFromDictionary:_private->htmlIcons forSize:size cache:YES];
    }else{
        icon = [workspace iconForFile:[fileURL path]];
        return [self _iconByScalingIcon:icon toSize:size];
    }
}

- (NSString *)_pathForBuiltInIconForHost:(NSString *)host
{
    NSArray *hostParts = [host componentsSeparatedByString:@"."];
    NSMutableString *truncatedHost = [NSMutableString string];
    NSString *hostPart, *path;
    BOOL firstPart = YES;

    NSEnumerator *enumerator = [hostParts reverseObjectEnumerator];
    while ((hostPart = [enumerator nextObject]) != nil) {
        if(firstPart){
            [truncatedHost insertString:hostPart atIndex:0];
            firstPart = NO;
        }else{
            [truncatedHost insertString:[NSString stringWithFormat:@"%@.", hostPart] atIndex:0];
            path = [_private->hostToBuiltInIconPath objectForKey:truncatedHost];
            if(path){
                return path;
            }
        }
    }
    
    return nil;
}

- (NSMutableDictionary *)_builtInIconsForHost:(NSString *)host
{
    NSString *path = [self _pathForBuiltInIconForHost:host];
    NSMutableDictionary *icons = nil;
    
    if(path){
        icons = [_private->pathToBuiltInIcons objectForKey:path];    
        if(!icons){
            NSImage *icon = [[NSImage alloc] initWithContentsOfFile:path];
            if(icon){
                icons = [self _iconsBySplittingRepresentationsOfIcon:icon];
                [_private->pathToBuiltInIcons setObject:icons forKey:path];
                [icon release];
            }
        }
    }
    
    return icons;
}

- (void)_setIcon:(NSImage *)icon forIconURL:(NSURL *)iconURL
{
    if(!icon || !iconURL){
        return;
    }
    
    NSString *iconURLString = [iconURL absoluteString];

    NSMutableDictionary *icons = [_private->iconURLToIcons objectForKey:iconURLString];
    [icons removeAllObjects];

    icons = [self _iconsBySplittingRepresentationsOfIcon:icon];

    if(!icons){
        return;
    }
    
    [_private->iconURLToIcons setObject:icons forKey:iconURLString];

    [self _retainIconForIconURLString:iconURLString];
    
    // Release the newly created icon much like an autorelease.
    // This gives the client enough time to retain it.
    // FIXME: Should use an actual autorelease here using a proxy object instead.
    [self performSelector:@selector(_releaseIconForIconURLString:) withObject:iconURLString afterDelay:0];
}

// FIXME: fix custom icons
- (void)_setIconURL:(NSURL *)iconURL forSiteURL:(NSURL *)siteURL
{
    if(!iconURL || !siteURL){
        return;
    }
    
    NSString *siteURLString = [siteURL absoluteString];
    NSString *iconURLString = [iconURL absoluteString];
    
    [_private->siteURLToIconURL setObject:iconURLString forKey:siteURLString];
    
    [self _addObject:siteURLString toSetForKey:iconURLString inDictionary:_private->iconURLToSiteURLs];

    [self _addObject:siteURLString toSetForKey:[siteURL host] inDictionary:_private->hostToSiteURLs];
    
    NSNumber *predeterminedRetainCount = [_private->futureSiteURLToRetainCount objectForKey:siteURLString];

    if(predeterminedRetainCount){
        NSNumber *retainCount = [_private->iconURLToRetainCount objectForKey:iconURLString];

        if(!retainCount){
            //NSLog(@"_setIconURL: forKey: no retainCount for iconURL");
            return;
        }

        int newRetainCount = [retainCount intValue] + [predeterminedRetainCount intValue];
        [_private->iconURLToRetainCount setObject:[NSNumber numberWithInt:newRetainCount] forKey:iconURLString];
        [_private->futureSiteURLToRetainCount removeObjectForKey:siteURLString];
    }

    [self _sendNotificationForSiteURL:siteURL];
    [self _updateFileDatabase];
}

- (void)_setBuiltInIconAtPath:(NSString *)path forHost:(NSString *)host
{
    if(!path || !host){
        return;
    }

    [_private->hostToBuiltInIconPath setObject:path forKey:host];
    
    NSMutableSet *siteURLStrings = [_private->hostToSiteURLs objectForKey:host];
    NSString *siteURLString;

    NSEnumerator *enumerator = [siteURLStrings objectEnumerator];
    while ((siteURLString = [enumerator nextObject]) != nil) {
        [self _sendNotificationForSiteURL:[NSURL _web_URLWithString:siteURLString]];
    }
}

- (void)_retainIconForIconURLString:(NSString *)iconURLString
{
    NSNumber *retainCount = [_private->iconURLToRetainCount objectForKey:iconURLString];
    int newRetainCount;
    
    if(!retainCount){
        newRetainCount = 1;
    }else{
        newRetainCount = [retainCount intValue] + 1;
    }

    [_private->iconURLToRetainCount setObject:[NSNumber numberWithInt:newRetainCount] forKey:iconURLString];

    if(newRetainCount == 1 && ![_private->iconsOnDiskWithURLs containsObject:iconURLString]){
        [_private->iconsToSaveWithURLs addObject:iconURLString];
        [_private->iconsToEraseWithURLs removeObject:iconURLString];
    }

    //NSLog(@"_retainIconForIconURLString: %@ %d", iconURLString, newRetainCount);
}

- (void)_releaseIconForIconURLString:(NSString *)iconURLString
{
    NSNumber *retainCount = [_private->iconURLToRetainCount objectForKey:iconURLString];

    if (!retainCount) {
        ERROR("retain count is zero for %@", iconURLString);
        return;
    }
    
    int newRetainCount = [retainCount intValue] - 1;
    [_private->iconURLToRetainCount setObject:[NSNumber numberWithInt:newRetainCount] forKey:iconURLString];

    if(newRetainCount == 0){

        if([_private->iconsOnDiskWithURLs containsObject:iconURLString]){
            [_private->iconsToEraseWithURLs addObject:iconURLString];
            [_private->iconsToSaveWithURLs removeObject:iconURLString];
        }

        // Remove the icon's images
        NSMutableDictionary *icons = [_private->iconURLToIcons objectForKey:iconURLString];
        [icons removeAllObjects];
        [_private->iconURLToIcons removeObjectForKey:iconURLString];

        // Remove the icon's retain count
        [_private->iconURLToRetainCount removeObjectForKey:iconURLString];

        // Remove the icon's associated site URLs
        NSMutableSet *siteURLStringsForHost, *siteURLStrings;
        NSEnumerator *enumerator;
        NSString *siteURLString;
        
        siteURLStrings = [_private->iconURLToSiteURLs objectForKey:iconURLString];
        [_private->siteURLToIconURL removeObjectsForKeys:[siteURLStrings allObjects]];

        enumerator = [siteURLStrings objectEnumerator];
        while ((siteURLString = [enumerator nextObject]) != nil) {
            NSURL *siteURL = [NSURL _web_URLWithString:siteURLString];
            siteURLStringsForHost = [_private->hostToSiteURLs objectForKey:[siteURL host]];
            [siteURLStringsForHost removeObject:siteURLString];
            if([siteURLStringsForHost count] == 0){
                [_private->hostToSiteURLs removeObjectForKey:[siteURL host]];
            }
        }
        
        [_private->iconURLToSiteURLs removeObjectForKey:iconURLString];
    }

    //NSLog(@"_releaseIconForIconURLString: %@ %d", iconURLString, newRetainCount);
}

- (void)_retainFutureIconForSiteURL:(NSURL *)siteURL
{
    NSString *siteURLString = [siteURL absoluteString];
    
    NSNumber *retainCount = [_private->futureSiteURLToRetainCount objectForKey:siteURLString];
    int newRetainCount;

    if(retainCount){
        newRetainCount = [retainCount intValue] + 1;
    }else{
        newRetainCount = 1;
    }

    [_private->futureSiteURLToRetainCount setObject:[NSNumber numberWithInt:newRetainCount] forKey:siteURLString];

    ////NSLog(@"_setFutureIconRetainToDictionary: %@ %d", key, newRetainCount);
}

- (void)_releaseFutureIconForSiteURL:(NSURL *)siteURL
{
    NSString *siteURLString = [siteURL absoluteString];
    
    NSNumber *retainCount = [_private->futureSiteURLToRetainCount objectForKey:siteURLString];

    if(!retainCount){
        [NSException raise:NSGenericException
                    format:@"Releasing a future icon that was not previously retained."];
    }

    int newRetainCount = [retainCount intValue] - 1;

    if(newRetainCount == 0){
        [_private->futureSiteURLToRetainCount removeObjectForKey:siteURLString];
    }else{
        [_private->futureSiteURLToRetainCount setObject:[NSNumber numberWithInt:newRetainCount] forKey:siteURLString];
    }

    ////NSLog(@"_setFutureIconReleaseToDictionary: %@ %d", key, newRetainCount);
}

- (void)_retainOriginalIconsOnDisk
{
    NSEnumerator *enumerator;
    NSString *iconURLString;

    //NSLog(@"_retainOriginalOnDiskIcons");
    enumerator = [_private->iconsOnDiskWithURLs objectEnumerator];

    while ((iconURLString = [enumerator nextObject]) != nil) {
        [self _retainIconForIconURLString:iconURLString];
    }
}

- (void)_releaseOriginalIconsOnDisk
{
    if(_private->cleanupCount > 0){
        _private->waitingToCleanup = YES;
        return;
    }
    //NSLog(@"_releaseOriginalOnDiskIcons, %@", _private->iconsOnDiskWithURLs);
    NSEnumerator *enumerator = [_private->iconsOnDiskWithURLs objectEnumerator];
    NSString *iconURLString;

    while ((iconURLString = [enumerator nextObject]) != nil) {
        [self _releaseIconForIconURLString:iconURLString];
    }

    _private->didCleanup = YES;
}

- (void)_sendNotificationForSiteURL:(NSURL *)siteURL
{
    NSDictionary *userInfo = [NSDictionary dictionaryWithObject:siteURL forKey:WebIconNotificationUserInfoSiteURLKey];
    [[NSNotificationCenter defaultCenter] postNotificationName:WebIconDidChangeNotification
                                                        object:self
                                                      userInfo:userInfo];
}

- (void)_addObject:(id)object toSetForKey:(id)key inDictionary:(NSMutableDictionary *)dictionary
{
    NSMutableSet *set = [dictionary objectForKey:key];

    if(!set){
        set = [NSMutableSet set];
    }
        
    [set addObject:object];
    [dictionary setObject:set forKey:key];
}

- (NSURL *)_uniqueIconURL
{
    CFUUIDRef uid = CFUUIDCreate(NULL);
    NSString *string = (NSString *)CFUUIDCreateString(NULL, uid);
    NSURL *uniqueURL = [NSURL _web_URLWithString:[NSString stringWithFormat:@"icon:%@", string]];

    CFRelease(uid);
    CFRelease(string);

    return uniqueURL;
}

- (NSImage *)_largestIconFromDictionary:(NSMutableDictionary *)icons
{
    NSEnumerator *enumerator = [icons keyEnumerator];
    NSValue *currentSize, *largestSize=nil;
    float currentSizeArea, largestSizeArea=0;
    NSSize currentSizeSize;

    while ((currentSize = [enumerator nextObject]) != nil) {
        currentSizeSize = [currentSize sizeValue];
        currentSizeArea = currentSizeSize.width * currentSizeSize.height;
        if(!largestSizeArea || (currentSizeArea > largestSizeArea)){
            largestSize = currentSize;
            largestSizeArea = currentSizeArea;
        }
    }

    return [icons objectForKey:largestSize];
}

- (NSMutableDictionary *)_iconsBySplittingRepresentationsOfIcon:(NSImage *)icon
{
    if(!icon){
        return nil;
    }

    NSMutableDictionary *icons = [NSMutableDictionary dictionary];
    NSEnumerator *enumerator = [[icon representations] objectEnumerator];
    NSImageRep *rep;
    NSImage *subIcon;
    NSSize size;

    while ((rep = [enumerator nextObject]) != nil) {
        size = [rep size];
        subIcon = [[NSImage alloc] initWithSize:size];
        [subIcon addRepresentation:rep];
        [icons setObject:subIcon forKey:[NSValue valueWithSize:size]];
        [subIcon release];
    }

    if([icons count] > 0){
        return icons;
    }

    return nil;
}

- (NSImage *)_iconFromDictionary:(NSMutableDictionary *)icons forSize:(NSSize)size cache:(BOOL)cache
{
    ASSERT(size.width);
    ASSERT(size.height);

    NSImage *icon = [icons objectForKey:[NSValue valueWithSize:size]];

    if(!icon){
        icon = [[[self _largestIconFromDictionary:icons] copy] autorelease];
        icon = [self _iconByScalingIcon:icon toSize:size];

        if(cache){
            [icons setObject:icon forKey:[NSValue valueWithSize:size]];
        }
    }

    return icon;
}

- (NSImage *)_iconByScalingIcon:(NSImage *)icon toSize:(NSSize)size
{
    ASSERT(size.width);
    ASSERT(size.height);
    
    double start, duration;
    NSImage *scaledIcon;
        
    start = CFAbsoluteTimeGetCurrent();

    if([[NSUserDefaults standardUserDefaults] boolForKey:@"Experiments"]){
        // Note: This doesn't seem to make a difference for scaling up.
        
        NSSize originalSize = [icon size];
        scaledIcon = [[[NSImage alloc] initWithSize:size] autorelease];

        [scaledIcon lockFocus];
        NSGraphicsContext *currentContent = [NSGraphicsContext currentContext];
        [currentContent setImageInterpolation:NSImageInterpolationHigh];
        [icon drawInRect:NSMakeRect(0, 0, size.width, size.height)
                fromRect:NSMakeRect(0, 0, originalSize.width, originalSize.height)
               operation:NSCompositeSourceOver	// Renders transparency correctly
                fraction:1.0];
        [scaledIcon unlockFocus];
        
    }else{
        scaledIcon = icon;
        [scaledIcon setScalesWhenResized:YES];
        [scaledIcon setSize:size];
    }
    
    duration = CFAbsoluteTimeGetCurrent() - start;
    LOG(Timing, "scaling icon took %f seconds.", duration);

    return scaledIcon;
}

@end

@implementation WebIconDatabasePrivate

@end

@implementation NSEnumerator (WebIconDatabase)

- (BOOL)_web_isAllStrings
{
    Class c = [NSString class];
    id o;
    while ((o = [self nextObject])) {
        if (![o isKindOfClass:c]) {
            return NO;
        }
    }
    return YES;
}

@end
