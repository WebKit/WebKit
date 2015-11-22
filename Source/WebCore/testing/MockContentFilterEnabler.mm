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
#import "MockContentFilterEnabler.h"

static NSString * const DecisionKey = @"Decision";
static NSString * const DecisionPointKey = @"DecisionPoint";
static NSString * const BlockedStringKey = @"BlockedString";

@implementation WebMockContentFilterEnabler

- (instancetype)initWithDecision:(WebMockContentFilterDecision)decision decisionPoint:(WebMockContentFilterDecisionPoint)decisionPoint blockedString:(NSString *)blockedString
{
    if (!(self = [super init]))
        return nil;

    _decision = decision;
    _decisionPoint = decisionPoint;
    _blockedString = [blockedString copy];
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder
{
    if (!(self = [super init]))
        return nil;

    _decision = static_cast<WebMockContentFilterDecision>([decoder decodeIntForKey:DecisionKey]);
    _decisionPoint = static_cast<WebMockContentFilterDecisionPoint>([decoder decodeIntForKey:DecisionPointKey]);
    _blockedString = [[decoder decodeObjectOfClass:[NSString class] forKey:BlockedStringKey] retain];
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder
{
    [coder encodeInt:static_cast<int>(_decision) forKey:DecisionKey];
    [coder encodeInt:static_cast<int>(_decisionPoint) forKey:DecisionPointKey];
    [coder encodeObject:_blockedString forKey:BlockedStringKey];
}

- (void)enable
{
    if (_enabled)
        return;

    auto& settings = WebCore::MockContentFilterSettings::singleton();
    if (settings.enabled())
        return;

    settings.setEnabled(true);
    settings.setDecision(_decision);
    settings.setDecisionPoint(_decisionPoint);
    settings.setBlockedString(_blockedString);
    _enabled = YES;
}

- (void)dealloc
{
    [_blockedString release];
    if (_enabled)
        WebCore::MockContentFilterSettings::singleton().setEnabled(false);
    [super dealloc];
}

+ (BOOL)supportsSecureCoding
{
    return YES;
}

- (id)copyWithZone:(NSZone *)zone
{
    UNUSED_PARAM(zone);
    return [self retain];
}

@end
