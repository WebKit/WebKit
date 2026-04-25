/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#import "_WKArchiveExclusionRule.h"

#import <wtf/RetainPtr.h>

@implementation _WKArchiveExclusionRule {
    RetainPtr<NSString> _elementLocalName;
    RetainPtr<NSArray<NSString *>> _attributeLocalNames;
    RetainPtr<NSArray<NSString *>> _attributeValues;
}

- (instancetype)initWithElementLocalName:(NSString*)elementLocalName attributeLocalNames:(NSArray<NSString *> *)attributeLocalNames attributeValues:(NSArray<NSString *> *)attributeValues
{
    if (!(self = [super init]))
        return nil;

    if (attributeLocalNames.count != attributeValues.count)
        [NSException raise:NSInvalidArgumentException format:@"attributeLocalNames and attributeValues must have same size"];

    if (!elementLocalName && !attributeLocalNames)
        [NSException raise:NSInvalidArgumentException format:@"elementLocalName and attributeLocalNames cannot both be null"];

    _elementLocalName = elementLocalName;
    _attributeLocalNames = attributeLocalNames;
    _attributeValues = attributeValues;

    return self;
}

- (NSString *)elementLocalName
{
    return _elementLocalName.get();
}

- (NSArray<NSString *> *)attributeLocalNames
{
    return _attributeLocalNames.get();
}

- (NSArray<NSString *> *)attributeValues
{
    return _attributeValues.get();
}


@end
