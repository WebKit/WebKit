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

#define WebIconsOnDiskKey 	@"WebIconsOnDisk"
#define WebSiteURLToIconURLKey 	@"WebSiteURLToIconURLKey"
#define WebIconURLToSiteURLsKey @"WebIconURLToSiteURLs"
#define WebHostToSiteURLsKey 	@"WebHostToSiteURLs"

NSString *WebIconDatabaseDidAddIconNotification = @"WebIconDatabaseDidAddIconNotification";
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
- (NSMutableDictionary *)_builtInIconsForHost:(NSString *)host;
- (void)_retainIconForIconURLString:(NSString *)iconURL;
- (void)_releaseIconForIconURLString:(NSString *)iconURL;
- (void)_retainFutureIconForSiteURL:(NSURL *)siteURL;
- (void)_releaseFutureIconForSiteURL:(NSURL *)siteURL;
- (void)_retainOriginalIconsOnDisk;
- (void)_releaseOriginalIconsOnDisk;
- (void)_sendNotificationForSiteURL:(NSURL *)siteURL;
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

    _private->iconURLToIcons = 			[[NSMutableDictionary dictionary] retain];
    _private->iconURLToRetainCount = 		[[NSMutableDictionary dictionary] retain];
    _private->futureSiteURLToRetainCount = 	[[NSMutableDictionary dictionary] retain];

    _private->iconsToEraseWithURLs = 	[[NSMutableSet set] retain];
    _private->iconsToSaveWithURLs = 	[[NSMutableSet set] retain];
    
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
    
    NSString *iconURLString = [_private->siteURLToIconURL objectForKey:[siteURL absoluteString]];
    if (!iconURLString) {
        // Don't have it
        return [self defaultIconWithSize:size];
    }

    NSMutableDictionary *icons = [self _iconsForIconURLString:iconURLString];
    if (!icons) {
        ERROR("WebIconDatabase said it had %@, but it doesn't.", iconURLString);
        return [self defaultIconWithSize:size];
    }        

    return [self _iconFromDictionary:icons forSize:size cache:cache];
}

- (NSImage *)iconForSiteURL:(NSURL *)siteURL withSize:(NSSize)size
{
    return [self iconForSiteURL:siteURL withSize:size cache:YES];
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

- (void)retainIconForSiteURL:(NSURL *)siteURL
{
    ASSERT(siteURL);
    
    NSString *iconURLString = [_private->siteURLToIconURL objectForKey:[siteURL absoluteString]];
    
    if(iconURLString){
        [self _retainIconForIconURLString:iconURLString];
    }else{
        [self _retainFutureIconForSiteURL:siteURL];
    }
}

- (void)releaseIconForSiteURL:(NSURL *)siteURL
{
    ASSERT(siteURL);
    
    NSString *iconURLString = [_private->siteURLToIconURL objectForKey:[siteURL absoluteString]];
    
    if(iconURLString){
        [self _releaseIconForIconURLString:iconURLString];
    }else{
        [self _releaseFutureIconForSiteURL:siteURL];        
    }
}

- (void)delayDatabaseCleanup
{
    if(_private->didCleanup){
        ERROR("delayDatabaseCleanup cannot be called after cleanup has begun");
        return;
    }
    
    _private->cleanupCount++;
}

- (void)allowDatabaseCleanup
{
    if(_private->didCleanup){
        ERROR("allowDatabaseCleanup cannot be called after cleanup has begun");
        return;
    }
    
    _private->cleanupCount--;

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
    
    return YES;
}

- (void)_loadIconDictionaries
{
    WebFileDatabase *fileDB = _private->fileDatabase;

    _private->iconsOnDiskWithURLs = 	[fileDB objectForKey:WebIconsOnDiskKey];
    _private->siteURLToIconURL = 	[fileDB objectForKey:WebSiteURLToIconURLKey];
    _private->iconURLToSiteURLs = 	[fileDB objectForKey:WebIconURLToSiteURLsKey];

    if (![self _iconDictionariesAreGood]) {
        _private->iconsOnDiskWithURLs = [NSMutableSet set];
        _private->siteURLToIconURL = 	[NSMutableDictionary dictionary];
        _private->iconURLToSiteURLs = 	[NSMutableDictionary dictionary];
    }

    [_private->iconsOnDiskWithURLs retain];
    [_private->iconURLToSiteURLs retain];
    [_private->siteURLToIconURL retain];
}

// Only called by _setIconURL:forKey:
- (void)_updateFileDatabase
{
    if(_private->cleanupCount != 0){
        return;
    }

    WebFileDatabase *fileDB = _private->fileDatabase;

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
        if(icons){
            // Save the 16 x 16 size icons as this is the only size the Safari uses.
            // If we ever use larger sizes, we should save the largest size so icons look better when scaling up.
            // This also worksaround the problem with cnet's blank 32x32 icon (3105486).
            NSImage *icon = [icons objectForKey:[NSValue valueWithSize:NSMakeSize(16,16)]];
            if (!icon) {
                // In case there is no 16 x 16 size.
                icon = [self _largestIconFromDictionary:icons];
            }
            NSData *iconData = [icon TIFFRepresentation];
            if(iconData){
                //NSLog(@"Writing icon: %@", iconURLString);
                [fileDB setObject:iconData forKey:iconURLString];
                [_private->iconsOnDiskWithURLs addObject:iconURLString];
            }
        }
    }
    
    [_private->iconsToEraseWithURLs removeAllObjects];
    [_private->iconsToSaveWithURLs removeAllObjects];

    // Save the icon dictionaries to disk
    [fileDB setObject:_private->iconsOnDiskWithURLs 	forKey:WebIconsOnDiskKey];
    [fileDB setObject:_private->siteURLToIconURL	forKey:WebSiteURLToIconURLKey];
    [fileDB setObject:_private->iconURLToSiteURLs 	forKey:WebIconURLToSiteURLsKey];
}

- (BOOL)_hasIconForIconURL:(NSURL *)iconURL;
{
    NSString *iconURLString = [iconURL absoluteString];
    return (([_private->iconURLToIcons objectForKey:iconURLString] ||
             [_private->iconsOnDiskWithURLs containsObject:iconURLString]) &&
            [_private->iconURLToRetainCount objectForKey:iconURLString]);
}

- (NSMutableDictionary *)_iconsForIconURLString:(NSString *)iconURLString
{
    ASSERT(iconURLString);
    
    NSMutableDictionary *icons = [_private->iconURLToIcons objectForKey:iconURLString];
    
    if(!icons){
        // Not in memory, check disk
        if([_private->iconsOnDiskWithURLs containsObject:iconURLString]){
            
#if LOG_ENABLED         
            double start = CFAbsoluteTimeGetCurrent();
#endif
            NSData *iconData = [_private->fileDatabase objectForKey:iconURLString];
            
            if(iconData){
                NSImage *icon = [[NSImage alloc] initWithData:iconData];
                icons = [self _iconsBySplittingRepresentationsOfIcon:icon];

                if(icons){
#if LOG_ENABLED 
                    double duration = CFAbsoluteTimeGetCurrent() - start;
                    LOG(Timing, "loading and creating icon %@ took %f seconds", iconURLString, duration);
#endif
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
        [self _scaleIcon:icon toSize:size];
        return icon;
    }
}

- (void)_setIcon:(NSImage *)icon forIconURL:(NSURL *)iconURL
{
    ASSERT(icon);
    ASSERT(iconURL);
    
    NSMutableDictionary *icons = [self _iconsBySplittingRepresentationsOfIcon:icon];

    if(!icons){
        return;
    }

    NSString *iconURLString = [iconURL absoluteString];
    [_private->iconURLToIcons setObject:icons forKey:iconURLString];

    [self _retainIconForIconURLString:iconURLString];
    
    // Release the newly created icon much like an autorelease.
    // This gives the client enough time to retain it.
    // FIXME: Should use an actual autorelease here using a proxy object instead.
    [self performSelector:@selector(_releaseIconForIconURLString:) withObject:iconURLString afterDelay:0];
}

- (void)_setIconURL:(NSURL *)iconURL forSiteURL:(NSURL *)siteURL
{
    ASSERT(iconURL);
    ASSERT(siteURL);
    ASSERT([self _hasIconForIconURL:iconURL]);
    
    NSString *siteURLString = [siteURL absoluteString];
    NSString *iconURLString = [iconURL absoluteString];

    if([[_private->siteURLToIconURL objectForKey:siteURLString] isEqualToString:iconURLString] &&
       [_private->iconsOnDiskWithURLs containsObject:iconURLString]){
        // Don't do any work if the icon URL is already bound to the site URL
        return;
    }
    
    [_private->siteURLToIconURL setObject:iconURLString forKey:siteURLString];
    
    NSMutableSet *siteURLStrings = [_private->iconURLToSiteURLs objectForKey:iconURLString];
    if(!siteURLStrings){
        siteURLStrings = [NSMutableSet set];
        [_private->iconURLToSiteURLs setObject:siteURLStrings forKey:iconURLString];
    }
    [siteURLStrings addObject:siteURLString];

    
    NSNumber *futureRetainCount = [_private->futureSiteURLToRetainCount objectForKey:siteURLString];

    if(futureRetainCount){
        NSNumber *retainCount = [_private->iconURLToRetainCount objectForKey:iconURLString];
        int newRetainCount = [retainCount intValue] + [futureRetainCount intValue];
        [_private->iconURLToRetainCount setObject:[NSNumber numberWithInt:newRetainCount] forKey:iconURLString];
        [_private->futureSiteURLToRetainCount removeObjectForKey:siteURLString];
    }

    [self _sendNotificationForSiteURL:siteURL];
    [self _updateFileDatabase];
}

- (void)_retainIconForIconURLString:(NSString *)iconURLString
{
    ASSERT(iconURLString);
    
    NSNumber *retainCount = [_private->iconURLToRetainCount objectForKey:iconURLString];
    int newRetainCount;
    
    if(retainCount){
        newRetainCount = [retainCount intValue] + 1;
    }else{
        newRetainCount = 1;        
    }

    [_private->iconURLToRetainCount setObject:[NSNumber numberWithInt:newRetainCount] forKey:iconURLString];

    if(newRetainCount == 1 && ![_private->iconsOnDiskWithURLs containsObject:iconURLString]){
        [_private->iconsToSaveWithURLs addObject:iconURLString];
        [_private->iconsToEraseWithURLs removeObject:iconURLString];
    }
}

- (void)_releaseIconForIconURLString:(NSString *)iconURLString
{
    ASSERT(iconURLString);
    
    NSNumber *retainCount = [_private->iconURLToRetainCount objectForKey:iconURLString];

    if (!retainCount) {
        ERROR("Tried to release a non-retained icon: %@", iconURLString);
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
        [_private->iconURLToIcons removeObjectForKey:iconURLString];
        
        // Remove the icon's retain count
        [_private->iconURLToRetainCount removeObjectForKey:iconURLString];

        // Remove the icon's associated site URLs
        [iconURLString retain];
        NSSet *siteURLStrings = [_private->iconURLToSiteURLs objectForKey:iconURLString];
        [_private->siteURLToIconURL removeObjectsForKeys:[siteURLStrings allObjects]];
        [_private->iconURLToSiteURLs removeObjectForKey:iconURLString];
        [iconURLString release];
    }
}

- (void)_retainFutureIconForSiteURL:(NSURL *)siteURL
{
    ASSERT(siteURL);
    
    NSString *siteURLString = [siteURL absoluteString];
    
    NSNumber *retainCount = [_private->futureSiteURLToRetainCount objectForKey:siteURLString];
    int newRetainCount;

    if(retainCount){
        newRetainCount = [retainCount intValue] + 1;
    }else{
        newRetainCount = 1;
    }

    [_private->futureSiteURLToRetainCount setObject:[NSNumber numberWithInt:newRetainCount] forKey:siteURLString];
}

- (void)_releaseFutureIconForSiteURL:(NSURL *)siteURL
{
    ASSERT(siteURL);
    
    NSString *siteURLString = [siteURL absoluteString];
    
    NSNumber *retainCount = [_private->futureSiteURLToRetainCount objectForKey:siteURLString];

    if(!retainCount){
        [NSException raise:NSGenericException
                    format:@"Releasing a future icon that was not previously retained."];
        return;
    }

    int newRetainCount = [retainCount intValue] - 1;

    if(newRetainCount == 0){
        [_private->futureSiteURLToRetainCount removeObjectForKey:siteURLString];
    }else{
        [_private->futureSiteURLToRetainCount setObject:[NSNumber numberWithInt:newRetainCount] forKey:siteURLString];
    }
}

- (void)_retainOriginalIconsOnDisk
{
    NSEnumerator *enumerator;
    NSString *iconURLString;

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

    NSEnumerator *enumerator = [_private->iconsOnDiskWithURLs objectEnumerator];
    NSString *iconURLString;

    while ((iconURLString = [enumerator nextObject]) != nil) {
        [self _releaseIconForIconURLString:iconURLString];
    }

    _private->didCleanup = YES;
}

- (void)_sendNotificationForSiteURL:(NSURL *)siteURL
{
    ASSERT(siteURL);
    
    NSDictionary *userInfo = [NSDictionary dictionaryWithObject:siteURL
                                                         forKey:WebIconNotificationUserInfoSiteURLKey];
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
    
#if LOG_ENABLED        
    double start = CFAbsoluteTimeGetCurrent();
#endif
    
    [icon setScalesWhenResized:YES];
    [icon setSize:size];
    
#if LOG_ENABLED
    double duration = CFAbsoluteTimeGetCurrent() - start;
    LOG(Timing, "scaling icon took %f seconds.", duration);
#endif
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
