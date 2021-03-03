/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
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
#include "WKPage.h"
#include "WKPagePrivate.h"

#include "APIArray.h"
#include "APIContextMenuClient.h"
#include "APIData.h"
#include "APIDictionary.h"
#include "APIFindClient.h"
#include "APIFindMatchesClient.h"
#include "APIFrameHandle.h"
#include "APIFrameInfo.h"
#include "APIGeometry.h"
#include "APIHitTestResult.h"
#include "APILoaderClient.h"
#include "APINavigation.h"
#include "APINavigationAction.h"
#include "APINavigationClient.h"
#include "APINavigationResponse.h"
#include "APIOpenPanelParameters.h"
#include "APIPageConfiguration.h"
#include "APIPolicyClient.h"
#include "APISessionState.h"
#include "APIUIClient.h"
#include "APIWebAuthenticationPanel.h"
#include "APIWebAuthenticationPanelClient.h"
#include "APIWebsitePolicies.h"
#include "APIWindowFeatures.h"
#include "AuthenticationChallengeDisposition.h"
#include "AuthenticationChallengeProxy.h"
#include "AuthenticationDecisionListener.h"
#include "ContentAsStringIncludesChildFrames.h"
#include "DownloadProxy.h"
#include "LegacySessionStateCoding.h"
#include "Logging.h"
#include "MediaKeySystemPermissionRequest.h"
#include "NativeWebKeyboardEvent.h"
#include "NativeWebWheelEvent.h"
#include "NavigationActionData.h"
#include "PageClient.h"
#include "PluginInformation.h"
#include "PrintInfo.h"
#include "SpeechRecognitionPermissionRequest.h"
#include "WKAPICast.h"
#include "WKPagePolicyClientInternal.h"
#include "WKPageRenderingProgressEventsInternal.h"
#include "WKPluginInformation.h"
#include "WebBackForwardList.h"
#include "WebFormClient.h"
#include "WebImage.h"
#include "WebInspectorProxy.h"
#include "WebOpenPanelResultListenerProxy.h"
#include "WebPageGroup.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include "WebProtectionSpace.h"
#include <WebCore/ContentRuleListResults.h>
#include <WebCore/MockRealtimeMediaSourceCenter.h>
#include <WebCore/Page.h>
#include <WebCore/SSLKeyGenerator.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/SerializedCryptoKeyWrap.h>
#include <WebCore/WindowFeatures.h>

#ifdef __BLOCKS__
#include <Block.h>
#endif

#if ENABLE(CONTEXT_MENUS)
#include "WebContextMenuItem.h"
#endif

#if PLATFORM(COCOA)
#include <WebCore/VersionChecks.h>
#endif

namespace API {
using namespace WebCore;
using namespace WebKit;
    
template<> struct ClientTraits<WKPageLoaderClientBase> {
    typedef std::tuple<WKPageLoaderClientV0, WKPageLoaderClientV1, WKPageLoaderClientV2, WKPageLoaderClientV3, WKPageLoaderClientV4, WKPageLoaderClientV5, WKPageLoaderClientV6> Versions;
};

template<> struct ClientTraits<WKPageNavigationClientBase> {
    typedef std::tuple<WKPageNavigationClientV0, WKPageNavigationClientV1, WKPageNavigationClientV2, WKPageNavigationClientV3> Versions;
};

template<> struct ClientTraits<WKPagePolicyClientBase> {
    typedef std::tuple<WKPagePolicyClientV0, WKPagePolicyClientV1, WKPagePolicyClientInternal> Versions;
};

template<> struct ClientTraits<WKPageUIClientBase> {
    typedef std::tuple<WKPageUIClientV0, WKPageUIClientV1, WKPageUIClientV2, WKPageUIClientV3, WKPageUIClientV4, WKPageUIClientV5, WKPageUIClientV6, WKPageUIClientV7, WKPageUIClientV8, WKPageUIClientV9, WKPageUIClientV10, WKPageUIClientV11, WKPageUIClientV12, WKPageUIClientV13, WKPageUIClientV14, WKPageUIClientV15, WKPageUIClientV16> Versions;
};

#if ENABLE(CONTEXT_MENUS)
template<> struct ClientTraits<WKPageContextMenuClientBase> {
    typedef std::tuple<WKPageContextMenuClientV0, WKPageContextMenuClientV1, WKPageContextMenuClientV2, WKPageContextMenuClientV3, WKPageContextMenuClientV4> Versions;
};
#endif

template<> struct ClientTraits<WKPageFindClientBase> {
    typedef std::tuple<WKPageFindClientV0> Versions;
};

template<> struct ClientTraits<WKPageFindMatchesClientBase> {
    typedef std::tuple<WKPageFindMatchesClientV0> Versions;
};

template<> struct ClientTraits<WKPageStateClientBase> {
    typedef std::tuple<WKPageStateClientV0> Versions;
};
    
} // namespace API

using namespace WebKit;

WKTypeID WKPageGetTypeID()
{
    return toAPI(WebPageProxy::APIType);
}

WKContextRef WKPageGetContext(WKPageRef pageRef)
{
    return toAPI(&toImpl(pageRef)->process().processPool());
}

WKPageGroupRef WKPageGetPageGroup(WKPageRef pageRef)
{
    return toAPI(&toImpl(pageRef)->pageGroup());
}

WKPageConfigurationRef WKPageCopyPageConfiguration(WKPageRef pageRef)
{
    return toAPI(&toImpl(pageRef)->configuration().copy().leakRef());
}

void WKPageLoadURL(WKPageRef pageRef, WKURLRef URLRef)
{
    toImpl(pageRef)->loadRequest(URL(URL(), toWTFString(URLRef)));
}

void WKPageLoadURLWithShouldOpenExternalURLsPolicy(WKPageRef pageRef, WKURLRef URLRef, bool shouldOpenExternalURLs)
{
    WebCore::ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy = shouldOpenExternalURLs ? WebCore::ShouldOpenExternalURLsPolicy::ShouldAllow : WebCore::ShouldOpenExternalURLsPolicy::ShouldNotAllow;
    toImpl(pageRef)->loadRequest(URL(URL(), toWTFString(URLRef)), shouldOpenExternalURLsPolicy);
}

void WKPageLoadURLWithUserData(WKPageRef pageRef, WKURLRef URLRef, WKTypeRef userDataRef)
{
    toImpl(pageRef)->loadRequest(URL(URL(), toWTFString(URLRef)), WebCore::ShouldOpenExternalURLsPolicy::ShouldNotAllow, toImpl(userDataRef));
}

void WKPageLoadURLRequest(WKPageRef pageRef, WKURLRequestRef urlRequestRef)
{
    auto resourceRequest = toImpl(urlRequestRef)->resourceRequest();
    toImpl(pageRef)->loadRequest(WTFMove(resourceRequest));
}

void WKPageLoadURLRequestWithUserData(WKPageRef pageRef, WKURLRequestRef urlRequestRef, WKTypeRef userDataRef)
{
    auto resourceRequest = toImpl(urlRequestRef)->resourceRequest();
    toImpl(pageRef)->loadRequest(WTFMove(resourceRequest), WebCore::ShouldOpenExternalURLsPolicy::ShouldNotAllow, toImpl(userDataRef));
}

void WKPageLoadFile(WKPageRef pageRef, WKURLRef fileURL, WKURLRef resourceDirectoryURL)
{
    toImpl(pageRef)->loadFile(toWTFString(fileURL), toWTFString(resourceDirectoryURL));
}

void WKPageLoadFileWithUserData(WKPageRef pageRef, WKURLRef fileURL, WKURLRef resourceDirectoryURL, WKTypeRef userDataRef)
{
    toImpl(pageRef)->loadFile(toWTFString(fileURL), toWTFString(resourceDirectoryURL), toImpl(userDataRef));
}

void WKPageLoadData(WKPageRef pageRef, WKDataRef dataRef, WKStringRef MIMETypeRef, WKStringRef encodingRef, WKURLRef baseURLRef)
{
    toImpl(pageRef)->loadData(toImpl(dataRef)->dataReference(), toWTFString(MIMETypeRef), toWTFString(encodingRef), toWTFString(baseURLRef));
}

void WKPageLoadDataWithUserData(WKPageRef pageRef, WKDataRef dataRef, WKStringRef MIMETypeRef, WKStringRef encodingRef, WKURLRef baseURLRef, WKTypeRef userDataRef)
{
    toImpl(pageRef)->loadData(toImpl(dataRef)->dataReference(), toWTFString(MIMETypeRef), toWTFString(encodingRef), toWTFString(baseURLRef), toImpl(userDataRef));
}

static String encodingOf(const String& string)
{
    if (string.isNull() || !string.is8Bit())
        return "utf-16"_s;
    return "latin1"_s;
}

static IPC::DataReference dataFrom(const String& string)
{
    if (string.isNull() || !string.is8Bit())
        return { reinterpret_cast<const uint8_t*>(string.characters16()), string.length() * sizeof(UChar) };
    return { reinterpret_cast<const uint8_t*>(string.characters8()), string.length() * sizeof(LChar) };
}

static void loadString(WKPageRef pageRef, WKStringRef stringRef, const String& mimeType, const String& baseURL, WKTypeRef userDataRef)
{
    String string = toWTFString(stringRef);
    toImpl(pageRef)->loadData(dataFrom(string), mimeType, encodingOf(string), baseURL, toImpl(userDataRef));
}

void WKPageLoadHTMLString(WKPageRef pageRef, WKStringRef htmlStringRef, WKURLRef baseURLRef)
{
    WKPageLoadHTMLStringWithUserData(pageRef, htmlStringRef, baseURLRef, nullptr);
}

void WKPageLoadHTMLStringWithUserData(WKPageRef pageRef, WKStringRef htmlStringRef, WKURLRef baseURLRef, WKTypeRef userDataRef)
{
    loadString(pageRef, htmlStringRef, "text/html"_s, toWTFString(baseURLRef), userDataRef);
}

void WKPageLoadAlternateHTMLString(WKPageRef pageRef, WKStringRef htmlStringRef, WKURLRef baseURLRef, WKURLRef unreachableURLRef)
{
    WKPageLoadAlternateHTMLStringWithUserData(pageRef, htmlStringRef, baseURLRef, unreachableURLRef, nullptr);
}

void WKPageLoadAlternateHTMLStringWithUserData(WKPageRef pageRef, WKStringRef htmlStringRef, WKURLRef baseURLRef, WKURLRef unreachableURLRef, WKTypeRef userDataRef)
{
    String string = toWTFString(htmlStringRef);
    toImpl(pageRef)->loadAlternateHTML(dataFrom(string), encodingOf(string), URL(URL(), toWTFString(baseURLRef)), URL(URL(), toWTFString(unreachableURLRef)), toImpl(userDataRef));
}

void WKPageLoadPlainTextString(WKPageRef pageRef, WKStringRef plainTextStringRef)
{
    WKPageLoadPlainTextStringWithUserData(pageRef, plainTextStringRef, nullptr);
}

void WKPageLoadPlainTextStringWithUserData(WKPageRef pageRef, WKStringRef plainTextStringRef, WKTypeRef userDataRef)
{
    loadString(pageRef, plainTextStringRef, "text/plain"_s, aboutBlankURL().string(), userDataRef);
}

void WKPageLoadWebArchiveData(WKPageRef pageRef, WKDataRef webArchiveDataRef)
{
    toImpl(pageRef)->loadWebArchiveData(toImpl(webArchiveDataRef));
}

void WKPageLoadWebArchiveDataWithUserData(WKPageRef pageRef, WKDataRef webArchiveDataRef, WKTypeRef userDataRef)
{
    toImpl(pageRef)->loadWebArchiveData(toImpl(webArchiveDataRef), toImpl(userDataRef));
}

void WKPageStopLoading(WKPageRef pageRef)
{
    toImpl(pageRef)->stopLoading();
}

void WKPageReload(WKPageRef pageRef)
{
    OptionSet<WebCore::ReloadOption> reloadOptions;
#if PLATFORM(COCOA)
    if (linkedOnOrAfter(WebCore::SDKVersion::FirstWithExpiredOnlyReloadBehavior))
        reloadOptions.add(WebCore::ReloadOption::ExpiredOnly);
#endif

    toImpl(pageRef)->reload(reloadOptions);
}

void WKPageReloadWithoutContentBlockers(WKPageRef pageRef)
{
    toImpl(pageRef)->reload(WebCore::ReloadOption::DisableContentBlockers);
}

void WKPageReloadFromOrigin(WKPageRef pageRef)
{
    toImpl(pageRef)->reload(WebCore::ReloadOption::FromOrigin);
}

void WKPageReloadExpiredOnly(WKPageRef pageRef)
{
    toImpl(pageRef)->reload(WebCore::ReloadOption::ExpiredOnly);
}

bool WKPageTryClose(WKPageRef pageRef)
{
    return toImpl(pageRef)->tryClose();
}

void WKPageClose(WKPageRef pageRef)
{
    toImpl(pageRef)->close();
}

bool WKPageIsClosed(WKPageRef pageRef)
{
    return toImpl(pageRef)->isClosed();
}

void WKPageGoForward(WKPageRef pageRef)
{
    toImpl(pageRef)->goForward();
}

bool WKPageCanGoForward(WKPageRef pageRef)
{
    return toImpl(pageRef)->backForwardList().forwardItem();
}

void WKPageGoBack(WKPageRef pageRef)
{
    auto& page = *toImpl(pageRef);
    if (page.pageClient().hasSafeBrowsingWarning()) {
        WKPageReload(pageRef);
        return;
    }
    page.goBack();
}

bool WKPageCanGoBack(WKPageRef pageRef)
{
    return toImpl(pageRef)->backForwardList().backItem();
}

void WKPageGoToBackForwardListItem(WKPageRef pageRef, WKBackForwardListItemRef itemRef)
{
    toImpl(pageRef)->goToBackForwardItem(*toImpl(itemRef));
}

void WKPageTryRestoreScrollPosition(WKPageRef pageRef)
{
    toImpl(pageRef)->tryRestoreScrollPosition();
}

WKBackForwardListRef WKPageGetBackForwardList(WKPageRef pageRef)
{
    return toAPI(&toImpl(pageRef)->backForwardList());
}

bool WKPageWillHandleHorizontalScrollEvents(WKPageRef pageRef)
{
    return toImpl(pageRef)->willHandleHorizontalScrollEvents();
}

void WKPageUpdateWebsitePolicies(WKPageRef pageRef, WKWebsitePoliciesRef websitePoliciesRef)
{
    RELEASE_ASSERT_WITH_MESSAGE(!toImpl(websitePoliciesRef)->websiteDataStore(), "Setting WebsitePolicies.websiteDataStore is only supported during WKFramePolicyListenerUseWithPolicies().");
    RELEASE_ASSERT_WITH_MESSAGE(!toImpl(websitePoliciesRef)->userContentController(), "Setting WebsitePolicies.userContentController is only supported during WKFramePolicyListenerUseWithPolicies().");
    auto data = toImpl(websitePoliciesRef)->data();
    toImpl(pageRef)->updateWebsitePolicies(WTFMove(data));
}

WKStringRef WKPageCopyTitle(WKPageRef pageRef)
{
    return toCopiedAPI(toImpl(pageRef)->pageLoadState().title());
}

WKFrameRef WKPageGetMainFrame(WKPageRef pageRef)
{
    return toAPI(toImpl(pageRef)->mainFrame());
}

WKFrameRef WKPageGetFocusedFrame(WKPageRef pageRef)
{
    return toAPI(toImpl(pageRef)->focusedFrame());
}

WKFrameRef WKPageGetFrameSetLargestFrame(WKPageRef pageRef)
{
    return nullptr;
}

uint64_t WKPageGetRenderTreeSize(WKPageRef page)
{
    return toImpl(page)->renderTreeSize();
}

WKWebsiteDataStoreRef WKPageGetWebsiteDataStore(WKPageRef page)
{
    return toAPI(&toImpl(page)->websiteDataStore());
}

WKInspectorRef WKPageGetInspector(WKPageRef pageRef)
{
    return toAPI(toImpl(pageRef)->inspector());
}

double WKPageGetEstimatedProgress(WKPageRef pageRef)
{
    return toImpl(pageRef)->estimatedProgress();
}

WKStringRef WKPageCopyUserAgent(WKPageRef pageRef)
{
    return toCopiedAPI(toImpl(pageRef)->userAgent());
}

WKStringRef WKPageCopyApplicationNameForUserAgent(WKPageRef pageRef)
{
    return toCopiedAPI(toImpl(pageRef)->applicationNameForUserAgent());
}

void WKPageSetApplicationNameForUserAgent(WKPageRef pageRef, WKStringRef applicationNameRef)
{
    toImpl(pageRef)->setApplicationNameForUserAgent(toWTFString(applicationNameRef));
}

WKStringRef WKPageCopyCustomUserAgent(WKPageRef pageRef)
{
    return toCopiedAPI(toImpl(pageRef)->customUserAgent());
}

void WKPageSetCustomUserAgent(WKPageRef pageRef, WKStringRef userAgentRef)
{
    toImpl(pageRef)->setCustomUserAgent(toWTFString(userAgentRef));
}

void WKPageSetUserContentExtensionsEnabled(WKPageRef pageRef, bool enabled)
{
    // FIXME: Remove this function once it is no longer used.
}

bool WKPageSupportsTextEncoding(WKPageRef pageRef)
{
    return toImpl(pageRef)->supportsTextEncoding();
}

WKStringRef WKPageCopyCustomTextEncodingName(WKPageRef pageRef)
{
    return toCopiedAPI(toImpl(pageRef)->customTextEncodingName());
}

void WKPageSetCustomTextEncodingName(WKPageRef pageRef, WKStringRef encodingNameRef)
{
    toImpl(pageRef)->setCustomTextEncodingName(toWTFString(encodingNameRef));
}

void WKPageTerminate(WKPageRef pageRef)
{
    Ref<WebProcessProxy> protectedProcessProxy(toImpl(pageRef)->process());
    protectedProcessProxy->requestTermination(ProcessTerminationReason::RequestedByClient);
}

WKStringRef WKPageGetSessionHistoryURLValueType()
{
    static API::String& sessionHistoryURLValueType = API::String::create("SessionHistoryURL").leakRef();
    return toAPI(&sessionHistoryURLValueType);
}

WKStringRef WKPageGetSessionBackForwardListItemValueType()
{
    static API::String& sessionBackForwardListValueType = API::String::create("SessionBackForwardListItem").leakRef();
    return toAPI(&sessionBackForwardListValueType);
}

WKTypeRef WKPageCopySessionState(WKPageRef pageRef, void* context, WKPageSessionStateFilterCallback filter)
{
    // FIXME: This is a hack to make sure we return a WKDataRef to maintain compatibility with older versions of Safari.
    bool shouldReturnData = !(reinterpret_cast<uintptr_t>(context) & 1);
    context = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(context) & ~1);

    auto sessionState = toImpl(pageRef)->sessionState([pageRef, context, filter](WebBackForwardListItem& item) {
        if (filter) {
            if (!filter(pageRef, WKPageGetSessionBackForwardListItemValueType(), toAPI(&item), context))
                return false;

            if (!filter(pageRef, WKPageGetSessionHistoryURLValueType(), toURLRef(item.originalURL().impl()), context))
                return false;
        }

        return true;
    });

    auto data = encodeLegacySessionState(sessionState);
    if (shouldReturnData)
        return toAPI(data.leakRef());

    return toAPI(&API::SessionState::create(WTFMove(sessionState)).leakRef());
}

static void restoreFromSessionState(WKPageRef pageRef, WKTypeRef sessionStateRef, bool navigate)
{
    SessionState sessionState;

    // FIXME: This is for backwards compatibility with Safari. Remove it once Safari no longer depends on it.
    if (toImpl(sessionStateRef)->type() == API::Object::Type::Data) {
        if (!decodeLegacySessionState(toImpl(static_cast<WKDataRef>(sessionStateRef))->bytes(), toImpl(static_cast<WKDataRef>(sessionStateRef))->size(), sessionState))
            return;
    } else {
        ASSERT(toImpl(sessionStateRef)->type() == API::Object::Type::SessionState);

        sessionState = toImpl(static_cast<WKSessionStateRef>(sessionStateRef))->sessionState();
    }

    toImpl(pageRef)->restoreFromSessionState(WTFMove(sessionState), navigate);
}

void WKPageRestoreFromSessionState(WKPageRef pageRef, WKTypeRef sessionStateRef)
{
    restoreFromSessionState(pageRef, sessionStateRef, true);
}

void WKPageRestoreFromSessionStateWithoutNavigation(WKPageRef pageRef, WKTypeRef sessionStateRef)
{
    restoreFromSessionState(pageRef, sessionStateRef, false);
}

double WKPageGetTextZoomFactor(WKPageRef pageRef)
{
    return toImpl(pageRef)->textZoomFactor();
}

double WKPageGetBackingScaleFactor(WKPageRef pageRef)
{
    return toImpl(pageRef)->deviceScaleFactor();
}

void WKPageSetCustomBackingScaleFactor(WKPageRef pageRef, double customScaleFactor)
{
    toImpl(pageRef)->setCustomDeviceScaleFactor(customScaleFactor);
}

bool WKPageSupportsTextZoom(WKPageRef pageRef)
{
    return toImpl(pageRef)->supportsTextZoom();
}

void WKPageSetTextZoomFactor(WKPageRef pageRef, double zoomFactor)
{
    toImpl(pageRef)->setTextZoomFactor(zoomFactor);
}

double WKPageGetPageZoomFactor(WKPageRef pageRef)
{
    return toImpl(pageRef)->pageZoomFactor();
}

void WKPageSetPageZoomFactor(WKPageRef pageRef, double zoomFactor)
{
    toImpl(pageRef)->setPageZoomFactor(zoomFactor);
}

void WKPageSetPageAndTextZoomFactors(WKPageRef pageRef, double pageZoomFactor, double textZoomFactor)
{
    toImpl(pageRef)->setPageAndTextZoomFactors(pageZoomFactor, textZoomFactor);
}

void WKPageSetScaleFactor(WKPageRef pageRef, double scale, WKPoint origin)
{
    toImpl(pageRef)->scalePage(scale, toIntPoint(origin));
}

double WKPageGetScaleFactor(WKPageRef pageRef)
{
    return toImpl(pageRef)->pageScaleFactor();
}

void WKPageSetUseFixedLayout(WKPageRef pageRef, bool fixed)
{
    toImpl(pageRef)->setUseFixedLayout(fixed);
}

void WKPageSetFixedLayoutSize(WKPageRef pageRef, WKSize size)
{
    toImpl(pageRef)->setFixedLayoutSize(toIntSize(size));
}

bool WKPageUseFixedLayout(WKPageRef pageRef)
{
    return toImpl(pageRef)->useFixedLayout();
}

WKSize WKPageFixedLayoutSize(WKPageRef pageRef)
{
    return toAPI(toImpl(pageRef)->fixedLayoutSize());
}

void WKPageListenForLayoutMilestones(WKPageRef pageRef, WKLayoutMilestones milestones)
{
    toImpl(pageRef)->listenForLayoutMilestones(toLayoutMilestones(milestones));
}

bool WKPageHasHorizontalScrollbar(WKPageRef pageRef)
{
    return toImpl(pageRef)->hasHorizontalScrollbar();
}

bool WKPageHasVerticalScrollbar(WKPageRef pageRef)
{
    return toImpl(pageRef)->hasVerticalScrollbar();
}

void WKPageSetSuppressScrollbarAnimations(WKPageRef pageRef, bool suppressAnimations)
{
    toImpl(pageRef)->setSuppressScrollbarAnimations(suppressAnimations);
}

bool WKPageAreScrollbarAnimationsSuppressed(WKPageRef pageRef)
{
    return toImpl(pageRef)->areScrollbarAnimationsSuppressed();
}

bool WKPageIsPinnedToLeftSide(WKPageRef pageRef)
{
    return toImpl(pageRef)->isPinnedToLeftSide();
}

bool WKPageIsPinnedToRightSide(WKPageRef pageRef)
{
    return toImpl(pageRef)->isPinnedToRightSide();
}

bool WKPageIsPinnedToTopSide(WKPageRef pageRef)
{
    return toImpl(pageRef)->isPinnedToTopSide();
}

bool WKPageIsPinnedToBottomSide(WKPageRef pageRef)
{
    return toImpl(pageRef)->isPinnedToBottomSide();
}

bool WKPageRubberBandsAtLeft(WKPageRef pageRef)
{
    return toImpl(pageRef)->rubberBandsAtLeft();
}

void WKPageSetRubberBandsAtLeft(WKPageRef pageRef, bool rubberBandsAtLeft)
{
    toImpl(pageRef)->setRubberBandsAtLeft(rubberBandsAtLeft);
}

bool WKPageRubberBandsAtRight(WKPageRef pageRef)
{
    return toImpl(pageRef)->rubberBandsAtRight();
}

void WKPageSetRubberBandsAtRight(WKPageRef pageRef, bool rubberBandsAtRight)
{
    toImpl(pageRef)->setRubberBandsAtRight(rubberBandsAtRight);
}

bool WKPageRubberBandsAtTop(WKPageRef pageRef)
{
    return toImpl(pageRef)->rubberBandsAtTop();
}

void WKPageSetRubberBandsAtTop(WKPageRef pageRef, bool rubberBandsAtTop)
{
    toImpl(pageRef)->setRubberBandsAtTop(rubberBandsAtTop);
}

bool WKPageRubberBandsAtBottom(WKPageRef pageRef)
{
    return toImpl(pageRef)->rubberBandsAtBottom();
}

void WKPageSetRubberBandsAtBottom(WKPageRef pageRef, bool rubberBandsAtBottom)
{
    toImpl(pageRef)->setRubberBandsAtBottom(rubberBandsAtBottom);
}

bool WKPageVerticalRubberBandingIsEnabled(WKPageRef pageRef)
{
    return toImpl(pageRef)->verticalRubberBandingIsEnabled();
}

void WKPageSetEnableVerticalRubberBanding(WKPageRef pageRef, bool enableVerticalRubberBanding)
{
    toImpl(pageRef)->setEnableVerticalRubberBanding(enableVerticalRubberBanding);
}

bool WKPageHorizontalRubberBandingIsEnabled(WKPageRef pageRef)
{
    return toImpl(pageRef)->horizontalRubberBandingIsEnabled();
}

void WKPageSetEnableHorizontalRubberBanding(WKPageRef pageRef, bool enableHorizontalRubberBanding)
{
    toImpl(pageRef)->setEnableHorizontalRubberBanding(enableHorizontalRubberBanding);
}

void WKPageSetBackgroundExtendsBeyondPage(WKPageRef pageRef, bool backgroundExtendsBeyondPage)
{
    toImpl(pageRef)->setBackgroundExtendsBeyondPage(backgroundExtendsBeyondPage);
}

bool WKPageBackgroundExtendsBeyondPage(WKPageRef pageRef)
{
    return toImpl(pageRef)->backgroundExtendsBeyondPage();
}

void WKPageSetPaginationMode(WKPageRef pageRef, WKPaginationMode paginationMode)
{
    WebCore::Pagination::Mode mode;
    switch (paginationMode) {
    case kWKPaginationModeUnpaginated:
        mode = WebCore::Pagination::Unpaginated;
        break;
    case kWKPaginationModeLeftToRight:
        mode = WebCore::Pagination::LeftToRightPaginated;
        break;
    case kWKPaginationModeRightToLeft:
        mode = WebCore::Pagination::RightToLeftPaginated;
        break;
    case kWKPaginationModeTopToBottom:
        mode = WebCore::Pagination::TopToBottomPaginated;
        break;
    case kWKPaginationModeBottomToTop:
        mode = WebCore::Pagination::BottomToTopPaginated;
        break;
    default:
        return;
    }
    toImpl(pageRef)->setPaginationMode(mode);
}

WKPaginationMode WKPageGetPaginationMode(WKPageRef pageRef)
{
    switch (toImpl(pageRef)->paginationMode()) {
    case WebCore::Pagination::Unpaginated:
        return kWKPaginationModeUnpaginated;
    case WebCore::Pagination::LeftToRightPaginated:
        return kWKPaginationModeLeftToRight;
    case WebCore::Pagination::RightToLeftPaginated:
        return kWKPaginationModeRightToLeft;
    case WebCore::Pagination::TopToBottomPaginated:
        return kWKPaginationModeTopToBottom;
    case WebCore::Pagination::BottomToTopPaginated:
        return kWKPaginationModeBottomToTop;
    }

    ASSERT_NOT_REACHED();
    return kWKPaginationModeUnpaginated;
}

void WKPageSetPaginationBehavesLikeColumns(WKPageRef pageRef, bool behavesLikeColumns)
{
    toImpl(pageRef)->setPaginationBehavesLikeColumns(behavesLikeColumns);
}

bool WKPageGetPaginationBehavesLikeColumns(WKPageRef pageRef)
{
    return toImpl(pageRef)->paginationBehavesLikeColumns();
}

void WKPageSetPageLength(WKPageRef pageRef, double pageLength)
{
    toImpl(pageRef)->setPageLength(pageLength);
}

double WKPageGetPageLength(WKPageRef pageRef)
{
    return toImpl(pageRef)->pageLength();
}

void WKPageSetGapBetweenPages(WKPageRef pageRef, double gap)
{
    toImpl(pageRef)->setGapBetweenPages(gap);
}

double WKPageGetGapBetweenPages(WKPageRef pageRef)
{
    return toImpl(pageRef)->gapBetweenPages();
}

void WKPageSetPaginationLineGridEnabled(WKPageRef pageRef, bool lineGridEnabled)
{
    toImpl(pageRef)->setPaginationLineGridEnabled(lineGridEnabled);
}

bool WKPageGetPaginationLineGridEnabled(WKPageRef pageRef)
{
    return toImpl(pageRef)->paginationLineGridEnabled();
}

unsigned WKPageGetPageCount(WKPageRef pageRef)
{
    return toImpl(pageRef)->pageCount();
}

bool WKPageCanDelete(WKPageRef pageRef)
{
    return toImpl(pageRef)->canDelete();
}

bool WKPageHasSelectedRange(WKPageRef pageRef)
{
    return toImpl(pageRef)->hasSelectedRange();
}

bool WKPageIsContentEditable(WKPageRef pageRef)
{
    return toImpl(pageRef)->isContentEditable();
}

void WKPageSetMaintainsInactiveSelection(WKPageRef pageRef, bool newValue)
{
    return toImpl(pageRef)->setMaintainsInactiveSelection(newValue);
}

void WKPageCenterSelectionInVisibleArea(WKPageRef pageRef)
{
    return toImpl(pageRef)->centerSelectionInVisibleArea();
}

void WKPageFindStringMatches(WKPageRef pageRef, WKStringRef string, WKFindOptions options, unsigned maxMatchCount)
{
    toImpl(pageRef)->findStringMatches(toImpl(string)->string(), toFindOptions(options), maxMatchCount);
}

void WKPageGetImageForFindMatch(WKPageRef pageRef, int32_t matchIndex)
{
    toImpl(pageRef)->getImageForFindMatch(matchIndex);
}

void WKPageSelectFindMatch(WKPageRef pageRef, int32_t matchIndex)
{
    toImpl(pageRef)->selectFindMatch(matchIndex);
}

void WKPageFindString(WKPageRef pageRef, WKStringRef string, WKFindOptions options, unsigned maxMatchCount)
{
    toImpl(pageRef)->findString(toImpl(string)->string(), toFindOptions(options), maxMatchCount);
}

void WKPageHideFindUI(WKPageRef pageRef)
{
    toImpl(pageRef)->hideFindUI();
}

void WKPageCountStringMatches(WKPageRef pageRef, WKStringRef string, WKFindOptions options, unsigned maxMatchCount)
{
    toImpl(pageRef)->countStringMatches(toImpl(string)->string(), toFindOptions(options), maxMatchCount);
}

void WKPageSetPageContextMenuClient(WKPageRef pageRef, const WKPageContextMenuClientBase* wkClient)
{
#if ENABLE(CONTEXT_MENUS)
    class ContextMenuClient final : public API::Client<WKPageContextMenuClientBase>, public API::ContextMenuClient {
    public:
        explicit ContextMenuClient(const WKPageContextMenuClientBase* client)
        {
            initialize(client);
        }

    private:
        void getContextMenuFromProposedMenu(WebPageProxy& page, Vector<Ref<WebKit::WebContextMenuItem>>&& proposedMenuVector, WebKit::WebContextMenuListenerProxy& contextMenuListener, const WebHitTestResultData& hitTestResultData, API::Object* userData) override
        {
            if (m_client.base.version >= 4 && m_client.getContextMenuFromProposedMenuAsync) {
                Vector<RefPtr<API::Object>> proposedMenuItems;
                proposedMenuItems.reserveInitialCapacity(proposedMenuVector.size());
                
                for (const auto& menuItem : proposedMenuVector)
                    proposedMenuItems.uncheckedAppend(menuItem.ptr());
                
                auto webHitTestResult = API::HitTestResult::create(hitTestResultData);
                m_client.getContextMenuFromProposedMenuAsync(toAPI(&page), toAPI(API::Array::create(WTFMove(proposedMenuItems)).ptr()), toAPI(&contextMenuListener), toAPI(webHitTestResult.ptr()), toAPI(userData), m_client.base.clientInfo);
                return;
            }
            
            if (!m_client.getContextMenuFromProposedMenu && !m_client.getContextMenuFromProposedMenu_deprecatedForUseWithV0) {
                contextMenuListener.useContextMenuItems(WTFMove(proposedMenuVector));
                return;
            }

            if (m_client.base.version >= 2 && !m_client.getContextMenuFromProposedMenu) {
                contextMenuListener.useContextMenuItems(WTFMove(proposedMenuVector));
                return;
            }

            Vector<RefPtr<API::Object>> proposedMenuItems;
            proposedMenuItems.reserveInitialCapacity(proposedMenuVector.size());

            for (const auto& menuItem : proposedMenuVector)
                proposedMenuItems.uncheckedAppend(menuItem.ptr());

            WKArrayRef newMenu = nullptr;
            if (m_client.base.version >= 2) {
                auto webHitTestResult = API::HitTestResult::create(hitTestResultData);
                m_client.getContextMenuFromProposedMenu(toAPI(&page), toAPI(API::Array::create(WTFMove(proposedMenuItems)).ptr()), &newMenu, toAPI(webHitTestResult.ptr()), toAPI(userData), m_client.base.clientInfo);
            } else
                m_client.getContextMenuFromProposedMenu_deprecatedForUseWithV0(toAPI(&page), toAPI(API::Array::create(WTFMove(proposedMenuItems)).ptr()), &newMenu, toAPI(userData), m_client.base.clientInfo);

            RefPtr<API::Array> array = adoptRef(toImpl(newMenu));

            Vector<Ref<WebContextMenuItem>> customMenu;
            size_t newSize = array ? array->size() : 0;
            customMenu.reserveInitialCapacity(newSize);
            for (size_t i = 0; i < newSize; ++i) {
                WebContextMenuItem* item = array->at<WebContextMenuItem>(i);
                if (!item) {
                    LOG(ContextMenu, "New menu entry at index %i is not a WebContextMenuItem", (int)i);
                    continue;
                }

                customMenu.uncheckedAppend(*item);
            }

            contextMenuListener.useContextMenuItems(WTFMove(customMenu));
        }

        void customContextMenuItemSelected(WebPageProxy& page, const WebContextMenuItemData& itemData) override
        {
            if (!m_client.customContextMenuItemSelected)
                return;

            m_client.customContextMenuItemSelected(toAPI(&page), toAPI(WebContextMenuItem::create(itemData).ptr()), m_client.base.clientInfo);
        }

        void showContextMenu(WebPageProxy& page, const WebCore::IntPoint& menuLocation, const Vector<Ref<WebContextMenuItem>>& menuItemsVector) override
        {
            if (!canShowContextMenu())
                return;

            Vector<RefPtr<API::Object>> menuItems;
            menuItems.reserveInitialCapacity(menuItemsVector.size());

            for (const auto& menuItem : menuItemsVector)
                menuItems.uncheckedAppend(menuItem.ptr());

            m_client.showContextMenu(toAPI(&page), toAPI(menuLocation), toAPI(API::Array::create(WTFMove(menuItems)).ptr()), m_client.base.clientInfo);
        }

        bool canShowContextMenu() const override
        {
            return m_client.showContextMenu;
        }

        bool hideContextMenu(WebPageProxy& page) override
        {
            if (!m_client.hideContextMenu)
                return false;

            m_client.hideContextMenu(toAPI(&page), m_client.base.clientInfo);

            return true;
        }
    };

    toImpl(pageRef)->setContextMenuClient(makeUnique<ContextMenuClient>(wkClient));
#else
    UNUSED_PARAM(pageRef);
    UNUSED_PARAM(wkClient);
#endif
}

void WKPageSetPageDiagnosticLoggingClient(WKPageRef pageRef, const WKPageDiagnosticLoggingClientBase* wkClient)
{
    toImpl(pageRef)->setDiagnosticLoggingClient(makeUnique<WebPageDiagnosticLoggingClient>(wkClient));
}

void WKPageSetPageFindClient(WKPageRef pageRef, const WKPageFindClientBase* wkClient)
{
    class FindClient : public API::Client<WKPageFindClientBase>, public API::FindClient {
    public:
        explicit FindClient(const WKPageFindClientBase* client)
        {
            initialize(client);
        }

    private:
        void didFindString(WebPageProxy* page, const String& string, const Vector<WebCore::IntRect>&, uint32_t matchCount, int32_t, bool didWrapAround) override
        {
            if (!m_client.didFindString)
                return;
            
            m_client.didFindString(toAPI(page), toAPI(string.impl()), matchCount, m_client.base.clientInfo);
        }

        void didFailToFindString(WebPageProxy* page, const String& string) override
        {
            if (!m_client.didFailToFindString)
                return;
            
            m_client.didFailToFindString(toAPI(page), toAPI(string.impl()), m_client.base.clientInfo);
        }

        void didCountStringMatches(WebPageProxy* page, const String& string, uint32_t matchCount) override
        {
            if (!m_client.didCountStringMatches)
                return;

            m_client.didCountStringMatches(toAPI(page), toAPI(string.impl()), matchCount, m_client.base.clientInfo);
        }
    };

    toImpl(pageRef)->setFindClient(makeUnique<FindClient>(wkClient));
}

void WKPageSetPageFindMatchesClient(WKPageRef pageRef, const WKPageFindMatchesClientBase* wkClient)
{
    class FindMatchesClient : public API::Client<WKPageFindMatchesClientBase>, public API::FindMatchesClient {
    public:
        explicit FindMatchesClient(const WKPageFindMatchesClientBase* client)
        {
            initialize(client);
        }

    private:
        void didFindStringMatches(WebPageProxy* page, const String& string, const Vector<Vector<WebCore::IntRect>>& matchRects, int32_t index) override
        {
            if (!m_client.didFindStringMatches)
                return;

            Vector<RefPtr<API::Object>> matches;
            matches.reserveInitialCapacity(matchRects.size());

            for (const auto& rects : matchRects) {
                Vector<RefPtr<API::Object>> apiRects;
                apiRects.reserveInitialCapacity(rects.size());

                for (const auto& rect : rects)
                    apiRects.uncheckedAppend(API::Rect::create(toAPI(rect)));

                matches.uncheckedAppend(API::Array::create(WTFMove(apiRects)));
            }

            m_client.didFindStringMatches(toAPI(page), toAPI(string.impl()), toAPI(API::Array::create(WTFMove(matches)).ptr()), index, m_client.base.clientInfo);
        }

        void didGetImageForMatchResult(WebPageProxy* page, WebImage* image, int32_t index) override
        {
            if (!m_client.didGetImageForMatchResult)
                return;

            m_client.didGetImageForMatchResult(toAPI(page), toAPI(image), index, m_client.base.clientInfo);
        }
    };

    toImpl(pageRef)->setFindMatchesClient(makeUnique<FindMatchesClient>(wkClient));
}

void WKPageSetPageInjectedBundleClient(WKPageRef pageRef, const WKPageInjectedBundleClientBase* wkClient)
{
    toImpl(pageRef)->setInjectedBundleClient(wkClient);
}

void WKPageSetPageFormClient(WKPageRef pageRef, const WKPageFormClientBase* wkClient)
{
    toImpl(pageRef)->setFormClient(makeUnique<WebFormClient>(wkClient));
}

void WKPageSetPageLoaderClient(WKPageRef pageRef, const WKPageLoaderClientBase* wkClient)
{
    class LoaderClient : public API::Client<WKPageLoaderClientBase>, public API::LoaderClient {
    public:
        explicit LoaderClient(const WKPageLoaderClientBase* client)
        {
            initialize(client);
            
#if !PLATFORM(MAC) || __MAC_OS_X_VERSION_MIN_REQUIRED > 101400
            // WKPageSetPageLoaderClient is deprecated. Use WKPageSetPageNavigationClient instead.
            RELEASE_ASSERT(!m_client.didFinishDocumentLoadForFrame);
            RELEASE_ASSERT(!m_client.didSameDocumentNavigationForFrame);
            RELEASE_ASSERT(!m_client.didReceiveTitleForFrame);
            RELEASE_ASSERT(!m_client.didFirstLayoutForFrame);
            RELEASE_ASSERT(!m_client.didRemoveFrameFromHierarchy);
            RELEASE_ASSERT(!m_client.didDisplayInsecureContentForFrame);
            RELEASE_ASSERT(!m_client.didRunInsecureContentForFrame);
            RELEASE_ASSERT(!m_client.canAuthenticateAgainstProtectionSpaceInFrame);
            RELEASE_ASSERT(!m_client.didReceiveAuthenticationChallengeInFrame);
            RELEASE_ASSERT(!m_client.didStartProgress);
            RELEASE_ASSERT(!m_client.didChangeProgress);
            RELEASE_ASSERT(!m_client.didFinishProgress);
            RELEASE_ASSERT(!m_client.processDidBecomeUnresponsive);
            RELEASE_ASSERT(!m_client.processDidBecomeResponsive);
            RELEASE_ASSERT(!m_client.shouldGoToBackForwardListItem);
            RELEASE_ASSERT(!m_client.didFailToInitializePlugin_deprecatedForUseWithV0);
            RELEASE_ASSERT(!m_client.didDetectXSSForFrame);
            RELEASE_ASSERT(!m_client.didNewFirstVisuallyNonEmptyLayout_unavailable);
            RELEASE_ASSERT(!m_client.willGoToBackForwardListItem);
            RELEASE_ASSERT(!m_client.interactionOccurredWhileProcessUnresponsive);
            RELEASE_ASSERT(!m_client.pluginDidFail_deprecatedForUseWithV1);
            RELEASE_ASSERT(!m_client.didReceiveIntentForFrame_unavailable);
            RELEASE_ASSERT(!m_client.registerIntentServiceForFrame_unavailable);
            RELEASE_ASSERT(!m_client.pluginLoadPolicy_deprecatedForUseWithV2);
            RELEASE_ASSERT(!m_client.pluginDidFail);
            RELEASE_ASSERT(!m_client.pluginLoadPolicy);
            RELEASE_ASSERT(!m_client.webGLLoadPolicy);
            RELEASE_ASSERT(!m_client.resolveWebGLLoadPolicy);
            RELEASE_ASSERT(!m_client.navigationGestureDidBegin);
            RELEASE_ASSERT(!m_client.navigationGestureWillEnd);
            RELEASE_ASSERT(!m_client.navigationGestureDidEnd);
#endif
        }

    private:
        
        void didCommitLoadForFrame(WebPageProxy& page, WebFrameProxy& frame, API::Navigation*, API::Object* userData) override
        {
            if (!m_client.didCommitLoadForFrame)
                return;

            m_client.didCommitLoadForFrame(toAPI(&page), toAPI(&frame), toAPI(userData), m_client.base.clientInfo);
        }
        
        void didStartProvisionalLoadForFrame(WebPageProxy& page, WebFrameProxy& frame, API::Navigation*, API::Object* userData) override
        {
            if (!m_client.didStartProvisionalLoadForFrame)
                return;

            m_client.didStartProvisionalLoadForFrame(toAPI(&page), toAPI(&frame), toAPI(userData), m_client.base.clientInfo);
        }

        void didReceiveServerRedirectForProvisionalLoadForFrame(WebPageProxy& page, WebFrameProxy& frame, API::Navigation*, API::Object* userData) override
        {
            if (!m_client.didReceiveServerRedirectForProvisionalLoadForFrame)
                return;

            m_client.didReceiveServerRedirectForProvisionalLoadForFrame(toAPI(&page), toAPI(&frame), toAPI(userData), m_client.base.clientInfo);
        }

        void didFailProvisionalLoadWithErrorForFrame(WebPageProxy& page, WebFrameProxy& frame, API::Navigation*, const WebCore::ResourceError& error, API::Object* userData) override
        {
            if (!m_client.didFailProvisionalLoadWithErrorForFrame)
                return;

            m_client.didFailProvisionalLoadWithErrorForFrame(toAPI(&page), toAPI(&frame), toAPI(error), toAPI(userData), m_client.base.clientInfo);
        }

        void didFinishLoadForFrame(WebPageProxy& page, WebFrameProxy& frame, API::Navigation*, API::Object* userData) override
        {
            if (!m_client.didFinishLoadForFrame)
                return;

            m_client.didFinishLoadForFrame(toAPI(&page), toAPI(&frame), toAPI(userData), m_client.base.clientInfo);
        }

        void didFailLoadWithErrorForFrame(WebPageProxy& page, WebFrameProxy& frame, API::Navigation*, const WebCore::ResourceError& error, API::Object* userData) override
        {
            if (!m_client.didFailLoadWithErrorForFrame)
                return;

            m_client.didFailLoadWithErrorForFrame(toAPI(&page), toAPI(&frame), toAPI(error), toAPI(userData), m_client.base.clientInfo);
        }

        void didFirstVisuallyNonEmptyLayoutForFrame(WebPageProxy& page, WebFrameProxy& frame, API::Object* userData) override
        {
            if (!m_client.didFirstVisuallyNonEmptyLayoutForFrame)
                return;

            m_client.didFirstVisuallyNonEmptyLayoutForFrame(toAPI(&page), toAPI(&frame), toAPI(userData), m_client.base.clientInfo);
        }

        void didReachLayoutMilestone(WebPageProxy& page, OptionSet<WebCore::LayoutMilestone> milestones) override
        {
            if (!m_client.didLayout)
                return;

            m_client.didLayout(toAPI(&page), toWKLayoutMilestones(milestones), nullptr, m_client.base.clientInfo);
        }

        bool processDidCrash(WebPageProxy& page) override
        {
            if (!m_client.processDidCrash)
                return false;

            m_client.processDidCrash(toAPI(&page), m_client.base.clientInfo);
            return true;
        }

        void didChangeBackForwardList(WebPageProxy& page, WebBackForwardListItem* addedItem, Vector<Ref<WebBackForwardListItem>>&& removedItems) override
        {
            if (!m_client.didChangeBackForwardList)
                return;

            RefPtr<API::Array> removedItemsArray;
            if (!removedItems.isEmpty()) {
                Vector<RefPtr<API::Object>> removedItemsVector;
                removedItemsVector.reserveInitialCapacity(removedItems.size());
                for (auto& removedItem : removedItems)
                removedItemsVector.append(WTFMove(removedItem));

                removedItemsArray = API::Array::create(WTFMove(removedItemsVector));
            }

            m_client.didChangeBackForwardList(toAPI(&page), toAPI(addedItem), toAPI(removedItemsArray.get()), m_client.base.clientInfo);
        }
        
        bool shouldKeepCurrentBackForwardListItemInList(WebKit::WebPageProxy& page, WebKit::WebBackForwardListItem& item) override
        {
            if (!m_client.shouldKeepCurrentBackForwardListItemInList)
                return true;

            return m_client.shouldKeepCurrentBackForwardListItemInList(toAPI(&page), toAPI(&item), m_client.base.clientInfo);
        }
    };

    WebPageProxy* webPageProxy = toImpl(pageRef);

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    // GCC 10 gets confused here and warns that WKPageSetPagePolicyClient is deprecated when we call
    // makeUnique<LoaderClient>(). This seems to be a GCC bug. It's not really appropriate to use
    // ALLOW_DEPRECATED_DECLARATIONS_BEGIN/END here because we are not using a deprecated symbol.
    auto loaderClient = makeUnique<LoaderClient>(wkClient);
ALLOW_DEPRECATED_DECLARATIONS_END

    // It would be nice to get rid of this code and transition all clients to using didLayout instead of
    // didFirstLayoutInFrame and didFirstVisuallyNonEmptyLayoutInFrame. In the meantime, this is required
    // for backwards compatibility.
    OptionSet<WebCore::LayoutMilestone> milestones;
    if (loaderClient->client().didFirstLayoutForFrame)
        milestones.add(WebCore::DidFirstLayout);
    if (loaderClient->client().didFirstVisuallyNonEmptyLayoutForFrame)
        milestones.add(WebCore::DidFirstVisuallyNonEmptyLayout);

    if (milestones)
        webPageProxy->send(Messages::WebPage::ListenForLayoutMilestones(milestones));

    webPageProxy->setLoaderClient(WTFMove(loaderClient));
}

void WKPageSetPagePolicyClient(WKPageRef pageRef, const WKPagePolicyClientBase* wkClient)
{
    class PolicyClient : public API::Client<WKPagePolicyClientBase>, public API::PolicyClient {
    public:
        explicit PolicyClient(const WKPagePolicyClientBase* client)
        {
            initialize(client);
        }

    private:
        void decidePolicyForNavigationAction(WebPageProxy& page, WebFrameProxy* frame, Ref<API::NavigationAction>&& navigationAction, WebFrameProxy* originatingFrame, const WebCore::ResourceRequest& originalResourceRequest, const WebCore::ResourceRequest& resourceRequest, Ref<WebFramePolicyListenerProxy>&& listener, API::Object* userData) override
        {
            if (!m_client.decidePolicyForNavigationAction_deprecatedForUseWithV0 && !m_client.decidePolicyForNavigationAction_deprecatedForUseWithV1 && !m_client.decidePolicyForNavigationAction) {
                listener->use();
                return;
            }

            Ref<API::URLRequest> originalRequest = API::URLRequest::create(originalResourceRequest);
            Ref<API::URLRequest> request = API::URLRequest::create(resourceRequest);

            if (m_client.decidePolicyForNavigationAction_deprecatedForUseWithV0)
                m_client.decidePolicyForNavigationAction_deprecatedForUseWithV0(toAPI(&page), toAPI(frame), toAPI(navigationAction->data().navigationType), toAPI(navigationAction->data().modifiers), toAPI(navigationAction->data().mouseButton), toAPI(request.ptr()), toAPI(listener.ptr()), toAPI(userData), m_client.base.clientInfo);
            else if (m_client.decidePolicyForNavigationAction_deprecatedForUseWithV1)
                m_client.decidePolicyForNavigationAction_deprecatedForUseWithV1(toAPI(&page), toAPI(frame), toAPI(navigationAction->data().navigationType), toAPI(navigationAction->data().modifiers), toAPI(navigationAction->data().mouseButton), toAPI(originatingFrame), toAPI(request.ptr()), toAPI(listener.ptr()), toAPI(userData), m_client.base.clientInfo);
            else
                m_client.decidePolicyForNavigationAction(toAPI(&page), toAPI(frame), toAPI(navigationAction->data().navigationType), toAPI(navigationAction->data().modifiers), toAPI(navigationAction->data().mouseButton), toAPI(originatingFrame), toAPI(originalRequest.ptr()), toAPI(request.ptr()), toAPI(listener.ptr()), toAPI(userData), m_client.base.clientInfo);
        }

        void decidePolicyForNewWindowAction(WebPageProxy& page, WebFrameProxy& frame, Ref<API::NavigationAction>&& navigationAction, const WebCore::ResourceRequest& resourceRequest, const String& frameName, Ref<WebFramePolicyListenerProxy>&& listener, API::Object* userData) override
        {
            if (!m_client.decidePolicyForNewWindowAction) {
                listener->use();
                return;
            }

            Ref<API::URLRequest> request = API::URLRequest::create(resourceRequest);

            m_client.decidePolicyForNewWindowAction(toAPI(&page), toAPI(&frame), toAPI(navigationAction->data().navigationType), toAPI(navigationAction->data().modifiers), toAPI(navigationAction->data().mouseButton), toAPI(request.ptr()), toAPI(frameName.impl()), toAPI(listener.ptr()), toAPI(userData), m_client.base.clientInfo);
        }

        void decidePolicyForResponse(WebPageProxy& page, WebFrameProxy& frame, const WebCore::ResourceResponse& resourceResponse, const WebCore::ResourceRequest& resourceRequest, bool canShowMIMEType, Ref<WebFramePolicyListenerProxy>&& listener, API::Object* userData) override
        {
            if (!m_client.decidePolicyForResponse_deprecatedForUseWithV0 && !m_client.decidePolicyForResponse) {
                listener->use();
                return;
            }

            Ref<API::URLResponse> response = API::URLResponse::create(resourceResponse);
            Ref<API::URLRequest> request = API::URLRequest::create(resourceRequest);

            if (m_client.decidePolicyForResponse_deprecatedForUseWithV0)
                m_client.decidePolicyForResponse_deprecatedForUseWithV0(toAPI(&page), toAPI(&frame), toAPI(response.ptr()), toAPI(request.ptr()), toAPI(listener.ptr()), toAPI(userData), m_client.base.clientInfo);
            else
                m_client.decidePolicyForResponse(toAPI(&page), toAPI(&frame), toAPI(response.ptr()), toAPI(request.ptr()), canShowMIMEType, toAPI(listener.ptr()), toAPI(userData), m_client.base.clientInfo);
        }

        void unableToImplementPolicy(WebPageProxy& page, WebFrameProxy& frame, const WebCore::ResourceError& error, API::Object* userData) override
        {
            if (!m_client.unableToImplementPolicy)
                return;
            
            m_client.unableToImplementPolicy(toAPI(&page), toAPI(&frame), toAPI(error), toAPI(userData), m_client.base.clientInfo);
        }
    };

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    // GCC 10 gets confused here and warns that WKPageSetPagePolicyClient is deprecated when we call
    // makeUnique<PolicyClient>(). This seems to be a GCC bug. It's not really appropriate to use
    // ALLOW_DEPRECATED_DECLARATIONS_BEGIN/END here because we are not using a deprecated symbol.
    toImpl(pageRef)->setPolicyClient(makeUnique<PolicyClient>(wkClient));
ALLOW_DEPRECATED_DECLARATIONS_END
}

namespace WebKit {
using namespace WebCore;
    
class RunBeforeUnloadConfirmPanelResultListener : public API::ObjectImpl<API::Object::Type::RunBeforeUnloadConfirmPanelResultListener> {
public:
    static Ref<RunBeforeUnloadConfirmPanelResultListener> create(Function<void(bool)>&& completionHandler)
    {
        return adoptRef(*new RunBeforeUnloadConfirmPanelResultListener(WTFMove(completionHandler)));
    }

    virtual ~RunBeforeUnloadConfirmPanelResultListener()
    {
    }

    void call(bool result)
    {
        m_completionHandler(result);
    }

private:
    explicit RunBeforeUnloadConfirmPanelResultListener(Function<void (bool)>&& completionHandler)
        : m_completionHandler(WTFMove(completionHandler))
    {
    }

    Function<void (bool)> m_completionHandler;
};

class RunJavaScriptAlertResultListener : public API::ObjectImpl<API::Object::Type::RunJavaScriptAlertResultListener> {
public:
    static Ref<RunJavaScriptAlertResultListener> create(Function<void()>&& completionHandler)
    {
        return adoptRef(*new RunJavaScriptAlertResultListener(WTFMove(completionHandler)));
    }

    virtual ~RunJavaScriptAlertResultListener()
    {
    }

    void call()
    {
        m_completionHandler();
    }

private:
    explicit RunJavaScriptAlertResultListener(Function<void ()>&& completionHandler)
        : m_completionHandler(WTFMove(completionHandler))
    {
    }
    
    Function<void ()> m_completionHandler;
};

class RunJavaScriptConfirmResultListener : public API::ObjectImpl<API::Object::Type::RunJavaScriptConfirmResultListener> {
public:
    static Ref<RunJavaScriptConfirmResultListener> create(Function<void(bool)>&& completionHandler)
    {
        return adoptRef(*new RunJavaScriptConfirmResultListener(WTFMove(completionHandler)));
    }

    virtual ~RunJavaScriptConfirmResultListener()
    {
    }

    void call(bool result)
    {
        m_completionHandler(result);
    }

private:
    explicit RunJavaScriptConfirmResultListener(Function<void(bool)>&& completionHandler)
        : m_completionHandler(WTFMove(completionHandler))
    {
    }

    Function<void (bool)> m_completionHandler;
};

class RunJavaScriptPromptResultListener : public API::ObjectImpl<API::Object::Type::RunJavaScriptPromptResultListener> {
public:
    static Ref<RunJavaScriptPromptResultListener> create(Function<void(const String&)>&& completionHandler)
    {
        return adoptRef(*new RunJavaScriptPromptResultListener(WTFMove(completionHandler)));
    }

    virtual ~RunJavaScriptPromptResultListener()
    {
    }

    void call(const String& result)
    {
        m_completionHandler(result);
    }

private:
    explicit RunJavaScriptPromptResultListener(Function<void(const String&)>&& completionHandler)
        : m_completionHandler(WTFMove(completionHandler))
    {
    }

    Function<void (const String&)> m_completionHandler;
};

class RequestStorageAccessConfirmResultListener : public API::ObjectImpl<API::Object::Type::RequestStorageAccessConfirmResultListener> {
public:
    static Ref<RequestStorageAccessConfirmResultListener> create(CompletionHandler<void(bool)>&& completionHandler)
    {
        return adoptRef(*new RequestStorageAccessConfirmResultListener(WTFMove(completionHandler)));
    }
    
    virtual ~RequestStorageAccessConfirmResultListener()
    {
    }
    
    void call(bool result)
    {
        m_completionHandler(result);
    }
    
private:
    explicit RequestStorageAccessConfirmResultListener(CompletionHandler<void(bool)>&& completionHandler)
        : m_completionHandler(WTFMove(completionHandler))
    {
    }
    
    CompletionHandler<void(bool)> m_completionHandler;
};

WK_ADD_API_MAPPING(WKPageRunBeforeUnloadConfirmPanelResultListenerRef, RunBeforeUnloadConfirmPanelResultListener)
WK_ADD_API_MAPPING(WKPageRunJavaScriptAlertResultListenerRef, RunJavaScriptAlertResultListener)
WK_ADD_API_MAPPING(WKPageRunJavaScriptConfirmResultListenerRef, RunJavaScriptConfirmResultListener)
WK_ADD_API_MAPPING(WKPageRunJavaScriptPromptResultListenerRef, RunJavaScriptPromptResultListener)
WK_ADD_API_MAPPING(WKPageRequestStorageAccessConfirmResultListenerRef, RequestStorageAccessConfirmResultListener)

}

WKTypeID WKPageRunBeforeUnloadConfirmPanelResultListenerGetTypeID()
{
    return toAPI(RunBeforeUnloadConfirmPanelResultListener::APIType);
}

void WKPageRunBeforeUnloadConfirmPanelResultListenerCall(WKPageRunBeforeUnloadConfirmPanelResultListenerRef listener, bool result)
{
    toImpl(listener)->call(result);
}

WKTypeID WKPageRunJavaScriptAlertResultListenerGetTypeID()
{
    return toAPI(RunJavaScriptAlertResultListener::APIType);
}

void WKPageRunJavaScriptAlertResultListenerCall(WKPageRunJavaScriptAlertResultListenerRef listener)
{
    toImpl(listener)->call();
}

WKTypeID WKPageRunJavaScriptConfirmResultListenerGetTypeID()
{
    return toAPI(RunJavaScriptConfirmResultListener::APIType);
}

void WKPageRunJavaScriptConfirmResultListenerCall(WKPageRunJavaScriptConfirmResultListenerRef listener, bool result)
{
    toImpl(listener)->call(result);
}

WKTypeID WKPageRunJavaScriptPromptResultListenerGetTypeID()
{
    return toAPI(RunJavaScriptPromptResultListener::APIType);
}

void WKPageRunJavaScriptPromptResultListenerCall(WKPageRunJavaScriptPromptResultListenerRef listener, WKStringRef result)
{
    toImpl(listener)->call(toWTFString(result));
}

WKTypeID WKPageRequestStorageAccessConfirmResultListenerGetTypeID()
{
    return toAPI(RequestStorageAccessConfirmResultListener::APIType);
}

void WKPageRequestStorageAccessConfirmResultListenerCall(WKPageRequestStorageAccessConfirmResultListenerRef listener, bool result)
{
    toImpl(listener)->call(result);
}

void WKPageSetPageUIClient(WKPageRef pageRef, const WKPageUIClientBase* wkClient)
{
    class UIClient : public API::Client<WKPageUIClientBase>, public API::UIClient {
    public:
        explicit UIClient(const WKPageUIClientBase* client)
        {
            initialize(client);
        }

    private:
        void createNewPage(WebPageProxy& page, WebCore::WindowFeatures&& windowFeatures, Ref<API::NavigationAction>&& navigationAction, CompletionHandler<void(RefPtr<WebPageProxy>&&)>&& completionHandler) final
        {
            if (m_client.createNewPage) {
                auto configuration = page.configuration().copy();
                configuration->setRelatedPage(&page);

                auto apiWindowFeatures = API::WindowFeatures::create(windowFeatures);

                return completionHandler(adoptRef(toImpl(m_client.createNewPage(toAPI(&page), toAPI(configuration.ptr()), toAPI(navigationAction.ptr()), toAPI(apiWindowFeatures.ptr()), m_client.base.clientInfo))));
            }
        
            if (m_client.createNewPage_deprecatedForUseWithV1 || m_client.createNewPage_deprecatedForUseWithV0) {
                API::Dictionary::MapType map;
                if (windowFeatures.x)
                    map.set("x", API::Double::create(*windowFeatures.x));
                if (windowFeatures.y)
                    map.set("y", API::Double::create(*windowFeatures.y));
                if (windowFeatures.width)
                    map.set("width", API::Double::create(*windowFeatures.width));
                if (windowFeatures.height)
                    map.set("height", API::Double::create(*windowFeatures.height));
                map.set("menuBarVisible", API::Boolean::create(windowFeatures.menuBarVisible));
                map.set("statusBarVisible", API::Boolean::create(windowFeatures.statusBarVisible));
                map.set("toolBarVisible", API::Boolean::create(windowFeatures.toolBarVisible));
                map.set("locationBarVisible", API::Boolean::create(windowFeatures.locationBarVisible));
                map.set("scrollbarsVisible", API::Boolean::create(windowFeatures.scrollbarsVisible));
                map.set("resizable", API::Boolean::create(windowFeatures.resizable));
                map.set("fullscreen", API::Boolean::create(windowFeatures.fullscreen));
                map.set("dialog", API::Boolean::create(windowFeatures.dialog));
                Ref<API::Dictionary> featuresMap = API::Dictionary::create(WTFMove(map));

                if (m_client.createNewPage_deprecatedForUseWithV1) {
                    Ref<API::URLRequest> request = API::URLRequest::create(navigationAction->request());
                    return completionHandler(adoptRef(toImpl(m_client.createNewPage_deprecatedForUseWithV1(toAPI(&page), toAPI(request.ptr()), toAPI(featuresMap.ptr()), toAPI(navigationAction->modifiers()), toAPI(navigationAction->mouseButton()), m_client.base.clientInfo))));
                }
    
                ASSERT(m_client.createNewPage_deprecatedForUseWithV0);
                return completionHandler(adoptRef(toImpl(m_client.createNewPage_deprecatedForUseWithV0(toAPI(&page), toAPI(featuresMap.ptr()), toAPI(navigationAction->modifiers()), toAPI(navigationAction->mouseButton()), m_client.base.clientInfo))));
            }

            completionHandler(nullptr);
        }

        void showPage(WebPageProxy* page) final
        {
            if (!m_client.showPage)
                return;

            m_client.showPage(toAPI(page), m_client.base.clientInfo);
        }

        void fullscreenMayReturnToInline(WebPageProxy* page) final
        {
            if (!m_client.fullscreenMayReturnToInline)
                return;

            m_client.fullscreenMayReturnToInline(toAPI(page), m_client.base.clientInfo);
        }
        
        void hasVideoInPictureInPictureDidChange(WebPageProxy* page, bool hasVideoInPictureInPicture) final
        {
            if (!m_client.hasVideoInPictureInPictureDidChange)
                return;
            
            m_client.hasVideoInPictureInPictureDidChange(toAPI(page), hasVideoInPictureInPicture, m_client.base.clientInfo);
        }

        void didExceedBackgroundResourceLimitWhileInForeground(WebPageProxy& page, WKResourceLimit limit) final
        {
            if (!m_client.didExceedBackgroundResourceLimitWhileInForeground)
                return;

            m_client.didExceedBackgroundResourceLimitWhileInForeground(toAPI(&page), limit, m_client.base.clientInfo);
        }

        void close(WebPageProxy* page) final
        {
            if (!m_client.close)
                return;

            m_client.close(toAPI(page), m_client.base.clientInfo);
        }

        bool takeFocus(WebPageProxy* page, WKFocusDirection direction) final
        {
            if (!m_client.takeFocus)
                return false;

            m_client.takeFocus(toAPI(page), direction, m_client.base.clientInfo);
            return true;
        }

        void focus(WebPageProxy* page) final
        {
            if (!m_client.focus)
                return;

            m_client.focus(toAPI(page), m_client.base.clientInfo);
        }

        void unfocus(WebPageProxy* page) final
        {
            if (!m_client.unfocus)
                return;

            m_client.unfocus(toAPI(page), m_client.base.clientInfo);
        }

        void runJavaScriptAlert(WebPageProxy& page, const String& message, WebFrameProxy* frame, FrameInfoData&& frameInfo, Function<void()>&& completionHandler) final
        {
            if (m_client.runJavaScriptAlert) {
                RefPtr<RunJavaScriptAlertResultListener> listener = RunJavaScriptAlertResultListener::create(WTFMove(completionHandler));
                RefPtr<API::SecurityOrigin> securityOrigin = API::SecurityOrigin::create(frameInfo.securityOrigin.protocol, frameInfo.securityOrigin.host, frameInfo.securityOrigin.port);
                m_client.runJavaScriptAlert(toAPI(&page), toAPI(message.impl()), toAPI(frame), toAPI(securityOrigin.get()), toAPI(listener.get()), m_client.base.clientInfo);
                return;
            }

            if (m_client.runJavaScriptAlert_deprecatedForUseWithV5) {
                RefPtr<API::SecurityOrigin> securityOrigin = API::SecurityOrigin::create(frameInfo.securityOrigin.protocol, frameInfo.securityOrigin.host, frameInfo.securityOrigin.port);
                m_client.runJavaScriptAlert_deprecatedForUseWithV5(toAPI(&page), toAPI(message.impl()), toAPI(frame), toAPI(securityOrigin.get()), m_client.base.clientInfo);
                completionHandler();
                return;
            }
            
            if (m_client.runJavaScriptAlert_deprecatedForUseWithV0) {
                m_client.runJavaScriptAlert_deprecatedForUseWithV0(toAPI(&page), toAPI(message.impl()), toAPI(frame), m_client.base.clientInfo);
                completionHandler();
                return;
            }


            completionHandler();
        }

        void runJavaScriptConfirm(WebPageProxy& page, const String& message, WebFrameProxy* frame, FrameInfoData&& frameInfo, Function<void(bool)>&& completionHandler) final
        {
            if (m_client.runJavaScriptConfirm) {
                RefPtr<RunJavaScriptConfirmResultListener> listener = RunJavaScriptConfirmResultListener::create(WTFMove(completionHandler));
                RefPtr<API::SecurityOrigin> securityOrigin = API::SecurityOrigin::create(frameInfo.securityOrigin.protocol, frameInfo.securityOrigin.host, frameInfo.securityOrigin.port);
                m_client.runJavaScriptConfirm(toAPI(&page), toAPI(message.impl()), toAPI(frame), toAPI(securityOrigin.get()), toAPI(listener.get()), m_client.base.clientInfo);
                return;
            }

            if (m_client.runJavaScriptConfirm_deprecatedForUseWithV5) {
                RefPtr<API::SecurityOrigin> securityOrigin = API::SecurityOrigin::create(frameInfo.securityOrigin.protocol, frameInfo.securityOrigin.host, frameInfo.securityOrigin.port);
                bool result = m_client.runJavaScriptConfirm_deprecatedForUseWithV5(toAPI(&page), toAPI(message.impl()), toAPI(frame), toAPI(securityOrigin.get()), m_client.base.clientInfo);
                
                completionHandler(result);
                return;
            }
            
            if (m_client.runJavaScriptConfirm_deprecatedForUseWithV0) {
                bool result = m_client.runJavaScriptConfirm_deprecatedForUseWithV0(toAPI(&page), toAPI(message.impl()), toAPI(frame), m_client.base.clientInfo);

                completionHandler(result);
                return;
            }
            
            completionHandler(false);
        }

        void runJavaScriptPrompt(WebPageProxy& page, const String& message, const String& defaultValue, WebFrameProxy* frame, FrameInfoData&& frameInfo, Function<void(const String&)>&& completionHandler) final
        {
            if (m_client.runJavaScriptPrompt) {
                RefPtr<RunJavaScriptPromptResultListener> listener = RunJavaScriptPromptResultListener::create(WTFMove(completionHandler));
                RefPtr<API::SecurityOrigin> securityOrigin = API::SecurityOrigin::create(frameInfo.securityOrigin.protocol, frameInfo.securityOrigin.host, frameInfo.securityOrigin.port);
                m_client.runJavaScriptPrompt(toAPI(&page), toAPI(message.impl()), toAPI(defaultValue.impl()), toAPI(frame), toAPI(securityOrigin.get()), toAPI(listener.get()), m_client.base.clientInfo);
                return;
            }

            if (m_client.runJavaScriptPrompt_deprecatedForUseWithV5) {
                RefPtr<API::SecurityOrigin> securityOrigin = API::SecurityOrigin::create(frameInfo.securityOrigin.protocol, frameInfo.securityOrigin.host, frameInfo.securityOrigin.port);
                RefPtr<API::String> string = adoptRef(toImpl(m_client.runJavaScriptPrompt_deprecatedForUseWithV5(toAPI(&page), toAPI(message.impl()), toAPI(defaultValue.impl()), toAPI(frame), toAPI(securityOrigin.get()), m_client.base.clientInfo)));
                
                if (string)
                    completionHandler(string->string());
                else
                    completionHandler(String());
                return;
            }
            
            if (m_client.runJavaScriptPrompt_deprecatedForUseWithV0) {
                RefPtr<API::String> string = adoptRef(toImpl(m_client.runJavaScriptPrompt_deprecatedForUseWithV0(toAPI(&page), toAPI(message.impl()), toAPI(defaultValue.impl()), toAPI(frame), m_client.base.clientInfo)));
                
                if (string)
                    completionHandler(string->string());
                else
                    completionHandler(String());
                return;
            }

            completionHandler(String());
        }

        void setStatusText(WebPageProxy* page, const String& text) final
        {
            if (!m_client.setStatusText)
                return;

            m_client.setStatusText(toAPI(page), toAPI(text.impl()), m_client.base.clientInfo);
        }

        void mouseDidMoveOverElement(WebPageProxy& page, const WebHitTestResultData& data, OptionSet<WebKit::WebEvent::Modifier> modifiers, API::Object* userData) final
        {
            if (!m_client.mouseDidMoveOverElement && !m_client.mouseDidMoveOverElement_deprecatedForUseWithV0)
                return;

            if (m_client.base.version > 0 && !m_client.mouseDidMoveOverElement)
                return;

            if (!m_client.base.version) {
                m_client.mouseDidMoveOverElement_deprecatedForUseWithV0(toAPI(&page), toAPI(modifiers), toAPI(userData), m_client.base.clientInfo);
                return;
            }

            auto apiHitTestResult = API::HitTestResult::create(data);
            m_client.mouseDidMoveOverElement(toAPI(&page), toAPI(apiHitTestResult.ptr()), toAPI(modifiers), toAPI(userData), m_client.base.clientInfo);
        }

#if ENABLE(NETSCAPE_PLUGIN_API)
        void unavailablePluginButtonClicked(WebPageProxy& page, WKPluginUnavailabilityReason pluginUnavailabilityReason, API::Dictionary& pluginInformation) final
        {
            if (pluginUnavailabilityReason == kWKPluginUnavailabilityReasonPluginMissing) {
                if (m_client.missingPluginButtonClicked_deprecatedForUseWithV0)
                    m_client.missingPluginButtonClicked_deprecatedForUseWithV0(
                        toAPI(&page),
                        toAPI(pluginInformation.get<API::String>(pluginInformationMIMETypeKey())),
                        toAPI(pluginInformation.get<API::String>(pluginInformationPluginURLKey())),
                        toAPI(pluginInformation.get<API::String>(pluginInformationPluginspageAttributeURLKey())),
                        m_client.base.clientInfo);
            }

            if (m_client.unavailablePluginButtonClicked_deprecatedForUseWithV1)
                m_client.unavailablePluginButtonClicked_deprecatedForUseWithV1(
                    toAPI(&page),
                    pluginUnavailabilityReason,
                    toAPI(pluginInformation.get<API::String>(pluginInformationMIMETypeKey())),
                    toAPI(pluginInformation.get<API::String>(pluginInformationPluginURLKey())),
                    toAPI(pluginInformation.get<API::String>(pluginInformationPluginspageAttributeURLKey())),
                    m_client.base.clientInfo);

            if (m_client.unavailablePluginButtonClicked)
                m_client.unavailablePluginButtonClicked(
                    toAPI(&page),
                    pluginUnavailabilityReason,
                    toAPI(&pluginInformation),
                    m_client.base.clientInfo);
        }
#endif // ENABLE(NETSCAPE_PLUGIN_API)

        void didNotHandleKeyEvent(WebPageProxy* page, const NativeWebKeyboardEvent& event) final
        {
            if (!m_client.didNotHandleKeyEvent)
                return;
            m_client.didNotHandleKeyEvent(toAPI(page), event.nativeEvent(), m_client.base.clientInfo);
        }

        void didNotHandleWheelEvent(WebPageProxy* page, const NativeWebWheelEvent& event) final
        {
            if (!m_client.didNotHandleWheelEvent)
                return;
            m_client.didNotHandleWheelEvent(toAPI(page), event.nativeEvent(), m_client.base.clientInfo);
        }

        void toolbarsAreVisible(WebPageProxy& page, Function<void(bool)>&& completionHandler) final
        {
            if (!m_client.toolbarsAreVisible)
                return completionHandler(true);
            completionHandler(m_client.toolbarsAreVisible(toAPI(&page), m_client.base.clientInfo));
        }

        void setToolbarsAreVisible(WebPageProxy& page, bool visible) final
        {
            if (!m_client.setToolbarsAreVisible)
                return;
            m_client.setToolbarsAreVisible(toAPI(&page), visible, m_client.base.clientInfo);
        }

        void menuBarIsVisible(WebPageProxy& page, Function<void(bool)>&& completionHandler) final
        {
            if (!m_client.menuBarIsVisible)
                return completionHandler(true);
            completionHandler(m_client.menuBarIsVisible(toAPI(&page), m_client.base.clientInfo));
        }

        void setMenuBarIsVisible(WebPageProxy& page, bool visible) final
        {
            if (!m_client.setMenuBarIsVisible)
                return;
            m_client.setMenuBarIsVisible(toAPI(&page), visible, m_client.base.clientInfo);
        }

        void statusBarIsVisible(WebPageProxy& page, Function<void(bool)>&& completionHandler) final
        {
            if (!m_client.statusBarIsVisible)
                return completionHandler(true);
            completionHandler(m_client.statusBarIsVisible(toAPI(&page), m_client.base.clientInfo));
        }

        void setStatusBarIsVisible(WebPageProxy& page, bool visible) final
        {
            if (!m_client.setStatusBarIsVisible)
                return;
            m_client.setStatusBarIsVisible(toAPI(&page), visible, m_client.base.clientInfo);
        }

        void setIsResizable(WebPageProxy& page, bool resizable) final
        {
            if (!m_client.setIsResizable)
                return;
            m_client.setIsResizable(toAPI(&page), resizable, m_client.base.clientInfo);
        }

        void setWindowFrame(WebPageProxy& page, const FloatRect& frame) final
        {
            if (!m_client.setWindowFrame)
                return;

            m_client.setWindowFrame(toAPI(&page), toAPI(frame), m_client.base.clientInfo);
        }

        void windowFrame(WebPageProxy& page, Function<void(WebCore::FloatRect)>&& completionHandler) final
        {
            if (!m_client.getWindowFrame)
                return completionHandler({ });

            completionHandler(toFloatRect(m_client.getWindowFrame(toAPI(&page), m_client.base.clientInfo)));
        }

        bool canRunBeforeUnloadConfirmPanel() const final
        {
            return m_client.runBeforeUnloadConfirmPanel_deprecatedForUseWithV6 || m_client.runBeforeUnloadConfirmPanel;
        }

        void runBeforeUnloadConfirmPanel(WebKit::WebPageProxy& page, const WTF::String& message, WebKit::WebFrameProxy* frame, FrameInfoData&&, Function<void(bool)>&& completionHandler) final
        {
            if (m_client.runBeforeUnloadConfirmPanel) {
                RefPtr<RunBeforeUnloadConfirmPanelResultListener> listener = RunBeforeUnloadConfirmPanelResultListener::create(WTFMove(completionHandler));
                m_client.runBeforeUnloadConfirmPanel(toAPI(&page), toAPI(message.impl()), toAPI(frame), toAPI(listener.get()), m_client.base.clientInfo);
                return;
            }

            if (m_client.runBeforeUnloadConfirmPanel_deprecatedForUseWithV6) {
                bool result = m_client.runBeforeUnloadConfirmPanel_deprecatedForUseWithV6(toAPI(&page), toAPI(message.impl()), toAPI(frame), m_client.base.clientInfo);
                completionHandler(result);
                return;
            }

            completionHandler(true);
        }

        void pageDidScroll(WebPageProxy* page) final
        {
            if (!m_client.pageDidScroll)
                return;

            m_client.pageDidScroll(toAPI(page), m_client.base.clientInfo);
        }

        void exceededDatabaseQuota(WebPageProxy* page, WebFrameProxy* frame, API::SecurityOrigin* origin, const String& databaseName, const String& databaseDisplayName, unsigned long long currentQuota, unsigned long long currentOriginUsage, unsigned long long currentDatabaseUsage, unsigned long long expectedUsage, Function<void(unsigned long long)>&& completionHandler) final
        {
            if (!m_client.exceededDatabaseQuota) {
                completionHandler(currentQuota);
                return;
            }

            completionHandler(m_client.exceededDatabaseQuota(toAPI(page), toAPI(frame), toAPI(origin), toAPI(databaseName.impl()), toAPI(databaseDisplayName.impl()), currentQuota, currentOriginUsage, currentDatabaseUsage, expectedUsage, m_client.base.clientInfo));
        }

        bool runOpenPanel(WebPageProxy& page, WebFrameProxy* frame, FrameInfoData&&, API::OpenPanelParameters* parameters, WebOpenPanelResultListenerProxy* listener) final
        {
            if (!m_client.runOpenPanel)
                return false;

            m_client.runOpenPanel(toAPI(&page), toAPI(frame), toAPI(parameters), toAPI(listener), m_client.base.clientInfo);
            return true;
        }

        void decidePolicyForGeolocationPermissionRequest(WebPageProxy& page, WebFrameProxy& frame, const FrameInfoData& frameInfo, Function<void(bool)>& completionHandler) final
        {
            if (!m_client.decidePolicyForGeolocationPermissionRequest)
                return;

            auto origin = API::SecurityOrigin::create(frameInfo.securityOrigin.protocol, frameInfo.securityOrigin.host, frameInfo.securityOrigin.port);
            m_client.decidePolicyForGeolocationPermissionRequest(toAPI(&page), toAPI(&frame), toAPI(origin.ptr()), toAPI(GeolocationPermissionRequest::create(std::exchange(completionHandler, nullptr)).ptr()), m_client.base.clientInfo);
        }

        void decidePolicyForUserMediaPermissionRequest(WebPageProxy& page, WebFrameProxy& frame, API::SecurityOrigin& userMediaDocumentOrigin, API::SecurityOrigin& topLevelDocumentOrigin, UserMediaPermissionRequestProxy& permissionRequest) final
        {
            if (!m_client.decidePolicyForUserMediaPermissionRequest) {
                permissionRequest.deny();
                return;
            }

            m_client.decidePolicyForUserMediaPermissionRequest(toAPI(&page), toAPI(&frame), toAPI(&userMediaDocumentOrigin), toAPI(&topLevelDocumentOrigin), toAPI(&permissionRequest), m_client.base.clientInfo);
        }

        void checkUserMediaPermissionForOrigin(WebPageProxy& page, WebFrameProxy& frame, API::SecurityOrigin& userMediaDocumentOrigin, API::SecurityOrigin& topLevelDocumentOrigin, UserMediaPermissionCheckProxy& request) final
        {
            if (!m_client.checkUserMediaPermissionForOrigin) {
                request.deny();
                return;
            }

            m_client.checkUserMediaPermissionForOrigin(toAPI(&page), toAPI(&frame), toAPI(&userMediaDocumentOrigin), toAPI(&topLevelDocumentOrigin), toAPI(&request), m_client.base.clientInfo);
        }
        
        void decidePolicyForNotificationPermissionRequest(WebPageProxy& page, API::SecurityOrigin& origin, Function<void(bool)>&& completionHandler) final
        {
            if (!m_client.decidePolicyForNotificationPermissionRequest)
                return completionHandler(false);

            m_client.decidePolicyForNotificationPermissionRequest(toAPI(&page), toAPI(&origin), toAPI(NotificationPermissionRequest::create(WTFMove(completionHandler)).ptr()), m_client.base.clientInfo);
        }

        void requestStorageAccessConfirm(WebPageProxy& page, WebFrameProxy* frame, const WebCore::RegistrableDomain& requestingDomain, const WebCore::RegistrableDomain& currentDomain, CompletionHandler<void(bool)>&& completionHandler) final
        {
            if (!m_client.requestStorageAccessConfirm) {
                completionHandler(true);
                return;
            }

            auto listener = RequestStorageAccessConfirmResultListener::create(WTFMove(completionHandler));
            m_client.requestStorageAccessConfirm(toAPI(&page), toAPI(frame), toAPI(requestingDomain.string().impl()), toAPI(currentDomain.string().impl()), toAPI(listener.ptr()), m_client.base.clientInfo);
        }

#if ENABLE(DEVICE_ORIENTATION)
        void shouldAllowDeviceOrientationAndMotionAccess(WebPageProxy& page, WebFrameProxy&, FrameInfoData&& frameInfo, CompletionHandler<void(bool)>&& completionHandler) final
        {
            if (!m_client.shouldAllowDeviceOrientationAndMotionAccess)
                return completionHandler(false);

            auto origin = API::SecurityOrigin::create(frameInfo.securityOrigin.securityOrigin());
            completionHandler(m_client.shouldAllowDeviceOrientationAndMotionAccess(toAPI(&page), toAPI(origin.ptr()), m_client.base.clientInfo));
        }
#endif

        // Printing.
        float headerHeight(WebPageProxy& page, WebFrameProxy& frame) final
        {
            if (!m_client.headerHeight)
                return 0;

            return m_client.headerHeight(toAPI(&page), toAPI(&frame), m_client.base.clientInfo);
        }

        float footerHeight(WebPageProxy& page, WebFrameProxy& frame) final
        {
            if (!m_client.footerHeight)
                return 0;

            return m_client.footerHeight(toAPI(&page), toAPI(&frame), m_client.base.clientInfo);
        }

        void drawHeader(WebPageProxy& page, WebFrameProxy& frame, WebCore::FloatRect&& rect) final
        {
            if (!m_client.drawHeader)
                return;

            m_client.drawHeader(toAPI(&page), toAPI(&frame), toAPI(rect), m_client.base.clientInfo);
        }

        void drawFooter(WebPageProxy& page, WebFrameProxy& frame, WebCore::FloatRect&& rect) final
        {
            if (!m_client.drawFooter)
                return;

            m_client.drawFooter(toAPI(&page), toAPI(&frame), toAPI(rect), m_client.base.clientInfo);
        }

        void printFrame(WebPageProxy& page, WebFrameProxy& frame, const WebCore::FloatSize&, CompletionHandler<void()>&& completionHandler) final
        {
            if (m_client.printFrame)
                m_client.printFrame(toAPI(&page), toAPI(&frame), m_client.base.clientInfo);
            completionHandler();
        }

        bool canRunModal() const final
        {
            return m_client.runModal;
        }

        void runModal(WebPageProxy& page) final
        {
            if (!m_client.runModal)
                return;

            m_client.runModal(toAPI(&page), m_client.base.clientInfo);
        }

        void saveDataToFileInDownloadsFolder(WebPageProxy* page, const String& suggestedFilename, const String& mimeType, const URL& originatingURL, API::Data& data) final
        {
            if (!m_client.saveDataToFileInDownloadsFolder)
                return;

            m_client.saveDataToFileInDownloadsFolder(toAPI(page), toAPI(suggestedFilename.impl()), toAPI(mimeType.impl()), toURLRef(originatingURL.string().impl()), toAPI(&data), m_client.base.clientInfo);
        }

        void pinnedStateDidChange(WebPageProxy& page) final
        {
            if (!m_client.pinnedStateDidChange)
                return;

            m_client.pinnedStateDidChange(toAPI(&page), m_client.base.clientInfo);
        }

        void isPlayingMediaDidChange(WebPageProxy& page) final
        {
            if (!m_client.isPlayingAudioDidChange)
                return;

            m_client.isPlayingAudioDidChange(toAPI(&page), m_client.base.clientInfo);
        }

        void didClickAutoFillButton(WebPageProxy& page, API::Object* userInfo) final
        {
            if (!m_client.didClickAutoFillButton)
                return;

            m_client.didClickAutoFillButton(toAPI(&page), toAPI(userInfo), m_client.base.clientInfo);
        }

        void didResignInputElementStrongPasswordAppearance(WebPageProxy& page, API::Object* userInfo) final
        {
            if (!m_client.didResignInputElementStrongPasswordAppearance)
                return;

            m_client.didResignInputElementStrongPasswordAppearance(toAPI(&page), toAPI(userInfo), m_client.base.clientInfo);
        }

#if ENABLE(POINTER_LOCK)
        void requestPointerLock(WebPageProxy* page) final
        {
            if (!m_client.requestPointerLock)
                return;
            
            m_client.requestPointerLock(toAPI(page), m_client.base.clientInfo);
        }

        void didLosePointerLock(WebPageProxy* page) final
        {
            if (!m_client.didLosePointerLock)
                return;

            m_client.didLosePointerLock(toAPI(page), m_client.base.clientInfo);
        }
#endif

        static WKAutoplayEventFlags toWKAutoplayEventFlags(OptionSet<WebCore::AutoplayEventFlags> flags)
        {
            WKAutoplayEventFlags wkFlags = kWKAutoplayEventFlagsNone;
            if (flags.contains(WebCore::AutoplayEventFlags::HasAudio))
                wkFlags |= kWKAutoplayEventFlagsHasAudio;
            if (flags.contains(WebCore::AutoplayEventFlags::PlaybackWasPrevented))
                wkFlags |= kWKAutoplayEventFlagsPlaybackWasPrevented;
            if (flags.contains(WebCore::AutoplayEventFlags::MediaIsMainContent))
                wkFlags |= kWKAutoplayEventFlagsMediaIsMainContent;

            return wkFlags;
        }

        static WKAutoplayEvent toWKAutoplayEvent(WebCore::AutoplayEvent event)
        {
            switch (event) {
            case WebCore::AutoplayEvent::DidAutoplayMediaPastThresholdWithoutUserInterference:
                return kWKAutoplayEventDidAutoplayMediaPastThresholdWithoutUserInterference;
            case WebCore::AutoplayEvent::DidPlayMediaWithUserGesture:
                return kWKAutoplayEventDidPlayMediaWithUserGesture;
            case WebCore::AutoplayEvent::DidPreventMediaFromPlaying:
                return kWKAutoplayEventDidPreventFromAutoplaying;
            case WebCore::AutoplayEvent::UserDidInterfereWithPlayback:
                return kWKAutoplayEventUserDidInterfereWithPlayback;
            }

            RELEASE_ASSERT_NOT_REACHED();
        }

        void handleAutoplayEvent(WebPageProxy& page, WebCore::AutoplayEvent event, OptionSet<WebCore::AutoplayEventFlags> flags) final
        {
            if (!m_client.handleAutoplayEvent)
                return;

            m_client.handleAutoplayEvent(toAPI(&page), toWKAutoplayEvent(event), toWKAutoplayEventFlags(flags), m_client.base.clientInfo);
        }

#if ENABLE(WEB_AUTHN)
        // The current method is specialized for WebKitTestRunner.
        void runWebAuthenticationPanel(WebPageProxy&, API::WebAuthenticationPanel& panel, WebFrameProxy&, FrameInfoData&&, CompletionHandler<void(WebKit::WebAuthenticationPanelResult)>&& completionHandler) final
        {
            class PanelClient final : public API::WebAuthenticationPanelClient {
            public:
                void selectAssertionResponse(Vector<Ref<WebCore::AuthenticatorAssertionResponse>>&& responses, WebKit::WebAuthenticationSource, CompletionHandler<void(WebCore::AuthenticatorAssertionResponse*)>&& completionHandler) const final
                {
                    ASSERT(!responses.isEmpty());
                    completionHandler(responses[0].ptr());
                }

                void decidePolicyForLocalAuthenticator(CompletionHandler<void(WebKit::LocalAuthenticatorPolicy)>&& completionHandler) const final
                {
                    completionHandler(WebKit::LocalAuthenticatorPolicy::Allow);
                }
            };

            if (!m_client.runWebAuthenticationPanel) {
                completionHandler(WebKit::WebAuthenticationPanelResult::Unavailable);
                return;
            }

            panel.setClient(WTF::makeUniqueRef<PanelClient>());
            completionHandler(WebKit::WebAuthenticationPanelResult::Presented);
        }
#endif
        void decidePolicyForSpeechRecognitionPermissionRequest(WebPageProxy& page, API::SecurityOrigin& origin, CompletionHandler<void(bool)>&& completionHandler) final
        {
            if (!m_client.decidePolicyForSpeechRecognitionPermissionRequest)
                return;

            m_client.decidePolicyForSpeechRecognitionPermissionRequest(toAPI(&page), toAPI(&origin), toAPI(SpeechRecognitionPermissionCallback::create(WTFMove(completionHandler)).ptr()));
        }

        void decidePolicyForMediaKeySystemPermissionRequest(WebPageProxy& page, API::SecurityOrigin& origin, const String& keySystem, CompletionHandler<void(bool)>&& completionHandler) final
        {
            if (!m_client.decidePolicyForMediaKeySystemPermissionRequest) {
                completionHandler(false);
                return;
            }

            m_client.decidePolicyForMediaKeySystemPermissionRequest(toAPI(&page), toAPI(&origin), toAPI(API::String::create(keySystem).ptr()), toAPI(MediaKeySystemPermissionCallback::create(WTFMove(completionHandler)).ptr()));
        }
    };

    toImpl(pageRef)->setUIClient(makeUnique<UIClient>(wkClient));
}

void WKPageSetPageNavigationClient(WKPageRef pageRef, const WKPageNavigationClientBase* wkClient)
{
    class NavigationClient : public API::Client<WKPageNavigationClientBase>, public API::NavigationClient {
    public:
        explicit NavigationClient(const WKPageNavigationClientBase* client)
        {
            initialize(client);
        }

    private:
        void decidePolicyForNavigationAction(WebPageProxy& page, Ref<API::NavigationAction>&& navigationAction, Ref<WebKit::WebFramePolicyListenerProxy>&& listener, API::Object* userData) final
        {
            if (!m_client.decidePolicyForNavigationAction) {
                listener->use();
                return;
            }
            m_client.decidePolicyForNavigationAction(toAPI(&page), toAPI(navigationAction.ptr()), toAPI(listener.ptr()), toAPI(userData), m_client.base.clientInfo);
        }

        void decidePolicyForNavigationResponse(WebPageProxy& page, Ref<API::NavigationResponse>&& navigationResponse, Ref<WebKit::WebFramePolicyListenerProxy>&& listener, API::Object* userData) override
        {
            if (!m_client.decidePolicyForNavigationResponse) {
                listener->use();
                return;
            }
            m_client.decidePolicyForNavigationResponse(toAPI(&page), toAPI(navigationResponse.ptr()), toAPI(listener.ptr()), toAPI(userData), m_client.base.clientInfo);
        }

        void didStartProvisionalNavigation(WebPageProxy& page, const ResourceRequest&, API::Navigation* navigation, API::Object* userData) override
        {
            if (m_client.didStartProvisionalNavigation)
                m_client.didStartProvisionalNavigation(toAPI(&page), toAPI(navigation), toAPI(userData), m_client.base.clientInfo);
        }

        void didReceiveServerRedirectForProvisionalNavigation(WebPageProxy& page, API::Navigation* navigation, API::Object* userData) override
        {
            if (!m_client.didReceiveServerRedirectForProvisionalNavigation)
                return;
            m_client.didReceiveServerRedirectForProvisionalNavigation(toAPI(&page), toAPI(navigation), toAPI(userData), m_client.base.clientInfo);
        }

        void didFailProvisionalNavigationWithError(WebPageProxy& page, FrameInfoData&& frameInfo, API::Navigation* navigation, const WebCore::ResourceError& error, API::Object* userData) override
        {
            if (frameInfo.isMainFrame) {
                if (m_client.didFailProvisionalNavigation)
                    m_client.didFailProvisionalNavigation(toAPI(&page), toAPI(navigation), toAPI(error), toAPI(userData), m_client.base.clientInfo);
            } else {
                if (m_client.didFailProvisionalLoadInSubframe)
                    m_client.didFailProvisionalLoadInSubframe(toAPI(&page), toAPI(navigation), toAPI(API::FrameInfo::create(WTFMove(frameInfo), &page).ptr()), toAPI(error), toAPI(userData), m_client.base.clientInfo);
            }
        }

        void didCommitNavigation(WebPageProxy& page, API::Navigation* navigation, API::Object* userData) override
        {
            if (m_client.didCommitNavigation)
                m_client.didCommitNavigation(toAPI(&page), toAPI(navigation), toAPI(userData), m_client.base.clientInfo);
        }

        void didFinishNavigation(WebPageProxy& page, API::Navigation* navigation, API::Object* userData) override
        {
            if (m_client.didFinishNavigation)
                m_client.didFinishNavigation(toAPI(&page), toAPI(navigation), toAPI(userData), m_client.base.clientInfo);
        }

        void didFailNavigationWithError(WebPageProxy& page, const FrameInfoData&, API::Navigation* navigation, const WebCore::ResourceError& error, API::Object* userData) override
        {
            if (m_client.didFailNavigation)
                m_client.didFailNavigation(toAPI(&page), toAPI(navigation), toAPI(error), toAPI(userData), m_client.base.clientInfo);
        }

        void didFinishDocumentLoad(WebPageProxy& page, API::Navigation* navigation, API::Object* userData) override
        {
            if (!m_client.didFinishDocumentLoad)
                return;
            m_client.didFinishDocumentLoad(toAPI(&page), toAPI(navigation), toAPI(userData), m_client.base.clientInfo);
        }

        void didSameDocumentNavigation(WebPageProxy& page, API::Navigation* navigation, WebKit::SameDocumentNavigationType navigationType, API::Object* userData) override
        {
            if (!m_client.didSameDocumentNavigation)
                return;
            m_client.didSameDocumentNavigation(toAPI(&page), toAPI(navigation), toAPI(navigationType), toAPI(userData), m_client.base.clientInfo);
        }
        
        void renderingProgressDidChange(WebPageProxy& page, OptionSet<WebCore::LayoutMilestone> milestones) override
        {
            if (!m_client.renderingProgressDidChange)
                return;
            m_client.renderingProgressDidChange(toAPI(&page), pageRenderingProgressEvents(milestones), nullptr, m_client.base.clientInfo);
        }
        
        void didReceiveAuthenticationChallenge(WebPageProxy& page, AuthenticationChallengeProxy& authenticationChallenge) override
        {
            if (m_client.canAuthenticateAgainstProtectionSpace && !m_client.canAuthenticateAgainstProtectionSpace(toAPI(&page), toAPI(WebProtectionSpace::create(authenticationChallenge.core().protectionSpace()).ptr()), m_client.base.clientInfo))
                return authenticationChallenge.listener().completeChallenge(WebKit::AuthenticationChallengeDisposition::RejectProtectionSpaceAndContinue);
            if (!m_client.didReceiveAuthenticationChallenge)
                return authenticationChallenge.listener().completeChallenge(WebKit::AuthenticationChallengeDisposition::PerformDefaultHandling);
            m_client.didReceiveAuthenticationChallenge(toAPI(&page), toAPI(&authenticationChallenge), m_client.base.clientInfo);
        }

        bool processDidTerminate(WebPageProxy& page, WebKit::ProcessTerminationReason reason) override
        {
            if (m_client.webProcessDidTerminate) {
                m_client.webProcessDidTerminate(toAPI(&page), toAPI(reason), m_client.base.clientInfo);
                return true;
            }

            if (m_client.webProcessDidCrash && reason != WebKit::ProcessTerminationReason::RequestedByClient) {
                m_client.webProcessDidCrash(toAPI(&page), m_client.base.clientInfo);
                return true;
            }

            return false;
        }

        RefPtr<API::Data> webCryptoMasterKey(WebPageProxy& page) override
        {
            if (m_client.copyWebCryptoMasterKey)
                return adoptRef(toImpl(m_client.copyWebCryptoMasterKey(toAPI(&page), m_client.base.clientInfo)));

            Vector<uint8_t> masterKey;
#if ENABLE(WEB_CRYPTO)
            if (!getDefaultWebCryptoMasterKey(masterKey))
                return nullptr;
#endif

            return API::Data::create(masterKey.data(), masterKey.size());
        }

        RefPtr<API::String> signedPublicKeyAndChallengeString(WebPageProxy& page, unsigned keySizeIndex, const RefPtr<API::String>& challengeString, const URL& url) override
        {
            if (m_client.copySignedPublicKeyAndChallengeString)
                return adoptRef(toImpl(m_client.copySignedPublicKeyAndChallengeString(toAPI(&page), m_client.base.clientInfo)));
            return API::String::create(WebCore::signedPublicKeyAndChallengeString(keySizeIndex, challengeString->string(), url));
        }

        void navigationActionDidBecomeDownload(WebKit::WebPageProxy& page, API::NavigationAction& action, WebKit::DownloadProxy& download) override
        {
            if (!m_client.navigationActionDidBecomeDownload)
                return;
            m_client.navigationActionDidBecomeDownload(toAPI(&page), toAPI(&action), toAPI(&download), m_client.base.clientInfo);
        }

        void navigationResponseDidBecomeDownload(WebKit::WebPageProxy& page, API::NavigationResponse& response, WebKit::DownloadProxy& download) override
        {
            if (!m_client.navigationResponseDidBecomeDownload)
                return;
            m_client.navigationResponseDidBecomeDownload(toAPI(&page), toAPI(&response), toAPI(&download), m_client.base.clientInfo);
        }

        void contextMenuDidCreateDownload(WebKit::WebPageProxy& page, WebKit::DownloadProxy& download) override
        {
            if (!m_client.contextMenuDidCreateDownload)
                return;
            m_client.contextMenuDidCreateDownload(toAPI(&page), toAPI(&download), m_client.base.clientInfo);
        }

        void didBeginNavigationGesture(WebPageProxy& page) override
        {
            if (!m_client.didBeginNavigationGesture)
                return;
            m_client.didBeginNavigationGesture(toAPI(&page), m_client.base.clientInfo);
        }

        void didEndNavigationGesture(WebPageProxy& page, bool willNavigate, WebKit::WebBackForwardListItem& item) override
        {
            if (!m_client.didEndNavigationGesture)
                return;
            m_client.didEndNavigationGesture(toAPI(&page), willNavigate ? toAPI(&item) : nullptr, m_client.base.clientInfo);
        }

        void willEndNavigationGesture(WebPageProxy& page, bool willNavigate, WebKit::WebBackForwardListItem& item) override
        {
            if (!m_client.willEndNavigationGesture)
                return;
            m_client.willEndNavigationGesture(toAPI(&page), willNavigate ? toAPI(&item) : nullptr, m_client.base.clientInfo);
        }

        void didRemoveNavigationGestureSnapshot(WebPageProxy& page) override
        {
            if (!m_client.didRemoveNavigationGestureSnapshot)
                return;
            m_client.didRemoveNavigationGestureSnapshot(toAPI(&page), m_client.base.clientInfo);
        }
        
        void contentRuleListNotification(WebPageProxy& page, URL&& url, ContentRuleListResults&& results) final
        {
            if (!m_client.contentRuleListNotification)
                return;

            Vector<RefPtr<API::Object>> apiListIdentifiers;
            Vector<RefPtr<API::Object>> apiNotifications;
            for (const auto& pair : results.results) {
                const String& listIdentifier = pair.first;
                const auto& result = pair.second;
                for (const String& notification : result.notifications) {
                    apiListIdentifiers.append(API::String::create(listIdentifier));
                    apiNotifications.append(API::String::create(notification));
                }
            }

            if (!apiNotifications.isEmpty())
                m_client.contentRuleListNotification(toAPI(&page), toURLRef(url.string().impl()), toAPI(API::Array::create(WTFMove(apiListIdentifiers)).ptr()), toAPI(API::Array::create(WTFMove(apiNotifications)).ptr()), m_client.base.clientInfo);
        }
    };

    WebPageProxy* webPageProxy = toImpl(pageRef);

    webPageProxy->setNavigationClient(makeUniqueRef<NavigationClient>(wkClient));
}

class StateClient final : public API::Client<WKPageStateClientBase>, public PageLoadState::Observer {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit StateClient(const WKPageStateClientBase* client)
    {
        initialize(client);
    }
private:
    void willChangeIsLoading() override
    {
        if (!m_client.willChangeIsLoading)
            return;
        m_client.willChangeIsLoading(m_client.base.clientInfo);
    }

    void didChangeIsLoading() override
    {
        if (!m_client.didChangeIsLoading)
            return;
        m_client.didChangeIsLoading(m_client.base.clientInfo);
    }

    void willChangeTitle() override
    {
        if (!m_client.willChangeTitle)
            return;
        m_client.willChangeTitle(m_client.base.clientInfo);
    }

    void didChangeTitle() override
    {
        if (!m_client.didChangeTitle)
            return;
        m_client.didChangeTitle(m_client.base.clientInfo);
    }

    void willChangeActiveURL() override
    {
        if (!m_client.willChangeActiveURL)
            return;
        m_client.willChangeActiveURL(m_client.base.clientInfo);
    }

    void didChangeActiveURL() override
    {
        if (!m_client.didChangeActiveURL)
            return;
        m_client.didChangeActiveURL(m_client.base.clientInfo);
    }

    void willChangeHasOnlySecureContent() override
    {
        if (!m_client.willChangeHasOnlySecureContent)
            return;
        m_client.willChangeHasOnlySecureContent(m_client.base.clientInfo);
    }

    void didChangeHasOnlySecureContent() override
    {
        if (!m_client.didChangeHasOnlySecureContent)
            return;
        m_client.didChangeHasOnlySecureContent(m_client.base.clientInfo);
    }

    void willChangeEstimatedProgress() override
    {
        if (!m_client.willChangeEstimatedProgress)
            return;
        m_client.willChangeEstimatedProgress(m_client.base.clientInfo);
    }

    void didChangeEstimatedProgress() override
    {
        if (!m_client.didChangeEstimatedProgress)
            return;
        m_client.didChangeEstimatedProgress(m_client.base.clientInfo);
    }

    void willChangeCanGoBack() override
    {
        if (!m_client.willChangeCanGoBack)
            return;
        m_client.willChangeCanGoBack(m_client.base.clientInfo);
    }

    void didChangeCanGoBack() override
    {
        if (!m_client.didChangeCanGoBack)
            return;
        m_client.didChangeCanGoBack(m_client.base.clientInfo);
    }

    void willChangeCanGoForward() override
    {
        if (!m_client.willChangeCanGoForward)
            return;
        m_client.willChangeCanGoForward(m_client.base.clientInfo);
    }

    void didChangeCanGoForward() override
    {
        if (!m_client.didChangeCanGoForward)
            return;
        m_client.didChangeCanGoForward(m_client.base.clientInfo);
    }

    void willChangeNetworkRequestsInProgress() override
    {
        if (!m_client.willChangeNetworkRequestsInProgress)
            return;
        m_client.willChangeNetworkRequestsInProgress(m_client.base.clientInfo);
    }

    void didChangeNetworkRequestsInProgress() override
    {
        if (!m_client.didChangeNetworkRequestsInProgress)
            return;
        m_client.didChangeNetworkRequestsInProgress(m_client.base.clientInfo);
    }

    void willChangeCertificateInfo() override
    {
        if (!m_client.willChangeCertificateInfo)
            return;
        m_client.willChangeCertificateInfo(m_client.base.clientInfo);
    }

    void didChangeCertificateInfo() override
    {
        if (!m_client.didChangeCertificateInfo)
            return;
        m_client.didChangeCertificateInfo(m_client.base.clientInfo);
    }

    void willChangeWebProcessIsResponsive() override
    {
        if (!m_client.willChangeWebProcessIsResponsive)
            return;
        m_client.willChangeWebProcessIsResponsive(m_client.base.clientInfo);
    }

    void didChangeWebProcessIsResponsive() override
    {
        if (!m_client.didChangeWebProcessIsResponsive)
            return;
        m_client.didChangeWebProcessIsResponsive(m_client.base.clientInfo);
    }

    void didSwapWebProcesses() override
    {
        if (!m_client.didSwapWebProcesses)
            return;
        m_client.didSwapWebProcesses(m_client.base.clientInfo);
    }
};

void WKPageSetPageStateClient(WKPageRef page, WKPageStateClientBase* client)
{
    if (client)
        toImpl(page)->setPageLoadStateObserver(makeUnique<StateClient>(client));
    else
        toImpl(page)->setPageLoadStateObserver(nullptr);
}

void WKPageRunJavaScriptInMainFrame(WKPageRef pageRef, WKStringRef scriptRef, void* context, WKPageRunJavaScriptFunction callback)
{
    toImpl(pageRef)->runJavaScriptInMainFrame({ toImpl(scriptRef)->string(), URL { }, false, WTF::nullopt, true }, [context, callback] (auto&& result) {
        if (result.has_value())
            callback(toAPI(result.value().get()), nullptr, context);
        else
            callback(nullptr, nullptr, context);
    });
}

#ifdef __BLOCKS__
static void callRunJavaScriptBlockAndRelease(WKSerializedScriptValueRef resultValue, WKErrorRef error, void* context)
{
    WKPageRunJavaScriptBlock block = (WKPageRunJavaScriptBlock)context;
    block(resultValue, error);
    Block_release(block);
}

void WKPageRunJavaScriptInMainFrame_b(WKPageRef pageRef, WKStringRef scriptRef, WKPageRunJavaScriptBlock block)
{
    WKPageRunJavaScriptInMainFrame(pageRef, scriptRef, Block_copy(block), callRunJavaScriptBlockAndRelease);
}
#endif

static CompletionHandler<void(const String&)> toStringCallback(void* context, void(*callback)(WKStringRef, WKErrorRef, void*))
{
    return [context, callback] (const String& returnValue) {
        callback(toAPI(API::String::create(returnValue).ptr()), nullptr, context);
    };
}

void WKPageRenderTreeExternalRepresentation(WKPageRef pageRef, void* context, WKPageRenderTreeExternalRepresentationFunction callback)
{
    toImpl(pageRef)->getRenderTreeExternalRepresentation(toStringCallback(context, callback));
}

void WKPageGetSourceForFrame(WKPageRef pageRef, WKFrameRef frameRef, void* context, WKPageGetSourceForFrameFunction callback)
{
    toImpl(pageRef)->getSourceForFrame(toImpl(frameRef), toStringCallback(context, callback));
}

void WKPageGetContentsAsString(WKPageRef pageRef, void* context, WKPageGetContentsAsStringFunction callback)
{
    toImpl(pageRef)->getContentsAsString(ContentAsStringIncludesChildFrames::No, toStringCallback(context, callback));
}

void WKPageGetBytecodeProfile(WKPageRef pageRef, void* context, WKPageGetBytecodeProfileFunction callback)
{
    toImpl(pageRef)->getBytecodeProfile(toStringCallback(context, callback));
}

void WKPageGetSamplingProfilerOutput(WKPageRef pageRef, void* context, WKPageGetSamplingProfilerOutputFunction callback)
{
    toImpl(pageRef)->getSamplingProfilerOutput(toStringCallback(context, callback));
}

void WKPageGetSelectionAsWebArchiveData(WKPageRef pageRef, void* context, WKPageGetSelectionAsWebArchiveDataFunction callback)
{
    toImpl(pageRef)->getSelectionAsWebArchiveData([context, callback] (API::Data* data) {
        callback(toAPI(data), nullptr, context);
    });
}

void WKPageGetContentsAsMHTMLData(WKPageRef pageRef, void* context, WKPageGetContentsAsMHTMLDataFunction callback)
{
#if ENABLE(MHTML)
    toImpl(pageRef)->getContentsAsMHTMLData([context, callback] (API::Data* data) {
        callback(toAPI(data), nullptr, context);
    });
#else
    UNUSED_PARAM(pageRef);
    UNUSED_PARAM(context);
    UNUSED_PARAM(callback);
#endif
}

void WKPageForceRepaint(WKPageRef pageRef, void* context, WKPageForceRepaintFunction callback)
{
    toImpl(pageRef)->forceRepaint([context, callback]() {
        callback(nullptr, context);
    });
}

WK_EXPORT WKURLRef WKPageCopyPendingAPIRequestURL(WKPageRef pageRef)
{
    const String& pendingAPIRequestURL = toImpl(pageRef)->pageLoadState().pendingAPIRequestURL();

    if (pendingAPIRequestURL.isNull())
        return nullptr;

    return toCopiedURLAPI(pendingAPIRequestURL);
}

WKURLRef WKPageCopyActiveURL(WKPageRef pageRef)
{
    return toCopiedURLAPI(toImpl(pageRef)->pageLoadState().activeURL());
}

WKURLRef WKPageCopyProvisionalURL(WKPageRef pageRef)
{
    return toCopiedURLAPI(toImpl(pageRef)->pageLoadState().provisionalURL());
}

WKURLRef WKPageCopyCommittedURL(WKPageRef pageRef)
{
    return toCopiedURLAPI(toImpl(pageRef)->pageLoadState().url());
}

WKStringRef WKPageCopyStandardUserAgentWithApplicationName(WKStringRef applicationName)
{
    return toCopiedAPI(WebPageProxy::standardUserAgent(toImpl(applicationName)->string()));
}

void WKPageValidateCommand(WKPageRef pageRef, WKStringRef command, void* context, WKPageValidateCommandCallback callback)
{
    auto commandName = toImpl(command)->string();
    toImpl(pageRef)->validateCommand(commandName, [context, callback, commandName](bool isEnabled, int32_t state) {
        callback(toAPI(API::String::create(commandName).ptr()), isEnabled, state, nullptr, context);
    });
}

void WKPageExecuteCommand(WKPageRef pageRef, WKStringRef command)
{
    toImpl(pageRef)->executeEditCommand(toImpl(command)->string());
}

static PrintInfo printInfoFromWKPrintInfo(const WKPrintInfo& printInfo)
{
    PrintInfo result;
    result.pageSetupScaleFactor = printInfo.pageSetupScaleFactor;
    result.availablePaperWidth = printInfo.availablePaperWidth;
    result.availablePaperHeight = printInfo.availablePaperHeight;
    return result;
}

void WKPageComputePagesForPrinting(WKPageRef page, WKFrameRef frame, WKPrintInfo printInfo, WKPageComputePagesForPrintingFunction callback, void* context)
{
    toImpl(page)->computePagesForPrinting(toImpl(frame), printInfoFromWKPrintInfo(printInfo), [context, callback](const Vector<WebCore::IntRect>& rects, double scaleFactor, const WebCore::FloatBoxExtent& computedPageMargin) {
        Vector<WKRect> wkRects(rects.size());
        for (size_t i = 0; i < rects.size(); ++i)
            wkRects[i] = toAPI(rects[i]);
        callback(wkRects.data(), wkRects.size(), scaleFactor, nullptr, context);
    });
}

#if PLATFORM(COCOA)
void WKPageDrawPagesToPDF(WKPageRef page, WKFrameRef frame, WKPrintInfo printInfo, uint32_t first, uint32_t count, WKPageDrawToPDFFunction callback, void* context)
{
    toImpl(page)->drawPagesToPDF(toImpl(frame), printInfoFromWKPrintInfo(printInfo), first, count, [context, callback] (API::Data* data) {
        callback(toAPI(data), nullptr, context);
    });
}
#endif

void WKPageBeginPrinting(WKPageRef page, WKFrameRef frame, WKPrintInfo printInfo)
{
    toImpl(page)->beginPrinting(toImpl(frame), printInfoFromWKPrintInfo(printInfo));
}

void WKPageEndPrinting(WKPageRef page)
{
    toImpl(page)->endPrinting();
}

bool WKPageGetIsControlledByAutomation(WKPageRef page)
{
    return toImpl(page)->isControlledByAutomation();
}

void WKPageSetControlledByAutomation(WKPageRef page, bool controlled)
{
    toImpl(page)->setControlledByAutomation(controlled);
}

bool WKPageGetAllowsRemoteInspection(WKPageRef page)
{
#if ENABLE(REMOTE_INSPECTOR)
    return toImpl(page)->allowsRemoteInspection();
#else
    UNUSED_PARAM(page);
    return false;
#endif    
}

void WKPageSetAllowsRemoteInspection(WKPageRef page, bool allow)
{
#if ENABLE(REMOTE_INSPECTOR)
    toImpl(page)->setAllowsRemoteInspection(allow);
#else
    UNUSED_PARAM(page);
    UNUSED_PARAM(allow);
#endif
}

void WKPageSetMediaVolume(WKPageRef page, float volume)
{
    toImpl(page)->setMediaVolume(volume);    
}

void WKPageSetMuted(WKPageRef page, WKMediaMutedState muted)
{
    toImpl(page)->setMuted(muted);
}

void WKPageSetMediaCaptureEnabled(WKPageRef page, bool enabled)
{
    toImpl(page)->setMediaCaptureEnabled(enabled);
}

bool WKPageGetMediaCaptureEnabled(WKPageRef page)
{
    return toImpl(page)->mediaCaptureEnabled();
}

void WKPageDidAllowPointerLock(WKPageRef page)
{
#if ENABLE(POINTER_LOCK)
    toImpl(page)->didAllowPointerLock();
#else
    UNUSED_PARAM(page);
#endif
}

void WKPageClearUserMediaState(WKPageRef page)
{
#if ENABLE(MEDIA_STREAM)
    toImpl(page)->clearUserMediaState();
#else
    UNUSED_PARAM(page);
#endif
}

void WKPageDidDenyPointerLock(WKPageRef page)
{
#if ENABLE(POINTER_LOCK)
    toImpl(page)->didDenyPointerLock();
#else
    UNUSED_PARAM(page);
#endif
}

void WKPagePostMessageToInjectedBundle(WKPageRef pageRef, WKStringRef messageNameRef, WKTypeRef messageBodyRef)
{
    toImpl(pageRef)->postMessageToInjectedBundle(toImpl(messageNameRef)->string(), toImpl(messageBodyRef));
}

WKArrayRef WKPageCopyRelatedPages(WKPageRef pageRef)
{
    Vector<RefPtr<API::Object>> relatedPages;

    for (auto& page : toImpl(pageRef)->process().pages()) {
        if (page != toImpl(pageRef))
            relatedPages.append(page);
    }

    return toAPI(&API::Array::create(WTFMove(relatedPages)).leakRef());
}

WKFrameRef WKPageLookUpFrameFromHandle(WKPageRef pageRef, WKFrameHandleRef handleRef)
{
    auto page = toImpl(pageRef);
    auto frame = page->process().webFrame(toImpl(handleRef)->frameID());
    if (!frame || frame->page() != page)
        return nullptr;

    return toAPI(frame);
}

void WKPageSetMayStartMediaWhenInWindow(WKPageRef pageRef, bool mayStartMedia)
{
    toImpl(pageRef)->setMayStartMediaWhenInWindow(mayStartMedia);
}


void WKPageSelectContextMenuItem(WKPageRef page, WKContextMenuItemRef item)
{
#if ENABLE(CONTEXT_MENUS)
    toImpl(page)->contextMenuItemSelected((toImpl(item)->data()));
#else
    UNUSED_PARAM(page);
    UNUSED_PARAM(item);
#endif
}

WKScrollPinningBehavior WKPageGetScrollPinningBehavior(WKPageRef page)
{
    ScrollPinningBehavior pinning = toImpl(page)->scrollPinningBehavior();
    
    switch (pinning) {
    case WebCore::ScrollPinningBehavior::DoNotPin:
        return kWKScrollPinningBehaviorDoNotPin;
    case WebCore::ScrollPinningBehavior::PinToTop:
        return kWKScrollPinningBehaviorPinToTop;
    case WebCore::ScrollPinningBehavior::PinToBottom:
        return kWKScrollPinningBehaviorPinToBottom;
    }
    
    ASSERT_NOT_REACHED();
    return kWKScrollPinningBehaviorDoNotPin;
}

void WKPageSetScrollPinningBehavior(WKPageRef page, WKScrollPinningBehavior pinning)
{
    ScrollPinningBehavior corePinning = ScrollPinningBehavior::DoNotPin;

    switch (pinning) {
    case kWKScrollPinningBehaviorDoNotPin:
        corePinning = ScrollPinningBehavior::DoNotPin;
        break;
    case kWKScrollPinningBehaviorPinToTop:
        corePinning = ScrollPinningBehavior::PinToTop;
        break;
    case kWKScrollPinningBehaviorPinToBottom:
        corePinning = ScrollPinningBehavior::PinToBottom;
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    
    toImpl(page)->setScrollPinningBehavior(corePinning);
}

bool WKPageGetAddsVisitedLinks(WKPageRef page)
{
    return toImpl(page)->addsVisitedLinks();
}

void WKPageSetAddsVisitedLinks(WKPageRef page, bool addsVisitedLinks)
{
    toImpl(page)->setAddsVisitedLinks(addsVisitedLinks);
}

bool WKPageIsPlayingAudio(WKPageRef page)
{
    return toImpl(page)->isPlayingAudio();
}

WKMediaState WKPageGetMediaState(WKPageRef page)
{
    WebCore::MediaProducer::MediaStateFlags coreState = toImpl(page)->reportedMediaState();
    WKMediaState state = kWKMediaIsNotPlaying;

    if (coreState & WebCore::MediaProducer::IsPlayingAudio)
        state |= kWKMediaIsPlayingAudio;
    if (coreState & WebCore::MediaProducer::IsPlayingVideo)
        state |= kWKMediaIsPlayingVideo;
    if (coreState & WebCore::MediaProducer::HasActiveAudioCaptureDevice)
        state |= kWKMediaHasActiveAudioCaptureDevice;
    if (coreState & WebCore::MediaProducer::HasActiveVideoCaptureDevice)
        state |= kWKMediaHasActiveVideoCaptureDevice;
    if (coreState & WebCore::MediaProducer::HasMutedAudioCaptureDevice)
        state |= kWKMediaHasMutedAudioCaptureDevice;
    if (coreState & WebCore::MediaProducer::HasMutedVideoCaptureDevice)
        state |= kWKMediaHasMutedVideoCaptureDevice;
    if (coreState & WebCore::MediaProducer::HasActiveDisplayCaptureDevice)
        state |= kWKMediaHasActiveDisplayCaptureDevice;
    if (coreState & WebCore::MediaProducer::HasMutedDisplayCaptureDevice)
        state |= kWKMediaHasMutedDisplayCaptureDevice;

    return state;
}

void WKPageClearWheelEventTestMonitor(WKPageRef pageRef)
{
    toImpl(pageRef)->clearWheelEventTestMonitor();
}

void WKPageCallAfterNextPresentationUpdate(WKPageRef pageRef, void* context, WKPagePostPresentationUpdateFunction callback)
{
    toImpl(pageRef)->callAfterNextPresentationUpdate([context, callback](WebKit::CallbackBase::Error error) {
        callback(error != WebKit::CallbackBase::Error::None ? toAPI(API::Error::create().ptr()) : 0, context);
    });
}

void WKPageSetIgnoresViewportScaleLimits(WKPageRef page, bool ignoresViewportScaleLimits)
{
#if ENABLE(META_VIEWPORT)
    toImpl(page)->setForceAlwaysUserScalable(ignoresViewportScaleLimits);
#endif
}

ProcessID WKPageGetProcessIdentifier(WKPageRef page)
{
    return toImpl(page)->processIdentifier();
}

#ifdef __BLOCKS__
void WKPageGetApplicationManifest_b(WKPageRef page, WKPageGetApplicationManifestBlock block)
{
#if ENABLE(APPLICATION_MANIFEST)
    toImpl(page)->getApplicationManifest([block](const Optional<WebCore::ApplicationManifest>& manifest) {
        block();
    });
#else // ENABLE(APPLICATION_MANIFEST)
    UNUSED_PARAM(page);
    block();
#endif // not ENABLE(APPLICATION_MANIFEST)
}
#endif

void WKPageDumpPrivateClickMeasurement(WKPageRef page, WKPageDumpPrivateClickMeasurementFunction callback, void* callbackContext)
{
    toImpl(page)->dumpPrivateClickMeasurement([callbackContext, callback] (const String& privateClickMeasurement) {
        callback(WebKit::toAPI(privateClickMeasurement.impl()), callbackContext);
    });
}

void WKPageClearPrivateClickMeasurement(WKPageRef page, WKPageClearPrivateClickMeasurementFunction callback, void* callbackContext)
{
    toImpl(page)->clearPrivateClickMeasurement([callbackContext, callback] () {
        callback(callbackContext);
    });
}

void WKPageSetPrivateClickMeasurementOverrideTimerForTesting(WKPageRef page, bool value, WKPageSetPrivateClickMeasurementOverrideTimerForTestingFunction callback, void* callbackContext)
{
    toImpl(page)->setPrivateClickMeasurementOverrideTimerForTesting(value, [callbackContext, callback] () {
        callback(callbackContext);
    });
}

void WKPageMarkAttributedPrivateClickMeasurementsAsExpiredForTesting(WKPageRef page, WKPageMarkAttributedPrivateClickMeasurementsAsExpiredForTestingFunction callback, void* callbackContext)
{
    toImpl(page)->markAttributedPrivateClickMeasurementsAsExpiredForTesting([callbackContext, callback] () {
        callback(callbackContext);
    });
}

void WKPageSimulateResourceLoadStatisticsSessionRestart(WKPageRef page, WKPageSimulateResourceLoadStatisticsSessionRestartFunction callback, void* callbackContext)
{
    toImpl(page)->simulateResourceLoadStatisticsSessionRestart([callbackContext, callback] () {
        callback(callbackContext);
    });
}

void WKPageSetPrivateClickMeasurementTokenPublicKeyURLForTesting(WKPageRef page, WKURLRef URLRef, WKPageSetPrivateClickMeasurementTokenPublicKeyURLForTestingFunction callback, void* callbackContext)
{
    toImpl(page)->setPrivateClickMeasurementTokenPublicKeyURLForTesting(URL(URL(), toWTFString(URLRef)), [callbackContext, callback] () {
        callback(callbackContext);
    });
}

void WKPageSetPrivateClickMeasurementTokenSignatureURLForTesting(WKPageRef page, WKURLRef URLRef, WKPageSetPrivateClickMeasurementTokenSignatureURLForTestingFunction callback, void* callbackContext)
{
    toImpl(page)->setPrivateClickMeasurementTokenSignatureURLForTesting(URL(URL(), toWTFString(URLRef)), [callbackContext, callback] () {
        callback(callbackContext);
    });
}

void WKPageSetPrivateClickMeasurementAttributionReportURLForTesting(WKPageRef page, WKURLRef URLRef, WKPageSetPrivateClickMeasurementAttributionReportURLForTestingFunction callback, void* callbackContext)
{
    toImpl(page)->setPrivateClickMeasurementAttributionReportURLForTesting(URL(URL(), toWTFString(URLRef)), [callbackContext, callback] () {
        callback(callbackContext);
    });
}

void WKPageMarkPrivateClickMeasurementsAsExpiredForTesting(WKPageRef page, WKPageMarkPrivateClickMeasurementsAsExpiredForTestingFunction callback, void* callbackContext)
{
    toImpl(page)->markPrivateClickMeasurementsAsExpiredForTesting([callbackContext, callback] () {
        callback(callbackContext);
    });
}

void WKPageSetFraudPreventionValuesForTesting(WKPageRef page, WKStringRef secretToken, WKStringRef unlinkableToken, WKStringRef signature, WKStringRef keyID, WKPageSetFraudPreventionValuesForTestingFunction callback, void* callbackContext)
{
    toImpl(page)->setFraudPreventionValuesForTesting(toWTFString(secretToken), toWTFString(unlinkableToken), toWTFString(signature), toWTFString(keyID), [callbackContext, callback] () {
        callback(callbackContext);
    });
}

void WKPageSetMockCameraOrientation(WKPageRef page, uint64_t orientation)
{
#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
    toImpl(page)->setOrientationForMediaCapture(orientation);
#endif
}

WK_EXPORT bool WKPageIsMockRealtimeMediaSourceCenterEnabled(WKPageRef)
{
#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
    return MockRealtimeMediaSourceCenter::mockRealtimeMediaSourceCenterEnabled();
#else
    return false;
#endif
}

void WKPageLoadedSubresourceDomains(WKPageRef page, WKPageLoadedSubresourceDomainsFunction callback, void* callbackContext)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    toImpl(page)->getLoadedSubresourceDomains([callbackContext, callback](Vector<RegistrableDomain>&& domains) {
        Vector<RefPtr<API::Object>> apiDomains = WTF::map(domains, [](auto& domain) {
            return RefPtr<API::Object>(API::String::create(WTFMove(domain.string())));
        });
        callback(toAPI(API::Array::create(WTFMove(apiDomains)).ptr()), callbackContext);
    });
#else
    UNUSED_PARAM(page);
    callback(nullptr, callbackContext);
#endif
}

void WKPageClearLoadedSubresourceDomains(WKPageRef page)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    toImpl(page)->clearLoadedSubresourceDomains();
#else
    UNUSED_PARAM(page);
#endif
}

void WKPageSetMediaCaptureReportingDelayForTesting(WKPageRef page, double delay)
{
    toImpl(page)->setMediaCaptureReportingDelay(Seconds(delay));
}

void WKPageDispatchActivityStateUpdateForTesting(WKPageRef page)
{
    toImpl(page)->dispatchActivityStateUpdateForTesting();
}
