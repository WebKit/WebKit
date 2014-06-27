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
#import "WKBackForwardListInternal.h"

#if WK_API_ENABLED

#import "WKBackForwardListItemInternal.h"
#import "WKNSArray.h"

using namespace WebKit;

@implementation WKBackForwardList {
    API::ObjectStorage<WebBackForwardList> _list;
}

- (void)dealloc
{
    _list->~WebBackForwardList();

    [super dealloc];
}

static WKBackForwardListItem *toWKBackForwardListItem(WebBackForwardListItem* item)
{
    if (!item)
        return nil;

    return wrapper(*item);
}

- (WKBackForwardListItem *)currentItem
{
    return toWKBackForwardListItem(_list->currentItem());
}

- (WKBackForwardListItem *)backItem
{
    return toWKBackForwardListItem(_list->backItem());
}

- (WKBackForwardListItem *)forwardItem
{
    return toWKBackForwardListItem(_list->forwardItem());
}

- (WKBackForwardListItem *)itemAtIndex:(NSInteger)index
{
    return toWKBackForwardListItem(_list->itemAtIndex(index));
}

- (NSArray *)backList
{
    RefPtr<API::Array> list = _list->backList();
    if (!list)
        return nil;

    return [wrapper(*list.release().leakRef()) autorelease];
}

- (NSArray *)forwardList
{
    RefPtr<API::Array> list = _list->forwardList();
    if (!list)
        return nil;

    return [wrapper(*list.release().leakRef()) autorelease];
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_list;
}

@end

@implementation WKBackForwardList (WKPrivate)

- (void)_removeAllItems
{
    _list->removeAllItems();
}

- (void)_clear
{
    _list->clear();
}

@end

#endif // WK_API_ENABLED
