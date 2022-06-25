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
#import "_WKTextManipulationItem.h"

#import "_WKTextManipulationToken.h"
#import <wtf/RetainPtr.h>

NSString * const _WKTextManipulationItemErrorDomain = @"WKTextManipulationItemErrorDomain";
NSString * const _WKTextManipulationItemErrorItemKey = @"item";

@implementation _WKTextManipulationItem {
    RetainPtr<NSString> _identifier;
    RetainPtr<NSArray<_WKTextManipulationToken *>> _tokens;
}

- (instancetype)initWithIdentifier:(NSString *)identifier tokens:(NSArray<_WKTextManipulationToken *> *)tokens
{
    if (!(self = [super init]))
        return nil;

    _identifier = identifier;
    _tokens = tokens;
    return self;
}

- (NSString *)identifier
{
    return _identifier.get();
}

- (NSArray<_WKTextManipulationToken *> *)tokens
{
    return _tokens.get();
}

- (BOOL)isEqual:(id)object
{
    if (self == object)
        return YES;

    if (![object isKindOfClass:[self class]])
        return NO;

    return [self isEqualToTextManipulationItem:object includingContentEquality:YES];
}

- (BOOL)isEqualToTextManipulationItem:(_WKTextManipulationItem *)otherItem includingContentEquality:(BOOL)includingContentEquality
{
    if (!otherItem)
        return NO;

    if (!(self.identifier == otherItem.identifier || [self.identifier isEqualToString:otherItem.identifier]) || self.tokens.count != otherItem.tokens.count)
        return NO;

    __block BOOL tokensAreEqual = YES;
    NSUInteger count = std::min(self.tokens.count, otherItem.tokens.count);
    [self.tokens enumerateObjectsAtIndexes:[NSIndexSet indexSetWithIndexesInRange:NSMakeRange(0, count)] options:0 usingBlock:^(_WKTextManipulationToken *token, NSUInteger index, BOOL* stop) {
        _WKTextManipulationToken *otherToken = otherItem.tokens[index];
        if (![token isEqualToTextManipulationToken:otherToken includingContentEquality:includingContentEquality]) {
            tokensAreEqual = NO;
            *stop = YES;
        }
    }];

    return tokensAreEqual;
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
    NSMutableArray<NSString *> *recursiveDescriptions = [NSMutableArray array];
    [self.tokens enumerateObjectsUsingBlock:^(_WKTextManipulationToken *token, NSUInteger index, BOOL* stop) {
        NSString *description = preservePrivacy ? token.description : token.debugDescription;
        [recursiveDescriptions addObject:description];
    }];
    NSString *tokenDescription = [NSString stringWithFormat:@"[\n\t%@\n]", [recursiveDescriptions componentsJoinedByString:@",\n\t"]];
    NSString *description = [NSString stringWithFormat:@"<%@: %p; identifier = %@ tokens = %@>", self.class, self, self.identifier, tokenDescription];
    return description;
}

@end
