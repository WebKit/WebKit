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
#import "_WKCustomHeaderFields.h"

#import "_WKCustomHeaderFieldsInternal.h"
#import <wtf/BlockPtr.h>

@implementation _WKCustomHeaderFields

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;
    
    API::Object::constructInWrapper<API::CustomHeaderFields>(self);
    return self;
}

- (void)dealloc
{
    _fields->API::CustomHeaderFields::~CustomHeaderFields();
    [super dealloc];
}

- (NSDictionary<NSString *, NSString *> *)fields
{
    auto& vector = _fields->fields();
    NSMutableDictionary<NSString *, NSString *> *dictionary = [NSMutableDictionary dictionaryWithCapacity:vector.size()];
    for (auto& field : vector)
        [dictionary setObject:field.value() forKey:field.name()];
    return dictionary;
}

- (void)setFields:(NSDictionary<NSString *, NSString *> *)fields
{
    Vector<WebCore::HTTPHeaderField> vector;
    vector.reserveInitialCapacity(fields.count);
    [fields enumerateKeysAndObjectsUsingBlock:makeBlockPtr([&](id key, id value, BOOL* stop) {
        if (auto field = WebCore::HTTPHeaderField::create((NSString *)key, (NSString *)value); field && startsWithLettersIgnoringASCIICase(field->name(), "x-"))
            vector.uncheckedAppend(WTFMove(*field));
    }).get()];
    _fields->setFields(WTFMove(vector));
}

- (NSArray<NSString *> *)thirdPartyDomains
{
    auto& domains = _fields->thirdPartyDomains();
    NSMutableArray *array = [NSMutableArray arrayWithCapacity:domains.size()];
    for (auto& domain : domains)
        [array addObject:domain];
    return array;
}

- (void)setThirdPartyDomains:(NSArray<NSString *> *)thirdPartyDomains
{
    Vector<String> domains;
    domains.reserveInitialCapacity(thirdPartyDomains.count);
    for (NSString *domain in thirdPartyDomains)
        domains.uncheckedAppend(domain);
    _fields->setThirdPartyDomains(WTFMove(domains));
}

- (API::Object&)_apiObject
{
    return *_fields;
}

@end
