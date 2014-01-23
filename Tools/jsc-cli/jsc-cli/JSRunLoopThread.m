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

#import <JavaScriptCore/JavaScriptCore.h>
#import "JSRunLoopThread.h"
#import "ScriptInputSource.h"
#import <pthread.h>

@implementation JSRunLoopThread {
    NSArray *m_filesToRun;
    CFRunLoopRef m_runLoop;
    ScriptInputSource *m_scriptSource;
    JSContext *m_context;
    dispatch_queue_t m_asyncQueue;
}

static void* jsThreadMain(void* context)
{
    JSRunLoopThread *thread = (__bridge JSRunLoopThread *)(context);
    [thread startRunLoop];
    return 0;
}

+ (ThreadMainType)threadMain
{
    return jsThreadMain;
}

- (id)initWithFiles:(NSArray *)files andContext:(JSContext *)context
{
    self = [super init];
    if (!self)
        return nil;
    
    m_filesToRun = files;
    m_context = context;
    m_scriptSource = [[ScriptInputSource alloc] initWithContext:context];
    m_asyncQueue = dispatch_queue_create("node.jsc async queue", DISPATCH_QUEUE_CONCURRENT);
    
    return self;
}

- (void)startRunLoop
{
    m_runLoop = CFRunLoopGetCurrent();
    [m_scriptSource addToCurrentRunLoop];
    
    [self didFinishRunLoopInitialization];
    
    CFRunLoopRun();
}

- (void)start
{
    [super start];
    
    if (![m_filesToRun count])
        return;
    
    for (NSString *file in m_filesToRun) {
        NSString *script = [NSString stringWithContentsOfFile:file encoding:NSUTF8StringEncoding error:nil];
        [m_scriptSource runScriptRemotely:script];
    }
}

- (void)join
{
    [m_scriptSource removeFromRemoteRunLoop];
    CFRunLoopStop(m_runLoop);
    
    [super join];
}

- (JSValue *)didReceiveInput:(NSString *)input
{
    return [m_scriptSource runScriptRemotely:input];
}

- (void)performCallback:(JSValue *)callback withError:(NSString *)errorMessage
{
}

- (void)performCallback:(JSValue *)callback withArguments:(NSArray *)arguments
{
}

- (void)didFinishRunLoopInitialization
{
    NSMutableDictionary *threadStorage = [[NSThread currentThread] threadDictionary];
    [threadStorage setObject:self forKey:@"currentJSThread"];    
    [super didFinishRunLoopInitialization];
}

@end