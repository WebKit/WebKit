/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#import "_WKHitTestResultInternal.h"

#if PLATFORM(MAC)

@implementation _WKHitTestResult

- (void)dealloc
{
    _hitTestResult->~HitTestResult();

    [super dealloc];
}

- (NSURL *)absoluteImageURL
{
    return [NSURL URLWithString:_hitTestResult->absoluteImageURL()];
}

- (NSURL *)absolutePDFURL
{
    return [NSURL URLWithString:_hitTestResult->absolutePDFURL()];
}

- (NSURL *)absoluteLinkURL
{
    return [NSURL URLWithString:_hitTestResult->absoluteLinkURL()];
}

- (NSURL *)absoluteMediaURL
{
    return [NSURL URLWithString:_hitTestResult->absoluteMediaURL()];
}

- (NSString *)linkLabel
{
    return _hitTestResult->linkLabel();
}

- (NSString *)linkTitle
{
    return _hitTestResult->linkTitle();
}

- (NSString *)lookupText
{
    return _hitTestResult->lookupText();
}

- (BOOL)isContentEditable
{
    return _hitTestResult->isContentEditable();
}

- (CGRect)elementBoundingBox
{
    return _hitTestResult->elementBoundingBox();
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_hitTestResult;
}

@end

#endif
