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
#include "WebKitWebContextPrivate.h"
#include "WebKitWebViewPrivate.h"
#include "WebsiteDataStore.h"
#include <WebCore/UserAgent.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/text/CString.h>

using namespace WebCore;
using namespace WebKit;

class DownloadClient final : public API::DownloadClient {
public:
    explicit DownloadClient(WebKitWebContext* webContext)
        : m_webContext(webContext)
    {
    }

private:
    void didStart(DownloadProxy& downloadProxy) override
    {
        GRefPtr<WebKitDownload> download = webkitWebContextGetOrCreateDownload(&downloadProxy);
        webkitDownloadStarted(download.get());
        webkitWebContextDownloadStarted(m_webContext, download.get());
    }

    void willSendRequest(DownloadProxy& downloadProxy, ResourceRequest&& request, const ResourceResponse&, CompletionHandler<void(ResourceRequest&&)>&& completionHandler) override
    {
        if (!request.hasHTTPHeaderField(HTTPHeaderName::UserAgent)) {
            GRefPtr<WebKitDownload> download = webkitWebContextGetOrCreateDownload(&downloadProxy);
            auto* webView = webkit_download_get_web_view(download.get());
            request.setHTTPUserAgent(webView ? webkitWebViewGetPage(webView).userAgentForURL(request.url()) : WebPageProxy::standardUserAgent());
        }

        completionHandler(WTFMove(request));
    }

    void didReceiveAuthenticationChallenge(DownloadProxy& downloadProxy, AuthenticationChallengeProxy& authenticationChallenge) override
    {
        GRefPtr<WebKitDownload> download = webkitWebContextGetOrCreateDownload(&downloadProxy);
        if (webkitDownloadIsCancelled(download.get()))
            return;

        // FIXME: Add API to handle authentication of downloads without a web view associted.
        if (auto* webView = webkit_download_get_web_view(download.get()))
            webkitWebViewHandleAuthenticationChallenge(webView, &authenticationChallenge);
    }

    void didReceiveResponse(DownloadProxy& downloadProxy, const ResourceResponse& resourceResponse) override
    {
        GRefPtr<WebKitDownload> download = webkitWebContextGetOrCreateDownload(&downloadProxy);
        if (webkitDownloadIsCancelled(download.get()))
            return;

        GRefPtr<WebKitURIResponse> response = adoptGRef(webkitURIResponseCreateForResourceResponse(resourceResponse));
        webkitDownloadSetResponse(download.get(), response.get());
    }

    void didReceiveData(DownloadProxy& downloadProxy, uint64_t length, uint64_t, uint64_t) override
    {
        GRefPtr<WebKitDownload> download = webkitWebContextGetOrCreateDownload(&downloadProxy);
        webkitDownloadNotifyProgress(download.get(), length);
    }

    void decideDestinationWithSuggestedFilename(DownloadProxy& downloadProxy, const String& filename, Function<void(AllowOverwrite, String)>&& completionHandler) override
    {
        GRefPtr<WebKitDownload> download = webkitWebContextGetOrCreateDownload(&downloadProxy);
        bool allowOverwrite = false;
        String destination = webkitDownloadDecideDestinationWithSuggestedFilename(download.get(), filename.utf8(), allowOverwrite);
        completionHandler(allowOverwrite ? AllowOverwrite::Yes : AllowOverwrite::No, destination);
    }

    void didCreateDestination(DownloadProxy& downloadProxy, const String& path) override
    {
        GRefPtr<WebKitDownload> download = webkitWebContextGetOrCreateDownload(&downloadProxy);
        webkitDownloadDestinationCreated(download.get(), path);
    }

    void didFail(DownloadProxy& downloadProxy, const ResourceError& error) override
    {
        GRefPtr<WebKitDownload> download = webkitWebContextGetOrCreateDownload(&downloadProxy);
        if (webkitDownloadIsCancelled(download.get())) {
            // Cancellation takes precedence over other errors.
            webkitDownloadCancelled(download.get());
        } else
            webkitDownloadFailed(download.get(), error);
        webkitWebContextRemoveDownload(&downloadProxy);
    }

    void didCancel(DownloadProxy& downloadProxy) override
    {
        GRefPtr<WebKitDownload> download = webkitWebContextGetOrCreateDownload(&downloadProxy);
        webkitDownloadCancelled(download.get());
        webkitWebContextRemoveDownload(&downloadProxy);
    }

    void didFinish(DownloadProxy& downloadProxy) override
    {
        GRefPtr<WebKitDownload> download = webkitWebContextGetOrCreateDownload(&downloadProxy);
        webkitDownloadFinished(download.get());
        webkitWebContextRemoveDownload(&downloadProxy);
    }

    WebKitWebContext* m_webContext;
};

void attachDownloadClientToContext(WebKitWebContext* webContext)
{
    webkitWebContextGetProcessPool(webContext).setDownloadClient(makeUniqueRef<DownloadClient>(webContext));
}
