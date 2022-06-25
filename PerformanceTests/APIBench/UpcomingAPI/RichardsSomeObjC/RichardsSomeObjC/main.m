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

#import <Foundation/Foundation.h>
#import <JavaScriptCore/JSLockRefPrivate.h>
#import <JavaScriptCore/JavaScriptCore.h>
#import <QuartzCore/QuartzCore.h>

static unsigned ID_HANDLER_A = 2;
static unsigned ID_HANDLER_B = 3;
static unsigned DATA_SIZE = 4;

@protocol WorkerTaskExports<JSExport>

@property (nonatomic, strong, readonly) JSValue *scheduler;
@property (nonatomic, assign, readonly) int v1;
@property (nonatomic, assign, readonly) int v2;

- (instancetype)initWithScheduler:(JSValue *)scheduler v1:(int)v1 v2:(int)v2;
- (NSObject *)run:(JSValue *)run;
- (NSString *)toString;

@end

@interface WorkerTask : NSObject<WorkerTaskExports>
@end

@implementation WorkerTask {
    JSValue* _scheduler;
    int _v1;
    int _v2;
}

@dynamic scheduler;
@dynamic v1;
@dynamic v2;

- (instancetype)initWithScheduler:(JSValue *)scheduler v1:(int)v1 v2:(int)v2
{
    _scheduler = scheduler;
    _v1 = v1;
    _v2 = v2;
    return self;
}

- (NSString *)toString
{
    return @"WorkerTask";
}

- (NSObject *)run:(JSValue*)packet
{
    if (packet.isNull || packet.isUndefined)
        return [_scheduler invokeMethod:@"suspendCurrent" withArguments:@[]];

    _v1 = ID_HANDLER_A + ID_HANDLER_B - _v1;
    [packet setValue:@(_v1) forProperty:@"id"];
    [packet setValue:@0 forProperty:@"a1"];
    for (unsigned i = 0; i < DATA_SIZE; ++i) {
        _v2 += 1;
        if (_v2 > 26)
            _v2 = 1;
        [[packet valueForProperty:@"a2"] setValue:@(_v2) forProperty:@(i)];
    }
    return [_scheduler invokeMethod:@"queue" withArguments:@[packet]];
}

@end

int main()
{
    JSContext *context = [[JSContext alloc] init];
    if (!context)
        exit(1);
    JSLock(context.JSGlobalContextRef);
    [context setObject:WorkerTask.self forKeyedSubscript:@"WorkerTask"];
    [context setExceptionHandler:^(JSContext *context, JSValue *exception) {
        NSLog(@"%@", exception.toString);
        exit(1);
    }];

    NSString* source = [NSString stringWithContentsOfFile:@"richards.js" encoding:NSUTF8StringEncoding error:nil];
    [context evaluateScript:source];

    unsigned iterations = 50;
    CFTimeInterval start = CACurrentMediaTime();
    for (unsigned i = 1; i <= iterations; ++i)
        [[context objectForKeyedSubscript:@"runRichards"] callWithArguments:@[]];
    CFTimeInterval end = CACurrentMediaTime();
    printf("%d\n", (int)((end-start) * 1000));

    return EXIT_SUCCESS;
}
