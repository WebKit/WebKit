/*
 * Copyright (C) 2017 Igalia S.L.
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
#include "WebKitIconLoadingClient.h"

#include "APIIconLoadingClient.h"
#include "PageLoadState.h"
#include "WebKitFaviconDatabasePrivate.h"
#include "WebKitWebViewPrivate.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/glib/GWeakPtr.h>

using namespace WebKit;

class IconLoadingClient : public API::IconLoadingClient {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(IconLoadingClient);
public:
    explicit IconLoadingClient(WebKitWebView* webView)
        : m_webView(webView)
    {
    }

private:
    void getLoadDecisionForIcons(const HashMap<WebKit::CallbackID, WebCore::LinkIcon>& icons, CompletionHandler<void(HashSet<WebKit::CallbackID>&&)>&& completionHandler) override
    {
        m_pendingIcons.clear();
        webkitWebViewGetLoadDecisionForIcons(m_webView, icons, [this, weakWebView = GWeakPtr { m_webView }, completionHandler = WTF::move(completionHandler)](HashSet<CallbackID>&& loadIdentifiers) mutable {
            if (!weakWebView)
                return completionHandler({ });

            if (loadIdentifiers.isEmpty())
                finishedLoadingIcons();
            else
                m_pendingIcons = loadIdentifiers;

            completionHandler(WTF::move(loadIdentifiers));
        });
    }

    void iconLoaded(const WebKit::CallbackID& loadIdentifier, const WebCore::LinkIcon& icon, API::Data* iconData) override
    {
        m_pendingIcons.remove(loadIdentifier);

        if (iconData)
            webkitWebViewSetIcon(m_webView, icon, *iconData);

        if (m_pendingIcons.isEmpty())
            finishedLoadingIcons();
    }

    void finishedLoadingIcons()
    {
#if ENABLE(2022_GLIB_API)
        webkitWebViewUpdatePageIcons(m_webView);
#endif
    }

    HashSet<CallbackID> m_pendingIcons;

    WebKitWebView* m_webView;
};

void attachIconLoadingClientToView(WebKitWebView* webView)
{
    webkitWebViewGetPage(webView).setIconLoadingClient(makeUnique<IconLoadingClient>(webView));
}
