/*
 * Copyright (C) 2007 Holger Hans Peter Freyther
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

#include "webkitprivate.h"
#include "ChromeClientGtk.h"
#include "FrameLoader.h"
#include "FrameLoaderClientGtk.h"
#include "Logging.h"
#include "NotImplemented.h"
#include "PageCache.h"
#include "PageGroup.h"
#include "Pasteboard.h"
#include "PasteboardHelperGtk.h"
#include <runtime/InitializeThreading.h>

#if ENABLE(DATABASE)
#include "DatabaseTracker.h"
#endif

using namespace WebCore;

namespace WebKit {

WebKitWebView* getViewFromFrame(WebKitWebFrame* frame)
{
    WebKitWebFramePrivate* priv = frame->priv;
    return priv->webView;
}

WebCore::Frame* core(WebKitWebFrame* frame)
{
    if (!frame)
        return 0;

    WebKitWebFramePrivate* priv = frame->priv;
    return priv ? priv->coreFrame.get() : 0;
}

WebKitWebFrame* kit(WebCore::Frame* coreFrame)
{
    if (!coreFrame)
        return 0;

    ASSERT(coreFrame->loader());
    WebKit::FrameLoaderClient* client = static_cast<WebKit::FrameLoaderClient*>(coreFrame->loader()->client());
    return client ? client->webFrame() : 0;
}

WebCore::Page* core(WebKitWebView* webView)
{
    if (!webView)
        return 0;

    WebKitWebViewPrivate* priv = webView->priv;
    return priv ? priv->corePage : 0;
}

WebKitWebView* kit(WebCore::Page* corePage)
{
    if (!corePage)
        return 0;

    ASSERT(corePage->chrome());
    WebKit::ChromeClient* client = static_cast<WebKit::ChromeClient*>(corePage->chrome()->client());
    return client ? client->webView() : 0;
}

} /** end namespace WebKit */

void webkit_init()
{
    static bool isInitialized = false;
    if (isInitialized)
        return;
    isInitialized = true;

    JSC::initializeThreading();
    WebCore::InitializeLoggingChannelsIfNecessary();

    // Page cache capacity (in pages). Comment from Mac port:
    // (Research indicates that value / page drops substantially after 3 pages.)
    // FIXME: Expose this with an API and/or calculate based on available resources
    WebCore::pageCache()->setCapacity(3);

#if ENABLE(DATABASE)
    // FIXME: It should be possible for client applications to override this default location
    gchar* databaseDirectory = g_build_filename(g_get_user_data_dir(), "webkit", "databases", NULL);
    WebCore::DatabaseTracker::tracker().setDatabaseDirectoryPath(databaseDirectory);
    g_free(databaseDirectory);
#endif

    PageGroup::setShouldTrackVisitedLinks(true);

    Pasteboard::generalPasteboard()->setHelper(new WebKit::PasteboardHelperGtk());
}
