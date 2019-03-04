/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import "_WKVisitedLinkStoreInternal.h"

#import "VisitedLinkStore.h"
#import <WebCore/SharedStringHash.h>

@implementation _WKVisitedLinkStore

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    API::Object::constructInWrapper<WebKit::VisitedLinkStore>(self);

    return self;
}

- (void)dealloc
{
    _visitedLinkStore->~VisitedLinkStore();

    [super dealloc];
}

- (void)addVisitedLinkWithURL:(NSURL *)URL
{
    auto linkHash = WebCore::computeSharedStringHash(URL.absoluteString);

    _visitedLinkStore->addVisitedLinkHash(linkHash);
}

- (void)addVisitedLinkWithString:(NSString *)string
{
    _visitedLinkStore->addVisitedLinkHash(WebCore::computeSharedStringHash(string));
}

- (void)removeAll
{
    _visitedLinkStore->removeAll();
}

- (BOOL)containsVisitedLinkWithURL:(NSURL *)URL
{
    auto linkHash = WebCore::computeSharedStringHash(URL.absoluteString);

    return _visitedLinkStore->containsVisitedLinkHash(linkHash);
}

- (void)removeVisitedLinkWithURL:(NSURL *)URL
{
    auto linkHash = WebCore::computeSharedStringHash(URL.absoluteString);

    _visitedLinkStore->removeVisitedLinkHash(linkHash);
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_visitedLinkStore;
}

@end
