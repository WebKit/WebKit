/*
 * Copyright (C) 2014-2025 Apple Inc. All rights reserved.
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

#import "APISecurityOrigin.h"
#import "CocoaImage.h"
#import "ImageAnalysisUtilities.h"
#import "ImageOptions.h"
#import "PageLoadState.h"
#import "TextExtractionAssertionScope.h"
#import "TextExtractionCache.h"
#import "TextExtractionFilter.h"
#import "TextExtractionToStringConversion.h"
#import "TextExtractionTokenizer.h"
#import "TextExtractionURLCache.h"
#import "WKErrorInternal.h"
#import "WKFrameInfoInternal.h"
#import "WKJSHandleInternal.h"
#import "WKSecurityOriginInternal.h"
#import "WKTextExtractionUtilities.h"
#import "WKWebViewInternal.h"
#import "WebFrameProxy.h"
#import "WebPageProxy.h"
#import "WebPreferences.h"
#import "_WKTextExtractionInternal.h"
#if PLATFORM(IOS_FAMILY)
#import "WKContentViewInteraction.h"
#endif
#import <WebCore/DataDetectorType.h>
#import <WebCore/ElementTargetingTypes.h>
#import <WebCore/ICUSearcher.h>
#import <WebCore/ShareableBitmap.h>
#import <WebCore/SharedMemory.h>
#import <WebCore/TextExtractionTypes.h>
#import <WebCore/TextIndicator.h>
#import <wtf/BlockPtr.h>
#import <wtf/Box.h>
#import <wtf/CallbackAggregator.h>
#import <wtf/Scope.h>
#import <wtf/SystemTracing.h>
#import <wtf/UUID.h>
#import <wtf/cocoa/SpanCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>

#if PLATFORM(IOS_FAMILY)
static std::optional<WebCore::NodeIdentifier> activeContextMenuTargetNodeIdentifier(WKContentView *contentView)
{
    return [contentView activeContextMenuElementContext].and_then([](const auto& elementContext) {
        return elementContext.nodeIdentifier.asOptional();
    });
}
#endif

@implementation WKWebView (WKTextExtractionPrivate)

- (void)_simulateClickOverFirstMatchingTextInViewportWithUserInteraction:(NSString *)targetText completionHandler:(void(^)(BOOL))completionHandler
{
    if (!targetText.length)
        [NSException raise:NSInvalidArgumentException format:@"The target text must be non-empty."];

    if (!self._isValid)
        return completionHandler(NO);

    _page->simulateClickOverFirstMatchingTextInViewportWithUserInteraction(targetText, [completionHandler = makeBlockPtr(completionHandler)](bool success) {
        completionHandler(static_cast<BOOL>(success));
    });
}

- (void)_takeSnapshotOfNode:(_WKJSHandle *)node completionHandler:(void (^)(CocoaImage *image, NSError *))completionHandler
{
    if (!node)
        return completionHandler(nil, [NSError errorWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:nil]);

    auto info = node->_ref->info();
    RefPtr webFrame = WebKit::WebFrameProxy::webFrame(info.frameInfo.frameID);
    if (!webFrame)
        return completionHandler(nil, [NSError errorWithDomain:WKErrorDomain code:WKErrorWebViewInvalidated userInfo:nil]);

    webFrame->takeSnapshotOfNode(info.identifier, [completionHandler = makeBlockPtr(completionHandler)](auto&& handle) {
        auto makeUnknownError = [] {
            return [NSError errorWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:nil];
        };

        if (!handle)
            return completionHandler(nil, makeUnknownError());

        RefPtr bitmap = WebCore::ShareableBitmap::create(WTF::move(*handle), WebCore::SharedMemory::Protection::ReadOnly);
        if (!bitmap)
            return completionHandler(nil, makeUnknownError());

        RetainPtr cgImage = bitmap->createPlatformImage();
#if PLATFORM(MAC)
        RetainPtr image = adoptNS([[NSImage alloc] initWithCGImage:cgImage.get() size:bitmap->size()]);
#else
        RetainPtr image = adoptNS([[UIImage alloc] initWithCGImage:cgImage.get()]);
#endif
        completionHandler(image.get(), nil);
    });
}

- (void)_getSelectorPathDataForNode:(_WKJSHandle *)node completionHandler:(void (^)(NSData *))completion
{
    [self _getSelectorPathData:node completionHandler:completion];
}

- (void)_getSelectorPathData:(WKJSHandle *)node completionHandler:(void (^)(NSData *))completionHandler
{
    auto info = node->_ref->info();
    RefPtr frame = WebKit::WebFrameProxy::webFrame(info.frameInfo.frameID);
    if (!frame || !frame->isMainFrame())
        return completionHandler(nil);

    frame->getSelectorPathsForNode(WTF::move(info), [completionHandler = makeBlockPtr(completionHandler)](auto&& selectors) {
        completionHandler(WebCore::serializeTargetedElementSelectors(selectors)->createNSData().get());
    });
}

- (void)_getNodeForSelectorPathData:(NSData *)data completionHandler:(void (^)(_WKJSHandle *))completion
{
    RefPtr frame = _page->mainFrame();
    if (!frame)
        return completion(nil);

    auto selectors = WebCore::deserializeTargetedElementSelectors(span(data));
    if (!selectors)
        return completion(nil);

    frame->getNodeForSelectorPaths(WTF::move(*selectors), [completion = makeBlockPtr(completion)](auto&& info) {
        completion(info ? wrapper(API::JSHandle::create(WTF::move(*info))).get() : nil);
    });
}

@end

@interface WKWebView (WKTextExtractionInternal)
- (void)_describeInteraction:(WebCore::TextExtraction::Interaction)interaction inFrame:(RefPtr<WebKit::WebFrameProxy>)targetFrame nodeIdentifier:(const String&)nodeIdentifier staleNodeNote:(const String&)staleNodeNote shouldResolveStaleNodeIdentifier:(BOOL)shouldResolveStaleNodeIdentifier completionHandler:(void (^)(NSString *, NSError *))completionHandler;
- (Vector<String>)_activeNativeMenuItemTitles;
#if PLATFORM(MAC)
- (RetainPtr<NSPopUpButtonCell>)_activePopupButtonCell;
#endif
@end

@implementation WKWebView (WKTextExtraction)

- (NSString *)_activeContextMenuTargetNodeIdentifier
{
#if PLATFORM(IOS_FAMILY)
    if (auto nodeIdentifier = activeContextMenuTargetNodeIdentifier(_contentView))
        return [NSString stringWithFormat:@"%llu", nodeIdentifier->toUInt64()];
#endif
    return nil;
}

static Vector<std::pair<String, String>> extractReplacementStrings(_WKTextExtractionConfiguration *configuration)
{
    Vector<std::pair<String, String>> result;
    RetainPtr replacementStrings = [configuration replacementStrings];
    for (NSString *replacement in replacementStrings.get()) {
        if (!replacement.length)
            continue;

        auto foldedKey = WebKit::foldTextForReplacement(String { replacement });
        if (foldedKey.isEmpty())
            continue;

        result.append({ WTF::move(foldedKey), String { [replacementStrings objectForKey:replacement] } });
    }

    std::ranges::sort(result, [](auto& a, auto& b) {
        if (a.first.length() != b.first.length())
            return a.first.length() > b.first.length();
        return codePointCompareLessThan(a.first, b.first);
    });
    return result;
}

static WebKit::TextExtractionOutputFormat textExtractionOutputFormat(_WKTextExtractionConfiguration *configuration)
{
    switch (configuration.outputFormat) {
    case _WKTextExtractionOutputFormatTextTree:
        return WebKit::TextExtractionOutputFormat::TextTree;
    case _WKTextExtractionOutputFormatHTML:
        return WebKit::TextExtractionOutputFormat::HTMLMarkup;
    case _WKTextExtractionOutputFormatMarkdown:
        return WebKit::TextExtractionOutputFormat::Markdown;
    case _WKTextExtractionOutputFormatJSON:
        return WebKit::TextExtractionOutputFormat::MinifiedJSON;
    case _WKTextExtractionOutputFormatPlainText:
        return WebKit::TextExtractionOutputFormat::PlainText;
    default:
        ASSERT_NOT_REACHED();
        return WebKit::TextExtractionOutputFormat::TextTree;
    }
}

#if USE(APPLE_INTERNAL_SDK) || (!PLATFORM(WATCHOS) && !PLATFORM(APPLETV))
- (void)_ensureTextExtractionFilterRulesWithCompletionHandler:(CompletionHandler<void()>&&)completionHandler
{
    _page->hasTextExtractionFilterRules([completionHandler = WTF::move(completionHandler), weakSelf = WeakObjCPtr<WKWebView>(self)](bool hasRules) mutable {
        if (hasRules)
            return completionHandler();

        WebKit::requestTextExtractionFilterRuleData([completionHandler = WTF::move(completionHandler), weakSelf](auto&& data) mutable {
            if (RetainPtr strongSelf = weakSelf.get())
                strongSelf->_page->updateTextExtractionFilterRules(WTF::move(data));
            completionHandler();
        });
    });
}
#endif // USE(APPLE_INTERNAL_SDK) || (!PLATFORM(WATCHOS) && !PLATFORM(APPLETV))

- (void)_extractDebugTextWithConfigurationWithoutUpdatingFilterRules:(_WKTextExtractionConfiguration *)configuration assertionScope:(UniqueRef<WebKit::TextExtractionAssertionScope>&&)assertionScope completionHandler:(void(^)(_WKTextExtractionResult *))completionHandler
{
    bool allowFiltering = protect(_page->preferences())->textExtractionFilterEnabled();
    bool filterUsingClassifier = allowFiltering && configuration.filterOptions & _WKTextExtractionFilterClassifier;
    bool filterHiddenText = allowFiltering && configuration.filterOptions & _WKTextExtractionFilterTextRecognition;
    bool filterUsingRules = allowFiltering && configuration.filterOptions & _WKTextExtractionFilterRules;

    static uint64_t nextTextExtractionTracingID = 0;
    auto currentTextExtractionTracingID = ++nextTextExtractionTracingID;
    auto mainFrameWebProcessID = static_cast<uint64_t>(self._webProcessIdentifier);
    auto gpuProcessID = static_cast<uint64_t>(self._gpuProcessIdentifier);
    tracePoint(TextExtractionStart, currentTextExtractionTracingID, mainFrameWebProcessID, gpuProcessID);

    auto endTextExtractionScope = makeScopeExit([assertionScope = WTF::move(assertionScope), currentTextExtractionTracingID, mainFrameWebProcessID, gpuProcessID] {
        tracePoint(TextExtractionEnd, currentTextExtractionTracingID, mainFrameWebProcessID, gpuProcessID);
    });

#if ENABLE(TEXT_EXTRACTION_FILTER)
    if (filterUsingClassifier)
        WebKit::TextExtractionFilter::singleton().prewarm();
#endif

    RefPtr mainFrame = _page->mainFrame();
    if (!mainFrame)
        return completionHandler(nil);

    std::optional<WebKit::TextExtractionVersion> version;
    if (RetainPtr overrideVersion = dynamic_objc_cast<NSNumber>([[NSUserDefaults standardUserDefaults] objectForKey:@"WebKit2TextExtractionOutputVersion"]))
        version = [overrideVersion unsignedIntValue];

    std::optional<uint64_t> maxWordsPerParagraph;
    if (configuration.maxWordsPerParagraph < NSUIntegerMax)
        maxWordsPerParagraph = { static_cast<uint64_t>(configuration.maxWordsPerParagraph) };

    if (!_textExtractionURLCache)
        _textExtractionURLCache = WebKit::TextExtractionURLCache::create();

    _lastTextExtractionReplacementStrings = extractReplacementStrings(configuration);

    [self _requestTextExtractionInternal:configuration completion:[
        startTime = MonotonicTime::now(),
        completionHandler = makeBlockPtr(completionHandler),
        weakSelf = WeakObjCPtr<WKWebView>(self),
        mainFrameIdentifier = mainFrame->frameID(),
        filterUsingClassifier,
        filterHiddenText,
        filterUsingRules,
        includeURLs = configuration.includeURLs,
        includeRects = configuration.includeRects,
        includeTagName = configuration.includeTagName,
        includeSelectOptions = configuration.includeSelectOptions,
        applyDiscretionaryWordLimit = configuration.maxWordsPerParagraphPolicy == _WKTextExtractionWordLimitPolicyDiscretionary,
        shortenURLs = configuration.shortenURLs,
        maxWordsPerParagraph = WTF::move(maxWordsPerParagraph),
        version,
        replacementStrings = _lastTextExtractionReplacementStrings,
        outputFormat = textExtractionOutputFormat(configuration),
        endTextExtractionScope = WTF::move(endTextExtractionScope),
        origin = _page->pageLoadState().origin(),
        topHostName = URL { _page->pageLoadState().activeURL() }.host().toString()
    ](auto&& result) mutable {
        RetainPtr strongSelf = weakSelf.get();
        if (!strongSelf)
            return completionHandler(nil);

        if (!result)
            return completionHandler(nil);

        Vector<WebKit::TextExtractionFilterCallback> filterCallbacks;

        if (filterUsingClassifier) {
#if ENABLE(TEXT_EXTRACTION_FILTER)
            filterCallbacks.append([](auto& text, auto&&, auto&&) mutable {
                WebKit::TextExtractionFilterPromise::Producer producer;
                Ref promise = producer.promise();

                WebKit::TextExtractionFilter::singleton().shouldFilter(text, [producer = WTF::move(producer), text](bool shouldFilterOut) mutable {
                    if (shouldFilterOut)
                        producer.settle(emptyString());
                    else
                        producer.settle(text);
                });
                return promise;
            });
#endif // ENABLE(TEXT_EXTRACTION_FILTER)
        }

        if (filterHiddenText) {
#if ENABLE(TEXT_EXTRACTION_FILTER)
            filterCallbacks.append([strongSelf](auto& text, auto&& frameID, auto&& enclosingNodeID) mutable {
                WebKit::TextExtractionFilterPromise::Producer producer;
                Ref promise = producer.promise();

                auto lines = text.splitAllowingEmptyEntries('\n');
                auto components = Box<Vector<String>>::create();
                components->resizeToFit(lines.size());

                Ref aggregator = MainRunLoopCallbackAggregator::create([producer = WTF::move(producer), components] mutable {
                    producer.settle(makeStringByJoining(WTF::move(*components), "\n"_s));
                });

                for (size_t index = 0; index < lines.size(); ++index) {
                    static constexpr auto minimumLengthForTextDetection = 100;
                    auto line = lines[index];
                    if (line.length() < minimumLengthForTextDetection) {
                        components->at(index) = WTF::move(line);
                        continue;
                    }

                    [strongSelf _validateText:line inFrame:std::optional { frameID } inNode:std::optional { enclosingNodeID } completionHandler:[aggregator, components, index](auto& result) mutable {
                        components->at(index) = result;
                    }];
                }

                return promise;
            });
#endif // ENABLE(TEXT_EXTRACTION_FILTER)
        }

        if (filterUsingRules) {
#if ENABLE(TEXT_EXTRACTION_FILTER)
            filterCallbacks.append([page = strongSelf->_page](auto& text, auto&&, auto&&) mutable {
                WebKit::TextExtractionFilterPromise::Producer producer;
                Ref promise = producer.promise();

                page->applyTextExtractionFilter(text, [producer = WTF::move(producer)](auto&& output) mutable {
                    producer.settle(WTF::move(output));
                });

                return promise;
            });
#endif // ENABLE(TEXT_EXTRACTION_FILTER)
        }

        static constexpr auto minimumTextLengthWhenApplyingDiscretionaryWordLimit = 1 << 12;
        bool enforceWordLimit = !applyDiscretionaryWordLimit || result->visibleTextLength >= minimumTextLengthWhenApplyingDiscretionaryWordLimit;
        if (!enforceWordLimit)
            maxWordsPerParagraph = std::nullopt;

        using enum WebKit::TextExtractionOptionFlag;
        WebKit::TextExtractionOptionFlags optionFlags;
        if (includeURLs)
            optionFlags.add(IncludeURLs);
        if (includeRects)
            optionFlags.add(IncludeRects);
        if (shortenURLs)
            optionFlags.add(ShortenURLs);
        if (includeSelectOptions)
            optionFlags.add(IncludeSelectOptions);
        if (includeTagName)
            optionFlags.add(IncludeTagName);
        RefPtr urlCache = strongSelf->_textExtractionURLCache;
        WebKit::TextExtractionOptions options {
            WTF::move(mainFrameIdentifier),
            WTF::move(filterCallbacks),
            [strongSelf _activeNativeMenuItemTitles],
            WTF::move(replacementStrings),
            version,
            optionFlags,
            outputFormat,
            urlCache.get(),
            WTF::move(maxWordsPerParagraph),
            WTF::move(topHostName),
        };

        WebKit::convertToText(WTF::move(result->rootItem), WTF::move(options), [weakSelf, startTime, urlCache, origin = WTF::move(origin), completionHandler = WTF::move(completionHandler), endTextExtractionScope = WTF::move(endTextExtractionScope)](auto&& result) {
            RetainPtr strongSelf = weakSelf.get();
            if (!strongSelf)
                return completionHandler(WebKit::createEmptyTextExtractionResult().get());

            RELEASE_LOG(TextExtraction, "<%@: %p> Extraction complete (%.0f ms)", [strongSelf class], strongSelf.get(), (MonotonicTime::now() - startTime).milliseconds());
            auto [text, filteredOutAnyText, shortenedURLStrings, textToContainerMap, lineContents] = result;

            if (strongSelf->_page)
                strongSelf->_page->textExtractionCache().add(strongSelf->_page->currentURL(), WTF::move(lineContents));

            RetainPtr shortenedURLs = adoptNS([[NSMutableDictionary alloc] initWithCapacity:shortenedURLStrings.size()]);
            for (auto& string : shortenedURLStrings) {
                if (auto url = urlCache->urlForShortenedString(string); url.isValid()) {
                    if (RetainPtr nsURL = url.createNSURL())
                        [shortenedURLs setObject:nsURL.get() forKey:string.createNSString().get()];
                }
            }
            completionHandler(adoptNS([[_WKTextExtractionResult alloc]
                initWithWebView:strongSelf.get()
                origin:wrapper(API::SecurityOrigin::create(origin)).get()
                textContent:text.createNSString().get()
                filteredOutAnyText:filteredOutAnyText
                shortenedURLs:shortenedURLs.get()
                textToContainerMap:WTF::move(textToContainerMap)]).get());
        });
    }];
}

- (Expected<std::pair<RefPtr<WebKit::WebFrameProxy>, WebCore::TextExtraction::Interaction>, RetainPtr<NSString>>)_convertToWebCoreInteraction:(_WKTextExtractionInteraction *)wkInteraction nodeIdentifier:(const String&)nodeIdentifierString
{
    std::optional<WebCore::FrameIdentifier> frameIdentifier;
    WebCore::TextExtraction::Interaction interaction;
    interaction.action = [&] {
        switch (wkInteraction.action) {
        case _WKTextExtractionActionClick:
            return WebCore::TextExtraction::Action::Click;
        case _WKTextExtractionActionSelectText:
            return WebCore::TextExtraction::Action::SelectText;
        case _WKTextExtractionActionSelectMenuItem:
            return WebCore::TextExtraction::Action::SelectMenuItem;
        case _WKTextExtractionActionTextInput:
            return WebCore::TextExtraction::Action::TextInput;
        case _WKTextExtractionActionKeyPress:
            return WebCore::TextExtraction::Action::KeyPress;
        case _WKTextExtractionActionHighlightText:
            return WebCore::TextExtraction::Action::HighlightText;
        case _WKTextExtractionActionScroll:
            return WebCore::TextExtraction::Action::Scroll;
        case _WKTextExtractionActionHover:
            return WebCore::TextExtraction::Action::Hover;
        default:
            ASSERT_NOT_REACHED();
            return WebCore::TextExtraction::Action::Click;
        }
    }();

    if (RetainPtr elementHandle = [wkInteraction elementHandle]) {
        const auto& info = elementHandle->_ref->info();
        interaction.targetNodeHandleIdentifier = info.identifier;
        frameIdentifier = info.frameInfo.frameID;
    } else if (auto identifiers = WebKit::parseExtractedNodeInfo(nodeIdentifierString)) {
        interaction.nodeIdentifier = { WTF::move(identifiers->nodeIdentifier) };
        frameIdentifier = WTF::move(identifiers->frameIdentifier);
    }

    if (wkInteraction.hasSetLocation) {
        auto insets = self.obscuredContentInsets;
        auto location = CGPointMake(wkInteraction.location.x + insets.left, wkInteraction.location.y + insets.top);
#if PLATFORM(IOS_FAMILY)
        interaction.locationInRootView = [self convertPoint:location toView:_contentView.get()];
#else
        interaction.locationInRootView = location;
#endif
    }
    interaction.text = wkInteraction.text;
    interaction.replaceAll = wkInteraction.replaceAll;
    interaction.scrollToVisible = wkInteraction.scrollToVisible;
    interaction.scrollDelta = WebCore::FloatSize { wkInteraction.scrollDelta };
    if (!interaction.nodeIdentifier && !interaction.targetNodeHandleIdentifier) {
        if (RetainPtr context = [wkInteraction extractionContext]) {
            auto result = [context resolveContainerForSearchText:wkInteraction.text];
            if (!result.has_value())
                return makeUnexpected(result.error().createNSString());

            if (auto container = *result) {
                interaction.nodeIdentifier = container->nodeIdentifier;
                if (container->frameIdentifier)
                    frameIdentifier = *container->frameIdentifier;
            }
        }
    }
    return std::pair {
        RefPtr { WebKit::WebFrameProxy::webFrame(frameIdentifier) ?: _page->mainFrame() },
        WTF::move(interaction)
    };
}

- (void)_performInteraction:(WebCore::TextExtraction::Interaction)interaction inFrame:(RefPtr<WebKit::WebFrameProxy>)targetFrame actionType:(_WKTextExtractionAction)actionType nodeIdentifier:(const String&)attemptedIdentifier staleNodeNote:(const String&)staleNodeNote shouldResolveStaleNodeIdentifier:(BOOL)shouldResolveStaleNodeIdentifier completionHandler:(void(^)(_WKTextExtractionInteractionResult *))completionHandler
{
#if USE(APPLE_INTERNAL_SDK) || (!PLATFORM(WATCHOS) && !PLATFORM(APPLETV))
    RefPtr page = _page;
    if (!page || !targetFrame)
        return completionHandler(adoptNS([[_WKTextExtractionInteractionResult alloc] initWithErrorDescription:@"Web view is invalid" summary:nil interactedElementBounds:CGRectNull]));

    UniqueRef assertionScope = page->createTextExtractionAssertionScope();
    auto interactionForRetry = interaction;
    targetFrame->handleTextExtractionInteraction(WTF::move(interaction), [
        weakSelf = WeakObjCPtr<WKWebView>(self),
        weakPage = WeakPtr { *page },
        assertionScope = WTF::move(assertionScope),
        actionType,
        attemptedIdentifier,
        staleNodeNote,
        shouldResolveStaleNodeIdentifier,
        interaction = WTF::move(interactionForRetry),
        completionHandler = makeBlockPtr(WTF::move(completionHandler))
    ](bool success, String&& description, WebCore::FloatRect interactedElementBounds) mutable {
        RetainPtr strongSelf = weakSelf.get();
        RefPtr strongPage = weakPage.get();

        if (!success && shouldResolveStaleNodeIdentifier && strongSelf && strongPage && !attemptedIdentifier.isEmpty()) {
            auto resolved = strongPage->textExtractionCache().resolve(attemptedIdentifier);
            if (resolved.resolution == WebKit::TextExtractionCache::NodeResolution::Remapped) {
                RELEASE_LOG(TextExtraction, "<%@: %p> Interaction failed; re-resolved stale node %" PUBLIC_LOG_STRING " to %" PUBLIC_LOG_STRING " and retrying", [strongSelf class], strongSelf.get(), attemptedIdentifier.utf8().data(), resolved.identifier.utf8().data());
                auto note = makeString("Note: the targeted node (uid="_s, attemptedIdentifier, ") was stale from an earlier page state and was automatically re-resolved to the current matching element."_s);
                RefPtr<WebKit::WebFrameProxy> retryFrame;
                if (auto identifiers = WebKit::parseExtractedNodeInfo(resolved.identifier)) {
                    interaction.nodeIdentifier = { WTF::move(identifiers->nodeIdentifier) };
                    retryFrame = WebKit::WebFrameProxy::webFrame(WTF::move(identifiers->frameIdentifier));
                }
                if (!retryFrame)
                    retryFrame = strongPage->mainFrame();
                [strongSelf _performInteraction:WTF::move(interaction) inFrame:WTF::move(retryFrame) actionType:actionType nodeIdentifier:resolved.identifier staleNodeNote:note shouldResolveStaleNodeIdentifier:NO completionHandler:completionHandler.get()];
                return;
            }
            if (resolved.resolution == WebKit::TextExtractionCache::NodeResolution::Stale || resolved.resolution == WebKit::TextExtractionCache::NodeResolution::Ambiguous)
                description = makeString(description, " The page changed since this uid was last observed; re-extract the page and retry with a current uid."_s);
        }

        RetainPtr<NSString> errorDescription;
        RetainPtr<NSString> summary;
        if (success) {
            summary = description.createNSString();
            if (!staleNodeNote.isEmpty())
                summary = adoptNS([[NSString alloc] initWithFormat:@"%@ %@", summary.get(), staleNodeNote.createNSString().get()]);
        } else
            errorDescription = description.createNSString();

        CGRect bounds = CGRectNull;
        if (!interactedElementBounds.isEmpty()) {
#if PLATFORM(IOS_FAMILY)
            if (RetainPtr contentView = strongSelf ? strongSelf->_contentView : nil)
                bounds = [strongSelf convertRect:interactedElementBounds fromView:contentView.get()];
            else
#endif
                bounds = interactedElementBounds;
        }
        RetainPtr result = adoptNS([[_WKTextExtractionInteractionResult alloc] initWithErrorDescription:errorDescription.get() summary:summary.get() interactedElementBounds:bounds]);
        if (!strongSelf)
            return completionHandler(result.get());

        if (success)
            RELEASE_LOG(TextExtraction, "<%@: %p> %@ succeeded", [strongSelf class], strongSelf.get(), WebKit::nameForTextExtractionAction(actionType));
        else
            RELEASE_LOG_ERROR(TextExtraction, "<%@: %p> %@ failed", [strongSelf class], strongSelf.get(), WebKit::nameForTextExtractionAction(actionType));

        if (!strongPage)
            return completionHandler(result.get());

        Ref aggregator = EagerCallbackAggregator<void()>::create([completion = WTF::move(completionHandler), assertionScope = WTF::move(assertionScope), result] mutable {
            completion(result.get());
        });

        strongPage->callAfterNextPresentationUpdate([aggregator] {
            aggregator.get()();
        });

        RunLoop::mainSingleton().dispatchAfter(100_ms, [aggregator] {
            aggregator.get()();
        });
    });
#endif // USE(APPLE_INTERNAL_SDK) || (!PLATFORM(WATCHOS) && !PLATFORM(APPLETV))
}

- (void)_filterExtractedStringWithoutUpdatingFilterRules:(NSString *)string options:(_WKTextExtractionFilterOptions)options completionHandler:(void(^)(NSString *))completionHandler
{
#if ENABLE(TEXT_EXTRACTION_FILTER)
    bool allowFiltering = protect(_page->preferences())->textExtractionFilterEnabled();
    bool filterUsingClassifier = allowFiltering && options & _WKTextExtractionFilterClassifier;
    bool filterUsingRules = allowFiltering && options & _WKTextExtractionFilterRules;

    struct Applier : RefCounted<Applier> {
        Vector<WebKit::TextExtractionFilterCallback> callbacks;
        BlockPtr<void(NSString *)> completion;

        void apply(String&& text, size_t index)
        {
            if (index >= callbacks.size()) {
                completion(text.createNSString().get());
                return;
            }

            Ref promise = callbacks[index](text, std::nullopt, std::nullopt);
            promise->whenSettled(RunLoop::mainSingleton(), [text, protectedThis = Ref { *this }, index](auto&& result) mutable {
                if (!result)
                    protectedThis->completion(@"");
                else
                    protectedThis->apply(WTF::move(*result), index + 1);
            });
        }
    };

    Ref applier = adoptRef(*new Applier);
    applier->completion = makeBlockPtr(completionHandler);

    if (filterUsingClassifier) {
        applier->callbacks.append([](auto& text, auto&&, auto&&) mutable {
            WebKit::TextExtractionFilterPromise::Producer producer;

            Ref promise = producer.promise();
            WebKit::TextExtractionFilter::singleton().shouldFilter(text, [producer = WTF::move(producer), text](bool shouldFilterOut) mutable {
                if (shouldFilterOut)
                    producer.settle(emptyString());
                else
                    producer.settle(text);
            });

            return promise;
        });
    }

    if (filterUsingRules) {
        applier->callbacks.append([page = _page](auto& text, auto&&, auto&&) mutable {
            WebKit::TextExtractionFilterPromise::Producer producer;

            Ref promise = producer.promise();
            page->applyTextExtractionFilter(text, [producer = WTF::move(producer)](auto&& output) mutable {
                producer.settle(WTF::move(output));
            });

            return promise;
        });
    }

    applier->apply(String(string), 0);
#else
    completionHandler(string);
#endif // ENABLE(TEXT_EXTRACTION_FILTER)
}

#if USE(APPLE_INTERNAL_SDK) || (!PLATFORM(WATCHOS) && !PLATFORM(APPLETV))

static Vector<WebCore::JSHandleIdentifier> extractHandleIdentifiersOfNodesToSkip(Ref<WebKit::WebFrameProxy>&& frame, _WKTextExtractionConfiguration *configuration)
{
    Vector<WebCore::JSHandleIdentifier> nodes;
    RetainPtr nodesToSkip = [configuration nodesToSkip];
    nodes.reserveInitialCapacity([nodesToSkip count]);
    for (_WKJSHandle *handle in nodesToSkip.get()) {
        if (auto identifier = WebKit::jsHandleIdentifierInFrame(frame, handle))
            nodes.append(WTF::move(*identifier));
    }
    return nodes;
}

static HashMap<String, HashMap<WebCore::JSHandleIdentifier, String>> extractClientNodeAttributes(Ref<WebKit::WebFrameProxy>&& frame, _WKTextExtractionConfiguration *configuration)
{
    __block HashMap<String, HashMap<WebCore::JSHandleIdentifier, String>> result;

    [configuration forEachClientNodeAttribute:^(NSString *attribute, NSString *value, _WKJSHandle *nodeHandle) {
        auto handleIdentifier = WebKit::jsHandleIdentifierInFrame(frame.copyRef(), nodeHandle);
        if (!handleIdentifier)
            return;

        result.ensure(String { attribute }, [] {
            return HashMap<WebCore::JSHandleIdentifier, String> { };
        }).iterator->value.add(WTF::move(*handleIdentifier), String { value });
    }];

    return result;
}

static OptionSet<WebCore::TextExtraction::EventListenerCategory> NODELETE coreEventListenerCategories(_WKTextExtractionEventListenerCategory categories)
{
    OptionSet<WebCore::TextExtraction::EventListenerCategory> coreCategories;
    if (categories & _WKTextExtractionEventListenerCategoryClick)
        coreCategories.add(WebCore::TextExtraction::EventListenerCategory::Click);
    if (categories & _WKTextExtractionEventListenerCategoryHover)
        coreCategories.add(WebCore::TextExtraction::EventListenerCategory::Hover);
    if (categories & _WKTextExtractionEventListenerCategoryTouch)
        coreCategories.add(WebCore::TextExtraction::EventListenerCategory::Touch);
    if (categories & _WKTextExtractionEventListenerCategoryWheel)
        coreCategories.add(WebCore::TextExtraction::EventListenerCategory::Wheel);
    if (categories & _WKTextExtractionEventListenerCategoryKeyboard)
        coreCategories.add(WebCore::TextExtraction::EventListenerCategory::Keyboard);
    return coreCategories;
}

#if ENABLE(DATA_DETECTION)

static OptionSet<WebCore::DataDetectorType> NODELETE coreDataDetectorTypes(_WKTextExtractionDataDetectorTypes types)
{
    OptionSet<WebCore::DataDetectorType> coreTypes;
    if (types & _WKTextExtractionDataDetectorMoney)
        coreTypes.add(WebCore::DataDetectorType::Money);
    if (types & _WKTextExtractionDataDetectorAddress)
        coreTypes.add(WebCore::DataDetectorType::Address);
    if (types & _WKTextExtractionDataDetectorCalendarEvent)
        coreTypes.add(WebCore::DataDetectorType::CalendarEvent);
    if (types & _WKTextExtractionDataDetectorTrackingNumber)
        coreTypes.add(WebCore::DataDetectorType::TrackingNumber);
    return coreTypes;
}

#endif // ENABLE(DATA_DETECTION)

#endif // USE(APPLE_INTERNAL_SDK) || (!PLATFORM(WATCHOS) && !PLATFORM(APPLETV))

- (void)_requestTextExtractionInternal:(_WKTextExtractionConfiguration *)configuration completion:(CompletionHandler<void(std::optional<WebCore::TextExtraction::Result>&&)>&&)completion
{
#if USE(APPLE_INTERNAL_SDK) || (!PLATFORM(WATCHOS) && !PLATFORM(APPLETV))
    Ref preferences = _page->preferences();
    if (!self._isValid || !preferences->textExtractionEnabled())
        return completion({ });

    RefPtr mainFrame = _page->mainFrame();
    if (!mainFrame)
        return completion({ });

    auto rectInWebView = configuration.targetRect;
    bool mergeParagraphs = configuration.mergeParagraphs;
    auto nodeIdentifierInclusion = [&] {
        switch (configuration.nodeIdentifierInclusion) {
        case _WKTextExtractionNodeIdentifierInclusionNone:
            return WebCore::TextExtraction::NodeIdentifierInclusion::None;
        case _WKTextExtractionNodeIdentifierInclusionEditableOnly:
            return WebCore::TextExtraction::NodeIdentifierInclusion::EditableOnly;
        case _WKTextExtractionNodeIdentifierInclusionInteractive:
            return WebCore::TextExtraction::NodeIdentifierInclusion::Interactive;
        case _WKTextExtractionNodeIdentifierInclusionAllContainers:
            return WebCore::TextExtraction::NodeIdentifierInclusion::AllContainers;
        }
        return WebCore::TextExtraction::NodeIdentifierInclusion::None;
    }();
    bool skipNearlyTransparentContent = configuration.skipNearlyTransparentContent;
    auto rectInRootView = [&] -> std::optional<WebCore::FloatRect> {
        if (CGRectIsNull(rectInWebView))
            return std::nullopt;

#if PLATFORM(IOS_FAMILY)
        return WebCore::FloatRect { [self convertRect:rectInWebView toView:_contentView.get()] };
#else
        return WebCore::FloatRect { rectInWebView };
#endif
    }();

#if PLATFORM(IOS_FAMILY)
    auto contextMenuTargetNodeIdentifier = activeContextMenuTargetNodeIdentifier(_contentView);
#else
    std::optional<WebCore::NodeIdentifier> contextMenuTargetNodeIdentifier;
#endif

    auto makeRequest = [&](Ref<WebKit::WebFrameProxy>&& frame) {
        return WebCore::TextExtraction::Request {
            .clientNodeAttributes = extractClientNodeAttributes(frame.copyRef(), configuration),
            .collectionRectInRootView = rectInRootView,
            .targetNodeHandleIdentifier = WebKit::jsHandleIdentifierInFrame(frame, configuration.targetNode),
            .handleIdentifiersOfNodesToSkip = extractHandleIdentifiersOfNodesToSkip(frame.copyRef(), configuration),
            .contextMenuTargetNodeIdentifier = contextMenuTargetNodeIdentifier,
            .mergeParagraphs = mergeParagraphs,
            .skipNearlyTransparentContent = skipNearlyTransparentContent,
            .nodeIdentifierInclusion = nodeIdentifierInclusion,
            .eventListenerCategories = coreEventListenerCategories(configuration.eventListenerCategories),
            .includeAccessibilityAttributes = !!configuration.includeAccessibilityAttributes,
            .includeTextInAutoFilledControls = !!configuration.includeTextInAutoFilledControls,
            .includeOffscreenPasswordFields = !!configuration.includeOffscreenPasswordFields,
            .includeTagName = !!configuration.includeTagName,
#if ENABLE(DATA_DETECTION)
            .dataDetectorTypes = coreDataDetectorTypes(configuration.dataDetectorTypes),
#endif
        };
    };

    HashSet<Ref<WebKit::WebFrameProxy>> additionalFrames;
    for (WKFrameInfo *info in [configuration additionalFrames]) {
        RefPtr frame = WebKit::WebFrameProxy::webFrame(info->_frameInfo->frameInfoData().frameID);
        if (!frame)
            continue;

        RefPtr parentFrame = frame->parentFrame();
        if (!parentFrame)
            continue;

        if (frame->isSameOriginAs(*parentFrame))
            continue;

        additionalFrames.add(frame.releaseNonNull());
    }

    WeakObjCPtr weakSelf = self;
    auto startTime = MonotonicTime::now();
    RELEASE_LOG(TextExtraction, "<%@: %p> Starting text extraction", [self class], self);
    auto results = Box<WebCore::TextExtraction::PageResults>::create();
    auto aggregator = MainRunLoopCallbackAggregator::create([results, completion = WTF::move(completion)] mutable {
        auto result = WebCore::TextExtraction::collatePageResults(WTF::move(*results));
        auto rootData = result.rootItem.dataAs<WebCore::TextExtraction::ScrollableItemData>();
        if (!rootData || !rootData->isRoot)
            return completion(std::nullopt);

        completion(WTF::move(result));
    });

    mainFrame->requestTextExtraction(makeRequest({ *mainFrame }), [weakSelf, startTime, aggregator, results](auto&& result) {
        RetainPtr strongSelf = weakSelf.get();
        if (!strongSelf)
            return;

        RELEASE_LOG(TextExtraction, "<%@: %p> • Mainframe items received (%.0f ms)", [strongSelf class], strongSelf.get(), (MonotonicTime::now() - startTime).milliseconds());
        results->mainFrameResult = WTF::move(result);
    });

    for (auto& frame : additionalFrames) {
        frame->requestTextExtraction(makeRequest(frame.copyRef()), [weakSelf, startTime, frameID = frame->frameID(), aggregator, results](auto&& result) {
            RetainPtr strongSelf = weakSelf.get();
            if (!strongSelf)
                return;

            RELEASE_LOG(TextExtraction, "<%@: %p> • Subframe items received (%.0f ms)", [strongSelf class], strongSelf.get(), (MonotonicTime::now() - startTime).milliseconds());
            auto addResult = results->subFrameResults.add(frameID, makeUniqueRef<WebCore::TextExtraction::Result>(WTF::move(result)));
            ASSERT_UNUSED(addResult, addResult.isNewEntry);
        });
    }

    WebKit::TextExtractionTokenizer::singleton().prewarm();

#if ENABLE(TEXT_EXTRACTION_FILTER) && HAVE(VISION)
    if (!_textExtractionRecognizedWords && preferences->textExtractionFilterEnabled() && (configuration.filterOptions & _WKTextExtractionFilterTextRecognition)) {
        protect(_page)->callAfterNextPresentationUpdate([rectInWebView, weakSelf, aggregator, startTime] mutable {
            RetainPtr strongSelf = weakSelf.get();
            if (!strongSelf)
                return;

            WebCore::FloatRect snapshotRect { rectInWebView };
            if (CGRectIsNull(rectInWebView)) {
#if PLATFORM(IOS_FAMILY)
                snapshotRect = [strongSelf convertRect:[strongSelf->_contentView bounds] fromView:strongSelf->_contentView.get()];
#else
                auto scrollPosition = strongSelf->_page->mainFrameScrollPosition();
                snapshotRect = NSMakeRect(-scrollPosition.x(), -scrollPosition.y(), strongSelf->_lastContentSize.width, strongSelf->_lastContentSize.height);
#endif
            }

            WebCore::FloatSize bitmapSize { snapshotRect.width(), snapshotRect.height() };
            bitmapSize.scale(strongSelf->_page->deviceScaleFactor());

            static constexpr OptionSet snapshotOptions { WebKit::SnapshotOption::FullContentRect, WebKit::SnapshotOption::ExcludeSelectionHighlighting };

            strongSelf->_page->takeSnapshot(WebCore::enclosingIntRect(snapshotRect), WebCore::expandedIntSize(bitmapSize), snapshotOptions, [weakSelf, aggregator = WTF::move(aggregator), startTime](CGImageRef image) mutable {
                RetainPtr strongSelf = weakSelf.get();
                if (!strongSelf)
                    return;

                auto snapshotEndTime = MonotonicTime::now();
                auto millisecondsSpent = (snapshotEndTime - startTime).milliseconds();
                if (!image) {
                    RELEASE_LOG_ERROR(TextExtraction, "<%@: %p> • Failed to take full snapshot (%.0f ms)", [strongSelf class], strongSelf.get(), millisecondsSpent);
                    return;
                }

                RELEASE_LOG(TextExtraction, "<%@: %p> • Took full snapshot (%.0f ms)", [strongSelf class], strongSelf.get(), millisecondsSpent);
                WebKit::recognizeText(image, WebKit::TextRecognitionLevel::Fast, [weakSelf, snapshotEndTime, aggregator = WTF::move(aggregator)](NSString *text, NSError *error) mutable {
                    RetainPtr strongSelf = weakSelf.get();
                    if (!strongSelf)
                        return;

                    auto millisecondsSpent = (MonotonicTime::now() - snapshotEndTime).milliseconds();
                    if (error) {
                        RELEASE_LOG_ERROR(TextExtraction, "<%@: %p> • Failed to recognize text in full snapshot: %@ (%.0f ms)", [strongSelf class], strongSelf.get(), error, millisecondsSpent);
                        return;
                    }

                    __block HashSet<String> recognizedWords;
                    [text enumerateSubstringsInRange:NSMakeRange(0, text.length) options:NSStringEnumerationByWords usingBlock:^(NSString *word, NSRange, NSRange, BOOL *) {
                        if (!word.length)
                            return;

                        recognizedWords.add(WebCore::foldQuoteMarks(word.lowercaseString));
                    }];
                    RELEASE_LOG(TextExtraction, "<%@: %p> • Recognized text in full snapshot (%.0f ms) - %u words", [strongSelf class], strongSelf.get(), millisecondsSpent, recognizedWords.size());
                    strongSelf->_textExtractionRecognizedWords = { WTF::move(recognizedWords) };
                });
            });
        });
    }
#endif // ENABLE(TEXT_EXTRACTION_FILTER) && HAVE(VISION)
#endif // USE(APPLE_INTERNAL_SDK) || (!PLATFORM(WATCHOS) && !PLATFORM(APPLETV))
}

- (void)_requestTextExtraction:(_WKTextExtractionConfiguration *)configuration completionHandler:(void(^)(WKTextExtractionItem *))completionHandler
{
#if USE(APPLE_INTERNAL_SDK) || (!PLATFORM(WATCHOS) && !PLATFORM(APPLETV))
    [self _requestTextExtractionInternal:configuration completion:[completionHandler = makeBlockPtr(completionHandler), weakSelf = WeakObjCPtr<WKWebView>(self)](auto&& result) {
        RetainPtr strongSelf = weakSelf.get();
        if (!strongSelf)
            return completionHandler(nil);

        if (!result)
            return completionHandler(nil);

        RetainPtr rootItem = WebKit::createItem(WTF::move(result->rootItem), [strongSelf](auto& rectInRootView) -> WebCore::FloatRect {
#if PLATFORM(IOS_FAMILY)
            if (RetainPtr contentView = strongSelf ? strongSelf->_contentView : nil)
                return { [strongSelf convertRect:rectInRootView fromView:contentView.get()] };
#endif
            return rectInRootView;
        });
        completionHandler(rootItem.get());
    }];
#endif // USE(APPLE_INTERNAL_SDK) || (!PLATFORM(WATCHOS) && !PLATFORM(APPLETV))
}

- (void)_describeInteraction:(_WKTextExtractionInteraction *)wkInteraction completionHandler:(void (^)(NSString *, NSError *))completionHandler
{
#if USE(APPLE_INTERNAL_SDK) || (!PLATFORM(WATCHOS) && !PLATFORM(APPLETV))
    if (!self._isValid)
        return completionHandler(nil, [NSError errorWithDomain:WKErrorDomain code:WKErrorWebViewInvalidated userInfo:nil]);

    RefPtr page = _page;
    if (!protect(page->preferences())->textExtractionEnabled())
        return completionHandler(nil, [NSError errorWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:nil]);

    auto nodeIdentifierString = wkInteraction.elementHandle ? emptyString() : String { wkInteraction.nodeIdentifier };
    auto conversionResult = [self _convertToWebCoreInteraction:wkInteraction nodeIdentifier:nodeIdentifierString];
    if (!conversionResult)
        return completionHandler(nil, [NSError errorWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:@{ NSDebugDescriptionErrorKey: conversionResult.error().get() }]);

    auto& [targetFrame, interaction] = *conversionResult;
    if (!targetFrame)
        return completionHandler(nil, [NSError errorWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:nil]);

#if PLATFORM(MAC)
    if ([self _activePopupButtonCell] && interaction.action == WebCore::TextExtraction::Action::SelectMenuItem && !interaction.text.isEmpty())
        return completionHandler([NSString stringWithFormat:@"Select popup menu item labeled '%s'", interaction.text.utf8().data()], nil);
#endif

    [self _describeInteraction:WTF::move(interaction) inFrame:targetFrame nodeIdentifier:nodeIdentifierString staleNodeNote:emptyString() shouldResolveStaleNodeIdentifier:YES completionHandler:completionHandler];
#endif // USE(APPLE_INTERNAL_SDK) || (!PLATFORM(WATCHOS) && !PLATFORM(APPLETV))
}

- (void)_describeInteraction:(WebCore::TextExtraction::Interaction)interaction inFrame:(RefPtr<WebKit::WebFrameProxy>)targetFrame nodeIdentifier:(const String&)attemptedIdentifier staleNodeNote:(const String&)staleNodeNote shouldResolveStaleNodeIdentifier:(BOOL)shouldResolveStaleNodeIdentifier completionHandler:(void (^)(NSString *, NSError *))completionHandler
{
#if USE(APPLE_INTERNAL_SDK) || (!PLATFORM(WATCHOS) && !PLATFORM(APPLETV))
    RefPtr page = _page;
    if (!page || !targetFrame)
        return completionHandler(nil, [NSError errorWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:nil]);

    auto interactionForRetry = interaction;
    targetFrame->describeTextExtractionInteraction(WTF::move(interaction), [
        weakSelf = WeakObjCPtr<WKWebView>(self),
        weakPage = WeakPtr { *page },
        attemptedIdentifier,
        staleNodeNote,
        shouldResolveStaleNodeIdentifier,
        interaction = WTF::move(interactionForRetry),
        completionHandler = makeBlockPtr(WTF::move(completionHandler))
    ](auto&& result) mutable {
        RetainPtr strongSelf = weakSelf.get();
        RefPtr strongPage = weakPage.get();

        if (!result.didFindTargetNode && shouldResolveStaleNodeIdentifier && strongSelf && strongPage && !attemptedIdentifier.isEmpty()) {
            auto resolved = strongPage->textExtractionCache().resolve(attemptedIdentifier);
            if (resolved.resolution == WebKit::TextExtractionCache::NodeResolution::Remapped) {
                RELEASE_LOG(TextExtraction, "<%@: %p> Describe target missing; re-resolved stale node %" PUBLIC_LOG_STRING " to %" PUBLIC_LOG_STRING " and retrying", [strongSelf class], strongSelf.get(), attemptedIdentifier.utf8().data(), resolved.identifier.utf8().data());
                auto note = makeString("Note: the targeted node (uid="_s, attemptedIdentifier, ") was stale from an earlier page state and was automatically re-resolved to the current matching element."_s);
                RefPtr<WebKit::WebFrameProxy> retryFrame;
                if (auto identifiers = WebKit::parseExtractedNodeInfo(resolved.identifier)) {
                    interaction.nodeIdentifier = { WTF::move(identifiers->nodeIdentifier) };
                    retryFrame = WebKit::WebFrameProxy::webFrame(WTF::move(identifiers->frameIdentifier));
                }
                if (!retryFrame)
                    retryFrame = strongPage->mainFrame();
                [strongSelf _describeInteraction:WTF::move(interaction) inFrame:WTF::move(retryFrame) nodeIdentifier:resolved.identifier staleNodeNote:note shouldResolveStaleNodeIdentifier:NO completionHandler:completionHandler.get()];
                return;
            }
        }

        auto description = WTF::move(result.description);
        auto stringsToValidate = WTF::move(result.stringsToValidate);
        auto valid = Box<bool>::create(true);
        Ref aggregator = MainRunLoopCallbackAggregator::create([completionHandler = WTF::move(completionHandler), description, valid, staleNodeNote, weakSelf, stringsToValidate] {
            if (!valid.get()) {
                completionHandler(nil, [NSError errorWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:@{
                    NSDebugDescriptionErrorKey: @"One or more strings failed validation."
                }]);
                return;
            }

            String replacedDescription = description;
            if (RetainPtr strongSelf = weakSelf.get(); strongSelf && !strongSelf->_lastTextExtractionReplacementStrings.isEmpty()) {
                for (auto& string : stringsToValidate) {
                    auto replaced = WebKit::applyReplacements(string, strongSelf->_lastTextExtractionReplacementStrings);
                    if (replaced != string)
                        replacedDescription = makeStringByReplacingAll(replacedDescription, string, replaced);
                }
            }

            RetainPtr summary = replacedDescription.createNSString();
            if (!staleNodeNote.isEmpty())
                summary = adoptNS([[NSString alloc] initWithFormat:@"%@ %@", summary.get(), staleNodeNote.createNSString().get()]);
            completionHandler(summary, nil);
        });

        if (!strongPage || !protect(strongPage->preferences())->textExtractionFilterEnabled())
            return;

#if ENABLE(TEXT_EXTRACTION_FILTER)
        for (auto& string : stringsToValidate) {
            WebKit::TextExtractionFilter::singleton().shouldFilter(string, [aggregator = aggregator.copyRef(), valid](bool result) {
                if (result)
                    *valid = false;
            });
        }
#endif // ENABLE(TEXT_EXTRACTION_FILTER)
    });
#endif // USE(APPLE_INTERNAL_SDK) || (!PLATFORM(WATCHOS) && !PLATFORM(APPLETV))
}

#if ENABLE(SYSTEM_TEXT_EXTRACTION)

- (NSUUID *)_textExtractionIdentifier
{
    if (!_textExtractionIdentifier)
        _textExtractionIdentifier = WTF::UUID::createVersion4Weak();
    return _textExtractionIdentifier->createNSUUID().autorelease();
}

#endif

#if ENABLE(TEXT_EXTRACTION_FILTER)

- (void)_validateText:(const String&)text inFrame:(std::optional<WebCore::FrameIdentifier>&&)frameIdentifier inNode:(std::optional<WebCore::NodeIdentifier>&&)nodeIdentifier completionHandler:(CompletionHandler<void(const String&)>&&)completionHandler
{
    if (text.isEmpty())
        return completionHandler(text);

    auto textHash = text.hash();
    if (auto cachedResult = _textValidationCache.getOptional(textHash)) {
        return WTF::switchOn(*cachedResult, [&](const String& stringResult) {
            completionHandler(stringResult);
        }, [&](SimilarToOriginalTextTag) {
            completionHandler(text);
        });
    }

    RefPtr mainFrame = _page->mainFrame();
    if (!mainFrame)
        return completionHandler(text);

    if (_textExtractionRecognizedWords) {
        __block unsigned totalWords = 0;
        __block unsigned matchedWords = 0;
        [text.createNSString() enumerateSubstringsInRange:NSMakeRange(0, text.length()) options:NSStringEnumerationByWords usingBlock:^(NSString *word, NSRange, NSRange, BOOL *) {
            if (!word.length)
                return;

            totalWords += 1;
            if (_textExtractionRecognizedWords->contains(WebCore::foldQuoteMarks(word.lowercaseString)))
                matchedWords += 1;
        }];

        if (!totalWords)
            return completionHandler(text);

        static constexpr auto minimumMatchRatio = 0.9;
        if (static_cast<double>(matchedWords) / totalWords >= minimumMatchRatio) {
            RELEASE_LOG_INFO(TextExtraction, "<%@: %p> • Skipping text recognition for paragraph with length: %u", [self class], self, text.length());
            _textValidationCache.add(textHash, TextValidationMapValue { SimilarToOriginalTextTag::Value });
            return completionHandler(text);
        }
    }

    RELEASE_LOG_INFO(TextExtraction, "<%@: %p> • Snapshotting paragraph with length: %u", [self class], self, text.length());
    mainFrame->takeSnapshotOfExtractedText({ text, WTF::move(nodeIdentifier) }, [text = text, completionHandler = WTF::move(completionHandler), view = retainPtr(self), textHash](auto textIndicator) mutable {
        if (!textIndicator)
            return completionHandler(text);

        RefPtr contentImage = textIndicator->contentImage();
        if (!contentImage)
            return completionHandler(text);

        RefPtr nativeImage = contentImage->nativeImage();
        if (!nativeImage)
            return completionHandler(text);

        RetainPtr cgImage = nativeImage->platformImage();
        if (!cgImage)
            return completionHandler(text);

        WebKit::recognizeText(cgImage.get(), WebKit::TextRecognitionLevel::Accurate, [text = WTF::move(text), completionHandler = WTF::move(completionHandler), view = WTF::move(view), textHash](NSString *recognizedText, NSError *error) mutable {
            if (error)
                return completionHandler(text);

            // FIXME: This similarity threshold seems low, but in practice, dense but visible text sometimes
            // gets partially ignored in text recognition results. In the future, we could consider raising
            // this threshold if we get a more reliable way to recognize dense text.
            static constexpr auto minimumSimilarity = 0.5;
            static constexpr auto minimumLength = 10;

            auto similarity = WebKit::computeSimilarity(text.createNSString().get(), recognizedText, minimumLength);
            if (similarity < minimumSimilarity) {
                view->_textValidationCache.add(textHash, TextValidationMapValue { String { recognizedText } });
                completionHandler(recognizedText);
            } else {
                view->_textValidationCache.add(textHash, TextValidationMapValue { SimilarToOriginalTextTag::Value });
                completionHandler(text);
            }
        });
    });
}

- (void)_clearTextExtractionFilterCache
{
    if (!self.window)
        return;

    if (RefPtr filter = WebKit::TextExtractionFilter::singletonIfCreated())
        filter->resetCache();

    if (RefPtr cache = _textExtractionURLCache)
        cache->clear();

    _textValidationCache.clear();
    _textExtractionRecognizedWords = { };
}

#endif // ENABLE(TEXT_EXTRACTION_FILTER)

- (void)_requestJSHandleForNodeIdentifier:(NSString *)nodeIdentifierString searchText:(NSString *)searchText completionHandler:(void (^)(_WKJSHandle *))completion
{
    auto identifiers = WebKit::parseExtractedNodeInfo(String { nodeIdentifierString });
    if (!identifiers && !searchText.length)
        return completion(nil);

    RefPtr targetFrame = _page->mainFrame();
    std::optional<WebCore::NodeIdentifier> nodeIdentifier;
    if (identifiers) {
        nodeIdentifier = identifiers->nodeIdentifier;
        if (identifiers->frameIdentifier)
            targetFrame = WebKit::WebFrameProxy::webFrame(identifiers->frameIdentifier);
    }

    if (!targetFrame)
        return completion(nil);

    targetFrame->requestJSHandleForExtractedText({ String { searchText }, WTF::move(nodeIdentifier) }, [completion = makeBlockPtr(completion)](auto&& info) {
        completion(info ? wrapper(API::JSHandle::create(WTF::move(*info))).get() : nil);
    });
}

- (void)_requestContainerJSHandleForNodeIdentifier:(NSString *)nodeIdentifierString searchText:(NSString *)searchText completionHandler:(void (^)(_WKJSHandle *))completion
{
    auto identifiers = WebKit::parseExtractedNodeInfo(String { nodeIdentifierString });
    if (!identifiers && !searchText.length)
        return completion(nil);

    RefPtr targetFrame = _page->mainFrame();
    std::optional<WebCore::NodeIdentifier> nodeIdentifier;
    if (identifiers) {
        nodeIdentifier = identifiers->nodeIdentifier;
        if (identifiers->frameIdentifier)
            targetFrame = WebKit::WebFrameProxy::webFrame(identifiers->frameIdentifier);
    }

    if (!targetFrame)
        return completion(nil);

    targetFrame->requestContainerJSHandleForExtractedText({ searchText, WTF::move(nodeIdentifier) }, [completion = makeBlockPtr(completion)](auto&& info) {
        completion(info ? wrapper(API::JSHandle::create(WTF::move(*info))).get() : nil);
    });
}

- (void)_requestContainerJSHandleForSearchTexts:(NSArray<NSString *> *)texts nodeIdentifier:(NSString *)nodeIdentifierString completionHandler:(void (^)(_WKJSHandle *))completion
{
    if (!texts.count && !nodeIdentifierString.length)
        return completion(nil);

    RefPtr targetFrame = _page->mainFrame();
    std::optional<WebCore::NodeIdentifier> targetNodeIdentifier;
    if (auto identifiers = WebKit::parseExtractedNodeInfo(String { nodeIdentifierString })) {
        targetNodeIdentifier = identifiers->nodeIdentifier;
        if (identifiers->frameIdentifier)
            targetFrame = WebKit::WebFrameProxy::webFrame(identifiers->frameIdentifier);
    }

    if (!targetFrame)
        return completion(nil);

    auto searchTexts = makeVector<String>(texts);
    targetFrame->requestContainerJSHandleForSearchTexts(WTF::move(searchTexts), WTF::move(targetNodeIdentifier), [completion = makeBlockPtr(completion)](auto&& info) {
        completion(info ? wrapper(API::JSHandle::create(WTF::move(*info))).get() : nil);
    });
}

@end
