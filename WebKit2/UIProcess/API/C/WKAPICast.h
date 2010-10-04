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

#ifndef WKAPICast_h
#define WKAPICast_h

#include "CacheModel.h"
#include "WKContext.h"
#include "WKPage.h"
#include "WKSharedAPICast.h"
#include <WebCore/FrameLoaderTypes.h>

namespace WebKit {

class WebBackForwardList;
class WebBackForwardListItem;
class WebContext;
class WebFormSubmissionListenerProxy;
class WebFramePolicyListenerProxy;
class WebFrameProxy;
class WebNavigationData;
class WebPageNamespace;
class WebPageProxy;
class WebPreferences;

WK_ADD_API_MAPPING(WKBackForwardListItemRef, WebBackForwardListItem)
WK_ADD_API_MAPPING(WKBackForwardListRef, WebBackForwardList)
WK_ADD_API_MAPPING(WKContextRef, WebContext)
WK_ADD_API_MAPPING(WKFormSubmissionListenerRef, WebFormSubmissionListenerProxy)
WK_ADD_API_MAPPING(WKFramePolicyListenerRef, WebFramePolicyListenerProxy)
WK_ADD_API_MAPPING(WKFrameRef, WebFrameProxy)
WK_ADD_API_MAPPING(WKNavigationDataRef, WebNavigationData)
WK_ADD_API_MAPPING(WKPageNamespaceRef, WebPageNamespace)
WK_ADD_API_MAPPING(WKPageRef, WebPageProxy)
WK_ADD_API_MAPPING(WKPreferencesRef, WebPreferences)

/* Enum conversions */

inline WKFrameNavigationType toRef(WebCore::NavigationType type)
{
    WKFrameNavigationType wkType = kWKFrameNavigationTypeOther;

    switch (type) {
    case WebCore::NavigationTypeLinkClicked:
        wkType = kWKFrameNavigationTypeLinkClicked;
        break;
    case WebCore::NavigationTypeFormSubmitted:
        wkType = kWKFrameNavigationTypeFormSubmitted;
        break;
    case WebCore::NavigationTypeBackForward:
        wkType = kWKFrameNavigationTypeBackForward;
        break;
    case WebCore::NavigationTypeReload:
        wkType = kWKFrameNavigationTypeReload;
        break;
    case WebCore::NavigationTypeFormResubmitted:
        wkType = kWKFrameNavigationTypeFormResubmitted;
        break;
    case WebCore::NavigationTypeOther:
        wkType = kWKFrameNavigationTypeOther;
        break;
    }
    
    return wkType;
}

inline CacheModel toCacheModel(WKCacheModel wkCacheModel)
{
    switch (wkCacheModel) {
    case kWKCacheModelDocumentViewer:
        return CacheModelDocumentViewer;
    case kWKCacheModelDocumentBrowser:
        return CacheModelDocumentBrowser;
    case kWKCacheModelPrimaryWebBrowser:
        return CacheModelPrimaryWebBrowser;
    }

    ASSERT_NOT_REACHED();
    return CacheModelDocumentViewer;
}

inline WKCacheModel toRef(CacheModel cacheModel)
{
    switch (cacheModel) {
    case CacheModelDocumentViewer:
        return kWKCacheModelDocumentViewer;
    case CacheModelDocumentBrowser:
        return kWKCacheModelDocumentBrowser;
    case CacheModelPrimaryWebBrowser:
        return kWKCacheModelPrimaryWebBrowser;
    }
    
    return kWKCacheModelDocumentViewer;
}

} // namespace WebKit

#if defined(WIN32) || defined(_WIN32)
#include "WKAPICastWin.h"
#endif

#endif // WKAPICast_h
