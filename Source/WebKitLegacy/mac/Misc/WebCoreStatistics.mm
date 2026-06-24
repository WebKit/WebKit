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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#import "DOMElementInternal.h"
#import "WebCache.h"
#import "WebFrameInternal.h"
#import <JavaScriptCore/JSLock.h>
#import <JavaScriptCore/MemoryStatistics.h>
#import <JavaScriptCore/VM.h>
#import <WebCore/BackForwardCache.h>
#import <WebCore/CommonVM.h>
#import <WebCore/FontCache.h>
#import <WebCore/FrameConsoleClient.h>
#import <WebCore/GarbageCollectionController.h>
#import <WebCore/GlyphPage.h>
#import <WebCore/GraphicsContextCG.h>
#import <WebCore/LocalFrame.h>
#import <WebCore/PrintContext.h>
#import <WebCore/RenderTreeAsText.h>
#import <WebCore/RenderView.h>


@implementation WebCoreStatistics

+ (NSArray *)statistics
{
    return [WebCache statistics];
}

+ (size_t)javaScriptObjectsCount
{
    JSC::JSLockHolder lock(WebCore::commonVM());
    return WebCore::commonVM().heap.objectCount();
}

+ (size_t)javaScriptGlobalObjectsCount
{
    JSC::JSLockHolder lock(WebCore::commonVM());
    return WebCore::commonVM().heap.globalObjectCount();
}

+ (size_t)javaScriptProtectedObjectsCount
{
    JSC::JSLockHolder lock(WebCore::commonVM());
    return WebCore::commonVM().heap.protectedObjectCount();
}

+ (size_t)javaScriptProtectedGlobalObjectsCount
{
    JSC::JSLockHolder lock(WebCore::commonVM());
    return WebCore::commonVM().heap.protectedGlobalObjectCount();
}

static RetainPtr<NSCountedSet> createNSCountedSet(const HashCountedSet<ASCIILiteral>& set)
{
    auto result = adoptNS([[NSCountedSet alloc] initWithCapacity:set.size()]);
    for (auto& entry : set) {
        auto key = [NSString stringWithUTF8String:entry.key];
        for (unsigned i = 0; i < entry.value; ++i)
            [result addObject:key];
    }
    return result;
}

+ (NSCountedSet *)javaScriptProtectedObjectTypeCounts
{
    JSC::JSLockHolder lock(WebCore::commonVM());
    return createNSCountedSet(WebCore::commonVM().heap.protectedObjectTypeCounts()).autorelease();
}

+ (NSCountedSet *)javaScriptObjectTypeCounts
{
    JSC::JSLockHolder lock(WebCore::commonVM());
    return createNSCountedSet(WebCore::commonVM().heap.objectTypeCounts()).autorelease();
}

+ (void)garbageCollectJavaScriptObjects
{
    WebCore::GarbageCollectionController::singleton().garbageCollectNow();
}

+ (void)garbageCollectJavaScriptObjectsOnAlternateThreadForDebugging:(BOOL)waitUntilDone
{
    WebCore::GarbageCollectionController::singleton().garbageCollectOnAlternateThreadForDebugging(waitUntilDone);
}

+ (void)setJavaScriptGarbageCollectorTimerEnabled:(BOOL)enable
{
    WebCore::GarbageCollectionController::singleton().setJavaScriptGarbageCollectorTimerEnabled(enable);
}

+ (size_t)iconPageURLMappingCount
{
    return 0;
}

+ (size_t)iconRetainedPageURLCount
{
    return 0;
}

+ (size_t)iconRecordCount
{
    return 0;
}

+ (size_t)iconsWithDataCount
{
    return 0;
}

+ (size_t)cachedFontDataCount
{
    return protect(WebCore::FontCache::forCurrentThread())->fontCount();
}

+ (size_t)cachedFontDataInactiveCount
{
    return protect(WebCore::FontCache::forCurrentThread())->inactiveFontCount();
}

+ (void)purgeInactiveFontData
{
    protect(WebCore::FontCache::forCurrentThread())->purgeInactiveFontData();
}

+ (size_t)glyphPageCount
{
    return WebCore::GlyphPage::count();
}

+ (BOOL)shouldPrintExceptions
{
    JSC::JSLockHolder lock(WebCore::commonVM());
    return WebCore::FrameConsoleClient::shouldPrintExceptions();
}

+ (void)setShouldPrintExceptions:(BOOL)print
{
    JSC::JSLockHolder lock(WebCore::commonVM());
    WebCore::FrameConsoleClient::setShouldPrintExceptions(print);
}

+ (void)emptyCache
{
    [WebCache empty];
}

+ (void)setCacheDisabled:(BOOL)disabled
{
    [WebCache setDisabled:disabled];
}

+ (NSDictionary *)memoryStatistics
{
    auto fastMallocStatistics = WTF::fastMallocStatistics();
    JSC::JSLockHolder lock(WebCore::commonVM());
    size_t heapSize = WebCore::commonVM().heap.size();
    size_t heapFree = WebCore::commonVM().heap.capacity() - heapSize;
    auto globalMemoryStats = JSC::globalMemoryStatistics();
    return @{
        @"FastMallocReservedVMBytes": @(fastMallocStatistics.reservedVMBytes),
        @"FastMallocCommittedVMBytes": @(fastMallocStatistics.committedVMBytes),
        @"FastMallocFreeListBytes": @(fastMallocStatistics.freeListBytes),
        @"JavaScriptHeapSize": @(heapSize),
        @"JavaScriptFreeSize": @(heapFree),
        @"JavaScriptStackSize": @(globalMemoryStats.stackBytes),
        @"JavaScriptJITSize": @(globalMemoryStats.JITBytes),
    };
}

+ (void)returnFreeMemoryToSystem
{
    WTF::releaseFastMallocFreeMemory();
}

+ (int)cachedPageCount
{
    return WebCore::BackForwardCache::singleton().pageCount();
}

+ (int)cachedFrameCount
{
    return WebCore::BackForwardCache::singleton().frameCount();
}

// Deprecated
+ (int)autoreleasedPageCount
{
    return 0;
}

// Deprecated
+ (size_t)javaScriptNoGCAllowedObjectsCount
{
    return 0;
}

+ (size_t)javaScriptReferencedObjectsCount
{
    JSC::JSLockHolder lock(WebCore::commonVM());
    return WebCore::commonVM().heap.protectedObjectCount();
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

- (NSString *)renderTreeAsExternalRepresentationForPrinting
{
    return externalRepresentation(_private->coreFrame, { WebCore::RenderAsTextFlag::PrintingMode }).createNSString().autorelease();
}

static OptionSet<WebCore::RenderAsTextFlag> NODELETE toRenderAsTextFlags(WebRenderTreeAsTextOptions options)
{
    OptionSet<WebCore::RenderAsTextFlag> flags;

    if (options & WebRenderTreeAsTextShowAllLayers)
        flags.add(WebCore::RenderAsTextFlag::ShowAllLayers);
    if (options & WebRenderTreeAsTextShowLayerNesting)
        flags.add(WebCore::RenderAsTextFlag::ShowLayerNesting);
    if (options & WebRenderTreeAsTextShowCompositedLayers)
        flags.add(WebCore::RenderAsTextFlag::ShowCompositedLayers);
    if (options & WebRenderTreeAsTextShowOverflow)
        flags.add(WebCore::RenderAsTextFlag::ShowOverflow);
    if (options & WebRenderTreeAsTextShowSVGGeometry)
        flags.add(WebCore::RenderAsTextFlag::ShowSVGGeometry);
    if (options & WebRenderTreeAsTextShowLayerFragments)
        flags.add(WebCore::RenderAsTextFlag::ShowLayerFragments);

    return flags;
}

- (NSString *)renderTreeAsExternalRepresentationWithOptions:(WebRenderTreeAsTextOptions)options
{
    return externalRepresentation(_private->coreFrame, toRenderAsTextFlags(options)).createNSString().autorelease();
}

- (int)numberOfPagesWithPageWidth:(float)pageWidthInPixels pageHeight:(float)pageHeightInPixels
{
    auto coreFrame = _private->coreFrame;
    if (!coreFrame)
        return -1;

    return WebCore::PrintContext::numberOfPages(*coreFrame, WebCore::FloatSize(pageWidthInPixels, pageHeightInPixels));
}

- (void)printToCGContext:(CGContextRef)cgContext pageWidth:(float)pageWidthInPixels pageHeight:(float)pageHeightInPixels
{
    auto coreFrame = _private->coreFrame;
    if (!coreFrame)
        return;

    WebCore::GraphicsContextCG graphicsContext(cgContext);
    WebCore::PrintContext::spoolAllPagesWithBoundaries(*coreFrame, graphicsContext, WebCore::FloatSize(pageWidthInPixels, pageHeightInPixels));
}

@end
