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

#include "APILoaderClient.h"
#include "APINavigation.h"
#include "APIUserContentWorld.h"
#include "WebPageGroup.h"
#include "WebPageProxy.h"
#include "WebScriptMessageHandler.h"
#include "WebUserContentControllerProxy.h"
#include <WebCore/JSDOMExceptionHandling.h>
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

    void didPostMessage(WebPageProxy& page, FrameInfoData&&, WebCore::SerializedScriptValue& serializedScriptValue) override
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

class LoaderClient final : public API::LoaderClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    LoaderClient(Function<void()>&& loadedCallback)
        : m_loadedCallback { WTFMove(loadedCallback) } { }

    void didFinishLoadForFrame(WebKit::WebPageProxy&, WebKit::WebFrameProxy&, API::Navigation*, API::Object*) final
    {
        m_loadedCallback();
    }

private:
    Function<void()> m_loadedCallback;
};

static Optional<Inspector::DebuggableType> parseDebuggableTypeFromString(const String& debuggableTypeString)
{
    if (debuggableTypeString == "javascript"_s)
        return Inspector::DebuggableType::JavaScript;
    if (debuggableTypeString == "page"_s)
        return Inspector::DebuggableType::Page;
    if (debuggableTypeString == "service-worker"_s)
        return Inspector::DebuggableType::ServiceWorker;
    if (debuggableTypeString == "web-page"_s)
        return Inspector::DebuggableType::WebPage;

    return WTF::nullopt;
}

void RemoteInspectorProtocolHandler::inspect(const String& hostAndPort, ConnectionID connectionID, TargetID targetID, const String& type)
{
    auto debuggableType = parseDebuggableTypeFromString(type);
    if (!debuggableType) {
        LOG_ERROR("Unknown debuggable type: \"%s\"", type.utf8().data());
        return;
    }

    if (m_inspectorClient)
        m_inspectorClient->inspect(connectionID, targetID, debuggableType.value());
}

void RemoteInspectorProtocolHandler::runScript(const String& script)
{
    m_page.runJavaScriptInMainFrame({ script, false, WTF::nullopt, false }, 
        [](API::SerializedScriptValue*, Optional<WebCore::ExceptionDetails> exceptionDetails, CallbackBase::Error) {
            if (exceptionDetails)
                LOG_ERROR("Exception running script \"%s\"", exceptionDetails->message.utf8().data());
    });
}

void RemoteInspectorProtocolHandler::targetListChanged(RemoteInspectorClient& client)
{
    StringBuilder html;
    if (client.targets().isEmpty())
        html.append("<p>No targets found</p>"_s);
    else {
        html.append("<table>");
        for (auto& connectionID : client.targets().keys()) {
            for (auto& target : client.targets().get(connectionID)) {
                html.append(makeString(
                    "<tbody><tr>"
                    "<td class=\"data\"><div class=\"targetname\">", target.name, "</div><div class=\"targeturl\">", target.url, "</div></td>"
                    "<td class=\"input\"><input type=\"button\" value=\"Inspect\" onclick=\"window.webkit.messageHandlers.inspector.postMessage(\\'", connectionID, ":", target.id, ":", target.type, "\\');\"></td>"
                    "</tr></tbody>"
                ));
            }
        }
        html.append("</table>");
    }
    m_targetListsHtml = html.toString();
    if (m_pageLoaded)
        updateTargetList();
}

void RemoteInspectorProtocolHandler::updateTargetList()
{
    if (!m_targetListsHtml.isEmpty()) {
        runScript(makeString("updateTargets(`", m_targetListsHtml, "`);"));
        m_targetListsHtml = { };
    }
}

void RemoteInspectorProtocolHandler::platformStartTask(WebPageProxy& pageProxy, WebURLSchemeTask& task)
{
    auto& requestURL = task.request().url();

    // Destroy the client before creating a new connection so it can connect to the same port
    m_inspectorClient = nullptr;
    m_inspectorClient = makeUnique<RemoteInspectorClient>(requestURL, *this);

    // Setup target postMessage listener
    auto handler = WebScriptMessageHandler::create(makeUnique<ScriptMessageClient>(*this), "inspector", API::UserContentWorld::normalWorld());
    pageProxy.pageGroup().userContentController().addUserScriptMessageHandler(handler.get());

    // Setup loader client to get notified of page load
    m_page.setLoaderClient(makeUnique<LoaderClient>([this] {
        m_pageLoaded = true;
        updateTargetList();
    }));
    m_pageLoaded = false;

    StringBuilder htmlBuilder;
    htmlBuilder.append(
        "<html><head><title>Remote Inspector</title>"
        "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />"
        "<style>"
        "  h1 { color: #babdb6; text-shadow: 0 1px 0 white; margin-bottom: 0; }"
        "  html { font-family: -webkit-system-font; font-size: 11pt; color: #2e3436; padding: 20px 20px 0 20px; background-color: #f6f6f4; "
        "         background-image: -webkit-gradient(linear, left top, left bottom, color-stop(0, #eeeeec), color-stop(1, #f6f6f4));"
        "         background-size: 100% 5em; background-repeat: no-repeat; }"
        "  table { width: 100%; border-collapse: collapse; table-layout: fixed; }"
        "  table, td { border: 1px solid #d3d7cf; border-left: none; border-right: none; }"
        "  p { margin-bottom: 30px; }"
        "  td { padding: 15px; }"
        "  td.data { width: 200px; }"
        "  .targetname { font-weight: bold; overflow: hidden; white-space:nowrap; text-overflow: ellipsis; }"
        "  .targeturl { color: #babdb6; background: #eee; word-wrap: break-word; overflow-wrap: break-word; }"
        "  td.input { width: 64px; }"
        "  input { width: 100%; padding: 8px; }"
        "</style>"
        "</head><body><h1>Inspectable targets</h1>"
        "<div id=\"targetlist\"><p>No targets found</p></div></body>"
        "<script>"
        "function updateTargets(str) {"
            "let targetDiv = document.getElementById('targetlist');"
            "targetDiv.innerHTML = str;"
        "}"
        "</script>");
    htmlBuilder.append("</html>");

    auto html = htmlBuilder.toString().utf8();
    auto data = SharedBuffer::create(html.data(), html.length());
    ResourceResponse response(requestURL, "text/html"_s, html.length(), "UTF-8"_s);
    task.didReceiveResponse(response);
    task.didReceiveData(WTFMove(data));
    task.didComplete(ResourceError());
}

} // namespace WebKit

#endif // ENABLE(REMOTE_INSPECTOR)
