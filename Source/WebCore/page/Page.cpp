/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All Rights Reserved.
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

#include "AlternativeTextClient.h"
#include "AnimationController.h"
#include "BackForwardClient.h"
#include "BackForwardController.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "ClientRectList.h"
#include "ContextMenuClient.h"
#include "ContextMenuController.h"
#include "DocumentMarkerController.h"
#include "DocumentStyleSheetCollection.h"
#include "DragController.h"
#include "Editor.h"
#include "EditorClient.h"
#include "Event.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "ExceptionCodePlaceholder.h"
#include "FileSystem.h"
#include "FocusController.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameSelection.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLElement.h"
#include "HistoryController.h"
#include "HistoryItem.h"
#include "InspectorController.h"
#include "InspectorInstrumentation.h"
#include "Logging.h"
#include "MainFrame.h"
#include "MediaCanStartListener.h"
#include "Navigator.h"
#include "NetworkStateNotifier.h"
#include "PageActivityAssertionToken.h"
#include "PageCache.h"
#include "PageConsole.h"
#include "PageDebuggable.h"
#include "PageGroup.h"
#include "PageThrottler.h"
#include "PlugInClient.h"
#include "PluginData.h"
#include "PluginView.h"
#include "PointerLockController.h"
#include "ProgressTracker.h"
#include "RenderLayerCompositor.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "RuntimeEnabledFeatures.h"
#include "SchemeRegistry.h"
#include "ScriptController.h"
#include "ScrollingCoordinator.h"
#include "Settings.h"
#include "SharedBuffer.h"
#include "StorageArea.h"
#include "StorageNamespace.h"
#include "StyleResolver.h"
#include "SubframeLoader.h"
#include "TextResourceDecoder.h"
#include "UserContentController.h"
#include "UserInputBridge.h"
#include "ViewStateChangeObserver.h"
#include "VisitedLinkState.h"
#include "VisitedLinkStore.h"
#include "VoidCallback.h"
#include "Widget.h"
#include <wtf/HashMap.h>
#include <wtf/RefCountedLeakCounter.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/Base64.h>
#include <wtf/text/StringHash.h>

#if ENABLE(WEB_REPLAY)
#include "ReplayController.h"
#include <replay/InputCursor.h>
#endif

namespace WebCore {

static HashSet<Page*>* allPages;

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, pageCounter, ("Page"));

static void networkStateChanged(bool isOnLine)
{
    Vector<Ref<Frame>> frames;
    
    // Get all the frames of all the pages in all the page groups
    for (auto it = allPages->begin(), end = allPages->end(); it != end; ++it) {
        for (Frame* frame = &(*it)->mainFrame(); frame; frame = frame->tree().traverseNext())
            frames.append(*frame);
        InspectorInstrumentation::networkStateChanged(*it);
    }

    AtomicString eventName = isOnLine ? eventNames().onlineEvent : eventNames().offlineEvent;
    for (unsigned i = 0; i < frames.size(); i++)
        frames[i]->document()->dispatchWindowEvent(Event::create(eventName, false, false));
}

static const ViewState::Flags PageInitialViewState = ViewState::IsVisible | ViewState::IsInWindow;

Page::Page(PageClients& pageClients)
    : m_chrome(std::make_unique<Chrome>(*this, *pageClients.chromeClient))
    , m_dragCaretController(std::make_unique<DragCaretController>())
#if ENABLE(DRAG_SUPPORT)
    , m_dragController(std::make_unique<DragController>(*this, *pageClients.dragClient))
#endif
    , m_focusController(std::make_unique<FocusController>(*this, PageInitialViewState))
#if ENABLE(CONTEXT_MENUS)
    , m_contextMenuController(std::make_unique<ContextMenuController>(*this, *pageClients.contextMenuClient))
#endif
    , m_userInputBridge(std::make_unique<UserInputBridge>(*this))
#if ENABLE(WEB_REPLAY)
    , m_replayController(std::make_unique<ReplayController>(*this))
#endif
#if ENABLE(INSPECTOR)
    , m_inspectorController(std::make_unique<InspectorController>(*this, pageClients.inspectorClient))
#endif
#if ENABLE(POINTER_LOCK)
    , m_pointerLockController(std::make_unique<PointerLockController>(*this))
#endif
    , m_settings(Settings::create(this))
    , m_progress(std::make_unique<ProgressTracker>(*pageClients.progressTrackerClient))
    , m_backForwardController(std::make_unique<BackForwardController>(*this, pageClients.backForwardClient))
    , m_mainFrame(MainFrame::create(*this, *pageClients.loaderClientForMainFrame))
    , m_theme(RenderTheme::themeForPage(this))
    , m_editorClient(pageClients.editorClient)
    , m_plugInClient(pageClients.plugInClient)
    , m_validationMessageClient(pageClients.validationMessageClient)
    , m_subframeCount(0)
    , m_openedByDOM(false)
    , m_tabKeyCyclesThroughElements(true)
    , m_defersLoading(false)
    , m_defersLoadingCallCount(0)
    , m_inLowQualityInterpolationMode(false)
    , m_areMemoryCacheClientCallsEnabled(true)
    , m_mediaVolume(1)
    , m_pageScaleFactor(1)
    , m_zoomedOutPageScaleFactor(0)
    , m_deviceScaleFactor(1)
    , m_topContentInset(0)
#if ENABLE(IOS_TEXT_AUTOSIZING)
    , m_textAutosizingWidth(0)
#endif
    , m_suppressScrollbarAnimations(false)
    , m_verticalScrollElasticity(ScrollElasticityAllowed)
    , m_horizontalScrollElasticity(ScrollElasticityAllowed)
    , m_didLoadUserStyleSheet(false)
    , m_userStyleSheetModificationTime(0)
    , m_group(0)
    , m_debugger(0)
    , m_canStartMedia(true)
#if ENABLE(VIEW_MODE_CSS_MEDIA)
    , m_viewMode(ViewModeWindowed)
#endif // ENABLE(VIEW_MODE_CSS_MEDIA)
    , m_minimumTimerInterval(Settings::defaultMinDOMTimerInterval())
    , m_timerThrottlingEnabled(false)
    , m_timerAlignmentInterval(Settings::defaultDOMTimerAlignmentInterval())
    , m_isEditable(false)
    , m_isPrerender(false)
    , m_viewState(PageInitialViewState)
    , m_requestedLayoutMilestones(0)
    , m_headerHeight(0)
    , m_footerHeight(0)
    , m_isCountingRelevantRepaintedObjects(false)
#ifndef NDEBUG
    , m_isPainting(false)
#endif
    , m_alternativeTextClient(pageClients.alternativeTextClient)
    , m_scriptedAnimationsSuspended(false)
    , m_console(std::make_unique<PageConsole>(*this))
#if ENABLE(REMOTE_INSPECTOR)
    , m_inspectorDebuggable(std::make_unique<PageDebuggable>(*this))
#endif
    , m_lastSpatialNavigationCandidatesCount(0) // NOTE: Only called from Internals for Spatial Navigation testing.
    , m_framesHandlingBeforeUnloadEvent(0)
    , m_userContentController(WTF::move(pageClients.userContentController))
    , m_visitedLinkStore(WTF::move(pageClients.visitedLinkStore))
    , m_sessionID(SessionID::defaultSessionID())
    , m_isClosing(false)
{
    ASSERT(m_editorClient);
    
    setTimerThrottlingEnabled(m_viewState & ViewState::IsVisuallyIdle);

    if (m_userContentController)
        m_userContentController->addPage(*this);

    if (m_visitedLinkStore)
        m_visitedLinkStore->addPage(*this);

    if (!allPages) {
        allPages = new HashSet<Page*>;
        
        networkStateNotifier().addNetworkStateChangeListener(networkStateChanged);
    }

    ASSERT(!allPages->contains(this));
    allPages->add(this);

#ifndef NDEBUG
    pageCounter.increment();
#endif

#if ENABLE(REMOTE_INSPECTOR)
    m_inspectorDebuggable->init();
#endif
}

Page::~Page()
{
    m_mainFrame->setView(0);
    setGroupName(String());
    allPages->remove(this);
    
    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        frame->willDetachPage();
        frame->detachFromPage();
    }

    m_editorClient->pageDestroyed();
    if (m_plugInClient)
        m_plugInClient->pageDestroyed();
    if (m_alternativeTextClient)
        m_alternativeTextClient->pageDestroyed();

#if ENABLE(INSPECTOR)
    m_inspectorController->inspectedPageDestroyed();
#endif

    if (m_scrollingCoordinator)
        m_scrollingCoordinator->pageDestroyed();

    backForward().close();

#ifndef NDEBUG
    pageCounter.decrement();
#endif

    if (m_userContentController)
        m_userContentController->removePage(*this);
    if (m_visitedLinkStore)
        m_visitedLinkStore->removePage(*this);
}

void Page::clearPreviousItemFromAllPages(HistoryItem* item)
{
    if (!allPages)
        return;

    for (auto& page : *allPages) {
        HistoryController& controller = page->mainFrame().loader().history();
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

ViewportArguments Page::viewportArguments() const
{
    return mainFrame().document() ? mainFrame().document()->viewportArguments() : ViewportArguments();
}

ScrollingCoordinator* Page::scrollingCoordinator()
{
    if (!m_scrollingCoordinator && m_settings->scrollingCoordinatorEnabled()) {
        m_scrollingCoordinator = chrome().client().createScrollingCoordinator(this);
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

PassRefPtr<ClientRectList> Page::nonFastScrollableRects(const Frame* frame)
{
    if (Document* document = m_mainFrame->document())
        document->updateLayout();

    Vector<IntRect> rects;
    if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
        rects = scrollingCoordinator->computeNonFastScrollableRegion(frame, IntPoint()).rects();

    Vector<FloatQuad> quads(rects.size());
    for (size_t i = 0; i < rects.size(); ++i)
        quads[i] = FloatRect(rects[i]);
    return ClientRectList::create(quads);
}

#if ENABLE(VIEW_MODE_CSS_MEDIA)
struct ViewModeInfo {
    const char* name;
    Page::ViewMode type;
};
static const int viewModeMapSize = 5;
static ViewModeInfo viewModeMap[viewModeMapSize] = {
    {"windowed", Page::ViewModeWindowed},
    {"floating", Page::ViewModeFloating},
    {"fullscreen", Page::ViewModeFullscreen},
    {"maximized", Page::ViewModeMaximized},
    {"minimized", Page::ViewModeMinimized}
};

Page::ViewMode Page::stringToViewMode(const String& text)
{
    for (int i = 0; i < viewModeMapSize; ++i) {
        if (text == viewModeMap[i].name)
            return viewModeMap[i].type;
    }
    return Page::ViewModeInvalid;
}

void Page::setViewMode(ViewMode viewMode)
{
    if (viewMode == m_viewMode || viewMode == ViewModeInvalid)
        return;

    m_viewMode = viewMode;

    if (!m_mainFrame)
        return;

    if (m_mainFrame->view())
        m_mainFrame->view()->forceLayout();

    if (m_mainFrame->document())
        m_mainFrame->document()->styleResolverChanged(RecalcStyleImmediately);
}
#endif // ENABLE(VIEW_MODE_CSS_MEDIA)

bool Page::openedByDOM() const
{
    return m_openedByDOM;
}

void Page::setOpenedByDOM()
{
    m_openedByDOM = true;
}

void Page::goToItem(HistoryItem* item, FrameLoadType type)
{
    // stopAllLoaders may end up running onload handlers, which could cause further history traversals that may lead to the passed in HistoryItem
    // being deref()-ed. Make sure we can still use it with HistoryController::goToItem later.
    RefPtr<HistoryItem> protector(item);

    if (m_mainFrame->loader().history().shouldStopLoadingForHistoryItem(item))
        m_mainFrame->loader().stopAllLoaders();

    m_mainFrame->loader().history().goToItem(item, type);
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
    return m_group ? m_group->name() : nullAtom.string();
}

void Page::initGroup()
{
    ASSERT(!m_singlePageGroup);
    ASSERT(!m_group);
    m_singlePageGroup = std::make_unique<PageGroup>(*this);
    m_group = m_singlePageGroup.get();
}

void Page::updateStyleForAllPagesAfterGlobalChangeInEnvironment()
{
    if (!allPages)
        return;
    HashSet<Page*>::iterator end = allPages->end();
    for (HashSet<Page*>::iterator it = allPages->begin(); it != end; ++it)
        for (Frame* frame = &(*it)->mainFrame(); frame; frame = frame->tree().traverseNext()) {
            // If a change in the global environment has occurred, we need to
            // make sure all the properties a recomputed, therefore we invalidate
            // the properties cache.
            if (StyleResolver* styleResolver = frame->document()->styleResolverIfExists())
                styleResolver->invalidateMatchedPropertiesCache();
            frame->document()->scheduleForcedStyleRecalc();
        }
}

void Page::setNeedsRecalcStyleInAllFrames()
{
    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (Document* document = frame->document())
            document->styleResolverChanged(DeferRecalcStyle);
    }
}

void Page::refreshPlugins(bool reload)
{
    if (!allPages)
        return;

    PluginData::refresh();

    Vector<Ref<Frame>> framesNeedingReload;

    for (auto it = allPages->begin(), end = allPages->end(); it != end; ++it) {
        Page& page = **it;
        page.m_pluginData.clear();

        if (!reload)
            continue;
        
        for (Frame* frame = &page.mainFrame(); frame; frame = frame->tree().traverseNext()) {
            if (frame->loader().subframeLoader().containsPlugins())
                framesNeedingReload.append(*frame);
        }
    }

    for (size_t i = 0; i < framesNeedingReload.size(); ++i)
        framesNeedingReload[i]->loader().reload();
}

PluginData& Page::pluginData() const
{
    if (!m_pluginData)
        m_pluginData = PluginData::create(this);
    return *m_pluginData;
}

inline MediaCanStartListener* Page::takeAnyMediaCanStartListener()
{
    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (MediaCanStartListener* listener = frame->document()->takeAnyMediaCanStartListener())
            return listener;
    }
    return 0;
}

void Page::setCanStartMedia(bool canStartMedia)
{
    if (m_canStartMedia == canStartMedia)
        return;

    m_canStartMedia = canStartMedia;

    while (m_canStartMedia) {
        MediaCanStartListener* listener = takeAnyMediaCanStartListener();
        if (!listener)
            break;
        listener->mediaCanStart();
    }
}

static Frame* incrementFrame(Frame* curr, bool forward, bool wrapFlag)
{
    return forward
        ? curr->tree().traverseNextWithWrap(wrapFlag)
        : curr->tree().traversePreviousWithWrap(wrapFlag);
}

bool Page::findString(const String& target, FindOptions options)
{
    if (target.isEmpty())
        return false;

    bool shouldWrap = options & WrapAround;
    Frame* frame = &focusController().focusedOrMainFrame();
    Frame* startFrame = frame;
    do {
        if (frame->editor().findString(target, (options & ~WrapAround) | StartInSelection)) {
            if (frame != startFrame)
                startFrame->selection().clear();
            focusController().setFocusedFrame(frame);
            return true;
        }
        frame = incrementFrame(frame, !(options & Backwards), shouldWrap);
    } while (frame && frame != startFrame);

    // Search contents of startFrame, on the other side of the selection that we did earlier.
    // We cheat a bit and just research with wrap on
    if (shouldWrap && !startFrame->selection().isNone()) {
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
    Frame* frameWithSelection = 0;
    do {
        frame->editor().countMatchesForText(target, 0, options, limit ? (limit - matchRanges.size()) : 0, true, &matchRanges);
        if (frame->selection().isRange())
            frameWithSelection = frame;
        frame = incrementFrame(frame, true, false);
    } while (frame);

    if (matchRanges.isEmpty())
        return;

    if (frameWithSelection) {
        indexForSelection = NoMatchAfterUserSelection;
        RefPtr<Range> selectedRange = frameWithSelection->selection().selection().firstRange();
        if (options & Backwards) {
            for (size_t i = matchRanges.size(); i > 0; --i) {
                if (selectedRange->compareBoundaryPoints(Range::END_TO_START, matchRanges[i - 1].get(), IGNORE_EXCEPTION) > 0) {
                    indexForSelection = i - 1;
                    break;
                }
            }
        } else {
            for (size_t i = 0, size = matchRanges.size(); i < size; ++i) {
                if (selectedRange->compareBoundaryPoints(Range::START_TO_END, matchRanges[i].get(), IGNORE_EXCEPTION) < 0) {
                    indexForSelection = i;
                    break;
                }
            }
        }
    } else {
        if (options & Backwards)
            indexForSelection = matchRanges.size() - 1;
        else
            indexForSelection = 0;
    }
}

PassRefPtr<Range> Page::rangeOfString(const String& target, Range* referenceRange, FindOptions options)
{
    if (target.isEmpty())
        return 0;

    if (referenceRange && referenceRange->ownerDocument().page() != this)
        return 0;

    bool shouldWrap = options & WrapAround;
    Frame* frame = referenceRange ? referenceRange->ownerDocument().frame() : &mainFrame();
    Frame* startFrame = frame;
    do {
        if (RefPtr<Range> resultRange = frame->editor().rangeOfString(target, frame == startFrame ? referenceRange : 0, options & ~WrapAround))
            return resultRange.release();

        frame = incrementFrame(frame, !(options & Backwards), shouldWrap);
    } while (frame && frame != startFrame);

    // Search contents of startFrame, on the other side of the reference range that we did earlier.
    // We cheat a bit and just search again with wrap on.
    if (shouldWrap && referenceRange) {
        if (RefPtr<Range> resultRange = startFrame->editor().rangeOfString(target, referenceRange, options | WrapAround | StartInSelection))
            return resultRange.release();
    }

    return 0;
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
        frame = incrementFrame(frame, true, false);
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
        frame = incrementFrame(frame, true, false);
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

void Page::setMediaVolume(float volume)
{
    if (volume < 0 || volume > 1)
        return;

    if (m_mediaVolume == volume)
        return;

    m_mediaVolume = volume;
    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
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
    Document* document = mainFrame().document();
    FrameView* view = document->view();

    if (scale == m_pageScaleFactor) {
        if (view && view->scrollPosition() != origin) {
            if (!m_settings->delegatesPageScaling())
                document->updateLayoutIgnorePendingStylesheets();

            if (!view->delegatesScrolling())
                view->setScrollPosition(origin);
#if USE(TILED_BACKING_STORE)
            else
                view->hostWindow()->delegatedScrollRequested(origin);
#endif
        }
#if ENABLE(MEDIA_CONTROLS_SCRIPT)
        if (inStableState) {
            for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext())
                frame->document()->pageScaleFactorChangedAndStable();
        }
#endif
        return;
    }

    m_pageScaleFactor = scale;

    if (!m_settings->delegatesPageScaling()) {
        if (document->renderView())
            document->renderView()->setNeedsLayout();

        document->recalcStyle(Style::Force);

        // Transform change on RenderView doesn't trigger repaint on non-composited contents.
        mainFrame().view()->invalidateRect(IntRect(LayoutRect::infiniteRect()));
    }

    mainFrame().deviceOrPageScaleFactorChanged();

    if (view && view->fixedElementsLayoutRelativeToFrame())
        view->setViewportConstrainedObjectsNeedLayout();

    if (view && view->scrollPosition() != origin) {
        if (!m_settings->delegatesPageScaling() && document->renderView() && document->renderView()->needsLayout() && view->didFirstLayout())
            view->layout();
        
        if (!view->delegatesScrolling())
            view->setScrollPosition(origin);
#if USE(TILED_BACKING_STORE)
        else
            view->hostWindow()->delegatedScrollRequested(origin);
#endif
    }

#if ENABLE(MEDIA_CONTROLS_SCRIPT)
    if (inStableState) {
        for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext())
            frame->document()->pageScaleFactorChangedAndStable();
    }
#endif
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
    pageCache()->markPagesForDeviceScaleChanged(this);

    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext())
        frame->editor().deviceScaleFactorChanged();

    pageCache()->markPagesForFullStyleRecalc(this);
    GraphicsContext::updateDocumentMarkerResources();
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

        for (HashSet<ScrollableArea*>::const_iterator it = scrollableAreas->begin(), end = scrollableAreas->end(); it != end; ++it) {
            ScrollableArea* scrollableArea = *it;
            scrollableArea->lockOverlayScrollbarStateToHidden(lockOverlayScrollbars);
        }
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
    pageCache()->markPagesForFullStyleRecalc(this);
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
    setViewState(isInWindow ? m_viewState | ViewState::IsInWindow : m_viewState & ~ViewState::IsInWindow);
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

void Page::addViewStateChangeObserver(ViewStateChangeObserver& observer)
{
    m_viewStateChangeObservers.add(&observer);
}

void Page::removeViewStateChangeObserver(ViewStateChangeObserver& observer)
{
    m_viewStateChangeObservers.remove(&observer);
}

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

void Page::setIsVisuallyIdleInternal(bool isVisuallyIdle)
{
    setTimerThrottlingEnabled(isVisuallyIdle);
    
    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (frame->document())
            frame->document()->scriptedAnimationControllerSetThrottled(isVisuallyIdle);
    }
}

void Page::userStyleSheetLocationChanged()
{
    // FIXME: Eventually we will move to a model of just being handed the sheet
    // text instead of loading the URL ourselves.
    URL url = m_settings->userStyleSheetLocation();
    
    // Allow any local file URL scheme to be loaded.
    if (SchemeRegistry::shouldTreatURLSchemeAsLocal(url.protocol()))
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
        if (base64Decode(decodeURLEscapeSequences(url.string().substring(35)), styleSheetAsUTF8, Base64IgnoreWhitespace))
            m_userStyleSheet = String::fromUTF8(styleSheetAsUTF8.data(), styleSheetAsUTF8.size());
    }

    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (frame->document())
            frame->document()->styleSheetCollection().updatePageUserSheet();
    }
}

const String& Page::userStyleSheet() const
{
    if (m_userStyleSheetPath.isEmpty())
        return m_userStyleSheet;

    time_t modTime;
    if (!getFileModificationTime(m_userStyleSheetPath, modTime)) {
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

void Page::removeAllVisitedLinks()
{
    if (!allPages)
        return;
    HashSet<PageGroup*> groups;
    HashSet<Page*>::iterator pagesEnd = allPages->end();
    for (HashSet<Page*>::iterator it = allPages->begin(); it != pagesEnd; ++it) {
        if (PageGroup* group = (*it)->groupPtr())
            groups.add(group);
    }
    HashSet<PageGroup*>::iterator groupsEnd = groups.end();
    for (HashSet<PageGroup*>::iterator it = groups.begin(); it != groupsEnd; ++it)
        (*it)->removeVisitedLinks();
}

void Page::invalidateStylesForAllLinks()
{
    for (Frame* frame = m_mainFrame.get(); frame; frame = frame->tree().traverseNext())
        frame->document()->visitedLinkState().invalidateStyleForAllLinks();
}

void Page::invalidateStylesForLink(LinkHash linkHash)
{
    for (Frame* frame = m_mainFrame.get(); frame; frame = frame->tree().traverseNext())
        frame->document()->visitedLinkState().invalidateStyleForLink(linkHash);
}

void Page::setDebugger(JSC::Debugger* debugger)
{
    if (m_debugger == debugger)
        return;

    m_debugger = debugger;

    for (Frame* frame = m_mainFrame.get(); frame; frame = frame->tree().traverseNext())
        frame->script().attachDebugger(m_debugger);
}

StorageNamespace* Page::sessionStorage(bool optionalCreate)
{
    if (!m_sessionStorage && optionalCreate)
        m_sessionStorage = StorageNamespace::sessionStorageNamespace(this);

    return m_sessionStorage.get();
}

void Page::setSessionStorage(PassRefPtr<StorageNamespace> newStorage)
{
    m_sessionStorage = newStorage;
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

void Page::setMinimumTimerInterval(double minimumTimerInterval)
{
    double oldTimerInterval = m_minimumTimerInterval;
    m_minimumTimerInterval = minimumTimerInterval;
    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNextWithWrap(false)) {
        if (frame->document())
            frame->document()->adjustMinimumTimerInterval(oldTimerInterval);
    }
}

double Page::minimumTimerInterval() const
{
    return m_minimumTimerInterval;
}

void Page::hiddenPageDOMTimerThrottlingStateChanged()
{
    setTimerThrottlingEnabled(m_viewState & ViewState::IsVisuallyIdle);
}

void Page::setTimerThrottlingEnabled(bool enabled)
{
#if ENABLE(HIDDEN_PAGE_DOM_TIMER_THROTTLING)
    if (!m_settings->hiddenPageDOMTimerThrottlingEnabled())
        enabled = false;
#endif

    if (enabled == m_timerThrottlingEnabled)
        return;

    m_timerThrottlingEnabled = enabled;
    m_timerAlignmentInterval = enabled ? Settings::hiddenPageDOMTimerAlignmentInterval() : Settings::defaultDOMTimerAlignmentInterval();
    
    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNextWithWrap(false)) {
        if (frame->document())
            frame->document()->didChangeTimerAlignmentInterval();
    }
}

void Page::dnsPrefetchingStateChanged()
{
    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext())
        frame->document()->initDNSPrefetch();
}

Vector<Ref<PluginViewBase>> Page::pluginViews()
{
    Vector<Ref<PluginViewBase>> views;

    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        FrameView* view = frame->view();
        if (!view)
            break;

        for (auto it = view->children().begin(), end = view->children().end(); it != end; ++it) {
            Widget* widget = (*it).get();
            if (widget->isPluginViewBase())
                views.append(*toPluginViewBase(widget));
        }
    }

    return views;
}

void Page::storageBlockingStateChanged()
{
    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext())
        frame->document()->storageBlockingStateDidChange();

    // Collect the PluginViews in to a vector to ensure that action the plug-in takes
    // from below storageBlockingStateChanged does not affect their lifetime.
    auto views = pluginViews();

    for (unsigned i = 0; i < views.size(); ++i)
        views[i]->storageBlockingStateChanged();
}

void Page::enableLegacyPrivateBrowsing(bool privateBrowsingEnabled)
{
    // Don't allow changing the legacy private browsing state if we have set a session ID.
    ASSERT(m_sessionID == SessionID::defaultSessionID() || m_sessionID == SessionID::legacyPrivateSessionID());

    setSessionID(privateBrowsingEnabled ? SessionID::legacyPrivateSessionID() : SessionID::defaultSessionID());
}

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

void Page::createPageThrottler()
{
    ASSERT(!m_pageThrottler);
    m_pageThrottler = std::make_unique<PageThrottler>(*this, m_viewState);
}

void Page::setViewState(ViewState::Flags viewState)
{
    ViewState::Flags changed = m_viewState ^ viewState;
    if (!changed)
        return;

    ViewState::Flags oldViewState = m_viewState;

    m_viewState = viewState;
    m_focusController->setViewState(viewState);
    if (m_pageThrottler)
        m_pageThrottler->setViewState(viewState);

    if (changed & ViewState::IsVisible)
        setIsVisibleInternal(viewState & ViewState::IsVisible);
    if (changed & ViewState::IsInWindow)
        setIsInWindowInternal(viewState & ViewState::IsInWindow);
    if (changed & ViewState::IsVisuallyIdle)
        setIsVisuallyIdleInternal(viewState & ViewState::IsVisuallyIdle);

    for (auto* observer : m_viewStateChangeObservers)
        observer->viewStateDidChange(oldViewState, m_viewState);
}

void Page::setIsVisible(bool isVisible)
{
    if (isVisible)
        setViewState((m_viewState & ~ViewState::IsVisuallyIdle) | ViewState::IsVisible | ViewState::IsVisibleOrOccluded);
    else
        setViewState((m_viewState & ~(ViewState::IsVisible | ViewState::IsVisibleOrOccluded)) | ViewState::IsVisuallyIdle);
}

void Page::setIsVisibleInternal(bool isVisible)
{
    // FIXME: The visibility state should be stored on the top-level document.
    // https://bugs.webkit.org/show_bug.cgi?id=116769

    if (isVisible) {
        m_isPrerender = false;

        for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
            if (FrameView* frameView = frame->view())
                frameView->didMoveOnscreen();
        }

        resumeScriptedAnimations();

        if (FrameView* view = mainFrame().view())
            view->show();

        if (m_settings->hiddenPageCSSAnimationSuspensionEnabled())
            mainFrame().animation().resumeAnimations();

        resumeAnimatingImages();
    }

    Vector<Ref<Document>> documents;
    for (Frame* frame = m_mainFrame.get(); frame; frame = frame->tree().traverseNext())
        documents.append(*frame->document());

    for (size_t i = 0, size = documents.size(); i < size; ++i)
        documents[i]->visibilityStateChanged();

    if (!isVisible) {
        if (m_settings->hiddenPageCSSAnimationSuspensionEnabled())
            mainFrame().animation().suspendAnimations();

        for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
            if (FrameView* frameView = frame->view())
                frameView->willMoveOffscreen();
        }

        suspendScriptedAnimations();

        if (FrameView* view = mainFrame().view())
            view->hide();
    }
}

void Page::setIsPrerender()
{
    m_isPrerender = true;
}

PageVisibilityState Page::visibilityState() const
{
    if (isVisible())
        return PageVisibilityStateVisible;
    if (m_isPrerender)
        return PageVisibilityStatePrerender;
    return PageVisibilityStateHidden;
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

#if ENABLE(REMOTE_INSPECTOR)
bool Page::remoteInspectionAllowed() const
{
    return m_inspectorDebuggable->remoteDebuggingAllowed();
}

void Page::setRemoteInspectionAllowed(bool allowed)
{
    m_inspectorDebuggable->setRemoteDebuggingAllowed(allowed);
}

void Page::remoteInspectorInformationDidChange() const
{
    m_inspectorDebuggable->update();
}
#endif

void Page::addLayoutMilestones(LayoutMilestones milestones)
{
    // In the future, we may want a function that replaces m_layoutMilestones instead of just adding to it.
    m_requestedLayoutMilestones |= milestones;
}

void Page::removeLayoutMilestones(LayoutMilestones milestones)
{
    m_requestedLayoutMilestones &= ~milestones;
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
static double gMinimumPaintedAreaRatio = 0.1;
static double gMaximumUnpaintedAreaRatio = 0.04;

bool Page::isCountingRelevantRepaintedObjects() const
{
    return m_isCountingRelevantRepaintedObjects && (m_requestedLayoutMilestones & DidHitRelevantRepaintedObjectsAreaThreshold);
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
    // DidHitRelevantRepaintedObjectsAreaThreshold is a LayoutMilestone intended to indicate that
    // a certain relevant amount of content has been drawn to the screen. This is the rect that
    // has been determined to be relevant in the context of this goal. We may choose to tweak
    // the rect over time, much like we may choose to tweak gMinimumPaintedAreaRatio and
    // gMaximumUnpaintedAreaRatio. But this seems to work well right now.
    LayoutRect relevantViewRect = LayoutRect(0, 0, 980, 1300);

    LayoutRect viewRect = view->viewRect();
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
    if (!objectPaintRect.intersects(pixelSnappedIntRect(relevantRect)))
        return;

    IntRect snappedPaintRect = pixelSnappedIntRect(objectPaintRect);

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
        topIntersection.intersect(pixelSnappedIntRect(topRelevantRect));
        m_topRelevantPaintedRegion.unite(topIntersection);

        IntRect bottomIntersection = snappedPaintRect;
        bottomIntersection.intersect(pixelSnappedIntRect(bottomRelevantRect));
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
            frame->loader().didLayout(DidHitRelevantRepaintedObjectsAreaThreshold);
    }
}

void Page::addRelevantUnpaintedObject(RenderObject* object, const LayoutRect& objectPaintRect)
{
    if (!isCountingRelevantRepaintedObjects())
        return;

    // The objects are only relevant if they are being painted within the relevantViewRect().
    if (!objectPaintRect.intersects(pixelSnappedIntRect(relevantViewRect(&object->view()))))
        return;

    m_relevantUnpaintedRenderObjects.add(object);
    m_relevantUnpaintedRegion.unite(pixelSnappedIntRect(objectPaintRect));
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
        if (m_settings->hiddenPageCSSAnimationSuspensionEnabled())
            mainFrame().animation().suspendAnimations();
        else
            mainFrame().animation().resumeAnimations();
    }
}

#if ENABLE(VIDEO_TRACK)
void Page::captionPreferencesChanged()
{
    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext())
        frame->document()->captionPreferencesChanged();
}
#endif

void Page::incrementFrameHandlingBeforeUnloadEventCount()
{
    ++m_framesHandlingBeforeUnloadEvent;
}

void Page::decrementFrameHandlingBeforeUnloadEventCount()
{
    ASSERT(m_framesHandlingBeforeUnloadEvent);
    --m_framesHandlingBeforeUnloadEvent;
}

bool Page::isAnyFrameHandlingBeforeUnloadEvent()
{
    return m_framesHandlingBeforeUnloadEvent;
}

void Page::setUserContentController(UserContentController* userContentController)
{
    if (m_userContentController)
        m_userContentController->removePage(*this);

    m_userContentController = userContentController;

    if (m_userContentController)
        m_userContentController->addPage(*this);
}

VisitedLinkStore& Page::visitedLinkStore()
{
    if (m_visitedLinkStore)
        return *m_visitedLinkStore;

    return group().visitedLinkStore();
}

SessionID Page::sessionID() const
{
    return m_sessionID;
}

void Page::setSessionID(SessionID sessionID)
{
    ASSERT(sessionID.isValid());

    bool privateBrowsingStateChanged = (sessionID.isEphemeral() != m_sessionID.isEphemeral());

    m_sessionID = sessionID;

    if (!privateBrowsingStateChanged)
        return;

    for (Frame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext())
        frame->document()->privateBrowsingStateDidChange();

    // Collect the PluginViews in to a vector to ensure that action the plug-in takes
    // from below privateBrowsingStateChanged does not affect their lifetime.

    for (auto& view : pluginViews())
        view->privateBrowsingStateChanged(sessionID.isEphemeral());

}

Page::PageClients::PageClients()
    : alternativeTextClient(nullptr)
    , chromeClient(nullptr)
#if ENABLE(CONTEXT_MENUS)
    , contextMenuClient(nullptr)
#endif
    , editorClient(nullptr)
    , dragClient(nullptr)
    , inspectorClient(nullptr)
    , plugInClient(nullptr)
    , progressTrackerClient(nullptr)
    , validationMessageClient(nullptr)
    , loaderClientForMainFrame(nullptr)
{
}

Page::PageClients::~PageClients()
{
}

} // namespace WebCore
