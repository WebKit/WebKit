/*
 * Copyright (C) 2019 Sony Interactive Entertainment Inc.
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
#include "RemoteInspectorProtocolHandler.h"

#if ENABLE(REMOTE_INSPECTOR)

#include "APINavigation.h"
#include "APIUserContentWorld.h"
#include "WebPageGroup.h"
#include "WebPageProxy.h"
#include "WebScriptMessageHandler.h"
#include "WebUserContentControllerProxy.h"
#include <WebCore/SerializedScriptValue.h>
#include <wtf/URL.h>
#include <wtf/text/StringBuilder.h>

namespace WebKit {

using namespace WebCore;

class ScriptMessageClient final : public WebScriptMessageHandler::Client {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ScriptMessageClient(RemoteInspectorProtocolHandler& inspectorProtocolHandler)
        : m_inspectorProtocolHandler(inspectorProtocolHandler) { }

    ~ScriptMessageClient() { }

    void didPostMessage(WebPageProxy& page, const FrameInfoData&, WebCore::SerializedScriptValue& serializedScriptValue) override
    {
        auto tokens = serializedScriptValue.toString().split(":");
        if (tokens.size() != 3)
            return;

        URL requestURL { { }, page.pageLoadState().url() };
        m_inspectorProtocolHandler.inspect(requestURL.hostAndPort(), tokens[0].toUIntStrict(), tokens[1].toUIntStrict(), tokens[2]);
    }

private:
    RemoteInspectorProtocolHandler& m_inspectorProtocolHandler;
};

void RemoteInspectorProtocolHandler::inspect(const String& hostAndPort, ConnectionID connectionID, TargetID targetID, const String& type)
{
    if (auto* client = m_inspectorClients.get(hostAndPort))
        client->inspect(connectionID, targetID, type);
}

void RemoteInspectorProtocolHandler::targetListChanged(RemoteInspectorClient&)
{
    if (m_page.pageLoadState().isLoading())
        return;

    m_page.reload({ });
}

void RemoteInspectorProtocolHandler::platformStartTask(WebPageProxy& pageProxy, WebURLSchemeTask& task)
{
    auto& requestURL = task.request().url();
    if (!requestURL.port())
        return;

    auto* client = m_inspectorClients.ensure(requestURL.hostAndPort(), [this, &requestURL] {
        return makeUnique<RemoteInspectorClient>(requestURL.host().utf8().data(), requestURL.port().value(), *this);
    }).iterator->value.get();

    // Setup target postMessage listener
    auto handler = WebScriptMessageHandler::create(makeUnique<ScriptMessageClient>(*this), "inspector", API::UserContentWorld::normalWorld());
    pageProxy.pageGroup().userContentController().addUserScriptMessageHandler(handler.get());

    StringBuilder htmlBuilder;
    htmlBuilder.append(
        "<html><head><title>Remote Inspector</title>"
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
        htmlBuilder.append("<p>No targets found</p>");
    else {
        htmlBuilder.append("<table>");
        for (auto& connectionID : client->targets().keys()) {
            for (auto& target : client->targets().get(connectionID)) {
                htmlBuilder.append(makeString(
                    "<tbody><tr>"
                    "<td class=\"data\"><div class=\"targetname\">", target.name, "</div><div class=\"targeturl\">", target.url, "</div></td>"
                    "<td class=\"input\"><input type=\"button\" value=\"Inspect\" onclick=\"window.webkit.messageHandlers.inspector.postMessage('", connectionID, ":", target.id, ":", target.type, "');\"></td>"
                    "</tr></tbody>"
                ));
            }
        }
        htmlBuilder.append("</table>");
    }

    htmlBuilder.append("</body></html>");

    auto html = htmlBuilder.toString().utf8();
    pageProxy.loadData({ reinterpret_cast<const uint8_t*>(html.data()), html.length() }, "text/html"_s, "UTF-8"_s, requestURL);
}

} // namespace WebKit

#endif // ENABLE(REMOTE_INSPECTOR)
