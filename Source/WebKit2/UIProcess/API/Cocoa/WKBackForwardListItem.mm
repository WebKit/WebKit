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

#import "WKNSURLExtras.h"

using namespace WebKit;

@implementation WKBackForwardListItem {
    std::aligned_storage<sizeof(WebBackForwardListItem), std::alignment_of<WebBackForwardListItem>::value>::type _item;
}

- (void)dealloc
{
    reinterpret_cast<WebBackForwardListItem*>(&_item)->~WebBackForwardListItem();

    [super dealloc];
}

- (NSURL *)URL
{
    return [NSURL _web_URLWithWTFString:reinterpret_cast<WebBackForwardListItem*>(&_item)->url() relativeToURL:nil];
}

- (NSString *)title
{
    if (!reinterpret_cast<WebBackForwardListItem*>(&_item)->title())
        return nil;

    return reinterpret_cast<WebBackForwardListItem*>(&_item)->title();
}

- (NSURL *)originalURL
{
    return [NSURL _web_URLWithWTFString:reinterpret_cast<WebBackForwardListItem*>(&_item)->originalURL() relativeToURL:nil];
}

#pragma mark WKObject protocol implementation

- (APIObject&)_apiObject
{
    return *reinterpret_cast<APIObject*>(&_item);
}

@end

@implementation WKBackForwardListItem (Internal)

- (WebKit::WebBackForwardListItem&)_item
{
    return *reinterpret_cast<WebBackForwardListItem*>(&_item);
}

@end

#endif // WK_API_ENABLED
