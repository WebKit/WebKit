/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
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
#include "WebPageProxy.h"

#include "InputMethodState.h"
#include "PageClientImpl.h"
#include "WebKitUserMessage.h"
#include "WebKitWebViewBasePrivate.h"
#include "WebKitWebViewPrivate.h"
#include "WebPageMessages.h"
#include "WebPasteboardProxy.h"
#include "WebProcessProxy.h"
#include "WebsiteDataStore.h"
#include <WebCore/NotImplemented.h>
#include <WebCore/PlatformDisplay.h>
#include <WebCore/UserAgent.h>
#include <gtk/gtkx.h>
#include <wtf/NeverDestroyed.h>

namespace WebKit {

void WebPageProxy::platformInitialize()
{
}

GtkWidget* WebPageProxy::viewWidget()
{
    return static_cast<PageClientImpl&>(pageClient()).viewWidget();
}

String WebPageProxy::standardUserAgent(const String& applicationNameForUserAgent)
{
    return WebCore::standardUserAgent(applicationNameForUserAgent);
}

void WebPageProxy::bindAccessibilityTree(const String& plugID)
{
    auto* accessible = gtk_widget_get_accessible(viewWidget());
    atk_socket_embed(ATK_SOCKET(accessible), const_cast<char*>(plugID.utf8().data()));
    atk_object_notify_state_change(accessible, ATK_STATE_TRANSIENT, FALSE);
}

void WebPageProxy::saveRecentSearches(const String&, const Vector<WebCore::RecentSearch>&)
{
    notImplemented();
}

void WebPageProxy::loadRecentSearches(const String&, CompletionHandler<void(Vector<WebCore::RecentSearch>&&)>&& completionHandler)
{
    notImplemented();
    completionHandler({ });
}

void WebsiteDataStore::platformRemoveRecentSearches(WallTime oldestTimeToRemove)
{
    UNUSED_PARAM(oldestTimeToRemove);
    notImplemented();
}

void WebPageProxy::updateEditorState(const EditorState& editorState)
{
    m_editorState = editorState;
    
    if (editorState.shouldIgnoreSelectionChanges)
        return;
    if (m_editorState.selectionIsRange)
        WebPasteboardProxy::singleton().setPrimarySelectionOwner(focusedFrame());
    pageClient().selectionDidChange();
}

#if PLATFORM(X11)
typedef HashMap<uint64_t, GtkWidget* > PluginWindowMap;
static PluginWindowMap& pluginWindowMap()
{
    static NeverDestroyed<PluginWindowMap> map;
    return map;
}

static gboolean pluginContainerPlugRemoved(GtkSocket* socket)
{
    uint64_t windowID = static_cast<uint64_t>(gtk_socket_get_id(socket));
    pluginWindowMap().remove(windowID);
    return FALSE;
}

void WebPageProxy::createPluginContainer(CompletionHandler<void(uint64_t)>&& completionHandler)
{
    RELEASE_ASSERT(WebCore::PlatformDisplay::sharedDisplay().type() == WebCore::PlatformDisplay::Type::X11);
    GtkWidget* socket = gtk_socket_new();
    g_signal_connect(socket, "plug-removed", G_CALLBACK(pluginContainerPlugRemoved), 0);
    gtk_container_add(GTK_CONTAINER(viewWidget()), socket);

    uint64_t windowID = static_cast<uint64_t>(gtk_socket_get_id(GTK_SOCKET(socket)));
    pluginWindowMap().set(windowID, socket);
    completionHandler(windowID);
}

void WebPageProxy::windowedPluginGeometryDidChange(const WebCore::IntRect& frameRect, const WebCore::IntRect& clipRect, uint64_t windowID)
{
    GtkWidget* plugin = pluginWindowMap().get(windowID);
    if (!plugin)
        return;

    if (gtk_widget_get_realized(plugin)) {
        GdkRectangle clip = clipRect;
        cairo_region_t* clipRegion = cairo_region_create_rectangle(&clip);
        gdk_window_shape_combine_region(gtk_widget_get_window(plugin), clipRegion, 0, 0);
        cairo_region_destroy(clipRegion);
    }

    webkitWebViewBaseChildMoveResize(WEBKIT_WEB_VIEW_BASE(viewWidget()), plugin, frameRect);
}

void WebPageProxy::windowedPluginVisibilityDidChange(bool isVisible, uint64_t windowID)
{
    GtkWidget* plugin = pluginWindowMap().get(windowID);
    if (!plugin)
        return;

    if (isVisible)
        gtk_widget_show(plugin);
    else
        gtk_widget_hide(plugin);
}
#endif // PLATFORM(X11)

void WebPageProxy::setInputMethodState(Optional<InputMethodState>&& state)
{
    webkitWebViewBaseSetInputMethodState(WEBKIT_WEB_VIEW_BASE(viewWidget()), WTFMove(state));
}

void WebPageProxy::getCenterForZoomGesture(const WebCore::IntPoint& centerInViewCoordinates, WebCore::IntPoint& center)
{
    process().sendSync(Messages::WebPage::GetCenterForZoomGesture(centerInViewCoordinates), Messages::WebPage::GetCenterForZoomGesture::Reply(center), m_webPageID);
}

bool WebPageProxy::makeGLContextCurrent()
{
    return webkitWebViewBaseMakeGLContextCurrent(WEBKIT_WEB_VIEW_BASE(viewWidget()));
}

void WebPageProxy::showEmojiPicker(const WebCore::IntRect& caretRect, CompletionHandler<void(String)>&& completionHandler)
{
    webkitWebViewBaseShowEmojiChooser(WEBKIT_WEB_VIEW_BASE(viewWidget()), caretRect, WTFMove(completionHandler));
}

void WebPageProxy::sendMessageToWebViewWithReply(UserMessage&& message, CompletionHandler<void(UserMessage&&)>&& completionHandler)
{
    if (!WEBKIT_IS_WEB_VIEW(viewWidget())) {
        completionHandler(UserMessage(message.name, WEBKIT_USER_MESSAGE_UNHANDLED_MESSAGE));
        return;
    }

    webkitWebViewDidReceiveUserMessage(WEBKIT_WEB_VIEW(viewWidget()), WTFMove(message), WTFMove(completionHandler));
}

void WebPageProxy::sendMessageToWebView(UserMessage&& message)
{
    sendMessageToWebViewWithReply(WTFMove(message), [](UserMessage&&) { });
}

void WebPageProxy::themeDidChange()
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::ThemeDidChange(pageClient().themeName()));
    effectiveAppearanceDidChange();
}

} // namespace WebKit
