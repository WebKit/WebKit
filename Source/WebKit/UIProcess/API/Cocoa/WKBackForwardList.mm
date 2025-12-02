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

#import "WKBackForwardListItemInternal.h"
#import "WKNSArray.h"
#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/AlignedStorage.h>

@implementation WKBackForwardList {
    AlignedStorage<WebKit::WebBackForwardList> _list;
}

WK_OBJECT_DISABLE_DISABLE_KVC_IVAR_ACCESS;

- (Ref<WebKit::WebBackForwardList>)_protectedList
{
    return *_list;
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(WKBackForwardList.class, self))
        return;

    self._protectedList->~WebBackForwardList();

    [super dealloc];
}

- (WKBackForwardListItem *)currentItem
{
    return WebKit::wrapper(self._protectedList->protectedCurrentItem().get());
}

- (WKBackForwardListItem *)backItem
{
    return WebKit::wrapper(self._protectedList->protectedBackItem().get());
}

- (WKBackForwardListItem *)forwardItem
{
    return WebKit::wrapper(self._protectedList->protectedForwardItem().get());
}

- (WKBackForwardListItem *)itemAtIndex:(NSInteger)index
{
    return WebKit::wrapper(self._protectedList->protectedItemAtIndex(index).get());
}

- (NSArray *)backList
{
    return WebKit::wrapper(self._protectedList->backList()).autorelease();
}

- (NSArray *)forwardList
{
    return WebKit::wrapper(self._protectedList->forwardList()).autorelease();
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
    self._protectedList->removeAllItems();
}

- (void)_clear
{
    self._protectedList->clear();
}

- (NSString *)_loggingStringForTesting
{
    return self._protectedList->loggingString().createNSString().autorelease();
}

@end
