/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "WebNSObjectExtras.h"

@implementation NSObject (WebNSObjectExtras)

- (void)_webkit_performSelectorWithArguments:(NSMutableDictionary *)arguments
{
    SEL selector = static_cast<SEL>([[arguments objectForKey:@"selector"] pointerValue]);
    @try {
        id object = [arguments objectForKey:@"object"];
        id result = [self performSelector:selector withObject:object];
        if (result)
            [arguments setObject:result forKey:@"result"];
    } @catch(NSException *exception) {
        [arguments setObject:exception forKey:@"exception"];
    }
}

- (id)_webkit_performSelectorOnMainThread:(SEL)selector withObject:(id)object
{
    NSMutableDictionary *arguments = [[NSMutableDictionary alloc] init];
    [arguments setObject:[NSValue valueWithPointer:selector] forKey:@"selector"];
    if (object)
        [arguments setObject:object forKey:@"object"];

    [self performSelectorOnMainThread:@selector(_webkit_performSelectorWithArguments:) withObject:arguments waitUntilDone:TRUE];

    NSException *exception = [[[arguments objectForKey:@"exception"] retain] autorelease];
    id value = [[[arguments objectForKey:@"result"] retain] autorelease];
    [arguments release];

    if (exception)
        [exception raise];
    return value;
}

- (id)_webkit_getPropertyOnMainThread:(SEL)selector
{
    return [self _webkit_performSelectorOnMainThread:selector withObject:nil];
}

@end
