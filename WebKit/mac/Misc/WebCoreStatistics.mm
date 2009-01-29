/*
 * Copyright (C) 2005, 2006, 2008 Apple Inc. All rights reserved.
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
#import "WebFrameInternal.h"
#import <runtime/JSLock.h>
#import <WebCore/Console.h>
#import <WebCore/FontCache.h>
#import <WebCore/Frame.h>
#import <WebCore/GCController.h>
#import <WebCore/GlyphPageTreeNode.h>
#import <WebCore/IconDatabase.h>
#import <WebCore/JSDOMWindow.h>
#import <WebCore/RenderTreeAsText.h>
#import <WebCore/RenderView.h>

using namespace JSC;
using namespace WebCore;

@implementation WebCoreStatistics

+ (NSArray *)statistics
{
    return [WebCache statistics];
}

+ (size_t)javaScriptObjectsCount
{
    JSLock lock(false);
    return JSDOMWindow::commonJSGlobalData()->heap.objectCount();
}

+ (size_t)javaScriptGlobalObjectsCount
{
    JSLock lock(false);
    return JSDOMWindow::commonJSGlobalData()->heap.globalObjectCount();
}

+ (size_t)javaScriptProtectedObjectsCount
{
    JSLock lock(false);
    return JSDOMWindow::commonJSGlobalData()->heap.protectedObjectCount();
}

+ (size_t)javaScriptProtectedGlobalObjectsCount
{
    JSLock lock(false);
    return JSDOMWindow::commonJSGlobalData()->heap.protectedGlobalObjectCount();
}

+ (NSCountedSet *)javaScriptProtectedObjectTypeCounts
{
    JSLock lock(false);
    
    NSCountedSet *result = [NSCountedSet set];

    OwnPtr<HashCountedSet<const char*> > counts(JSDOMWindow::commonJSGlobalData()->heap.protectedObjectTypeCounts());
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

+ (void)garbageCollectJavaScriptObjectsOnAlternateThreadForDebugging:(BOOL)waitUntilDone
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

+ (size_t)cachedFontDataCount
{
    return fontCache()->fontDataCount();
}

+ (size_t)cachedFontDataInactiveCount
{
    return fontCache()->inactiveFontDataCount();
}

+ (void)purgeInactiveFontData
{
    fontCache()->purgeInactiveFontData();
}

+ (size_t)glyphPageCount
{
    return GlyphPageTreeNode::treeGlyphPageCount();
}

+ (BOOL)shouldPrintExceptions
{
    JSLock lock(false);
    return Console::shouldPrintExceptions();
}

+ (void)setShouldPrintExceptions:(BOOL)print
{
    JSLock lock(false);
    Console::setShouldPrintExceptions(print);
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

+ (void)stopIgnoringWebCoreNodeLeaks
{
    WebCore::Node::stopIgnoringLeaks();
}

+ (NSDictionary *)memoryStatistics
{
    WTF::FastMallocStatistics fastMallocStatistics = WTF::fastMallocStatistics();
    JSLock lock(false);
    Heap::Statistics jsHeapStatistics = JSDOMWindow::commonJSGlobalData()->heap.statistics();
    return [NSDictionary dictionaryWithObjectsAndKeys:
                [NSNumber numberWithInt:fastMallocStatistics.heapSize], @"FastMallocHeapSize",
                [NSNumber numberWithInt:fastMallocStatistics.freeSizeInHeap], @"FastMallocFreeSizeInHeap",
                [NSNumber numberWithInt:fastMallocStatistics.freeSizeInCaches], @"FastMallocFreeSizeInCaches",
                [NSNumber numberWithInt:fastMallocStatistics.returnedSize], @"FastMallocReturnedSize",
                [NSNumber numberWithInt:jsHeapStatistics.size], @"JavaScriptHeapSize",
                [NSNumber numberWithInt:jsHeapStatistics.free], @"JavaScriptFreeSize",
            nil];
}

+ (void)returnFreeMemoryToSystem
{
    WTF::releaseFastMallocFreeMemory();
}

// Deprecated
+ (size_t)javaScriptNoGCAllowedObjectsCount
{
    return 0;
}

+ (size_t)javaScriptReferencedObjectsCount
{
    JSLock lock(false);
    return JSDOMWindow::commonJSGlobalData()->heap.protectedObjectCount();
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
    return externalRepresentation(_private->coreFrame->contentRenderer());
}

@end
