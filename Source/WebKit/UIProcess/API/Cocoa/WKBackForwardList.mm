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
    AlignedStorage<WebKit::WebBackForwardListWrapper> _list;
}

WK_OBJECT_DISABLE_DISABLE_KVC_IVAR_ACCESS;

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(WKBackForwardList.class, self))
        return;

#if ENABLE(BACK_FORWARD_LIST_SWIFT)
    protect(*_list)->~WebBackForwardListWrapper();
#else
    protect(*_list)->~WebBackForwardList();
#endif

    [super dealloc];
}

- (WKBackForwardListItem *)currentItem
{
    return WebKit::wrapper(protect(protect(*_list)->currentItem()).get());
}

- (WKBackForwardListItem *)backItem
{
    return WebKit::wrapper(protect(protect(*_list)->backItem()).get());
}

- (WKBackForwardListItem *)forwardItem
{
    return WebKit::wrapper(protect(protect(*_list)->forwardItem()).get());
}

- (WKBackForwardListItem *)itemAtIndex:(NSInteger)index
{
    return WebKit::wrapper(protect(protect(*_list)->itemAtIndex(index)).get());
}

- (NSArray *)backList
{
    return WebKit::wrapper(protect(*_list)->backList()).autorelease();
}

- (NSArray *)forwardList
{
    return WebKit::wrapper(protect(*_list)->forwardList()).autorelease();
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
    protect(*_list)->removeAllItems();
}

- (void)_clear
{
    protect(*_list)->clear();
}

- (NSString *)_loggingStringForTesting
{
    return protect(*_list)->loggingString().createNSString().autorelease();
}

@end
