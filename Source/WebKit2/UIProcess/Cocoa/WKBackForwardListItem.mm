/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "WKBackForwardListItemInternal.h"

#if WK_API_ENABLED

#import "WebBackForwardListItem.h"
#import "WKRetainPtr.h"
#import "WKSharedAPICast.h"
#import "WKStringCF.h"
#import "WKURLCF.h"

using namespace WebKit;

@implementation WKBackForwardListItem {
    RefPtr<WebBackForwardListItem> _item;
}

- (NSURL *)URL
{
    if (!_item->url())
        return nil;

    return CFBridgingRelease(WKURLCopyCFURL(kCFAllocatorDefault, adoptWK(toCopiedURLAPI(_item->url())).get()));
}

- (NSString *)title
{
    if (!_item->title())
        return nil;

    return CFBridgingRelease(WKStringCopyCFString(kCFAllocatorDefault, adoptWK(toCopiedAPI(_item->title())).get()));
}

- (NSURL *)originalURL
{
    if (!_item->originalURL())
        return nil;

    return CFBridgingRelease(WKURLCopyCFURL(kCFAllocatorDefault, adoptWK(toCopiedURLAPI(_item->originalURL())).get()));
}

#pragma mark - NSObject protocol implementation

- (BOOL)isEqual:(id)object
{
    if ([object class] != [self class])
        return NO;

    return _item == ((WKBackForwardListItem *)object)->_item;
}

- (NSUInteger)hash
{
    return (NSUInteger)_item.get();
}

@end

#pragma mark -

@implementation WKBackForwardListItem (Internal)

- (id)_initWithItem:(WebBackForwardListItem&)item
{
    if (!(self = [super init]))
        return nil;

    _item = &item;

    return self;
}

- (WebKit::WebBackForwardListItem&)_item
{
    return *_item;
}

@end

#endif // WK_API_ENABLED
