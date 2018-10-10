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

#import "Compiler.h"

@import Cocoa;
@import JavaScriptCore;

@interface Compiler ()

@property JSContext* context;

@end

@implementation Compiler

- (instancetype)init
{
    self = [super init];
    if (self) {
        self.context = [[JSContext alloc] init];
        [self loadScripts];
    }
    return self;
}

- (void)loadScripts
{
    __weak JSContext* weakContext = self.context;
    __block NSMutableSet<NSString*>* alreadyLoaded = [NSMutableSet new];
    void (^loadFunction)(NSString *) = ^void(NSString* name)
    {
        if (![alreadyLoaded containsObject:name]) {
            NSString* path = [[NSBundle mainBundle] pathForResource:[name stringByDeletingPathExtension] ofType:@"js"];
            NSError* loadError = nil;
            NSString* src = [NSString stringWithContentsOfFile:path encoding:NSUTF8StringEncoding error:&loadError];
            if (loadError)
                NSLog(@"Loading %@ failed: %@", name, loadError.localizedDescription);
            else {
                [weakContext evaluateScript:src];
                [alreadyLoaded addObject:name];
            }
        } else
            NSLog(@"Attempting to load duplicate file %@", name);
    };
    self.context[@"load"] = loadFunction;

    loadFunction(@"All.js");
    loadFunction(@"MSLCodegenAll.js");
}

- (CompileResult*)compileWhlslToMetal:(NSString *)whlslSource error:(NSError * _Nullable __autoreleasing *)error
{
    JSValue *compileFunction = [self.context objectForKeyedSubscript:@"whlslToMsl"];
    if (compileFunction) {
        JSValue *result = [compileFunction callWithArguments:@[whlslSource]];
        if (result) {
            if ([result hasProperty:@"_error"] && [result hasProperty:@"_metalShaderLanguageSource"] && [result hasProperty:@"_originalFunctionNameToMangledNames"]) {
                if ([[result valueForProperty:@"_error"] isNull]) {
                    NSString *src = [[result valueForProperty:@"_metalShaderLanguageSource"] toString];
                    NSDictionary *map = [[result valueForProperty:@"_originalFunctionNameToMangledNames"] toDictionary];
                    return [[CompileResult alloc] initWithMetalSource:src functionNameMap:map];
                }

                *error = [NSError errorWithDomain:@"WhlslCompiler" code:1 userInfo:@{ NSLocalizedDescriptionKey: [[result valueForProperty:@"_error"] toString]}];
                return nil;
            }

            *error = [NSError errorWithDomain:@"WhlslCompiler" code:2 userInfo:@{ NSLocalizedDescriptionKey: @"Keys not present in result" }];
            return nil;
        }

        *error = [NSError errorWithDomain:@"WhlslCompiler" code:3 userInfo:@{ NSLocalizedDescriptionKey: @"Compilation failed" }];
        return nil;

    }

    *error = [NSError errorWithDomain:@"WhlslCompiler" code:4 userInfo:@{ NSLocalizedDescriptionKey: @"Didn't find compilation function" }];
    return nil;
}

@end
