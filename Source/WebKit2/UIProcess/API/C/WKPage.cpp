/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "APIData.h"
#include "APIFindClient.h"
#include "APILoaderClient.h"
#include "APIPolicyClient.h"
#include "APISessionState.h"
#include "APIUIClient.h"
#include "ImmutableDictionary.h"
#include "LegacySessionStateCoding.h"
#include "NativeWebKeyboardEvent.h"
#include "NativeWebWheelEvent.h"
#include "NavigationActionData.h"
#include "PluginInformation.h"
#include "PrintInfo.h"
#include "WKAPICast.h"
#include "WKPagePolicyClientInternal.h"
#include "WKPluginInformation.h"
#include "WebBackForwardList.h"
#include "WebContext.h"
#include "WebFormClient.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <WebCore/Page.h>
#include <WebCore/WindowFeatures.h>

#ifdef __BLOCKS__
#include <Block.h>
#endif

#if ENABLE(CONTEXT_MENUS)
#include "WebContextMenuItem.h"
#endif

using namespace WebCore;
using namespace WebKit;

namespace API {
template<> struct ClientTraits<WKPageLoaderClientBase> {
    typedef std::tuple<WKPageLoaderClientV0, WKPageLoaderClientV1, WKPageLoaderClientV2, WKPageLoaderClientV3, WKPageLoaderClientV4, WKPageLoaderClientV5> Versions;
};

template<> struct ClientTraits<WKPagePolicyClientBase> {
    typedef std::tuple<WKPagePolicyClientV0, WKPagePolicyClientV1, WKPagePolicyClientInternal> Versions;
};

template<> struct ClientTraits<WKPageUIClientBase> {
    typedef std::tuple<WKPageUIClientV0, WKPageUIClientV1, WKPageUIClientV2, WKPageUIClientV3> Versions;
};

}

WKTypeID WKPageGetTypeID()
{
    return toAPI(WebPageProxy::APIType);
}

WKContextRef WKPageGetContext(WKPageRef pageRef)
{
    return toAPI(&toImpl(pageRef)->process().context());
}

WKPageGroupRef WKPageGetPageGroup(WKPageRef pageRef)
{
    return toAPI(&toImpl(pageRef)->pageGroup());
}

void WKPageLoadURL(WKPageRef pageRef, WKURLRef URLRef)
{
    toImpl(pageRef)->loadRequest(URL(URL(), toWTFString(URLRef)));
}

void WKPageLoadURLWithUserData(WKPageRef pageRef, WKURLRef URLRef, WKTypeRef userDataRef)
{
    toImpl(pageRef)->loadRequest(URL(URL(), toWTFString(URLRef)), toImpl(userDataRef));
}

void WKPageLoadURLRequest(WKPageRef pageRef, WKURLRequestRef urlRequestRef)
{
    toImpl(pageRef)->loadRequest(toImpl(urlRequestRef)->resourceRequest());
}

void WKPageLoadURLRequestWithUserData(WKPageRef pageRef, WKURLRequestRef urlRequestRef, WKTypeRef userDataRef)
{
    toImpl(pageRef)->loadRequest(toImpl(urlRequestRef)->resourceRequest(), toImpl(userDataRef));
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
    toImpl(pageRef)->loadData(toImpl(dataRef), toWTFString(MIMETypeRef), toWTFString(encodingRef), toWTFString(baseURLRef));
}

void WKPageLoadDataWithUserData(WKPageRef pageRef, WKDataRef dataRef, WKStringRef MIMETypeRef, WKStringRef encodingRef, WKURLRef baseURLRef, WKTypeRef userDataRef)
{
    toImpl(pageRef)->loadData(toImpl(dataRef), toWTFString(MIMETypeRef), toWTFString(encodingRef), toWTFString(baseURLRef), toImpl(userDataRef));
}

void WKPageLoadHTMLString(WKPageRef pageRef, WKStringRef htmlStringRef, WKURLRef baseURLRef)
{
    toImpl(pageRef)->loadHTMLString(toWTFString(htmlStringRef), toWTFString(baseURLRef));
}

void WKPageLoadHTMLStringWithUserData(WKPageRef pageRef, WKStringRef htmlStringRef, WKURLRef baseURLRef, WKTypeRef userDataRef)
{
    toImpl(pageRef)->loadHTMLString(toWTFString(htmlStringRef), toWTFString(baseURLRef), toImpl(userDataRef));
}

void WKPageLoadAlternateHTMLString(WKPageRef pageRef, WKStringRef htmlStringRef, WKURLRef baseURLRef, WKURLRef unreachableURLRef)
{
    toImpl(pageRef)->loadAlternateHTMLString(toWTFString(htmlStringRef), toWTFString(baseURLRef), toWTFString(unreachableURLRef));
}

void WKPageLoadAlternateHTMLStringWithUserData(WKPageRef pageRef, WKStringRef htmlStringRef, WKURLRef baseURLRef, WKURLRef unreachableURLRef, WKTypeRef userDataRef)
{
    toImpl(pageRef)->loadAlternateHTMLString(toWTFString(htmlStringRef), toWTFString(baseURLRef), toWTFString(unreachableURLRef), toImpl(userDataRef));
}

void WKPageLoadPlainTextString(WKPageRef pageRef, WKStringRef plainTextStringRef)
{
    toImpl(pageRef)->loadPlainTextString(toWTFString(plainTextStringRef));    
}

void WKPageLoadPlainTextStringWithUserData(WKPageRef pageRef, WKStringRef plainTextStringRef, WKTypeRef userDataRef)
{
    toImpl(pageRef)->loadPlainTextString(toWTFString(plainTextStringRef), toImpl(userDataRef));    
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
    toImpl(pageRef)->reload(false);
}

void WKPageReloadFromOrigin(WKPageRef pageRef)
{
    toImpl(pageRef)->reload(true);
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
    toImpl(pageRef)->goBack();
}

bool WKPageCanGoBack(WKPageRef pageRef)
{
    return toImpl(pageRef)->backForwardList().backItem();
}

void WKPageGoToBackForwardListItem(WKPageRef pageRef, WKBackForwardListItemRef itemRef)
{
    toImpl(pageRef)->goToBackForwardItem(toImpl(itemRef));
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
    return toAPI(toImpl(pageRef)->frameSetLargestFrame());
}

uint64_t WKPageGetRenderTreeSize(WKPageRef page)
{
    return toImpl(page)->renderTreeSize();
}

WKInspectorRef WKPageGetInspector(WKPageRef pageRef)
{
#if defined(ENABLE_INSPECTOR) && ENABLE_INSPECTOR
    return toAPI(toImpl(pageRef)->inspector());
#else
    UNUSED_PARAM(pageRef);
    return 0;
#endif
}

WKVibrationRef WKPageGetVibration(WKPageRef page)
{
#if ENABLE(VIBRATION)
    return toAPI(toImpl(page)->vibration());
#else
    UNUSED_PARAM(page);
    return 0;
#endif
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
    toImpl(pageRef)->terminateProcess();
}

WKStringRef WKPageGetSessionHistoryURLValueType()
{
    static API::String* sessionHistoryURLValueType = API::String::create("SessionHistoryURL").leakRef();
    return toAPI(sessionHistoryURLValueType);
}

WKStringRef WKPageGetSessionBackForwardListItemValueType()
{
    static API::String* sessionBackForwardListValueType = API::String::create("SessionBackForwardListItem").leakRef();
    return toAPI(sessionBackForwardListValueType);
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

    if (shouldReturnData)
        return toAPI(encodeLegacySessionState(sessionState).release().leakRef());

    return toAPI(API::SessionState::create(WTF::move(sessionState)).leakRef());
}

void WKPageRestoreFromSessionState(WKPageRef pageRef, WKTypeRef sessionStateRef)
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

    toImpl(pageRef)->restoreFromSessionState(WTF::move(sessionState), true);
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
    Pagination::Mode mode;
    switch (paginationMode) {
    case kWKPaginationModeUnpaginated:
        mode = Pagination::Unpaginated;
        break;
    case kWKPaginationModeLeftToRight:
        mode = Pagination::LeftToRightPaginated;
        break;
    case kWKPaginationModeRightToLeft:
        mode = Pagination::RightToLeftPaginated;
        break;
    case kWKPaginationModeTopToBottom:
        mode = Pagination::TopToBottomPaginated;
        break;
    case kWKPaginationModeBottomToTop:
        mode = Pagination::BottomToTopPaginated;
        break;
    default:
        return;
    }
    toImpl(pageRef)->setPaginationMode(mode);
}

WKPaginationMode WKPageGetPaginationMode(WKPageRef pageRef)
{
    switch (toImpl(pageRef)->paginationMode()) {
    case Pagination::Unpaginated:
        return kWKPaginationModeUnpaginated;
    case Pagination::LeftToRightPaginated:
        return kWKPaginationModeLeftToRight;
    case Pagination::RightToLeftPaginated:
        return kWKPaginationModeRightToLeft;
    case Pagination::TopToBottomPaginated:
        return kWKPaginationModeTopToBottom;
    case Pagination::BottomToTopPaginated:
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
    toImpl(pageRef)->initializeContextMenuClient(wkClient);
#else
    UNUSED_PARAM(pageRef);
    UNUSED_PARAM(wkClient);
#endif
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
        virtual void didFindString(WebPageProxy* page, const String& string, uint32_t matchCount, int32_t) override
        {
            if (!m_client.didFindString)
                return;
            
            m_client.didFindString(toAPI(page), toAPI(string.impl()), matchCount, m_client.base.clientInfo);
        }

        virtual void didFailToFindString(WebPageProxy* page, const String& string) override
        {
            if (!m_client.didFailToFindString)
                return;
            
            m_client.didFailToFindString(toAPI(page), toAPI(string.impl()), m_client.base.clientInfo);
        }

        virtual void didCountStringMatches(WebPageProxy* page, const String& string, uint32_t matchCount) override
        {
            if (!m_client.didCountStringMatches)
                return;

            m_client.didCountStringMatches(toAPI(page), toAPI(string.impl()), matchCount, m_client.base.clientInfo);
        }
    };

    toImpl(pageRef)->setFindClient(std::make_unique<FindClient>(wkClient));
}

void WKPageSetPageFindMatchesClient(WKPageRef pageRef, const WKPageFindMatchesClientBase* wkClient)
{
    toImpl(pageRef)->initializeFindMatchesClient(wkClient);
}

void WKPageSetPageFormClient(WKPageRef pageRef, const WKPageFormClientBase* wkClient)
{
    toImpl(pageRef)->setFormClient(std::make_unique<WebFormClient>(wkClient));
}

void WKPageSetPageLoaderClient(WKPageRef pageRef, const WKPageLoaderClientBase* wkClient)
{
    class LoaderClient : public API::Client<WKPageLoaderClientBase>, public API::LoaderClient {
    public:
        explicit LoaderClient(const WKPageLoaderClientBase* client)
        {
            initialize(client);
        }

    private:
        virtual void didStartProvisionalLoadForFrame(WebPageProxy* page, WebFrameProxy* frame, uint64_t, API::Object* userData) override
        {
            if (!m_client.didStartProvisionalLoadForFrame)
                return;

            m_client.didStartProvisionalLoadForFrame(toAPI(page), toAPI(frame), toAPI(userData), m_client.base.clientInfo);
        }

        virtual void didReceiveServerRedirectForProvisionalLoadForFrame(WebPageProxy* page, WebFrameProxy* frame, uint64_t, API::Object* userData) override
        {
            if (!m_client.didReceiveServerRedirectForProvisionalLoadForFrame)
                return;

            m_client.didReceiveServerRedirectForProvisionalLoadForFrame(toAPI(page), toAPI(frame), toAPI(userData), m_client.base.clientInfo);
        }

        virtual void didFailProvisionalLoadWithErrorForFrame(WebPageProxy* page, WebFrameProxy* frame, uint64_t, const ResourceError& error, API::Object* userData) override
        {
            if (!m_client.didFailProvisionalLoadWithErrorForFrame)
                return;

            m_client.didFailProvisionalLoadWithErrorForFrame(toAPI(page), toAPI(frame), toAPI(error), toAPI(userData), m_client.base.clientInfo);
        }

        virtual void didCommitLoadForFrame(WebPageProxy* page, WebFrameProxy* frame, uint64_t, API::Object* userData) override
        {
            if (!m_client.didCommitLoadForFrame)
                return;

            m_client.didCommitLoadForFrame(toAPI(page), toAPI(frame), toAPI(userData), m_client.base.clientInfo);
        }

        virtual void didFinishDocumentLoadForFrame(WebPageProxy* page, WebFrameProxy* frame, uint64_t, API::Object* userData) override
        {
            if (!m_client.didFinishDocumentLoadForFrame)
                return;

            m_client.didFinishDocumentLoadForFrame(toAPI(page), toAPI(frame), toAPI(userData), m_client.base.clientInfo);
        }

        virtual void didFinishLoadForFrame(WebPageProxy* page, WebFrameProxy* frame, uint64_t, API::Object* userData) override
        {
            if (!m_client.didFinishLoadForFrame)
                return;

            m_client.didFinishLoadForFrame(toAPI(page), toAPI(frame), toAPI(userData), m_client.base.clientInfo);
        }

        virtual void didFailLoadWithErrorForFrame(WebPageProxy* page, WebFrameProxy* frame, uint64_t, const ResourceError& error, API::Object* userData) override
        {
            if (!m_client.didFailLoadWithErrorForFrame)
                return;

            m_client.didFailLoadWithErrorForFrame(toAPI(page), toAPI(frame), toAPI(error), toAPI(userData), m_client.base.clientInfo);
        }

        virtual void didSameDocumentNavigationForFrame(WebPageProxy* page, WebFrameProxy* frame, uint64_t, SameDocumentNavigationType type, API::Object* userData) override
        {
            if (!m_client.didSameDocumentNavigationForFrame)
                return;

            m_client.didSameDocumentNavigationForFrame(toAPI(page), toAPI(frame), toAPI(type), toAPI(userData), m_client.base.clientInfo);
        }

        virtual void didReceiveTitleForFrame(WebPageProxy* page, const String& title, WebFrameProxy* frame, API::Object* userData) override
        {
            if (!m_client.didReceiveTitleForFrame)
                return;

            m_client.didReceiveTitleForFrame(toAPI(page), toAPI(title.impl()), toAPI(frame), toAPI(userData), m_client.base.clientInfo);
        }

        virtual void didFirstLayoutForFrame(WebPageProxy* page, WebFrameProxy* frame, API::Object* userData) override
        {
            if (!m_client.didFirstLayoutForFrame)
                return;

            m_client.didFirstLayoutForFrame(toAPI(page), toAPI(frame), toAPI(userData), m_client.base.clientInfo);
        }

        virtual void didFirstVisuallyNonEmptyLayoutForFrame(WebPageProxy* page, WebFrameProxy* frame, API::Object* userData) override
        {
            if (!m_client.didFirstVisuallyNonEmptyLayoutForFrame)
                return;

            m_client.didFirstVisuallyNonEmptyLayoutForFrame(toAPI(page), toAPI(frame), toAPI(userData), m_client.base.clientInfo);
        }

        virtual void didLayout(WebPageProxy* page, LayoutMilestones milestones, API::Object* userData) override
        {
            if (!m_client.didLayout)
                return;

            m_client.didLayout(toAPI(page), toWKLayoutMilestones(milestones), toAPI(userData), m_client.base.clientInfo);
        }

        virtual void didRemoveFrameFromHierarchy(WebPageProxy* page, WebFrameProxy* frame, API::Object* userData) override
        {
            if (!m_client.didRemoveFrameFromHierarchy)
                return;

            m_client.didRemoveFrameFromHierarchy(toAPI(page), toAPI(frame), toAPI(userData), m_client.base.clientInfo);
        }

        virtual void didDisplayInsecureContentForFrame(WebPageProxy* page, WebFrameProxy* frame, API::Object* userData) override
        {
            if (!m_client.didDisplayInsecureContentForFrame)
                return;

            m_client.didDisplayInsecureContentForFrame(toAPI(page), toAPI(frame), toAPI(userData), m_client.base.clientInfo);
        }

        virtual void didRunInsecureContentForFrame(WebPageProxy* page, WebFrameProxy* frame, API::Object* userData) override
        {
            if (!m_client.didRunInsecureContentForFrame)
                return;

            m_client.didRunInsecureContentForFrame(toAPI(page), toAPI(frame), toAPI(userData), m_client.base.clientInfo);
        }

        virtual void didDetectXSSForFrame(WebPageProxy* page, WebFrameProxy* frame, API::Object* userData) override
        {
            if (!m_client.didDetectXSSForFrame)
                return;

            m_client.didDetectXSSForFrame(toAPI(page), toAPI(frame), toAPI(userData), m_client.base.clientInfo);
        }

        virtual bool canAuthenticateAgainstProtectionSpaceInFrame(WebPageProxy* page, WebFrameProxy* frame, WebProtectionSpace* protectionSpace) override
        {
            if (!m_client.canAuthenticateAgainstProtectionSpaceInFrame)
                return false;

            return m_client.canAuthenticateAgainstProtectionSpaceInFrame(toAPI(page), toAPI(frame), toAPI(protectionSpace), m_client.base.clientInfo);
        }

        virtual void didReceiveAuthenticationChallengeInFrame(WebPageProxy* page, WebFrameProxy* frame, AuthenticationChallengeProxy* authenticationChallenge) override
        {
            if (!m_client.didReceiveAuthenticationChallengeInFrame)
                return;

            m_client.didReceiveAuthenticationChallengeInFrame(toAPI(page), toAPI(frame), toAPI(authenticationChallenge), m_client.base.clientInfo);
        }

        virtual void didStartProgress(WebPageProxy* page) override
        {
            if (!m_client.didStartProgress)
                return;

            m_client.didStartProgress(toAPI(page), m_client.base.clientInfo);
        }

        virtual void didChangeProgress(WebPageProxy* page) override
        {
            if (!m_client.didChangeProgress)
                return;

            m_client.didChangeProgress(toAPI(page), m_client.base.clientInfo);
        }

        virtual void didFinishProgress(WebPageProxy* page) override
        {
            if (!m_client.didFinishProgress)
                return;

            m_client.didFinishProgress(toAPI(page), m_client.base.clientInfo);
        }

        virtual void processDidBecomeUnresponsive(WebPageProxy* page) override
        {
            if (!m_client.processDidBecomeUnresponsive)
                return;

            m_client.processDidBecomeUnresponsive(toAPI(page), m_client.base.clientInfo);
        }

        virtual void interactionOccurredWhileProcessUnresponsive(WebPageProxy* page) override
        {
            if (!m_client.interactionOccurredWhileProcessUnresponsive)
                return;

            m_client.interactionOccurredWhileProcessUnresponsive(toAPI(page), m_client.base.clientInfo);
        }

        virtual void processDidBecomeResponsive(WebPageProxy* page) override
        {
            if (!m_client.processDidBecomeResponsive)
                return;

            m_client.processDidBecomeResponsive(toAPI(page), m_client.base.clientInfo);
        }

        virtual void processDidCrash(WebPageProxy* page) override
        {
            if (!m_client.processDidCrash)
                return;

            m_client.processDidCrash(toAPI(page), m_client.base.clientInfo);
        }

        virtual void didChangeBackForwardList(WebPageProxy* page, WebBackForwardListItem* addedItem, Vector<RefPtr<WebBackForwardListItem>> removedItems) override
        {
            if (!m_client.didChangeBackForwardList)
                return;

            RefPtr<API::Array> removedItemsArray;
            if (!removedItems.isEmpty()) {
                Vector<RefPtr<API::Object>> removedItemsVector;
                removedItemsVector.reserveInitialCapacity(removedItems.size());
                for (auto& removedItem : removedItems)
                    removedItemsVector.append(WTF::move(removedItem));

                removedItemsArray = API::Array::create(WTF::move(removedItemsVector));
            }

            m_client.didChangeBackForwardList(toAPI(page), toAPI(addedItem), toAPI(removedItemsArray.get()), m_client.base.clientInfo);
        }

        virtual bool shouldKeepCurrentBackForwardListItemInList(WebKit::WebPageProxy* page, WebKit::WebBackForwardListItem* item) override
        {
            if (!m_client.shouldKeepCurrentBackForwardListItemInList)
                return true;

            return m_client.shouldKeepCurrentBackForwardListItemInList(toAPI(page), toAPI(item), m_client.base.clientInfo);
        }

        virtual void willGoToBackForwardListItem(WebPageProxy* page, WebBackForwardListItem* item, API::Object* userData) override
        {
            if (m_client.willGoToBackForwardListItem)
                m_client.willGoToBackForwardListItem(toAPI(page), toAPI(item), toAPI(userData), m_client.base.clientInfo);
        }

        virtual PassRefPtr<API::Data> webCryptoMasterKey(WebPageProxy& page) override
        {
            return page.process().context().client().copyWebCryptoMasterKey(&page.process().context());
        }

#if ENABLE(NETSCAPE_PLUGIN_API)
        virtual void didFailToInitializePlugin(WebPageProxy* page, ImmutableDictionary* pluginInformation) override
        {
            if (m_client.didFailToInitializePlugin_deprecatedForUseWithV0)
                m_client.didFailToInitializePlugin_deprecatedForUseWithV0(toAPI(page), toAPI(pluginInformation->get<API::String>(pluginInformationMIMETypeKey())), m_client.base.clientInfo);

            if (m_client.pluginDidFail_deprecatedForUseWithV1)
                m_client.pluginDidFail_deprecatedForUseWithV1(toAPI(page), kWKErrorCodeCannotLoadPlugIn, toAPI(pluginInformation->get<API::String>(pluginInformationMIMETypeKey())), 0, 0, m_client.base.clientInfo);

            if (m_client.pluginDidFail)
                m_client.pluginDidFail(toAPI(page), kWKErrorCodeCannotLoadPlugIn, toAPI(pluginInformation), m_client.base.clientInfo);
        }

        virtual void didBlockInsecurePluginVersion(WebPageProxy* page, ImmutableDictionary* pluginInformation) override
        {
            if (m_client.pluginDidFail_deprecatedForUseWithV1)
                m_client.pluginDidFail_deprecatedForUseWithV1(toAPI(page), kWKErrorCodeInsecurePlugInVersion, toAPI(pluginInformation->get<API::String>(pluginInformationMIMETypeKey())), toAPI(pluginInformation->get<API::String>(pluginInformationBundleIdentifierKey())), toAPI(pluginInformation->get<API::String>(pluginInformationBundleVersionKey())), m_client.base.clientInfo);

            if (m_client.pluginDidFail)
                m_client.pluginDidFail(toAPI(page), kWKErrorCodeInsecurePlugInVersion, toAPI(pluginInformation), m_client.base.clientInfo);
        }

        virtual PluginModuleLoadPolicy pluginLoadPolicy(WebPageProxy* page, PluginModuleLoadPolicy currentPluginLoadPolicy, ImmutableDictionary* pluginInformation, String& unavailabilityDescription) override
        {
            WKStringRef unavailabilityDescriptionOut = 0;
            PluginModuleLoadPolicy loadPolicy = currentPluginLoadPolicy;

            if (m_client.pluginLoadPolicy_deprecatedForUseWithV2)
                loadPolicy = toPluginModuleLoadPolicy(m_client.pluginLoadPolicy_deprecatedForUseWithV2(toAPI(page), toWKPluginLoadPolicy(currentPluginLoadPolicy), toAPI(pluginInformation), m_client.base.clientInfo));
            else if (m_client.pluginLoadPolicy)
                loadPolicy = toPluginModuleLoadPolicy(m_client.pluginLoadPolicy(toAPI(page), toWKPluginLoadPolicy(currentPluginLoadPolicy), toAPI(pluginInformation), &unavailabilityDescriptionOut, m_client.base.clientInfo));

            if (unavailabilityDescriptionOut) {
                RefPtr<API::String> webUnavailabilityDescription = adoptRef(toImpl(unavailabilityDescriptionOut));
                unavailabilityDescription = webUnavailabilityDescription->string();
            }
            
            return loadPolicy;
        }
#endif // ENABLE(NETSCAPE_PLUGIN_API)

#if ENABLE(WEBGL)
        virtual WebCore::WebGLLoadPolicy webGLLoadPolicy(WebPageProxy* page, const String& url) const override
        {
            WebCore::WebGLLoadPolicy loadPolicy = WebGLAllowCreation;

            if (m_client.webGLLoadPolicy)
                loadPolicy = toWebGLLoadPolicy(m_client.webGLLoadPolicy(toAPI(page), toAPI(url.impl()), m_client.base.clientInfo));

            return loadPolicy;
        }

        virtual WebCore::WebGLLoadPolicy resolveWebGLLoadPolicy(WebPageProxy* page, const String& url) const override
        {
            WebCore::WebGLLoadPolicy loadPolicy = WebGLAllowCreation;

            if (m_client.resolveWebGLLoadPolicy)
                loadPolicy = toWebGLLoadPolicy(m_client.resolveWebGLLoadPolicy(toAPI(page), toAPI(url.impl()), m_client.base.clientInfo));

            return loadPolicy;
        }

#endif // ENABLE(WEBGL)
    };

    WebPageProxy* webPageProxy = toImpl(pageRef);

    auto loaderClient = std::make_unique<LoaderClient>(wkClient);

    // It would be nice to get rid of this code and transition all clients to using didLayout instead of
    // didFirstLayoutInFrame and didFirstVisuallyNonEmptyLayoutInFrame. In the meantime, this is required
    // for backwards compatibility.
    WebCore::LayoutMilestones milestones = 0;
    if (loaderClient->client().didFirstLayoutForFrame)
        milestones |= WebCore::DidFirstLayout;
    if (loaderClient->client().didFirstVisuallyNonEmptyLayoutForFrame)
        milestones |= WebCore::DidFirstVisuallyNonEmptyLayout;

    if (milestones)
        webPageProxy->process().send(Messages::WebPage::ListenForLayoutMilestones(milestones), webPageProxy->pageID());

    webPageProxy->setLoaderClient(WTF::move(loaderClient));
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
        virtual void decidePolicyForNavigationAction(WebPageProxy* page, WebFrameProxy* frame, const NavigationActionData& navigationActionData, WebFrameProxy* originatingFrame, const WebCore::ResourceRequest& originalResourceRequest, const WebCore::ResourceRequest& resourceRequest, RefPtr<WebFramePolicyListenerProxy> listener, API::Object* userData) override
        {
            if (!m_client.decidePolicyForNavigationAction_deprecatedForUseWithV0 && !m_client.decidePolicyForNavigationAction_deprecatedForUseWithV1 && !m_client.decidePolicyForNavigationAction) {
                listener->use();
                return;
            }

            RefPtr<API::URLRequest> originalRequest = API::URLRequest::create(originalResourceRequest);
            RefPtr<API::URLRequest> request = API::URLRequest::create(resourceRequest);

            if (m_client.decidePolicyForNavigationAction_deprecatedForUseWithV0)
                m_client.decidePolicyForNavigationAction_deprecatedForUseWithV0(toAPI(page), toAPI(frame), toAPI(navigationActionData.navigationType), toAPI(navigationActionData.modifiers), toAPI(navigationActionData.mouseButton), toAPI(request.get()), toAPI(listener.get()), toAPI(userData), m_client.base.clientInfo);
            else if (m_client.decidePolicyForNavigationAction_deprecatedForUseWithV1)
                m_client.decidePolicyForNavigationAction_deprecatedForUseWithV1(toAPI(page), toAPI(frame), toAPI(navigationActionData.navigationType), toAPI(navigationActionData.modifiers), toAPI(navigationActionData.mouseButton), toAPI(originatingFrame), toAPI(request.get()), toAPI(listener.get()), toAPI(userData), m_client.base.clientInfo);
            else
                m_client.decidePolicyForNavigationAction(toAPI(page), toAPI(frame), toAPI(navigationActionData.navigationType), toAPI(navigationActionData.modifiers), toAPI(navigationActionData.mouseButton), toAPI(originatingFrame), toAPI(originalRequest.get()), toAPI(request.get()), toAPI(listener.get()), toAPI(userData), m_client.base.clientInfo);
        }

        virtual void decidePolicyForNewWindowAction(WebPageProxy* page, WebFrameProxy* frame, const NavigationActionData& navigationActionData, const ResourceRequest& resourceRequest, const String& frameName, RefPtr<WebFramePolicyListenerProxy> listener, API::Object* userData) override
        {
            if (!m_client.decidePolicyForNewWindowAction) {
                listener->use();
                return;
            }

            RefPtr<API::URLRequest> request = API::URLRequest::create(resourceRequest);

            m_client.decidePolicyForNewWindowAction(toAPI(page), toAPI(frame), toAPI(navigationActionData.navigationType), toAPI(navigationActionData.modifiers), toAPI(navigationActionData.mouseButton), toAPI(request.get()), toAPI(frameName.impl()), toAPI(listener.get()), toAPI(userData), m_client.base.clientInfo);
        }

        virtual void decidePolicyForResponse(WebPageProxy* page, WebFrameProxy* frame, const ResourceResponse& resourceResponse, const ResourceRequest& resourceRequest, bool canShowMIMEType, RefPtr<WebFramePolicyListenerProxy> listener, API::Object* userData) override
        {
            if (!m_client.decidePolicyForResponse_deprecatedForUseWithV0 && !m_client.decidePolicyForResponse) {
                listener->use();
                return;
            }

            RefPtr<API::URLResponse> response = API::URLResponse::create(resourceResponse);
            RefPtr<API::URLRequest> request = API::URLRequest::create(resourceRequest);

            if (m_client.decidePolicyForResponse_deprecatedForUseWithV0)
                m_client.decidePolicyForResponse_deprecatedForUseWithV0(toAPI(page), toAPI(frame), toAPI(response.get()), toAPI(request.get()), toAPI(listener.get()), toAPI(userData), m_client.base.clientInfo);
            else
                m_client.decidePolicyForResponse(toAPI(page), toAPI(frame), toAPI(response.get()), toAPI(request.get()), canShowMIMEType, toAPI(listener.get()), toAPI(userData), m_client.base.clientInfo);
        }

        virtual void unableToImplementPolicy(WebPageProxy* page, WebFrameProxy* frame, const ResourceError& error, API::Object* userData) override
        {
            if (!m_client.unableToImplementPolicy)
                return;
            
            m_client.unableToImplementPolicy(toAPI(page), toAPI(frame), toAPI(error), toAPI(userData), m_client.base.clientInfo);
        }
    };

    toImpl(pageRef)->setPolicyClient(std::make_unique<PolicyClient>(wkClient));
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
        virtual PassRefPtr<WebPageProxy> createNewPage(WebPageProxy* page, WebFrameProxy*, const ResourceRequest& resourceRequest, const WindowFeatures& windowFeatures, const NavigationActionData& navigationActionData) override
        {
            if (!m_client.base.version && !m_client.createNewPage_deprecatedForUseWithV0)
                return 0;

            if (m_client.base.version > 0 && !m_client.createNewPage)
                return 0;

            ImmutableDictionary::MapType map;
            if (windowFeatures.xSet)
                map.set("x", API::Double::create(windowFeatures.x));
            if (windowFeatures.ySet)
                map.set("y", API::Double::create(windowFeatures.y));
            if (windowFeatures.widthSet)
                map.set("width", API::Double::create(windowFeatures.width));
            if (windowFeatures.heightSet)
                map.set("height", API::Double::create(windowFeatures.height));
            map.set("menuBarVisible", API::Boolean::create(windowFeatures.menuBarVisible));
            map.set("statusBarVisible", API::Boolean::create(windowFeatures.statusBarVisible));
            map.set("toolBarVisible", API::Boolean::create(windowFeatures.toolBarVisible));
            map.set("locationBarVisible", API::Boolean::create(windowFeatures.locationBarVisible));
            map.set("scrollbarsVisible", API::Boolean::create(windowFeatures.scrollbarsVisible));
            map.set("resizable", API::Boolean::create(windowFeatures.resizable));
            map.set("fullscreen", API::Boolean::create(windowFeatures.fullscreen));
            map.set("dialog", API::Boolean::create(windowFeatures.dialog));
            RefPtr<ImmutableDictionary> featuresMap = ImmutableDictionary::create(WTF::move(map));

            if (!m_client.base.version)
                return adoptRef(toImpl(m_client.createNewPage_deprecatedForUseWithV0(toAPI(page), toAPI(featuresMap.get()), toAPI(navigationActionData.modifiers), toAPI(navigationActionData.mouseButton), m_client.base.clientInfo)));

            RefPtr<API::URLRequest> request = API::URLRequest::create(resourceRequest);
            return adoptRef(toImpl(m_client.createNewPage(toAPI(page), toAPI(request.get()), toAPI(featuresMap.get()), toAPI(navigationActionData.modifiers), toAPI(navigationActionData.mouseButton), m_client.base.clientInfo)));
        }

        virtual void showPage(WebPageProxy* page) override
        {
            if (!m_client.showPage)
                return;

            m_client.showPage(toAPI(page), m_client.base.clientInfo);
        }

        virtual void close(WebPageProxy* page) override
        {
            if (!m_client.close)
                return;

            m_client.close(toAPI(page), m_client.base.clientInfo);
        }

        virtual void takeFocus(WebPageProxy* page, WKFocusDirection direction) override
        {
            if (!m_client.takeFocus)
                return;

            m_client.takeFocus(toAPI(page), direction, m_client.base.clientInfo);
        }

        virtual void focus(WebPageProxy* page) override
        {
            if (!m_client.focus)
                return;

            m_client.focus(toAPI(page), m_client.base.clientInfo);
        }

        virtual void unfocus(WebPageProxy* page) override
        {
            if (!m_client.unfocus)
                return;

            m_client.unfocus(toAPI(page), m_client.base.clientInfo);
        }

        virtual void runJavaScriptAlert(WebPageProxy* page, const String& message, WebFrameProxy* frame, std::function<void ()> completionHandler) override
        {
            if (!m_client.runJavaScriptAlert) {
                completionHandler();
                return;
            }

            m_client.runJavaScriptAlert(toAPI(page), toAPI(message.impl()), toAPI(frame), m_client.base.clientInfo);
            completionHandler();
        }

        virtual void runJavaScriptConfirm(WebPageProxy* page, const String& message, WebFrameProxy* frame, std::function<void (bool)> completionHandler) override
        {
            if (!m_client.runJavaScriptConfirm) {
                completionHandler(false);
                return;
            }

            bool result = m_client.runJavaScriptConfirm(toAPI(page), toAPI(message.impl()), toAPI(frame), m_client.base.clientInfo);
            completionHandler(result);
        }

        virtual void runJavaScriptPrompt(WebPageProxy* page, const String& message, const String& defaultValue, WebFrameProxy* frame, std::function<void (const String&)> completionHandler) override
        {
            if (!m_client.runJavaScriptPrompt) {
                completionHandler(String());
                return;
            }

            RefPtr<API::String> string = adoptRef(toImpl(m_client.runJavaScriptPrompt(toAPI(page), toAPI(message.impl()), toAPI(defaultValue.impl()), toAPI(frame), m_client.base.clientInfo)));
            if (!string) {
                completionHandler(String());
                return;
            }

            completionHandler(string->string());
        }

        virtual void setStatusText(WebPageProxy* page, const String& text) override
        {
            if (!m_client.setStatusText)
                return;

            m_client.setStatusText(toAPI(page), toAPI(text.impl()), m_client.base.clientInfo);
        }

        virtual void mouseDidMoveOverElement(WebPageProxy* page, const WebHitTestResult::Data& data, WebEvent::Modifiers modifiers, API::Object* userData) override
        {
            if (!m_client.mouseDidMoveOverElement && !m_client.mouseDidMoveOverElement_deprecatedForUseWithV0)
                return;

            if (m_client.base.version > 0 && !m_client.mouseDidMoveOverElement)
                return;

            if (!m_client.base.version) {
                m_client.mouseDidMoveOverElement_deprecatedForUseWithV0(toAPI(page), toAPI(modifiers), toAPI(userData), m_client.base.clientInfo);
                return;
            }

            RefPtr<WebHitTestResult> webHitTestResult = WebHitTestResult::create(data);
            m_client.mouseDidMoveOverElement(toAPI(page), toAPI(webHitTestResult.get()), toAPI(modifiers), toAPI(userData), m_client.base.clientInfo);
        }

#if ENABLE(NETSCAPE_PLUGIN_API)
        virtual void unavailablePluginButtonClicked(WebPageProxy* page, WKPluginUnavailabilityReason pluginUnavailabilityReason, ImmutableDictionary* pluginInformation) override
        {
            if (pluginUnavailabilityReason == kWKPluginUnavailabilityReasonPluginMissing) {
                if (m_client.missingPluginButtonClicked_deprecatedForUseWithV0)
                    m_client.missingPluginButtonClicked_deprecatedForUseWithV0(
                        toAPI(page),
                        toAPI(pluginInformation->get<API::String>(pluginInformationMIMETypeKey())),
                        toAPI(pluginInformation->get<API::String>(pluginInformationPluginURLKey())),
                        toAPI(pluginInformation->get<API::String>(pluginInformationPluginspageAttributeURLKey())),
                        m_client.base.clientInfo);
            }

            if (m_client.unavailablePluginButtonClicked_deprecatedForUseWithV1)
                m_client.unavailablePluginButtonClicked_deprecatedForUseWithV1(
                    toAPI(page),
                    pluginUnavailabilityReason,
                    toAPI(pluginInformation->get<API::String>(pluginInformationMIMETypeKey())),
                    toAPI(pluginInformation->get<API::String>(pluginInformationPluginURLKey())),
                    toAPI(pluginInformation->get<API::String>(pluginInformationPluginspageAttributeURLKey())),
                    m_client.base.clientInfo);

            if (m_client.unavailablePluginButtonClicked)
                m_client.unavailablePluginButtonClicked(
                    toAPI(page),
                    pluginUnavailabilityReason,
                    toAPI(pluginInformation),
                    m_client.base.clientInfo);
        }
#endif // ENABLE(NETSCAPE_PLUGIN_API)

        virtual bool implementsDidNotHandleKeyEvent() const override
        {
            return m_client.didNotHandleKeyEvent;
        }

        virtual void didNotHandleKeyEvent(WebPageProxy* page, const NativeWebKeyboardEvent& event) override
        {
            if (!m_client.didNotHandleKeyEvent)
                return;
            m_client.didNotHandleKeyEvent(toAPI(page), event.nativeEvent(), m_client.base.clientInfo);
        }

        virtual bool implementsDidNotHandleWheelEvent() const override
        {
            return m_client.didNotHandleWheelEvent;
        }

        virtual void didNotHandleWheelEvent(WebPageProxy* page, const NativeWebWheelEvent& event) override
        {
            if (!m_client.didNotHandleWheelEvent)
                return;
            m_client.didNotHandleWheelEvent(toAPI(page), event.nativeEvent(), m_client.base.clientInfo);
        }

        virtual bool toolbarsAreVisible(WebPageProxy* page) override
        {
            if (!m_client.toolbarsAreVisible)
                return true;
            return m_client.toolbarsAreVisible(toAPI(page), m_client.base.clientInfo);
        }

        virtual void setToolbarsAreVisible(WebPageProxy* page, bool visible) override
        {
            if (!m_client.setToolbarsAreVisible)
                return;
            m_client.setToolbarsAreVisible(toAPI(page), visible, m_client.base.clientInfo);
        }

        virtual bool menuBarIsVisible(WebPageProxy* page) override
        {
            if (!m_client.menuBarIsVisible)
                return true;
            return m_client.menuBarIsVisible(toAPI(page), m_client.base.clientInfo);
        }

        virtual void setMenuBarIsVisible(WebPageProxy* page, bool visible) override
        {
            if (!m_client.setMenuBarIsVisible)
                return;
            m_client.setMenuBarIsVisible(toAPI(page), visible, m_client.base.clientInfo);
        }

        virtual bool statusBarIsVisible(WebPageProxy* page) override
        {
            if (!m_client.statusBarIsVisible)
                return true;
            return m_client.statusBarIsVisible(toAPI(page), m_client.base.clientInfo);
        }

        virtual void setStatusBarIsVisible(WebPageProxy* page, bool visible) override
        {
            if (!m_client.setStatusBarIsVisible)
                return;
            m_client.setStatusBarIsVisible(toAPI(page), visible, m_client.base.clientInfo);
        }

        virtual bool isResizable(WebPageProxy* page) override
        {
            if (!m_client.isResizable)
                return true;
            return m_client.isResizable(toAPI(page), m_client.base.clientInfo);
        }

        virtual void setIsResizable(WebPageProxy* page, bool resizable) override
        {
            if (!m_client.setIsResizable)
                return;
            m_client.setIsResizable(toAPI(page), resizable, m_client.base.clientInfo);
        }

        virtual void setWindowFrame(WebPageProxy* page, const FloatRect& frame) override
        {
            if (!m_client.setWindowFrame)
                return;

            m_client.setWindowFrame(toAPI(page), toAPI(frame), m_client.base.clientInfo);
        }

        virtual FloatRect windowFrame(WebPageProxy* page) override
        {
            if (!m_client.getWindowFrame)
                return FloatRect();

            return toFloatRect(m_client.getWindowFrame(toAPI(page), m_client.base.clientInfo));
        }

        virtual bool canRunBeforeUnloadConfirmPanel() const override
        {
            return m_client.runBeforeUnloadConfirmPanel;
        }

        virtual bool runBeforeUnloadConfirmPanel(WebPageProxy* page, const String& message, WebFrameProxy* frame) override
        {
            if (!m_client.runBeforeUnloadConfirmPanel)
                return true;

            return m_client.runBeforeUnloadConfirmPanel(toAPI(page), toAPI(message.impl()), toAPI(frame), m_client.base.clientInfo);
        }

        virtual void didDraw(WebPageProxy* page) override
        {
            if (!m_client.didDraw)
                return;

            m_client.didDraw(toAPI(page), m_client.base.clientInfo);
        }

        virtual void pageDidScroll(WebPageProxy* page) override
        {
            if (!m_client.pageDidScroll)
                return;

            m_client.pageDidScroll(toAPI(page), m_client.base.clientInfo);
        }

        virtual void exceededDatabaseQuota(WebPageProxy* page, WebFrameProxy* frame, WebSecurityOrigin* origin, const String& databaseName, const String& databaseDisplayName, unsigned long long currentQuota, unsigned long long currentOriginUsage, unsigned long long currentDatabaseUsage, unsigned long long expectedUsage, std::function<void (unsigned long long)> completionHandler) override
        {
            if (!m_client.exceededDatabaseQuota)
                completionHandler(currentQuota);

            completionHandler(m_client.exceededDatabaseQuota(toAPI(page), toAPI(frame), toAPI(origin), toAPI(databaseName.impl()), toAPI(databaseDisplayName.impl()), currentQuota, currentOriginUsage, currentDatabaseUsage, expectedUsage, m_client.base.clientInfo));
        }

        virtual bool runOpenPanel(WebPageProxy* page, WebFrameProxy* frame, WebOpenPanelParameters* parameters, WebOpenPanelResultListenerProxy* listener) override
        {
            if (!m_client.runOpenPanel)
                return false;

            m_client.runOpenPanel(toAPI(page), toAPI(frame), toAPI(parameters), toAPI(listener), m_client.base.clientInfo);
            return true;
        }

        virtual bool decidePolicyForGeolocationPermissionRequest(WebPageProxy* page, WebFrameProxy* frame, WebSecurityOrigin* origin, GeolocationPermissionRequestProxy* permissionRequest) override
        {
            if (!m_client.decidePolicyForGeolocationPermissionRequest)
                return false;

            m_client.decidePolicyForGeolocationPermissionRequest(toAPI(page), toAPI(frame), toAPI(origin), toAPI(permissionRequest), m_client.base.clientInfo);
            return true;
        }

        virtual bool decidePolicyForNotificationPermissionRequest(WebPageProxy* page, WebSecurityOrigin* origin, NotificationPermissionRequest* permissionRequest) override
        {
            if (!m_client.decidePolicyForNotificationPermissionRequest)
                return false;

            m_client.decidePolicyForNotificationPermissionRequest(toAPI(page), toAPI(origin), toAPI(permissionRequest), m_client.base.clientInfo);
            return true;
        }

        // Printing.
        virtual float headerHeight(WebPageProxy* page, WebFrameProxy* frame) override
        {
            if (!m_client.headerHeight)
                return 0;

            return m_client.headerHeight(toAPI(page), toAPI(frame), m_client.base.clientInfo);
        }

        virtual float footerHeight(WebPageProxy* page, WebFrameProxy* frame) override
        {
            if (!m_client.footerHeight)
                return 0;

            return m_client.footerHeight(toAPI(page), toAPI(frame), m_client.base.clientInfo);
        }

        virtual void drawHeader(WebPageProxy* page, WebFrameProxy* frame, const WebCore::FloatRect& rect) override
        {
            if (!m_client.drawHeader)
                return;

            m_client.drawHeader(toAPI(page), toAPI(frame), toAPI(rect), m_client.base.clientInfo);
        }

        virtual void drawFooter(WebPageProxy* page, WebFrameProxy* frame, const WebCore::FloatRect& rect) override
        {
            if (!m_client.drawFooter)
                return;

            m_client.drawFooter(toAPI(page), toAPI(frame), toAPI(rect), m_client.base.clientInfo);
        }

        virtual void printFrame(WebPageProxy* page, WebFrameProxy* frame) override
        {
            if (!m_client.printFrame)
                return;

            m_client.printFrame(toAPI(page), toAPI(frame), m_client.base.clientInfo);
        }

        virtual bool canRunModal() const override
        {
            return m_client.runModal;
        }

        virtual void runModal(WebPageProxy* page) override
        {
            if (!m_client.runModal)
                return;

            m_client.runModal(toAPI(page), m_client.base.clientInfo);
        }

        virtual void saveDataToFileInDownloadsFolder(WebPageProxy* page, const String& suggestedFilename, const String& mimeType, const String& originatingURLString, API::Data* data) override
        {
            if (!m_client.saveDataToFileInDownloadsFolder)
                return;

            m_client.saveDataToFileInDownloadsFolder(toAPI(page), toAPI(suggestedFilename.impl()), toAPI(mimeType.impl()), toURLRef(originatingURLString.impl()), toAPI(data), m_client.base.clientInfo);
        }

        virtual bool shouldInterruptJavaScript(WebPageProxy* page) override
        {
            if (!m_client.shouldInterruptJavaScript)
                return false;

            return m_client.shouldInterruptJavaScript(toAPI(page), m_client.base.clientInfo);
        }

        virtual void pinnedStateDidChange(WebPageProxy& page) override
        {
            if (!m_client.pinnedStateDidChange)
                return;

            m_client.pinnedStateDidChange(toAPI(&page), m_client.base.clientInfo);
        }
    };

    toImpl(pageRef)->setUIClient(std::make_unique<UIClient>(wkClient));
}

void WKPageSetSession(WKPageRef pageRef, WKSessionRef session)
{
    toImpl(pageRef)->setSession(*toImpl(session));
}

void WKPageRunJavaScriptInMainFrame(WKPageRef pageRef, WKStringRef scriptRef, void* context, WKPageRunJavaScriptFunction callback)
{
    toImpl(pageRef)->runJavaScriptInMainFrame(toImpl(scriptRef)->string(), toGenericCallbackFunction(context, callback));
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

static std::function<void (const String&, CallbackBase::Error)> toGenericCallbackFunction(void* context, void (*callback)(WKStringRef, WKErrorRef, void*))
{
    return [context, callback](const String& returnValue, CallbackBase::Error error) {
        callback(toAPI(API::String::create(returnValue).get()), error != CallbackBase::Error::None ? toAPI(API::Error::create().get()) : 0, context);
    };
}

void WKPageRenderTreeExternalRepresentation(WKPageRef pageRef, void* context, WKPageRenderTreeExternalRepresentationFunction callback)
{
    toImpl(pageRef)->getRenderTreeExternalRepresentation(toGenericCallbackFunction(context, callback));
}

void WKPageGetSourceForFrame(WKPageRef pageRef, WKFrameRef frameRef, void* context, WKPageGetSourceForFrameFunction callback)
{
    toImpl(pageRef)->getSourceForFrame(toImpl(frameRef), toGenericCallbackFunction(context, callback));
}

void WKPageGetContentsAsString(WKPageRef pageRef, void* context, WKPageGetContentsAsStringFunction callback)
{
    toImpl(pageRef)->getContentsAsString(toGenericCallbackFunction(context, callback));
}

void WKPageGetBytecodeProfile(WKPageRef pageRef, void* context, WKPageGetBytecodeProfileFunction callback)
{
    toImpl(pageRef)->getBytecodeProfile(toGenericCallbackFunction(context, callback));
}

void WKPageGetSelectionAsWebArchiveData(WKPageRef pageRef, void* context, WKPageGetSelectionAsWebArchiveDataFunction callback)
{
    toImpl(pageRef)->getSelectionAsWebArchiveData(toGenericCallbackFunction(context, callback));
}

void WKPageGetContentsAsMHTMLData(WKPageRef pageRef, bool useBinaryEncoding, void* context, WKPageGetContentsAsMHTMLDataFunction callback)
{
#if ENABLE(MHTML)
    toImpl(pageRef)->getContentsAsMHTMLData(toGenericCallbackFunction(context, callback), useBinaryEncoding);
#else
    UNUSED_PARAM(pageRef);
    UNUSED_PARAM(useBinaryEncoding);
    UNUSED_PARAM(context);
    UNUSED_PARAM(callback);
#endif
}

void WKPageForceRepaint(WKPageRef pageRef, void* context, WKPageForceRepaintFunction callback)
{
    toImpl(pageRef)->forceRepaint(VoidCallback::create([context, callback](CallbackBase::Error error) {
        callback(error == CallbackBase::Error::None ? nullptr : toAPI(API::Error::create().get()), context);
    }));
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
    toImpl(pageRef)->validateCommand(toImpl(command)->string(), [context, callback](const String& commandName, bool isEnabled, int32_t state, CallbackBase::Error error) {
        callback(toAPI(API::String::create(commandName).get()), isEnabled, state, error != CallbackBase::Error::None ? toAPI(API::Error::create().get()) : 0, context);
    });
}

void WKPageExecuteCommand(WKPageRef pageRef, WKStringRef command)
{
    toImpl(pageRef)->executeEditCommand(toImpl(command)->string());
}

#if PLATFORM(COCOA)
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
    toImpl(page)->computePagesForPrinting(toImpl(frame), printInfoFromWKPrintInfo(printInfo), ComputedPagesCallback::create([context, callback](const Vector<WebCore::IntRect>& rects, double scaleFactor, CallbackBase::Error error) {
        Vector<WKRect> wkRects(rects.size());
        for (size_t i = 0; i < rects.size(); ++i)
            wkRects[i] = toAPI(rects[i]);
        callback(wkRects.data(), wkRects.size(), scaleFactor, error != CallbackBase::Error::None ? toAPI(API::Error::create().get()) : 0, context);
    }));
}

void WKPageBeginPrinting(WKPageRef page, WKFrameRef frame, WKPrintInfo printInfo)
{
    toImpl(page)->beginPrinting(toImpl(frame), printInfoFromWKPrintInfo(printInfo));
}

void WKPageDrawPagesToPDF(WKPageRef page, WKFrameRef frame, WKPrintInfo printInfo, uint32_t first, uint32_t count, WKPageDrawToPDFFunction callback, void* context)
{
    toImpl(page)->drawPagesToPDF(toImpl(frame), printInfoFromWKPrintInfo(printInfo), first, count, DataCallback::create(toGenericCallbackFunction(context, callback)));
}

void WKPageEndPrinting(WKPageRef page)
{
    toImpl(page)->endPrinting();
}
#endif

WKImageRef WKPageCreateSnapshotOfVisibleContent(WKPageRef)
{
    return 0;
}

void WKPageSetShouldSendEventsSynchronously(WKPageRef page, bool sync)
{
    toImpl(page)->setShouldSendEventsSynchronously(sync);
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

    return toAPI(API::Array::create(WTF::move(relatedPages)).leakRef());
}

void WKPageSetMayStartMediaWhenInWindow(WKPageRef pageRef, bool mayStartMedia)
{
    toImpl(pageRef)->setMayStartMediaWhenInWindow(mayStartMedia);
}


void WKPageSelectContextMenuItem(WKPageRef page, WKContextMenuItemRef item)
{
#if ENABLE(CONTEXT_MENUS)
    toImpl(page)->contextMenuItemSelected(*(toImpl(item)->data()));
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

void WKPageSetInvalidMessageFunction(WKPageInvalidMessageFunction)
{
    // FIXME: Remove this function when doing so won't break WebKit nightlies.
}

#if ENABLE(NETSCAPE_PLUGIN_API)

// -- DEPRECATED --

WKStringRef WKPageGetPluginInformationBundleIdentifierKey()
{
    return WKPluginInformationBundleIdentifierKey();
}

WKStringRef WKPageGetPluginInformationBundleVersionKey()
{
    return WKPluginInformationBundleVersionKey();
}

WKStringRef WKPageGetPluginInformationDisplayNameKey()
{
    return WKPluginInformationDisplayNameKey();
}

WKStringRef WKPageGetPluginInformationFrameURLKey()
{
    return WKPluginInformationFrameURLKey();
}

WKStringRef WKPageGetPluginInformationMIMETypeKey()
{
    return WKPluginInformationMIMETypeKey();
}

WKStringRef WKPageGetPluginInformationPageURLKey()
{
    return WKPluginInformationPageURLKey();
}

WKStringRef WKPageGetPluginInformationPluginspageAttributeURLKey()
{
    return WKPluginInformationPluginspageAttributeURLKey();
}

WKStringRef WKPageGetPluginInformationPluginURLKey()
{
    return WKPluginInformationPluginURLKey();
}

// -- DEPRECATED --

#endif // ENABLE(NETSCAPE_PLUGIN_API)
