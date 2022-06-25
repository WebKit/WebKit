/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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

#import "WebFeatureInternal.h"

@implementation WebFeature

- (instancetype)initWithKey:(NSString *)key preferenceKey:(NSString *)preferenceKey name:(NSString *)name details:(NSString *)details defaultValue:(BOOL)defaultValue hidden:(BOOL)hidden
{
    if (!(self = [super init]))
        return nil;

    _key = [key copy];
    _preferenceKey = [preferenceKey copy];
    _name = [name copy];
    _details = [details copy];
    _defaultValue = defaultValue;
    _hidden = hidden;
    return self;
}

- (void)dealloc
{
    [_key release];
    [_preferenceKey release];
    [_name release];
    [_details release];
    [super dealloc];
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<%@: %p; name = %@; key = %@; defaultValue = %@>", NSStringFromClass(self.class), self, self.name, self.key, self.defaultValue ? @"on" : @"off"];
}

@end
