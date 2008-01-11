/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#import "WebCoreStatistics.h"

#import "WebCache.h"
#import <JavaScriptCore/collector.h>
#import <JavaScriptCore/interpreter.h>
#import <WebCore/GCController.h>
#import <WebCore/IconDatabase.h>
#import <WebCore/Node.h>
#import <WebKit/WebFrameBridge.h>
#import <WebKit/WebFrameInternal.h>

using namespace KJS;
using namespace WebCore;

@implementation WebCoreStatistics

+ (NSArray *)statistics
{
    return [WebCache statistics];
}

+ (size_t)javaScriptObjectsCount
{
    JSLock lock;
    return Collector::size();
}

+ (size_t)javaScriptGlobalObjectsCount
{
    JSLock lock;
    return Collector::globalObjectCount();
}

+ (size_t)javaScriptProtectedObjectsCount
{
    JSLock lock;
    return Collector::protectedObjectCount();
}

+ (size_t)javaScriptProtectedGlobalObjectsCount
{
    JSLock lock;
    return Collector::protectedGlobalObjectCount();
}

+ (NSCountedSet *)javaScriptProtectedObjectTypeCounts
{
    JSLock lock;
    
    NSCountedSet *result = [NSCountedSet set];

    OwnPtr<HashCountedSet<const char*> > counts(Collector::protectedObjectTypeCounts());
    HashCountedSet<const char*>::iterator end = counts->end();
    for (HashCountedSet<const char*>::iterator it = counts->begin(); it != end; ++it)
        for (unsigned i = 0; i < it->second; ++i)
            [result addObject:[NSString stringWithUTF8String:it->first]];
    
    return result;
}

+ (void)garbageCollectJavaScriptObjects
{
    gcController().garbageCollectNow();
}

+ (void)garbageCollectJavaScriptObjectsOnAlternateThreadForDebugging:(BOOL)waitUntilDone;
{
    gcController().garbageCollectOnAlternateThreadForDebugging(waitUntilDone);
}

+ (size_t)iconPageURLMappingCount
{
    return iconDatabase()->pageURLMappingCount();
}

+ (size_t)iconRetainedPageURLCount
{
    return iconDatabase()->retainedPageURLCount();
}

+ (size_t)iconRecordCount
{
    return iconDatabase()->iconRecordCount();
}

+ (size_t)iconsWithDataCount
{
    return iconDatabase()->iconRecordCountWithData();
}

+ (BOOL)shouldPrintExceptions
{
    JSLock lock;
    return Interpreter::shouldPrintExceptions();
}

+ (void)setShouldPrintExceptions:(BOOL)print
{
    JSLock lock;
    Interpreter::setShouldPrintExceptions(print);
}

+ (void)emptyCache
{
    [WebCache empty];
}

+ (void)setCacheDisabled:(BOOL)disabled
{
    [WebCache setDisabled:disabled];
}

+ (void)startIgnoringWebCoreNodeLeaks
{
    WebCore::Node::startIgnoringLeaks();
}

+ (void)stopIgnoringWebCoreNodeLeaks;
{
    WebCore::Node::stopIgnoringLeaks();
}

// Deprecated
+ (size_t)javaScriptNoGCAllowedObjectsCount
{
    return 0;
}

+ (size_t)javaScriptReferencedObjectsCount
{
    JSLock lock;
    return Collector::protectedObjectCount();
}

+ (NSSet *)javaScriptRootObjectClasses
{
    return [self javaScriptRootObjectTypeCounts];
}

+ (size_t)javaScriptInterpretersCount
{
    return [self javaScriptProtectedGlobalObjectsCount];
}

+ (NSCountedSet *)javaScriptRootObjectTypeCounts
{
    return [self javaScriptProtectedObjectTypeCounts];
}

@end

@implementation WebFrame (WebKitDebug)

- (NSString *)renderTreeAsExternalRepresentation
{
    return [[self _bridge] renderTreeAsExternalRepresentation];
}

@end
