/*
 * Copyright (C) 2010, 2011, 2013, 2015 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WKBundlePage.h"
#include "WKBundlePagePrivate.h"

#include "APIArray.h"
#include "APIFrameHandle.h"
#include "APIString.h"
#include "APIURL.h"
#include "APIURLRequest.h"
#include "InjectedBundleBackForwardList.h"
#include "InjectedBundleNodeHandle.h"
#include "InjectedBundlePageEditorClient.h"
#include "InjectedBundlePageFormClient.h"
#include "InjectedBundlePageLoaderClient.h"
#include "InjectedBundlePageResourceLoadClient.h"
#include "InjectedBundlePageUIClient.h"
#include "PageBanner.h"
#include "WKAPICast.h"
#include "WKArray.h"
#include "WKBundleAPICast.h"
#include "WKRetainPtr.h"
#include "WKString.h"
#include "WebContextMenu.h"
#include "WebContextMenuItem.h"
#include "WebFrame.h"
#include "WebFullScreenManager.h"
#include "WebImage.h"
#include "WebInspector.h"
#include "WebPage.h"
#include "WebPageGroupProxy.h"
#include "WebPageOverlay.h"
#include "WebRenderLayer.h"
#include "WebRenderObject.h"
#include <WebCore/AXObjectCache.h>
#include <WebCore/AccessibilityObject.h>
#include <WebCore/ApplicationCacheStorage.h>
#include <WebCore/Frame.h>
#include <WebCore/Page.h>
#include <WebCore/PageOverlay.h>
#include <WebCore/PageOverlayController.h>
#include <WebCore/RenderLayerCompositor.h>
#include <WebCore/ScriptExecutionContext.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/URL.h>
#include <WebCore/WheelEventTestTrigger.h>
#include <wtf/StdLibExtras.h>

using namespace WebKit;

WKTypeID WKBundlePageGetTypeID()
{
    return toAPI(WebPage::APIType);
}

void WKBundlePageSetContextMenuClient(WKBundlePageRef pageRef, WKBundlePageContextMenuClientBase* wkClient)
{
#if ENABLE(CONTEXT_MENUS)
    toImpl(pageRef)->setInjectedBundleContextMenuClient(std::make_unique<InjectedBundlePageContextMenuClient>(wkClient));
#else
    UNUSED_PARAM(pageRef);
    UNUSED_PARAM(wkClient);
#endif
}

void WKBundlePageSetEditorClient(WKBundlePageRef pageRef, WKBundlePageEditorClientBase* wkClient)
{
    toImpl(pageRef)->setInjectedBundleEditorClient(wkClient ? std::make_unique<InjectedBundlePageEditorClient>(*wkClient) : std::make_unique<API::InjectedBundle::EditorClient>());
}

void WKBundlePageSetFormClient(WKBundlePageRef pageRef, WKBundlePageFormClientBase* wkClient)
{
    toImpl(pageRef)->setInjectedBundleFormClient(std::make_unique<InjectedBundlePageFormClient>(wkClient));
}

void WKBundlePageSetPageLoaderClient(WKBundlePageRef pageRef, WKBundlePageLoaderClientBase* wkClient)
{
    toImpl(pageRef)->setInjectedBundlePageLoaderClient(std::make_unique<InjectedBundlePageLoaderClient>(wkClient));
}

void WKBundlePageSetResourceLoadClient(WKBundlePageRef pageRef, WKBundlePageResourceLoadClientBase* wkClient)
{
    toImpl(pageRef)->setInjectedBundleResourceLoadClient(std::make_unique<InjectedBundlePageResourceLoadClient>(wkClient));
}

void WKBundlePageSetPolicyClient(WKBundlePageRef pageRef, WKBundlePagePolicyClientBase* wkClient)
{
    toImpl(pageRef)->initializeInjectedBundlePolicyClient(wkClient);
}

void WKBundlePageSetUIClient(WKBundlePageRef pageRef, WKBundlePageUIClientBase* wkClient)
{
    toImpl(pageRef)->setInjectedBundleUIClient(std::make_unique<InjectedBundlePageUIClient>(wkClient));
}

void WKBundlePageSetFullScreenClient(WKBundlePageRef pageRef, WKBundlePageFullScreenClientBase* wkClient)
{
#if defined(ENABLE_FULLSCREEN_API) && ENABLE_FULLSCREEN_API
    toImpl(pageRef)->initializeInjectedBundleFullScreenClient(wkClient);
#else
    UNUSED_PARAM(pageRef);
    UNUSED_PARAM(wkClient);
#endif
}

void WKBundlePageWillEnterFullScreen(WKBundlePageRef pageRef)
{
#if defined(ENABLE_FULLSCREEN_API) && ENABLE_FULLSCREEN_API
    toImpl(pageRef)->fullScreenManager()->willEnterFullScreen();
#else
    UNUSED_PARAM(pageRef);
#endif
}

void WKBundlePageDidEnterFullScreen(WKBundlePageRef pageRef)
{
#if defined(ENABLE_FULLSCREEN_API) && ENABLE_FULLSCREEN_API
    toImpl(pageRef)->fullScreenManager()->didEnterFullScreen();
#else
    UNUSED_PARAM(pageRef);
#endif
}

void WKBundlePageWillExitFullScreen(WKBundlePageRef pageRef)
{
#if defined(ENABLE_FULLSCREEN_API) && ENABLE_FULLSCREEN_API
    toImpl(pageRef)->fullScreenManager()->willExitFullScreen();
#else
    UNUSED_PARAM(pageRef);
#endif
}

void WKBundlePageDidExitFullScreen(WKBundlePageRef pageRef)
{
#if defined(ENABLE_FULLSCREEN_API) && ENABLE_FULLSCREEN_API
    toImpl(pageRef)->fullScreenManager()->didExitFullScreen();
#else
    UNUSED_PARAM(pageRef);
#endif
}

WKBundlePageGroupRef WKBundlePageGetPageGroup(WKBundlePageRef pageRef)
{
    return toAPI(toImpl(pageRef)->pageGroup());
}

WKBundleFrameRef WKBundlePageGetMainFrame(WKBundlePageRef pageRef)
{
    return toAPI(toImpl(pageRef)->mainWebFrame());
}

WKFrameHandleRef WKBundleFrameCreateFrameHandle(WKBundleFrameRef bundleFrameRef)
{
    return toAPI(&API::FrameHandle::create(toImpl(bundleFrameRef)->frameID()).leakRef());
}

void WKBundlePageClickMenuItem(WKBundlePageRef pageRef, WKContextMenuItemRef item)
{
#if ENABLE(CONTEXT_MENUS)
    toImpl(pageRef)->contextMenu()->itemSelected(toImpl(item)->data());
#else
    UNUSED_PARAM(pageRef);
    UNUSED_PARAM(item);
#endif
}

#if ENABLE(CONTEXT_MENUS)
static Ref<API::Array> contextMenuItems(const WebContextMenu& contextMenu)
{
    auto items = contextMenu.items();

    Vector<RefPtr<API::Object>> menuItems;
    menuItems.reserveInitialCapacity(items.size());

    for (const auto& item : items)
        menuItems.uncheckedAppend(WebContextMenuItem::create(item));

    return API::Array::create(WTFMove(menuItems));
}
#endif

WKArrayRef WKBundlePageCopyContextMenuItems(WKBundlePageRef pageRef)
{
#if ENABLE(CONTEXT_MENUS)
    WebContextMenu* contextMenu = toImpl(pageRef)->contextMenu();

    return toAPI(&contextMenuItems(*contextMenu).leakRef());
#else
    UNUSED_PARAM(pageRef);
    return nullptr;
#endif
}

WKArrayRef WKBundlePageCopyContextMenuAtPointInWindow(WKBundlePageRef pageRef, WKPoint point)
{
#if ENABLE(CONTEXT_MENUS)
    WebContextMenu* contextMenu = toImpl(pageRef)->contextMenuAtPointInWindow(toIntPoint(point));
    if (!contextMenu)
        return nullptr;

    return toAPI(&contextMenuItems(*contextMenu).leakRef());
#else
    UNUSED_PARAM(pageRef);
    UNUSED_PARAM(point);
    return nullptr;
#endif
}

void WKBundlePageInsertNewlineInQuotedContent(WKBundlePageRef pageRef)
{
    toImpl(pageRef)->insertNewlineInQuotedContent();
}

void* WKAccessibilityRootObject(WKBundlePageRef pageRef)
{
#if HAVE(ACCESSIBILITY)
    if (!pageRef)
        return 0;
    
    WebCore::Page* page = toImpl(pageRef)->corePage();
    if (!page)
        return 0;
    
    WebCore::Frame& core = page->mainFrame();
    if (!core.document())
        return 0;
    
    WebCore::AXObjectCache::enableAccessibility();

    WebCore::AccessibilityObject* root = core.document()->axObjectCache()->rootObject();
    if (!root)
        return 0;
    
    return root->wrapper();
#else
    UNUSED_PARAM(pageRef);
    return 0;
#endif
}

void* WKAccessibilityFocusedObject(WKBundlePageRef pageRef)
{
#if HAVE(ACCESSIBILITY)
    if (!pageRef)
        return 0;
    
    WebCore::Page* page = toImpl(pageRef)->corePage();
    if (!page)
        return 0;

    WebCore::AXObjectCache::enableAccessibility();

    WebCore::AccessibilityObject* focusedObject = WebCore::AXObjectCache::focusedUIElementForPage(page);
    if (!focusedObject)
        return 0;
    
    return focusedObject->wrapper();
#else
    UNUSED_PARAM(pageRef);
    return 0;
#endif
}

void WKAccessibilityEnableEnhancedAccessibility(bool enable)
{
#if HAVE(ACCESSIBILITY)
    WebCore::AXObjectCache::setEnhancedUserInterfaceAccessibility(enable);
#endif
}

bool WKAccessibilityEnhancedAccessibilityEnabled()
{
#if HAVE(ACCESSIBILITY)
    return WebCore::AXObjectCache::accessibilityEnhancedUserInterfaceEnabled();
#else
    return false;
#endif
}

void WKBundlePageStopLoading(WKBundlePageRef pageRef)
{
    toImpl(pageRef)->stopLoading();
}

void WKBundlePageSetDefersLoading(WKBundlePageRef pageRef, bool defersLoading)
{
    toImpl(pageRef)->setDefersLoading(defersLoading);
}

WKStringRef WKBundlePageCopyRenderTreeExternalRepresentation(WKBundlePageRef pageRef)
{
    return toCopiedAPI(toImpl(pageRef)->renderTreeExternalRepresentation());
}

WKStringRef WKBundlePageCopyRenderTreeExternalRepresentationForPrinting(WKBundlePageRef pageRef)
{
    return toCopiedAPI(toImpl(pageRef)->renderTreeExternalRepresentationForPrinting());
}

void WKBundlePageExecuteEditingCommand(WKBundlePageRef pageRef, WKStringRef name, WKStringRef argument)
{
    toImpl(pageRef)->executeEditingCommand(toWTFString(name), toWTFString(argument));
}

bool WKBundlePageIsEditingCommandEnabled(WKBundlePageRef pageRef, WKStringRef name)
{
    return toImpl(pageRef)->isEditingCommandEnabled(toWTFString(name));
}

void WKBundlePageClearMainFrameName(WKBundlePageRef pageRef)
{
    toImpl(pageRef)->clearMainFrameName();
}

void WKBundlePageClose(WKBundlePageRef pageRef)
{
    toImpl(pageRef)->sendClose();
}

double WKBundlePageGetTextZoomFactor(WKBundlePageRef pageRef)
{
    return toImpl(pageRef)->textZoomFactor();
}

void WKBundlePageSetTextZoomFactor(WKBundlePageRef pageRef, double zoomFactor)
{
    toImpl(pageRef)->setTextZoomFactor(zoomFactor);
}

double WKBundlePageGetPageZoomFactor(WKBundlePageRef pageRef)
{
    return toImpl(pageRef)->pageZoomFactor();
}

void WKBundlePageSetPageZoomFactor(WKBundlePageRef pageRef, double zoomFactor)
{
    toImpl(pageRef)->setPageZoomFactor(zoomFactor);
}

void WKBundlePageSetScaleAtOrigin(WKBundlePageRef pageRef, double scale, WKPoint origin)
{
    toImpl(pageRef)->scalePage(scale, toIntPoint(origin));
}

WKBundleBackForwardListRef WKBundlePageGetBackForwardList(WKBundlePageRef pageRef)
{
    return toAPI(toImpl(pageRef)->backForwardList());
}

void WKBundlePageInstallPageOverlay(WKBundlePageRef pageRef, WKBundlePageOverlayRef pageOverlayRef)
{
    toImpl(pageRef)->corePage()->pageOverlayController().installPageOverlay(*toImpl(pageOverlayRef)->coreOverlay(), WebCore::PageOverlay::FadeMode::DoNotFade);
}

void WKBundlePageUninstallPageOverlay(WKBundlePageRef pageRef, WKBundlePageOverlayRef pageOverlayRef)
{
    toImpl(pageRef)->corePage()->pageOverlayController().uninstallPageOverlay(*toImpl(pageOverlayRef)->coreOverlay(), WebCore::PageOverlay::FadeMode::DoNotFade);
}

void WKBundlePageInstallPageOverlayWithAnimation(WKBundlePageRef pageRef, WKBundlePageOverlayRef pageOverlayRef)
{
    toImpl(pageRef)->corePage()->pageOverlayController().installPageOverlay(*toImpl(pageOverlayRef)->coreOverlay(), WebCore::PageOverlay::FadeMode::Fade);
}

void WKBundlePageUninstallPageOverlayWithAnimation(WKBundlePageRef pageRef, WKBundlePageOverlayRef pageOverlayRef)
{
    toImpl(pageRef)->corePage()->pageOverlayController().uninstallPageOverlay(*toImpl(pageOverlayRef)->coreOverlay(), WebCore::PageOverlay::FadeMode::Fade);
}

void WKBundlePageSetTopOverhangImage(WKBundlePageRef pageRef, WKImageRef imageRef)
{
#if PLATFORM(MAC)
    toImpl(pageRef)->setTopOverhangImage(toImpl(imageRef));
#else
    UNUSED_PARAM(pageRef);
    UNUSED_PARAM(imageRef);
#endif
}

void WKBundlePageSetBottomOverhangImage(WKBundlePageRef pageRef, WKImageRef imageRef)
{
#if PLATFORM(MAC)
    toImpl(pageRef)->setBottomOverhangImage(toImpl(imageRef));
#else
    UNUSED_PARAM(pageRef);
    UNUSED_PARAM(imageRef);
#endif
}

#if !PLATFORM(IOS)
void WKBundlePageSetHeaderBanner(WKBundlePageRef pageRef, WKBundlePageBannerRef bannerRef)
{
    toImpl(pageRef)->setHeaderPageBanner(toImpl(bannerRef));
}

void WKBundlePageSetFooterBanner(WKBundlePageRef pageRef, WKBundlePageBannerRef bannerRef)
{
    toImpl(pageRef)->setFooterPageBanner(toImpl(bannerRef));
}
#endif // !PLATFORM(IOS)

bool WKBundlePageHasLocalDataForURL(WKBundlePageRef pageRef, WKURLRef urlRef)
{
    return toImpl(pageRef)->hasLocalDataForURL(WebCore::URL(WebCore::URL(), toWTFString(urlRef)));
}

bool WKBundlePageCanHandleRequest(WKURLRequestRef requestRef)
{
    if (!requestRef)
        return false;
    return WebPage::canHandleRequest(toImpl(requestRef)->resourceRequest());
}

bool WKBundlePageFindString(WKBundlePageRef pageRef, WKStringRef target, WKFindOptions findOptions)
{
    return toImpl(pageRef)->findStringFromInjectedBundle(toWTFString(target), toFindOptions(findOptions));
}

WKImageRef WKBundlePageCreateSnapshotWithOptions(WKBundlePageRef pageRef, WKRect rect, WKSnapshotOptions options)
{
    RefPtr<WebImage> webImage = toImpl(pageRef)->scaledSnapshotWithOptions(toIntRect(rect), 1, toSnapshotOptions(options));
    return toAPI(webImage.leakRef());
}

WKImageRef WKBundlePageCreateSnapshotInViewCoordinates(WKBundlePageRef pageRef, WKRect rect, WKImageOptions options)
{
    SnapshotOptions snapshotOptions = snapshotOptionsFromImageOptions(options);
    snapshotOptions |= SnapshotOptionsInViewCoordinates;
    RefPtr<WebImage> webImage = toImpl(pageRef)->scaledSnapshotWithOptions(toIntRect(rect), 1, snapshotOptions);
    return toAPI(webImage.leakRef());
}

WKImageRef WKBundlePageCreateSnapshotInDocumentCoordinates(WKBundlePageRef pageRef, WKRect rect, WKImageOptions options)
{
    RefPtr<WebImage> webImage = toImpl(pageRef)->scaledSnapshotWithOptions(toIntRect(rect), 1, snapshotOptionsFromImageOptions(options));
    return toAPI(webImage.leakRef());
}

WKImageRef WKBundlePageCreateScaledSnapshotInDocumentCoordinates(WKBundlePageRef pageRef, WKRect rect, double scaleFactor, WKImageOptions options)
{
    RefPtr<WebImage> webImage = toImpl(pageRef)->scaledSnapshotWithOptions(toIntRect(rect), scaleFactor, snapshotOptionsFromImageOptions(options));
    return toAPI(webImage.leakRef());
}

double WKBundlePageGetBackingScaleFactor(WKBundlePageRef pageRef)
{
    return toImpl(pageRef)->deviceScaleFactor();
}

void WKBundlePageListenForLayoutMilestones(WKBundlePageRef pageRef, WKLayoutMilestones milestones)
{
    toImpl(pageRef)->listenForLayoutMilestones(toLayoutMilestones(milestones));
}

WKBundleInspectorRef WKBundlePageGetInspector(WKBundlePageRef pageRef)
{
    return toAPI(toImpl(pageRef)->inspector());
}

void WKBundlePageForceRepaint(WKBundlePageRef page)
{
    toImpl(page)->forceRepaintWithoutCallback();
}

void WKBundlePageSimulateMouseDown(WKBundlePageRef page, int button, WKPoint position, int clickCount, WKEventModifiers modifiers, double time)
{
    toImpl(page)->simulateMouseDown(button, toIntPoint(position), clickCount, modifiers, WallTime::fromRawSeconds(time));
}

void WKBundlePageSimulateMouseUp(WKBundlePageRef page, int button, WKPoint position, int clickCount, WKEventModifiers modifiers, double time)
{
    toImpl(page)->simulateMouseUp(button, toIntPoint(position), clickCount, modifiers, WallTime::fromRawSeconds(time));
}

void WKBundlePageSimulateMouseMotion(WKBundlePageRef page, WKPoint position, double time)
{
    toImpl(page)->simulateMouseMotion(toIntPoint(position), WallTime::fromRawSeconds(time));
}

uint64_t WKBundlePageGetRenderTreeSize(WKBundlePageRef pageRef)
{
    return toImpl(pageRef)->renderTreeSize();
}

WKRenderObjectRef WKBundlePageCopyRenderTree(WKBundlePageRef pageRef)
{
    return toAPI(WebRenderObject::create(toImpl(pageRef)).leakRef());
}

WKRenderLayerRef WKBundlePageCopyRenderLayerTree(WKBundlePageRef pageRef)
{
    return toAPI(WebRenderLayer::create(toImpl(pageRef)).leakRef());
}

void WKBundlePageSetPaintedObjectsCounterThreshold(WKBundlePageRef, uint64_t)
{
    // FIXME: This function is only still here to keep open source Mac builds building.
    // We should remove it as soon as we can.
}

void WKBundlePageSetTracksRepaints(WKBundlePageRef pageRef, bool trackRepaints)
{
    toImpl(pageRef)->setTracksRepaints(trackRepaints);
}

bool WKBundlePageIsTrackingRepaints(WKBundlePageRef pageRef)
{
    return toImpl(pageRef)->isTrackingRepaints();
}

void WKBundlePageResetTrackedRepaints(WKBundlePageRef pageRef)
{
    toImpl(pageRef)->resetTrackedRepaints();
}

WKArrayRef WKBundlePageCopyTrackedRepaintRects(WKBundlePageRef pageRef)
{
    return toAPI(&toImpl(pageRef)->trackedRepaintRects().leakRef());
}

void WKBundlePageSetComposition(WKBundlePageRef pageRef, WKStringRef text, int from, int length, bool suppressUnderline)
{
    toImpl(pageRef)->setCompositionForTesting(toWTFString(text), from, length, suppressUnderline);
}

bool WKBundlePageHasComposition(WKBundlePageRef pageRef)
{
    return toImpl(pageRef)->hasCompositionForTesting();
}

void WKBundlePageConfirmComposition(WKBundlePageRef pageRef)
{
    toImpl(pageRef)->confirmCompositionForTesting(String());
}

void WKBundlePageConfirmCompositionWithText(WKBundlePageRef pageRef, WKStringRef text)
{
    toImpl(pageRef)->confirmCompositionForTesting(toWTFString(text));
}

bool WKBundlePageCanShowMIMEType(WKBundlePageRef pageRef, WKStringRef mimeTypeRef)
{
    return toImpl(pageRef)->canShowMIMEType(toWTFString(mimeTypeRef));
}

WKRenderingSuppressionToken WKBundlePageExtendIncrementalRenderingSuppression(WKBundlePageRef pageRef)
{
    return toImpl(pageRef)->extendIncrementalRenderingSuppression();
}

void WKBundlePageStopExtendingIncrementalRenderingSuppression(WKBundlePageRef pageRef, WKRenderingSuppressionToken token)
{
    toImpl(pageRef)->stopExtendingIncrementalRenderingSuppression(token);
}

bool WKBundlePageIsUsingEphemeralSession(WKBundlePageRef pageRef)
{
    return toImpl(pageRef)->usesEphemeralSession();
}

bool WKBundlePageIsControlledByAutomation(WKBundlePageRef pageRef)
{
    return toImpl(pageRef)->isControlledByAutomation();
}

#if TARGET_OS_IPHONE
void WKBundlePageSetUseTestingViewportConfiguration(WKBundlePageRef pageRef, bool useTestingViewportConfiguration)
{
    toImpl(pageRef)->setUseTestingViewportConfiguration(useTestingViewportConfiguration);
}
#endif

void WKBundlePageStartMonitoringScrollOperations(WKBundlePageRef pageRef)
{
    WebKit::WebPage* webPage = toImpl(pageRef);
    WebCore::Page* page = webPage ? webPage->corePage() : nullptr;
    
    if (!page)
        return;

    page->ensureTestTrigger();
}

void WKBundlePageRegisterScrollOperationCompletionCallback(WKBundlePageRef pageRef, WKBundlePageTestNotificationCallback callback, void* context)
{
    if (!callback)
        return;
    
    WebKit::WebPage* webPage = toImpl(pageRef);
    WebCore::Page* page = webPage ? webPage->corePage() : nullptr;
    if (!page || !page->expectsWheelEventTriggers())
        return;
    
    page->ensureTestTrigger().setTestCallbackAndStartNotificationTimer([=]() {
        callback(context);
    });
}

void WKBundlePageCallAfterTasksAndTimers(WKBundlePageRef pageRef, WKBundlePageTestNotificationCallback callback, void* context)
{
    if (!callback)
        return;
    
    WebKit::WebPage* webPage = toImpl(pageRef);
    WebCore::Page* page = webPage ? webPage->corePage() : nullptr;
    if (!page)
        return;

    WebCore::Document* document = page->mainFrame().document();
    if (!document)
        return;

    class TimerOwner {
    public:
        TimerOwner(WTF::Function<void (void*)>&& callback, void* context)
            : m_timer(*this, &TimerOwner::timerFired)
            , m_callback(WTFMove(callback))
            , m_context(context)
        {
            m_timer.startOneShot(0_s);
        }
        
        void timerFired()
        {
            m_callback(m_context);
            delete this;
        }
        
        WebCore::Timer m_timer;
        WTF::Function<void (void*)> m_callback;
        void* m_context;
    };
    
    document->postTask([=] (WebCore::ScriptExecutionContext&) {
        new TimerOwner(callback, context); // deletes itself when done.
    });
}

void WKBundlePagePostMessage(WKBundlePageRef pageRef, WKStringRef messageNameRef, WKTypeRef messageBodyRef)
{
    toImpl(pageRef)->postMessage(toWTFString(messageNameRef), toImpl(messageBodyRef));
}

void WKBundlePagePostMessageIgnoringFullySynchronousMode(WKBundlePageRef pageRef, WKStringRef messageNameRef, WKTypeRef messageBodyRef)
{
    toImpl(pageRef)->postMessageIgnoringFullySynchronousMode(toWTFString(messageNameRef), toImpl(messageBodyRef));
}

void WKBundlePagePostSynchronousMessageForTesting(WKBundlePageRef pageRef, WKStringRef messageNameRef, WKTypeRef messageBodyRef, WKTypeRef* returnDataRef)
{
    WebPage* page = toImpl(pageRef);
    page->layoutIfNeeded();

    RefPtr<API::Object> returnData;
    page->postSynchronousMessageForTesting(toWTFString(messageNameRef), toImpl(messageBodyRef), returnData);
    if (returnDataRef)
        *returnDataRef = toAPI(returnData.leakRef());
}

void WKBundlePageAddUserScript(WKBundlePageRef pageRef, WKStringRef source, _WKUserScriptInjectionTime injectionTime, WKUserContentInjectedFrames injectedFrames)
{
    toImpl(pageRef)->addUserScript(toWTFString(source), toUserContentInjectedFrames(injectedFrames), toUserScriptInjectionTime(injectionTime));
}

void WKBundlePageAddUserStyleSheet(WKBundlePageRef pageRef, WKStringRef source, WKUserContentInjectedFrames injectedFrames)
{
    toImpl(pageRef)->addUserStyleSheet(toWTFString(source), toUserContentInjectedFrames(injectedFrames));
}

void WKBundlePageRemoveAllUserContent(WKBundlePageRef pageRef)
{
    toImpl(pageRef)->removeAllUserContent();
}

WKStringRef WKBundlePageCopyGroupIdentifier(WKBundlePageRef pageRef)
{
    return toCopiedAPI(toImpl(pageRef)->pageGroup()->identifier());
}

void WKBundlePageClearApplicationCache(WKBundlePageRef page)
{
    toImpl(page)->corePage()->applicationCacheStorage().deleteAllEntries();
}

void WKBundlePageClearApplicationCacheForOrigin(WKBundlePageRef page, WKStringRef origin)
{
    toImpl(page)->corePage()->applicationCacheStorage().deleteCacheForOrigin(WebCore::SecurityOrigin::createFromString(toImpl(origin)->string()));
}

void WKBundlePageSetAppCacheMaximumSize(WKBundlePageRef page, uint64_t size)
{
    toImpl(page)->corePage()->applicationCacheStorage().setMaximumSize(size);
}

uint64_t WKBundlePageGetAppCacheUsageForOrigin(WKBundlePageRef page, WKStringRef origin)
{
    return toImpl(page)->corePage()->applicationCacheStorage().diskUsageForOrigin(WebCore::SecurityOrigin::createFromString(toImpl(origin)->string()));
}

void WKBundlePageSetApplicationCacheOriginQuota(WKBundlePageRef page, WKStringRef origin, uint64_t bytes)
{
    toImpl(page)->corePage()->applicationCacheStorage().storeUpdatedQuotaForOrigin(WebCore::SecurityOrigin::createFromString(toImpl(origin)->string()).ptr(), bytes);
}

void WKBundlePageResetApplicationCacheOriginQuota(WKBundlePageRef page, WKStringRef origin)
{
    toImpl(page)->corePage()->applicationCacheStorage().storeUpdatedQuotaForOrigin(WebCore::SecurityOrigin::createFromString(toImpl(origin)->string()).ptr(), toImpl(page)->corePage()->applicationCacheStorage().defaultOriginQuota());
}

WKArrayRef WKBundlePageCopyOriginsWithApplicationCache(WKBundlePageRef page)
{
    auto origins = toImpl(page)->corePage()->applicationCacheStorage().originsWithCache();

    Vector<RefPtr<API::Object>> originIdentifiers;
    originIdentifiers.reserveInitialCapacity(origins.size());

    for (const auto& origin : origins)
        originIdentifiers.uncheckedAppend(API::String::create(origin->data().databaseIdentifier()));

    return toAPI(&API::Array::create(WTFMove(originIdentifiers)).leakRef());
}

void WKBundlePageSetEventThrottlingBehaviorOverride(WKBundlePageRef page, WKEventThrottlingBehavior* behavior)
{
    std::optional<WebCore::EventThrottlingBehavior> behaviorValue;
    if (behavior) {
        switch (*behavior) {
        case kWKEventThrottlingBehaviorResponsive:
            behaviorValue = WebCore::EventThrottlingBehavior::Responsive;
            break;
        case kWKEventThrottlingBehaviorUnresponsive:
            behaviorValue = WebCore::EventThrottlingBehavior::Unresponsive;
            break;
        }
    }

    toImpl(page)->corePage()->setEventThrottlingBehaviorOverride(behaviorValue);
}

void WKBundlePageSetCompositingPolicyOverride(WKBundlePageRef page, WKCompositingPolicy* policy)
{
    std::optional<WebCore::CompositingPolicy> policyValue;
    if (policy) {
        switch (*policy) {
        case kWKCompositingPolicyNormal:
            policyValue = WebCore::CompositingPolicy::Normal;
            break;
        case kWKCompositingPolicyConservative:
            policyValue = WebCore::CompositingPolicy::Conservative;
            break;
        }
    }

    toImpl(page)->corePage()->setCompositingPolicyOverride(policyValue);
}

