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
#include "APILoaderClient.h"
#include "APIPolicyClient.h"
#include "ImmutableDictionary.h"
#include "NavigationActionData.h"
#include "PluginInformation.h"
#include "PrintInfo.h"
#include "WKAPICast.h"
#include "WKPagePolicyClientInternal.h"
#include "WKPluginInformation.h"
#include "WebBackForwardList.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <WebCore/Page.h>

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
    typedef std::tuple<WKPageLoaderClientV0, WKPageLoaderClientV1, WKPageLoaderClientV2, WKPageLoaderClientV3, WKPageLoaderClientV4> Versions;
};

template<> struct ClientTraits<WKPagePolicyClientBase> {
    typedef std::tuple<WKPagePolicyClientV0, WKPagePolicyClientV1, WKPagePolicyClientInternal> Versions;
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
    toImpl(pageRef)->loadURL(toWTFString(URLRef));
}

void WKPageLoadURLWithUserData(WKPageRef pageRef, WKURLRef URLRef, WKTypeRef userDataRef)
{
    toImpl(pageRef)->loadURL(toWTFString(URLRef), toImpl(userDataRef));
}

void WKPageLoadURLRequest(WKPageRef pageRef, WKURLRequestRef urlRequestRef)
{
    toImpl(pageRef)->loadURLRequest(toImpl(urlRequestRef));    
}

void WKPageLoadURLRequestWithUserData(WKPageRef pageRef, WKURLRequestRef urlRequestRef, WKTypeRef userDataRef)
{
    toImpl(pageRef)->loadURLRequest(toImpl(urlRequestRef), toImpl(userDataRef));    
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

void WKPageSetMemoryCacheClientCallsEnabled(WKPageRef pageRef, bool memoryCacheClientCallsEnabled)
{
    toImpl(pageRef)->setMemoryCacheClientCallsEnabled(memoryCacheClientCallsEnabled);
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

WKDataRef WKPageCopySessionState(WKPageRef pageRef, void *context, WKPageSessionStateFilterCallback filter)
{
    return toAPI(toImpl(pageRef)->sessionStateData(filter, context).leakRef());
}

void WKPageRestoreFromSessionState(WKPageRef pageRef, WKDataRef sessionStateData)
{
    toImpl(pageRef)->restoreFromSessionStateData(toImpl(sessionStateData));
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

void WKPageSetVisibilityState(WKPageRef pageRef, WKPageVisibilityState state, bool)
{
    if (state == kWKPageVisibilityStatePrerender)
        toImpl(pageRef)->setVisibilityStatePrerender();
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
    toImpl(pageRef)->initializeFindClient(wkClient);
}

void WKPageSetPageFindMatchesClient(WKPageRef pageRef, const WKPageFindMatchesClientBase* wkClient)
{
    toImpl(pageRef)->initializeFindMatchesClient(wkClient);
}

void WKPageSetPageFormClient(WKPageRef pageRef, const WKPageFormClientBase* wkClient)
{
    toImpl(pageRef)->initializeFormClient(wkClient);
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
        virtual void didStartProvisionalLoadForFrame(WebPageProxy* page, WebFrameProxy* frame, API::Object* userData) override
        {
            if (!m_client.didStartProvisionalLoadForFrame)
                return;

            m_client.didStartProvisionalLoadForFrame(toAPI(page), toAPI(frame), toAPI(userData), m_client.base.clientInfo);
        }

        virtual void didReceiveServerRedirectForProvisionalLoadForFrame(WebPageProxy* page, WebFrameProxy* frame, API::Object* userData) override
        {
            if (!m_client.didReceiveServerRedirectForProvisionalLoadForFrame)
                return;

            m_client.didReceiveServerRedirectForProvisionalLoadForFrame(toAPI(page), toAPI(frame), toAPI(userData), m_client.base.clientInfo);
        }

        virtual void didFailProvisionalLoadWithErrorForFrame(WebPageProxy* page, WebFrameProxy* frame, const ResourceError& error, API::Object* userData) override
        {
            if (!m_client.didFailProvisionalLoadWithErrorForFrame)
                return;

            m_client.didFailProvisionalLoadWithErrorForFrame(toAPI(page), toAPI(frame), toAPI(error), toAPI(userData), m_client.base.clientInfo);
        }

        virtual void didCommitLoadForFrame(WebPageProxy* page, WebFrameProxy* frame, API::Object* userData) override
        {
            if (!m_client.didCommitLoadForFrame)
                return;

            m_client.didCommitLoadForFrame(toAPI(page), toAPI(frame), toAPI(userData), m_client.base.clientInfo);
        }

        virtual void didFinishDocumentLoadForFrame(WebPageProxy* page, WebFrameProxy* frame, API::Object* userData) override
        {
            if (!m_client.didFinishDocumentLoadForFrame)
                return;

            m_client.didFinishDocumentLoadForFrame(toAPI(page), toAPI(frame), toAPI(userData), m_client.base.clientInfo);
        }

        virtual void didFinishLoadForFrame(WebPageProxy* page, WebFrameProxy* frame, API::Object* userData) override
        {
            if (!m_client.didFinishLoadForFrame)
                return;

            m_client.didFinishLoadForFrame(toAPI(page), toAPI(frame), toAPI(userData), m_client.base.clientInfo);
        }

        virtual void didFailLoadWithErrorForFrame(WebPageProxy* page, WebFrameProxy* frame, const ResourceError& error, API::Object* userData) override
        {
            if (!m_client.didFailLoadWithErrorForFrame)
                return;

            m_client.didFailLoadWithErrorForFrame(toAPI(page), toAPI(frame), toAPI(error), toAPI(userData), m_client.base.clientInfo);
        }

        virtual void didSameDocumentNavigationForFrame(WebPageProxy* page, WebFrameProxy* frame, SameDocumentNavigationType type, API::Object* userData) override
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

        virtual void didChangeBackForwardList(WebPageProxy* page, WebBackForwardListItem* addedItem, Vector<RefPtr<API::Object>>* removedItems) override
        {
            if (!m_client.didChangeBackForwardList)
                return;

            RefPtr<API::Array> removedItemsArray;
            if (removedItems && !removedItems->isEmpty())
                removedItemsArray = API::Array::create(std::move(*removedItems));

            m_client.didChangeBackForwardList(toAPI(page), toAPI(addedItem), toAPI(removedItemsArray.get()), m_client.base.clientInfo);
        }

        virtual void willGoToBackForwardListItem(WebPageProxy* page, WebBackForwardListItem* item, API::Object* userData) override
        {
            if (m_client.willGoToBackForwardListItem)
                m_client.willGoToBackForwardListItem(toAPI(page), toAPI(item), toAPI(userData), m_client.base.clientInfo);
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
            WebCore::WebGLLoadPolicy loadPolicy = WebGLAllow;

            if (m_client.webGLLoadPolicy)
                loadPolicy = toWebGLLoadPolicy(m_client.webGLLoadPolicy(toAPI(page), toAPI(url.impl()), m_client.base.clientInfo));

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

    webPageProxy->setLoaderClient(std::move(loaderClient));
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
        virtual void decidePolicyForNavigationAction(WebPageProxy* page, WebFrameProxy* frame, const NavigationActionData& navigationActionData, WebFrameProxy* originatingFrame, const WebCore::ResourceRequest& originalResourceRequest, const WebCore::ResourceRequest& resourceRequest, WebFramePolicyListenerProxy* listener, API::Object* userData) override
        {
            if (!m_client.decidePolicyForNavigationAction_deprecatedForUseWithV0 && !m_client.decidePolicyForNavigationAction_deprecatedForUseWithV1 && !m_client.decidePolicyForNavigationAction) {
                listener->use();
                return;
            }

            RefPtr<API::URLRequest> originalRequest = API::URLRequest::create(originalResourceRequest);
            RefPtr<API::URLRequest> request = API::URLRequest::create(resourceRequest);

            if (m_client.decidePolicyForNavigationAction_deprecatedForUseWithV0)
                m_client.decidePolicyForNavigationAction_deprecatedForUseWithV0(toAPI(page), toAPI(frame), toAPI(navigationActionData.navigationType), toAPI(navigationActionData.modifiers), toAPI(navigationActionData.mouseButton), toAPI(request.get()), toAPI(listener), toAPI(userData), m_client.base.clientInfo);
            else if (m_client.decidePolicyForNavigationAction_deprecatedForUseWithV1)
                m_client.decidePolicyForNavigationAction_deprecatedForUseWithV1(toAPI(page), toAPI(frame), toAPI(navigationActionData.navigationType), toAPI(navigationActionData.modifiers), toAPI(navigationActionData.mouseButton), toAPI(originatingFrame), toAPI(request.get()), toAPI(listener), toAPI(userData), m_client.base.clientInfo);
            else
                m_client.decidePolicyForNavigationAction(toAPI(page), toAPI(frame), toAPI(navigationActionData.navigationType), toAPI(navigationActionData.modifiers), toAPI(navigationActionData.mouseButton), toAPI(originatingFrame), toAPI(originalRequest.get()), toAPI(request.get()), toAPI(listener), toAPI(userData), m_client.base.clientInfo);
        }

        virtual void decidePolicyForNewWindowAction(WebPageProxy* page, WebFrameProxy* frame, NavigationType type, WebEvent::Modifiers modifiers, WebMouseEvent::Button mouseButton, const ResourceRequest& resourceRequest, const String& frameName, WebFramePolicyListenerProxy* listener, API::Object* userData) override
        {
            if (!m_client.decidePolicyForNewWindowAction) {
                listener->use();
                return;
            }

            RefPtr<API::URLRequest> request = API::URLRequest::create(resourceRequest);

            m_client.decidePolicyForNewWindowAction(toAPI(page), toAPI(frame), toAPI(type), toAPI(modifiers), toAPI(mouseButton), toAPI(request.get()), toAPI(frameName.impl()), toAPI(listener), toAPI(userData), m_client.base.clientInfo);
        }

        virtual void decidePolicyForResponse(WebPageProxy* page, WebFrameProxy* frame, const ResourceResponse& resourceResponse, const ResourceRequest& resourceRequest, bool canShowMIMEType, WebFramePolicyListenerProxy* listener, API::Object* userData) override
        {
            if (!m_client.decidePolicyForResponse_deprecatedForUseWithV0 && !m_client.decidePolicyForResponse) {
                listener->use();
                return;
            }

            RefPtr<API::URLResponse> response = API::URLResponse::create(resourceResponse);
            RefPtr<API::URLRequest> request = API::URLRequest::create(resourceRequest);

            if (m_client.decidePolicyForResponse_deprecatedForUseWithV0)
                m_client.decidePolicyForResponse_deprecatedForUseWithV0(toAPI(page), toAPI(frame), toAPI(response.get()), toAPI(request.get()), toAPI(listener), toAPI(userData), m_client.base.clientInfo);
            else
                m_client.decidePolicyForResponse(toAPI(page), toAPI(frame), toAPI(response.get()), toAPI(request.get()), canShowMIMEType, toAPI(listener), toAPI(userData), m_client.base.clientInfo);
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
    toImpl(pageRef)->initializeUIClient(wkClient);
}

void WKPageSetSession(WKPageRef pageRef, WKSessionRef session)
{
    toImpl(pageRef)->setSession(*toImpl(session));
}

void WKPageRunJavaScriptInMainFrame(WKPageRef pageRef, WKStringRef scriptRef, void* context, WKPageRunJavaScriptFunction callback)
{
    toImpl(pageRef)->runJavaScriptInMainFrame(toImpl(scriptRef)->string(), ScriptValueCallback::create(context, callback));
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

void WKPageRenderTreeExternalRepresentation(WKPageRef pageRef, void* context, WKPageRenderTreeExternalRepresentationFunction callback)
{
    toImpl(pageRef)->getRenderTreeExternalRepresentation(StringCallback::create(context, callback));
}

void WKPageGetSourceForFrame(WKPageRef pageRef, WKFrameRef frameRef, void* context, WKPageGetSourceForFrameFunction callback)
{
    toImpl(pageRef)->getSourceForFrame(toImpl(frameRef), StringCallback::create(context, callback));
}

void WKPageGetContentsAsString(WKPageRef pageRef, void* context, WKPageGetContentsAsStringFunction callback)
{
    toImpl(pageRef)->getContentsAsString(StringCallback::create(context, callback));
}

void WKPageGetSelectionAsWebArchiveData(WKPageRef pageRef, void* context, WKPageGetSelectionAsWebArchiveDataFunction callback)
{
    toImpl(pageRef)->getSelectionAsWebArchiveData(DataCallback::create(context, callback));
}

void WKPageGetContentsAsMHTMLData(WKPageRef pageRef, bool useBinaryEncoding, void* context, WKPageGetContentsAsMHTMLDataFunction callback)
{
#if ENABLE(MHTML)
    toImpl(pageRef)->getContentsAsMHTMLData(DataCallback::create(context, callback), useBinaryEncoding);
#else
    UNUSED_PARAM(pageRef);
    UNUSED_PARAM(useBinaryEncoding);
    UNUSED_PARAM(context);
    UNUSED_PARAM(callback);
#endif
}

void WKPageForceRepaint(WKPageRef pageRef, void* context, WKPageForceRepaintFunction callback)
{
    toImpl(pageRef)->forceRepaint(VoidCallback::create(context, callback));
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
    toImpl(pageRef)->validateCommand(toImpl(command)->string(), ValidateCommandCallback::create(context, callback)); 
}

void WKPageExecuteCommand(WKPageRef pageRef, WKStringRef command)
{
    toImpl(pageRef)->executeEditCommand(toImpl(command)->string());
}

#if PLATFORM(MAC)
struct ComputedPagesContext {
    ComputedPagesContext(WKPageComputePagesForPrintingFunction callback, void* context)
        : callback(callback)
        , context(context)
    {
    }
    WKPageComputePagesForPrintingFunction callback;
    void* context;
};

static void computedPagesCallback(const Vector<WebCore::IntRect>& rects, double scaleFactor, WKErrorRef error, void* untypedContext)
{
    OwnPtr<ComputedPagesContext> context = adoptPtr(static_cast<ComputedPagesContext*>(untypedContext));
    Vector<WKRect> wkRects(rects.size());
    for (size_t i = 0; i < rects.size(); ++i)
        wkRects[i] = toAPI(rects[i]);
    context->callback(wkRects.data(), wkRects.size(), scaleFactor, error, context->context);
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
    toImpl(page)->computePagesForPrinting(toImpl(frame), printInfoFromWKPrintInfo(printInfo), ComputedPagesCallback::create(new ComputedPagesContext(callback, context), computedPagesCallback));
}

void WKPageBeginPrinting(WKPageRef page, WKFrameRef frame, WKPrintInfo printInfo)
{
    toImpl(page)->beginPrinting(toImpl(frame), printInfoFromWKPrintInfo(printInfo));
}

void WKPageDrawPagesToPDF(WKPageRef page, WKFrameRef frame, WKPrintInfo printInfo, uint32_t first, uint32_t count, WKPageDrawToPDFFunction callback, void* context)
{
    toImpl(page)->drawPagesToPDF(toImpl(frame), printInfoFromWKPrintInfo(printInfo), first, count, DataCallback::create(context, callback));
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
    return toAPI(toImpl(pageRef)->relatedPages().leakRef());
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
