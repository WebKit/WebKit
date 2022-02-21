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
#import <WebCore/BackForwardCache.h>
#import <WebCore/CommonVM.h>
#import <WebCore/FontCache.h>
#import <WebCore/Frame.h>
#import <WebCore/GCController.h>
#import <WebCore/GlyphPage.h>
#import <WebCore/GraphicsContextCG.h>
#import <WebCore/JSDOMWindow.h>
#import <WebCore/PageConsoleClient.h>
#import <WebCore/PrintContext.h>
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
    JSLockHolder lock(commonVM());
    return commonVM().heap.objectCount();
}

+ (size_t)javaScriptGlobalObjectsCount
{
    JSLockHolder lock(commonVM());
    return commonVM().heap.globalObjectCount();
}

+ (size_t)javaScriptProtectedObjectsCount
{
    JSLockHolder lock(commonVM());
    return commonVM().heap.protectedObjectCount();
}

+ (size_t)javaScriptProtectedGlobalObjectsCount
{
    JSLockHolder lock(commonVM());
    return commonVM().heap.protectedGlobalObjectCount();
}

static RetainPtr<NSCountedSet> createNSCountedSet(const HashCountedSet<const char*>& set)
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
    JSLockHolder lock(commonVM());
    return createNSCountedSet(*commonVM().heap.protectedObjectTypeCounts()).autorelease();
}

+ (NSCountedSet *)javaScriptObjectTypeCounts
{
    JSLockHolder lock(commonVM());
    return createNSCountedSet(*commonVM().heap.objectTypeCounts()).autorelease();
}

+ (void)garbageCollectJavaScriptObjects
{
    GCController::singleton().garbageCollectNow();
}

+ (void)garbageCollectJavaScriptObjectsOnAlternateThreadForDebugging:(BOOL)waitUntilDone
{
    GCController::singleton().garbageCollectOnAlternateThreadForDebugging(waitUntilDone);
}

+ (void)setJavaScriptGarbageCollectorTimerEnabled:(BOOL)enable
{
    GCController::singleton().setJavaScriptGarbageCollectorTimerEnabled(enable);
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
    return FontCache::forCurrentThread().fontCount();
}

+ (size_t)cachedFontDataInactiveCount
{
    return FontCache::forCurrentThread().inactiveFontCount();
}

+ (void)purgeInactiveFontData
{
    FontCache::forCurrentThread().purgeInactiveFontData();
}

+ (size_t)glyphPageCount
{
    return GlyphPage::count();
}

+ (BOOL)shouldPrintExceptions
{
    JSLockHolder lock(commonVM());
    return PageConsoleClient::shouldPrintExceptions();
}

+ (void)setShouldPrintExceptions:(BOOL)print
{
    JSLockHolder lock(commonVM());
    PageConsoleClient::setShouldPrintExceptions(print);
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
    auto fastMallocStatistics = WTF::fastMallocStatistics();
    JSLockHolder lock(commonVM());
    size_t heapSize = commonVM().heap.size();
    size_t heapFree = commonVM().heap.capacity() - heapSize;
    auto globalMemoryStats = globalMemoryStatistics();
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
    return BackForwardCache::singleton().pageCount();
}

+ (int)cachedFrameCount
{
    return BackForwardCache::singleton().frameCount();
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
    JSLockHolder lock(commonVM());
    return commonVM().heap.protectedObjectCount();
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
    return externalRepresentation(_private->coreFrame, { RenderAsTextFlag::PrintingMode });
}

static OptionSet<RenderAsTextFlag> toRenderAsTextFlags(WebRenderTreeAsTextOptions options)
{
    OptionSet<RenderAsTextFlag> flags;

    if (options & WebRenderTreeAsTextShowAllLayers)
        flags.add(RenderAsTextFlag::ShowAllLayers);
    if (options & WebRenderTreeAsTextShowLayerNesting)
        flags.add(RenderAsTextFlag::ShowLayerNesting);
    if (options & WebRenderTreeAsTextShowCompositedLayers)
        flags.add(RenderAsTextFlag::ShowCompositedLayers);
    if (options & WebRenderTreeAsTextShowOverflow)
        flags.add(RenderAsTextFlag::ShowOverflow);
    if (options & WebRenderTreeAsTextShowSVGGeometry)
        flags.add(RenderAsTextFlag::ShowSVGGeometry);
    if (options & WebRenderTreeAsTextShowLayerFragments)
        flags.add(RenderAsTextFlag::ShowLayerFragments);

    return flags;
}

- (NSString *)renderTreeAsExternalRepresentationWithOptions:(WebRenderTreeAsTextOptions)options
{
    return externalRepresentation(_private->coreFrame, toRenderAsTextFlags(options));
}

- (int)numberOfPagesWithPageWidth:(float)pageWidthInPixels pageHeight:(float)pageHeightInPixels
{
    Frame* coreFrame = _private->coreFrame;
    if (!coreFrame)
        return -1;

    return PrintContext::numberOfPages(*coreFrame, FloatSize(pageWidthInPixels, pageHeightInPixels));
}

- (void)printToCGContext:(CGContextRef)cgContext pageWidth:(float)pageWidthInPixels pageHeight:(float)pageHeightInPixels
{
    Frame* coreFrame = _private->coreFrame;
    if (!coreFrame)
        return;

    GraphicsContextCG graphicsContext(cgContext);
    PrintContext::spoolAllPagesWithBoundaries(*coreFrame, graphicsContext, FloatSize(pageWidthInPixels, pageHeightInPixels));
}

@end
