/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */
#import <JavaScriptCore/WebScriptObject.h>

@implementation WebScriptObject

+ (BOOL)throwException:(NSString *)exceptionMessage
{
    NSLog (@"%s:%d  not yet implemented");
    return NO;
}

- (id)callWebScriptMethod:(NSString *)name withArguments:(NSArray *)args
{
    NSLog (@"%s:%d  not yet implemented");
    return nil;
}

- (id)evaluateWebScript:(NSString *)script
{
    NSLog (@"%s:%d  not yet implemented");
    return nil;
}

- (void)removeWebScriptKey:(NSString *)name;
{
    NSLog (@"%s:%d  not yet implemented");
}

- (NSString *)stringRepresentation
{
    NSLog (@"%s:%d  not yet implemented");
    return nil;
}

- (id)webScriptValueAtIndex:(unsigned int)index;
{
    NSLog (@"%s:%d  not yet implemented");
    return nil;
}

- (void)setWebScriptValueAtIndex:(unsigned int)index value:(id)value;
{
    NSLog (@"%s:%d  not yet implemented");
}

- (void)setException: (NSString *)description;
{
    NSLog (@"%s:%d  not yet implemented");
}

@end



@interface WebScriptObjectPrivate : NSObject
{
}
@end

@implementation WebScriptObjectPrivate
@end



@implementation WebUndefined
+ (WebUndefined *)undefined
{
    NSLog (@"%s:%d  not yet implemented");
    return nil;
}

- (id)initWithCoder:(NSCoder *)coder
{
    NSLog (@"%s:%d  not yet implemented");
    return nil;
}

- (void)encodeWithCoder:(NSCoder *)encoder
{
    NSLog (@"%s:%d  not yet implemented");
}

- (id)copyWithZone:(NSZone *)zone
{
    NSLog (@"%s:%d  not yet implemented");
    return nil;
}

@end
