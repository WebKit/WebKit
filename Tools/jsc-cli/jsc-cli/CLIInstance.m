/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#import "CLIInstance.h"

#import <JavaScriptCore/JavaScriptCore.h>

#import "JSModule.h"
#import "JSRunLoopThread.h"
#import "CLIInstance.h"
#import "ReadEvalPrintLoop.h"

@implementation CLIInstance {
    JSModule *_moduleLoader;
    NSString *_baseFile;
    JSContext *_context;
    
    JSRunLoopThread *_jsThread;
}

- (id)init
{
    self = [super init];
    if (!self)
        return nil;
    
    _baseFile = nil;
    _moduleLoader = [[JSModule alloc] init];
    _context = [[JSContext alloc] init];
    
    _context[@"require"] = ^(NSString *arg) {
        JSModule *module = [JSModule require:arg atPath:[[NSFileManager defaultManager] currentDirectoryPath]];
        if (!module) {
            [[JSContext currentContext] evaluateScript:@"throw 'not found'"];
            return [JSValue valueWithUndefinedInContext:[JSContext currentContext]];
        }
        return module.exports;
    };
    
    _context[@"ARGV"] = [JSValue valueWithNewArrayInContext:_context];
    _jsThread = [[JSRunLoopThread alloc] initWithContext:_context];
    
    return self;
}

- (void)loadFile:(NSString *)path
{
    [_jsThread loadFile:path];
}

- (void)didReceiveArguments:(const char **)args atOffset:(int)offset withLength:(int)length
{
    [_context[@"ARGV"] setValue:(_baseFile ? _baseFile : @"") atIndex:0];
    for (int i = offset; i < length; ++i)
        [_context[@"ARGV"] setValue:@(args[i]) atIndex:(offset - i + 1)];
}

- (void)run
{
    [_jsThread start];
    
    if (!_baseFile) {
        ReadEvalPrintLoop *repl = [[ReadEvalPrintLoop alloc] initWithJSThread:_jsThread];
        [repl run];
    }

    [_jsThread join];
}

@end
