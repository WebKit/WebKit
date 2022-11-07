/*
 * Copyright (C) 2022 Igalia S.L.
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
#include "WebKitWebResourceLoadManager.h"

#include "WebFrameProxy.h"
#include "WebKitWebResourcePrivate.h"
#include "WebKitWebViewPrivate.h"

namespace WebKit {
using namespace WebCore;

WebKitWebResourceLoadManager::WebKitWebResourceLoadManager(WebKitWebView* webView)
    : m_webView(webView)
{
    g_signal_connect(m_webView, "load-changed", G_CALLBACK(+[](WebKitWebView*, WebKitLoadEvent loadEvent, WebKitWebResourceLoadManager* manager) {
        if (loadEvent == WEBKIT_LOAD_STARTED)
            manager->m_resources.clear();
    }), this);
}

WebKitWebResourceLoadManager::~WebKitWebResourceLoadManager()
{
    g_signal_handlers_disconnect_by_data(m_webView, this);
}

void WebKitWebResourceLoadManager::didInitiateLoad(ResourceLoaderIdentifier resourceID, FrameIdentifier frameID, ResourceRequest&& request)
{
    auto* frame = WebFrameProxy::webFrame(frameID);
    if (!frame)
        return;

    GRefPtr<WebKitWebResource> resource = adoptGRef(webkitWebResourceCreate(*frame, request));
    m_resources.set({ resourceID, frameID }, resource);
    webkitWebViewResourceLoadStarted(m_webView, resource.get(), WTFMove(request));
}

void WebKitWebResourceLoadManager::didSendRequest(ResourceLoaderIdentifier resourceID, FrameIdentifier frameID, ResourceRequest&& request, ResourceResponse&& redirectResponse)
{
    if (auto* resource = m_resources.get({ resourceID, frameID }))
        webkitWebResourceSentRequest(resource, WTFMove(request), WTFMove(redirectResponse));
}

void WebKitWebResourceLoadManager::didReceiveResponse(ResourceLoaderIdentifier resourceID, FrameIdentifier frameID, ResourceResponse&& response)
{
    if (auto* resource = m_resources.get({ resourceID, frameID }))
        webkitWebResourceSetResponse(resource, WTFMove(response));
}

void WebKitWebResourceLoadManager::didFinishLoad(ResourceLoaderIdentifier resourceID, FrameIdentifier frameID, ResourceError&& error)
{
    auto resource = m_resources.take({ resourceID, frameID });
    if (!resource)
        return;

    if (error.isNull())
        webkitWebResourceFinished(resource.get());
    else
        webkitWebResourceFailed(resource.get(), WTFMove(error));
}
} // namespace WebKit
