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

#import "ReadEvalPrintLoop.h"

#import <JavaScriptCore/JavaScriptCore.h>
#import <histedit.h>

#import "JSRunLoopThread.h"

@implementation ReadEvalPrintLoop {
    JSRunLoopThread* _jsThread;
    EditLine* _editLine;
    History* _history;
    HistEvent _histEvent;
}

- (id)initWithJSThread:(JSRunLoopThread *)jsThread
{
    self = [super init];
    if (!self)
        return nil;
    
    _jsThread = jsThread;
    
    return self;
}

static const char* prompt(EditLine* editLine)
{
    return ">> ";
}

static NSString *escapeStringForOutput(NSString *input)
{
    return [input stringByReplacingOccurrencesOfString:@"\n" withString:@"\\n"];
}

- (bool)processNextInput
{
    int count;
    char* line = (char *)el_gets(_editLine, &count);
    if (!count)
        return false;
        
    line[count - 1] = 0;
        
    NSString *inputString = [[NSString alloc] initWithCString:line encoding:NSUTF8StringEncoding];
        
    JSValue *result = [_jsThread didReceiveInput:inputString];
    if ([result isString]) {
        printf("=> '%s'\n\n", [escapeStringForOutput([result toString]) UTF8String]);
    } else {
        printf("=> %s\n\n", [[result toString] UTF8String]);
    }
        
    if (count)
        history(_history, &_histEvent, H_ENTER, line);
    
    return true;
}

- (void)run
{    
    _editLine = el_init("node.jsc", stdin, stdout, stderr);
    el_set(_editLine, EL_PROMPT, &prompt);
    
    _history = history_init();
    history(_history, &_histEvent, H_SETSIZE, 1000);
    el_set(_editLine, EL_HIST, history, _history);
    
    while (true) {
        @autoreleasepool {
            if (![self processNextInput])
                break;
        }
    }
        
    history_end(_history);
    el_end(_editLine);
}

@end