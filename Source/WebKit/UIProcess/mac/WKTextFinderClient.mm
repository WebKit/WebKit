/*
 * Copyright (C) 2015-2022 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WKTextFinderClient.h"

#if PLATFORM(MAC)

#import "APIFindClient.h"
#import "APIFindMatchesClient.h"
#import "WebFindOptions.h"
#import "WebImage.h"
#import "WebPageProxy.h"
#import <WebCore/NativeImage.h>
#import <algorithm>
#import <pal/spi/mac/NSTextFinderSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/CheckedPtr.h>
#import <wtf/Deque.h>
#import <wtf/NakedPtr.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/cocoa/VectorCocoa.h>

// FIXME: Implement scrollFindMatchToVisible.
// FIXME: The NSTextFinder overlay doesn't move with scrolling; we should have a mode where we manage the overlay.

static const NSUInteger maximumFindMatches = 1000;

@interface WKTextFinderClient ()

- (void)didFindStringMatchesWithRects:(const Vector<Vector<WebCore::IntRect>>&)rects didWrapAround:(BOOL)didWrapAround;
- (void)didFindSelectedMatchWithRects:(const Vector<WebCore::IntRect>&)rects didWrapAround:(BOOL)didWrapAround;

- (void)getImageForMatchResult:(id <NSTextFinderAsynchronousDocumentFindMatch>)findMatch completionHandler:(void (^)(NSImage *generatedImage))completionHandler;
- (void)didGetImageForMatchResult:(WebKit::WebImage *)string;

@end

namespace WebKit {
using namespace WebCore;

class TextFinderFindClient : public API::FindMatchesClient, public API::FindClient {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(TextFinderFindClient);
public:
    explicit TextFinderFindClient(WKTextFinderClient *textFinderClient)
        : m_textFinderClient(textFinderClient)
    {
    }

private:
    void didFindStringMatches(WebPageProxy* page, const String&, const Vector<Vector<WebCore::IntRect>>& matchRects, int32_t) override
    {
        [m_textFinderClient didFindStringMatchesWithRects:matchRects didWrapAround:NO];
    }

    void didGetImageForMatchResult(WebPageProxy* page, WebImage* image, int32_t index) override
    {
        [m_textFinderClient didGetImageForMatchResult:image];
    }

    void didFindString(WebPageProxy*, const String&, const Vector<WebCore::IntRect>& matchRects, uint32_t, int32_t, bool didWrapAround) override
    {
        // NSTextFinder asked for one match and only looks at the first one, so return just the match
        // that was found, however many there are.
        [m_textFinderClient didFindSelectedMatchWithRects:matchRects didWrapAround:didWrapAround];
    }

    void didFailToFindString(WebPageProxy*, const String& string) override
    {
        [m_textFinderClient didFindStringMatchesWithRects:{ } didWrapAround:NO];
    }

    const RetainPtr<WKTextFinderClient> m_textFinderClient;
};
    
}

@interface WKTextFinderMatch : NSObject <NSTextFinderAsynchronousDocumentFindMatch>

@property (nonatomic, readonly) unsigned index;
// The match found by stepping to the next or previous match. WebKit has already selected and revealed it,
// and its index doesn't refer to the results of finding all matches.
@property (nonatomic, readonly) BOOL representsSelection;

@end

@implementation WKTextFinderMatch {
    WeakObjCPtr<WKTextFinderClient> _client;
    WeakObjCPtr<NSView> _view;
    RetainPtr<NSArray> _rects;
    unsigned _index;
    BOOL _representsSelection;
}

- (instancetype)initWithClient:(WKTextFinderClient *)client view:(NSView *)view index:(unsigned)index rects:(NSArray *)rects representsSelection:(BOOL)representsSelection
{
    self = [super init];

    if (!self)
        return nil;

    _client = client;
    _view = view;
    _rects = rects;
    _index = index;
    _representsSelection = representsSelection;

    return self;
}

- (NSView *)containingView
{
    // To maintain binary compatibility, this is weakly held even though `containingView` is marked `retain`.
    return _view.getAutoreleased();
}

- (NSArray *)textRects
{
    return _rects.get();
}

- (void)generateTextImage:(void (^)(NSImage *generatedImage))completionHandler
{
    RetainPtr strongClient = _client.get();
    if (!strongClient)
        return completionHandler(nil);

    [strongClient getImageForMatchResult:self completionHandler:completionHandler];
}

- (unsigned)index
{
    return _index;
}

- (BOOL)representsSelection
{
    return _representsSelection;
}

@end

@implementation WKTextFinderClient {
    WeakPtr<WebKit::WebPageProxy> _page;
    WeakObjCPtr<NSView> _view;
    Deque<WTF::Function<void(NSArray *, bool didWrap)>> _findReplyCallbacks;
    Deque<WTF::Function<void(NSImage *)>> _imageReplyCallbacks;
    BOOL _usePlatformFindUI;
}

- (instancetype)initWithPage:(std::reference_wrapper<WebKit::WebPageProxy>)page view:(NSView *)view usePlatformFindUI:(BOOL)usePlatformFindUI
{
    self = [super init];

    if (!self)
        return nil;

    _page = page.get();
    _view = view;
    _usePlatformFindUI = usePlatformFindUI;
    
    Ref protectedPage = page.get();
    protectedPage->setFindMatchesClient(makeUnique<WebKit::TextFinderFindClient>(self));
    protectedPage->setFindClient(makeUnique<WebKit::TextFinderFindClient>(self));

    return self;
}

- (void)willDestroyView:(NSView *)view
{
    RefPtr page = std::exchange(_page, nullptr).get();
    page->setFindMatchesClient(nullptr);
    page->setFindClient(nullptr);

    _view = nil;
}

#pragma mark - NSTextFinderClient SPI

- (void)replaceMatches:(NSArray *)matches withString:(NSString *)replacementText inSelectionOnly:(BOOL)selectionOnly resultCollector:(void (^)(NSUInteger replacementCount))resultCollector
{
    Vector<uint32_t> matchIndices(ReserveCapacityTag { }, matches.count);
    for (id match in matches) {
        // Replacing no matches replaces the selection, which is where a step's match is.
        if ([match isKindOfClass:WKTextFinderMatch.class] && ![(WKTextFinderMatch *)match representsSelection])
            matchIndices.append([(WKTextFinderMatch *)match index]);
    }
    protect(_page)->replaceMatches(WTF::move(matchIndices), replacementText, selectionOnly, [collector = makeBlockPtr(resultCollector)] (uint64_t numberOfReplacements) {
        collector(numberOfReplacements);
    });
}

- (void)findMatchesForString:(NSString *)targetString relativeToMatch:(id <NSTextFinderAsynchronousDocumentFindMatch>)relativeMatch findOptions:(NSTextFinderAsynchronousDocumentFindOptions)findOptions maxResults:(NSUInteger)maxResults resultCollector:(void (^)(NSArray *matches, BOOL didWrap))resultCollector
{
    // Limit the number of results, for performance reasons; NSTextFinder always
    // passes either 1 or NSUIntegerMax, which results in search time being
    // proportional to document length.
    maxResults = std::min(maxResults, maximumFindMatches);

    OptionSet<WebKit::FindOptions> kitFindOptions;

    if (findOptions & NSTextFinderAsynchronousDocumentFindOptionsBackwards)
        kitFindOptions.add(WebKit::FindOptions::Backwards);
    if (findOptions & NSTextFinderAsynchronousDocumentFindOptionsWrap)
        kitFindOptions.add(WebKit::FindOptions::WrapAround);
    if (findOptions & NSTextFinderAsynchronousDocumentFindOptionsCaseInsensitive)
        kitFindOptions.add(WebKit::FindOptions::CaseInsensitive);
    if (findOptions & NSTextFinderAsynchronousDocumentFindOptionsStartsWith)
        kitFindOptions.add(WebKit::FindOptions::AtWordStarts);

    if (!_usePlatformFindUI) {
        kitFindOptions.add(WebKit::FindOptions::ShowOverlay);
        kitFindOptions.add(WebKit::FindOptions::ShowFindIndicator);
        kitFindOptions.add(WebKit::FindOptions::DetermineMatchIndex);
    }

    RetainPtr progress = [NSProgress progressWithTotalUnitCount:1];
    auto copiedResultCollector = Block_copy(resultCollector);
    _findReplyCallbacks.append([progress, copiedResultCollector] (NSArray *matches, bool didWrap) {
        [progress setCompletedUnitCount:1];
        copiedResultCollector(matches, didWrap);
        Block_release(copiedResultCollector);
    });

    // NSTextFinder asks for one match when stepping, but WebKit's find UI still counts and highlights the others.
    if (maxResults == 1)
        protect(_page)->findString(targetString, kitFindOptions, _usePlatformFindUI ? maxResults : maximumFindMatches);
    else
        protect(_page)->findStringMatches(targetString, kitFindOptions, maxResults);
}

- (void)getSelectedText:(void (^)(NSString *selectedTextString))completionHandler
{
    void (^copiedCompletionHandler)(NSString *) = Block_copy(completionHandler);

    protect(_page)->getSelectionOrContentsAsString([copiedCompletionHandler] (const String& string) {
        copiedCompletionHandler(string.createNSString().get());
        Block_release(copiedCompletionHandler);
    });
}

- (void)selectFindMatch:(id <NSTextFinderAsynchronousDocumentFindMatch>)findMatch completionHandler:(void (^)(void))completionHandler
{
    ASSERT([findMatch isKindOfClass:[WKTextFinderMatch class]]);
    ASSERT(!completionHandler);

    WKTextFinderMatch *textFinderMatch = static_cast<WKTextFinderMatch *>(findMatch);
    if (textFinderMatch.representsSelection)
        return;
    protect(_page)->selectFindMatch(textFinderMatch.index);
}

- (void)scrollFindMatchToVisible:(id <NSTextFinderAsynchronousDocumentFindMatch>)findMatch
{
    if (_usePlatformFindUI)
        return;

    ASSERT([findMatch isKindOfClass:[WKTextFinderMatch class]]);

    WKTextFinderMatch *textFinderMatch = static_cast<WKTextFinderMatch *>(findMatch);
    if (textFinderMatch.representsSelection)
        return;
    protect(_page)->indicateFindMatch(textFinderMatch.index);
}

#pragma mark - FindMatchesClient

- (RetainPtr<WKTextFinderMatch>)_matchWithIndex:(unsigned)index rects:(const Vector<WebCore::IntRect>&)rects representsSelection:(BOOL)representsSelection
{
    RetainPtr<NSArray> rectsArray;
    if (_usePlatformFindUI)
        rectsArray = createNSArray(rects);
    else
        rectsArray = @[];
    RetainPtr strongView = _view.get();
    return adoptNS([[WKTextFinderMatch alloc] initWithClient:self view:strongView.get() index:index rects:rectsArray.get() representsSelection:representsSelection]);
}

- (void)didFindStringMatchesWithRects:(const Vector<Vector<WebCore::IntRect>>&)rectsForMatches didWrapAround:(BOOL)didWrapAround
{
    if (_findReplyCallbacks.isEmpty())
        return;

    unsigned index = 0;
    auto matchObjects = createNSArray(rectsForMatches, [&] (auto& rects) {
        return [self _matchWithIndex:index++ rects:rects representsSelection:NO];
    });

    _findReplyCallbacks.takeFirst()(matchObjects.get(), didWrapAround);
}

- (void)didFindSelectedMatchWithRects:(const Vector<WebCore::IntRect>&)rects didWrapAround:(BOOL)didWrapAround
{
    if (_findReplyCallbacks.isEmpty())
        return;

    RetainPtr match = [self _matchWithIndex:0 rects:rects representsSelection:YES];
    _findReplyCallbacks.takeFirst()(@[ match.get() ], didWrapAround);
}

- (void)didGetImageForMatchResult:(WebKit::WebImage *)image
{
    if (_imageReplyCallbacks.isEmpty())
        return;

    auto size = image->size();
    size.scale(1 / _page->deviceScaleFactor());
    
    auto nativeImage = image->copyNativeImage(WebCore::DontCopyBackingStore);
    if (!nativeImage)
        return;

    auto imageCallback = _imageReplyCallbacks.takeFirst();
    imageCallback(adoptNS([[NSImage alloc] initWithCGImage:nativeImage->platformImage().get() size:size]).get());
}

#pragma mark - WKTextFinderMatch callbacks

- (void)getImageForMatchResult:(id <NSTextFinderAsynchronousDocumentFindMatch>)findMatch completionHandler:(void (^)(NSImage *generatedImage))completionHandler
{
    if (!_usePlatformFindUI) {
        completionHandler(nil);
        return;
    }

    ASSERT([findMatch isKindOfClass:[WKTextFinderMatch class]]);

    RetainPtr textFinderMatch = static_cast<WKTextFinderMatch *>(findMatch);

    auto copiedImageCallback = Block_copy(completionHandler);
    _imageReplyCallbacks.append([copiedImageCallback] (NSImage *image) {
        copiedImageCallback(image);
        Block_release(copiedImageCallback);
    });

    // FIXME: There is no guarantee that this will ever result in didGetImageForMatchResult
    // being called (and thus us calling our completion handler); we should harden this
    // against all of the early returns in FindController::getImageForFindMatch.
    // FIXME: A match that represents the selection has no index of its own, so this snapshots the first
    // match from the last find-all, if there was one, instead of the selection.
    protect(_page)->getImageForFindMatch([textFinderMatch index]);
}

@end

#endif // PLATFORM(MAC)
