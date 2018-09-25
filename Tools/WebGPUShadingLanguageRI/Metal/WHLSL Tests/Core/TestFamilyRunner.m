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

#import "TestFamilyRunner.h"

#import "Compiler.h"
#import "OffscreenRenderer.h"

static NSUInteger executionCount = 0;

@interface TestFamilyRunner ()

@property TestFamily* testFamily;

@end

@implementation TestFamilyRunner

- (instancetype)initWithTestFamily:(TestFamily *)testFamily
{
    if (self = [super init])
        self.testFamily = testFamily;
    return self;
}

+ (id<MTLDevice>)defaultTestingDevice
{
    NSArray<id<MTLDevice>>* allDevices = MTLCopyAllDevices();
    for (id<MTLDevice> device in allDevices) {
        if (device.isLowPower)
            return device;
    }
    return MTLCreateSystemDefaultDevice();
}

+ (OffscreenRenderer*)intOffscreenRenderer
{
    static OffscreenRenderer* offscreenRenderer;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        offscreenRenderer = [[OffscreenRenderer alloc] initWithDevice:[TestFamilyRunner defaultTestingDevice]];
        [offscreenRenderer setOutputPixelFormat:MTLPixelFormatR32Sint];
        [offscreenRenderer setTargetWidth:1 setTargetHeight:1];
    });
    return offscreenRenderer;
}

+ (OffscreenRenderer*)uintOffscreenRenderer
{
    static OffscreenRenderer* offscreenRenderer;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        offscreenRenderer = [[OffscreenRenderer alloc] initWithDevice:[TestFamilyRunner defaultTestingDevice]];
        [offscreenRenderer setOutputPixelFormat:MTLPixelFormatR32Uint];
        [offscreenRenderer setTargetWidth:1 setTargetHeight:1];
    });
    return offscreenRenderer;
}

+ (OffscreenRenderer*)floatOffscreenRenderer
{
    static OffscreenRenderer* offscreenRenderer;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        offscreenRenderer = [[OffscreenRenderer alloc] initWithDevice:[TestFamilyRunner defaultTestingDevice]];
        [offscreenRenderer setOutputPixelFormat:MTLPixelFormatR32Float];
        [offscreenRenderer setTargetWidth:1 setTargetHeight:1];
    });
    return offscreenRenderer;
}

+ (Compiler*)compiler
{
    static Compiler* compiler;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        compiler = [Compiler new];
    });
    return compiler;
}

- (BOOL)executeAllTests
{
    if (!self.testFamily.totalTestCaseCount)
        return YES;

    NSString* whlslSource = self.testFamily.whlslSource;
    NSError* whlslCompileError = nil;
    CompileResult* result = [[TestFamilyRunner compiler] compileWhlslToMetal:whlslSource error:&whlslCompileError];
    if (!result) {
        NSLog(@"[%@]: WHLSL Compile Error: %@", self.testFamily.name, whlslCompileError.localizedDescription);
        return NO;
    }

    NSLog(@"Compiled WHLSL");

    NSMutableString* mslString = [NSMutableString new];
    [mslString appendString:result.source];
    [mslString appendString:@"\n"];
    [mslString appendString:[TestFamily sharedMetalSource]];
    [mslString appendString:[self.testFamily metalSourceForCallingFunctionsWithCompiledNames:result.functionNameMap]];

    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    NSError *error = nil;
    id<MTLLibrary> library = [device newLibraryWithSource:mslString options:nil error:&error];
    if (!library) {
        NSLog(@"[Library]: %@", error.localizedDescription);
        return NO;
    }

    if (error)
        NSLog(@"[Library compiled with warnings]: %@", error.localizedDescription);
    NSLog(@"Compiled metal");

    BOOL status = YES;

    NSString *vertexShaderName = @"vertexShader";

    OffscreenRenderer *intRenderer = [TestFamilyRunner intOffscreenRenderer];
    for (TestDescription* test in self.testFamily.integerTests) {
        [intRenderer setLibrary:library vertexShaderName:vertexShaderName fragmentShaderName:test.fragmentShaderName];
        [intRenderer draw];

        int result = intRenderer.intValue;
        if (result != test.expectation.intValue) {
            NSLog(@"%@ = %d, should be %d", test, result, test.expectation.intValue);
            status = NO;
        } else
            NSLog(@"%@ = %d", test, result);
        executionCount++;
    }

    OffscreenRenderer *uintRenderer = [TestFamilyRunner uintOffscreenRenderer];
    for (TestDescription* test in self.testFamily.unsignedIntegerTests) {
        [uintRenderer setLibrary:library vertexShaderName:vertexShaderName fragmentShaderName:test.fragmentShaderName];
        [uintRenderer draw];

        unsigned result = uintRenderer.uintValue;
        if (result != test.expectation.unsignedIntValue) {
            NSLog(@"%@ = %u, should be %u", test, result, test.expectation.unsignedIntValue);
            status = NO;
        } else
            NSLog(@"%@ = %u", test, result);
        executionCount++;
    }

    OffscreenRenderer *floatRenderer = [TestFamilyRunner floatOffscreenRenderer];
    for (TestDescription* test in self.testFamily.floatTests) {
        [floatRenderer setLibrary:library vertexShaderName:vertexShaderName fragmentShaderName:test.fragmentShaderName];
        [floatRenderer draw];

        float result = floatRenderer.floatValue;
        if (result != test.expectation.floatValue) {
            NSLog(@"%@ = %f, should be %f", test, result, test.expectation.floatValue);
            status = NO;
        } else
            NSLog(@"%@ = %f", test, result);
        executionCount++;
    }

    return status;
}

+ (NSUInteger)executionCount
{
    return executionCount;
}

@end
