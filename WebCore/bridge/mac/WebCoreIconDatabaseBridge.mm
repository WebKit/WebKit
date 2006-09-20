/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"
#import "WebCoreIconDatabaseBridge.h"

#import "Logging.h"
#import "IconDatabase.h"
#import "Image.h"
#import "PlatformString.h"

using namespace WebCore;

@implementation WebCoreIconDatabaseBridge

- (BOOL)openSharedDatabaseWithPath:(NSString *)path
{
    ASSERT(path);
    
    IconDatabase* iconDB = IconDatabase::sharedIconDatabase();
    
    iconDB->open((String([path stringByStandardizingPath])));
    if (!iconDB->isOpen()) {
        [self closeSharedDatabase];
        NSLog(@"Unable to open icon database at %@ - Check your write permissions at that path.  Icon database will be disabled for this browsing session", path);
        return NO;
    }
    return YES;
}

- (void)closeSharedDatabase
{
    IconDatabase::sharedIconDatabase()->close();
}

- (BOOL)isOpen
{
    return IconDatabase::sharedIconDatabase()->isOpen();
}

- (void)removeAllIcons
{
    IconDatabase::sharedIconDatabase()->removeAllIcons();
}

- (BOOL)_isEmpty
{
    return IconDatabase::sharedIconDatabase()->isEmpty();
}

- (BOOL)isIconExpiredForIconURL:(NSString *)iconURL
{
    return IconDatabase::sharedIconDatabase()->isIconExpiredForIconURL(iconURL);
}

- (void)setPrivateBrowsingEnabled:(BOOL)flag
{
    IconDatabase::sharedIconDatabase()->setPrivateBrowsingEnabled(flag);
}

- (BOOL)privateBrowsingEnabled
{
    return IconDatabase::sharedIconDatabase()->getPrivateBrowsingEnabled();
}

- (NSImage *)iconForPageURL:(NSString *)url withSize:(NSSize)size
{
    ASSERT(url);
    ASSERT(size.width);
    ASSERT(size.height);

    // FIXME - We're doing the resize here for now because WebCore::Image doesn't yet support resizing/multiple representations
    // This makes it so there's effectively only one size of a particular icon in the system at a time.  We really need to move this
    // to WebCore::Image and WebCore::IconDatabase asap
    Image* image = IconDatabase::sharedIconDatabase()->iconForPageURL(String(url), IntSize(size));
    if (image) {
        NSImage* nsImage = image->getNSImage();
        if (!nsImage) {
            LOG(IconDatabase, "image didn't return a valid NSImage for URL %@", url);
            return nil;
        }
        LOG(IconDatabase, "Found %i representations in the NSImage", [[nsImage representations] count]);
        
        if (!NSEqualSizes([nsImage size], size)) {
            [nsImage setScalesWhenResized:YES];
            [nsImage setSize:size];
        }
        return nsImage;
    }
    return nil;
}

- (NSString *)iconURLForPageURL:(NSString *)url
{
    ASSERT(url);
    
    String iconURL = IconDatabase::sharedIconDatabase()->iconURLForPageURL(String(url));
    return (NSString*)iconURL;
}

- (NSImage *)defaultIconWithSize:(NSSize)size
{
    ASSERT(size.width);
    ASSERT(size.height);
    
    Image* image = IconDatabase::sharedIconDatabase()->defaultIcon(IntSize(size));
    if (image)
        return image->getNSImage();
    return nil;
}

- (void)retainIconForURL:(NSString *)url
{
    ASSERT(url);
    
    IconDatabase::sharedIconDatabase()->retainIconForPageURL(String(url));
}

- (void)releaseIconForURL:(NSString *)url
{
    ASSERT(url);
    
    IconDatabase::sharedIconDatabase()->releaseIconForPageURL(String(url));
}

- (void)_setIconData:(NSData *)data forIconURL:(NSString *)iconURL
{
    ASSERT(data);
    ASSERT(iconURL);
    
    IconDatabase::sharedIconDatabase()->setIconDataForIconURL([data bytes], [data length], String(iconURL));
}

- (void)_setHaveNoIconForIconURL:(NSString *)iconURL
{
    ASSERT(iconURL);
    
    IconDatabase::sharedIconDatabase()->setHaveNoIconForIconURL(String(iconURL));
}

- (BOOL)_setIconURL:(NSString *)iconURL forPageURL:(NSString *)pageURL
{
    ASSERT(iconURL);
    ASSERT(pageURL);
    
    return IconDatabase::sharedIconDatabase()->setIconURLForPageURL(String(iconURL), String(pageURL));
}

- (BOOL)_hasEntryForIconURL:(NSString *)iconURL
{    
    return IconDatabase::sharedIconDatabase()->hasEntryForIconURL(iconURL);
}

- (NSString *)defaultDatabaseFilename
{
    return IconDatabase::defaultDatabaseFilename();
}

- (void)_setEnabled:(BOOL)enabled
{
    IconDatabase::sharedIconDatabase()->setEnabled(enabled);
}

- (BOOL)_isEnabled
{
    return IconDatabase::sharedIconDatabase()->enabled();
}


@end
