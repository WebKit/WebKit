/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#import "WebImage.h"
#import "WebPageProxy.h"
#import <algorithm>
#import <pal/spi/mac/NSTextFinderSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/Deque.h>
#import <wtf/NakedPtr.h>
#import <wtf/cocoa/VectorCocoa.h>

// FIXME: Implement scrollFindMatchToVisible.
// FIXME: The NSTextFinder overlay doesn't move with scrolling; we should have a mode where we manage the overlay.

static const NSUInteger maximumFindMatches = 1000;

@interface WKTextFinderClient ()

- (void)didFindStringMatchesWithRects:(const Vector<Vector<WebCore::IntRect>>&)rects didWrapAround:(BOOL)didWrapAround;

- (void)getImageForMatchResult:(id <NSTextFinderAsynchronousDocumentFindMatch>)findMatch completionHandler:(void (^)(NSImage *generatedImage))completionHandler;
- (void)didGetImageForMatchResult:(WebKit::WebImage *)string;

@end

namespace WebKit {
using namespace WebCore;

class TextFinderFindClient : public API::FindMatchesClient, public API::FindClient {
    WTF_MAKE_FAST_ALLOCATED;
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

    void didFindString(WebPageProxy*, const String&, const Vector<WebCore::IntRect>& matchRects, uint32_t matchCount, int32_t matchIndex, bool didWrapAround) override
    {
        Vector<Vector<WebCore::IntRect>> allMatches;
        if (matchCount != static_cast<unsigned>(kWKMoreThanMaximumMatchCount)) {
            // Synthesize a vector of match rects for all `matchCount` matches,
            // filling in the actual rects for the one that we received.
            // The rest will remain empty, but it's important to NSTextFinder
            // that they at least exist.
            allMatches.resize(matchCount);
            // FIXME: Clean this up and figure out why we are getting a -1 index
            if (matchIndex >= 0 && static_cast<uint32_t>(matchIndex) < matchCount)
                allMatches[matchIndex].appendVector(matchRects);
        }

        [m_textFinderClient didFindStringMatchesWithRects:allMatches didWrapAround:didWrapAround];
    }

    void didFailToFindString(WebPageProxy*, const String& string) override
    {
        [m_textFinderClient didFindStringMatchesWithRects:{ } didWrapAround:NO];
    }

    RetainPtr<WKTextFinderClient> m_textFinderClient;
};
    
}

@interface WKTextFinderMatch : NSObject <NSTextFinderAsynchronousDocumentFindMatch>

@property (nonatomic, readonly) unsigned index;

@end

@implementation WKTextFinderMatch {
    WKTextFinderClient *_client;
    NSView *_view;
    RetainPtr<NSArray> _rects;
    unsigned _index;
}

- (instancetype)initWithClient:(WKTextFinderClient *)client view:(NSView *)view index:(unsigned)index rects:(NSArray *)rects
{
    self = [super init];

    if (!self)
        return nil;

    _client = client;
    _view = view;
    _rects = rects;
    _index = index;

    return self;
}

- (NSView *)containingView
{
    return _view;
}

- (NSArray *)textRects
{
    return _rects.get();
}

- (void)generateTextImage:(void (^)(NSImage *generatedImage))completionHandler
{
    [_client getImageForMatchResult:self completionHandler:completionHandler];
}

- (unsigned)index
{
    return _index;
}

@end

@implementation WKTextFinderClient {
    NakedPtr<WebKit::WebPageProxy> _page;
    NSView *_view;
    Deque<WTF::Function<void(NSArray *, bool didWrap)>> _findReplyCallbacks;
    Deque<WTF::Function<void(NSImage *)>> _imageReplyCallbacks;
    BOOL _usePlatformFindUI;
}

- (instancetype)initWithPage:(NakedRef<WebKit::WebPageProxy>)page view:(NSView *)view usePlatformFindUI:(BOOL)usePlatformFindUI
{
    self = [super init];

    if (!self)
        return nil;

    _page = page.ptr();
    _view = view;
    _usePlatformFindUI = usePlatformFindUI;
    
    _page->setFindMatchesClient(makeUnique<WebKit::TextFinderFindClient>(self));
    _page->setFindClient(makeUnique<WebKit::TextFinderFindClient>(self));

    return self;
}

- (void)willDestroyView:(NSView *)view
{
    _page->setFindMatchesClient(nullptr);
    _page->setFindClient(nullptr);

    _page = nullptr;
    _view = nil;
}

#pragma mark - NSTextFinderClient SPI

- (void)replaceMatches:(NSArray *)matches withString:(NSString *)replacementText inSelectionOnly:(BOOL)selectionOnly resultCollector:(void (^)(NSUInteger replacementCount))resultCollector
{
    Vector<uint32_t> matchIndices;
    matchIndices.reserveCapacity(matches.count);
    for (id match in matches) {
        if ([match isKindOfClass:WKTextFinderMatch.class])
            matchIndices.uncheckedAppend([(WKTextFinderMatch *)match index]);
    }
    _page->replaceMatches(WTFMove(matchIndices), replacementText, selectionOnly, [collector = makeBlockPtr(resultCollector)] (uint64_t numberOfReplacements, auto error) {
        collector(error == WebKit::CallbackBase::Error::None ? numberOfReplacements : 0);
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

    RetainPtr<NSProgress> progress = [NSProgress progressWithTotalUnitCount:1];
    auto copiedResultCollector = Block_copy(resultCollector);
    _findReplyCallbacks.append([progress, copiedResultCollector] (NSArray *matches, bool didWrap) {
        [progress setCompletedUnitCount:1];
        copiedResultCollector(matches, didWrap);
        Block_release(copiedResultCollector);
    });

    if (maxResults == 1)
        _page->findString(targetString, kitFindOptions, maxResults);
    else
        _page->findStringMatches(targetString, kitFindOptions, maxResults);
}

- (void)getSelectedText:(void (^)(NSString *selectedTextString))completionHandler
{
    void (^copiedCompletionHandler)(NSString *) = Block_copy(completionHandler);

    _page->getSelectionOrContentsAsString([copiedCompletionHandler] (const String& string, WebKit::CallbackBase::Error) {
        copiedCompletionHandler(string);
        Block_release(copiedCompletionHandler);
    });
}

- (void)selectFindMatch:(id <NSTextFinderAsynchronousDocumentFindMatch>)findMatch completionHandler:(void (^)(void))completionHandler
{
    ASSERT([findMatch isKindOfClass:[WKTextFinderMatch class]]);
    ASSERT(!completionHandler);

    WKTextFinderMatch *textFinderMatch = static_cast<WKTextFinderMatch *>(findMatch);
    _page->selectFindMatch(textFinderMatch.index);
}

- (void)scrollFindMatchToVisible:(id <NSTextFinderAsynchronousDocumentFindMatch>)findMatch
{
    if (_usePlatformFindUI)
        return;

    ASSERT([findMatch isKindOfClass:[WKTextFinderMatch class]]);

    WKTextFinderMatch *textFinderMatch = static_cast<WKTextFinderMatch *>(findMatch);
    _page->indicateFindMatch(textFinderMatch.index);
}

#pragma mark - FindMatchesClient

- (void)didFindStringMatchesWithRects:(const Vector<Vector<WebCore::IntRect>>&)rectsForMatches didWrapAround:(BOOL)didWrapAround
{
    if (_findReplyCallbacks.isEmpty())
        return;

    unsigned index = 0;
    auto matchObjects = createNSArray(rectsForMatches, [&] (auto& rects) {
        RetainPtr<NSArray> rectsArray;
        if (_usePlatformFindUI)
            rectsArray = createNSArray(rects);
        else
            rectsArray = @[];
        return adoptNS([[WKTextFinderMatch alloc] initWithClient:self view:_view index:index++ rects:rectsArray.get()]);
    });

    _findReplyCallbacks.takeFirst()(matchObjects.get(), didWrapAround);
}

- (void)didGetImageForMatchResult:(WebKit::WebImage *)image
{
    if (_imageReplyCallbacks.isEmpty())
        return;

    WebCore::IntSize size = image->bitmap().size();
    size.scale(1 / _page->deviceScaleFactor());

    auto imageCallback = _imageReplyCallbacks.takeFirst();
    imageCallback(adoptNS([[NSImage alloc] initWithCGImage:image->bitmap().makeCGImage().get() size:size]).get());
}

#pragma mark - WKTextFinderMatch callbacks

- (void)getImageForMatchResult:(id <NSTextFinderAsynchronousDocumentFindMatch>)findMatch completionHandler:(void (^)(NSImage *generatedImage))completionHandler
{
    if (!_usePlatformFindUI) {
        completionHandler(nil);
        return;
    }

    ASSERT([findMatch isKindOfClass:[WKTextFinderMatch class]]);

    WKTextFinderMatch *textFinderMatch = static_cast<WKTextFinderMatch *>(findMatch);

    auto copiedImageCallback = Block_copy(completionHandler);
    _imageReplyCallbacks.append([copiedImageCallback] (NSImage *image) {
        copiedImageCallback(image);
        Block_release(copiedImageCallback);
    });

    // FIXME: There is no guarantee that this will ever result in didGetImageForMatchResult
    // being called (and thus us calling our completion handler); we should harden this
    // against all of the early returns in FindController::getImageForFindMatch.
    _page->getImageForFindMatch(textFinderMatch.index);
}

@end

#endif // PLATFORM(MAC)
