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

#import "WKNSURLExtras.h"
#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/AlignedStorage.h>

@implementation WKBackForwardListItem {
    AlignedStorage<WebKit::WebBackForwardListItem> _item;
}

WK_OBJECT_DISABLE_DISABLE_KVC_IVAR_ACCESS;

- (Ref<WebKit::WebBackForwardListItem>)_protectedItem
{
    return *_item;
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(WKBackForwardListItem.class, self))
        return;

    self._protectedItem->~WebBackForwardListItem();

    [super dealloc];
}

- (NSURL *)URL
{
    return [NSURL _web_URLWithWTFString:self._protectedItem->url()];
}

- (NSString *)title
{
    Ref item = *_item;
    if (!item->title())
        return nil;

    return item->title().createNSString().autorelease();
}

- (NSURL *)initialURL
{
    return [NSURL _web_URLWithWTFString:self._protectedItem->originalURL()];
}

- (WebKit::WebBackForwardListItem&)_item
{
    return *_item;
}

- (CGImageRef)_copySnapshotForTesting CF_RETURNS_RETAINED
{
    if (RefPtr snapshot = _item->snapshot()) {
        // FIXME(rdar://162218496): SaferCPP should notice that our API is a copy.
        SUPPRESS_RETAINPTR_CTOR_ADOPT return snapshot->asImageForTesting().leakRef();
    }
    return nullptr;
}

- (CGPoint)_scrollPosition
{
    Ref item = *_item;
    return CGPointMake(item->mainFrameState()->scrollPosition.x(), item->mainFrameState()->scrollPosition.y());
}

- (BOOL)_wasCreatedByJSWithoutUserInteraction
{
    return self._protectedItem->wasCreatedByJSWithoutUserInteraction();
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_item;
}

@end
