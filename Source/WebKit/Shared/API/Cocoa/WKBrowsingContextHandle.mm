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
#import "WKBrowsingContextHandleInternal.h"

#import "WebPage.h"
#import "WebPageProxy.h"
#import <wtf/HashFunctions.h>

@implementation WKBrowsingContextHandle

- (id)_initWithPageProxy:(NakedRef<WebKit::WebPageProxy>)page
{
    return [self _initWithPageProxyID:page->identifier() andWebPageID:page->webPageID()];
}

- (id)_initWithPage:(NakedRef<WebKit::WebPage>)page
{
    return [self _initWithPageProxyID:page->webPageProxyIdentifier() andWebPageID:page->identifier()];
}

- (id)_initWithPageProxyID:(WebKit::WebPageProxyIdentifier)pageProxyID andWebPageID:(WebCore::PageIdentifier)webPageID
{
    if (!(self = [super init]))
        return nil;

    _pageProxyID = pageProxyID;
    _webPageID = webPageID.toUInt64();

    return self;
}

- (NSUInteger)hash
{
    return computeHash(_pageProxyID, _webPageID);
}

- (BOOL)isEqual:(id)object
{
    if (![object isKindOfClass:[WKBrowsingContextHandle class]])
        return NO;

    return _pageProxyID == static_cast<WKBrowsingContextHandle *>(object)->_pageProxyID && _webPageID == static_cast<WKBrowsingContextHandle *>(object)->_webPageID;
}

- (void)encodeWithCoder:(NSCoder *)coder
{
    [coder encodeInt64:_pageProxyID.toUInt64() forKey:@"pageProxyID"];
    [coder encodeInt64:_webPageID forKey:@"webPageID"];
}

- (id)initWithCoder:(NSCoder *)coder
{
    return [self _initWithPageProxyID:makeObjectIdentifier<WebKit::WebPageProxyIdentifierType>([coder decodeInt64ForKey:@"pageProxyID"]) andWebPageID:makeObjectIdentifier<WebCore::PageIdentifierType>([coder decodeInt64ForKey:@"webPageID"])];
}

+ (BOOL)supportsSecureCoding
{
    return YES;
}

- (id)copyWithZone:(NSZone *)zone
{
    return [[WKBrowsingContextHandle allocWithZone:zone] _initWithPageProxyID:_pageProxyID andWebPageID:makeObjectIdentifier<WebCore::PageIdentifierType>(_webPageID)];
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<%@: %p; pageProxyID = %llu; webPageID = %llu>", NSStringFromClass(self.class), self, _pageProxyID.toUInt64(), _webPageID];
}
@end
