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

#import "TestDescription.h"

@implementation TestDescription

- (instancetype)initWithFunctionName:(NSString *)functionName arguments:(NSArray<TestCallArgument *> *)arguments
{
    if (self = [super init]) {
        self.functionName = functionName;
        self.arguments = arguments;
    }
    return self;
}

- (NSString*)callString:(NSString*)compiledNameOfFunction
{
    NSMutableString* string = [NSMutableString new];
    [string appendFormat:@"%@(", compiledNameOfFunction];

    BOOL first = YES;
    for (TestCallArgument* argument in self.arguments) {
        if (!first)
            [string appendFormat:@", %@", argument.whlslSource];
        else
            [string appendString:argument.whlslSource];
        first = NO;
    }

    [string appendString:@")"];
    return string;
}

- (NSString*)description
{
    NSMutableString* string = [NSMutableString new];
    [string appendFormat:@"%@ %@(", WHLSLTypeString(self.returnType), self.functionName];
    BOOL first = YES;
    for (TestCallArgument* argument in self.arguments) {
        if (!first)
            [string appendFormat:@", %@ %@", WHLSLTypeString(self.returnType), argument.whlslSource];
        else
            [string appendFormat:@"%@ %@", WHLSLTypeString(self.returnType), argument.whlslSource];
        first = NO;
    }
    [string appendString:@")"];
    return string;
}

- (NSString*)metalFragmentShaderSourceForCallToMetalFunction:(NSString *)compiledNameOfFunction
{
    NSString* returnTypeString;
    switch (self.returnType) {
    case WHLSLTypeBool:
    case WHLSLTypeUchar:
    case WHLSLTypeUshort:
    case WHLSLTypeUint:
        returnTypeString = @"FragmentOutputUint";
        break;
    case WHLSLTypeShort:
    case WHLSLTypeInt:
        returnTypeString = @"FragmentOutputInt";
        break;
    case WHLSLTypeHalf:
    case WHLSLTypeFloat:
        returnTypeString = @"FragmentOutputFloat";
        break;
    }
    NSMutableString* string = [NSMutableString new];
    [string appendFormat:@"fragment %@ %@(VertexOutput stageIn [[stage_in]])\n", returnTypeString, self.fragmentShaderName];
    [string appendString:@"{\n"];
    [string appendFormat:@"    %@ fragmentOutput;\n", returnTypeString];

    NSString * call = [self callString:compiledNameOfFunction];

    switch (self.returnType) {
    case WHLSLTypeBool:
    case WHLSLTypeUchar:
    case WHLSLTypeUshort:
    case WHLSLTypeUint:
        [string appendFormat:@"    fragmentOutput.result = uint32_t(%@);\n", call];
        break;
    case WHLSLTypeShort:
    case WHLSLTypeInt:
        [string appendFormat:@"    fragmentOutput.result = int32_t(%@);\n", call];
        break;
    case WHLSLTypeHalf:
    case WHLSLTypeFloat:
        [string appendFormat:@"    fragmentOutput.result = float(%@);\n", call];
        break;
    }

    [string appendString:@"    return fragmentOutput;\n"];
    [string appendString:@"}\n"];

    return string;
}

@end
