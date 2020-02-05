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
#include "APIDictionary.h"
#include "APIFrameHandle.h"
#include "APINumber.h"
#include "APIString.h"
#include "APIURL.h"
#include "APIURLRequest.h"
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
#include <WebCore/AccessibilityObjectInterface.h>
#include <WebCore/ApplicationCacheStorage.h>
#include <WebCore/CompositionHighlight.h>
#include <WebCore/FocusController.h>
#include <WebCore/Frame.h>
#include <WebCore/Page.h>
#include <WebCore/PageOverlay.h>
#include <WebCore/PageOverlayController.h>
#include <WebCore/RenderLayerCompositor.h>
#include <WebCore/ScriptExecutionContext.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/WheelEventTestMonitor.h>
#include <wtf/StdLibExtras.h>
#include <wtf/URL.h>

WKTypeID WKBundlePageGetTypeID()
{
    return WebKit::toAPI(WebKit::WebPage::APIType);
}

void WKBundlePageSetContextMenuClient(WKBundlePageRef pageRef, WKBundlePageContextMenuClientBase* wkClient)
{
#if ENABLE(CONTEXT_MENUS)
    WebKit::toImpl(pageRef)->setInjectedBundleContextMenuClient(makeUnique<WebKit::InjectedBundlePageContextMenuClient>(wkClient));
#else
    UNUSED_PARAM(pageRef);
    UNUSED_PARAM(wkClient);
#endif
}

void WKBundlePageSetEditorClient(WKBundlePageRef pageRef, WKBundlePageEditorClientBase* wkClient)
{
    WebKit::toImpl(pageRef)->setInjectedBundleEditorClient(wkClient ? makeUnique<WebKit::InjectedBundlePageEditorClient>(*wkClient) : makeUnique<API::InjectedBundle::EditorClient>());
}

void WKBundlePageSetFormClient(WKBundlePageRef pageRef, WKBundlePageFormClientBase* wkClient)
{
    WebKit::toImpl(pageRef)->setInjectedBundleFormClient(makeUnique<WebKit::InjectedBundlePageFormClient>(wkClient));
}

void WKBundlePageSetPageLoaderClient(WKBundlePageRef pageRef, WKBundlePageLoaderClientBase* wkClient)
{
    WebKit::toImpl(pageRef)->setInjectedBundlePageLoaderClient(makeUnique<WebKit::InjectedBundlePageLoaderClient>(wkClient));
}

void WKBundlePageSetResourceLoadClient(WKBundlePageRef pageRef, WKBundlePageResourceLoadClientBase* wkClient)
{
    WebKit::toImpl(pageRef)->setInjectedBundleResourceLoadClient(makeUnique<WebKit::InjectedBundlePageResourceLoadClient>(wkClient));
}

void WKBundlePageSetPolicyClient(WKBundlePageRef pageRef, WKBundlePagePolicyClientBase* wkClient)
{
    WebKit::toImpl(pageRef)->initializeInjectedBundlePolicyClient(wkClient);
}

void WKBundlePageSetUIClient(WKBundlePageRef pageRef, WKBundlePageUIClientBase* wkClient)
{
    WebKit::toImpl(pageRef)->setInjectedBundleUIClient(makeUnique<WebKit::InjectedBundlePageUIClient>(wkClient));
}

void WKBundlePageSetFullScreenClient(WKBundlePageRef pageRef, WKBundlePageFullScreenClientBase* wkClient)
{
#if defined(ENABLE_FULLSCREEN_API) && ENABLE_FULLSCREEN_API
    WebKit::toImpl(pageRef)->initializeInjectedBundleFullScreenClient(wkClient);
#else
    UNUSED_PARAM(pageRef);
    UNUSED_PARAM(wkClient);
#endif
}

void WKBundlePageWillEnterFullScreen(WKBundlePageRef pageRef)
{
#if defined(ENABLE_FULLSCREEN_API) && ENABLE_FULLSCREEN_API
    WebKit::toImpl(pageRef)->fullScreenManager()->willEnterFullScreen();
#else
    UNUSED_PARAM(pageRef);
#endif
}

void WKBundlePageDidEnterFullScreen(WKBundlePageRef pageRef)
{
#if defined(ENABLE_FULLSCREEN_API) && ENABLE_FULLSCREEN_API
    WebKit::toImpl(pageRef)->fullScreenManager()->didEnterFullScreen();
#else
    UNUSED_PARAM(pageRef);
#endif
}

void WKBundlePageWillExitFullScreen(WKBundlePageRef pageRef)
{
#if defined(ENABLE_FULLSCREEN_API) && ENABLE_FULLSCREEN_API
    WebKit::toImpl(pageRef)->fullScreenManager()->willExitFullScreen();
#else
    UNUSED_PARAM(pageRef);
#endif
}

void WKBundlePageDidExitFullScreen(WKBundlePageRef pageRef)
{
#if defined(ENABLE_FULLSCREEN_API) && ENABLE_FULLSCREEN_API
    WebKit::toImpl(pageRef)->fullScreenManager()->didExitFullScreen();
#else
    UNUSED_PARAM(pageRef);
#endif
}

WKBundlePageGroupRef WKBundlePageGetPageGroup(WKBundlePageRef pageRef)
{
    return toAPI(WebKit::toImpl(pageRef)->pageGroup());
}

WKBundleFrameRef WKBundlePageGetMainFrame(WKBundlePageRef pageRef)
{
    return toAPI(WebKit::toImpl(pageRef)->mainWebFrame());
}

WKFrameHandleRef WKBundleFrameCreateFrameHandle(WKBundleFrameRef bundleFrameRef)
{
    return WebKit::toAPI(&API::FrameHandle::create(WebKit::toImpl(bundleFrameRef)->frameID()).leakRef());
}

void WKBundlePageClickMenuItem(WKBundlePageRef pageRef, WKContextMenuItemRef item)
{
#if ENABLE(CONTEXT_MENUS)
    WebKit::toImpl(pageRef)->contextMenu()->itemSelected(WebKit::toImpl(item)->data());
#else
    UNUSED_PARAM(pageRef);
    UNUSED_PARAM(item);
#endif
}

#if ENABLE(CONTEXT_MENUS)
static Ref<API::Array> contextMenuItems(const WebKit::WebContextMenu& contextMenu)
{
    auto items = contextMenu.items();

    Vector<RefPtr<API::Object>> menuItems;
    menuItems.reserveInitialCapacity(items.size());

    for (const auto& item : items)
        menuItems.uncheckedAppend(WebKit::WebContextMenuItem::create(item));

    return API::Array::create(WTFMove(menuItems));
}
#endif

WKArrayRef WKBundlePageCopyContextMenuItems(WKBundlePageRef pageRef)
{
#if ENABLE(CONTEXT_MENUS)
    WebKit::WebContextMenu* contextMenu = WebKit::toImpl(pageRef)->contextMenu();

    return WebKit::toAPI(&contextMenuItems(*contextMenu).leakRef());
#else
    UNUSED_PARAM(pageRef);
    return nullptr;
#endif
}

WKArrayRef WKBundlePageCopyContextMenuAtPointInWindow(WKBundlePageRef pageRef, WKPoint point)
{
#if ENABLE(CONTEXT_MENUS)
    WebKit::WebContextMenu* contextMenu = WebKit::toImpl(pageRef)->contextMenuAtPointInWindow(WebKit::toIntPoint(point));
    if (!contextMenu)
        return nullptr;

    return WebKit::toAPI(&contextMenuItems(*contextMenu).leakRef());
#else
    UNUSED_PARAM(pageRef);
    UNUSED_PARAM(point);
    return nullptr;
#endif
}

void WKBundlePageInsertNewlineInQuotedContent(WKBundlePageRef pageRef)
{
    WebKit::toImpl(pageRef)->insertNewlineInQuotedContent();
}

void* WKAccessibilityRootObject(WKBundlePageRef pageRef)
{
#if ENABLE(ACCESSIBILITY)
    if (!pageRef)
        return 0;
    
    WebCore::Page* page = WebKit::toImpl(pageRef)->corePage();
    if (!page)
        return 0;
    
    WebCore::Frame& core = page->mainFrame();
    if (!core.document())
        return 0;
    
    WebCore::AXObjectCache::enableAccessibility();

    WebCore::AXCoreObject* root = core.document()->axObjectCache()->rootObject();
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
#if ENABLE(ACCESSIBILITY)
    if (!pageRef)
        return 0;
    
    WebCore::Page* page = WebKit::toImpl(pageRef)->corePage();
    if (!page)
        return 0;

    auto* focusedDocument = page->focusController().focusedOrMainFrame().document();
    if (!focusedDocument)
        return 0;

    WebCore::AXObjectCache::enableAccessibility();

    auto* axObjectCache = focusedDocument->axObjectCache();
    if (!axObjectCache)
        return 0;

    auto* focusedObject = axObjectCache->focusedUIElementForPage(page);
    if (!focusedObject)
        return 0;
    
    return focusedObject->wrapper();
#else
    UNUSED_PARAM(pageRef);
    return 0;
#endif
}

bool WKAccessibilityCanUseSecondaryAXThread(WKBundlePageRef pageRef)
{
#if ENABLE(ACCESSIBILITY)
    if (!pageRef)
        return false;

    WebCore::Page* page = WebKit::toImpl(pageRef)->corePage();
    if (!page)
        return false;

    WebCore::Frame& core = page->mainFrame();
    if (!core.document())
        return false;

    WebCore::AXObjectCache::enableAccessibility();

    auto* axObjectCache = core.document()->axObjectCache();
    if (!axObjectCache)
        return false;

    return axObjectCache->canUseSecondaryAXThread();
#else
    UNUSED_PARAM(pageRef);
    return false;
#endif
}

void WKAccessibilityEnableEnhancedAccessibility(bool enable)
{
#if ENABLE(ACCESSIBILITY)
    WebCore::AXObjectCache::setEnhancedUserInterfaceAccessibility(enable);
#endif
}

bool WKAccessibilityEnhancedAccessibilityEnabled()
{
#if ENABLE(ACCESSIBILITY)
    return WebCore::AXObjectCache::accessibilityEnhancedUserInterfaceEnabled();
#else
    return false;
#endif
}

void WKBundlePageStopLoading(WKBundlePageRef pageRef)
{
    WebKit::toImpl(pageRef)->stopLoading();
}

void WKBundlePageSetDefersLoading(WKBundlePageRef, bool)
{
}

WKStringRef WKBundlePageCopyRenderTreeExternalRepresentation(WKBundlePageRef pageRef, RenderTreeExternalRepresentationBehavior options)
{
    // Convert to webcore options.
    return WebKit::toCopiedAPI(WebKit::toImpl(pageRef)->renderTreeExternalRepresentation(options));
}

WKStringRef WKBundlePageCopyRenderTreeExternalRepresentationForPrinting(WKBundlePageRef pageRef)
{
    return WebKit::toCopiedAPI(WebKit::toImpl(pageRef)->renderTreeExternalRepresentationForPrinting());
}

void WKBundlePageExecuteEditingCommand(WKBundlePageRef pageRef, WKStringRef name, WKStringRef argument)
{
    WebKit::toImpl(pageRef)->executeEditingCommand(WebKit::toWTFString(name), WebKit::toWTFString(argument));
}

bool WKBundlePageIsEditingCommandEnabled(WKBundlePageRef pageRef, WKStringRef name)
{
    return WebKit::toImpl(pageRef)->isEditingCommandEnabled(WebKit::toWTFString(name));
}

void WKBundlePageClearMainFrameName(WKBundlePageRef pageRef)
{
    WebKit::toImpl(pageRef)->clearMainFrameName();
}

void WKBundlePageClose(WKBundlePageRef pageRef)
{
    WebKit::toImpl(pageRef)->sendClose();
}

double WKBundlePageGetTextZoomFactor(WKBundlePageRef pageRef)
{
    return WebKit::toImpl(pageRef)->textZoomFactor();
}

void WKBundlePageSetTextZoomFactor(WKBundlePageRef pageRef, double zoomFactor)
{
    WebKit::toImpl(pageRef)->setTextZoomFactor(zoomFactor);
}

double WKBundlePageGetPageZoomFactor(WKBundlePageRef pageRef)
{
    return WebKit::toImpl(pageRef)->pageZoomFactor();
}

void WKBundlePageSetPageZoomFactor(WKBundlePageRef pageRef, double zoomFactor)
{
    WebKit::toImpl(pageRef)->setPageZoomFactor(zoomFactor);
}

void WKBundlePageSetScaleAtOrigin(WKBundlePageRef pageRef, double scale, WKPoint origin)
{
    WebKit::toImpl(pageRef)->scalePage(scale, WebKit::toIntPoint(origin));
}

WKStringRef WKBundlePageDumpHistoryForTesting(WKBundlePageRef page, WKStringRef directory)
{
    return WebKit::toCopiedAPI(WebKit::toImpl(page)->dumpHistoryForTesting(WebKit::toWTFString(directory)));
}

void WKBundleClearHistoryForTesting(WKBundlePageRef page)
{
    WebKit::toImpl(page)->clearHistory();
}

WKBundleBackForwardListRef WKBundlePageGetBackForwardList(WKBundlePageRef pageRef)
{
    return nullptr;
}

void WKBundlePageInstallPageOverlay(WKBundlePageRef pageRef, WKBundlePageOverlayRef pageOverlayRef)
{
    WebKit::toImpl(pageRef)->corePage()->pageOverlayController().installPageOverlay(*WebKit::toImpl(pageOverlayRef)->coreOverlay(), WebCore::PageOverlay::FadeMode::DoNotFade);
}

void WKBundlePageUninstallPageOverlay(WKBundlePageRef pageRef, WKBundlePageOverlayRef pageOverlayRef)
{
    WebKit::toImpl(pageRef)->corePage()->pageOverlayController().uninstallPageOverlay(*WebKit::toImpl(pageOverlayRef)->coreOverlay(), WebCore::PageOverlay::FadeMode::DoNotFade);
}

void WKBundlePageInstallPageOverlayWithAnimation(WKBundlePageRef pageRef, WKBundlePageOverlayRef pageOverlayRef)
{
    WebKit::toImpl(pageRef)->corePage()->pageOverlayController().installPageOverlay(*WebKit::toImpl(pageOverlayRef)->coreOverlay(), WebCore::PageOverlay::FadeMode::Fade);
}

void WKBundlePageUninstallPageOverlayWithAnimation(WKBundlePageRef pageRef, WKBundlePageOverlayRef pageOverlayRef)
{
    WebKit::toImpl(pageRef)->corePage()->pageOverlayController().uninstallPageOverlay(*WebKit::toImpl(pageOverlayRef)->coreOverlay(), WebCore::PageOverlay::FadeMode::Fade);
}

void WKBundlePageSetTopOverhangImage(WKBundlePageRef pageRef, WKImageRef imageRef)
{
#if PLATFORM(MAC)
    WebKit::toImpl(pageRef)->setTopOverhangImage(WebKit::toImpl(imageRef));
#else
    UNUSED_PARAM(pageRef);
    UNUSED_PARAM(imageRef);
#endif
}

void WKBundlePageSetBottomOverhangImage(WKBundlePageRef pageRef, WKImageRef imageRef)
{
#if PLATFORM(MAC)
    WebKit::toImpl(pageRef)->setBottomOverhangImage(WebKit::toImpl(imageRef));
#else
    UNUSED_PARAM(pageRef);
    UNUSED_PARAM(imageRef);
#endif
}

#if !PLATFORM(IOS_FAMILY)
void WKBundlePageSetHeaderBanner(WKBundlePageRef pageRef, WKBundlePageBannerRef bannerRef)
{
    WebKit::toImpl(pageRef)->setHeaderPageBanner(WebKit::toImpl(bannerRef));
}

void WKBundlePageSetFooterBanner(WKBundlePageRef pageRef, WKBundlePageBannerRef bannerRef)
{
    WebKit::toImpl(pageRef)->setFooterPageBanner(WebKit::toImpl(bannerRef));
}
#endif // !PLATFORM(IOS_FAMILY)

bool WKBundlePageHasLocalDataForURL(WKBundlePageRef pageRef, WKURLRef urlRef)
{
    return WebKit::toImpl(pageRef)->hasLocalDataForURL(URL(URL(), WebKit::toWTFString(urlRef)));
}

bool WKBundlePageCanHandleRequest(WKURLRequestRef requestRef)
{
    if (!requestRef)
        return false;
    return WebKit::WebPage::canHandleRequest(WebKit::toImpl(requestRef)->resourceRequest());
}

bool WKBundlePageFindString(WKBundlePageRef pageRef, WKStringRef target, WKFindOptions findOptions)
{
    return WebKit::toImpl(pageRef)->findStringFromInjectedBundle(WebKit::toWTFString(target), WebKit::toFindOptions(findOptions));
}

void WKBundlePageFindStringMatches(WKBundlePageRef pageRef, WKStringRef target, WKFindOptions findOptions)
{
    WebKit::toImpl(pageRef)->findStringMatchesFromInjectedBundle(WebKit::toWTFString(target), WebKit::toFindOptions(findOptions));
}

void WKBundlePageReplaceStringMatches(WKBundlePageRef pageRef, WKArrayRef matchIndicesRef, WKStringRef replacementText, bool selectionOnly)
{
    auto* matchIndices = WebKit::toImpl(matchIndicesRef);

    Vector<uint32_t> indices;
    indices.reserveInitialCapacity(matchIndices->size());

    auto numberOfMatchIndices = matchIndices->size();
    for (size_t i = 0; i < numberOfMatchIndices; ++i) {
        if (auto* indexAsObject = matchIndices->at<API::UInt64>(i))
            indices.uncheckedAppend(indexAsObject->value());
    }
    WebKit::toImpl(pageRef)->replaceStringMatchesFromInjectedBundle(indices, WebKit::toWTFString(replacementText), selectionOnly);
}

WKImageRef WKBundlePageCreateSnapshotWithOptions(WKBundlePageRef pageRef, WKRect rect, WKSnapshotOptions options)
{
    RefPtr<WebKit::WebImage> webImage = WebKit::toImpl(pageRef)->scaledSnapshotWithOptions(WebKit::toIntRect(rect), 1, WebKit::toSnapshotOptions(options));
    return toAPI(webImage.leakRef());
}

WKImageRef WKBundlePageCreateSnapshotInViewCoordinates(WKBundlePageRef pageRef, WKRect rect, WKImageOptions options)
{
    auto snapshotOptions = WebKit::snapshotOptionsFromImageOptions(options);
    snapshotOptions |= WebKit::SnapshotOptionsInViewCoordinates;
    RefPtr<WebKit::WebImage> webImage = WebKit::toImpl(pageRef)->scaledSnapshotWithOptions(WebKit::toIntRect(rect), 1, snapshotOptions);
    return toAPI(webImage.leakRef());
}

WKImageRef WKBundlePageCreateSnapshotInDocumentCoordinates(WKBundlePageRef pageRef, WKRect rect, WKImageOptions options)
{
    RefPtr<WebKit::WebImage> webImage = WebKit::toImpl(pageRef)->scaledSnapshotWithOptions(WebKit::toIntRect(rect), 1, WebKit::snapshotOptionsFromImageOptions(options));
    return toAPI(webImage.leakRef());
}

WKImageRef WKBundlePageCreateScaledSnapshotInDocumentCoordinates(WKBundlePageRef pageRef, WKRect rect, double scaleFactor, WKImageOptions options)
{
    RefPtr<WebKit::WebImage> webImage = WebKit::toImpl(pageRef)->scaledSnapshotWithOptions(WebKit::toIntRect(rect), scaleFactor, WebKit::snapshotOptionsFromImageOptions(options));
    return toAPI(webImage.leakRef());
}

double WKBundlePageGetBackingScaleFactor(WKBundlePageRef pageRef)
{
    return WebKit::toImpl(pageRef)->deviceScaleFactor();
}

void WKBundlePageListenForLayoutMilestones(WKBundlePageRef pageRef, WKLayoutMilestones milestones)
{
    WebKit::toImpl(pageRef)->listenForLayoutMilestones(WebKit::toLayoutMilestones(milestones));
}

WKBundleInspectorRef WKBundlePageGetInspector(WKBundlePageRef pageRef)
{
    return WebKit::toAPI(WebKit::toImpl(pageRef)->inspector());
}

void WKBundlePageForceRepaint(WKBundlePageRef page)
{
    WebKit::toImpl(page)->forceRepaintWithoutCallback();
}

void WKBundlePageFlushPendingEditorStateUpdate(WKBundlePageRef page)
{
    WebKit::toImpl(page)->flushPendingEditorStateUpdate();
}

void WKBundlePageSimulateMouseDown(WKBundlePageRef page, int button, WKPoint position, int clickCount, WKEventModifiers modifiers, double time)
{
    WebKit::toImpl(page)->simulateMouseDown(button, WebKit::toIntPoint(position), clickCount, modifiers, WallTime::fromRawSeconds(time));
}

void WKBundlePageSimulateMouseUp(WKBundlePageRef page, int button, WKPoint position, int clickCount, WKEventModifiers modifiers, double time)
{
    WebKit::toImpl(page)->simulateMouseUp(button, WebKit::toIntPoint(position), clickCount, modifiers, WallTime::fromRawSeconds(time));
}

void WKBundlePageSimulateMouseMotion(WKBundlePageRef page, WKPoint position, double time)
{
    WebKit::toImpl(page)->simulateMouseMotion(WebKit::toIntPoint(position), WallTime::fromRawSeconds(time));
}

uint64_t WKBundlePageGetRenderTreeSize(WKBundlePageRef pageRef)
{
    return WebKit::toImpl(pageRef)->renderTreeSize();
}

WKRenderObjectRef WKBundlePageCopyRenderTree(WKBundlePageRef pageRef)
{
    return WebKit::toAPI(WebKit::WebRenderObject::create(WebKit::toImpl(pageRef)).leakRef());
}

WKRenderLayerRef WKBundlePageCopyRenderLayerTree(WKBundlePageRef pageRef)
{
    return WebKit::toAPI(WebKit::WebRenderLayer::create(WebKit::toImpl(pageRef)).leakRef());
}

void WKBundlePageSetPaintedObjectsCounterThreshold(WKBundlePageRef, uint64_t)
{
    // FIXME: This function is only still here to keep open source Mac builds building.
    // We should remove it as soon as we can.
}

void WKBundlePageSetTracksRepaints(WKBundlePageRef pageRef, bool trackRepaints)
{
    WebKit::toImpl(pageRef)->setTracksRepaints(trackRepaints);
}

bool WKBundlePageIsTrackingRepaints(WKBundlePageRef pageRef)
{
    return WebKit::toImpl(pageRef)->isTrackingRepaints();
}

void WKBundlePageResetTrackedRepaints(WKBundlePageRef pageRef)
{
    WebKit::toImpl(pageRef)->resetTrackedRepaints();
}

WKArrayRef WKBundlePageCopyTrackedRepaintRects(WKBundlePageRef pageRef)
{
    return WebKit::toAPI(&WebKit::toImpl(pageRef)->trackedRepaintRects().leakRef());
}

void WKBundlePageSetComposition(WKBundlePageRef pageRef, WKStringRef text, int from, int length, bool suppressUnderline, WKArrayRef highlightData)
{
    Vector<WebCore::CompositionHighlight> highlights;
    if (highlightData) {
        auto* highlightDataArray = WebKit::toImpl(highlightData);
        highlights.reserveInitialCapacity(highlightDataArray->size());
        for (auto dictionary : highlightDataArray->elementsOfType<API::Dictionary>()) {
            auto startOffset = static_cast<API::UInt64*>(dictionary->get("from"))->value();
            highlights.uncheckedAppend({
                static_cast<unsigned>(startOffset),
                static_cast<unsigned>(startOffset + static_cast<API::UInt64*>(dictionary->get("length"))->value()),
                WebCore::Color(static_cast<API::String*>(dictionary->get("color"))->string())
            });
        }
    }
    WebKit::toImpl(pageRef)->setCompositionForTesting(WebKit::toWTFString(text), from, length, suppressUnderline, highlights);
}

bool WKBundlePageHasComposition(WKBundlePageRef pageRef)
{
    return WebKit::toImpl(pageRef)->hasCompositionForTesting();
}

void WKBundlePageConfirmComposition(WKBundlePageRef pageRef)
{
    WebKit::toImpl(pageRef)->confirmCompositionForTesting(String());
}

void WKBundlePageConfirmCompositionWithText(WKBundlePageRef pageRef, WKStringRef text)
{
    WebKit::toImpl(pageRef)->confirmCompositionForTesting(WebKit::toWTFString(text));
}

void WKBundlePageSetUseDarkAppearance(WKBundlePageRef pageRef, bool useDarkAppearance)
{
    WebKit::WebPage* webPage = WebKit::toImpl(pageRef);
    if (WebCore::Page* page = webPage ? webPage->corePage() : nullptr)
        page->effectiveAppearanceDidChange(useDarkAppearance, page->useElevatedUserInterfaceLevel());
}

bool WKBundlePageIsUsingDarkAppearance(WKBundlePageRef pageRef)
{
    WebKit::WebPage* webPage = WebKit::toImpl(pageRef);
    if (WebCore::Page* page = webPage ? webPage->corePage() : nullptr)
        return page->useDarkAppearance();
    return false;
}

bool WKBundlePageCanShowMIMEType(WKBundlePageRef pageRef, WKStringRef mimeTypeRef)
{
    return WebKit::toImpl(pageRef)->canShowMIMEType(WebKit::toWTFString(mimeTypeRef));
}

WKRenderingSuppressionToken WKBundlePageExtendIncrementalRenderingSuppression(WKBundlePageRef pageRef)
{
    return WebKit::toImpl(pageRef)->extendIncrementalRenderingSuppression();
}

void WKBundlePageStopExtendingIncrementalRenderingSuppression(WKBundlePageRef pageRef, WKRenderingSuppressionToken token)
{
    WebKit::toImpl(pageRef)->stopExtendingIncrementalRenderingSuppression(token);
}

bool WKBundlePageIsUsingEphemeralSession(WKBundlePageRef pageRef)
{
    return WebKit::toImpl(pageRef)->usesEphemeralSession();
}

bool WKBundlePageIsControlledByAutomation(WKBundlePageRef pageRef)
{
    return WebKit::toImpl(pageRef)->isControlledByAutomation();
}

#if TARGET_OS_IPHONE
void WKBundlePageSetUseTestingViewportConfiguration(WKBundlePageRef pageRef, bool useTestingViewportConfiguration)
{
    WebKit::toImpl(pageRef)->setUseTestingViewportConfiguration(useTestingViewportConfiguration);
}
#endif

void WKBundlePageStartMonitoringScrollOperations(WKBundlePageRef pageRef)
{
    WebKit::WebPage* webPage = WebKit::toImpl(pageRef);
    WebCore::Page* page = webPage ? webPage->corePage() : nullptr;
    
    if (!page)
        return;

    page->ensureWheelEventTestMonitor();
}

bool WKBundlePageRegisterScrollOperationCompletionCallback(WKBundlePageRef pageRef, WKBundlePageTestNotificationCallback callback, void* context)
{
    if (!callback)
        return false;
    
    WebKit::WebPage* webPage = WebKit::toImpl(pageRef);
    WebCore::Page* page = webPage ? webPage->corePage() : nullptr;
    if (!page || !page->isMonitoringWheelEvents())
        return false;
    
    page->ensureWheelEventTestMonitor().setTestCallbackAndStartNotificationTimer([=]() {
        callback(context);
    });
    return true;
}

void WKBundlePageCallAfterTasksAndTimers(WKBundlePageRef pageRef, WKBundlePageTestNotificationCallback callback, void* context)
{
    if (!callback)
        return;
    
    WebKit::WebPage* webPage = WebKit::toImpl(pageRef);
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
    WebKit::toImpl(pageRef)->postMessage(WebKit::toWTFString(messageNameRef), WebKit::toImpl(messageBodyRef));
}

void WKBundlePagePostMessageIgnoringFullySynchronousMode(WKBundlePageRef pageRef, WKStringRef messageNameRef, WKTypeRef messageBodyRef)
{
    WebKit::toImpl(pageRef)->postMessageIgnoringFullySynchronousMode(WebKit::toWTFString(messageNameRef), WebKit::toImpl(messageBodyRef));
}

void WKBundlePagePostSynchronousMessageForTesting(WKBundlePageRef pageRef, WKStringRef messageNameRef, WKTypeRef messageBodyRef, WKTypeRef* returnRetainedDataRef)
{
    WebKit::WebPage* page = WebKit::toImpl(pageRef);
    page->layoutIfNeeded();

    RefPtr<API::Object> returnData;
    page->postSynchronousMessageForTesting(WebKit::toWTFString(messageNameRef), WebKit::toImpl(messageBodyRef), returnData);
    if (returnRetainedDataRef)
        *returnRetainedDataRef = WebKit::toAPI(returnData.leakRef());
}

bool WKBundlePageIsSuspended(WKBundlePageRef pageRef)
{
    return WebKit::toImpl(pageRef)->isSuspended();
}

void WKBundlePageAddUserScript(WKBundlePageRef pageRef, WKStringRef source, _WKUserScriptInjectionTime injectionTime, WKUserContentInjectedFrames injectedFrames)
{
    WebKit::toImpl(pageRef)->addUserScript(WebKit::toWTFString(source), WebKit::toUserContentInjectedFrames(injectedFrames), WebKit::toUserScriptInjectionTime(injectionTime));
}

void WKBundlePageAddUserStyleSheet(WKBundlePageRef pageRef, WKStringRef source, WKUserContentInjectedFrames injectedFrames)
{
    WebKit::toImpl(pageRef)->addUserStyleSheet(WebKit::toWTFString(source), WebKit::toUserContentInjectedFrames(injectedFrames));
}

void WKBundlePageRemoveAllUserContent(WKBundlePageRef pageRef)
{
    WebKit::toImpl(pageRef)->removeAllUserContent();
}

WKStringRef WKBundlePageCopyGroupIdentifier(WKBundlePageRef pageRef)
{
    return WebKit::toCopiedAPI(WebKit::toImpl(pageRef)->pageGroup()->identifier());
}

void WKBundlePageClearApplicationCache(WKBundlePageRef page)
{
    WebKit::toImpl(page)->corePage()->applicationCacheStorage().deleteAllEntries();
}

void WKBundlePageClearApplicationCacheForOrigin(WKBundlePageRef page, WKStringRef origin)
{
    WebKit::toImpl(page)->corePage()->applicationCacheStorage().deleteCacheForOrigin(WebCore::SecurityOrigin::createFromString(WebKit::toImpl(origin)->string()));
}

void WKBundlePageSetAppCacheMaximumSize(WKBundlePageRef page, uint64_t size)
{
    WebKit::toImpl(page)->corePage()->applicationCacheStorage().setMaximumSize(size);
}

uint64_t WKBundlePageGetAppCacheUsageForOrigin(WKBundlePageRef page, WKStringRef origin)
{
    return WebKit::toImpl(page)->corePage()->applicationCacheStorage().diskUsageForOrigin(WebCore::SecurityOrigin::createFromString(WebKit::toImpl(origin)->string()));
}

void WKBundlePageSetApplicationCacheOriginQuota(WKBundlePageRef page, WKStringRef origin, uint64_t bytes)
{
    WebKit::toImpl(page)->corePage()->applicationCacheStorage().storeUpdatedQuotaForOrigin(WebCore::SecurityOrigin::createFromString(WebKit::toImpl(origin)->string()).ptr(), bytes);
}

void WKBundlePageResetApplicationCacheOriginQuota(WKBundlePageRef page, WKStringRef origin)
{
    WebKit::toImpl(page)->corePage()->applicationCacheStorage().storeUpdatedQuotaForOrigin(WebCore::SecurityOrigin::createFromString(WebKit::toImpl(origin)->string()).ptr(), WebKit::toImpl(page)->corePage()->applicationCacheStorage().defaultOriginQuota());
}

WKArrayRef WKBundlePageCopyOriginsWithApplicationCache(WKBundlePageRef page)
{
    auto origins = WebKit::toImpl(page)->corePage()->applicationCacheStorage().originsWithCache();

    Vector<RefPtr<API::Object>> originIdentifiers;
    originIdentifiers.reserveInitialCapacity(origins.size());

    for (const auto& origin : origins)
        originIdentifiers.uncheckedAppend(API::String::create(origin->data().databaseIdentifier()));

    return WebKit::toAPI(&API::Array::create(WTFMove(originIdentifiers)).leakRef());
}

void WKBundlePageSetEventThrottlingBehaviorOverride(WKBundlePageRef page, WKEventThrottlingBehavior* behavior)
{
    Optional<WebCore::EventThrottlingBehavior> behaviorValue;
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

    WebKit::toImpl(page)->corePage()->setEventThrottlingBehaviorOverride(behaviorValue);
}
