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

#include "config.h"
#include "_WKContentWorldConfiguration.h"

@implementation _WKContentWorldConfiguration {
    String _name;
}

WK_OBJECT_DISABLE_DISABLE_KVC_IVAR_ACCESS;

- (NSString *)name
{
    return _name;
}

- (void)setName:(NSString *)name
{
    _name = name;
}

#pragma mark NSCopying protocol implementation

- (id)copyWithZone:(NSZone *)zone
{
    _WKContentWorldConfiguration *clone = [(_WKContentWorldConfiguration *)[[self class] allocWithZone:zone] init];

    clone.name = self.name;
    clone.allowAccessToClosedShadowRoots = self.allowAccessToClosedShadowRoots;
    clone.allowAutofill = self.allowAutofill;
    clone.allowElementUserInfo = self.allowElementUserInfo;
    clone.disableLegacyBuiltinOverrides = self.disableLegacyBuiltinOverrides;

    return clone;
}

#pragma mark NSSecureCoding protocol implementation

+ (BOOL)supportsSecureCoding
{
    return YES;
}

- (void)encodeWithCoder:(NSCoder *)coder
{
    [coder encodeObject:self.name forKey:@"name"];
    [coder encodeBool:self.allowAccessToClosedShadowRoots forKey:@"allowAccessToClosedShadowRoots"];
    [coder encodeBool:self.allowAutofill forKey:@"allowAutofill"];
    [coder encodeBool:self.allowElementUserInfo forKey:@"allowElementUserInfo"];
    [coder encodeBool:self.disableLegacyBuiltinOverrides forKey:@"disableLegacyBuiltinOverrides"];
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    if (!(self = [self init]))
        return nil;

    self.name = [coder decodeObjectOfClass:[NSString class] forKey:@"name"];
    self.allowAccessToClosedShadowRoots = [coder decodeBoolForKey:@"allowAccessToClosedShadowRoots"];
    self.allowAutofill = [coder decodeBoolForKey:@"allowAutofill"];
    self.allowElementUserInfo = [coder decodeBoolForKey:@"allowElementUserInfo"];
    self.disableLegacyBuiltinOverrides = [coder decodeBoolForKey:@"disableLegacyBuiltinOverrides"];

    return self;
}

@end
