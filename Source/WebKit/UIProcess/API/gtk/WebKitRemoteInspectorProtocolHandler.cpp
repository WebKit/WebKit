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

#include "APIContentWorld.h"
#include "WebKitError.h"
#include "WebKitNavigationPolicyDecision.h"
#include "WebKitUserContentManagerPrivate.h"
#include "WebKitWebContextPrivate.h"
#include "WebPageProxy.h"
#include "WebScriptMessageHandler.h"
#include <wtf/URL.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebKit {
using namespace WebCore;

class ScriptMessageClient final : public WebScriptMessageHandler::Client {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ScriptMessageClient(RemoteInspectorProtocolHandler& inspectorProtocolHandler)
        : m_inspectorProtocolHandler(inspectorProtocolHandler)
    {
    }

    void didPostMessage(WebPageProxy& page, FrameInfoData&&, API::ContentWorld&, WebCore::SerializedScriptValue& serializedScriptValue) override
    {
        String message = serializedScriptValue.toString();
        Vector<String> tokens = message.split(':');
        if (tokens.size() != 3)
            return;

        URL requestURL = URL({ }, page.pageLoadState().url());
        m_inspectorProtocolHandler.inspect(requestURL.hostAndPort(), parseIntegerAllowingTrailingJunk<uint64_t>(tokens[0]).value_or(0), parseIntegerAllowingTrailingJunk<uint64_t>(tokens[1]).value_or(0), tokens[2]);
    }

    bool supportsAsyncReply() override
    {
        return false;
    }
    
    void didPostMessageWithAsyncReply(WebPageProxy&, FrameInfoData&&, API::ContentWorld&, WebCore::SerializedScriptValue&, WTF::Function<void(API::SerializedScriptValue*, const String&)>&&) override
    {
    }

    ~ScriptMessageClient() { }

private:
    RemoteInspectorProtocolHandler& m_inspectorProtocolHandler;
};

RemoteInspectorProtocolHandler::RemoteInspectorProtocolHandler(WebKitWebContext* context)
{
    webkit_web_context_register_uri_scheme(context, "inspector", [](WebKitURISchemeRequest* request, gpointer userData) {
        static_cast<RemoteInspectorProtocolHandler*>(userData)->handleRequest(request);
    }, this, nullptr);
}

RemoteInspectorProtocolHandler::~RemoteInspectorProtocolHandler()
{
    for (auto* webView : m_webViews.keys()) {
        g_signal_handlers_disconnect_by_data(webView, this);
        g_object_weak_unref(G_OBJECT(webView), reinterpret_cast<GWeakNotify>(webViewDestroyed), this);
    }

    for (auto* userContentManager : m_userContentManagers) {
        webkitUserContentManagerGetUserContentControllerProxy(userContentManager)->removeUserMessageHandlerForName("inspector"_s, API::ContentWorld::pageContentWorld());
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
    URL requestURL = URL(String::fromLatin1(webkit_uri_scheme_request_get_uri(request)));
    if (!requestURL.port()) {
        GUniquePtr<GError> error(g_error_new_literal(WEBKIT_POLICY_ERROR, WEBKIT_POLICY_ERROR_CANNOT_SHOW_URI, "Cannot show inspector URL: no port provided"));
        webkit_uri_scheme_request_finish_error(request, error.get());
        return;
    }

    auto* webView = webkit_uri_scheme_request_get_web_view(request);
    ASSERT(webView);
    auto webViewResult = m_webViews.add(webView, nullptr);
    if (webViewResult.isNewEntry) {
        g_signal_connect(webView, "notify::uri", G_CALLBACK(+[](WebKitWebView* webView, GParamSpec*, RemoteInspectorProtocolHandler* handler) {
            URL webViewURL = URL(String::fromUTF8(webkit_web_view_get_uri(webView)));
            if (!webViewURL.protocolIs("inspector"_s) || !handler->m_inspectorClients.contains(webViewURL.hostAndPort())) {
                g_signal_handlers_disconnect_by_data(webView, handler);
                g_object_weak_unref(G_OBJECT(webView), reinterpret_cast<GWeakNotify>(webViewDestroyed), handler);
                handler->m_webViews.remove(webView);
            }
        }), this);
        g_object_weak_ref(G_OBJECT(webView), reinterpret_cast<GWeakNotify>(webViewDestroyed), this);
    }

    auto* userContentManager = webkit_web_view_get_user_content_manager(webView);
    auto userContentManagerResult = m_userContentManagers.add(userContentManager);
    if (userContentManagerResult.isNewEntry) {
        auto handler = WebScriptMessageHandler::create(makeUnique<ScriptMessageClient>(*this), "inspector"_s, API::ContentWorld::pageContentWorld());
        webkitUserContentManagerGetUserContentControllerProxy(userContentManager)->addUserScriptMessageHandler(handler.get());
        g_object_weak_ref(G_OBJECT(userContentManager), reinterpret_cast<GWeakNotify>(userContentManagerDestroyed), this);
    }

    auto* client = m_inspectorClients.ensure(requestURL.hostAndPort(), [this, &requestURL] {
        return makeUnique<RemoteInspectorClient>(requestURL.hostAndPort(), *this);
    }).iterator->value.get();
    webViewResult.iterator->value = client;

    auto* html = client->buildTargetListPage(RemoteInspectorClient::InspectorType::UI);
    gsize streamLength = html->len;
    GRefPtr<GInputStream> stream = adoptGRef(g_memory_input_stream_new_from_data(g_string_free(html, FALSE), streamLength, g_free));
    webkit_uri_scheme_request_finish(request, stream.get(), streamLength, "text/html");
}

void RemoteInspectorProtocolHandler::inspect(const String& hostAndPort, uint64_t connectionID, uint64_t tatgetID, const String& targetType)
{
    if (auto* client = m_inspectorClients.get(hostAndPort))
        client->inspect(connectionID, tatgetID, targetType);
}

void RemoteInspectorProtocolHandler::updateTargetList(WebKitWebView* webView)
{
    auto* clientForWebView = m_webViews.get(webView);
    if (!clientForWebView)
        return;

    GString* script = g_string_new("document.getElementById('targetlist').innerHTML='");
    clientForWebView->appendTargertList(script, RemoteInspectorClient::InspectorType::UI, RemoteInspectorClient::ShouldEscapeSingleQuote::Yes);
    g_string_append(script, "';");
    webkit_web_view_evaluate_javascript(webView, script->str, script->len, nullptr, nullptr, nullptr, nullptr, nullptr);
    g_string_free(script, TRUE);
}

void RemoteInspectorProtocolHandler::webViewLoadChanged(WebKitWebView* webView, WebKitLoadEvent event, RemoteInspectorProtocolHandler* handler)
{
    if (event != WEBKIT_LOAD_FINISHED)
        return;

    g_signal_handlers_disconnect_by_func(webView, reinterpret_cast<gpointer>(webViewLoadChanged), handler);
    handler->updateTargetList(webView);
}

void RemoteInspectorProtocolHandler::targetListChanged(RemoteInspectorClient& client)
{
    for (auto* webView : m_webViews.keys()) {
        if (m_webViews.get(webView) != &client)
            continue;

        if (webkit_web_view_is_loading(webView))
            g_signal_connect(webView, "load-changed", reinterpret_cast<GCallback>(webViewLoadChanged), this);
        else
            updateTargetList(webView);
    }
}

void RemoteInspectorProtocolHandler::connectionClosed(RemoteInspectorClient& client)
{
    m_webViews.removeIf([&](auto& entry) {
        if (entry.value == &client) {
            g_signal_handlers_disconnect_by_data(entry.key, this);
            g_object_weak_unref(G_OBJECT(entry.key), reinterpret_cast<GWeakNotify>(webViewDestroyed), this);
            return true;
        }
        return false;
    });
    m_inspectorClients.remove(client.hostAndPort());
}

} // namespace WebKit

#endif // ENABLE(REMOTE_INSPECTOR)
