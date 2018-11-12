/*
 * Copyright (C) 2006-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "Page.h"

#include "ActivityStateChangeObserver.h"
#include "AlternativeTextClient.h"
#include "ApplicationCacheStorage.h"
#include "ApplicationStateChangeListener.h"
#include "AuthenticatorCoordinator.h"
#include "BackForwardClient.h"
#include "BackForwardController.h"
#include "CSSAnimationController.h"
#include "CacheStorageProvider.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "ConstantPropertyMap.h"
#include "ContextMenuClient.h"
#include "ContextMenuController.h"
#include "DOMRect.h"
#include "DOMRectList.h"
#include "DatabaseProvider.h"
#include "DiagnosticLoggingClient.h"
#include "DiagnosticLoggingKeys.h"
#include "DocumentLoader.h"
#include "DocumentMarkerController.h"
#include "DocumentTimeline.h"
#include "DragController.h"
#include "Editor.h"
#include "EditorClient.h"
#include "EmptyClients.h"
#include "Event.h"
#include "EventNames.h"
#include "ExtensionStyleSheets.h"
#include "FileSystem.h"
#include "FocusController.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameSelection.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLElement.h"
#include "HTMLMediaElement.h"
#include "HistoryController.h"
#include "HistoryItem.h"
#include "InspectorController.h"
#include "InspectorInstrumentation.h"
#include "LibWebRTCProvider.h"
#include "LoaderStrategy.h"
#include "Logging.h"
#include "LowPowerModeNotifier.h"
#include "MediaCanStartListener.h"
#include "Navigator.h"
#include "PageCache.h"
#include "PageConfiguration.h"
#include "PageConsoleClient.h"
#include "PageDebuggable.h"
#include "PageGroup.h"
#include "PageOverlayController.h"
#include "PaymentCoordinator.h"
#include "PerformanceLogging.h"
#include "PerformanceLoggingClient.h"
#include "PerformanceMonitor.h"
#include "PlatformMediaSessionManager.h"
#include "PlatformStrategies.h"
#include "PlugInClient.h"
#include "PluginData.h"
#include "PluginInfoProvider.h"
#include "PluginViewBase.h"
#include "PointerLockController.h"
#include "ProgressTracker.h"
#include "PublicSuffix.h"
#include "RenderLayerCompositor.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "ResourceUsageOverlay.h"
#include "RuntimeEnabledFeatures.h"
#include "SVGDocumentExtensions.h"
#include "SchemeRegistry.h"
#include "ScriptController.h"
#include "ScriptedAnimationController.h"
#include "ScrollLatchingState.h"
#include "ScrollingCoordinator.h"
#include "Settings.h"
#include "SharedBuffer.h"
#include "SocketProvider.h"
#include "StorageArea.h"
#include "StorageNamespace.h"
#include "StorageNamespaceProvider.h"
#include "StyleResolver.h"
#include "StyleScope.h"
#include "SubframeLoader.h"
#include "TextResourceDecoder.h"
#include "UserContentProvider.h"
#include "UserInputBridge.h"
#include "ValidationMessageClient.h"
#include "VisitedLinkState.h"
#include "VisitedLinkStore.h"
#include "VoidCallback.h"
#include "WebGLStateTracker.h"
#include "WheelEventDeltaFilter.h"
#include "Widget.h"
#include <wtf/RefCountedLeakCounter.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/Base64.h>
#include <wtf/text/StringHash.h>

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
#include "HTMLVideoElement.h"
#include "MediaPlaybackTarget.h"
#endif

#if PLATFORM(MAC)
#include "ServicesOverlayController.h"
#endif

#if ENABLE(MEDIA_SESSION)
#include "MediaSessionManager.h"
#endif

#if ENABLE(INDEXED_DATABASE)
#include "IDBConnectionToServer.h"
#include "InProcessIDBServer.h"
#endif

#if ENABLE(DATA_INTERACTION)
#include "SelectionRect.h"
#endif

namespace WebCore {

static HashSet<Page*>& allPages()
{
    static NeverDestroyed<HashSet<Page*>> set;
    return set;
}

static unsigned nonUtilityPageCount { 0 };

static inline bool isUtilityPageChromeClient(ChromeClient& chromeClient)
{
    return chromeClient.isEmptyChromeClient() || chromeClient.isSVGImageChromeClient();
}

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, pageCounter, ("Page"));

void Page::forEachPage(const WTF::Function<void(Page&)>& function)
{
    for (auto* page : allPages())
        function(*page);
}

void Page::updateValidationBubbleStateIfNeeded()
{
    if (auto* client = validationMessageClient())
        client->updateValidationBubbleStateIfNeeded();
}

static void networkStateChanged(bool isOnLine)
{
    Vector<Ref<Frame>> frames;

    // Get all the frames of all the pages in all the page groups
    for (auto* page : allPages()) {
        for (Frame* frame = &page->mainFrame(); frame; frame = frame->tree().traverseNext())
            frames.append(*frame);
        InspectorInstrumentation::networkStateChanged(*page);
    }

    auto& eventName = isOnLine ? eventNames().onlineEvent : eventNames().offlineEvent;
    for (auto& frame : frames) {
        if (!frame->document())
            continue;
        frame->document()->dispatchWindowEvent(Event::create(eventName, Event::CanBubble::No, Event::IsCancelable::No));
    }
}

static constexpr OptionSet<ActivityState::Flag> pageInitialActivityState()
{
    return { ActivityState::IsVisible, ActivityState::IsInWindow };
}

Page::Page(PageConfiguration&& pageConfiguration)
    : m_chrome(std::make_unique<Chrome>(*this, *pageConfiguration.chromeClient))
    , m_dragCaretController(std::make_unique<DragCaretController>())
#if ENABLE(DRAG_SUPPORT)
    , m_dragController(std::make_unique<DragController>(*this, *pageConfiguration.dragClient))
#endif
    , m_focusController(std::make_unique<FocusController>(*this, pageInitialActivityState()))
#if ENABLE(CONTEXT_MENUS)
    , m_contextMenuController(std::make_unique<ContextMenuController>(*this, *pageConfiguration.contextMenuClient))
#endif
    , m_userInputBridge(std::make_unique<UserInputBridge>(*this))
    , m_inspectorController(std::make_unique<InspectorController>(*this, pageConfiguration.inspectorClient))
#if ENABLE(POINTER_LOCK)
    , m_pointerLockController(std::make_unique<PointerLockController>(*this))
#endif
    , m_settings(Settings::create(this))
    , m_progress(std::make_unique<ProgressTracker>(*pageConfiguration.progressTrackerClient))
    , m_backForwardController(std::make_unique<BackForwardController>(*this, WTFMove(pageConfiguration.backForwardClient)))
    , m_mainFrame(Frame::create(this, nullptr, pageConfiguration.loaderClientForMainFrame))
    , m_editorClient(WTFMove(pageConfiguration.editorClient))
    , m_plugInClient(pageConfiguration.plugInClient)
    , m_validationMessageClient(WTFMove(pageConfiguration.validationMessageClient))
    , m_diagnosticLoggingClient(WTFMove(pageConfiguration.diagnosticLoggingClient))
    , m_performanceLoggingClient(WTFMove(pageConfiguration.performanceLoggingClient))
    , m_webGLStateTracker(WTFMove(pageConfiguration.webGLStateTracker))
    , m_libWebRTCProvider(WTFMove(pageConfiguration.libWebRTCProvider))
    , m_verticalScrollElasticity(ScrollElasticityAllowed)
    , m_horizontalScrollElasticity(ScrollElasticityAllowed)
    , m_domTimerAlignmentInterval(DOMTimer::defaultAlignmentInterval())
    , m_domTimerAlignmentIntervalIncreaseTimer(*this, &Page::domTimerAlignmentIntervalIncreaseTimerFired)
    , m_activityState(pageInitialActivityState())
    , m_alternativeTextClient(pageConfiguration.alternativeTextClient)
    , m_consoleClient(std::make_unique<PageConsoleClient>(*this))
#if ENABLE(REMOTE_INSPECTOR)
    , m_inspectorDebuggable(std::make_unique<PageDebuggable>(*this))
#endif
    , m_socketProvider(WTFMove(pageConfiguration.socketProvider))
    , m_applicationCacheStorage(*WTFMove(pageConfiguration.applicationCacheStorage))
    , m_cacheStorageProvider(WTFMove(pageConfiguration.cacheStorageProvider))
    , m_databaseProvider(*WTFMove(pageConfiguration.databaseProvider))
    , m_pluginInfoProvider(*WTFMove(pageConfiguration.pluginInfoProvider))
    , m_storageNamespaceProvider(*WTFMove(pageConfiguration.storageNamespaceProvider))
    , m_userContentProvider(*WTFMove(pageConfiguration.userContentProvider))
    , m_visitedLinkStore(*WTFMove(pageConfiguration.visitedLinkStore))
#if ENABLE(INTERSECTION_OBSERVER)
    , m_intersectionObservationUpdateTimer(*this, &Page::updateIntersectionObservations)
#endif
    , m_sessionID(PAL::SessionID::defaultSessionID())
#if ENABLE(VIDEO)
    , m_playbackControlsManagerUpdateTimer(*this, &Page::playbackControlsManagerUpdateTimerFired)
#endif
    , m_isUtilityPage(isUtilityPageChromeClient(chrome().client()))
    , m_performanceMonitor(isUtilityPage() ? nullptr : std::make_unique<PerformanceMonitor>(*this))
    , m_lowPowerModeNotifier(std::make_unique<LowPowerModeNotifier>([this](bool isLowPowerModeEnabled) { handleLowModePowerChange(isLowPowerModeEnabled); }))
    , m_performanceLogging(std::make_unique<PerformanceLogging>(*this))
#if PLATFORM(MAC) && (ENABLE(SERVICE_CONTROLS) || ENABLE(TELEPHONE_NUMBER_DETECTION))
    , m_servicesOverlayController(std::make_unique<ServicesOverlayController>(*this))
#endif
    , m_recentWheelEventDeltaFilter(WheelEventDeltaFilter::create())
    , m_pageOverlayController(std::make_unique<PageOverlayController>(*this))
#if ENABLE(APPLE_PAY)
    , m_paymentCoordinator(std::make_unique<PaymentCoordinator>(*pageConfiguration.paymentCoordinatorClient))
#endif
#if ENABLE(WEB_AUTHN)
    , m_authenticatorCoordinator(makeUniqueRef<AuthenticatorCoordinator>(WTFMove(pageConfiguration.authenticatorCoordinatorClient)))
#endif
#if ENABLE(APPLICATION_MANIFEST)
    , m_applicationManifest(pageConfiguration.applicationManifest)
#endif
{
    updateTimerThrottlingState();

    m_pluginInfoProvider->addPage(*this);
    m_storageNamespaceProvider->addPage(*this);
    m_userContentProvider->addPage(*this);
    m_visitedLinkStore->addPage(*this);

    static bool addedListener;
    if (!addedListener) {
        platformStrategies()->loaderStrategy()->addOnlineStateChangeListener(&networkStateChanged);
        addedListener = true;
    }

    ASSERT(!allPages().contains(this));
    allPages().add(this);

    if (!isUtilityPage()) {
        ++nonUtilityPageCount;
        MemoryPressureHandler::setPageCount(nonUtilityPageCount);
    }

#ifndef NDEBUG
    pageCounter.increment();
#endif

#if ENABLE(REMOTE_INSPECTOR)
    m_inspectorDebuggable->init();
#endif

#if PLATFORM(COCOA)
    platformInitialize();
#endif
}

Page::~Page()
{
    ASSERT(!m_nestedRunLoopCount);
    ASSERT(!m_unnestCallback);

    m_validationMessageClient = nullptr;
    m_diagnosticLoggingClient = nullptr;
    m_performanceLoggingClient = nullptr;
    m_mainFrame->setView(nullptr);
    setGroupName(String());
    allPages().remove(this);
    if (!isUtilityPage()) {
        --nonUtilityPageCount;
        MemoryPressureHandler::setPageCount(nonUtilityPageCount);
    }
    
    m_settings->pageDestroyed();

    m_inspectorController->inspectedPageDestroyed();

    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        frame->willDetachPage();
        frame->detachFromPage();
    }

    if (m_plugInClient)
        m_plugInClient->pageDestroyed();
    if (m_alternativeTextClient)
        m_alternativeTextClient->pageDestroyed();

    if (m_scrollingCoordinator)
        m_scrollingCoordinator->pageDestroyed();

    backForward().close();
    if (!isUtilityPage())
        PageCache::singleton().removeAllItemsForPage(*this);

#ifndef NDEBUG
    pageCounter.decrement();
#endif

    m_pluginInfoProvider->removePage(*this);
    m_storageNamespaceProvider->removePage(*this);
    m_userContentProvider->removePage(*this);
    m_visitedLinkStore->removePage(*this);
}

void Page::clearPreviousItemFromAllPages(HistoryItem* item)
{
    for (auto* page : allPages()) {
        auto& controller = page->mainFrame().loader().history();
        if (item == controller.previousItem()) {
            controller.clearPreviousItem();
            return;
        }
    }
}

uint64_t Page::renderTreeSize() const
{
    uint64_t total = 0;
    for (const Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (!frame->document() || !frame->document()->renderView())
            continue;
        total += frame->document()->renderView()->rendererCount();
    }
    return total;
}

OptionSet<DisabledAdaptations> Page::disabledAdaptations() const
{
    if (mainFrame().document())
        return mainFrame().document()->disabledAdaptations();

    return { };
}

ViewportArguments Page::viewportArguments() const
{
    return mainFrame().document() ? mainFrame().document()->viewportArguments() : ViewportArguments();
}

ScrollingCoordinator* Page::scrollingCoordinator()
{
    if (!m_scrollingCoordinator && m_settings->scrollingCoordinatorEnabled()) {
        m_scrollingCoordinator = chrome().client().createScrollingCoordinator(*this);
        if (!m_scrollingCoordinator)
            m_scrollingCoordinator = ScrollingCoordinator::create(this);
    }

    return m_scrollingCoordinator.get();
}

String Page::scrollingStateTreeAsText()
{
    if (Document* document = m_mainFrame->document())
        document->updateLayout();

    if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
        return scrollingCoordinator->scrollingStateTreeAsText();

    return String();
}

String Page::synchronousScrollingReasonsAsText()
{
    if (Document* document = m_mainFrame->document())
        document->updateLayout();

    if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
        return scrollingCoordinator->synchronousScrollingReasonsAsText();

    return String();
}

Ref<DOMRectList> Page::nonFastScrollableRects()
{
    if (Document* document = m_mainFrame->document())
        document->updateLayout();

    Vector<IntRect> rects;
    if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator()) {
        const EventTrackingRegions& eventTrackingRegions = scrollingCoordinator->absoluteEventTrackingRegions();
        for (const auto& synchronousEventRegion : eventTrackingRegions.eventSpecificSynchronousDispatchRegions)
            rects.appendVector(synchronousEventRegion.value.rects());
    }

    Vector<FloatQuad> quads(rects.size());
    for (size_t i = 0; i < rects.size(); ++i)
        quads[i] = FloatRect(rects[i]);

    return DOMRectList::create(quads);
}

Ref<DOMRectList> Page::touchEventRectsForEvent(const String& eventName)
{
    if (Document* document = m_mainFrame->document()) {
        document->updateLayout();
#if ENABLE(IOS_TOUCH_EVENTS)
        document->updateTouchEventRegions();
#endif
    }

    Vector<IntRect> rects;
    if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator()) {
        const EventTrackingRegions& eventTrackingRegions = scrollingCoordinator->absoluteEventTrackingRegions();
        const auto& region = eventTrackingRegions.eventSpecificSynchronousDispatchRegions.get(eventName);
        rects.appendVector(region.rects());
    }

    Vector<FloatQuad> quads(rects.size());
    for (size_t i = 0; i < rects.size(); ++i)
        quads[i] = FloatRect(rects[i]);

    return DOMRectList::create(quads);
}

Ref<DOMRectList> Page::passiveTouchEventListenerRects()
{
    if (Document* document = m_mainFrame->document()) {
        document->updateLayout();
#if ENABLE(IOS_TOUCH_EVENTS)
        document->updateTouchEventRegions();
#endif  
    }

    Vector<IntRect> rects;
    if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
        rects.appendVector(scrollingCoordinator->absoluteEventTrackingRegions().asynchronousDispatchRegion.rects());

    Vector<FloatQuad> quads(rects.size());
    for (size_t i = 0; i < rects.size(); ++i)
        quads[i] = FloatRect(rects[i]);

    return DOMRectList::create(quads);
}

bool Page::openedByDOM() const
{
    return m_openedByDOM;
}

void Page::setOpenedByDOM()
{
    m_openedByDOM = true;
}

void Page::goToItem(HistoryItem& item, FrameLoadType type, ShouldTreatAsContinuingLoad shouldTreatAsContinuingLoad)
{
    // stopAllLoaders may end up running onload handlers, which could cause further history traversals that may lead to the passed in HistoryItem
    // being deref()-ed. Make sure we can still use it with HistoryController::goToItem later.
    Ref<HistoryItem> protector(item);

    auto& frameLoader = m_mainFrame->loader();
    if (frameLoader.history().shouldStopLoadingForHistoryItem(item))
        m_mainFrame->loader().stopAllLoadersAndCheckCompleteness();

    m_mainFrame->loader().history().goToItem(item, type, shouldTreatAsContinuingLoad);
}

void Page::setGroupName(const String& name)
{
    if (m_group && !m_group->name().isEmpty()) {
        ASSERT(m_group != m_singlePageGroup.get());
        ASSERT(!m_singlePageGroup);
        m_group->removePage(*this);
    }

    if (name.isEmpty())
        m_group = m_singlePageGroup.get();
    else {
        m_singlePageGroup = nullptr;
        m_group = PageGroup::pageGroup(name);
        m_group->addPage(*this);
    }
}

const String& Page::groupName() const
{
    return m_group ? m_group->name() : nullAtom().string();
}

void Page::initGroup()
{
    ASSERT(!m_singlePageGroup);
    ASSERT(!m_group);
    m_singlePageGroup = std::make_unique<PageGroup>(*this);
    m_group = m_singlePageGroup.get();
}

void Page::updateStyleAfterChangeInEnvironment()
{
    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        // If a change in the global environment has occurred, we need to
        // make sure all the properties a recomputed, therefore we invalidate
        // the properties cache.
        auto* document = frame->document();
        if (!document)
            continue;

        if (StyleResolver* styleResolver = document->styleScope().resolverIfExists())
            styleResolver->invalidateMatchedPropertiesCache();
        document->scheduleForcedStyleRecalc();
        document->styleScope().didChangeStyleSheetEnvironment();
    }
}

void Page::updateStyleForAllPagesAfterGlobalChangeInEnvironment()
{
    for (auto* page : allPages())
        page->updateStyleAfterChangeInEnvironment();
}

void Page::setNeedsRecalcStyleInAllFrames()
{
    // FIXME: Figure out what this function is actually trying to add in different call sites.
    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (Document* document = frame->document())
            document->styleScope().didChangeStyleSheetEnvironment();
    }
}

void Page::refreshPlugins(bool reload)
{
    HashSet<PluginInfoProvider*> pluginInfoProviders;

    for (auto* page : allPages())
        pluginInfoProviders.add(&page->pluginInfoProvider());

    for (auto& pluginInfoProvider : pluginInfoProviders)
        pluginInfoProvider->refresh(reload);
}

PluginData& Page::pluginData()
{
    if (!m_pluginData)
        m_pluginData = PluginData::create(*this);
    return *m_pluginData;
}

void Page::clearPluginData()
{
    m_pluginData = nullptr;
}

bool Page::showAllPlugins() const
{
    if (m_showAllPlugins)
        return true;

    if (Document* document = mainFrame().document())
        return document->securityOrigin().isLocal();

    return false;
}

inline std::optional<std::pair<MediaCanStartListener&, Document&>>  Page::takeAnyMediaCanStartListener()
{
    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (!frame->document())
            continue;
        if (MediaCanStartListener* listener = frame->document()->takeAnyMediaCanStartListener())
            return { { *listener, *frame->document() } };
    }
    return std::nullopt;
}

void Page::setCanStartMedia(bool canStartMedia)
{
    if (m_canStartMedia == canStartMedia)
        return;

    m_canStartMedia = canStartMedia;

    while (m_canStartMedia) {
        auto listener = takeAnyMediaCanStartListener();
        if (!listener)
            break;
        listener->first.mediaCanStart(listener->second);
    }
}

static Frame* incrementFrame(Frame* curr, bool forward, CanWrap canWrap, DidWrap* didWrap = nullptr)
{
    return forward
        ? curr->tree().traverseNext(canWrap, didWrap)
        : curr->tree().traversePrevious(canWrap, didWrap);
}

bool Page::findString(const String& target, FindOptions options, DidWrap* didWrap)
{
    if (target.isEmpty())
        return false;

    CanWrap canWrap = options.contains(WrapAround) ? CanWrap::Yes : CanWrap::No;
    Frame* frame = &focusController().focusedOrMainFrame();
    Frame* startFrame = frame;
    do {
        if (frame->editor().findString(target, (options - WrapAround) | StartInSelection)) {
            if (frame != startFrame)
                startFrame->selection().clear();
            focusController().setFocusedFrame(frame);
            return true;
        }
        frame = incrementFrame(frame, !options.contains(Backwards), canWrap, didWrap);
    } while (frame && frame != startFrame);

    // Search contents of startFrame, on the other side of the selection that we did earlier.
    // We cheat a bit and just research with wrap on
    if (canWrap == CanWrap::Yes && !startFrame->selection().isNone()) {
        if (didWrap)
            *didWrap = DidWrap::Yes;
        bool found = startFrame->editor().findString(target, options | WrapAround | StartInSelection);
        focusController().setFocusedFrame(frame);
        return found;
    }

    return false;
}

void Page::findStringMatchingRanges(const String& target, FindOptions options, int limit, Vector<RefPtr<Range>>& matchRanges, int& indexForSelection)
{
    indexForSelection = 0;

    Frame* frame = &mainFrame();
    Frame* frameWithSelection = nullptr;
    do {
        frame->editor().countMatchesForText(target, 0, options, limit ? (limit - matchRanges.size()) : 0, true, &matchRanges);
        if (frame->selection().isRange())
            frameWithSelection = frame;
        frame = incrementFrame(frame, true, CanWrap::No);
    } while (frame);

    if (matchRanges.isEmpty())
        return;

    if (frameWithSelection) {
        indexForSelection = NoMatchAfterUserSelection;
        RefPtr<Range> selectedRange = frameWithSelection->selection().selection().firstRange();
        if (options.contains(Backwards)) {
            for (size_t i = matchRanges.size(); i > 0; --i) {
                auto result = selectedRange->compareBoundaryPoints(Range::END_TO_START, *matchRanges[i - 1]);
                if (!result.hasException() && result.releaseReturnValue() > 0) {
                    indexForSelection = i - 1;
                    break;
                }
            }
        } else {
            for (size_t i = 0, size = matchRanges.size(); i < size; ++i) {
                auto result = selectedRange->compareBoundaryPoints(Range::START_TO_END, *matchRanges[i]);
                if (!result.hasException() && result.releaseReturnValue() < 0) {
                    indexForSelection = i;
                    break;
                }
            }
        }
    } else {
        if (options.contains(Backwards))
            indexForSelection = matchRanges.size() - 1;
        else
            indexForSelection = 0;
    }
}

RefPtr<Range> Page::rangeOfString(const String& target, Range* referenceRange, FindOptions options)
{
    if (target.isEmpty())
        return nullptr;

    if (referenceRange && referenceRange->ownerDocument().page() != this)
        return nullptr;

    CanWrap canWrap = options.contains(WrapAround) ? CanWrap::Yes : CanWrap::No;
    Frame* frame = referenceRange ? referenceRange->ownerDocument().frame() : &mainFrame();
    Frame* startFrame = frame;
    do {
        if (RefPtr<Range> resultRange = frame->editor().rangeOfString(target, frame == startFrame ? referenceRange : 0, options - WrapAround))
            return resultRange;

        frame = incrementFrame(frame, !options.contains(Backwards), canWrap);
    } while (frame && frame != startFrame);

    // Search contents of startFrame, on the other side of the reference range that we did earlier.
    // We cheat a bit and just search again with wrap on.
    if (canWrap == CanWrap::Yes && referenceRange) {
        if (RefPtr<Range> resultRange = startFrame->editor().rangeOfString(target, referenceRange, options | WrapAround | StartInSelection))
            return resultRange;
    }

    return nullptr;
}

unsigned Page::findMatchesForText(const String& target, FindOptions options, unsigned maxMatchCount, ShouldHighlightMatches shouldHighlightMatches, ShouldMarkMatches shouldMarkMatches)
{
    if (target.isEmpty())
        return 0;

    unsigned matchCount = 0;

    Frame* frame = &mainFrame();
    do {
        if (shouldMarkMatches == MarkMatches)
            frame->editor().setMarkedTextMatchesAreHighlighted(shouldHighlightMatches == HighlightMatches);
        matchCount += frame->editor().countMatchesForText(target, 0, options, maxMatchCount ? (maxMatchCount - matchCount) : 0, shouldMarkMatches == MarkMatches, 0);
        frame = incrementFrame(frame, true, CanWrap::No);
    } while (frame);

    return matchCount;
}

unsigned Page::markAllMatchesForText(const String& target, FindOptions options, bool shouldHighlight, unsigned maxMatchCount)
{
    return findMatchesForText(target, options, maxMatchCount, shouldHighlight ? HighlightMatches : DoNotHighlightMatches, MarkMatches);
}

unsigned Page::countFindMatches(const String& target, FindOptions options, unsigned maxMatchCount)
{
    return findMatchesForText(target, options, maxMatchCount, DoNotHighlightMatches, DoNotMarkMatches);
}

void Page::unmarkAllTextMatches()
{
    Frame* frame = &mainFrame();
    do {
        frame->document()->markers().removeMarkers(DocumentMarker::TextMatch);
        frame = incrementFrame(frame, true, CanWrap::No);
    } while (frame);
}

const VisibleSelection& Page::selection() const
{
    return focusController().focusedOrMainFrame().selection().selection();
}

void Page::setDefersLoading(bool defers)
{
    if (!m_settings->loadDeferringEnabled())
        return;

    if (m_settings->wantsBalancedSetDefersLoadingBehavior()) {
        ASSERT(defers || m_defersLoadingCallCount);
        if (defers && ++m_defersLoadingCallCount > 1)
            return;
        if (!defers && --m_defersLoadingCallCount)
            return;
    } else {
        ASSERT(!m_defersLoadingCallCount);
        if (defers == m_defersLoading)
            return;
    }

    m_defersLoading = defers;
    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext())
        frame->loader().setDefersLoading(defers);
}

void Page::clearUndoRedoOperations()
{
    m_editorClient->clearUndoRedoOperations();
}

bool Page::inLowQualityImageInterpolationMode() const
{
    return m_inLowQualityInterpolationMode;
}

void Page::setInLowQualityImageInterpolationMode(bool mode)
{
    m_inLowQualityInterpolationMode = mode;
}

DiagnosticLoggingClient& Page::diagnosticLoggingClient() const
{
    if (!settings().diagnosticLoggingEnabled() || !m_diagnosticLoggingClient)
        return emptyDiagnosticLoggingClient();
    return *m_diagnosticLoggingClient;
}

void Page::setMediaVolume(float volume)
{
    if (volume < 0 || volume > 1)
        return;

    if (m_mediaVolume == volume)
        return;

    m_mediaVolume = volume;
    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (!frame->document())
            continue;
        frame->document()->mediaVolumeDidChange();
    }
}

void Page::setZoomedOutPageScaleFactor(float scale)
{
    if (m_zoomedOutPageScaleFactor == scale)
        return;
    m_zoomedOutPageScaleFactor = scale;

    mainFrame().deviceOrPageScaleFactorChanged();
}

void Page::setPageScaleFactor(float scale, const IntPoint& origin, bool inStableState)
{
    LOG(Viewports, "Page::setPageScaleFactor %.2f - inStableState %d", scale, inStableState);

    Document* document = mainFrame().document();
    FrameView* view = document->view();

    if (scale == m_pageScaleFactor) {
        if (view && view->scrollPosition() != origin) {
            if (!m_settings->delegatesPageScaling())
                document->updateLayoutIgnorePendingStylesheets();

            if (!view->delegatesScrolling())
                view->setScrollPosition(origin);
#if USE(COORDINATED_GRAPHICS)
            else
                view->requestScrollPositionUpdate(origin);
#endif
        }
#if ENABLE(MEDIA_CONTROLS_SCRIPT)
        if (inStableState) {
            for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
                if (!frame->document())
                    continue;
                frame->document()->pageScaleFactorChangedAndStable();
            }
        }
#endif
        return;
    }

    m_pageScaleFactor = scale;

    if (!m_settings->delegatesPageScaling()) {
        if (auto* renderView = document->renderView()) {
            renderView->setNeedsLayout();
            if (renderView->hasLayer() && renderView->layer()->isComposited())
                renderView->layer()->setNeedsCompositingGeometryUpdate();
        }

        document->resolveStyle(Document::ResolveStyleType::Rebuild);

        // Transform change on RenderView doesn't trigger repaint on non-composited contents.
        mainFrame().view()->invalidateRect(IntRect(LayoutRect::infiniteRect()));
    }

    mainFrame().deviceOrPageScaleFactorChanged();

    if (view && view->fixedElementsLayoutRelativeToFrame())
        view->setViewportConstrainedObjectsNeedLayout();

    if (view && view->scrollPosition() != origin) {
        if (!m_settings->delegatesPageScaling() && document->renderView() && document->renderView()->needsLayout() && view->didFirstLayout())
            view->layoutContext().layout();

        if (!view->delegatesScrolling())
            view->setScrollPosition(origin);
#if USE(COORDINATED_GRAPHICS)
        else
            view->requestScrollPositionUpdate(origin);
#endif
    }

#if ENABLE(MEDIA_CONTROLS_SCRIPT)
    if (inStableState) {
        for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
            if (!frame->document())
                continue;
            frame->document()->pageScaleFactorChangedAndStable();
        }
    }
#else
    UNUSED_PARAM(inStableState);
#endif
}

void Page::setViewScaleFactor(float scale)
{
    if (m_viewScaleFactor == scale)
        return;

    m_viewScaleFactor = scale;
    PageCache::singleton().markPagesForDeviceOrPageScaleChanged(*this);
}

void Page::setDeviceScaleFactor(float scaleFactor)
{
    ASSERT(scaleFactor > 0);
    if (scaleFactor <= 0)
        return;
    
    if (m_deviceScaleFactor == scaleFactor)
        return;

    m_deviceScaleFactor = scaleFactor;
    setNeedsRecalcStyleInAllFrames();

    mainFrame().deviceOrPageScaleFactorChanged();
    PageCache::singleton().markPagesForDeviceOrPageScaleChanged(*this);

    pageOverlayController().didChangeDeviceScaleFactor();
}

void Page::setUserInterfaceLayoutDirection(UserInterfaceLayoutDirection userInterfaceLayoutDirection)
{
    if (m_userInterfaceLayoutDirection == userInterfaceLayoutDirection)
        return;

    m_userInterfaceLayoutDirection = userInterfaceLayoutDirection;
#if ENABLE(MEDIA_CONTROLS_SCRIPT)
    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (!frame->document())
            continue;
        frame->document()->userInterfaceLayoutDirectionChanged();
    }
#endif
}

#if ENABLE(VIDEO)
void Page::updateMediaElementRateChangeRestrictions()
{
    for (auto* mediaElement : HTMLMediaElement::allMediaElements())
        mediaElement->updateRateChangeRestrictions();
}
#endif

void Page::didStartProvisionalLoad()
{
    if (m_performanceMonitor)
        m_performanceMonitor->didStartProvisionalLoad();
}

void Page::didFinishLoad()
{
    resetRelevantPaintedObjectCounter();

    if (m_performanceMonitor)
        m_performanceMonitor->didFinishLoad();
}

void Page::willDisplayPage()
{
#if ENABLE(INTERSECTION_OBSERVER)
    updateIntersectionObservations();
#endif
}

bool Page::isOnlyNonUtilityPage() const
{
    return !isUtilityPage() && nonUtilityPageCount == 1;
}

bool Page::isLowPowerModeEnabled() const
{
    if (m_lowPowerModeEnabledOverrideForTesting)
        return m_lowPowerModeEnabledOverrideForTesting.value();

    return m_lowPowerModeNotifier->isLowPowerModeEnabled();
}

void Page::setLowPowerModeEnabledOverrideForTesting(std::optional<bool> isEnabled)
{
    m_lowPowerModeEnabledOverrideForTesting = isEnabled;
    handleLowModePowerChange(m_lowPowerModeEnabledOverrideForTesting.value_or(false));
}

void Page::setTopContentInset(float contentInset)
{
    if (m_topContentInset == contentInset)
        return;
    
    m_topContentInset = contentInset;
    
    if (FrameView* view = mainFrame().view())
        view->topContentInsetDidChange(m_topContentInset);
}

void Page::setShouldSuppressScrollbarAnimations(bool suppressAnimations)
{
    if (suppressAnimations == m_suppressScrollbarAnimations)
        return;

    lockAllOverlayScrollbarsToHidden(suppressAnimations);
    m_suppressScrollbarAnimations = suppressAnimations;
}

void Page::lockAllOverlayScrollbarsToHidden(bool lockOverlayScrollbars)
{
    FrameView* view = mainFrame().view();
    if (!view)
        return;

    view->lockOverlayScrollbarStateToHidden(lockOverlayScrollbars);
    
    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        FrameView* frameView = frame->view();
        if (!frameView)
            continue;

        const HashSet<ScrollableArea*>* scrollableAreas = frameView->scrollableAreas();
        if (!scrollableAreas)
            continue;

        for (auto& scrollableArea : *scrollableAreas)
            scrollableArea->lockOverlayScrollbarStateToHidden(lockOverlayScrollbars);
    }
}
    
void Page::setVerticalScrollElasticity(ScrollElasticity elasticity)
{
    if (m_verticalScrollElasticity == elasticity)
        return;
    
    m_verticalScrollElasticity = elasticity;
    
    if (FrameView* view = mainFrame().view())
        view->setVerticalScrollElasticity(elasticity);
}
    
void Page::setHorizontalScrollElasticity(ScrollElasticity elasticity)
{
    if (m_horizontalScrollElasticity == elasticity)
        return;
    
    m_horizontalScrollElasticity = elasticity;
    
    if (FrameView* view = mainFrame().view())
        view->setHorizontalScrollElasticity(elasticity);
}

void Page::setPagination(const Pagination& pagination)
{
    if (m_pagination == pagination)
        return;

    m_pagination = pagination;

    setNeedsRecalcStyleInAllFrames();
}

void Page::setPaginationLineGridEnabled(bool enabled)
{
    if (m_paginationLineGridEnabled == enabled)
        return;
    
    m_paginationLineGridEnabled = enabled;
    
    setNeedsRecalcStyleInAllFrames();
}

unsigned Page::pageCount() const
{
    if (m_pagination.mode == Pagination::Unpaginated)
        return 0;

    if (Document* document = mainFrame().document())
        document->updateLayoutIgnorePendingStylesheets();

    RenderView* contentRenderer = mainFrame().contentRenderer();
    return contentRenderer ? contentRenderer->pageCount() : 0;
}

void Page::setIsInWindow(bool isInWindow)
{
    setActivityState(isInWindow ? m_activityState | ActivityState::IsInWindow : m_activityState - ActivityState::IsInWindow);
}

void Page::setIsInWindowInternal(bool isInWindow)
{
    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (FrameView* frameView = frame->view())
            frameView->setIsInWindow(isInWindow);
    }

    if (isInWindow)
        resumeAnimatingImages();
}

void Page::addActivityStateChangeObserver(ActivityStateChangeObserver& observer)
{
    m_activityStateChangeObservers.add(&observer);
}

void Page::removeActivityStateChangeObserver(ActivityStateChangeObserver& observer)
{
    m_activityStateChangeObservers.remove(&observer);
}

#if ENABLE(INTERSECTION_OBSERVER)
void Page::addDocumentNeedingIntersectionObservationUpdate(Document& document)
{
    if (m_documentsNeedingIntersectionObservationUpdate.find(&document) == notFound)
        m_documentsNeedingIntersectionObservationUpdate.append(makeWeakPtr(document));
}

void Page::updateIntersectionObservations()
{
    m_intersectionObservationUpdateTimer.stop();
    for (auto document : m_documentsNeedingIntersectionObservationUpdate) {
        if (document)
            document->updateIntersectionObservations();
    }
    m_documentsNeedingIntersectionObservationUpdate.clear();
}

void Page::scheduleForcedIntersectionObservationUpdate(Document& document)
{
    addDocumentNeedingIntersectionObservationUpdate(document);
    if (m_intersectionObservationUpdateTimer.isActive())
        return;
    m_intersectionObservationUpdateTimer.startOneShot(0_s);
}
#endif

void Page::suspendScriptedAnimations()
{
    m_scriptedAnimationsSuspended = true;
    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (frame->document())
            frame->document()->suspendScriptedAnimationControllerCallbacks();
    }
}

void Page::resumeScriptedAnimations()
{
    m_scriptedAnimationsSuspended = false;
    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (frame->document())
            frame->document()->resumeScriptedAnimationControllerCallbacks();
    }
}

enum class ThrottlingReasonOperation { Add, Remove };
static void updateScriptedAnimationsThrottlingReason(Page& page, ThrottlingReasonOperation operation, ScriptedAnimationController::ThrottlingReason reason)
{
    for (Frame* frame = &page.mainFrame(); frame; frame = frame->tree().traverseNext()) {
        auto* document = frame->document();
        if (!document)
            continue;
        auto* scriptedAnimationController = document->scriptedAnimationController();
        if (!scriptedAnimationController)
            continue;

        if (operation == ThrottlingReasonOperation::Add)
            scriptedAnimationController->addThrottlingReason(reason);
        else
            scriptedAnimationController->removeThrottlingReason(reason);
    }
}

void Page::setIsVisuallyIdleInternal(bool isVisuallyIdle)
{
    updateScriptedAnimationsThrottlingReason(*this, isVisuallyIdle ? ThrottlingReasonOperation::Add : ThrottlingReasonOperation::Remove, ScriptedAnimationController::ThrottlingReason::VisuallyIdle);
}

void Page::handleLowModePowerChange(bool isLowPowerModeEnabled)
{
    updateScriptedAnimationsThrottlingReason(*this, isLowPowerModeEnabled ? ThrottlingReasonOperation::Add : ThrottlingReasonOperation::Remove, ScriptedAnimationController::ThrottlingReason::LowPowerMode);
    if (RuntimeEnabledFeatures::sharedFeatures().webAnimationsCSSIntegrationEnabled()) {
        forEachDocument([&] (Document& document) {
            if (auto timeline = document.existingTimeline())
                timeline->updateThrottlingState();
        });
    } else
        mainFrame().animation().updateThrottlingState();
    updateDOMTimerAlignmentInterval();
}

void Page::userStyleSheetLocationChanged()
{
    // FIXME: Eventually we will move to a model of just being handed the sheet
    // text instead of loading the URL ourselves.
    URL url = m_settings->userStyleSheetLocation();
    
    // Allow any local file URL scheme to be loaded.
    if (SchemeRegistry::shouldTreatURLSchemeAsLocal(url.protocol().toStringWithoutCopying()))
        m_userStyleSheetPath = url.fileSystemPath();
    else
        m_userStyleSheetPath = String();

    m_didLoadUserStyleSheet = false;
    m_userStyleSheet = String();
    m_userStyleSheetModificationTime = 0;

    // Data URLs with base64-encoded UTF-8 style sheets are common. We can process them
    // synchronously and avoid using a loader. 
    if (url.protocolIsData() && url.string().startsWith("data:text/css;charset=utf-8;base64,")) {
        m_didLoadUserStyleSheet = true;

        Vector<char> styleSheetAsUTF8;
        if (base64Decode(decodeURLEscapeSequences(url.string().substring(35)), styleSheetAsUTF8, Base64IgnoreSpacesAndNewLines))
            m_userStyleSheet = String::fromUTF8(styleSheetAsUTF8.data(), styleSheetAsUTF8.size());
    }

    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (frame->document())
            frame->document()->extensionStyleSheets().updatePageUserSheet();
    }
}

const String& Page::userStyleSheet() const
{
    if (m_userStyleSheetPath.isEmpty())
        return m_userStyleSheet;

    time_t modTime;
    if (!FileSystem::getFileModificationTime(m_userStyleSheetPath, modTime)) {
        // The stylesheet either doesn't exist, was just deleted, or is
        // otherwise unreadable. If we've read the stylesheet before, we should
        // throw away that data now as it no longer represents what's on disk.
        m_userStyleSheet = String();
        return m_userStyleSheet;
    }

    // If the stylesheet hasn't changed since the last time we read it, we can
    // just return the old data.
    if (m_didLoadUserStyleSheet && modTime <= m_userStyleSheetModificationTime)
        return m_userStyleSheet;

    m_didLoadUserStyleSheet = true;
    m_userStyleSheet = String();
    m_userStyleSheetModificationTime = modTime;

    // FIXME: It would be better to load this asynchronously to avoid blocking
    // the process, but we will first need to create an asynchronous loading
    // mechanism that is not tied to a particular Frame. We will also have to
    // determine what our behavior should be before the stylesheet is loaded
    // and what should happen when it finishes loading, especially with respect
    // to when the load event fires, when Document::close is called, and when
    // layout/paint are allowed to happen.
    RefPtr<SharedBuffer> data = SharedBuffer::createWithContentsOfFile(m_userStyleSheetPath);
    if (!data)
        return m_userStyleSheet;

    m_userStyleSheet = TextResourceDecoder::create("text/css")->decodeAndFlush(data->data(), data->size());

    return m_userStyleSheet;
}

void Page::userAgentChanged()
{
    for (auto* frame = &m_mainFrame.get(); frame; frame = frame->tree().traverseNext()) {
        auto* window = frame->window();
        if (!window)
            continue;
        if (auto* navigator = window->optionalNavigator())
            navigator->userAgentChanged();
    }
}

void Page::invalidateStylesForAllLinks()
{
    for (Frame* frame = &m_mainFrame.get(); frame; frame = frame->tree().traverseNext()) {
        if (!frame->document())
            continue;
        frame->document()->visitedLinkState().invalidateStyleForAllLinks();
    }
}

void Page::invalidateStylesForLink(SharedStringHash linkHash)
{
    for (Frame* frame = &m_mainFrame.get(); frame; frame = frame->tree().traverseNext()) {
        if (!frame->document())
            continue;
        frame->document()->visitedLinkState().invalidateStyleForLink(linkHash);
    }
}

void Page::invalidateInjectedStyleSheetCacheInAllFrames()
{
    for (Frame* frame = &m_mainFrame.get(); frame; frame = frame->tree().traverseNext()) {
        Document* document = frame->document();
        if (!document)
            continue;
        document->extensionStyleSheets().invalidateInjectedStyleSheetCache();
    }
}

void Page::setDebugger(JSC::Debugger* debugger)
{
    if (m_debugger == debugger)
        return;

    m_debugger = debugger;

    for (Frame* frame = &m_mainFrame.get(); frame; frame = frame->tree().traverseNext())
        frame->windowProxy().attachDebugger(m_debugger);
}

StorageNamespace* Page::sessionStorage(bool optionalCreate)
{
    if (!m_sessionStorage && optionalCreate)
        m_sessionStorage = m_storageNamespaceProvider->createSessionStorageNamespace(*this, m_settings->sessionStorageQuota());

    return m_sessionStorage.get();
}

void Page::setSessionStorage(RefPtr<StorageNamespace>&& newStorage)
{
    m_sessionStorage = WTFMove(newStorage);
}

StorageNamespace* Page::ephemeralLocalStorage(bool optionalCreate)
{
    if (!m_ephemeralLocalStorage && optionalCreate)
        m_ephemeralLocalStorage = m_storageNamespaceProvider->createEphemeralLocalStorageNamespace(*this, m_settings->sessionStorageQuota());

    return m_ephemeralLocalStorage.get();
}

void Page::setEphemeralLocalStorage(RefPtr<StorageNamespace>&& newStorage)
{
    m_ephemeralLocalStorage = WTFMove(newStorage);
}

bool Page::hasCustomHTMLTokenizerTimeDelay() const
{
    return m_settings->maxParseDuration() != -1;
}

double Page::customHTMLTokenizerTimeDelay() const
{
    ASSERT(m_settings->maxParseDuration() != -1);
    return m_settings->maxParseDuration();
}

void Page::setMemoryCacheClientCallsEnabled(bool enabled)
{
    if (m_areMemoryCacheClientCallsEnabled == enabled)
        return;

    m_areMemoryCacheClientCallsEnabled = enabled;
    if (!enabled)
        return;

    for (RefPtr<Frame> frame = &mainFrame(); frame; frame = frame->tree().traverseNext())
        frame->loader().tellClientAboutPastMemoryCacheLoads();
}

void Page::hiddenPageDOMTimerThrottlingStateChanged()
{
    // Disable & reengage to ensure state is updated.
    setTimerThrottlingState(TimerThrottlingState::Disabled);
    updateTimerThrottlingState();
}

void Page::updateTimerThrottlingState()
{
    // Timer throttling disabled if page is visually active, or disabled by setting.
    if (!m_settings->hiddenPageDOMTimerThrottlingEnabled() || !(m_activityState & ActivityState::IsVisuallyIdle)) {
        setTimerThrottlingState(TimerThrottlingState::Disabled);
        return;
    }

    // If the page is visible (but idle), there is any activity (loading, media playing, etc), or per setting,
    // we allow timer throttling, but not increasing timer throttling.
    if (!m_settings->hiddenPageDOMTimerThrottlingAutoIncreases()
        || m_activityState.containsAny({ActivityState::IsVisible, ActivityState::IsAudible, ActivityState::IsLoading, ActivityState::IsCapturingMedia })) {
        setTimerThrottlingState(TimerThrottlingState::Enabled);
        return;
    }

    // If we get here increasing timer throttling is enabled.
    setTimerThrottlingState(TimerThrottlingState::EnabledIncreasing);
}

void Page::setTimerThrottlingState(TimerThrottlingState state)
{
    if (state == m_timerThrottlingState)
        return;

    m_timerThrottlingState = state;
    m_timerThrottlingStateLastChangedTime = MonotonicTime::now();

    updateDOMTimerAlignmentInterval();

    // When throttling is disabled, release all throttled timers.
    if (state == TimerThrottlingState::Disabled) {
        for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
            if (auto* document = frame->document())
                document->didChangeTimerAlignmentInterval();
        }
    }
}

void Page::setDOMTimerAlignmentIntervalIncreaseLimit(Seconds limit)
{
    m_domTimerAlignmentIntervalIncreaseLimit = limit;

    // If (m_domTimerAlignmentIntervalIncreaseLimit < m_domTimerAlignmentInterval) then we need
    // to update m_domTimerAlignmentInterval, if greater then need to restart the increase timer.
    if (m_timerThrottlingState == TimerThrottlingState::EnabledIncreasing)
        updateDOMTimerAlignmentInterval();
}

void Page::updateDOMTimerAlignmentInterval()
{
    bool needsIncreaseTimer = false;

    switch (m_timerThrottlingState) {
    case TimerThrottlingState::Disabled:
        m_domTimerAlignmentInterval = isLowPowerModeEnabled() ? DOMTimer::defaultAlignmentIntervalInLowPowerMode() : DOMTimer::defaultAlignmentInterval();
        break;

    case TimerThrottlingState::Enabled:
        m_domTimerAlignmentInterval = DOMTimer::hiddenPageAlignmentInterval();
        break;

    case TimerThrottlingState::EnabledIncreasing:
        // For pages in prerender state maximum throttling kicks in immediately.
        if (m_isPrerender)
            m_domTimerAlignmentInterval = m_domTimerAlignmentIntervalIncreaseLimit;
        else {
            ASSERT(!!m_timerThrottlingStateLastChangedTime);
            m_domTimerAlignmentInterval = MonotonicTime::now() - m_timerThrottlingStateLastChangedTime;
            // If we're below the limit, set the timer. If above, clamp to limit.
            if (m_domTimerAlignmentInterval < m_domTimerAlignmentIntervalIncreaseLimit)
                needsIncreaseTimer = true;
            else
                m_domTimerAlignmentInterval = m_domTimerAlignmentIntervalIncreaseLimit;
        }
        // Alignment interval should not be less than DOMTimer::hiddenPageAlignmentInterval().
        m_domTimerAlignmentInterval = std::max(m_domTimerAlignmentInterval, DOMTimer::hiddenPageAlignmentInterval());
    }

    // If throttling is enabled, auto-increasing of throttling is enabled, and the auto-increase
    // limit has not yet been reached, and then arm the timer to consider an increase. Time to wait
    // between increases is equal to the current throttle time. Since alinment interval increases
    // exponentially, time between steps is exponential too.
    if (!needsIncreaseTimer)
        m_domTimerAlignmentIntervalIncreaseTimer.stop();
    else if (!m_domTimerAlignmentIntervalIncreaseTimer.isActive())
        m_domTimerAlignmentIntervalIncreaseTimer.startOneShot(m_domTimerAlignmentInterval);
}

void Page::domTimerAlignmentIntervalIncreaseTimerFired()
{
    ASSERT(m_settings->hiddenPageDOMTimerThrottlingAutoIncreases());
    ASSERT(m_timerThrottlingState == TimerThrottlingState::EnabledIncreasing);
    ASSERT(m_domTimerAlignmentInterval < m_domTimerAlignmentIntervalIncreaseLimit);
    
    // Alignment interval is increased to equal the time the page has been throttled, to a limit.
    updateDOMTimerAlignmentInterval();
}

void Page::dnsPrefetchingStateChanged()
{
    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (!frame->document())
            continue;
        frame->document()->initDNSPrefetch();
    }
}

Vector<Ref<PluginViewBase>> Page::pluginViews()
{
    Vector<Ref<PluginViewBase>> views;
    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        auto* view = frame->view();
        if (!view)
            break;
        for (auto& widget : view->children()) {
            if (is<PluginViewBase>(widget))
                views.append(downcast<PluginViewBase>(widget.get()));
        }
    }
    return views;
}

void Page::storageBlockingStateChanged()
{
    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (!frame->document())
            continue;
        frame->document()->storageBlockingStateDidChange();
    }

    // Collect the PluginViews in to a vector to ensure that action the plug-in takes
    // from below storageBlockingStateChanged does not affect their lifetime.
    for (auto& view : pluginViews())
        view->storageBlockingStateChanged();
}

void Page::enableLegacyPrivateBrowsing(bool privateBrowsingEnabled)
{
    // Don't allow changing the legacy private browsing state if we have set a session ID.
    ASSERT(m_sessionID == PAL::SessionID::defaultSessionID() || m_sessionID == PAL::SessionID::legacyPrivateSessionID());

    setSessionID(privateBrowsingEnabled ? PAL::SessionID::legacyPrivateSessionID() : PAL::SessionID::defaultSessionID());
}

void Page::updateIsPlayingMedia(uint64_t sourceElementID)
{
    MediaProducer::MediaStateFlags state = MediaProducer::IsNotPlaying;
    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (Document* document = frame->document())
            state |= document->mediaState();
    }

    if (state == m_mediaState)
        return;

    m_mediaState = state;

    chrome().client().isPlayingMediaDidChange(state, sourceElementID);
}

void Page::schedulePlaybackControlsManagerUpdate()
{
#if ENABLE(VIDEO)
    if (!m_playbackControlsManagerUpdateTimer.isActive())
        m_playbackControlsManagerUpdateTimer.startOneShot(0_s);
#endif
}

#if ENABLE(VIDEO)
void Page::playbackControlsManagerUpdateTimerFired()
{
    if (auto bestMediaElement = HTMLMediaElement::bestMediaElementForShowingPlaybackControlsManager(MediaElementSession::PlaybackControlsPurpose::ControlsManager))
        chrome().client().setUpPlaybackControlsManager(*bestMediaElement);
    else
        chrome().client().clearPlaybackControlsManager();
}
#endif

void Page::setMuted(MediaProducer::MutedStateFlags muted)
{
    if (m_mutedState == muted)
        return;

    m_mutedState = muted;

    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (!frame->document())
            continue;
        frame->document()->pageMutedStateDidChange();
    }
}

void Page::stopMediaCapture()
{
#if ENABLE(MEDIA_STREAM)
    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (!frame->document())
            continue;

        frame->document()->stopMediaCapture();
    }
#endif
}

#if ENABLE(MEDIA_SESSION)
void Page::handleMediaEvent(MediaEventType eventType)
{
    switch (eventType) {
    case MediaEventType::PlayPause:
        MediaSessionManager::singleton().togglePlayback();
        break;
    case MediaEventType::TrackNext:
        MediaSessionManager::singleton().skipToNextTrack();
        break;
    case MediaEventType::TrackPrevious:
        MediaSessionManager::singleton().skipToPreviousTrack();
        break;
    }
}

void Page::setVolumeOfMediaElement(double volume, uint64_t elementID)
{
    if (HTMLMediaElement* element = HTMLMediaElement::elementWithID(elementID))
        element->setVolume(volume, ASSERT_NO_EXCEPTION);
}
#endif

#if !ASSERT_DISABLED
void Page::checkSubframeCountConsistency() const
{
    ASSERT(m_subframeCount >= 0);

    int subframeCount = 0;
    for (const Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext())
        ++subframeCount;

    ASSERT(m_subframeCount + 1 == subframeCount);
}
#endif

void Page::resumeAnimatingImages()
{
    // Drawing models which cache painted content while out-of-window (WebKit2's composited drawing areas, etc.)
    // require that we repaint animated images to kickstart the animation loop.
    if (FrameView* view = mainFrame().view())
        view->resumeVisibleImageAnimationsIncludingSubframes();
}

void Page::setActivityState(OptionSet<ActivityState::Flag> activityState)
{
    auto changed = m_activityState ^ activityState;
    if (!changed)
        return;

    auto oldActivityState = m_activityState;

    bool wasVisibleAndActive = isVisibleAndActive();
    m_activityState = activityState;

    m_focusController->setActivityState(activityState);

    if (changed & ActivityState::IsVisible)
        setIsVisibleInternal(activityState.contains(ActivityState::IsVisible));
    if (changed & ActivityState::IsInWindow)
        setIsInWindowInternal(activityState.contains(ActivityState::IsInWindow));
    if (changed & ActivityState::IsVisuallyIdle)
        setIsVisuallyIdleInternal(activityState.contains(ActivityState::IsVisuallyIdle));
    if (changed & ActivityState::WindowIsActive) {
        if (auto* view = m_mainFrame->view())
            view->updateTiledBackingAdaptiveSizing();
    }

    if (changed.containsAny({ActivityState::IsVisible, ActivityState::IsVisuallyIdle, ActivityState::IsAudible, ActivityState::IsLoading, ActivityState::IsCapturingMedia }))
        updateTimerThrottlingState();

    for (auto* observer : m_activityStateChangeObservers)
        observer->activityStateDidChange(oldActivityState, m_activityState);

    if (wasVisibleAndActive != isVisibleAndActive())
        PlatformMediaSessionManager::updateNowPlayingInfoIfNecessary();

    if (m_performanceMonitor)
        m_performanceMonitor->activityStateChanged(oldActivityState, activityState);
}

bool Page::isVisibleAndActive() const
{
    return m_activityState.contains(ActivityState::IsVisible) && m_activityState.contains(ActivityState::WindowIsActive);
}

bool Page::isWindowActive() const
{
    return m_activityState.contains(ActivityState::WindowIsActive);
}

void Page::setIsVisible(bool isVisible)
{
    auto state = m_activityState;

    if (isVisible) {
        state.remove(ActivityState::IsVisuallyIdle);
        state.add({ ActivityState::IsVisible, ActivityState::IsVisibleOrOccluded });
    } else {
        state.add(ActivityState::IsVisuallyIdle);
        state.remove({ ActivityState::IsVisible, ActivityState::IsVisibleOrOccluded });
    }
    setActivityState(state);
}

enum class SVGAnimationsState { Paused, Resumed };
static inline void setSVGAnimationsState(Page& page, SVGAnimationsState state)
{
    for (Frame* frame = &page.mainFrame(); frame; frame = frame->tree().traverseNext()) {
        auto* document = frame->document();
        if (!document)
            continue;

        if (!document->svgExtensions())
            continue;

        if (state == SVGAnimationsState::Paused)
            document->accessSVGExtensions().pauseAnimations();
        else
            document->accessSVGExtensions().unpauseAnimations();
    }
}

void Page::setIsVisibleInternal(bool isVisible)
{
    // FIXME: The visibility state should be stored on the top-level document.
    // https://bugs.webkit.org/show_bug.cgi?id=116769

    if (isVisible) {
        m_isPrerender = false;

        resumeScriptedAnimations();
#if PLATFORM(IOS_FAMILY)
        resumeDeviceMotionAndOrientationUpdates();
#endif

        if (FrameView* view = mainFrame().view())
            view->show();

        if (m_settings->hiddenPageCSSAnimationSuspensionEnabled()) {
            if (RuntimeEnabledFeatures::sharedFeatures().webAnimationsCSSIntegrationEnabled()) {
                forEachDocument([&] (Document& document) {
                    if (auto* timeline = document.existingTimeline())
                        timeline->resumeAnimations();
                });
            } else
                mainFrame().animation().resumeAnimations();
        }

        setSVGAnimationsState(*this, SVGAnimationsState::Resumed);

        resumeAnimatingImages();

        if (m_navigationToLogWhenVisible) {
            logNavigation(m_navigationToLogWhenVisible.value());
            m_navigationToLogWhenVisible = std::nullopt;
        }
    }

    if (!isVisible) {
        if (m_settings->hiddenPageCSSAnimationSuspensionEnabled()) {
            if (RuntimeEnabledFeatures::sharedFeatures().webAnimationsCSSIntegrationEnabled()) {
                forEachDocument([&] (Document& document) {
                    if (auto* timeline = document.existingTimeline())
                        timeline->suspendAnimations();
                });
            } else
                mainFrame().animation().suspendAnimations();
        }

        setSVGAnimationsState(*this, SVGAnimationsState::Paused);

#if PLATFORM(IOS_FAMILY)
        suspendDeviceMotionAndOrientationUpdates();
#endif

        suspendScriptedAnimations();

        if (FrameView* view = mainFrame().view())
            view->hide();
    }

    Vector<Ref<Document>> documents;
    for (Frame* frame = &m_mainFrame.get(); frame; frame = frame->tree().traverseNext())
        documents.append(*frame->document());

    for (auto& document : documents)
        document->visibilityStateChanged();
}

void Page::setIsPrerender()
{
    m_isPrerender = true;
    updateDOMTimerAlignmentInterval();
}

VisibilityState Page::visibilityState() const
{
    if (isVisible())
        return VisibilityState::Visible;
    if (m_isPrerender)
        return VisibilityState::Prerender;
    return VisibilityState::Hidden;
}

#if ENABLE(RUBBER_BANDING)
void Page::addHeaderWithHeight(int headerHeight)
{
    m_headerHeight = headerHeight;

    FrameView* frameView = mainFrame().view();
    if (!frameView)
        return;

    RenderView* renderView = frameView->renderView();
    if (!renderView)
        return;

    frameView->setHeaderHeight(m_headerHeight);
    renderView->compositor().updateLayerForHeader(m_headerHeight);
}

void Page::addFooterWithHeight(int footerHeight)
{
    m_footerHeight = footerHeight;

    FrameView* frameView = mainFrame().view();
    if (!frameView)
        return;

    RenderView* renderView = frameView->renderView();
    if (!renderView)
        return;

    frameView->setFooterHeight(m_footerHeight);
    renderView->compositor().updateLayerForFooter(m_footerHeight);
}
#endif

void Page::incrementNestedRunLoopCount()
{
    m_nestedRunLoopCount++;
}

void Page::decrementNestedRunLoopCount()
{
    ASSERT(m_nestedRunLoopCount);
    if (m_nestedRunLoopCount <= 0)
        return;

    m_nestedRunLoopCount--;

    if (!m_nestedRunLoopCount && m_unnestCallback) {
        callOnMainThread([this] {
            if (insideNestedRunLoop())
                return;

            // This callback may destruct the Page.
            if (m_unnestCallback) {
                auto callback = WTFMove(m_unnestCallback);
                callback();
            }
        });
    }
}

void Page::whenUnnested(WTF::Function<void()>&& callback)
{
    ASSERT(!m_unnestCallback);

    m_unnestCallback = WTFMove(callback);
}

#if ENABLE(REMOTE_INSPECTOR)
bool Page::remoteInspectionAllowed() const
{
    return m_inspectorDebuggable->remoteDebuggingAllowed();
}

void Page::setRemoteInspectionAllowed(bool allowed)
{
    m_inspectorDebuggable->setRemoteDebuggingAllowed(allowed);
}

String Page::remoteInspectionNameOverride() const
{
    return m_inspectorDebuggable->nameOverride();
}

void Page::setRemoteInspectionNameOverride(const String& name)
{
    m_inspectorDebuggable->setNameOverride(name);
}

void Page::remoteInspectorInformationDidChange() const
{
    m_inspectorDebuggable->update();
}
#endif

void Page::addLayoutMilestones(OptionSet<LayoutMilestone> milestones)
{
    // In the future, we may want a function that replaces m_layoutMilestones instead of just adding to it.
    m_requestedLayoutMilestones.add(milestones);
}

void Page::removeLayoutMilestones(OptionSet<LayoutMilestone> milestones)
{
    m_requestedLayoutMilestones.remove(milestones);
}

Color Page::pageExtendedBackgroundColor() const
{
    FrameView* frameView = mainFrame().view();
    if (!frameView)
        return Color();

    RenderView* renderView = frameView->renderView();
    if (!renderView)
        return Color();

    return renderView->compositor().rootExtendedBackgroundColor();
}

// These are magical constants that might be tweaked over time.
static const double gMinimumPaintedAreaRatio = 0.1;
static const double gMaximumUnpaintedAreaRatio = 0.04;

bool Page::isCountingRelevantRepaintedObjects() const
{
    return m_isCountingRelevantRepaintedObjects && m_requestedLayoutMilestones.contains(DidHitRelevantRepaintedObjectsAreaThreshold);
}

void Page::startCountingRelevantRepaintedObjects()
{
    // Reset everything in case we didn't hit the threshold last time.
    resetRelevantPaintedObjectCounter();

    m_isCountingRelevantRepaintedObjects = true;
}

void Page::resetRelevantPaintedObjectCounter()
{
    m_isCountingRelevantRepaintedObjects = false;
    m_relevantUnpaintedRenderObjects.clear();
    m_topRelevantPaintedRegion = Region();
    m_bottomRelevantPaintedRegion = Region();
    m_relevantUnpaintedRegion = Region();
}

static LayoutRect relevantViewRect(RenderView* view)
{
    LayoutRect viewRect = view->viewRect();

    float relevantViewRectWidth = 980;
#if PLATFORM(WATCHOS)
    // FIXME(186051): Consider limiting the relevant rect width to the view width everywhere.
    relevantViewRectWidth = std::min<float>(viewRect.width().toFloat(), relevantViewRectWidth);
#endif

    // DidHitRelevantRepaintedObjectsAreaThreshold is a LayoutMilestone intended to indicate that
    // a certain relevant amount of content has been drawn to the screen. This is the rect that
    // has been determined to be relevant in the context of this goal. We may choose to tweak
    // the rect over time, much like we may choose to tweak gMinimumPaintedAreaRatio and
    // gMaximumUnpaintedAreaRatio. But this seems to work well right now.
    LayoutRect relevantViewRect { 0, 0, relevantViewRectWidth, 1300 };
    // If the viewRect is wider than the relevantViewRect, center the relevantViewRect.
    if (viewRect.width() > relevantViewRect.width())
        relevantViewRect.setX((viewRect.width() - relevantViewRect.width()) / 2);

    return relevantViewRect;
}

void Page::addRelevantRepaintedObject(RenderObject* object, const LayoutRect& objectPaintRect)
{
    if (!isCountingRelevantRepaintedObjects())
        return;

    // Objects inside sub-frames are not considered to be relevant.
    if (&object->frame() != &mainFrame())
        return;

    LayoutRect relevantRect = relevantViewRect(&object->view());

    // The objects are only relevant if they are being painted within the viewRect().
    if (!objectPaintRect.intersects(snappedIntRect(relevantRect)))
        return;

    IntRect snappedPaintRect = snappedIntRect(objectPaintRect);

    // If this object was previously counted as an unpainted object, remove it from that HashSet
    // and corresponding Region. FIXME: This doesn't do the right thing if the objects overlap.
    if (m_relevantUnpaintedRenderObjects.remove(object))
        m_relevantUnpaintedRegion.subtract(snappedPaintRect);

    // Split the relevantRect into a top half and a bottom half. Making sure we have coverage in
    // both halves helps to prevent cases where we have a fully loaded menu bar or masthead with
    // no content beneath that.
    LayoutRect topRelevantRect = relevantRect;
    topRelevantRect.contract(LayoutSize(0, relevantRect.height() / 2));
    LayoutRect bottomRelevantRect = topRelevantRect;
    bottomRelevantRect.setY(relevantRect.height() / 2);

    // If the rect straddles both Regions, split it appropriately.
    if (topRelevantRect.intersects(snappedPaintRect) && bottomRelevantRect.intersects(snappedPaintRect)) {
        IntRect topIntersection = snappedPaintRect;
        topIntersection.intersect(snappedIntRect(topRelevantRect));
        m_topRelevantPaintedRegion.unite(topIntersection);

        IntRect bottomIntersection = snappedPaintRect;
        bottomIntersection.intersect(snappedIntRect(bottomRelevantRect));
        m_bottomRelevantPaintedRegion.unite(bottomIntersection);
    } else if (topRelevantRect.intersects(snappedPaintRect))
        m_topRelevantPaintedRegion.unite(snappedPaintRect);
    else
        m_bottomRelevantPaintedRegion.unite(snappedPaintRect);

    float topPaintedArea = m_topRelevantPaintedRegion.totalArea();
    float bottomPaintedArea = m_bottomRelevantPaintedRegion.totalArea();
    float viewArea = relevantRect.width() * relevantRect.height();

    float ratioThatIsPaintedOnTop = topPaintedArea / viewArea;
    float ratioThatIsPaintedOnBottom = bottomPaintedArea / viewArea;
    float ratioOfViewThatIsUnpainted = m_relevantUnpaintedRegion.totalArea() / viewArea;

    if (ratioThatIsPaintedOnTop > (gMinimumPaintedAreaRatio / 2) && ratioThatIsPaintedOnBottom > (gMinimumPaintedAreaRatio / 2)
        && ratioOfViewThatIsUnpainted < gMaximumUnpaintedAreaRatio) {
        m_isCountingRelevantRepaintedObjects = false;
        resetRelevantPaintedObjectCounter();
        if (Frame* frame = &mainFrame())
            frame->loader().didReachLayoutMilestone(DidHitRelevantRepaintedObjectsAreaThreshold);
    }
}

void Page::addRelevantUnpaintedObject(RenderObject* object, const LayoutRect& objectPaintRect)
{
    if (!isCountingRelevantRepaintedObjects())
        return;

    // The objects are only relevant if they are being painted within the relevantViewRect().
    if (!objectPaintRect.intersects(snappedIntRect(relevantViewRect(&object->view()))))
        return;

    m_relevantUnpaintedRenderObjects.add(object);
    m_relevantUnpaintedRegion.unite(snappedIntRect(objectPaintRect));
}

void Page::suspendDeviceMotionAndOrientationUpdates()
{
    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (Document* document = frame->document())
            document->suspendDeviceMotionAndOrientationUpdates();
    }
}

void Page::resumeDeviceMotionAndOrientationUpdates()
{
    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (Document* document = frame->document())
            document->resumeDeviceMotionAndOrientationUpdates();
    }
}

void Page::suspendActiveDOMObjectsAndAnimations()
{
    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext())
        frame->suspendActiveDOMObjectsAndAnimations();
}

void Page::resumeActiveDOMObjectsAndAnimations()
{
    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext())
        frame->resumeActiveDOMObjectsAndAnimations();

    resumeAnimatingImages();
}

bool Page::hasSeenAnyPlugin() const
{
    return !m_seenPlugins.isEmpty();
}

bool Page::hasSeenPlugin(const String& serviceType) const
{
    return m_seenPlugins.contains(serviceType);
}

void Page::sawPlugin(const String& serviceType)
{
    m_seenPlugins.add(serviceType);
}

void Page::resetSeenPlugins()
{
    m_seenPlugins.clear();
}

bool Page::hasSeenAnyMediaEngine() const
{
    return !m_seenMediaEngines.isEmpty();
}

bool Page::hasSeenMediaEngine(const String& engineDescription) const
{
    return m_seenMediaEngines.contains(engineDescription);
}

void Page::sawMediaEngine(const String& engineDescription)
{
    m_seenMediaEngines.add(engineDescription);
}

void Page::resetSeenMediaEngines()
{
    m_seenMediaEngines.clear();
}

void Page::hiddenPageCSSAnimationSuspensionStateChanged()
{
    if (!isVisible()) {
        if (RuntimeEnabledFeatures::sharedFeatures().webAnimationsCSSIntegrationEnabled()) {
            forEachDocument([&] (Document& document) {
                if (auto* timeline = document.existingTimeline()) {
                    if (m_settings->hiddenPageCSSAnimationSuspensionEnabled())
                        timeline->suspendAnimations();
                    else
                        timeline->resumeAnimations();
                }
            });
        } else {
            if (m_settings->hiddenPageCSSAnimationSuspensionEnabled())
                mainFrame().animation().suspendAnimations();
            else
                mainFrame().animation().resumeAnimations();
        }
    }
}

#if ENABLE(VIDEO_TRACK)
void Page::captionPreferencesChanged()
{
    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (!frame->document())
            continue;
        frame->document()->captionPreferencesChanged();
    }
}
#endif

void Page::forbidPrompts()
{
    ++m_forbidPromptsDepth;
}

void Page::allowPrompts()
{
    ASSERT(m_forbidPromptsDepth);
    --m_forbidPromptsDepth;
}

bool Page::arePromptsAllowed()
{
    return !m_forbidPromptsDepth;
}

void Page::logNavigation(const Navigation& navigation)
{
    String navigationDescription;
    switch (navigation.type) {
    case FrameLoadType::Standard:
        navigationDescription = "standard"_s;
        break;
    case FrameLoadType::Back:
        navigationDescription = "back"_s;
        break;
    case FrameLoadType::Forward:
        navigationDescription = "forward"_s;
        break;
    case FrameLoadType::IndexedBackForward:
        navigationDescription = "indexedBackForward"_s;
        break;
    case FrameLoadType::Reload:
        navigationDescription = "reload"_s;
        break;
    case FrameLoadType::Same:
        navigationDescription = "same"_s;
        break;
    case FrameLoadType::ReloadFromOrigin:
        navigationDescription = "reloadFromOrigin"_s;
        break;
    case FrameLoadType::ReloadExpiredOnly:
        navigationDescription = "reloadRevalidatingExpired"_s;
        break;
    case FrameLoadType::Replace:
    case FrameLoadType::RedirectWithLockedBackForwardList:
        // Not logging those for now.
        return;
    }
    diagnosticLoggingClient().logDiagnosticMessage(DiagnosticLoggingKeys::navigationKey(), navigationDescription, ShouldSample::No);

    if (!navigation.domain.isEmpty())
        diagnosticLoggingClient().logDiagnosticMessageWithEnhancedPrivacy(DiagnosticLoggingKeys::domainVisitedKey(), navigation.domain, ShouldSample::No);
}

void Page::mainFrameLoadStarted(const URL& destinationURL, FrameLoadType type)
{
    String domain;
#if ENABLE(PUBLIC_SUFFIX_LIST)
    domain = topPrivatelyControlledDomain(destinationURL.host().toString());
#else
    UNUSED_PARAM(destinationURL);
#endif

    Navigation navigation = { domain, type };

    // To avoid being too verbose, we only log navigations if the page is or becomes visible. This avoids logging non-user observable loads.
    if (!isVisible()) {
        m_navigationToLogWhenVisible = navigation;
        return;
    }

    m_navigationToLogWhenVisible = std::nullopt;
    logNavigation(navigation);
}

PluginInfoProvider& Page::pluginInfoProvider()
{
    return m_pluginInfoProvider;
}

UserContentProvider& Page::userContentProvider()
{
    return m_userContentProvider;
}

void Page::setUserContentProvider(Ref<UserContentProvider>&& userContentProvider)
{
    m_userContentProvider->removePage(*this);
    m_userContentProvider = WTFMove(userContentProvider);
    m_userContentProvider->addPage(*this);

    invalidateInjectedStyleSheetCacheInAllFrames();
}

void Page::setStorageNamespaceProvider(Ref<StorageNamespaceProvider>&& storageNamespaceProvider)
{
    m_storageNamespaceProvider->removePage(*this);
    m_storageNamespaceProvider = WTFMove(storageNamespaceProvider);
    m_storageNamespaceProvider->addPage(*this);

    // This needs to reset all the local storage namespaces of all the pages.
}

VisitedLinkStore& Page::visitedLinkStore()
{
    return m_visitedLinkStore;
}

void Page::setVisitedLinkStore(Ref<VisitedLinkStore>&& visitedLinkStore)
{
    m_visitedLinkStore->removePage(*this);
    m_visitedLinkStore = WTFMove(visitedLinkStore);
    m_visitedLinkStore->addPage(*this);

    invalidateStylesForAllLinks();
}

PAL::SessionID Page::sessionID() const
{
    return m_sessionID;
}

void Page::setSessionID(PAL::SessionID sessionID)
{
    ASSERT(sessionID.isValid());

#if ENABLE(INDEXED_DATABASE)
    if (sessionID != m_sessionID)
        m_idbConnectionToServer = nullptr;
#endif

    bool privateBrowsingStateChanged = (sessionID.isEphemeral() != m_sessionID.isEphemeral());

    m_sessionID = sessionID;

    if (!privateBrowsingStateChanged)
        return;

    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (!frame->document())
            continue;
        frame->document()->privateBrowsingStateDidChange();
    }

    // Collect the PluginViews in to a vector to ensure that action the plug-in takes
    // from below privateBrowsingStateChanged does not affect their lifetime.

    for (auto& view : pluginViews())
        view->privateBrowsingStateChanged(sessionID.isEphemeral());
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
void Page::addPlaybackTargetPickerClient(uint64_t contextId)
{
    chrome().client().addPlaybackTargetPickerClient(contextId);
}

void Page::removePlaybackTargetPickerClient(uint64_t contextId)
{
    chrome().client().removePlaybackTargetPickerClient(contextId);
}

void Page::showPlaybackTargetPicker(uint64_t contextId, const WebCore::IntPoint& location, bool isVideo, RouteSharingPolicy routeSharingPolicy, const String& routingContextUID)
{
#if PLATFORM(IOS_FAMILY)
    // FIXME: refactor iOS implementation.
    UNUSED_PARAM(contextId);
    UNUSED_PARAM(location);
    chrome().client().showPlaybackTargetPicker(isVideo, routeSharingPolicy, routingContextUID);
#else
    UNUSED_PARAM(routeSharingPolicy);
    UNUSED_PARAM(routingContextUID);
    chrome().client().showPlaybackTargetPicker(contextId, location, isVideo);
#endif
}

void Page::playbackTargetPickerClientStateDidChange(uint64_t contextId, MediaProducer::MediaStateFlags state)
{
    chrome().client().playbackTargetPickerClientStateDidChange(contextId, state);
}

void Page::setMockMediaPlaybackTargetPickerEnabled(bool enabled)
{
    chrome().client().setMockMediaPlaybackTargetPickerEnabled(enabled);
}

void Page::setMockMediaPlaybackTargetPickerState(const String& name, MediaPlaybackTargetContext::State state)
{
    chrome().client().setMockMediaPlaybackTargetPickerState(name, state);
}

void Page::setPlaybackTarget(uint64_t contextId, Ref<MediaPlaybackTarget>&& target)
{
    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (!frame->document())
            continue;
        frame->document()->setPlaybackTarget(contextId, target.copyRef());
    }
}

void Page::playbackTargetAvailabilityDidChange(uint64_t contextId, bool available)
{
    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (!frame->document())
            continue;
        frame->document()->playbackTargetAvailabilityDidChange(contextId, available);
    }
}

void Page::setShouldPlayToPlaybackTarget(uint64_t clientId, bool shouldPlay)
{
    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (!frame->document())
            continue;
        frame->document()->setShouldPlayToPlaybackTarget(clientId, shouldPlay);
    }
}
#endif

WheelEventTestTrigger& Page::ensureTestTrigger()
{
    if (!m_testTrigger) {
        m_testTrigger = adoptRef(new WheelEventTestTrigger());
        // We need to update the scrolling coordinator so that the mainframe scrolling node can expect wheel event test triggers.
        if (auto* frameView = mainFrame().view()) {
            if (m_scrollingCoordinator)
                m_scrollingCoordinator->updateExpectsWheelEventTestTriggerWithFrameView(*frameView);
        }
    }

    return *m_testTrigger;
}

#if ENABLE(VIDEO)
void Page::setAllowsMediaDocumentInlinePlayback(bool flag)
{
    if (m_allowsMediaDocumentInlinePlayback == flag)
        return;
    m_allowsMediaDocumentInlinePlayback = flag;

    Vector<Ref<Document>> documents;
    for (Frame* frame = &m_mainFrame.get(); frame; frame = frame->tree().traverseNext())
        documents.append(*frame->document());

    for (auto& document : documents)
        document->allowsMediaDocumentInlinePlaybackChanged();
}
#endif

#if ENABLE(INDEXED_DATABASE)
IDBClient::IDBConnectionToServer& Page::idbConnection()
{
    if (!m_idbConnectionToServer)
        m_idbConnectionToServer = &databaseProvider().idbConnectionToServerForSession(m_sessionID);
    
    return *m_idbConnectionToServer;
}

IDBClient::IDBConnectionToServer* Page::optionalIDBConnection()
{
    return m_idbConnectionToServer.get();
}

void Page::clearIDBConnection()
{
    m_idbConnectionToServer = nullptr;
}
#endif

#if ENABLE(RESOURCE_USAGE)
void Page::setResourceUsageOverlayVisible(bool visible)
{
    if (!visible) {
        m_resourceUsageOverlay = nullptr;
        return;
    }

    if (!m_resourceUsageOverlay && m_settings->acceleratedCompositingEnabled())
        m_resourceUsageOverlay = std::make_unique<ResourceUsageOverlay>(*this);
}
#endif

bool Page::isAlwaysOnLoggingAllowed() const
{
    return m_sessionID.isAlwaysOnLoggingAllowed();
}

String Page::captionUserPreferencesStyleSheet()
{
    return m_captionUserPreferencesStyleSheet;
}

void Page::setCaptionUserPreferencesStyleSheet(const String& styleSheet)
{
    if (m_captionUserPreferencesStyleSheet == styleSheet)
        return;

    m_captionUserPreferencesStyleSheet = styleSheet;
    
    invalidateInjectedStyleSheetCacheInAllFrames();
}

void Page::accessibilitySettingsDidChange()
{
    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (auto* document = frame->document()) {
            document->styleScope().evaluateMediaQueriesForAccessibilitySettingsChange();
            document->evaluateMediaQueryList();
        }
    }
}

void Page::appearanceDidChange()
{
    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        auto* document = frame->document();
        if (!document)
            continue;

        document->styleScope().didChangeStyleSheetEnvironment();
        document->styleScope().evaluateMediaQueriesForAppearanceChange();
        document->evaluateMediaQueryList();
    }
}

void Page::setUnobscuredSafeAreaInsets(const FloatBoxExtent& insets)
{
    if (m_unobscuredSafeAreaInsets == insets)
        return;

    m_unobscuredSafeAreaInsets = insets;

    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (!frame->document())
            continue;
        frame->document()->constantProperties().didChangeSafeAreaInsets();
    }
}

void Page::setUseSystemAppearance(bool value)
{
    if (m_useSystemAppearance == value)
        return;

    m_useSystemAppearance = value;

    appearanceDidChange();

    for (auto* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        auto* document = frame->document();
        if (!document)
            continue;

        // System apperance change may affect stylesheet parsing. We need to reparse.
        document->extensionStyleSheets().clearPageUserSheet();
        document->extensionStyleSheets().invalidateInjectedStyleSheetCache();
    }
}

void Page::setUseDarkAppearance(bool value)
{
    if (m_useDarkAppearance == value)
        return;

    m_useDarkAppearance = value;

    appearanceDidChange();
}

bool Page::useDarkAppearance() const
{
    FrameView* view = mainFrame().view();
    if (!view || !equalLettersIgnoringASCIICase(view->mediaType(), "screen"))
        return false;
    return m_useDarkAppearance;
}

void Page::setFullscreenInsets(const FloatBoxExtent& insets)
{
    if (insets == m_fullscreenInsets)
        return;
    m_fullscreenInsets = insets;

    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (!frame->document())
            continue;
        frame->document()->constantProperties().didChangeFullscreenInsets();
    }
}

void Page::setFullscreenAutoHideDuration(Seconds duration)
{
    if (duration == m_fullscreenAutoHideDuration)
        return;
    m_fullscreenAutoHideDuration = duration;
    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (!frame->document())
            continue;
        frame->document()->constantProperties().setFullscreenAutoHideDuration(duration);
    }
}

void Page::setFullscreenControlsHidden(bool hidden)
{
#if ENABLE(FULLSCREEN_API)
    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (!frame->document())
            continue;
        frame->document()->setFullscreenControlsHidden(hidden);
    }
#else
    UNUSED_PARAM(hidden);
#endif
}

#if ENABLE(DATA_INTERACTION)

bool Page::hasSelectionAtPosition(const FloatPoint& position) const
{
    auto currentSelection = m_mainFrame->selection().selection();
    if (!currentSelection.isRange())
        return false;

    if (auto selectedRange = currentSelection.toNormalizedRange()) {
        Vector<SelectionRect> selectionRects;
        selectedRange->collectSelectionRects(selectionRects);
        for (auto selectionRect : selectionRects) {
            if (FloatRect(selectionRect.rect()).contains(position))
                return true;
        }
    }

    return false;
}

#endif

void Page::disableICECandidateFiltering()
{
    m_shouldEnableICECandidateFilteringByDefault = false;
#if ENABLE(WEB_RTC)
    m_rtcController.disableICECandidateFilteringForAllOrigins();
#endif
}

void Page::enableICECandidateFiltering()
{
    m_shouldEnableICECandidateFilteringByDefault = true;
#if ENABLE(WEB_RTC)
    m_rtcController.enableICECandidateFiltering();
#endif
}

void Page::didChangeMainDocument()
{
#if ENABLE(WEB_RTC)
    m_rtcController.reset(m_shouldEnableICECandidateFilteringByDefault);
#endif
}

void Page::forEachDocument(const Function<void(Document&)>& functor)
{
    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (!frame->document())
            continue;

        functor(*frame->document());
    }
}

void Page::applicationWillResignActive()
{
    forEachDocument([&] (Document& document) {
        document.forEachApplicationStateChangeListener([&] (ApplicationStateChangeListener& listener) {
            listener.applicationWillResignActive();
        });
    });
}

void Page::applicationDidEnterBackground()
{
    m_libWebRTCProvider->setActive(false);
}

void Page::applicationWillEnterForeground()
{
    m_libWebRTCProvider->setActive(true);
}

void Page::applicationDidBecomeActive()
{
    forEachDocument([&] (Document& document) {
        document.forEachApplicationStateChangeListener([&] (ApplicationStateChangeListener& listener) {
            listener.applicationDidBecomeActive();
        });
    });
}

#if PLATFORM(MAC)
ScrollLatchingState* Page::latchingState()
{
    if (m_latchingState.isEmpty())
        return nullptr;

    return &m_latchingState.last();
}

void Page::pushNewLatchingState()
{
    m_latchingState.append(ScrollLatchingState());
}

void Page::resetLatchingState()
{
    m_latchingState.clear();
}

void Page::popLatchingState()
{
    m_latchingState.removeLast();
}

void Page::removeLatchingStateForTarget(Element& targetNode)
{
    if (m_latchingState.isEmpty())
        return;

    m_latchingState.removeAllMatching([&targetNode] (ScrollLatchingState& state) {
        auto* wheelElement = state.wheelEventElement();
        if (!wheelElement)
            return false;

        return targetNode.isEqualNode(wheelElement);
    });
}
#endif // PLATFORM(MAC)

#if ENABLE(APPLE_PAY)
void Page::setPaymentCoordinator(std::unique_ptr<PaymentCoordinator>&& paymentCoordinator)
{
    m_paymentCoordinator = WTFMove(paymentCoordinator);
}
#endif

} // namespace WebCore
