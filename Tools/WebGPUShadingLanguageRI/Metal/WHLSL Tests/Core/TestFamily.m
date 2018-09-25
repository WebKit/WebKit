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

#import "TestFamily.h"

@import JavaScriptCore;

@interface TestFamily ()

@property NSString* name;
@property NSString* whlslSource;
@property NSUInteger totalTestCaseCount;

@property NSMutableDictionary<NSNumber*, NSMutableArray<TestDescription*>*>* tests;

@end

@implementation TestFamily

- (instancetype)initWithName:(NSString *)name source:(NSString *)source
{
    if (self = [super init]) {
        self.name = name;
        self.whlslSource = source;
        self.tests = [NSMutableDictionary new];
        self.tests[@(WHLSLTypeBool)] = [NSMutableArray new];
        self.tests[@(WHLSLTypeUchar)] = [NSMutableArray new];
        self.tests[@(WHLSLTypeShort)] = [NSMutableArray new];
        self.tests[@(WHLSLTypeUshort)] = [NSMutableArray new];
        self.tests[@(WHLSLTypeUint)] = [NSMutableArray new];
        self.tests[@(WHLSLTypeInt)] = [NSMutableArray new];
        self.tests[@(WHLSLTypeFloat)] = [NSMutableArray new];
        self.tests[@(WHLSLTypeHalf)] = [NSMutableArray new];
    }
    return self;
}

- (void)addTestDescription:(TestDescription *)testDescription
{
    [self.tests[@(testDescription.returnType)] addObject:testDescription];
    testDescription.testFamily = self;
    testDescription.fragmentShaderName = [NSString stringWithFormat:@"fragment_%@_%ld", self.name, self.totalTestCaseCount++];
}

- (NSArray<TestDescription*>*)testsForReturnType:(WHLSLType)type
{
    return self.tests[@(type)];
}

- (NSArray<TestDescription*>*)integerTests
{
    NSMutableArray<TestDescription*>* result = [NSMutableArray new];
    [result addObjectsFromArray:[self testsForReturnType:WHLSLTypeShort]];
    [result addObjectsFromArray:[self testsForReturnType:WHLSLTypeInt]];
    return result;
}

- (NSArray<TestDescription*>*)unsignedIntegerTests
{
    NSMutableArray<TestDescription*>* result = [NSMutableArray new];
    [result addObjectsFromArray:[self testsForReturnType:WHLSLTypeBool]];
    [result addObjectsFromArray:[self testsForReturnType:WHLSLTypeUchar]];
    [result addObjectsFromArray:[self testsForReturnType:WHLSLTypeUshort]];
    [result addObjectsFromArray:[self testsForReturnType:WHLSLTypeUint]];
    return result;
}

- (NSArray<TestDescription*>*)floatTests
{
    NSMutableArray<TestDescription*>* result = [NSMutableArray new];
    [result addObjectsFromArray:[self testsForReturnType:WHLSLTypeHalf]];
    [result addObjectsFromArray:[self testsForReturnType:WHLSLTypeFloat]];
    return result;
}

+ (NSString*)sharedMetalSource
{
    static NSString* result;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        NSString* path = [[NSBundle mainBundle] pathForResource:@"_SharedMetal" ofType:@"txt"];
        result = [NSString stringWithContentsOfFile:path encoding:NSUTF8StringEncoding error:nil];
    });
    return result;
}

- (NSString*)metalSourceForCallingFunctionsWithCompiledNames:(NSDictionary<NSString *, NSString *> *)compiledNames
{
    NSMutableString* source = [NSMutableString new];

    [self.tests enumerateKeysAndObjectsUsingBlock:^(NSNumber * _Nonnull type, NSMutableArray<TestDescription *> * _Nonnull tests, BOOL * _Nonnull stop) {
        for (TestDescription* testDescription in tests)
            [source appendFormat:@"%@\n", [testDescription metalFragmentShaderSourceForCallToMetalFunction:compiledNames[testDescription.functionName]]];
    }];

    return source;
}

+ (NSArray<TestFamily*>*)allTests
{
    JSContext* compilerContext = [JSContext new];
    // The test suite doesn't actually need to load any of its dependencies in order to generate the data needed for test execution.
    compilerContext[@"load"] = ^void(NSString* filePath) { };
    // Some of the tests directly call prepare; but these tests can be ignored because they are testing aspects of the interpreter.
    compilerContext[@"prepare"] = ^id(NSString* filePath, NSNumber* lineNumber, NSString* src, NSNumber* enableInlining) 
    {
        return nil;
    };

    NSString* testFile = [NSString stringWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Test" ofType:@"js"] encoding:NSUTF8StringEncoding error:nil];
    [compilerContext evaluateScript:testFile];

    __block NSString *currentTestName = nil;
    __block BOOL includeTest = YES;

    NSMutableArray<TestFamily*>* testFamilies = [NSMutableArray new];

    // See the bottom of Test.js
    compilerContext[@"runningInCocoaHost"] = [JSValue valueWithBool:YES inContext:compilerContext];

    compilerContext[@"doPrep"] = ^TestFamily*(NSString* code)
    {
        TestFamily* family = [[TestFamily alloc] initWithName:currentTestName source:code];
        [testFamilies addObject:family];
        includeTest = YES;
        return family;
    };

    compilerContext.exceptionHandler = ^(JSContext *context, JSValue *exception) {
        includeTest = NO;
    };
    // FIXME: In line with Test.js, these should raise some kind of error if the value cannot be cast safely.
    // However, the conversion to an NSNumber may have already lost the precision.

    compilerContext[@"makeInt"] = ^TestCallArgument*(TestFamily* program, NSNumber* number)
    {
        return [[TestCallArgument alloc] initWithValue:number type:WHLSLTypeInt];
    };

    compilerContext[@"makeUint"] = ^TestCallArgument*(TestFamily* program, NSNumber* number)
    {
        return [[TestCallArgument alloc] initWithValue:number type:WHLSLTypeUint];
    };

    compilerContext[@"makeUchar"] = ^TestCallArgument*(TestFamily* program, NSNumber* number)
    {
        return [[TestCallArgument alloc] initWithValue:number type:WHLSLTypeUchar];
    };

    compilerContext[@"makeBool"] = ^TestCallArgument*(TestFamily* program, NSNumber* number)
    {
        return [[TestCallArgument alloc] initWithValue:number type:WHLSLTypeBool];
    };

    compilerContext[@"makeFloat"] = ^TestCallArgument*(TestFamily* program, NSNumber* number)
    {
        return [[TestCallArgument alloc] initWithValue:number type:WHLSLTypeFloat];
    };

    // FIXME: This should have the same logic as the implementation in Tests.js
    compilerContext[@"makeCastedFloat"] = ^TestCallArgument*(TestFamily* program, NSNumber* number)
    {
        return [[TestCallArgument alloc] initWithValue:number type:WHLSLTypeFloat];
    };

    compilerContext[@"makeHalf"] = ^TestCallArgument*(TestFamily* program, NSNumber* number)
    {
        return [[TestCallArgument alloc] initWithValue:number type:WHLSLTypeHalf];
    };

    compilerContext[@"callFunction"] = ^TestDescription*(TestFamily* program, NSString* functionName, NSArray* args)
    {
        return [[TestDescription alloc] initWithFunctionName:functionName arguments:args];
    };

    void(^addTest)(TestFamily*, TestDescription*, NSNumber*, WHLSLType) = ^void(TestFamily* program, TestDescription* testDesc, NSNumber* expectation, WHLSLType type)
    {
        if (includeTest) {
            testDesc.returnType = type;
            testDesc.expectation = expectation;
            [program addTestDescription:testDesc];
        }
    };

    compilerContext[@"checkInt"] = ^void(TestFamily* program, TestDescription* testDesc, NSNumber* expectation)
    {
        addTest(program, testDesc, expectation, WHLSLTypeInt);
    };

    compilerContext[@"checkUint"] = ^void(TestFamily* program, TestDescription* testDesc, NSNumber* expectation)
    {
        addTest(program, testDesc, expectation, WHLSLTypeUint);
    };

    compilerContext[@"checkHalf"] = ^void(TestFamily* program, TestDescription* testDesc, NSNumber* expectation)
    {
        addTest(program, testDesc, expectation, WHLSLTypeHalf);
    };

    compilerContext[@"checkFloat"] = ^void(TestFamily* program, TestDescription* testDesc, NSNumber* expectation)
    {
        addTest(program, testDesc, expectation, WHLSLTypeFloat);
    };

    compilerContext[@"checkBool"] = ^void(TestFamily* program, TestDescription* testDesc, NSNumber* expectation)
    {
        addTest(program, testDesc, expectation, WHLSLTypeBool);
    };

    compilerContext[@"checkUchar"] = ^void(TestFamily* program, TestDescription* testDesc, NSNumber* expectation)
    {
        addTest(program, testDesc, expectation, WHLSLTypeUchar);
    };

    // Deliberately not included
    compilerContext[@"checkEnum"] = ^void(TestFamily* program, TestDescription* testDesc, NSNumber* expectation) { };
    compilerContext[@"checkFloat4"] = ^void(TestFamily* program, TestDescription* testDesc, NSNumber* expectation) { };
    compilerContext[@"checkFail"] = ^void(id callback, id errorCallback) { };
    compilerContext[@"checkLexerToken"] = ^void(NSString* token, NSNumber* id, NSString* name, NSString* tokenText) { };

    JSValue* tests = [compilerContext evaluateScript:@"tests"];

    NSArray<NSString*>* testNames = [tests.toDictionary.allKeys sortedArrayUsingSelector:@selector(localizedCaseInsensitiveCompare:)];

    for (currentTestName in testNames) {
        JSValue* func = [tests objectForKeyedSubscript:currentTestName];
        [func callWithArguments:@[]];
    }

    return testFamilies;
}

@end
