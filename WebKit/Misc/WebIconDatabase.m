/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#import "WebIconDatabaseInternal.h"

#import "WebIconDatabaseDelegate.h"
#import "WebKitLogging.h"
#import "WebKitNSStringExtras.h"
#import "WebNSURLExtras.h"
#import "WebPreferences.h"
#import "WebTypesInternal.h"
#import <WebCore/IconDatabase.h>
#import <WebCore/Image.h>
#import <WebCore/IntSize.h>

using namespace WebCore;

NSString * const WebIconDatabaseVersionKey =    @"WebIconDatabaseVersion";
NSString * const WebURLToIconURLKey =           @"WebSiteURLToIconURLKey";

NSString *WebIconDatabaseDidAddIconNotification =          @"WebIconDatabaseDidAddIconNotification";
NSString *WebIconNotificationUserInfoURLKey =              @"WebIconNotificationUserInfoURLKey";
NSString *WebIconDatabaseDidRemoveAllIconsNotification =   @"WebIconDatabaseDidRemoveAllIconsNotification";

NSString *WebIconDatabaseDirectoryDefaultsKey = @"WebIconDatabaseDirectoryDefaultsKey";
NSString *WebIconDatabaseEnabledDefaultsKey =   @"WebIconDatabaseEnabled";

NSString *WebIconDatabasePath = @"~/Library/Icons";

NSSize WebIconSmallSize = {16, 16};
NSSize WebIconMediumSize = {32, 32};
NSSize WebIconLargeSize = {128, 128};

#define UniqueFilePathSize (34)

@interface WebIconDatabase (WebInternal)
- (BOOL)_isEnabled;
- (void)_setIconURL:(NSString *)iconURL forURL:(NSString *)URL;
- (BOOL)_hasEntryForIconURL:(NSString *)iconURL;
- (void)_sendNotificationForURL:(NSString *)URL;
- (NSImage *)_iconForFileURL:(NSString *)fileURL withSize:(NSSize)size;
- (void)_resetCachedWebPreferences:(NSNotification *)notification;
- (NSImage *)_largestIconFromDictionary:(NSMutableDictionary *)icons;
- (NSMutableDictionary *)_iconsBySplittingRepresentationsOfIcon:(NSImage *)icon;
- (NSImage *)_iconFromDictionary:(NSMutableDictionary *)icons forSize:(NSSize)size cache:(BOOL)cache;
- (void)_scaleIcon:(NSImage *)icon toSize:(NSSize)size;
- (void)_convertToWebCoreFormat; 
@end

@implementation WebIconDatabase

+ (WebIconDatabase *)sharedIconDatabase
{
    static WebIconDatabase *database = nil;
    if (!database)
        database = [[WebIconDatabase alloc] init];
    return database;
}

- init
{
    [super init];
    
    _private = [[WebIconDatabasePrivate alloc] init];
    
    // Check the user defaults and see if the icon database should even be enabled.
    // Inform the bridge and, if we're disabled, bail from init right here
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    // <rdar://problem/4741419> - IconDatabase should be disabled by default
    NSDictionary *initialDefaults = [[NSDictionary alloc] initWithObjectsAndKeys:[NSNumber numberWithBool:YES], WebIconDatabaseEnabledDefaultsKey, nil];
    [defaults registerDefaults:initialDefaults];
    [initialDefaults release];
    BOOL enabled = [defaults boolForKey:WebIconDatabaseEnabledDefaultsKey];
    IconDatabase::sharedIconDatabase()->setEnabled(enabled);
    if (!enabled)
        return self;
    
    // Figure out the directory we should be using for the icon.db
    NSString *databaseDirectory = [defaults objectForKey:WebIconDatabaseDirectoryDefaultsKey];
    if (!databaseDirectory) {
        databaseDirectory = WebIconDatabasePath;
        [defaults setObject:databaseDirectory forKey:WebIconDatabaseDirectoryDefaultsKey];
    }
    databaseDirectory = [[databaseDirectory stringByExpandingTildeInPath] stringByStandardizingPath];
    
    // Open the WebCore icon database and convert the old WebKit icon database if we haven't done the initial conversion yet
    if (!IconDatabase::sharedIconDatabase()->open(databaseDirectory))
        LOG_ERROR("Unable to open icon database");
    else
        if ([self _isEnabled])
            [self _convertToWebCoreFormat];

    IconDatabase::sharedIconDatabase()->setPrivateBrowsingEnabled([[WebPreferences standardPreferences] privateBrowsingEnabled]);
    
    // Register for important notifications
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(_applicationWillTerminate:)
                                                 name:NSApplicationWillTerminateNotification
                                               object:NSApp];
    [[NSNotificationCenter defaultCenter] 
            addObserver:self selector:@selector(_resetCachedWebPreferences:) 
                   name:WebPreferencesChangedNotification object:nil];

    return self;
}

- (NSImage *)iconForURL:(NSString *)URL withSize:(NSSize)size cache:(BOOL)cache
{
    ASSERT(size.width);
    ASSERT(size.height);

    if (!URL || ![self _isEnabled])
        return [self defaultIconForURL:URL withSize:size];

    // FIXME - <rdar://problem/4697934> - Move the handling of FileURLs to WebCore and implement in ObjC++
    if ([URL _webkit_isFileURL])
        return [self _iconForFileURL:URL withSize:size];
      
    if (Image* image = IconDatabase::sharedIconDatabase()->iconForPageURL(URL, IntSize(size)))
        if (NSImage *icon = webGetNSImage(image, size))
            return icon;
    return [self defaultIconForURL:URL withSize:size];
}

- (NSImage *)iconForURL:(NSString *)URL withSize:(NSSize)size
{
    return [self iconForURL:URL withSize:size cache:YES];
}

- (NSString *)iconURLForURL:(NSString *)URL
{
    if (![self _isEnabled])
        return nil;
        
    return IconDatabase::sharedIconDatabase()->iconURLForPageURL(URL);
}

- (NSImage *)defaultIconWithSize:(NSSize)size
{
    ASSERT(size.width);
    ASSERT(size.height);
    
    Image* image = IconDatabase::sharedIconDatabase()->defaultIcon(IntSize(size));
    return image ? image->getNSImage() : nil;
}

- (NSImage *)defaultIconForURL:(NSString *)URL withSize:(NSSize)size
{
    if (_private->delegateImplementsDefaultIconForURL)
        return [_private->delegate webIconDatabase:self defaultIconForURL:URL withSize:size];
    return [self defaultIconWithSize:size];
}

- (void)retainIconForURL:(NSString *)URL
{
    ASSERT(URL);
    if (![self _isEnabled])
        return;

    IconDatabase::sharedIconDatabase()->retainIconForPageURL(URL);
}

- (void)releaseIconForURL:(NSString *)pageURL
{
    ASSERT(pageURL);
    if (![self _isEnabled])
        return;

    IconDatabase::sharedIconDatabase()->releaseIconForPageURL(pageURL);
}

- (void)setDelegate:(id)delegate
{
    _private->delegate = delegate;
    _private->delegateImplementsDefaultIconForURL = [delegate respondsToSelector:@selector(webIconDatabase:defaultIconForURL:withSize:)];
}

- (id)delegate
{
    return _private->delegate;
}

@end


@implementation WebIconDatabase (WebPendingPublic)

- (void)removeAllIcons
{
    if (![self _isEnabled])
        return;
    IconDatabase::sharedIconDatabase()->removeAllIcons();
    // FIXME: This notification won't get sent if WebCore calls removeAllIcons.
    [[NSNotificationCenter defaultCenter] postNotificationName:WebIconDatabaseDidRemoveAllIconsNotification
                                                        object:self
                                                      userInfo:nil];
}

@end

@implementation WebIconDatabase (WebInternal)

- (BOOL)_isEnabled
{
    return IconDatabase::sharedIconDatabase()->enabled();
}

- (void)_setIconURL:(NSString *)iconURL forURL:(NSString *)URL
{
    ASSERT(iconURL);
    ASSERT(URL);
    ASSERT([self _isEnabled]);
    
    // If this iconURL already maps to this pageURL, don't bother sending the notification
    // The WebCore::IconDatabase returns TRUE if we should send the notification, and false if we shouldn't.
    // This is a measurable win on the iBench - about 1% worth on average
    if (IconDatabase::sharedIconDatabase()->setIconURLForPageURL(iconURL, URL))
        // FIXME: This notification won't get set when WebCore sets an icon.
        [self _sendNotificationForURL:URL];
}

- (BOOL)_hasEntryForIconURL:(NSString *)iconURL;
{
    ASSERT([self _isEnabled]);

    return IconDatabase::sharedIconDatabase()->hasEntryForIconURL(iconURL);
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

- (void)_applicationWillTerminate:(NSNotification *)notification
{
    IconDatabase::sharedIconDatabase()->close();
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
    IconDatabase::sharedIconDatabase()->setPrivateBrowsingEnabled(privateBrowsingEnabledNow);
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

// This hashing String->filename algorithm came from WebFileDatabase.m and is what was used in the 
// WebKit Icon Database
static void legacyIconDatabaseFilePathForKey(id key, char *buffer)
{
    const char *s;
    UInt32 hash1;
    UInt32 hash2;
    CFIndex len;
    CFIndex cnt;
    
    s = [[[[key description] lowercaseString] stringByStandardizingPath] UTF8String];
    len = strlen(s);

    // compute first hash    
    hash1 = len;
    for (cnt = 0; cnt < len; cnt++) {
        hash1 += (hash1 << 8) + s[cnt];
    }
    hash1 += (hash1 << (len & 31));

    // compute second hash    
    hash2 = len;
    for (cnt = 0; cnt < len; cnt++) {
        hash2 = (37 * hash2) ^ s[cnt];
    }

#ifdef __LP64__
    snprintf(buffer, UniqueFilePathSize, "%.2u/%.2u/%.10u-%.10u.cache", ((hash1 & 0xff) >> 4), ((hash2 & 0xff) >> 4), hash1, hash2);
#else
    snprintf(buffer, UniqueFilePathSize, "%.2lu/%.2lu/%.10lu-%.10lu.cache", ((hash1 & 0xff) >> 4), ((hash2 & 0xff) >> 4), hash1, hash2);
#endif
}

// This method of getting an object from the filesystem is taken from the old 
// WebKit Icon Database
static id objectFromPathForKey(NSString *databasePath, id key)
{
    ASSERT(key);
    id result = nil;

    // Use the key->filename hashing the old WebKit IconDatabase used
    char uniqueKey[UniqueFilePathSize];    
    legacyIconDatabaseFilePathForKey(key, uniqueKey);
    
    // Get the data from this file and setup for the un-archiving
    NSString *filePath = [[NSString alloc] initWithFormat:@"%@/%s", databasePath, uniqueKey];
    NSData *data = [[NSData alloc] initWithContentsOfFile:filePath];
    NSUnarchiver *unarchiver = nil;
    
    NS_DURING
        if (data) {
            unarchiver = [[NSUnarchiver alloc] initForReadingWithData:data];
            if (unarchiver) {
                id fileKey = [unarchiver decodeObject];
                if ([fileKey isEqual:key]) {
                    id object = [unarchiver decodeObject];
                    if (object) {
                        // Decoded objects go away when the unarchiver does, so we need to
                        // retain this so we can return it to our caller.
                        result = [[object retain] autorelease];
                        LOG(IconDatabase, "read disk cache file - %@", key);
                    }
                }
            }
        }
    NS_HANDLER
        LOG(IconDatabase, "cannot unarchive cache file - %@", key);
        result = nil;
    NS_ENDHANDLER

    [unarchiver release];
    [data release];
    [filePath release];
    
    return result;
}

static NSData* iconDataFromPathForIconURL(NSString *databasePath, NSString *iconURLString)
{
    ASSERT(iconURLString);
    ASSERT(databasePath);
    
    NSData *iconData = objectFromPathForKey(databasePath, iconURLString);
    
    if ((id)iconData == (id)[NSNull null]) 
        return nil;
        
    return iconData;
}

- (void)_convertToWebCoreFormat
{
    ASSERT(_private);    
    
    // If the WebCore Icon Database is not empty, we assume that this conversion has already
    // taken place and skip the rest of the steps 
    if (!IconDatabase::sharedIconDatabase()->isEmpty())
        return;

    // Get the directory the old icon database *should* be in
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSString *databaseDirectory = [defaults objectForKey:WebIconDatabaseDirectoryDefaultsKey];
    if (!databaseDirectory) {
        databaseDirectory = WebIconDatabasePath;
        [defaults setObject:databaseDirectory forKey:WebIconDatabaseDirectoryDefaultsKey];
    }
    databaseDirectory = [databaseDirectory stringByExpandingTildeInPath];

    // With this directory, get the PageURLToIconURL map that was saved to disk
    NSMutableDictionary *pageURLToIconURL = objectFromPathForKey(databaseDirectory, WebURLToIconURLKey);

    // If the retrieved object was not a valid NSMutableDictionary, then we have no valid
    // icons to convert
    if (![pageURLToIconURL isKindOfClass:[NSMutableDictionary class]])
        pageURLToIconURL = nil;
    
    NSEnumerator *enumerator = [pageURLToIconURL keyEnumerator];
    NSString *url, *iconURL;
    
    // First, we'll iterate through the PageURL->IconURL map
    while ((url = [enumerator nextObject]) != nil) {
        iconURL = [pageURLToIconURL objectForKey:url];
        if (!iconURL)
            continue;
        IconDatabase::sharedIconDatabase()->setIconURLForPageURL(iconURL, url);
    }    

    // Second, we'll get a list of the unique IconURLs we have
    NSMutableSet *iconsOnDiskWithURLs = [NSMutableSet setWithArray:[pageURLToIconURL allValues]];
    enumerator = [iconsOnDiskWithURLs objectEnumerator];
    NSData *iconData;
    
    // And iterate through them, adding the icon data to the new icon database
    while ((url = [enumerator nextObject]) != nil) {
        iconData = iconDataFromPathForIconURL(databaseDirectory, url);
        if (iconData)
            IconDatabase::sharedIconDatabase()->setIconDataForIconURL([iconData bytes], [iconData length], url);
        else {
            // This really *shouldn't* happen, so it'd be good to track down why it might happen in a debug build
            // however, we do know how to handle it gracefully in release
            LOG_ERROR("%@ is marked as having an icon on disk, but we couldn't get the data for it", url);
            IconDatabase::sharedIconDatabase()->setHaveNoIconForIconURL(url);
        }
    }

    // After we're done converting old style icons over to webcore icons, we delete the entire directory hierarchy 
    // for the old icon DB (skipping the new iconDB, which will likely be in the same directory)
    NSFileManager *fileManager = [NSFileManager defaultManager];
    enumerator = [[fileManager directoryContentsAtPath:databaseDirectory] objectEnumerator];
    
    NSString *databaseFilename = IconDatabase::sharedIconDatabase()->defaultDatabaseFilename();

    NSString *file;
    while ((file = [enumerator nextObject]) != nil) {
        if ([file isEqualTo:databaseFilename])
            continue;
        NSString *filePath = [databaseDirectory stringByAppendingPathComponent:file];
        if (![fileManager  removeFileAtPath:filePath handler:nil])
            LOG_ERROR("Failed to delete %@ from old icon directory", filePath);
    }
}

@end

@implementation WebIconDatabasePrivate
@end

NSImage *webGetNSImage(Image* image, NSSize size)
{
    ASSERT(size.width);
    ASSERT(size.height);

    // FIXME: We're doing the resize here for now because WebCore::Image doesn't yet support resizing/multiple representations
    // This makes it so there's effectively only one size of a particular icon in the system at a time. We should move this
    // to WebCore::Image at some point.
    if (!image)
        return nil;
    NSImage* nsImage = image->getNSImage();
    if (!nsImage)
        return nil;
    if (!NSEqualSizes([nsImage size], size)) {
        [nsImage setScalesWhenResized:YES];
        [nsImage setSize:size];
    }
    return nsImage;
}
