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

#import <WebFoundation/NSString_NSURLExtras.h>
#import <WebFoundation/NSURL_NSURLExtras.h>

NSString * const WebIconDatabaseVersionKey = 	@"WebIconDatabaseVersion";
NSString * const WebIconsOnDiskKey = 		@"WebIconsOnDisk";
NSString * const WebURLToIconURLKey = 		@"WebSiteURLToIconURLKey";
NSString * const WebIconURLToURLsKey = 		@"WebIconURLToSiteURLs";
NSString * const WebHostToURLsKey = 		@"WebHostToSiteURLs";

static const int WebIconDatabaseCurrentVersion = 1;

NSString *WebIconDatabaseDidAddIconNotification = @"WebIconDatabaseDidAddIconNotification";
NSString *WebIconNotificationUserInfoURLKey =     @"WebIconNotificationUserInfoURLKey";

NSString *WebIconDatabaseDirectoryDefaultsKey =   @"WebIconDatabaseDirectoryDefaultsKey";

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
- (NSImage *)_iconForFileURL:(NSString *)fileURL withSize:(NSSize)size;
- (NSMutableDictionary *)_builtInIconsForHost:(NSString *)host;
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
    _private->futureURLToRetainCount = 		[[NSMutableDictionary dictionary] retain];

    _private->iconsToEraseWithURLs = 	[[NSMutableSet set] retain];
    _private->iconsToSaveWithURLs = 	[[NSMutableSet set] retain];
    _private->iconURLsWithNoIcons = 	[[NSMutableSet set] retain];
    
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

- (NSImage *)iconForURL:(NSString *)URL withSize:(NSSize)size cache:(BOOL)cache
{
    ASSERT(size.width);
    ASSERT(size.height);
    
    if (!URL) {
        return [self defaultIconWithSize:size];
    }

    if ([URL _web_isFileURL]) {
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
    
    NSString *iconURLString = [_private->URLToIconURL objectForKey:URL];
    
    if(iconURLString){
        [self _releaseIconForIconURLString:iconURLString];
    }else{
        [self _releaseFutureIconForURL:URL];        
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
    NSString *databaseDirectory = [[NSUserDefaults standardUserDefaults] objectForKey:WebIconDatabaseDirectoryDefaultsKey];

    if (!databaseDirectory) {
        NSString *bundleIdentifier = [[NSBundle mainBundle] bundleIdentifier];
        databaseDirectory = [NSString stringWithFormat:@"%@/Library/Caches/WebKitIcons/%@", NSHomeDirectory(), bundleIdentifier];
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

    if (![_private->URLToIconURL isKindOfClass:[NSMutableDictionary class]]) {
        return NO;
    }
    if (![[_private->URLToIconURL keyEnumerator] _web_isAllStrings]) {
        return NO;
    }
    if (![[_private->URLToIconURL objectEnumerator] _web_isAllStrings]) {
        return NO;
    }

    if (![_private->iconURLToURLs isKindOfClass:[NSMutableDictionary class]]) {
        return NO;
    }
    if (![[_private->iconURLToURLs keyEnumerator] _web_isAllStrings]) {
        return NO;
    }
    e = [_private->iconURLToURLs objectEnumerator];
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

    NSNumber *version = [fileDB objectForKey:WebIconDatabaseVersionKey];
    int v = 0;
    // no version means first version
    if (version == nil) {
	v = 1;
    } else if ([version isKindOfClass:[NSNumber class]]) {
	v = [version intValue];
    }

    if (v == WebIconDatabaseCurrentVersion) {
	_private->iconsOnDiskWithURLs = 	[fileDB objectForKey:WebIconsOnDiskKey];
	_private->URLToIconURL = 		[fileDB objectForKey:WebURLToIconURLKey];
	_private->iconURLToURLs = 	[fileDB objectForKey:WebIconURLToURLsKey];
    }

    if (![self _iconDictionariesAreGood]) {
        _private->iconsOnDiskWithURLs = [NSMutableSet set];
        _private->URLToIconURL = 	[NSMutableDictionary dictionary];
        _private->iconURLToURLs = 	[NSMutableDictionary dictionary];
    }

    [_private->iconsOnDiskWithURLs retain];
    [_private->iconURLToURLs retain];
    [_private->URLToIconURL retain];
}

// Only called by _setIconURL:forKey:
- (void)_updateFileDatabase
{
    if(_private->cleanupCount != 0){
        return;
    }

    WebFileDatabase *fileDB = _private->fileDatabase;

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
        } else if ([_private->iconURLsWithNoIcons containsObject:iconURLString]) {
	    [fileDB setObject:[NSNull null] forKey:iconURLString];
	    [_private->iconsOnDiskWithURLs addObject:iconURLString];
	}
    }
    
    [_private->iconsToEraseWithURLs removeAllObjects];
    [_private->iconsToSaveWithURLs removeAllObjects];

    // Save the icon dictionaries to disk
    [fileDB setObject:_private->iconsOnDiskWithURLs 	forKey:WebIconsOnDiskKey];
    [fileDB setObject:_private->URLToIconURL		forKey:WebURLToIconURLKey];
    [fileDB setObject:_private->iconURLToURLs 		forKey:WebIconURLToURLsKey];
}

- (BOOL)_hasIconForIconURL:(NSString *)iconURL;
{
    return (([_private->iconURLToIcons objectForKey:iconURL] ||
	     [_private->iconURLsWithNoIcons containsObject:iconURL] ||
             [_private->iconsOnDiskWithURLs containsObject:iconURL]) &&
            [_private->iconURLToRetainCount objectForKey:iconURL]);
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
	NSImage *icon = [[NSImage alloc] initWithData:iconData];
	icons = [self _iconsBySplittingRepresentationsOfIcon:icon];
	
	if(icons){
#if !LOG_DISABLED 
	    double duration = CFAbsoluteTimeGetCurrent() - start;
	    LOG(Timing, "loading and creating icon %@ took %f seconds", iconURLString, duration);
#endif
	    [_private->iconURLToIcons setObject:icons forKey:iconURLString];
	}
    }
    
    return icons;
}


- (NSImage *)_iconForFileURL:(NSString *)file withSize:(NSSize)size
{
    ASSERT(size.width);
    ASSERT(size.height);

    NSWorkspace *workspace = [NSWorkspace sharedWorkspace];
    NSString *path = [[NSURL _web_URLWithString:file] path];
    NSString *suffix = [path pathExtension];
    NSImage *icon = nil;
    
    if ([suffix _web_isCaseInsensitiveEqualToString:@"htm"] || [suffix _web_isCaseInsensitiveEqualToString:@"html"]) {
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

- (void)_setIcon:(NSImage *)icon forIconURL:(NSString *)iconURL
{
    ASSERT(icon);
    ASSERT(iconURL);
    
    NSMutableDictionary *icons = [self _iconsBySplittingRepresentationsOfIcon:icon];

    if(!icons){
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
    ASSERT([self _hasIconForIconURL:iconURL]);

    if([[_private->URLToIconURL objectForKey:URL] isEqualToString:iconURL] &&
       [_private->iconsOnDiskWithURLs containsObject:iconURL]){
        // Don't do any work if the icon URL is already bound to the site URL
        return;
    }
    
    [_private->URLToIconURL setObject:iconURL forKey:URL];
    
    NSMutableSet *URLStrings = [_private->iconURLToURLs objectForKey:iconURL];
    if(!URLStrings){
        URLStrings = [NSMutableSet set];
        [_private->iconURLToURLs setObject:URLStrings forKey:iconURL];
    }
    [URLStrings addObject:URL];

    
    NSNumber *futureRetainCount = [_private->futureURLToRetainCount objectForKey:URL];

    if(futureRetainCount){
        NSNumber *retainCount = [_private->iconURLToRetainCount objectForKey:iconURL];
        int newRetainCount = [retainCount intValue] + [futureRetainCount intValue];
        [_private->iconURLToRetainCount setObject:[NSNumber numberWithInt:newRetainCount] forKey:iconURL];
        [_private->futureURLToRetainCount removeObjectForKey:URL];
    }

    [self _sendNotificationForURL:URL];
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

        // Remove negative cache item for icon, if any
	[_private->iconURLsWithNoIcons removeObject:iconURLString];

        // Remove the icon's retain count
        [_private->iconURLToRetainCount removeObjectForKey:iconURLString];

        // Remove the icon's associated site URLs
        [iconURLString retain];
        NSSet *URLStrings = [_private->iconURLToURLs objectForKey:iconURLString];
        [_private->URLToIconURL removeObjectsForKeys:[URLStrings allObjects]];
        [_private->iconURLToURLs removeObjectForKey:iconURLString];
        [iconURLString release];
    }
}

- (void)_retainFutureIconForURL:(NSString *)URL
{
    ASSERT(URL);
    
    NSNumber *retainCount = [_private->futureURLToRetainCount objectForKey:URL];
    int newRetainCount;

    if(retainCount){
        newRetainCount = [retainCount intValue] + 1;
    }else{
        newRetainCount = 1;
    }

    [_private->futureURLToRetainCount setObject:[NSNumber numberWithInt:newRetainCount] forKey:URL];
}

- (void)_releaseFutureIconForURL:(NSString *)URL
{
    ASSERT(URL);
    
    NSNumber *retainCount = [_private->futureURLToRetainCount objectForKey:URL];

    if(!retainCount){
        [NSException raise:NSGenericException
                    format:@"Releasing a future icon that was not previously retained."];
        return;
    }

    int newRetainCount = [retainCount intValue] - 1;

    if(newRetainCount == 0){
        [_private->futureURLToRetainCount removeObjectForKey:URL];
    }else{
        [_private->futureURLToRetainCount setObject:[NSNumber numberWithInt:newRetainCount] forKey:URL];
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
