/*
 * Copyright (C) 2012 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
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
#include "WebKitDownloadClient.h"

#include "APIDownloadClient.h"
#include "WebKitDownloadPrivate.h"
#include "WebKitURIResponsePrivate.h"
#include "WebKitWebViewPrivate.h"
#include "WebsiteDataStore.h"
#include <WebCore/UserAgent.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/text/CString.h>

using namespace WebCore;
using namespace WebKit;

class DownloadClient final : public API::DownloadClient {
public:
    explicit DownloadClient(GRefPtr<WebKitDownload>&& download)
        : m_download(WTFMove(download))
    {
    }

private:
    void willSendRequest(DownloadProxy& downloadProxy, ResourceRequest&& request, const ResourceResponse&, CompletionHandler<void(ResourceRequest&&)>&& completionHandler) override
    {
        ASSERT(m_download);
        if (!request.hasHTTPHeaderField(HTTPHeaderName::UserAgent)) {
            auto* webView = webkit_download_get_web_view(m_download.get());
            request.setHTTPUserAgent(webView ? webkitWebViewGetPage(webView).userAgentForURL(request.url()) : WebPageProxy::standardUserAgent());
        }

        completionHandler(WTFMove(request));
    }

    void didReceiveAuthenticationChallenge(DownloadProxy& downloadProxy, AuthenticationChallengeProxy& authenticationChallenge) override
    {
        ASSERT(m_download);
        if (webkitDownloadIsCancelled(m_download.get()))
            return;

        // FIXME: Add API to handle authentication of downloads without a web view associted.
        if (auto* webView = webkit_download_get_web_view(m_download.get()))
            webkitWebViewHandleAuthenticationChallenge(webView, &authenticationChallenge);
    }

    void didReceiveResponse(DownloadProxy& downloadProxy, const ResourceResponse& resourceResponse)
    {
        ASSERT(m_download);
        if (webkitDownloadIsCancelled(m_download.get()))
            return;

        GRefPtr<WebKitURIResponse> response = adoptGRef(webkitURIResponseCreateForResourceResponse(resourceResponse));
        webkitDownloadSetResponse(m_download.get(), response.get());
    }

    void didReceiveData(DownloadProxy& downloadProxy, uint64_t length, uint64_t, uint64_t) override
    {
        ASSERT(m_download);
        webkitDownloadNotifyProgress(m_download.get(), length);
    }

    void decideDestinationWithSuggestedFilename(DownloadProxy& downloadProxy, const ResourceResponse& resourceResponse, const String& filename, CompletionHandler<void(AllowOverwrite, String)>&& completionHandler) override
    {
        ASSERT(m_download);
        didReceiveResponse(downloadProxy, resourceResponse);
        webkitDownloadDecideDestinationWithSuggestedFilename(m_download.get(), filename.utf8(), WTFMove(completionHandler));
    }

    void didCreateDestination(DownloadProxy& downloadProxy, const String& path) override
    {
        ASSERT(m_download);
        webkitDownloadDestinationCreated(m_download.get(), path);
    }

    void didFail(DownloadProxy& downloadProxy, const ResourceError& error, API::Data*) override
    {
        if (webkitDownloadIsCancelled(m_download.get()))
            return;

        ASSERT(m_download);
        webkitDownloadFailed(m_download.get(), error);
        m_download = nullptr;
    }

    void didFinish(DownloadProxy& downloadProxy) override
    {
        if (webkitDownloadIsCancelled(m_download.get()))
            return;

        ASSERT(m_download);
        webkitDownloadFinished(m_download.get());
        m_download = nullptr;
    }

    void legacyDidCancel(WebKit::DownloadProxy&) override
    {
        ASSERT(m_download);
        webkitDownloadCancelled(m_download.get());
        m_download = nullptr;
    }

    void processDidCrash(DownloadProxy&) override
    {
        m_download = nullptr;
    }

    GRefPtr<WebKitDownload> m_download;
};

void attachDownloadClientToDownload(GRefPtr<WebKitDownload>&& download, DownloadProxy& downloadProxy)
{
    downloadProxy.setClient(adoptRef(*new DownloadClient(WTFMove(download))));
}
