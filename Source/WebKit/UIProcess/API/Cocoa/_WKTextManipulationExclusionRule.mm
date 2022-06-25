/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#import "_WKTextManipulationExclusionRule.h"

#import <wtf/RetainPtr.h>

@implementation _WKTextManipulationExclusionRule {
    BOOL _isExclusion;
    RetainPtr<NSString> _elementName;
    RetainPtr<NSString> _attributeName;
    RetainPtr<NSString> _attributeValue;
    RetainPtr<NSString> _className;
}

- (instancetype)initExclusion:(BOOL)exclusion forElement:(NSString *)localName
{
    if (!(self = [super init]))
        return nil;

    _isExclusion = exclusion;
    _elementName = localName;
    
    return self;
}

- (instancetype)initExclusion:(BOOL)exclusion forAttribute:(NSString *)name value:(NSString *)value
{
    if (!(self = [super init]))
        return nil;

    _isExclusion = exclusion;
    _attributeName = name;
    _attributeValue = value;

    return self;
}

- (instancetype)initExclusion:(BOOL)exclusion forClass:(NSString *)className
{
    if (!(self = [super init]))
        return nil;

    _isExclusion = exclusion;
    _className = className;

    return self;
}

- (NSString *)elementName
{
    return _elementName.get();
}

- (NSString *)attributeName
{
    return _attributeName.get();
}

- (NSString *)attributeValue
{
    return _attributeValue.get();
}

- (NSString *)className
{
    return _className.get();
}

@end

