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

namespace WebCore {

void IconDatabase::loadIconFromURL(const String& url)
{
    if (url.isEmpty())
        return;
    [[WebCoreIconDatabaseBridge sharedInstance] loadIconFromURL:(NSString *)url];
}

}

@implementation WebCoreIconDatabaseBridge

- (BOOL)openSharedDatabaseWithPath:(NSString *)path
{
    assert(path);
    
    _iconDB = IconDatabase::sharedIconDatabase();
    if (_iconDB) {
        _iconDB->open((String([path stringByStandardizingPath])));
        if (!_iconDB->isOpen()) {
            [self closeSharedDatabase];
            NSLog(@"Unable to open icon database at %@ - Check your write permissions at that path.  Icon database will be disabled for this browsing session", path);
            return NO;
        }
        return YES;
    }
    return NO;
}

- (void)closeSharedDatabase
{
    delete _iconDB;
    _iconDB = 0;
}

- (BOOL)isOpen
{
    return _iconDB != 0;
}

- (void)removeAllIcons
{
    if (_iconDB)
        _iconDB->removeAllIcons();
}

- (BOOL)_isEmpty
{
    return _iconDB ? _iconDB->isEmpty() : NO;
}

- (BOOL)isIconExpiredForIconURL:(NSString *)iconURL
{
    return _iconDB ? _iconDB->isIconExpiredForIconURL(iconURL) : NO;
}

- (void)setPrivateBrowsingEnabled:(BOOL)flag
{
    if (_iconDB)
        _iconDB->setPrivateBrowsingEnabled(flag);
}

- (BOOL)privateBrowsingEnabled
{
    if (_iconDB)
        return _iconDB->getPrivateBrowsingEnabled();
    return false;
}

- (NSImage *)iconForPageURL:(NSString *)url withSize:(NSSize)size
{
    if (!_iconDB)
        return nil;
        
    ASSERT(url);
    ASSERT(size.width);
    ASSERT(size.height);

    // FIXME - We're doing the resize here for now because WebCore::Image doesn't yet support resizing/multiple representations
    // This makes it so there's effectively only one size of a particular icon in the system at a time.  We really need to move this
    // to WebCore::Image and WebCore::IconDatabase asap
    Image* image = _iconDB->iconForPageURL(String(url), IntSize(size));
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
    if (!_iconDB)
        return nil;
    ASSERT(url);
    
    String iconURL = _iconDB->iconURLForPageURL(String(url));
    return (NSString*)iconURL;
}

- (NSImage *)defaultIconWithSize:(NSSize)size
{
    if (!_iconDB)
        return nil;
    ASSERT(size.width);
    ASSERT(size.height);
    
    Image* image = _iconDB->defaultIcon(IntSize(size));
    if (image)
        return image->getNSImage();
    return nil;
}

- (void)retainIconForURL:(NSString *)url
{
    if (!_iconDB)
        return;
    ASSERT(url);
    _iconDB->retainIconForPageURL(String(url));
}

- (void)releaseIconForURL:(NSString *)url
{
    if (!_iconDB)
        return;
    ASSERT(url);
    _iconDB->releaseIconForPageURL(String(url));
}

- (void)_setIconData:(NSData *)data forIconURL:(NSString *)iconURL
{
    if (!_iconDB)
        return;
    ASSERT(data);
    ASSERT(iconURL);
    
    _iconDB->setIconDataForIconURL([data bytes], [data length], String(iconURL));
}

- (void)_setHaveNoIconForIconURL:(NSString *)iconURL
{
    if (!_iconDB)
        return;
    ASSERT(iconURL);
    
    _iconDB->setHaveNoIconForIconURL(String(iconURL));
}

- (BOOL)_setIconURL:(NSString *)iconURL forPageURL:(NSString *)pageURL
{
    if (!_iconDB)
        return NO;
    ASSERT(iconURL);
    ASSERT(pageURL);
    
    return _iconDB->setIconURLForPageURL(String(iconURL), String(pageURL));
}

- (BOOL)_hasEntryForIconURL:(NSString *)iconURL
{
    if (!_iconDB)
        return NO;
    ASSERT(iconURL);
    
    return _iconDB->hasEntryForIconURL(iconURL);
}

- (NSString *)defaultDatabaseFilename
{
    return IconDatabase::defaultDatabaseFilename();
}

@end
