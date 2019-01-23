/*
 * Copyright (C) 2017 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2,1 of the License, or (at your option) any later version.
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
#include "WebKitRemoteInspectorProtocolHandler.h"

#if ENABLE(REMOTE_INSPECTOR)

#include "APIUserContentWorld.h"
#include "WebKitError.h"
#include "WebKitNavigationPolicyDecision.h"
#include "WebKitUserContentManagerPrivate.h"
#include "WebKitWebContextPrivate.h"
#include "WebScriptMessageHandler.h"
#include <wtf/URL.h>

namespace WebKit {
using namespace WebCore;

class ScriptMessageClient final : public WebScriptMessageHandler::Client {
public:
    ScriptMessageClient(RemoteInspectorProtocolHandler& inspectorProtocolHandler)
        : m_inspectorProtocolHandler(inspectorProtocolHandler)
    {
    }

    void didPostMessage(WebPageProxy& page, const FrameInfoData&, WebCore::SerializedScriptValue& serializedScriptValue) override
    {
        String message = serializedScriptValue.toString();
        Vector<String> tokens = message.split(':');
        if (tokens.size() != 2)
            return;

        URL requestURL = URL({ }, page.pageLoadState().url());
        m_inspectorProtocolHandler.inspect(requestURL.hostAndPort(), tokens[0].toUInt64(), tokens[1].toUInt64());
    }

    ~ScriptMessageClient() { }

private:
    RemoteInspectorProtocolHandler& m_inspectorProtocolHandler;
};

RemoteInspectorProtocolHandler::RemoteInspectorProtocolHandler(WebKitWebContext* context)
    : m_context(context)
{
    webkit_web_context_register_uri_scheme(context, "inspector", [](WebKitURISchemeRequest* request, gpointer userData) {
        static_cast<RemoteInspectorProtocolHandler*>(userData)->handleRequest(request);
    }, this, nullptr);
}

RemoteInspectorProtocolHandler::~RemoteInspectorProtocolHandler()
{
    for (auto* webView : m_webViews)
        g_object_weak_unref(G_OBJECT(webView), reinterpret_cast<GWeakNotify>(webViewDestroyed), this);

    for (auto* userContentManager : m_userContentManagers) {
        webkitUserContentManagerGetUserContentControllerProxy(userContentManager)->removeUserMessageHandlerForName("inspector", API::UserContentWorld::normalWorld());
        g_object_weak_unref(G_OBJECT(userContentManager), reinterpret_cast<GWeakNotify>(userContentManagerDestroyed), this);
    }
}

void RemoteInspectorProtocolHandler::webViewDestroyed(RemoteInspectorProtocolHandler* inspectorProtocolHandler, WebKitWebView* webView)
{
    inspectorProtocolHandler->m_webViews.remove(webView);
}

void RemoteInspectorProtocolHandler::userContentManagerDestroyed(RemoteInspectorProtocolHandler* inspectorProtocolHandler, WebKitUserContentManager* userContentManager)
{
    inspectorProtocolHandler->m_userContentManagers.remove(userContentManager);
}

void RemoteInspectorProtocolHandler::handleRequest(WebKitURISchemeRequest* request)
{
    URL requestURL = URL({ }, webkit_uri_scheme_request_get_uri(request));
    if (!requestURL.port()) {
        GUniquePtr<GError> error(g_error_new_literal(WEBKIT_POLICY_ERROR, WEBKIT_POLICY_ERROR_CANNOT_SHOW_URI, "Cannot show inspector URL: no port provided"));
        webkit_uri_scheme_request_finish_error(request, error.get());
        return;
    }

    auto* webView = webkit_uri_scheme_request_get_web_view(request);
    ASSERT(webView);
    auto webViewResult = m_webViews.add(webView);
    if (webViewResult.isNewEntry)
        g_object_weak_ref(G_OBJECT(webView), reinterpret_cast<GWeakNotify>(webViewDestroyed), this);

    auto* userContentManager = webkit_web_view_get_user_content_manager(webView);
    auto userContentManagerResult = m_userContentManagers.add(userContentManager);
    if (userContentManagerResult.isNewEntry) {
        auto handler = WebScriptMessageHandler::create(std::make_unique<ScriptMessageClient>(*this), "inspector", API::UserContentWorld::normalWorld());
        webkitUserContentManagerGetUserContentControllerProxy(userContentManager)->addUserScriptMessageHandler(handler.get());
        g_object_weak_ref(G_OBJECT(userContentManager), reinterpret_cast<GWeakNotify>(userContentManagerDestroyed), this);
    }

    auto* client = m_inspectorClients.ensure(requestURL.hostAndPort(), [this, &requestURL] {
        return std::make_unique<RemoteInspectorClient>(requestURL.host().utf8().data(), requestURL.port().value(), *this);
    }).iterator->value.get();

    GString* html = g_string_new(
        "<html><head><title>Remote inspector</title>"
        "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />"
        "<style>"
        "  h1 { color: #babdb6; text-shadow: 0 1px 0 white; margin-bottom: 0; }"
        "  html { font-family: -webkit-system-font; font-size: 11pt; color: #2e3436; padding: 20px 20px 0 20px; background-color: #f6f6f4; "
        "         background-image: -webkit-gradient(linear, left top, left bottom, color-stop(0, #eeeeec), color-stop(1, #f6f6f4));"
        "         background-size: 100% 5em; background-repeat: no-repeat; }"
        "  table { width: 100%; border-collapse: collapse; }"
        "  table, td { border: 1px solid #d3d7cf; border-left: none; border-right: none; }"
        "  p { margin-bottom: 30px; }"
        "  td { padding: 15px; }"
        "  td.data { width: 200px; }"
        "  .targetname { font-weight: bold; }"
        "  .targeturl { color: #babdb6; }"
        "  td.input { width: 64px; }"
        "  input { width: 100%; padding: 8px; }"
        "</style>"
        "</head><body><h1>Inspectable targets</h1>");
    if (client->targets().isEmpty())
        g_string_append(html, "<p>No targets found</p>");
    else {
        g_string_append(html, "<table>");
        for (auto connectionID : client->targets().keys()) {
            for (auto& target : client->targets().get(connectionID)) {
                g_string_append_printf(html,
                    "<tbody><tr>"
                    "<td class=\"data\"><div class=\"targetname\">%s</div><div class=\"targeturl\">%s</div></td>"
                    "<td class=\"input\"><input type=\"button\" value=\"Inspect\" onclick=\"window.webkit.messageHandlers.inspector.postMessage('%" G_GUINT64_FORMAT ":%" G_GUINT64_FORMAT "');\"></td>"
                    "</tr></tbody>", target.name.data(), target.url.data(), connectionID, target.id);
            }
        }
        g_string_append(html, "</table>");
    }
    g_string_append(html, "</body></html>");
    gsize streamLength = html->len;
    GRefPtr<GInputStream> stream = adoptGRef(g_memory_input_stream_new_from_data(g_string_free(html, FALSE), streamLength, g_free));
    webkit_uri_scheme_request_finish(request, stream.get(), streamLength, "text/html");
}

void RemoteInspectorProtocolHandler::inspect(const String& hostAndPort, uint64_t connectionID, uint64_t tatgetID)
{
    if (auto* client = m_inspectorClients.get(hostAndPort))
        client->inspect(connectionID, tatgetID);
}

void RemoteInspectorProtocolHandler::targetListChanged(RemoteInspectorClient& client)
{
    Vector<WebKitWebView*, 4> webViewsToRemove;
    for (auto* webView : m_webViews) {
        if (webkit_web_view_is_loading(webView))
            continue;

        URL webViewURL = URL({ }, webkit_web_view_get_uri(webView));
        auto clientForWebView = m_inspectorClients.get(webViewURL.hostAndPort());
        if (!clientForWebView) {
            // This view is not showing a inspector view anymore.
            webViewsToRemove.append(webView);
        } else if (clientForWebView == &client)
            webkit_web_view_reload(webView);
    }

    for (auto* webView : webViewsToRemove) {
        g_object_weak_unref(G_OBJECT(webView), reinterpret_cast<GWeakNotify>(webViewDestroyed), this);
        m_webViews.remove(webView);
    }
}

void RemoteInspectorProtocolHandler::connectionClosed(RemoteInspectorClient& client)
{
    targetListChanged(client);
    m_inspectorClients.remove(client.hostAndPort());
}

} // namespace WebKit

#endif // ENABLE(REMOTE_INSPECTOR)
