/*
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2008 Jasper Bryant-Greene <jasper@unix.geek.nz>
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
#include "MainThread.h"
#include "NotImplemented.h"
#include "Pasteboard.h"
#include "PasteboardHelperGtk.h"
#include "MouseEvent.h"
#include "CString.h"

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

WebKitNavigationAction* kit(const WebCore::NavigationAction& action)
{
    WebKitNavigationAction* webKitAction = WEBKIT_NAVIGATION_ACTION(g_object_new(WEBKIT_TYPE_NAVIGATION_ACTION, NULL));
    WebKitNavigationActionPrivate* priv = webKitAction->priv;

    const Event* event = action.event();
    if (event && event->isMouseEvent()) {
        const MouseEvent* mouseEvent = static_cast<const MouseEvent*>(event);
        priv->button = mouseEvent->button();
    }

    const UIEventWithKeyState* keyStateEvent = findEventWithKeyState(const_cast<Event*>(event));
    if (keyStateEvent) {
        int modifierFlags = 0;

        if (keyStateEvent->shiftKey())
            modifierFlags |= GDK_SHIFT_MASK;
        if (keyStateEvent->ctrlKey())
            modifierFlags |= GDK_CONTROL_MASK;
        if (keyStateEvent->altKey())
            modifierFlags |= GDK_MOD1_MASK;
        if (keyStateEvent->metaKey())
            modifierFlags |= GDK_MOD2_MASK;

        priv->modifierFlags = modifierFlags;
    }

    priv->navigationType = action.type();
    priv->originalURL = g_strdup(action.url().string().utf8().data());

    return webKitAction;
}

} /** end namespace WebCore */

void webkit_init()
{
    static bool isInitialized = false;
    if (isInitialized)
        return;
    isInitialized = true;

    WebCore::initializeThreadingAndMainThread();
    WebCore::InitializeLoggingChannelsIfNecessary();

#if ENABLE(DATABASE)
    // FIXME: It should be possible for client applications to override this default location
    gchar* databaseDirectory = g_build_filename(g_get_user_data_dir(), "webkit", "databases", NULL);
    WebCore::DatabaseTracker::tracker().setDatabaseDirectoryPath(databaseDirectory);
    g_free(databaseDirectory);
#endif

    Pasteboard::generalPasteboard()->setHelper(new WebKit::PasteboardHelperGtk());
}
