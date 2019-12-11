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

#import "ScriptInputSource.h"

#import <JavaScriptCore/JavaScriptCore.h>
#import <pthread.h>

void scriptInputSourceScheduleRoutine (void *info, CFRunLoopRef runLoop, CFStringRef mode)
{
}

void scriptInputSourcePerformRoutine (void *info)
{
    ScriptInputSource*  source = (__bridge ScriptInputSource *)info;
    [source didReceiveSignal];
}

void scriptInputSourceCancelRoutine (void *info, CFRunLoopRef runLoop, CFStringRef mode)
{
}

@implementation ScriptInputSource {
    CFRunLoopSourceRef m_source;
    CFRunLoopRef m_runLoop;
    NSMutableArray *m_inputQueue;
    pthread_mutex_t m_inputLock;
    
    NSMutableArray *m_outputQueue;
    pthread_mutex_t m_outputLock;
    pthread_cond_t m_outputCondition;
    
    JSContext *m_context;
}

- (id)initWithContext:(JSContext *)context
{
    self = [super init];
    if (!self)
        return nil;
    
    CFRunLoopSourceContext sourceContext = {
        0,
        (__bridge void *)(self),
        NULL, NULL, NULL, NULL, NULL,
        scriptInputSourceScheduleRoutine,
        scriptInputSourceCancelRoutine,
        scriptInputSourcePerformRoutine
    };
    m_source = CFRunLoopSourceCreate(NULL, 0, &sourceContext);
    
    m_inputQueue = [NSMutableArray array];
    m_outputQueue = [NSMutableArray array];
    
    m_context = context;
    pthread_mutex_init(&m_inputLock, NULL);
    pthread_mutex_init(&m_outputLock, NULL);
    pthread_cond_init(&m_outputCondition, NULL);
    
    return self;
}

- (void)dealloc
{
    pthread_mutex_destroy(&m_inputLock);
    pthread_mutex_destroy(&m_outputLock);
    pthread_cond_destroy(&m_outputCondition);
}

- (void)addToCurrentRunLoop
{
    m_runLoop = CFRunLoopGetCurrent();
    CFRunLoopAddSource(m_runLoop, m_source, kCFRunLoopDefaultMode);
}

- (void)removeFromRemoteRunLoop
{
    CFRunLoopRemoveSource(m_runLoop, m_source, kCFRunLoopDefaultMode);
}

- (void)didReceiveScript:(NSString *)script
{
    JSValue *result = [m_context evaluateScript:script];
    
    if (m_context.exception) {
        result = m_context.exception;
        m_context.exception = nil;
    }
    
    pthread_mutex_lock(&m_outputLock);
    [m_outputQueue addObject:result];
    pthread_cond_signal(&m_outputCondition);
    pthread_mutex_unlock(&m_outputLock);
}

- (void)didReceiveSignal
{
    pthread_mutex_lock(&m_inputLock);
    while ([m_inputQueue count]) {
        NSString *nextScript = [m_inputQueue objectAtIndex:0];
        [m_inputQueue removeObjectAtIndex:0];
        pthread_mutex_unlock(&m_inputLock);
        
        [self didReceiveScript:nextScript];
        
        pthread_mutex_lock(&m_inputLock);
    }
    pthread_mutex_unlock(&m_inputLock);
}

- (JSValue *)runScriptRemotely:(NSString *)script
{
    pthread_mutex_lock(&m_inputLock);
    [m_inputQueue addObject:script];
    pthread_mutex_unlock(&m_inputLock);
    
    CFRunLoopSourceSignal(m_source);
    CFRunLoopWakeUp(m_runLoop);
    
    pthread_mutex_lock(&m_outputLock);
    while (![m_outputQueue count])
        pthread_cond_wait(&m_outputCondition, &m_outputLock);
    JSValue *result = [m_outputQueue objectAtIndex:0];
    [m_outputQueue removeObjectAtIndex:0];
    pthread_mutex_unlock(&m_outputLock);
    
    return result;
}

- (void)finishAsyncCallback:(JSValue *)callback withResult:(int)result
{
    pthread_mutex_lock(&m_inputLock);
    [m_inputQueue addObject:^{
        if (result) {
            [callback callWithArguments:@[]];
            return;
        }
        [callback callWithArguments:@[]];
    }];
    pthread_mutex_unlock(&m_inputLock);
    
    CFRunLoopSourceSignal(m_source);
    CFRunLoopWakeUp(m_runLoop);
}

@end