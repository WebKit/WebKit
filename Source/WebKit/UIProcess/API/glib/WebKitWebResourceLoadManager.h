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

#pragma once

#include "WebKitWebResource.h"
#include "WebKitWebView.h"
#include <WebCore/GlobalFrameIdentifier.h>
#include <WebCore/ResourceLoaderIdentifier.h>
#include <wtf/HashMap.h>
#include <wtf/glib/GRefPtr.h>

namespace WebCore {
class ResourceError;
class ResourceRequest;
class ResourceResponse;
}

namespace WebKit {

class WebKitWebResourceLoadManager {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit WebKitWebResourceLoadManager(WebKitWebView*);
    ~WebKitWebResourceLoadManager();

    void didInitiateLoad(WebCore::ResourceLoaderIdentifier, WebCore::FrameIdentifier, WebCore::ResourceRequest&&);
    void didSendRequest(WebCore::ResourceLoaderIdentifier, WebCore::FrameIdentifier, WebCore::ResourceRequest&&, WebCore::ResourceResponse&&);
    void didReceiveResponse(WebCore::ResourceLoaderIdentifier, WebCore::FrameIdentifier, WebCore::ResourceResponse&&);
    void didFinishLoad(WebCore::ResourceLoaderIdentifier, WebCore::FrameIdentifier, WebCore::ResourceError&&);

private:
    WebKitWebView* m_webView { nullptr };
    HashMap<std::pair<WebCore::ResourceLoaderIdentifier, WebCore::FrameIdentifier>, GRefPtr<WebKitWebResource>> m_resources;
};

} // namespace WebKit
