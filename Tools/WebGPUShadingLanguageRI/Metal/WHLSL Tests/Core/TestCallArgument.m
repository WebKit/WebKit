/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "TestCallArgument.h"

NSString* WHLSLTypeString(WHLSLType type)
{
    switch (type) {
    case WHLSLTypeBool:
        return @"bool";
    case WHLSLTypeUchar:
        return @"uchar";
    case WHLSLTypeShort:
        return @"ushort";
    case WHLSLTypeInt:
        return @"int";
    case WHLSLTypeUshort:
        return @"ushort";
    case WHLSLTypeUint:
        return @"uint";
    case WHLSLTypeHalf:
        return @"half";
    case WHLSLTypeFloat:
        return @"float";
    default:
        return nil;
    }
}

@interface TestCallArgument ()

@property NSNumber* value;
@property WHLSLType type;

@end

@implementation TestCallArgument

- (instancetype)initWithValue:(NSNumber *)value type:(WHLSLType)type
{
    if (self = [super init]) {
        self.value = value;
        self.type = type;
    }
    return self;
}

// FIXME: This should probably be MSL source
- (NSString*)whlslSource
{
    switch (self.type) {
    case WHLSLTypeBool:
        return self.value.boolValue ? @"true" : @"false";
    case WHLSLTypeUchar:
        return [NSString stringWithFormat:@"uint8_t(%uu)", self.value.unsignedIntValue];
    case WHLSLTypeShort:
        return [NSString stringWithFormat:@"int16_t(%d)", self.value.intValue];
    case WHLSLTypeUshort:
        return [NSString stringWithFormat:@"uint16_t(%uu)", self.value.unsignedIntValue];
    case WHLSLTypeInt:
        return [NSString stringWithFormat:@"%d", self.value.intValue];
    case WHLSLTypeUint:
        return [NSString stringWithFormat:@"%uu", self.value.unsignedIntValue];
    case WHLSLTypeHalf:
        return [NSString stringWithFormat:@"%f", self.value.floatValue];
    case WHLSLTypeFloat:
        return [NSString stringWithFormat:@"half(%f)", self.value.floatValue];
    }
}

@end
