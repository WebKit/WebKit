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
#import "_WKTextManipulationToken.h"

#import <wtf/RetainPtr.h>

NSString * const _WKTextManipulationTokenUserInfoDocumentURLKey = @"_WKTextManipulationTokenUserInfoDocumentURLKey";
NSString * const _WKTextManipulationTokenUserInfoTagNameKey = @"_WKTextManipulationTokenUserInfoTagNameKey";
NSString * const _WKTextManipulationTokenUserInfoRoleAttributeKey = @"_WKTextManipulationTokenUserInfoRoleAttributeKey";
NSString * const _WKTextManipulationTokenUserInfoVisibilityKey = @"_WKTextManipulationTokenUserInfoVisibilityKey";

@implementation _WKTextManipulationToken {
    RetainPtr<NSDictionary<NSString *, id>> _userInfo;
}

- (void)dealloc
{
    [_identifier release];
    _identifier = nil;
    [_content release];
    _content = nil;

    [super dealloc];
}

- (void)setUserInfo:(NSDictionary<NSString *, id> *)userInfo
{
    if (userInfo == _userInfo || [_userInfo isEqual:userInfo])
        return;

    _userInfo = adoptNS(userInfo.copy);
}

- (NSDictionary<NSString *, id> *)userInfo
{
    return _userInfo.get();
}

static BOOL isEqualOrBothNil(id a, id b)
{
    if (a == b)
        return YES;

    return [a isEqual:b];
}

- (BOOL)isEqual:(id)object
{
    if (self == object)
        return YES;

    if (![object isKindOfClass:[self class]])
        return NO;

    return [self isEqualToTextManipulationToken:object includingContentEquality:YES];
}

- (BOOL)isEqualToTextManipulationToken:(_WKTextManipulationToken *)otherToken includingContentEquality:(BOOL)includingContentEquality
{
    if (!otherToken)
        return NO;

    BOOL equalIdentifiers = isEqualOrBothNil(self.identifier, otherToken.identifier);
    BOOL equalExclusion = self.isExcluded == otherToken.isExcluded;
    BOOL equalContent = !includingContentEquality || isEqualOrBothNil(self.content, otherToken.content);
    BOOL equalUserInfo = isEqualOrBothNil(self.userInfo, otherToken.userInfo);

    return equalIdentifiers && equalExclusion && equalContent && equalUserInfo;
}

- (NSString *)description
{
    return [self _descriptionPreservingPrivacy:YES];
}

- (NSString *)debugDescription
{
    return [self _descriptionPreservingPrivacy:NO];
}

- (NSString *)_descriptionPreservingPrivacy:(BOOL)preservePrivacy
{
    NSMutableString *description = [NSMutableString stringWithFormat:@"<%@: %p; identifier = %@; isExcluded = %i", self.class, self, self.identifier, self.isExcluded];
    if (preservePrivacy)
        [description appendFormat:@"; content length = %lu", (unsigned long)self.content.length];
    else
        [description appendFormat:@"; content = %@; user info = %@", self.content, self.userInfo];

    [description appendString:@">"];

    return [[description copy] autorelease];
}

@end
