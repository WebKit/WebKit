/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#import "_WKTargetedElementRequestInternal.h"

@implementation _WKTargetedElementRequest {
    RetainPtr<NSString> _searchText;
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(_WKTargetedElementRequest.class, self))
        return;
    _request->API::TargetedElementRequest::~TargetedElementRequest();
    [super dealloc];
}

- (API::Object&)_apiObject
{
    return *_request;
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    API::Object::constructInWrapper<API::TargetedElementRequest>(self, API::TargetedElementRequest { });
    return self;
}

- (instancetype)initWithSearchText:(NSString *)searchText
{
    if (!(self = [self init]))
        return nil;

    _request->setSearchText(searchText);
    return self;
}

- (instancetype)initWithPoint:(CGPoint)point
{
    if (!(self = [self init]))
        return nil;

    _request->setPoint(point);
    return self;
}

- (BOOL)canIncludeNearbyElements
{
    return _request->canIncludeNearbyElements();
}

- (void)setCanIncludeNearbyElements:(BOOL)value
{
    _request->setCanIncludeNearbyElements(value);
}

- (BOOL)shouldIgnorePointerEventsNone
{
    return _request->shouldIgnorePointerEventsNone();
}

- (void)setShouldIgnorePointerEventsNone:(BOOL)value
{
    _request->setShouldIgnorePointerEventsNone(value);
}

- (NSString *)searchText
{
    return _request->searchText();
}

- (CGPoint)point
{
    return _request->point();
}

@end
