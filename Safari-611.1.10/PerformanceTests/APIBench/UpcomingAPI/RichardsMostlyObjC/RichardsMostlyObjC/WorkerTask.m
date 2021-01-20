/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#import "WorkerTask.h"

#import <JavaScriptCore/JSLockRefPrivate.h>
#import <JavaScriptCore/JavaScriptCore.h>

@implementation WorkerTask {
    JSValue *_js;
}

+ (instancetype)createWithScheduler:(Scheduler *)scheduler priority:(Priority)priority queue:(Packet *)queue
{
    WorkerTask *task = [super createWithScheduler:scheduler priority:priority queue:queue];
    task->_js = [self.constructor constructWithArguments:@[scheduler]];
    [scheduler addTask:task];
    return task;
}

+ (JSContext *)context {
    static JSContext* context;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        context = [[JSContext alloc] init];
        JSLock(context.JSGlobalContextRef);
        [context setExceptionHandler:^(JSContext *context, JSValue *exception) {
            NSLog(@"%@", exception.toString);
            exit(2);
        }];
        
        NSString *source = [NSString stringWithContentsOfFile:@"richards.js" encoding:NSUTF8StringEncoding error:nil];
        [context evaluateScript:source];
    });
    return context;
}

+ (JSValue *)constructor {
    static JSValue *constructor;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        constructor = [[self.context objectForKeyedSubscript:@"createWorkerTask"] callWithArguments:@[@(A), @(B), @(WorkPacket.size)]];
    });
    return constructor;
}

- (Task *)run:(Packet *)packet
{
    NSArray* arguments = packet ? @[packet] : @[];
    return [_js invokeMethod:@"run" withArguments:arguments].toObject;
}

@end
