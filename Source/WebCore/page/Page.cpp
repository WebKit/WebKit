/*
 * Copyright (C) 2006-2020 Apple Inc. All rights reserved.
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
#include "AnimationFrameRate.h"
#include "AppHighlightStorage.h"
#include "ApplicationCacheStorage.h"
#include "AttachmentElementClient.h"
#include "AuthenticatorCoordinator.h"
#include "AuthenticatorCoordinatorClient.h"
#include "BackForwardCache.h"
#include "BackForwardClient.h"
#include "BackForwardController.h"
#include "BroadcastChannelRegistry.h"
#include "CacheStorageProvider.h"
#include "CachedImage.h"
#include "CachedResourceLoader.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "CommonAtomStrings.h"
#include "ConstantPropertyMap.h"
#include "ContextMenuClient.h"
#include "ContextMenuController.h"
#include "CookieJar.h"
#include "DOMRect.h"
#include "DOMRectList.h"
#include "DatabaseProvider.h"
#include "DebugOverlayRegions.h"
#include "DebugPageOverlays.h"
#include "DeprecatedGlobalSettings.h"
#include "DiagnosticLoggingClient.h"
#include "DiagnosticLoggingKeys.h"
#include "DisplayRefreshMonitorManager.h"
#include "DocumentInlines.h"
#include "DocumentLoader.h"
#include "DocumentMarkerController.h"
#include "DocumentTimelinesController.h"
#include "DragController.h"
#include "Editing.h"
#include "Editor.h"
#include "EditorClient.h"
#include "EmptyClients.h"
#include "Event.h"
#include "EventHandler.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "ExtensionStyleSheets.h"
#include "FocusController.h"
#include "FontCache.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameSelection.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "FullscreenManager.h"
#include "GeolocationController.h"
#include "HTMLElement.h"
#include "HTMLImageElement.h"
#include "HTMLMediaElement.h"
#include "HTMLTextAreaElement.h"
#include "HTMLTextFormControlElement.h"
#include "HistoryController.h"
#include "HistoryItem.h"
#include "IDBConnectionToServer.h"
#include "ImageAnalysisQueue.h"
#include "ImageOverlay.h"
#include "ImageOverlayController.h"
#include "InspectorClient.h"
#include "InspectorController.h"
#include "InspectorInstrumentation.h"
#include "LayoutDisallowedScope.h"
#include "LegacySchemeRegistry.h"
#include "LoaderStrategy.h"
#include "LogInitialization.h"
#include "Logging.h"
#include "LowPowerModeNotifier.h"
#include "MediaCanStartListener.h"
#include "MediaRecorderProvider.h"
#include "ModelPlayerProvider.h"
#include "Navigator.h"
#include "PageColorSampler.h"
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
#include "PlatformScreen.h"
#include "PlatformStrategies.h"
#include "PluginData.h"
#include "PluginInfoProvider.h"
#include "PluginViewBase.h"
#include "PointerCaptureController.h"
#include "PointerLockController.h"
#include "ProgressTracker.h"
#include "Range.h"
#include "RenderDescendantIterator.h"
#include "RenderImage.h"
#include "RenderLayerCompositor.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "RenderingUpdateScheduler.h"
#include "ResizeObserver.h"
#include "ResourceUsageOverlay.h"
#include "SVGDocumentExtensions.h"
#include "SVGImage.h"
#include "ScreenOrientationManager.h"
#include "ScriptController.h"
#include "ScriptDisallowedScope.h"
#include "ScriptRunner.h"
#include "ScriptedAnimationController.h"
#include "ScrollLatchingController.h"
#include "ScrollingCoordinator.h"
#include "ServiceWorkerGlobalScope.h"
#include "Settings.h"
#include "SharedBuffer.h"
#include "SocketProvider.h"
#include "SpeechRecognitionProvider.h"
#include "SpeechSynthesisClient.h"
#include "StorageArea.h"
#include "StorageNamespace.h"
#include "StorageNamespaceProvider.h"
#include "StorageProvider.h"
#include "StyleAdjuster.h"
#include "StyleResolver.h"
#include "StyleScope.h"
#include "SubframeLoader.h"
#include "TextIterator.h"
#include "TextRecognitionResult.h"
#include "TextResourceDecoder.h"
#include "UserContentProvider.h"
#include "UserContentURLPattern.h"
#include "UserInputBridge.h"
#include "UserScript.h"
#include "UserStyleSheet.h"
#include "ValidationMessageClient.h"
#include "VisitedLinkState.h"
#include "VisitedLinkStore.h"
#include "VoidCallback.h"
#include "WebCoreJSClientData.h"
#include "WebRTCProvider.h"
#include "WheelEventDeltaFilter.h"
#include "WheelEventTestMonitor.h"
#include "Widget.h"
#include "WorkerOrWorkletScriptController.h"
#include <wtf/FileSystem.h>
#include <wtf/RefCountedLeakCounter.h>
#include <wtf/StdLibExtras.h>
#include <wtf/SystemTracing.h>
#include <wtf/text/Base64.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/TextStream.h>

#if ENABLE(APPLE_PAY_AMS_UI)
#include "ApplePayAMSUIPaymentHandler.h"
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
#include "HTMLVideoElement.h"
#include "MediaPlaybackTarget.h"
#endif

#if PLATFORM(MAC)
#include "ServicesOverlayController.h"
#endif

#if ENABLE(WEBGL)
#include "WebGLStateTracker.h"
#endif

#include "DisplayView.h"

#if ENABLE(MEDIA_SESSION_COORDINATOR)
#include "MediaSessionCoordinator.h"
#include "NavigatorMediaSession.h"
#endif

namespace WebCore {

static HashSet<Page*>& allPages()
{
    static NeverDestroyed<HashSet<Page*>> set;
    return set;
}

static unsigned gNonUtilityPageCount { 0 };

static inline bool isUtilityPageChromeClient(ChromeClient& chromeClient)
{
    return chromeClient.isEmptyChromeClient() || chromeClient.isSVGImageChromeClient();
}

unsigned Page::nonUtilityPageCount()
{
    return gNonUtilityPageCount;
}

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, pageCounter, ("Page"));

void Page::forEachPage(const Function<void(Page&)>& function)
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
        for (AbstractFrame* frame = &page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
            auto* localFrame = dynamicDowncast<LocalFrame>(frame);
            if (!localFrame)
                continue;
            frames.append(*localFrame);
        }
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
    : m_chrome(makeUnique<Chrome>(*this, *pageConfiguration.chromeClient))
    , m_dragCaretController(makeUnique<DragCaretController>())
#if ENABLE(DRAG_SUPPORT)
    , m_dragController(makeUnique<DragController>(*this, WTFMove(pageConfiguration.dragClient)))
#endif
    , m_focusController(makeUnique<FocusController>(*this, pageInitialActivityState()))
#if ENABLE(CONTEXT_MENUS)
    , m_contextMenuController(makeUnique<ContextMenuController>(*this, *pageConfiguration.contextMenuClient))
#endif
    , m_userInputBridge(makeUnique<UserInputBridge>(*this))
    , m_inspectorController(makeUnique<InspectorController>(*this, pageConfiguration.inspectorClient))
    , m_pointerCaptureController(makeUnique<PointerCaptureController>(*this))
#if ENABLE(POINTER_LOCK)
    , m_pointerLockController(makeUnique<PointerLockController>(*this))
#endif
    , m_settings(Settings::create(this))
    , m_progress(makeUnique<ProgressTracker>(*this, WTFMove(pageConfiguration.progressTrackerClient)))
    , m_backForwardController(makeUnique<BackForwardController>(*this, WTFMove(pageConfiguration.backForwardClient)))
    , m_mainFrame(Frame::create(this, nullptr, WTFMove(pageConfiguration.loaderClientForMainFrame), pageConfiguration.mainFrameIdentifier ? *pageConfiguration.mainFrameIdentifier : FrameIdentifier::generate()))
    , m_editorClient(WTFMove(pageConfiguration.editorClient))
    , m_validationMessageClient(WTFMove(pageConfiguration.validationMessageClient))
    , m_diagnosticLoggingClient(WTFMove(pageConfiguration.diagnosticLoggingClient))
    , m_performanceLoggingClient(WTFMove(pageConfiguration.performanceLoggingClient))
#if ENABLE(WEBGL)
    , m_webGLStateTracker(WTFMove(pageConfiguration.webGLStateTracker))
#endif
#if ENABLE(SPEECH_SYNTHESIS)
    , m_speechSynthesisClient(WTFMove(pageConfiguration.speechSynthesisClient))
#endif
    , m_speechRecognitionProvider((WTFMove(pageConfiguration.speechRecognitionProvider)))
    , m_mediaRecorderProvider((WTFMove(pageConfiguration.mediaRecorderProvider)))
    , m_webRTCProvider(WTFMove(pageConfiguration.webRTCProvider))
    , m_domTimerAlignmentInterval(DOMTimer::defaultAlignmentInterval())
    , m_domTimerAlignmentIntervalIncreaseTimer(*this, &Page::domTimerAlignmentIntervalIncreaseTimerFired)
    , m_activityState(pageInitialActivityState())
    , m_alternativeTextClient(WTFMove(pageConfiguration.alternativeTextClient))
    , m_consoleClient(makeUnique<PageConsoleClient>(*this))
#if ENABLE(REMOTE_INSPECTOR)
    , m_inspectorDebuggable(makeUnique<PageDebuggable>(*this))
#endif
    , m_socketProvider(WTFMove(pageConfiguration.socketProvider))
    , m_cookieJar(WTFMove(pageConfiguration.cookieJar))
    , m_applicationCacheStorage(*WTFMove(pageConfiguration.applicationCacheStorage))
    , m_cacheStorageProvider(WTFMove(pageConfiguration.cacheStorageProvider))
    , m_databaseProvider(*WTFMove(pageConfiguration.databaseProvider))
    , m_pluginInfoProvider(*WTFMove(pageConfiguration.pluginInfoProvider))
    , m_storageNamespaceProvider(*WTFMove(pageConfiguration.storageNamespaceProvider))
    , m_userContentProvider(WTFMove(pageConfiguration.userContentProvider))
    , m_screenOrientationManager(WTFMove(pageConfiguration.screenOrientationManager))
    , m_visitedLinkStore(*WTFMove(pageConfiguration.visitedLinkStore))
    , m_broadcastChannelRegistry(WTFMove(pageConfiguration.broadcastChannelRegistry))
    , m_sessionID(pageConfiguration.sessionID)
#if ENABLE(VIDEO)
    , m_playbackControlsManagerUpdateTimer(*this, &Page::playbackControlsManagerUpdateTimerFired)
#endif
    , m_isUtilityPage(isUtilityPageChromeClient(chrome().client()))
    , m_performanceMonitor(isUtilityPage() ? nullptr : makeUnique<PerformanceMonitor>(*this))
    , m_lowPowerModeNotifier(makeUnique<LowPowerModeNotifier>([this](bool isLowPowerModeEnabled) { handleLowModePowerChange(isLowPowerModeEnabled); }))
    , m_performanceLogging(makeUnique<PerformanceLogging>(*this))
#if PLATFORM(MAC) && (ENABLE(SERVICE_CONTROLS) || ENABLE(TELEPHONE_NUMBER_DETECTION))
    , m_servicesOverlayController(makeUnique<ServicesOverlayController>(*this))
#endif
    , m_recentWheelEventDeltaFilter(WheelEventDeltaFilter::create())
    , m_pageOverlayController(makeUnique<PageOverlayController>(*this))
#if ENABLE(APPLE_PAY)
    , m_paymentCoordinator(makeUnique<PaymentCoordinator>(*pageConfiguration.paymentCoordinatorClient))
#endif
#if ENABLE(WEB_AUTHN)
    , m_authenticatorCoordinator(makeUniqueRef<AuthenticatorCoordinator>(WTFMove(pageConfiguration.authenticatorCoordinatorClient)))
#endif
#if ENABLE(APPLICATION_MANIFEST)
    , m_applicationManifest(pageConfiguration.applicationManifest)
#endif
#if ENABLE(DEVICE_ORIENTATION) && PLATFORM(IOS_FAMILY)
    , m_deviceOrientationUpdateProvider(WTFMove(pageConfiguration.deviceOrientationUpdateProvider))
#endif
    , m_corsDisablingPatterns(WTFMove(pageConfiguration.corsDisablingPatterns))
    , m_maskedURLSchemes(WTFMove(pageConfiguration.maskedURLSchemes))
    , m_allowedNetworkHosts(WTFMove(pageConfiguration.allowedNetworkHosts))
    , m_loadsSubresources(pageConfiguration.loadsSubresources)
    , m_shouldRelaxThirdPartyCookieBlocking(pageConfiguration.shouldRelaxThirdPartyCookieBlocking)
    , m_httpsUpgradeEnabled(pageConfiguration.httpsUpgradeEnabled)
    , m_storageProvider(WTFMove(pageConfiguration.storageProvider))
    , m_modelPlayerProvider(WTFMove(pageConfiguration.modelPlayerProvider))
#if ENABLE(ATTACHMENT_ELEMENT)
    , m_attachmentElementClient(WTFMove(pageConfiguration.attachmentElementClient))
#endif
    , m_contentSecurityPolicyModeForExtension(WTFMove(pageConfiguration.contentSecurityPolicyModeForExtension))
{
    updateTimerThrottlingState();

    m_pluginInfoProvider->addPage(*this);
    m_userContentProvider->addPage(*this);
    m_visitedLinkStore->addPage(*this);

    static bool firstTimeInitializationRan = false;
    if (!firstTimeInitializationRan) {
        firstTimeInitialization();
        firstTimeInitializationRan = true;
    }

    ASSERT(!allPages().contains(this));
    allPages().add(this);

    if (!isUtilityPage()) {
        ++gNonUtilityPageCount;
        MemoryPressureHandler::setPageCount(gNonUtilityPageCount);
    }

#ifndef NDEBUG
    pageCounter.increment();
#endif

    m_storageNamespaceProvider->setSessionStorageQuota(m_settings->sessionStorageQuota());

#if ENABLE(REMOTE_INSPECTOR)
    if (m_inspectorController->inspectorClient() && m_inspectorController->inspectorClient()->allowRemoteInspectionToPageDirectly())
        m_inspectorDebuggable->init();
#endif

#if PLATFORM(COCOA)
    platformInitialize();
#endif

    settingsDidChange();

    if (!pageConfiguration.userScriptsShouldWaitUntilNotification)
        m_hasBeenNotifiedToInjectUserScripts = true;

    if (m_lowPowerModeNotifier->isLowPowerModeEnabled())
        m_throttlingReasons.add(ThrottlingReason::LowPowerMode);
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
        --gNonUtilityPageCount;
        MemoryPressureHandler::setPageCount(gNonUtilityPageCount);
    }
    
    m_settings->pageDestroyed();

    m_inspectorController->inspectedPageDestroyed();

    forEachFrame([] (Frame& frame) {
        frame.willDetachPage();
        frame.detachFromPage();
    });

    if (m_scrollingCoordinator)
        m_scrollingCoordinator->pageDestroyed();

    backForward().close();
    if (!isUtilityPage())
        BackForwardCache::singleton().removeAllItemsForPage(*this);

#ifndef NDEBUG
    pageCounter.decrement();
#endif

    m_pluginInfoProvider->removePage(*this);
    m_userContentProvider->removePage(*this);
    m_visitedLinkStore->removePage(*this);
}

void Page::firstTimeInitialization()
{
    platformStrategies()->loaderStrategy()->addOnlineStateChangeListener(&networkStateChanged);

    FontCache::registerFontCacheInvalidationCallback([] {
        updateStyleForAllPagesAfterGlobalChangeInEnvironment();
    });
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
    forEachDocument([&] (Document& document) {
        if (auto* renderView = document.renderView())
            total += renderView->rendererCount();
    });
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

void Page::setOverrideViewportArguments(const std::optional<ViewportArguments>& viewportArguments)
{
    if (viewportArguments == m_overrideViewportArguments)
        return;

    m_overrideViewportArguments = viewportArguments;
    if (auto* document = mainFrame().document())
        document->updateViewportArguments();
}

ScrollingCoordinator* Page::scrollingCoordinator()
{
    if (!m_scrollingCoordinator && m_settings->scrollingCoordinatorEnabled()) {
        m_scrollingCoordinator = chrome().client().createScrollingCoordinator(*this);
        if (!m_scrollingCoordinator)
            m_scrollingCoordinator = ScrollingCoordinator::create(this);

        m_scrollingCoordinator->windowScreenDidChange(m_displayID, m_displayNominalFramesPerSecond);
    }

    return m_scrollingCoordinator.get();
}

String Page::scrollingStateTreeAsText()
{
    if (Document* document = m_mainFrame->document()) {
        document->updateLayout();
#if ENABLE(IOS_TOUCH_EVENTS)
        document->updateTouchEventRegions();
#endif
    }

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

Ref<DOMRectList> Page::nonFastScrollableRectsForTesting()
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
        for (const auto& synchronousEventRegion : eventTrackingRegions.eventSpecificSynchronousDispatchRegions)
            rects.appendVector(synchronousEventRegion.value.rects());
    }

    Vector<FloatQuad> quads(rects.size());
    for (size_t i = 0; i < rects.size(); ++i)
        quads[i] = FloatRect(rects[i]);

    return DOMRectList::create(quads);
}

Ref<DOMRectList> Page::touchEventRectsForEventForTesting(EventTrackingRegions::EventType eventType)
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
        const auto& region = eventTrackingRegions.eventSpecificSynchronousDispatchRegions.get(eventType);
        rects.appendVector(region.rects());
    }

    Vector<FloatQuad> quads(rects.size());
    for (size_t i = 0; i < rects.size(); ++i)
        quads[i] = FloatRect(rects[i]);

    return DOMRectList::create(quads);
}

Ref<DOMRectList> Page::passiveTouchEventListenerRectsForTesting()
{
    if (Document* document = m_mainFrame->document()) {
        document->updateLayout();
#if ENABLE(IOS_TOUCH_EVENTS)
        document->updateTouchEventRegions();
#endif  
    }

    Vector<IntRect> rects;
    if (auto* scrollingCoordinator = this->scrollingCoordinator())
        rects.appendVector(scrollingCoordinator->absoluteEventTrackingRegions().asynchronousDispatchRegion.rects());

    Vector<FloatQuad> quads(rects.size());
    for (size_t i = 0; i < rects.size(); ++i)
        quads[i] = FloatRect(rects[i]);

    return DOMRectList::create(quads);
}

void Page::settingsDidChange()
{
#if USE(LIBWEBRTC)
    m_webRTCProvider->setH265Support(settings().webRTCH265CodecEnabled());
    m_webRTCProvider->setVP9Support(settings().webRTCVP9Profile0CodecEnabled(), settings().webRTCVP9Profile2CodecEnabled());
    m_webRTCProvider->setAV1Support(settings().webRTCAV1CodecEnabled());
#endif
}

std::optional<AXTreeData> Page::accessibilityTreeData() const
{
    auto* document = mainFrame().document();
    if (!document)
        return std::nullopt;

    if (auto* cache = document->existingAXObjectCache())
        return { cache->treeData() };
    return std::nullopt;
}

void Page::progressEstimateChanged(Frame& frameWithProgressUpdate) const
{
    if (auto* document = frameWithProgressUpdate.document()) {
        if (auto* axObjectCache = document->existingAXObjectCache())
            axObjectCache->updateLoadingProgress(progress().estimatedProgress());
    }
}

void Page::progressFinished(Frame& frameWithCompletedProgress) const
{
    if (auto* document = frameWithCompletedProgress.document()) {
        if (auto* axObjectCache = document->existingAXObjectCache())
            axObjectCache->loadingFinished();
    }
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

void Page::setBroadcastChannelRegistry(Ref<BroadcastChannelRegistry>&& broadcastChannelRegistry)
{
    m_broadcastChannelRegistry = WTFMove(broadcastChannelRegistry);
}

void Page::initGroup()
{
    ASSERT(!m_singlePageGroup);
    ASSERT(!m_group);
    m_singlePageGroup = makeUnique<PageGroup>(*this);
    m_group = m_singlePageGroup.get();
}

void Page::updateStyleAfterChangeInEnvironment()
{
    forEachDocument([] (Document& document) {
        if (auto* styleResolver = document.styleScope().resolverIfExists())
            styleResolver->invalidateMatchedDeclarationsCache();
        document.scheduleFullStyleRebuild();
        document.styleScope().didChangeStyleSheetEnvironment();
        document.scheduleRenderingUpdate(RenderingUpdateStep::MediaQueryEvaluation);
    });
}

void Page::updateStyleForAllPagesAfterGlobalChangeInEnvironment()
{
    for (auto* page : allPages())
        page->updateStyleAfterChangeInEnvironment();
}

void Page::setNeedsRecalcStyleInAllFrames()
{
    // FIXME: Figure out what this function is actually trying to add in different call sites.
    forEachDocument([] (Document& document) {
        document.styleScope().didChangeStyleSheetEnvironment();
    });
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
    for (AbstractFrame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        if (!localFrame->document())
            continue;
        if (MediaCanStartListener* listener = localFrame->document()->takeAnyMediaCanStartListener())
            return { { *listener, *localFrame->document() } };
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

static AbstractFrame* incrementFrame(AbstractFrame* curr, bool forward, CanWrap canWrap, DidWrap* didWrap = nullptr)
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
    CheckedRef focusController { *m_focusController };
    RefPtr<AbstractFrame> frame = &focusController->focusedOrMainFrame();
    RefPtr<Frame> startFrame = dynamicDowncast<LocalFrame>(frame.get());
    do {
        RefPtr<Frame> localFrame = dynamicDowncast<LocalFrame>(frame.get());
        if (!localFrame) {
            frame = incrementFrame(frame.get(), !options.contains(Backwards), canWrap, didWrap);
            continue;
        }
        if (localFrame->editor().findString(target, (options - WrapAround) | StartInSelection)) {
            if (localFrame != startFrame)
                startFrame->selection().clear();
            focusController->setFocusedFrame(localFrame.get());
            return true;
        }
        frame = incrementFrame(frame.get(), !options.contains(Backwards), canWrap, didWrap);
    } while (frame && frame != startFrame);

    // Search contents of startFrame, on the other side of the selection that we did earlier.
    // We cheat a bit and just research with wrap on
    if (canWrap == CanWrap::Yes && !startFrame->selection().isNone()) {
        if (didWrap)
            *didWrap = DidWrap::Yes;
        bool found = startFrame->editor().findString(target, options | WrapAround | StartInSelection);
        if (auto* localFrame = dynamicDowncast<LocalFrame>(frame.get()))
            focusController->setFocusedFrame(localFrame);
        return found;
    }

    return false;
}

#if ENABLE(IMAGE_ANALYSIS)
void Page::analyzeImagesForFindInPage(Function<void()>&& callback)
{
    if (settings().imageAnalysisDuringFindInPageEnabled()) {
        imageAnalysisQueue().setDidBecomeEmptyCallback(WTFMove(callback));
        if (RefPtr mainDocument = m_mainFrame->document())
            imageAnalysisQueue().enqueueAllImagesIfNeeded(*mainDocument, { }, { });
    }
}
#endif

auto Page::findTextMatches(const String& target, FindOptions options, unsigned limit, bool markMatches) -> MatchingRanges
{
    MatchingRanges result;

    RefPtr<AbstractFrame> frame = &mainFrame();
    RefPtr<Frame> frameWithSelection;
    do {
        RefPtr<Frame> localFrame = dynamicDowncast<LocalFrame>(frame.get());
        if (!localFrame) {
            frame = incrementFrame(frame.get(), true, CanWrap::No);
            continue;
        }
        localFrame->editor().countMatchesForText(target, { }, options, limit ? (limit - result.ranges.size()) : 0, markMatches, &result.ranges);
        if (localFrame->selection().isRange())
            frameWithSelection = localFrame;
        frame = incrementFrame(frame.get(), true, CanWrap::No);
    } while (frame);

    if (result.ranges.isEmpty())
        return result;

    if (frameWithSelection) {
        result.indexForSelection = NoMatchAfterUserSelection;
        auto selectedRange = *frameWithSelection->selection().selection().firstRange();
        if (options.contains(Backwards)) {
            for (size_t i = result.ranges.size(); i > 0; --i) {
                // FIXME: Seems like this should be is_gteq to correctly handle the same string found twice in a row.
                if (is_gt(treeOrder<ComposedTree>(selectedRange.start, result.ranges[i - 1].end))) {
                    result.indexForSelection = i - 1;
                    break;
                }
            }
        } else {
            for (size_t i = 0, size = result.ranges.size(); i < size; ++i) {
                // FIXME: Seems like this should be is_lteq to correctly handle the same string found twice in a row.
                if (is_lt(treeOrder<ComposedTree>(selectedRange.end, result.ranges[i].start))) {
                    result.indexForSelection = i;
                    break;
                }
            }
        }
    } else {
        if (options.contains(Backwards))
            result.indexForSelection = result.ranges.size() - 1;
        else
            result.indexForSelection = 0;
    }

    return result;
}

std::optional<SimpleRange> Page::rangeOfString(const String& target, const std::optional<SimpleRange>& referenceRange, FindOptions options)
{
    if (target.isEmpty())
        return std::nullopt;

    if (referenceRange && referenceRange->start.document().page() != this)
        return std::nullopt;

    CanWrap canWrap = options.contains(WrapAround) ? CanWrap::Yes : CanWrap::No;
    RefPtr<AbstractFrame> frame = referenceRange ? referenceRange->start.document().frame() : &mainFrame();
    RefPtr<Frame> startFrame = dynamicDowncast<LocalFrame>(frame.get());
    do {
        RefPtr<Frame> localFrame = dynamicDowncast<LocalFrame>(frame.get());
        if (!localFrame) {
            frame = incrementFrame(frame.get(), !options.contains(Backwards), canWrap);
            continue;
        }
        if (auto resultRange = localFrame->editor().rangeOfString(target, localFrame.get() == startFrame.get() ? referenceRange : std::nullopt, options - WrapAround))
            return resultRange;
        frame = incrementFrame(localFrame.get(), !options.contains(Backwards), canWrap);
    } while (frame && frame != startFrame);

    // Search contents of startFrame, on the other side of the reference range that we did earlier.
    // We cheat a bit and just search again with wrap on.
    if (canWrap == CanWrap::Yes && referenceRange) {
        if (auto resultRange = startFrame->editor().rangeOfString(target, *referenceRange, options | WrapAround | StartInSelection))
            return resultRange;
    }

    return std::nullopt;
}

unsigned Page::findMatchesForText(const String& target, FindOptions options, unsigned maxMatchCount, ShouldHighlightMatches shouldHighlightMatches, ShouldMarkMatches shouldMarkMatches)
{
    if (target.isEmpty())
        return 0;

    unsigned matchCount = 0;

    RefPtr<AbstractFrame> frame = &mainFrame();
    do {
        RefPtr<Frame> localFrame = dynamicDowncast<LocalFrame>(frame.get());
        if (!localFrame) {
            frame = incrementFrame(frame.get(), true, CanWrap::No);
            continue;
        }
        if (shouldMarkMatches == MarkMatches)
            localFrame->editor().setMarkedTextMatchesAreHighlighted(shouldHighlightMatches == HighlightMatches);
        matchCount += localFrame->editor().countMatchesForText(target, std::nullopt, options, maxMatchCount ? (maxMatchCount - matchCount) : 0, shouldMarkMatches == MarkMatches, nullptr);
        frame = incrementFrame(frame.get(), true, CanWrap::No);
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

struct FindReplacementRange {
    RefPtr<ContainerNode> root;
    CharacterRange range;
};

static void replaceRanges(Page& page, const Vector<FindReplacementRange>& ranges, const String& replacementText)
{
    HashMap<RefPtr<ContainerNode>, Vector<FindReplacementRange>> rangesByContainerNode;
    for (auto& range : ranges) {
        auto& rangeList = rangesByContainerNode.ensure(range.root, [] {
            return Vector<FindReplacementRange> { };
        }).iterator->value;

        // Ensure that ranges are sorted by their end offsets, per editing container.
        auto endOffsetForRange = range.range.location + range.range.length;
        auto insertionIndex = rangeList.size();
        for (auto iterator = rangeList.rbegin(); iterator != rangeList.rend(); ++iterator) {
            auto endOffsetBeforeInsertionIndex = iterator->range.location + iterator->range.length;
            if (endOffsetForRange >= endOffsetBeforeInsertionIndex)
                break;
            insertionIndex--;
        }
        rangeList.insert(insertionIndex, range);
    }

    HashMap<RefPtr<Frame>, unsigned> frameToTraversalIndexMap;
    unsigned currentFrameTraversalIndex = 0;
    for (AbstractFrame* frame = &page.mainFrame(); frame; frame = frame->tree().traverseNext()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        frameToTraversalIndexMap.set(localFrame, currentFrameTraversalIndex++);
    }

    // Likewise, iterate backwards (in document and frame order) through editing containers that contain text matches,
    // so that we're consistent with our backwards iteration behavior per editing container when replacing text.
    auto containerNodesInOrderOfReplacement = copyToVector(rangesByContainerNode.keys());
    std::sort(containerNodesInOrderOfReplacement.begin(), containerNodesInOrderOfReplacement.end(), [frameToTraversalIndexMap] (auto& firstNode, auto& secondNode) {
        if (firstNode == secondNode)
            return false;

        RefPtr firstFrame = firstNode->document().frame();
        if (!firstFrame)
            return true;

        RefPtr secondFrame = secondNode->document().frame();
        if (!secondFrame)
            return false;

        if (firstFrame == secondFrame) {
            // Must not use Node::compareDocumentPosition here because some editing roots are inside shadow roots.
            return is_gt(treeOrder<ComposedTree>(*firstNode, *secondNode));
        }

        return frameToTraversalIndexMap.get(firstFrame) > frameToTraversalIndexMap.get(secondFrame);
    });

    for (auto& container : containerNodesInOrderOfReplacement) {
        RefPtr frame = container->document().frame();
        if (!frame)
            continue;

        // Iterate backwards through ranges when replacing text, such that earlier text replacements don't clobber replacement ranges later on.
        auto& ranges = rangesByContainerNode.find(container)->value;
        for (auto iterator = ranges.rbegin(); iterator != ranges.rend(); ++iterator) {
            auto range = resolveCharacterRange(makeRangeSelectingNodeContents(*container), iterator->range);
            if (range.collapsed())
                continue;

            frame->selection().setSelectedRange(range, Affinity::Downstream, FrameSelection::ShouldCloseTyping::Yes);
            frame->editor().replaceSelectionWithText(replacementText, Editor::SelectReplacement::Yes, Editor::SmartReplace::No, EditAction::InsertReplacement);
        }
    }
}

uint32_t Page::replaceRangesWithText(const Vector<SimpleRange>& rangesToReplace, const String& replacementText, bool /*selectionOnly*/)
{
    // FIXME: In the future, we should respect the `selectionOnly` flag by checking whether each range being replaced is contained within its frame's selection.

    Vector<FindReplacementRange> replacementRanges;
    replacementRanges.reserveInitialCapacity(rangesToReplace.size());

    for (auto& range : rangesToReplace) {
        RefPtr highestRoot = highestEditableRoot(makeDeprecatedLegacyPosition(range.start));
        if (!highestRoot || highestRoot != highestEditableRoot(makeDeprecatedLegacyPosition(range.end)) || !highestRoot->document().frame())
            continue;
        auto scope = makeRangeSelectingNodeContents(*highestRoot);
        replacementRanges.uncheckedAppend({ WTFMove(highestRoot), characterRange(scope, range) });
    }

    replaceRanges(*this, replacementRanges, replacementText);
    return rangesToReplace.size();
}

uint32_t Page::replaceSelectionWithText(const String& replacementText)
{
    Ref frame = CheckedRef(focusController())->focusedOrMainFrame();
    auto selection = frame->selection().selection();
    if (!selection.isContentEditable())
        return 0;

    auto editAction = selection.isRange() ? EditAction::InsertReplacement : EditAction::Insert;
    frame->editor().replaceSelectionWithText(replacementText, Editor::SelectReplacement::Yes, Editor::SmartReplace::No, editAction);
    return 1;
}

void Page::unmarkAllTextMatches()
{
    forEachDocument([] (Document& document) {
        document.markers().removeMarkers(DocumentMarker::TextMatch);
    });
}

#if ENABLE(EDITABLE_REGION)

void Page::setEditableRegionEnabled(bool enabled)
{
    if (m_isEditableRegionEnabled == enabled)
        return;
    m_isEditableRegionEnabled = enabled;
    RefPtr frameView = mainFrame().view();
    if (!frameView)
        return;
    if (auto* renderView = frameView->renderView())
        renderView->compositor().invalidateEventRegionForAllLayers();
}

#endif

#if ENABLE(EDITABLE_REGION)

bool Page::shouldBuildEditableRegion() const
{
    return m_isEditableRegionEnabled || OptionSet<DebugOverlayRegions>::fromRaw(m_settings->visibleDebugOverlayRegions()).contains(DebugOverlayRegions::EditableElementRegion);
}

#endif

Vector<Ref<Element>> Page::editableElementsInRect(const FloatRect& searchRectInRootViewCoordinates) const
{
    RefPtr frameView = mainFrame().view();
    if (!frameView)
        return { };

    RefPtr document = mainFrame().document();
    if (!document)
        return { };

    constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::CollectMultipleElements, HitTestRequest::Type::DisallowUserAgentShadowContent, HitTestRequest::Type::AllowVisibleChildFrameContentOnly };
    LayoutRect searchRectInMainFrameCoordinates = frameView->rootViewToContents(roundedIntRect(searchRectInRootViewCoordinates));
    HitTestResult hitTestResult { searchRectInMainFrameCoordinates };
    if (!document->hitTest(hitType, hitTestResult))
        return { };

    auto rootEditableElement = [](Node& node) -> Element* {
        if (is<HTMLTextFormControlElement>(node)) {
            if (downcast<HTMLTextFormControlElement>(node).isInnerTextElementEditable())
                return &downcast<Element>(node);
        } else if (is<Element>(node) && node.hasEditableStyle())
            return node.rootEditableElement();
        return nullptr;
    };

    ListHashSet<Ref<Element>> rootEditableElements;
    auto& nodeSet = hitTestResult.listBasedTestResult();
    for (auto& node : nodeSet) {
        if (auto* editableElement = rootEditableElement(node)) {
            ASSERT(searchRectInRootViewCoordinates.inclusivelyIntersects(editableElement->boundingBoxInRootViewCoordinates()));
            rootEditableElements.add(*editableElement);
        }
    }

    // Fix up for a now empty focused inline element, e.g. <span contenteditable='true'>Hello</span> became
    // <span contenteditable='true'></span>. Hit testing will likely not find this element because the engine
    // tries to avoid creating line boxes, which are things it hit tests, for them to reduce memory. If the
    // focused element is inside the search rect it's the most likely target for future editing operations,
    // even if it's empty. So, we special case it here.
    if (RefPtr focusedElement = CheckedRef(focusController())->focusedOrMainFrame().document()->focusedElement()) {
        if (searchRectInRootViewCoordinates.inclusivelyIntersects(focusedElement->boundingBoxInRootViewCoordinates())) {
            if (auto* editableElement = rootEditableElement(*focusedElement))
                rootEditableElements.add(*editableElement);
        }
    }
    return WTF::map(rootEditableElements, [](const auto& element) { return element.copyRef(); });
}

#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
bool Page::shouldBuildInteractionRegions() const
{
    return m_settings->interactionRegionsEnabled();
}

void Page::setInteractionRegionsEnabled(bool enable)
{
    bool needsUpdate = enable && !shouldBuildInteractionRegions();
    m_settings->setInteractionRegionsEnabled(enable);
    if (needsUpdate)
        mainFrame().invalidateContentEventRegionsIfNeeded(Frame::InvalidateContentEventRegionsReason::Layout);
}
#endif // ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)

const VisibleSelection& Page::selection() const
{
    return CheckedRef(focusController())->focusedOrMainFrame().selection().selection();
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
    for (AbstractFrame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        localFrame->loader().setDefersLoading(defers);
    }
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

void Page::logMediaDiagnosticMessage(const FormData* formData) const
{
    unsigned imageOrMediaFilesCount = formData ? formData->imageOrMediaFilesCount() : 0;
    if (!imageOrMediaFilesCount)
        return;
    auto message = makeString(imageOrMediaFilesCount, imageOrMediaFilesCount == 1 ? " media file has been submitted" : " media files have been submitted");
    diagnosticLoggingClient().logDiagnosticMessageWithDomain(message, DiagnosticLoggingDomain::Media);
}

void Page::setMediaVolume(float volume)
{
    if (!(volume >= 0 && volume <= 1))
        return;

    if (m_mediaVolume == volume)
        return;

    m_mediaVolume = volume;

#if ENABLE(VIDEO)
    forEachMediaElement([] (HTMLMediaElement& element) {
        element.mediaVolumeDidChange();
    });
#endif
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
    RefPtr<FrameView> view = document->view();

    if (scale == m_pageScaleFactor) {
        if (view && view->scrollPosition() != origin && !delegatesScaling())
            document->updateLayoutIgnorePendingStylesheets();
    } else {
        m_pageScaleFactor = scale;

        if (view && !delegatesScaling()) {
            view->setNeedsLayoutAfterViewConfigurationChange();
            view->setNeedsCompositingGeometryUpdate();
            view->setDescendantsNeedUpdateBackingAndHierarchyTraversal();

            document->resolveStyle(Document::ResolveStyleType::Rebuild);

            // Transform change on RenderView doesn't trigger repaint on non-composited contents.
            mainFrame().view()->invalidateRect(IntRect(LayoutRect::infiniteRect()));
        }

        mainFrame().deviceOrPageScaleFactorChanged();

        if (view && view->fixedElementsLayoutRelativeToFrame())
            view->setViewportConstrainedObjectsNeedLayout();

        if (view && view->scrollPosition() != origin && !delegatesScaling() && document->renderView() && document->renderView()->needsLayout() && view->didFirstLayout())
            view->layoutContext().layout();
    }

    if (view && view->scrollPosition() != origin) {
        if (!view->delegatesScrolling())
            view->setScrollPosition(origin);
#if USE(COORDINATED_GRAPHICS)
        else
            view->requestScrollPositionUpdate(origin);
#endif
    }

#if ENABLE(VIDEO)
    if (inStableState) {
        forEachMediaElement([] (HTMLMediaElement& element) {
            element.pageScaleFactorChanged();
        });
    }
#else
    UNUSED_PARAM(inStableState);
#endif
}

void Page::setDelegatesScaling(bool delegatesScaling)
{
    m_delegatesScaling = delegatesScaling;
}

void Page::setViewScaleFactor(float scale)
{
    if (m_viewScaleFactor == scale)
        return;

    m_viewScaleFactor = scale;
    BackForwardCache::singleton().markPagesForDeviceOrPageScaleChanged(*this);
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
    BackForwardCache::singleton().markPagesForDeviceOrPageScaleChanged(*this);

    pageOverlayController().didChangeDeviceScaleFactor();
}

void Page::screenPropertiesDidChange()
{
#if ENABLE(VIDEO)
    auto mode = preferredDynamicRangeMode(mainFrame().view());
    forEachMediaElement([mode] (auto& element) {
        element.setPreferredDynamicRangeMode(mode);
    });
#endif

    setNeedsRecalcStyleInAllFrames();
}

void Page::windowScreenDidChange(PlatformDisplayID displayID, std::optional<FramesPerSecond> nominalFramesPerSecond)
{
    if (displayID == m_displayID && nominalFramesPerSecond == m_displayNominalFramesPerSecond)
        return;

    m_displayID = displayID;
    m_displayNominalFramesPerSecond = nominalFramesPerSecond;
    if (!m_displayNominalFramesPerSecond) {
        // If the caller didn't give us a refresh rate, maybe the relevant DisplayRefreshMonitor can? This happens in WebKitLegacy
        // because WebView doesn't have a convenient way to access the display refresh rate.
        m_displayNominalFramesPerSecond = DisplayRefreshMonitorManager::sharedManager().nominalFramesPerSecondForDisplay(m_displayID, chrome().client().displayRefreshMonitorFactory());
    }

    forEachDocument([&] (Document& document) {
        document.windowScreenDidChange(displayID);
    });

#if ENABLE(VIDEO)
    auto mode = preferredDynamicRangeMode(mainFrame().view());
    forEachMediaElement([mode] (auto& element) {
        element.setPreferredDynamicRangeMode(mode);
    });
#endif

    if (m_scrollingCoordinator)
        m_scrollingCoordinator->windowScreenDidChange(displayID, m_displayNominalFramesPerSecond);

    renderingUpdateScheduler().windowScreenDidChange(displayID);

    setNeedsRecalcStyleInAllFrames();
}

void Page::setInitialScaleIgnoringContentSize(float scale)
{
    m_initialScaleIgnoringContentSize = scale;
}

void Page::setUserInterfaceLayoutDirection(UserInterfaceLayoutDirection userInterfaceLayoutDirection)
{
    if (m_userInterfaceLayoutDirection == userInterfaceLayoutDirection)
        return;

    m_userInterfaceLayoutDirection = userInterfaceLayoutDirection;
#if ENABLE(VIDEO)
    forEachMediaElement([] (HTMLMediaElement& element) {
        element.userInterfaceLayoutDirectionChanged();
    });
#endif
}

#if ENABLE(VIDEO)

void Page::updateMediaElementRateChangeRestrictions()
{
    // FIXME: This used to call this on all media elements, seemingly by accident. But was there some advantage to that for elements in the back/forward cache?
    forEachMediaElement([] (HTMLMediaElement& element) {
        element.updateRateChangeRestrictions();
    });
}

#endif

void Page::didStartProvisionalLoad()
{
    if (m_performanceMonitor)
        m_performanceMonitor->didStartProvisionalLoad();

    if (m_settings->resourceLoadSchedulingEnabled())
        setLoadSchedulingMode(LoadSchedulingMode::Prioritized);
}

void Page::didCommitLoad()
{
#if ENABLE(EDITABLE_REGION)
    m_isEditableRegionEnabled = false;
#endif

#if HAVE(OS_DARK_MODE_SUPPORT)
    setUseDarkAppearanceOverride(std::nullopt);
#endif

    resetSeenPlugins();
    resetSeenMediaEngines();

#if ENABLE(IMAGE_ANALYSIS)
    resetTextRecognitionResults();
    resetImageAnalysisQueue();
#endif

#if ENABLE(GEOLOCATION)
    if (auto* geolocationController = GeolocationController::from(this))
        geolocationController->didNavigatePage();
#endif
}

void Page::didFinishLoad()
{
    resetRelevantPaintedObjectCounter();

    if (m_performanceMonitor)
        m_performanceMonitor->didFinishLoad();

    setLoadSchedulingMode(LoadSchedulingMode::Direct);
}

bool Page::isOnlyNonUtilityPage() const
{
    return !isUtilityPage() && gNonUtilityPageCount == 1;
}

void Page::setLowPowerModeEnabledOverrideForTesting(std::optional<bool> isEnabled)
{
    // Remove ThrottlingReason::LowPowerMode so handleLowModePowerChange() can do its work.
    m_throttlingReasonsOverridenForTesting.remove(ThrottlingReason::LowPowerMode);

    // Use the current low power mode value of the device.
    if (!isEnabled) {
        handleLowModePowerChange(m_lowPowerModeNotifier->isLowPowerModeEnabled());
        return;
    }

    // Override the value and add ThrottlingReason::LowPowerMode so it override the device state.
    handleLowModePowerChange(isEnabled.value());
    m_throttlingReasonsOverridenForTesting.add(ThrottlingReason::LowPowerMode);
}

void Page::setOutsideViewportThrottlingEnabledForTesting(bool isEnabled)
{
    if (!isEnabled)
        m_throttlingReasonsOverridenForTesting.add(ThrottlingReason::OutsideViewport);
    else
        m_throttlingReasonsOverridenForTesting.remove(ThrottlingReason::OutsideViewport);

    m_throttlingReasons.remove(ThrottlingReason::OutsideViewport);
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
    
    for (AbstractFrame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        FrameView* frameView = localFrame->view();
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
    for (AbstractFrame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        if (FrameView* frameView = localFrame->view())
            frameView->setIsInWindow(isInWindow);
    }

    if (isInWindow)
        resumeAnimatingImages();
}

void Page::addActivityStateChangeObserver(ActivityStateChangeObserver& observer)
{
    m_activityStateChangeObservers.add(observer);
}

void Page::removeActivityStateChangeObserver(ActivityStateChangeObserver& observer)
{
    m_activityStateChangeObservers.remove(observer);
}

void Page::layoutIfNeeded()
{
    if (FrameView* view = m_mainFrame->view())
        view->updateLayoutAndStyleIfNeededRecursive();
}

void Page::scheduleRenderingUpdate(OptionSet<RenderingUpdateStep> requestedSteps)
{
    LOG_WITH_STREAM(EventLoop, stream << "Page " << this << " scheduleTimedRenderingUpdate() - requestedSteps " << requestedSteps << " remaining steps " << m_renderingUpdateRemainingSteps);
    if (m_renderingUpdateRemainingSteps.isEmpty()) {
        scheduleRenderingUpdateInternal();
        return;
    }
    computeUnfulfilledRenderingSteps(requestedSteps);
}

void Page::scheduleRenderingUpdateInternal()
{
    if (chrome().client().scheduleRenderingUpdate())
        return;
    renderingUpdateScheduler().scheduleRenderingUpdate();
}

void Page::didScheduleRenderingUpdate()
{
#if ENABLE(ASYNC_SCROLLING)
    if (auto* scrollingCoordinator = this->scrollingCoordinator())
        scrollingCoordinator->didScheduleRenderingUpdate();
#endif
}

void Page::computeUnfulfilledRenderingSteps(OptionSet<RenderingUpdateStep> requestedSteps)
{
    // m_renderingUpdateRemainingSteps only has more than one entry for the re-entrant rendering update triggered by testing.
    // For scheduling, we only care about the value of the first entry.
    auto remainingSteps = m_renderingUpdateRemainingSteps[0];
    auto stepsForNextUpdate = requestedSteps - remainingSteps;
    m_unfulfilledRequestedSteps.add(stepsForNextUpdate);
}

void Page::triggerRenderingUpdateForTesting()
{
    LOG_WITH_STREAM(EventLoop, stream << "Page " << this << " triggerRenderingUpdateForTesting()");
    renderingUpdateScheduler().triggerRenderingUpdateForTesting();
}

void Page::startTrackingRenderingUpdates()
{
    m_isTrackingRenderingUpdates = true;
    m_renderingUpdateCount = 0;
}

unsigned Page::renderingUpdateCount() const
{
    return m_renderingUpdateCount;
}

// https://html.spec.whatwg.org/multipage/webappapis.html#update-the-rendering
void Page::updateRendering()
{
    LOG(EventLoop, "Page %p updateRendering() - re-entering %d", this, !m_renderingUpdateRemainingSteps.isEmpty());

    if (m_renderingUpdateRemainingSteps.isEmpty())
        m_unfulfilledRequestedSteps = { };

    m_renderingUpdateRemainingSteps.append(allRenderingUpdateSteps);

    // This function is not reentrant, e.g. a rAF callback may trigger a forces repaint in testing.
    // This is why we track m_renderingUpdateRemainingSteps as a stack.
    if (m_renderingUpdateRemainingSteps.size() > 1) {
        layoutIfNeeded();
        m_renderingUpdateRemainingSteps.last().remove(updateRenderingSteps);
        return;
    }

    m_lastRenderingUpdateTimestamp = MonotonicTime::now();

    bool isSVGImagePage = chrome().client().isSVGImageChromeClient();
    if (!isSVGImagePage)
        tracePoint(RenderingUpdateStart);

    layoutIfNeeded();

#if ENABLE(ASYNC_SCROLLING)
    if (auto* scrollingCoordinator = this->scrollingCoordinator())
        scrollingCoordinator->willStartRenderingUpdate();
#endif

    // Timestamps should not change while serving the rendering update steps.
    Vector<WeakPtr<Document, WeakPtrImplWithEventTargetData>> initialDocuments;
    forEachDocument([&initialDocuments] (Document& document) {
        document.domWindow()->freezeNowTimestamp();
        initialDocuments.append(document);
    });

    auto runProcessingStep = [&](RenderingUpdateStep step, const Function<void(Document&)>& perDocumentFunction) {
        m_renderingUpdateRemainingSteps.last().remove(step);
        forEachDocument(perDocumentFunction);
    };

    runProcessingStep(RenderingUpdateStep::FlushAutofocusCandidates, [] (Document& document) {
        if (document.isTopDocument())
            document.flushAutofocusCandidates();
    });

    runProcessingStep(RenderingUpdateStep::Resize, [] (Document& document) {
        document.runResizeSteps();
    });

    runProcessingStep(RenderingUpdateStep::Scroll, [] (Document& document) {
        document.runScrollSteps();
    });

    runProcessingStep(RenderingUpdateStep::MediaQueryEvaluation, [] (Document& document) {
        document.evaluateMediaQueriesAndReportChanges();        
    });

    runProcessingStep(RenderingUpdateStep::Animations, [] (Document& document) {
        document.updateAnimationsAndSendEvents();
    });

    // FIXME: Run the fullscreen steps.
    m_renderingUpdateRemainingSteps.last().remove(RenderingUpdateStep::Fullscreen);

    runProcessingStep(RenderingUpdateStep::VideoFrameCallbacks, [] (Document& document) {
        document.serviceRequestVideoFrameCallbacks();
    });

    runProcessingStep(RenderingUpdateStep::AnimationFrameCallbacks, [] (Document& document) {
        document.serviceRequestAnimationFrameCallbacks();
    });

    runProcessingStep(RenderingUpdateStep::CaretAnimation, [] (Document& document) {
        document.serviceCaretAnimation();
    });

    layoutIfNeeded();

    runProcessingStep(RenderingUpdateStep::ResizeObservations, [&] (Document& document) {
        document.updateResizeObservations(*this);
    });

    runProcessingStep(RenderingUpdateStep::IntersectionObservations, [] (Document& document) {
        document.updateIntersectionObservations();
    });

    runProcessingStep(RenderingUpdateStep::Images, [] (Document& document) {
        for (auto& image : document.cachedResourceLoader().allCachedSVGImages()) {
            if (auto* page = image->internalPage())
                page->isolatedUpdateRendering();
        }
    });

    for (auto& document : initialDocuments) {
        if (document && document->domWindow())
            document->domWindow()->unfreezeNowTimestamp();
    }

    m_renderingUpdateRemainingSteps.last().remove(RenderingUpdateStep::WheelEventMonitorCallbacks);

    if (UNLIKELY(isMonitoringWheelEvents()))
        wheelEventTestMonitor()->checkShouldFireCallbacks();

    if (m_isTrackingRenderingUpdates)
        ++m_renderingUpdateCount;

    layoutIfNeeded();
    doAfterUpdateRendering();

    if (!isSVGImagePage)
        tracePoint(RenderingUpdateEnd);
}

void Page::isolatedUpdateRendering()
{
    LOG(EventLoop, "Page %p isolatedUpdateRendering()", this);
    updateRendering();
    renderingUpdateCompleted();
}

void Page::doAfterUpdateRendering()
{
    // Code here should do once-per-frame work that needs to be done before painting, and requires
    // layout to be up-to-date. It should not run script, trigger layout, or dirty layout.

    if (DeprecatedGlobalSettings::layoutFormattingContextEnabled()) {
        forEachDocument([] (Document& document) {
            if (auto* frameView = document.view())
                frameView->displayView().prepareForDisplay();
        });
    }

    auto runProcessingStep = [&](RenderingUpdateStep step, const Function<void(Document&)>& perDocumentFunction) {
        m_renderingUpdateRemainingSteps.last().remove(step);
        forEachDocument(perDocumentFunction);
    };

    runProcessingStep(RenderingUpdateStep::CursorUpdate, [] (Document& document) {
        if (auto* frame = document.frame())
            frame->eventHandler().updateCursorIfNeeded();
    });

    forEachDocument([] (Document& document) {
        document.enqueuePaintTimingEntryIfNeeded();
    });

    forEachDocument([] (Document& document) {
        document.selection().updateAppearanceAfterLayout();
    });

    forEachDocument([] (Document& document) {
        document.updateHighlightPositions();
    });
#if ENABLE(APP_HIGHLIGHTS)
    forEachDocument([] (Document& document) {
        auto appHighlightStorage = document.appHighlightStorageIfExists();
        if (!appHighlightStorage)
            return;
        
        if (appHighlightStorage->hasUnrestoredHighlights() && MonotonicTime::now() - appHighlightStorage->lastRangeSearchTime() > 1_s) {
            appHighlightStorage->resetLastRangeSearchTime();
            document.eventLoop().queueTask(TaskSource::InternalAsyncTask, [weakDocument = WeakPtr<Document, WeakPtrImplWithEventTargetData> { document }] {
                RefPtr document { weakDocument.get() };
                if (!document)
                    return;

                if (auto* appHighlightStorage = document->appHighlightStorageIfExists())
                    appHighlightStorage->restoreUnrestoredAppHighlights();
            });
        }
    });
#endif

#if ENABLE(VIDEO)
    forEachDocument([] (Document& document) {
        document.updateTextTrackRepresentationImageIfNeeded();
    });
#endif

#if ENABLE(IMAGE_ANALYSIS)
    updateElementsWithTextRecognitionResults();
#endif

    prioritizeVisibleResources();

    m_renderingUpdateRemainingSteps.last().remove(RenderingUpdateStep::EventRegionUpdate);

#if ENABLE(IOS_TOUCH_EVENTS)
    // updateTouchEventRegions() needs to be called only on the top document.
    if (RefPtr<Document> document = mainFrame().document())
        document->updateTouchEventRegions();
#endif
    forEachDocument([] (Document& document) {
        document.updateEventRegions();
    });

    DebugPageOverlays::doAfterUpdateRendering(*this);

    m_renderingUpdateRemainingSteps.last().remove(RenderingUpdateStep::PrepareCanvasesForDisplay);

    forEachDocument([] (Document& document) {
        document.prepareCanvasesForDisplayIfNeeded();
    });

    ASSERT(!mainFrame().view() || !mainFrame().view()->needsLayout());
#if ASSERT_ENABLED
    for (AbstractFrame* child = mainFrame().tree().firstRenderedChild(); child; child = child->tree().traverseNextRendered()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(child);
        auto* frameView = localFrame->view();
        ASSERT(!frameView || !frameView->needsLayout());
    }
#endif

    if (auto* view = mainFrame().view())
        view->notifyAllFramesThatContentAreaWillPaint();

    if (!m_sampledPageTopColor) {
        m_sampledPageTopColor = PageColorSampler::sampleTop(*this);
        if (m_sampledPageTopColor)
            chrome().client().sampledPageTopColorChanged();
    }
}

void Page::finalizeRenderingUpdate(OptionSet<FinalizeRenderingUpdateFlags> flags)
{
    LOG(EventLoop, "Page %p finalizeRenderingUpdate()", this);

    auto* view = mainFrame().view();
    if (!view)
        return;

    if (flags.contains(FinalizeRenderingUpdateFlags::InvalidateImagesWithAsyncDecodes))
        view->invalidateImagesWithAsyncDecodes();

    m_renderingUpdateRemainingSteps.last().remove(RenderingUpdateStep::LayerFlush);

    view->flushCompositingStateIncludingSubframes();

#if ENABLE(ASYNC_SCROLLING)
    m_renderingUpdateRemainingSteps.last().remove(RenderingUpdateStep::ScrollingTreeUpdate);

    if (auto* scrollingCoordinator = this->scrollingCoordinator()) {
        scrollingCoordinator->commitTreeStateIfNeeded();
        if (flags.contains(FinalizeRenderingUpdateFlags::ApplyScrollingTreeLayerPositions))
            scrollingCoordinator->applyScrollingTreeLayerPositions();

        scrollingCoordinator->didCompleteRenderingUpdate();
    }
#endif

    ASSERT(m_renderingUpdateRemainingSteps.last().isEmpty());
    renderingUpdateCompleted();
}

void Page::renderingUpdateCompleted()
{
    m_renderingUpdateRemainingSteps.removeLast();

    LOG_WITH_STREAM(EventLoop, stream << "Page " << this << " renderingUpdateCompleted() - steps " << m_renderingUpdateRemainingSteps << " unfulfilled steps " << m_unfulfilledRequestedSteps);

    if (m_unfulfilledRequestedSteps) {
        scheduleRenderingUpdateInternal();
        m_unfulfilledRequestedSteps = { };
    }
}

void Page::willStartPlatformRenderingUpdate()
{
    // Inspector's use of "composite" is rather innacurate. On Apple platforms, the "composite" step happens
    // in another process; these hooks wrap the non-WebKit CA commit time which is mostly painting-related.
    m_inspectorController->willComposite(mainFrame());

    if (m_scrollingCoordinator)
        m_scrollingCoordinator->willStartPlatformRenderingUpdate();
}

void Page::didCompletePlatformRenderingUpdate()
{
    if (m_scrollingCoordinator)
        m_scrollingCoordinator->didCompletePlatformRenderingUpdate();

    m_inspectorController->didComposite(mainFrame());
}

void Page::prioritizeVisibleResources()
{
    if (loadSchedulingMode() == LoadSchedulingMode::Direct)
        return;
    if (!mainFrame().document())
        return;

    Vector<CachedResource*> toPrioritize;

    forEachDocument([&] (Document& document) {
        toPrioritize.appendVector(document.cachedResourceLoader().visibleResourcesToPrioritize());
    });
    
    auto computeSchedulingMode = [&] {
        auto& document = *mainFrame().document();
        // Parsing generates resource loads.
        if (document.parsing())
            return LoadSchedulingMode::Prioritized;
        
        // Async script execution may generate more resource loads that benefit from prioritization.
        if (document.scriptRunner().hasPendingScripts())
            return LoadSchedulingMode::Prioritized;
        
        // We still haven't finished loading the visible resources.
        if (!toPrioritize.isEmpty())
            return LoadSchedulingMode::Prioritized;
        
        return LoadSchedulingMode::Direct;
    };
    
    setLoadSchedulingMode(computeSchedulingMode());

    if (toPrioritize.isEmpty())
        return;

    auto resourceLoaders = toPrioritize.map([](auto* resource) {
        return resource->loader();
    });

    platformStrategies()->loaderStrategy()->prioritizeResourceLoads(resourceLoaders);
}

void Page::setLoadSchedulingMode(LoadSchedulingMode mode)
{
    if (m_loadSchedulingMode == mode)
        return;

    m_loadSchedulingMode = mode;

    platformStrategies()->loaderStrategy()->setResourceLoadSchedulingMode(*this, m_loadSchedulingMode);
}

#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
void Page::setImageAnimationEnabled(bool enabled)
{
    if (m_imageAnimationEnabled == enabled || !settings().imageAnimationControlEnabled())
        return;
    m_imageAnimationEnabled = enabled;
    updatePlayStateForAllAnimations();
}
#endif

void Page::suspendScriptedAnimations()
{
    m_scriptedAnimationsSuspended = true;

    forEachDocument([] (Document& document) {
        document.suspendScriptedAnimationControllerCallbacks();
    });
}

void Page::resumeScriptedAnimations()
{
    m_scriptedAnimationsSuspended = false;

    forEachDocument([] (Document& document) {
        document.resumeScriptedAnimationControllerCallbacks();
    });
}

void Page::timelineControllerMaximumAnimationFrameRateDidChange(DocumentTimelinesController&)
{
    renderingUpdateScheduler().adjustRenderingUpdateFrequency();
}

std::optional<FramesPerSecond> Page::preferredRenderingUpdateFramesPerSecond(OptionSet<PreferredRenderingUpdateOption> flags) const
{
    // Unless the call site specifies an explicit set of options, this method will account for both
    // throttling reasons and the frame rate of animations to determine its return value. The only
    // place where we specify an explicit set of options is DocumentTimelinesController::updateAnimationsAndSendEvents()
    // where we need to identify what the update frame rate would be _not_ accounting for animations.

    auto throttlingReasons = m_throttlingReasons;
    if (!flags.contains(PreferredRenderingUpdateOption::IncludeThrottlingReasons))
        throttlingReasons = { };

    auto frameRate = preferredFramesPerSecond(throttlingReasons, m_displayNominalFramesPerSecond, settings().preferPageRenderingUpdatesNear60FPSEnabled());
    if (!flags.contains(PreferredRenderingUpdateOption::IncludeAnimationsFrameRate))
        return frameRate;

    // If we're throttled, we do not account for the frame rate set on animations and simply use the throttled frame rate.
    auto unthrottledDefaultFrameRate = preferredRenderingUpdateFramesPerSecond({ });
    auto isThrottled = frameRate && unthrottledDefaultFrameRate && *frameRate < *unthrottledDefaultFrameRate;
    if (isThrottled)
        return frameRate;

    forEachDocument([&] (Document& document) {
        if (auto* timelinesController = document.timelinesController()) {
            if (auto timelinePreferredFrameRate = timelinesController->maximumAnimationFrameRate()) {
                if (!frameRate || *frameRate < *timelinePreferredFrameRate)
                    frameRate = *timelinePreferredFrameRate;
            }
        }
    });

    return frameRate;
}

Seconds Page::preferredRenderingUpdateInterval() const
{
    return preferredFrameInterval(m_throttlingReasons, m_displayNominalFramesPerSecond, settings().preferPageRenderingUpdatesNear60FPSEnabled());
}

void Page::setIsVisuallyIdleInternal(bool isVisuallyIdle)
{
    if (isVisuallyIdle == m_throttlingReasons.contains(ThrottlingReason::VisuallyIdle))
        return;

    m_throttlingReasons.set(ThrottlingReason::VisuallyIdle, isVisuallyIdle);
    renderingUpdateScheduler().adjustRenderingUpdateFrequency();
}

void Page::handleLowModePowerChange(bool isLowPowerModeEnabled)
{
    if (!canUpdateThrottlingReason(ThrottlingReason::LowPowerMode))
        return;

    if (isLowPowerModeEnabled == m_throttlingReasons.contains(ThrottlingReason::LowPowerMode))
        return;

    m_throttlingReasons.set(ThrottlingReason::LowPowerMode, isLowPowerModeEnabled);
    renderingUpdateScheduler().adjustRenderingUpdateFrequency();

    updateDOMTimerAlignmentInterval();
}

void Page::userStyleSheetLocationChanged()
{
    // FIXME: Eventually we will move to a model of just being handed the sheet
    // text instead of loading the URL ourselves.
    URL url = m_settings->userStyleSheetLocation();
    
    // Allow any local file URL scheme to be loaded.
    if (LegacySchemeRegistry::shouldTreatURLSchemeAsLocal(url.protocol()))
        m_userStyleSheetPath = url.fileSystemPath();
    else
        m_userStyleSheetPath = String();

    m_didLoadUserStyleSheet = false;
    m_userStyleSheet = String();
    m_userStyleSheetModificationTime = std::nullopt;

    // Data URLs with base64-encoded UTF-8 style sheets are common. We can process them
    // synchronously and avoid using a loader. 
    if (url.protocolIsData() && url.string().startsWith("data:text/css;charset=utf-8;base64,"_s)) {
        m_didLoadUserStyleSheet = true;

        if (auto styleSheetAsUTF8 = base64Decode(PAL::decodeURLEscapeSequences(StringView(url.string()).substring(35)), Base64DecodeOptions::IgnoreSpacesAndNewLines))
            m_userStyleSheet = String::fromUTF8(styleSheetAsUTF8->data(), styleSheetAsUTF8->size());
    }

    forEachDocument([] (Document& document) {
        document.extensionStyleSheets().updatePageUserSheet();
    });
}

const String& Page::userStyleSheet() const
{
    if (m_userStyleSheetPath.isEmpty())
        return m_userStyleSheet;

    auto modificationTime = FileSystem::fileModificationTime(m_userStyleSheetPath);
    if (!modificationTime) {
        // The stylesheet either doesn't exist, was just deleted, or is
        // otherwise unreadable. If we've read the stylesheet before, we should
        // throw away that data now as it no longer represents what's on disk.
        m_userStyleSheet = String();
        return m_userStyleSheet;
    }

    // If the stylesheet hasn't changed since the last time we read it, we can
    // just return the old data.
    if (m_didLoadUserStyleSheet && (m_userStyleSheetModificationTime && modificationTime.value() <= m_userStyleSheetModificationTime.value()))
        return m_userStyleSheet;

    m_didLoadUserStyleSheet = true;
    m_userStyleSheet = String();
    m_userStyleSheetModificationTime = modificationTime;

    // FIXME: It would be better to load this asynchronously to avoid blocking
    // the process, but we will first need to create an asynchronous loading
    // mechanism that is not tied to a particular Frame. We will also have to
    // determine what our behavior should be before the stylesheet is loaded
    // and what should happen when it finishes loading, especially with respect
    // to when the load event fires, when Document::close is called, and when
    // layout/paint are allowed to happen.
    auto data = SharedBuffer::createWithContentsOfFile(m_userStyleSheetPath);
    if (!data)
        return m_userStyleSheet;

    m_userStyleSheet = TextResourceDecoder::create(cssContentTypeAtom())->decodeAndFlush(data->data(), data->size());

    return m_userStyleSheet;
}

void Page::userAgentChanged()
{
    forEachDocument([] (Document& document) {
        if (auto* window = document.domWindow()) {
            if (auto* navigator = window->optionalNavigator())
                navigator->userAgentChanged();
        }
    });
}

void Page::invalidateStylesForAllLinks()
{
    forEachDocument([] (Document& document) {
        document.visitedLinkState().invalidateStyleForAllLinks();
    });
}

void Page::invalidateStylesForLink(SharedStringHash linkHash)
{
    forEachDocument([&] (Document& document) {
        document.visitedLinkState().invalidateStyleForLink(linkHash);
    });
}

void Page::invalidateInjectedStyleSheetCacheInAllFrames()
{
    forEachDocument([] (Document& document) {
        document.extensionStyleSheets().invalidateInjectedStyleSheetCache();
    });
}

void Page::setDebugger(JSC::Debugger* debugger)
{
    if (m_debugger == debugger)
        return;

    m_debugger = debugger;

    for (AbstractFrame* frame = &m_mainFrame.get(); frame; frame = frame->tree().traverseNext()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        localFrame->windowProxy().attachDebugger(m_debugger);
    }
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

void Page::setCORSDisablingPatterns(Vector<UserContentURLPattern>&& patterns)
{
    m_corsDisablingPatterns = WTFMove(patterns);
}

void Page::setMemoryCacheClientCallsEnabled(bool enabled)
{
    if (m_areMemoryCacheClientCallsEnabled == enabled)
        return;

    m_areMemoryCacheClientCallsEnabled = enabled;
    if (!enabled)
        return;

    for (RefPtr<AbstractFrame> frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        RefPtr localFrame = dynamicDowncast<LocalFrame>(frame.get());
        if (!localFrame)
            continue;
        localFrame->loader().tellClientAboutPastMemoryCacheLoads();
    }
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
        forEachDocument([] (Document& document) {
            document.didChangeTimerAlignmentInterval();
        });
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
    forEachDocument([] (Document& document) {
        document.initDNSPrefetch();
    });
}

void Page::storageBlockingStateChanged()
{
    forEachDocument([] (Document& document) {
        document.storageBlockingStateDidChange();
    });
}

void Page::updateIsPlayingMedia()
{
    MediaProducerMediaStateFlags state;
    forEachDocument([&](auto& document) {
        state.add(document.mediaState());
    });

    if (state == m_mediaState)
        return;

    m_mediaState = state;

    chrome().client().isPlayingMediaDidChange(state);
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
    if (auto bestMediaElement = HTMLMediaElement::bestMediaElementForRemoteControls(MediaElementSession::PlaybackControlsPurpose::ControlsManager))
        chrome().client().setUpPlaybackControlsManager(*bestMediaElement);
    else
        chrome().client().clearPlaybackControlsManager();
}

void Page::playbackControlsMediaEngineChanged()
{
    chrome().client().playbackControlsMediaEngineChanged();
}

#endif

void Page::setMuted(MediaProducerMutedStateFlags muted)
{
    m_mutedState = muted;

    forEachDocument([] (Document& document) {
        document.pageMutedStateDidChange();
    });
}

void Page::stopMediaCapture(MediaProducerMediaCaptureKind kind)
{
    UNUSED_PARAM(kind);
#if ENABLE(MEDIA_STREAM)
    forEachDocument([kind] (Document& document) {
        document.stopMediaCapture(kind);
    });
#endif
}

bool Page::mediaPlaybackExists()
{
#if ENABLE(VIDEO)
    if (auto* platformMediaSessionManager = PlatformMediaSessionManager::sharedManagerIfExists())
        return !platformMediaSessionManager->hasNoSession();
#endif
    return false;
}

bool Page::mediaPlaybackIsPaused()
{
#if ENABLE(VIDEO)
    if (auto* platformMediaSessionManager = PlatformMediaSessionManager::sharedManagerIfExists())
        return platformMediaSessionManager->mediaPlaybackIsPaused(mediaSessionGroupIdentifier());
#endif
    return false;
}

void Page::pauseAllMediaPlayback()
{
#if ENABLE(VIDEO)
    if (auto* platformMediaSessionManager = PlatformMediaSessionManager::sharedManagerIfExists())
        platformMediaSessionManager->pauseAllMediaPlaybackForGroup(mediaSessionGroupIdentifier());
#endif
}

void Page::suspendAllMediaPlayback()
{
#if ENABLE(VIDEO)
    ASSERT(!m_mediaPlaybackIsSuspended);
    if (m_mediaPlaybackIsSuspended)
        return;

    if (auto* platformMediaSessionManager = PlatformMediaSessionManager::sharedManagerIfExists())
        platformMediaSessionManager->suspendAllMediaPlaybackForGroup(mediaSessionGroupIdentifier());

    // FIXME: We cannot set m_mediaPlaybackIsSuspended before, see https://bugs.webkit.org/show_bug.cgi?id=192829#c7.
    m_mediaPlaybackIsSuspended = true;
#endif
}

MediaSessionGroupIdentifier Page::mediaSessionGroupIdentifier() const
{
    if (!m_mediaSessionGroupIdentifier) {
        if (auto identifier = m_mainFrame->loader().pageID())
            m_mediaSessionGroupIdentifier = makeObjectIdentifier<MediaSessionGroupIdentifierType>(identifier->toUInt64());
    }
    return m_mediaSessionGroupIdentifier;
}

void Page::resumeAllMediaPlayback()
{
#if ENABLE(VIDEO)
    ASSERT(m_mediaPlaybackIsSuspended);
    if (!m_mediaPlaybackIsSuspended)
        return;
    m_mediaPlaybackIsSuspended = false;

    if (auto* platformMediaSessionManager = PlatformMediaSessionManager::sharedManagerIfExists())
        platformMediaSessionManager->resumeAllMediaPlaybackForGroup(mediaSessionGroupIdentifier());
#endif
}

void Page::suspendAllMediaBuffering()
{
#if ENABLE(VIDEO)
    ASSERT(!m_mediaBufferingIsSuspended);
    if (m_mediaBufferingIsSuspended)
        return;
    m_mediaBufferingIsSuspended = true;

    if (auto* platformMediaSessionManager = PlatformMediaSessionManager::sharedManagerIfExists())
        platformMediaSessionManager->suspendAllMediaBufferingForGroup(mediaSessionGroupIdentifier());
#endif
}

void Page::resumeAllMediaBuffering()
{
#if ENABLE(VIDEO)
    if (!m_mediaBufferingIsSuspended)
        return;
    m_mediaBufferingIsSuspended = false;

    if (auto* platformMediaSessionManager = PlatformMediaSessionManager::sharedManagerIfExists())
        platformMediaSessionManager->resumeAllMediaBufferingForGroup(mediaSessionGroupIdentifier());
#endif
}

unsigned Page::subframeCount() const
{
    return mainFrame().tree().descendantCount();
}

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

    CheckedRef(*m_focusController)->setActivityState(activityState);

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

    for (auto& observer : m_activityStateChangeObservers)
        observer.activityStateDidChange(oldActivityState, m_activityState);

    if (wasVisibleAndActive != isVisibleAndActive()) {
        PlatformMediaSessionManager::updateNowPlayingInfoIfNecessary();
        stopKeyboardScrollAnimation();
    }

    if (m_performanceMonitor)
        m_performanceMonitor->activityStateChanged(oldActivityState, activityState);
}

void Page::stopKeyboardScrollAnimation()
{
    for (AbstractFrame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        auto* frameView = localFrame->view();
        if (!frameView)
            continue;

        frameView->stopKeyboardScrollAnimation();

        auto scrollableAreas = frameView->scrollableAreas();
        if (!scrollableAreas)
            continue;

        for (auto& scrollableArea : *scrollableAreas) {
            // First call stopAsyncAnimatedScroll() to prepare for the keyboard scroller running on the scrolling thread.
            scrollableArea->stopAsyncAnimatedScroll();
            scrollableArea->stopKeyboardScrollAnimation();
        }
    }
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

void Page::setIsVisibleInternal(bool isVisible)
{
    // FIXME: The visibility state should be stored on the top-level document.
    // https://bugs.webkit.org/show_bug.cgi?id=116769

    if (isVisible) {
        m_isPrerender = false;

        resumeScriptedAnimations();

#if PLATFORM(IOS_FAMILY)
        forEachDocument([] (Document& document) {
            document.resumeDeviceMotionAndOrientationUpdates();
        });
#endif

        if (FrameView* view = mainFrame().view())
            view->show();

        if (m_settings->hiddenPageCSSAnimationSuspensionEnabled()) {
            forEachDocument([] (Document& document) {
                if (auto* timelines = document.timelinesController())
                    timelines->resumeAnimations();
            });
        }

        forEachDocument([] (Document& document) {
            if (document.svgExtensions())
                document.accessSVGExtensions().unpauseAnimations();
        });

        resumeAnimatingImages();

        if (m_navigationToLogWhenVisible) {
            logNavigation(m_navigationToLogWhenVisible.value());
            m_navigationToLogWhenVisible = std::nullopt;
        }
    }

    if (!isVisible) {
        if (m_settings->hiddenPageCSSAnimationSuspensionEnabled()) {
            forEachDocument([] (Document& document) {
                if (auto* timelines = document.timelinesController())
                    timelines->suspendAnimations();
            });
        }

        forEachDocument([] (Document& document) {
            if (document.svgExtensions())
                document.accessSVGExtensions().pauseAnimations();
        });

#if PLATFORM(IOS_FAMILY)
        forEachDocument([] (Document& document) {
            document.suspendDeviceMotionAndOrientationUpdates();
        });
#endif

        suspendScriptedAnimations();

        if (FrameView* view = mainFrame().view())
            view->hide();
    }

    forEachDocument([] (Document& document) {
        document.visibilityStateChanged();
    });
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
    return VisibilityState::Hidden;
}

void Page::setHeaderHeight(int headerHeight)
{
    if (headerHeight == m_headerHeight)
        return;

    m_headerHeight = headerHeight;

    FrameView* frameView = mainFrame().view();
    if (!frameView)
        return;

    RenderView* renderView = frameView->renderView();
    if (!renderView)
        return;

    frameView->setNeedsLayoutAfterViewConfigurationChange();
    frameView->setNeedsCompositingGeometryUpdate();
}

void Page::setFooterHeight(int footerHeight)
{
    if (footerHeight == m_footerHeight)
        return;

    m_footerHeight = footerHeight;

    FrameView* frameView = mainFrame().view();
    if (!frameView)
        return;

    RenderView* renderView = frameView->renderView();
    if (!renderView)
        return;

    frameView->setNeedsLayoutAfterViewConfigurationChange();
    frameView->setNeedsCompositingGeometryUpdate();
}

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

void Page::whenUnnested(Function<void()>&& callback)
{
    ASSERT(!m_unnestCallback);

    m_unnestCallback = WTFMove(callback);
}

void Page::setCurrentKeyboardScrollingAnimator(KeyboardScrollingAnimator* animator)
{
    m_currentKeyboardScrollingAnimator = animator;
}

#if ENABLE(REMOTE_INSPECTOR)

bool Page::inspectable() const
{
    return m_inspectorDebuggable->inspectable();
}

void Page::setInspectable(bool inspectable)
{
    m_inspectorDebuggable->setInspectable(inspectable);
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

Color Page::themeColor() const
{
    auto* document = mainFrame().document();
    if (!document)
        return { };

    return document->themeColor();
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

Color Page::sampledPageTopColor() const
{
    return valueOrDefault(m_sampledPageTopColor);
}

void Page::setUnderPageBackgroundColorOverride(Color&& underPageBackgroundColorOverride)
{
    if (underPageBackgroundColorOverride == m_underPageBackgroundColorOverride)
        return;

    m_underPageBackgroundColorOverride = WTFMove(underPageBackgroundColorOverride);

    scheduleRenderingUpdate({ });

#if HAVE(RUBBER_BANDING)
    if (RefPtr frameView = mainFrame().view()) {
        if (auto* renderView = frameView->renderView()) {
            if (renderView->usesCompositing())
                renderView->compositor().updateLayerForOverhangAreasBackgroundColor();
        }
    }
#endif // HAVE(RUBBER_BANDING)
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
    LayoutRect relevantViewRect { 0, 0, LayoutUnit(relevantViewRectWidth), 1300 };
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
    topRelevantRect.contract(LayoutSize(0_lu, relevantRect.height() / 2));
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

void Page::suspendActiveDOMObjectsAndAnimations()
{
    for (AbstractFrame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        localFrame->suspendActiveDOMObjectsAndAnimations();
    }
}

void Page::resumeActiveDOMObjectsAndAnimations()
{
    for (AbstractFrame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        localFrame->resumeActiveDOMObjectsAndAnimations();
    }

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
        forEachDocument([&] (Document& document) {
            if (auto* timelines = document.timelinesController()) {
                if (m_settings->hiddenPageCSSAnimationSuspensionEnabled())
                    timelines->suspendAnimations();
                else
                    timelines->resumeAnimations();
            }
        });
    }
}

#if ENABLE(VIDEO)

void Page::captionPreferencesChanged()
{
    forEachDocument([] (Document& document) {
        document.captionPreferencesChanged();
    });
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

void Page::forbidSynchronousLoads()
{
    ++m_forbidSynchronousLoadsDepth;
}

void Page::allowSynchronousLoads()
{
    ASSERT(m_forbidSynchronousLoadsDepth);
    --m_forbidSynchronousLoadsDepth;
}

bool Page::areSynchronousLoadsAllowed()
{
    return !m_forbidSynchronousLoadsDepth;
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
        diagnosticLoggingClient().logDiagnosticMessageWithEnhancedPrivacy(DiagnosticLoggingKeys::domainVisitedKey(), navigation.domain.string(), ShouldSample::Yes);
}

void Page::mainFrameLoadStarted(const URL& destinationURL, FrameLoadType type)
{
    Navigation navigation = { RegistrableDomain { destinationURL }, type };

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

void Page::notifyToInjectUserScripts()
{
    m_hasBeenNotifiedToInjectUserScripts = true;

    forEachFrame([] (Frame& frame) {
        frame.injectUserScriptsAwaitingNotification();
    });
}

void Page::setUserContentProvider(Ref<UserContentProvider>&& userContentProvider)
{
    m_userContentProvider->removePage(*this);
    m_userContentProvider = WTFMove(userContentProvider);
    m_userContentProvider->addPage(*this);

    invalidateInjectedStyleSheetCacheInAllFrames();
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

// This is only called by WebKitLegacy.
void Page::setSessionID(PAL::SessionID sessionID)
{
    ASSERT(sessionID.isValid());
    ASSERT(m_sessionID == PAL::SessionID::legacyPrivateSessionID() || m_sessionID == PAL::SessionID::defaultSessionID());
    ASSERT(sessionID == PAL::SessionID::legacyPrivateSessionID() || sessionID == PAL::SessionID::defaultSessionID());

    if (sessionID != m_sessionID)
        m_idbConnectionToServer = nullptr;

    if (sessionID != m_sessionID) {
        constexpr auto doNotCreate = StorageNamespaceProvider::ShouldCreateNamespace::No;
        if (auto sessionStorage = m_storageNamespaceProvider->sessionStorageNamespace(m_mainFrame->document()->topOrigin(), *this, doNotCreate))
            sessionStorage->setSessionIDForTesting(sessionID);
    }

    bool privateBrowsingStateChanged = (sessionID.isEphemeral() != m_sessionID.isEphemeral());

    m_sessionID = sessionID;

    if (!privateBrowsingStateChanged)
        return;

    forEachDocument([&] (Document& document) {
        document.privateBrowsingStateDidChange(m_sessionID);
    });
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
void Page::addPlaybackTargetPickerClient(PlaybackTargetClientContextIdentifier contextId)
{
    chrome().client().addPlaybackTargetPickerClient(contextId);
}

void Page::removePlaybackTargetPickerClient(PlaybackTargetClientContextIdentifier contextId)
{
    chrome().client().removePlaybackTargetPickerClient(contextId);
}

void Page::showPlaybackTargetPicker(PlaybackTargetClientContextIdentifier contextId, const WebCore::IntPoint& location, bool isVideo, RouteSharingPolicy routeSharingPolicy, const String& routingContextUID)
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

void Page::playbackTargetPickerClientStateDidChange(PlaybackTargetClientContextIdentifier contextId, MediaProducerMediaStateFlags state)
{
    chrome().client().playbackTargetPickerClientStateDidChange(contextId, state);
}

void Page::setMockMediaPlaybackTargetPickerEnabled(bool enabled)
{
    chrome().client().setMockMediaPlaybackTargetPickerEnabled(enabled);
}

void Page::setMockMediaPlaybackTargetPickerState(const String& name, MediaPlaybackTargetContext::MockState state)
{
    chrome().client().setMockMediaPlaybackTargetPickerState(name, state);
}

void Page::mockMediaPlaybackTargetPickerDismissPopup()
{
    chrome().client().mockMediaPlaybackTargetPickerDismissPopup();
}

void Page::setPlaybackTarget(PlaybackTargetClientContextIdentifier contextId, Ref<MediaPlaybackTarget>&& target)
{
    forEachDocument([&] (Document& document) {
        document.setPlaybackTarget(contextId, target.copyRef());
    });
}

void Page::playbackTargetAvailabilityDidChange(PlaybackTargetClientContextIdentifier contextId, bool available)
{
    forEachDocument([&] (Document& document) {
        document.playbackTargetAvailabilityDidChange(contextId, available);
    });
}

void Page::setShouldPlayToPlaybackTarget(PlaybackTargetClientContextIdentifier contextId, bool shouldPlay)
{
    forEachDocument([&] (Document& document) {
        document.setShouldPlayToPlaybackTarget(contextId, shouldPlay);
    });
}

void Page::playbackTargetPickerWasDismissed(PlaybackTargetClientContextIdentifier contextId)
{
    forEachDocument([&] (Document& document) {
        document.playbackTargetPickerWasDismissed(contextId);
    });
}

#endif

RefPtr<WheelEventTestMonitor> Page::wheelEventTestMonitor() const
{
    return m_wheelEventTestMonitor;
}

void Page::clearWheelEventTestMonitor()
{
    if (m_scrollingCoordinator)
        m_scrollingCoordinator->stopMonitoringWheelEvents();

    m_wheelEventTestMonitor = nullptr;
}

bool Page::isMonitoringWheelEvents() const
{
    return !!m_wheelEventTestMonitor;
}

void Page::startMonitoringWheelEvents(bool clearLatchingState)
{
    ensureWheelEventTestMonitor().clearAllTestDeferrals();

#if ENABLE(WHEEL_EVENT_LATCHING)
    if (clearLatchingState)
        scrollLatchingController().clear();
#endif

    if (auto* frameView = mainFrame().view()) {
        if (m_scrollingCoordinator) {
            m_scrollingCoordinator->startMonitoringWheelEvents(clearLatchingState);
            m_scrollingCoordinator->updateIsMonitoringWheelEventsForFrameView(*frameView);
        }
    }
}

WheelEventTestMonitor& Page::ensureWheelEventTestMonitor()
{
    if (!m_wheelEventTestMonitor)
        m_wheelEventTestMonitor = adoptRef(new WheelEventTestMonitor(*this));

    return *m_wheelEventTestMonitor;
}

#if ENABLE(VIDEO)

void Page::setAllowsMediaDocumentInlinePlayback(bool flag)
{
    if (m_allowsMediaDocumentInlinePlayback == flag)
        return;
    m_allowsMediaDocumentInlinePlayback = flag;

    forEachMediaElement([] (HTMLMediaElement& element) {
        element.allowsMediaDocumentInlinePlaybackChanged();
    });
}

#endif

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

#if ENABLE(RESOURCE_USAGE)
void Page::setResourceUsageOverlayVisible(bool visible)
{
    if (!visible) {
        m_resourceUsageOverlay = nullptr;
        return;
    }

    if (!m_resourceUsageOverlay && m_settings->acceleratedCompositingEnabled())
        m_resourceUsageOverlay = makeUnique<ResourceUsageOverlay>(*this);
}
#endif

String Page::captionUserPreferencesStyleSheet()
{
    return m_captionUserPreferencesStyleSheet;
}

void Page::setCaptionUserPreferencesStyleSheet(const String& styleSheet)
{
    if (m_captionUserPreferencesStyleSheet == styleSheet)
        return;

    m_captionUserPreferencesStyleSheet = styleSheet;
}

void Page::accessibilitySettingsDidChange()
{
    forEachDocument([] (auto& document) {
        document.styleScope().evaluateMediaQueriesForAccessibilitySettingsChange();
        document.updateElementsAffectedByMediaQueries();
        document.scheduleRenderingUpdate(RenderingUpdateStep::MediaQueryEvaluation);
    });

    InspectorInstrumentation::accessibilitySettingsDidChange(*this);
}

void Page::appearanceDidChange()
{
    forEachDocument([] (auto& document) {
        document.styleScope().didChangeStyleSheetEnvironment();
        document.styleScope().evaluateMediaQueriesForAppearanceChange();
        document.updateElementsAffectedByMediaQueries();
        document.scheduleRenderingUpdate(RenderingUpdateStep::MediaQueryEvaluation);
        document.invalidateScrollbars();
    });
}

void Page::setUnobscuredSafeAreaInsets(const FloatBoxExtent& insets)
{
    if (m_unobscuredSafeAreaInsets == insets)
        return;

    m_unobscuredSafeAreaInsets = insets;

    forEachDocument([&] (Document& document) {
        document.constantProperties().didChangeSafeAreaInsets();
    });
}

void Page::setUseSystemAppearance(bool value)
{
    if (m_useSystemAppearance == value)
        return;

    m_useSystemAppearance = value;

    appearanceDidChange();

    forEachDocument([&] (Document& document) {
        // System apperance change may affect stylesheet parsing. We need to reparse.
        document.extensionStyleSheets().clearPageUserSheet();
        document.extensionStyleSheets().invalidateInjectedStyleSheetCache();
    });
}

void Page::effectiveAppearanceDidChange(bool useDarkAppearance, bool useElevatedUserInterfaceLevel)
{
#if ENABLE(DARK_MODE_CSS)
    if (m_useDarkAppearance == useDarkAppearance && m_useElevatedUserInterfaceLevel == useElevatedUserInterfaceLevel)
        return;

    m_useDarkAppearance = useDarkAppearance;
    m_useElevatedUserInterfaceLevel = useElevatedUserInterfaceLevel;

    InspectorInstrumentation::defaultAppearanceDidChange(*this, useDarkAppearance);

    appearanceDidChange();
#else
    UNUSED_PARAM(useDarkAppearance);

    if (m_useElevatedUserInterfaceLevel == useElevatedUserInterfaceLevel)
        return;

    m_useElevatedUserInterfaceLevel = useElevatedUserInterfaceLevel;

    appearanceDidChange();
#endif
}

bool Page::useDarkAppearance() const
{
#if ENABLE(DARK_MODE_CSS)
    FrameView* view = mainFrame().view();
    if (!view || view->mediaType() != screenAtom())
        return false;
    if (m_useDarkAppearanceOverride)
        return m_useDarkAppearanceOverride.value();
    return m_useDarkAppearance;
#else
    return false;
#endif
}

void Page::setUseDarkAppearanceOverride(std::optional<bool> valueOverride)
{
#if HAVE(OS_DARK_MODE_SUPPORT)
    if (valueOverride == m_useDarkAppearanceOverride)
        return;

    m_useDarkAppearanceOverride = valueOverride;

    appearanceDidChange();
#else
    UNUSED_PARAM(valueOverride);
#endif
}

void Page::setFullscreenInsets(const FloatBoxExtent& insets)
{
    if (insets == m_fullscreenInsets)
        return;

    m_fullscreenInsets = insets;

    forEachDocument([] (Document& document) {
        document.constantProperties().didChangeFullscreenInsets();
    });
}

void Page::setFullscreenAutoHideDuration(Seconds duration)
{
    if (duration == m_fullscreenAutoHideDuration)
        return;

    m_fullscreenAutoHideDuration = duration;

    forEachDocument([&] (Document& document) {
        document.constantProperties().setFullscreenAutoHideDuration(duration);
    });
}

void Page::setFullscreenControlsHidden(bool hidden)
{
#if ENABLE(FULLSCREEN_API)
    forEachDocument([&] (Document& document) {
        document.fullscreenManager().setFullscreenControlsHidden(hidden);
    });
#else
    UNUSED_PARAM(hidden);
#endif
}

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
    m_pointerCaptureController->reset();

    if (m_sampledPageTopColor) {
        m_sampledPageTopColor = std::nullopt;
        chrome().client().sampledPageTopColorChanged();
    }
}

RenderingUpdateScheduler& Page::renderingUpdateScheduler()
{
    if (!m_renderingUpdateScheduler)
        m_renderingUpdateScheduler = RenderingUpdateScheduler::create(*this);
    return *m_renderingUpdateScheduler;
}

void Page::forEachDocumentFromMainFrame(const Frame& mainFrame, const Function<void(Document&)>& functor)
{
    Vector<Ref<Document>> documents;
    for (const AbstractFrame* frame = &mainFrame; frame; frame = frame->tree().traverseNext()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        auto* document = localFrame->document();
        if (!document)
            continue;
        documents.append(*document);
    }
    for (auto& document : documents)
        functor(document);
}

void Page::forEachDocument(const Function<void(Document&)>& functor) const
{
    forEachDocumentFromMainFrame(mainFrame(), functor);
}

void Page::forEachMediaElement(const Function<void(HTMLMediaElement&)>& functor)
{
#if ENABLE(VIDEO)
    forEachDocument([&] (Document& document) {
        document.forEachMediaElement(functor);
    });
#else
    UNUSED_PARAM(functor);
#endif
}

void Page::forEachFrame(const Function<void(Frame&)>& functor)
{
    Vector<Ref<Frame>> frames;
    for (AbstractFrame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        frames.append(*localFrame);
    }

    for (auto& frame : frames)
        functor(frame);
}

bool Page::allowsLoadFromURL(const URL& url, MainFrameMainResource mainFrameMainResource) const
{
    if (mainFrameMainResource == MainFrameMainResource::No && !m_loadsSubresources)
        return false;
    if (!m_allowedNetworkHosts)
        return true;
    if (!url.protocolIsInHTTPFamily() && !url.protocolIs("ws"_s) && !url.protocolIs("wss"_s))
        return true;
    return m_allowedNetworkHosts->contains<StringViewHashTranslator>(url.host());
}

void Page::applicationWillResignActive()
{
#if ENABLE(VIDEO)
    forEachMediaElement([] (HTMLMediaElement& element) {
        element.applicationWillResignActive();
    });
#endif
}

void Page::applicationDidEnterBackground()
{
    m_webRTCProvider->setActive(false);
}

void Page::applicationWillEnterForeground()
{
    m_webRTCProvider->setActive(true);
}

void Page::applicationDidBecomeActive()
{
#if ENABLE(VIDEO)
    forEachMediaElement([] (HTMLMediaElement& element) {
        element.applicationDidBecomeActive();
    });
#endif
}

#if ENABLE(WHEEL_EVENT_LATCHING)
ScrollLatchingController& Page::scrollLatchingController()
{
    if (!m_scrollLatchingController)
        m_scrollLatchingController = makeUnique<ScrollLatchingController>();
        
    return *m_scrollLatchingController;
}

ScrollLatchingController* Page::scrollLatchingControllerIfExists()
{
    return m_scrollLatchingController.get();
}
#endif // ENABLE(WHEEL_EVENT_LATCHING)

enum class DispatchedOnDocumentEventLoop : bool { No, Yes };
static void dispatchPrintEvent(Frame& mainFrame, const AtomString& eventType, DispatchedOnDocumentEventLoop dispatchedOnDocumentEventLoop)
{
    Vector<Ref<Frame>> frames;
    for (AbstractFrame* frame = &mainFrame; frame; frame = frame->tree().traverseNext()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        frames.append(*localFrame);
    }

    for (auto& frame : frames) {
        if (RefPtr window = frame->window()) {
            auto dispatchEvent = [window = WTFMove(window), eventType] {
                window->dispatchEvent(Event::create(eventType, Event::CanBubble::No, Event::IsCancelable::No), window->document());
            };
            if (dispatchedOnDocumentEventLoop == DispatchedOnDocumentEventLoop::No)
                return dispatchEvent();
            if (auto* document = frame->document())
                document->eventLoop().queueTask(TaskSource::DOMManipulation, WTFMove(dispatchEvent));
        }
    }
}

void Page::dispatchBeforePrintEvent()
{
    dispatchPrintEvent(m_mainFrame, eventNames().beforeprintEvent, DispatchedOnDocumentEventLoop::No);
}

void Page::dispatchAfterPrintEvent()
{
    dispatchPrintEvent(m_mainFrame, eventNames().afterprintEvent, DispatchedOnDocumentEventLoop::Yes);
}

#if ENABLE(APPLE_PAY)
void Page::setPaymentCoordinator(std::unique_ptr<PaymentCoordinator>&& paymentCoordinator)
{
    m_paymentCoordinator = WTFMove(paymentCoordinator);
}
#endif

#if ENABLE(APPLE_PAY_AMS_UI)

bool Page::startApplePayAMSUISession(Document& document, ApplePayAMSUIPaymentHandler& paymentHandler, const ApplePayAMSUIRequest& request)
{
    if (hasActiveApplePayAMSUISession())
        return false;

    m_activeApplePayAMSUIPaymentHandler = &paymentHandler;

    chrome().client().startApplePayAMSUISession(document.url(), request, [weakThis = WeakPtr { *this }, paymentHandlerPtr = &paymentHandler] (std::optional<bool>&& result) {
        auto strongThis = weakThis.get();
        if (!strongThis)
            return;

        if (paymentHandlerPtr != strongThis->m_activeApplePayAMSUIPaymentHandler)
            return;

        if (auto activePaymentHandler = std::exchange(strongThis->m_activeApplePayAMSUIPaymentHandler, nullptr))
            activePaymentHandler->finishSession(WTFMove(result));
    });
    return true;
}

void Page::abortApplePayAMSUISession(ApplePayAMSUIPaymentHandler& paymentHandler)
{
    if (&paymentHandler != m_activeApplePayAMSUIPaymentHandler)
        return;

    chrome().client().abortApplePayAMSUISession();

    if (auto activePaymentHandler = std::exchange(m_activeApplePayAMSUIPaymentHandler, nullptr))
        activePaymentHandler->finishSession(std::nullopt);
}

#endif // ENABLE(APPLE_PAY_AMS_UI)

#if ENABLE(MEDIA_SESSION_COORDINATOR)
void Page::setMediaSessionCoordinator(Ref<MediaSessionCoordinatorPrivate>&& mediaSessionCoordinator)
{
    m_mediaSessionCoordinator = WTFMove(mediaSessionCoordinator);

    auto* window = mainFrame().window();
    if (auto* navigator = window ? window->optionalNavigator() : nullptr)
        NavigatorMediaSession::mediaSession(*navigator).coordinator().setMediaSessionCoordinatorPrivate(*m_mediaSessionCoordinator);
}

void Page::invalidateMediaSessionCoordinator()
{
    m_mediaSessionCoordinator = nullptr;
    auto* window = mainFrame().window();
    if (!window)
        return;

    auto* navigator = window->optionalNavigator();
    if (!navigator)
        return;

    NavigatorMediaSession::mediaSession(*navigator).coordinator().close();
}
#endif

void Page::configureLoggingChannel(const String& channelName, WTFLogChannelState state, WTFLogLevel level)
{
#if !RELEASE_LOG_DISABLED
    if (auto* channel = getLogChannel(channelName)) {
        channel->state = state;
        channel->level = level;

#if USE(LIBWEBRTC)
        if (channel == &LogWebRTC && m_mainFrame->document() && !sessionID().isEphemeral())
            webRTCProvider().setLoggingLevel(LogWebRTC.level);
#endif
    }

    chrome().client().configureLoggingChannel(channelName, state, level);
#else
    UNUSED_PARAM(channelName);
    UNUSED_PARAM(state);
    UNUSED_PARAM(level);
#endif
}

void Page::didFinishLoadingImageForElement(HTMLImageElement& element)
{
    element.document().eventLoop().queueTask(TaskSource::Networking, [element = Ref { element }]() {
        RefPtr frame = element->document().frame();
        if (!frame)
            return;

        frame->editor().revealSelectionIfNeededAfterLoadingImageForElement(element);

        if (element->document().frame() != frame)
            return;

        if (auto* page = frame->page()) {
#if ENABLE(IMAGE_ANALYSIS)
            if (auto* queue = page->imageAnalysisQueueIfExists())
                queue->enqueueIfNeeded(element);
#endif
            page->chrome().client().didFinishLoadingImageForElement(element);
        }
    });
}

#if ENABLE(TEXT_AUTOSIZING)

void Page::recomputeTextAutoSizingInAllFrames()
{
    ASSERT(settings().textAutosizingEnabled() && settings().textAutosizingUsesIdempotentMode());
    forEachDocument([] (Document& document) {
        if (auto* renderView = document.renderView()) {
            for (auto& renderer : descendantsOfType<RenderElement>(*renderView)) {
                // Use the fact that descendantsOfType() returns parent nodes before child nodes.
                // The adjustment is only valid if the parent nodes have already been updated.
                if (auto* element = renderer.element()) {
                    if (auto adjustment = Style::Adjuster::adjustmentForTextAutosizing(renderer.style(), *element)) {
                        auto newStyle = RenderStyle::clone(renderer.style());
                        Style::Adjuster::adjustForTextAutosizing(newStyle, *element, adjustment);
                        renderer.setStyle(WTFMove(newStyle));
                    }
                }
            }
        }
    });
}

#endif

OptionSet<FilterRenderingMode> Page::preferredFilterRenderingModes() const
{
    OptionSet<FilterRenderingMode> modes = FilterRenderingMode::Software;
#if USE(CORE_IMAGE)
    if (settings().acceleratedFiltersEnabled())
        modes.add(FilterRenderingMode::Accelerated);
#endif
#if USE(GRAPHICS_CONTEXT_FILTERS)
    if (settings().graphicsContextFiltersEnabled())
        modes.add(FilterRenderingMode::GraphicsContext);
#endif
    return modes;
}

bool Page::shouldDisableCorsForRequestTo(const URL& url) const
{
    return WTF::anyOf(m_corsDisablingPatterns, [&] (const auto& pattern) {
        return pattern.matches(url);
    });
}

void Page::revealCurrentSelection()
{
    CheckedRef(focusController())->focusedOrMainFrame().selection().revealSelection(SelectionRevealMode::Reveal, ScrollAlignment::alignCenterIfNeeded);
}

void Page::injectUserStyleSheet(UserStyleSheet& userStyleSheet)
{
#if ENABLE(APP_BOUND_DOMAINS)
    if (m_mainFrame->loader().client().shouldEnableInAppBrowserPrivacyProtections()) {
        if (auto* document = m_mainFrame->document())
            document->addConsoleMessage(MessageSource::Security, MessageLevel::Warning, "Ignoring user style sheet for non-app bound domain."_s);
        return;
    }
    m_mainFrame->loader().client().notifyPageOfAppBoundBehavior();
#endif

    // We need to wait until we're no longer displaying the initial empty document before we can inject the stylesheets.
    if (m_mainFrame->loader().stateMachine().isDisplayingInitialEmptyDocument()) {
        m_userStyleSheetsPendingInjection.append(userStyleSheet);
        return;
    }

    if (userStyleSheet.injectedFrames() == UserContentInjectedFrames::InjectInTopFrameOnly) {
        if (auto* document = m_mainFrame->document())
            document->extensionStyleSheets().injectPageSpecificUserStyleSheet(userStyleSheet);
    } else {
        forEachDocument([&] (Document& document) {
            document.extensionStyleSheets().injectPageSpecificUserStyleSheet(userStyleSheet);
        });
    }
}

void Page::removeInjectedUserStyleSheet(UserStyleSheet& userStyleSheet)
{
    if (!m_userStyleSheetsPendingInjection.isEmpty()) {
        m_userStyleSheetsPendingInjection.removeFirstMatching([userStyleSheet](auto& storedUserStyleSheet) {
            return storedUserStyleSheet.url() == userStyleSheet.url();
        });
        return;
    }

    if (userStyleSheet.injectedFrames() == UserContentInjectedFrames::InjectInTopFrameOnly) {
        if (auto* document = m_mainFrame->document())
            document->extensionStyleSheets().removePageSpecificUserStyleSheet(userStyleSheet);
    } else {
        forEachDocument([&] (Document& document) {
            document.extensionStyleSheets().removePageSpecificUserStyleSheet(userStyleSheet);
        });
    }
}

void Page::mainFrameDidChangeToNonInitialEmptyDocument()
{
    ASSERT(!m_mainFrame->loader().stateMachine().isDisplayingInitialEmptyDocument());
    for (auto& userStyleSheet : m_userStyleSheetsPendingInjection)
        injectUserStyleSheet(userStyleSheet);
    m_userStyleSheetsPendingInjection.clear();
}

SpeechRecognitionConnection& Page::speechRecognitionConnection()
{
    return m_speechRecognitionProvider->speechRecognitionConnection();
}

WTF::TextStream& operator<<(WTF::TextStream& ts, RenderingUpdateStep step)
{
    switch (step) {
    case RenderingUpdateStep::FlushAutofocusCandidates: ts << "FlushAutofocusCandidates"; break;
    case RenderingUpdateStep::Resize: ts << "Resize"; break;
    case RenderingUpdateStep::Scroll: ts << "Scroll"; break;
    case RenderingUpdateStep::MediaQueryEvaluation: ts << "MediaQueryEvaluation"; break;
    case RenderingUpdateStep::Animations: ts << "Animations"; break;
    case RenderingUpdateStep::Fullscreen: ts << "Fullscreen"; break;
    case RenderingUpdateStep::AnimationFrameCallbacks: ts << "AnimationFrameCallbacks"; break;
    case RenderingUpdateStep::IntersectionObservations: ts << "IntersectionObservations"; break;
    case RenderingUpdateStep::ResizeObservations: ts << "ResizeObservations"; break;
    case RenderingUpdateStep::Images: ts << "Images"; break;
    case RenderingUpdateStep::WheelEventMonitorCallbacks: ts << "WheelEventMonitorCallbacks"; break;
    case RenderingUpdateStep::CursorUpdate: ts << "CursorUpdate"; break;
    case RenderingUpdateStep::EventRegionUpdate: ts << "EventRegionUpdate"; break;
    case RenderingUpdateStep::LayerFlush: ts << "LayerFlush"; break;
#if ENABLE(ASYNC_SCROLLING)
    case RenderingUpdateStep::ScrollingTreeUpdate: ts << "ScrollingTreeUpdate"; break;
#endif
    case RenderingUpdateStep::VideoFrameCallbacks: ts << "VideoFrameCallbacks"; break;
    case RenderingUpdateStep::PrepareCanvasesForDisplay: ts << "PrepareCanvasesForDisplay"; break;
    case RenderingUpdateStep::CaretAnimation: ts << "CaretAnimation"; break;
    }
    return ts;
}

ImageOverlayController& Page::imageOverlayController()
{
    if (!m_imageOverlayController)
        m_imageOverlayController = makeUnique<ImageOverlayController>(*this);
    return *m_imageOverlayController;
}

Page* Page::serviceWorkerPage(ScriptExecutionContextIdentifier serviceWorkerPageIdentifier)
{
    auto* serviceWorkerPageDocument = Document::allDocumentsMap().get(serviceWorkerPageIdentifier);
    return serviceWorkerPageDocument ? serviceWorkerPageDocument->page() : nullptr;
}

#if ENABLE(IMAGE_ANALYSIS)

ImageAnalysisQueue& Page::imageAnalysisQueue()
{
    if (!m_imageAnalysisQueue)
        m_imageAnalysisQueue = makeUnique<ImageAnalysisQueue>(*this);
    return *m_imageAnalysisQueue;
}

void Page::resetImageAnalysisQueue()
{
    if (auto previousQueue = std::exchange(m_imageAnalysisQueue, { }))
        previousQueue->clear();
}

void Page::updateElementsWithTextRecognitionResults()
{
    if (m_textRecognitionResults.isEmptyIgnoringNullReferences())
        return;

    m_textRecognitionResults.removeNullReferences();

    Vector<std::pair<Ref<HTMLElement>, TextRecognitionResult>> elementsToUpdate;
    for (auto entry : m_textRecognitionResults) {
        Ref protectedElement = entry.key;
        if (!protectedElement->isConnected())
            continue;

        auto renderer = protectedElement->renderer();
        if (!is<RenderImage>(renderer))
            continue;

        auto& [result, containerRect] = entry.value;
        auto newContainerRect = ImageOverlay::containerRect(protectedElement.get());
        if (containerRect == newContainerRect)
            continue;

        containerRect = newContainerRect;
        elementsToUpdate.append({ WTFMove(protectedElement), result });
    }

    for (auto& [element, result] : elementsToUpdate) {
        element->document().eventLoop().queueTask(TaskSource::InternalAsyncTask, [result = TextRecognitionResult { result }, weakElement = WeakPtr { element }] {
            if (RefPtr element = weakElement.get())
                ImageOverlay::updateWithTextRecognitionResult(*element, result, ImageOverlay::CacheTextRecognitionResults::No);
        });
    }
}

bool Page::hasCachedTextRecognitionResult(const HTMLElement& element) const
{
    return m_textRecognitionResults.contains(element);
}

std::optional<TextRecognitionResult> Page::cachedTextRecognitionResult(const HTMLElement& element) const
{
    auto iterator = m_textRecognitionResults.find(element);
    if (iterator == m_textRecognitionResults.end())
        return std::nullopt;

    return { iterator->value.first };
}

void Page::cacheTextRecognitionResult(const HTMLElement& element, const IntRect& containerRect, const TextRecognitionResult& result)
{
    m_textRecognitionResults.set(element, CachedTextRecognitionResult { result, containerRect });
}

void Page::resetTextRecognitionResults()
{
    m_textRecognitionResults.clear();
}

void Page::resetTextRecognitionResult(const HTMLElement& element)
{
    m_textRecognitionResults.remove(element);
}

#endif // ENABLE(IMAGE_ANALYSIS)

#if ENABLE(SERVICE_WORKER)
JSC::JSGlobalObject* Page::serviceWorkerGlobalObject(DOMWrapperWorld& world)
{
    if (!m_serviceWorkerGlobalScope)
        return nullptr;

    auto scriptController = m_serviceWorkerGlobalScope->script();
    if (!scriptController)
        return nullptr;

    // FIXME: We currently do not support non-normal worlds in service workers.
    RELEASE_ASSERT(&static_cast<JSVMClientData*>(m_serviceWorkerGlobalScope->vm().clientData)->normalWorld() == &world);
    return scriptController->globalScopeWrapper();
}

void Page::setServiceWorkerGlobalScope(ServiceWorkerGlobalScope& serviceWorkerGlobalScope)
{
    ASSERT(isMainThread());
    ASSERT(m_isServiceWorkerPage);
    m_serviceWorkerGlobalScope = serviceWorkerGlobalScope;
}
#endif

StorageConnection& Page::storageConnection()
{
    return m_storageProvider->storageConnection();
}

ModelPlayerProvider& Page::modelPlayerProvider()
{
    return m_modelPlayerProvider.get();
}

void Page::setupForRemoteWorker(const URL& scriptURL, const SecurityOriginData& topOrigin, const String& referrerPolicy)
{
    mainFrame().loader().initForSynthesizedDocument({ });
    auto document = Document::createNonRenderedPlaceholder(mainFrame(), scriptURL);
    document->createDOMWindow();
    document->storageBlockingStateDidChange();

    auto origin = topOrigin.securityOrigin();
    auto originAsURL = origin->toURL();
    document->setSiteForCookies(originAsURL);
    document->setFirstPartyForCookies(originAsURL);

    if (document->settings().storageBlockingPolicy() != StorageBlockingPolicy::BlockThirdParty)
        document->setDomainForCachePartition(String { emptyString() });
    else
        document->setDomainForCachePartition(origin->domainForCachePartition());

    if (auto policy = parseReferrerPolicy(referrerPolicy, ReferrerPolicySource::HTTPHeader))
        document->setReferrerPolicy(*policy);

    mainFrame().setDocument(WTFMove(document));
}

void Page::forceRepaintAllFrames()
{
    for (AbstractFrame* frame = &mainFrame(); frame; frame = frame->tree().traverseNext()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        FrameView* frameView = localFrame->view();
        if (!frameView || !frameView->renderView())
            continue;

        frameView->renderView()->repaintViewAndCompositedLayers();
    }
}

void Page::updatePlayStateForAllAnimations()
{
    if (auto* view = mainFrame().view())
        view->updatePlayStateForAllAnimationsIncludingSubframes();
}

ScreenOrientationManager* Page::screenOrientationManager() const
{
    return m_screenOrientationManager.get();
}

URL Page::sanitizeForCopyOrShare(const URL& url) const
{
    return chrome().client().sanitizeForCopyOrShare(url);
}

String Page::sanitizeForCopyOrShare(const String& urlString) const
{
    if (auto url = URL { urlString }; url.isValid())
        return sanitizeForCopyOrShare(WTFMove(url)).string();
    return urlString;
}

} // namespace WebCore
