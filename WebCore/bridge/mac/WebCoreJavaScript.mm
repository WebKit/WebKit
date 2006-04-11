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

#import "config.h"
#import "WebCoreJavaScript.h"

#import <JavaScriptCore/HashCountedSet.h>
#import <JavaScriptCore/JSLock.h>
#import <JavaScriptCore/collector.h>
#import <JavaScriptCore/interpreter.h>

using KJS::Collector;
using KJS::Interpreter;
using KJS::JSLock;

@implementation WebCoreJavaScript

+ (size_t)objectCount
{
    return Collector::size();
}

+ (size_t)interpreterCount
{
    return Collector::numInterpreters();
}

+ (size_t)protectedObjectCount
{
    return Collector::numProtectedObjects();
}

+ (NSCountedSet *)rootObjectTypeCounts
{
    JSLock lock;
    NSCountedSet* result = [NSCountedSet set];

    HashCountedSet<const char*>* counts = Collector::rootObjectTypeCounts();
    HashCountedSet<const char*>::iterator end = counts->end();
    for (HashCountedSet<const char*>::iterator it = counts->begin(); it != end; ++it)
        for (unsigned i = 0; i < it->second; ++i)
            [result addObject:[NSString stringWithUTF8String:it->first]];

    delete counts;
    return result;
}

+ (void)garbageCollect
{
    JSLock lock;
    while (Collector::collect()) { }
}

+ (BOOL)shouldPrintExceptions
{
    return Interpreter::shouldPrintExceptions();
}

+ (void)setShouldPrintExceptions:(BOOL)print
{
    Interpreter::setShouldPrintExceptions(print);
}

@end
