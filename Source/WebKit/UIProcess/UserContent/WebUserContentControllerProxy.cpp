/*
 * Copyright (C) 2014-2025 Apple Inc. All rights reserved.
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
#include "WebUserContentControllerProxy.h"

#include "APIArray.h"
#include "APIContentWorld.h"
#include "APISerializedScriptValue.h"
#include "APIUserScript.h"
#include "APIUserStyleSheet.h"
#include "InjectUserScriptImmediately.h"
#include "JavaScriptEvaluationResult.h"
#include "NetworkContentRuleListManagerMessages.h"
#include "NetworkProcessProxy.h"
#include "WebPageCreationParameters.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include "WebScriptMessageHandler.h"
#include "WebUserContentControllerDataTypes.h"
#include "WebUserContentControllerMessages.h"
#include "WebUserContentControllerProxyMessages.h"
#include <WebCore/SerializedScriptValue.h>
#include <wtf/CheckedPtr.h>

#if ENABLE(CONTENT_EXTENSIONS)
#include "APIContentRuleList.h"
#include "WebCompiledContentRuleList.h"
#endif

#if ENABLE(WK_WEB_EXTENSIONS)
#include "WebExtensionMatchPattern.h"
#endif

namespace WebKit {

using namespace WebCore;

static HashMap<UserContentControllerIdentifier, WeakRef<WebUserContentControllerProxy>>& webUserContentControllerProxies()
{
    static NeverDestroyed<HashMap<UserContentControllerIdentifier, WeakRef<WebUserContentControllerProxy>>> proxies;
    return proxies;
}

WebUserContentControllerProxy* WebUserContentControllerProxy::get(UserContentControllerIdentifier identifier)
{
    return webUserContentControllerProxies().get(identifier);
}
    
WebUserContentControllerProxy::WebUserContentControllerProxy()
    : m_userScripts(API::Array::create())
    , m_userStyleSheets(API::Array::create())
{
    webUserContentControllerProxies().add(identifier(), *this);
}

WebUserContentControllerProxy::~WebUserContentControllerProxy()
{
    for (const auto& identifier : m_associatedContentWorlds) {
        RefPtr world = API::ContentWorld::worldForIdentifier(identifier);
        RELEASE_ASSERT(world);
        world->userContentControllerProxyDestroyed(*this);
    }
    
    webUserContentControllerProxies().remove(identifier());
    for (Ref process : m_processes) {
        process->removeMessageReceiver(Messages::WebUserContentControllerProxy::messageReceiverName(), identifier());
        process->didDestroyWebUserContentControllerProxy(*this);
    }
#if ENABLE(CONTENT_EXTENSIONS)
    for (Ref process : m_networkProcesses)
        process->didDestroyWebUserContentControllerProxy(*this);
#endif
}

#if ENABLE(CONTENT_EXTENSIONS)
void WebUserContentControllerProxy::addNetworkProcess(NetworkProcessProxy& proxy)
{
    m_networkProcesses.add(proxy);
}

void WebUserContentControllerProxy::removeNetworkProcess(NetworkProcessProxy& proxy)
{
    m_networkProcesses.remove(proxy);
}
#endif

void WebUserContentControllerProxy::addProcess(WebProcessProxy& webProcessProxy)
{
    if (m_processes.add(webProcessProxy).isNewEntry)
        webProcessProxy.addMessageReceiver(Messages::WebUserContentControllerProxy::messageReceiverName(), identifier(), *this);
}

UserContentControllerParameters WebUserContentControllerProxy::parameters() const
{
    auto userContentWorlds = WTF::map(m_associatedContentWorlds, [](auto& identifier) {
        auto* world = API::ContentWorld::worldForIdentifier(identifier);
        RELEASE_ASSERT(world);
        return world->worldData();
    });

    Vector<WebUserScriptData> userScripts;
    for (RefPtr userScript : m_userScripts->elementsOfType<API::UserScript>())
        userScripts.append({ userScript->identifier(), userScript->contentWorld().identifier(), userScript->userScript() });

    Vector<WebUserStyleSheetData> userStyleSheets;
    for (RefPtr userStyleSheet : m_userStyleSheets->elementsOfType<API::UserStyleSheet>())
        userStyleSheets.append({ userStyleSheet->identifier(), userStyleSheet->contentWorld().identifier(), userStyleSheet->userStyleSheet() });

    auto messageHandlers = WTF::map(m_scriptMessageHandlers, [](auto entry) {
        return WebScriptMessageHandlerData { entry.value->identifier(), entry.value->world().identifier(), entry.value->name() };
    });

    return {
        identifier()
        , WTFMove(userContentWorlds)
        , WTFMove(userScripts)
        , WTFMove(userStyleSheets)
        , WTFMove(messageHandlers)
#if ENABLE(CONTENT_EXTENSIONS)
        , contentRuleListData()
#endif
    };
}

#if ENABLE(CONTENT_EXTENSIONS)
Vector<std::pair<WebCompiledContentRuleListData, URL>> WebUserContentControllerProxy::contentRuleListData() const
{
    return WTF::map(m_contentRuleLists, [](const auto& keyValue) -> std::pair<WebCompiledContentRuleListData, URL> {
        return { keyValue.value.first->compiledRuleList().data(), keyValue.value.second };
    });
}
#endif

void WebUserContentControllerProxy::removeProcess(WebProcessProxy& webProcessProxy)
{
    ASSERT(m_processes.contains(webProcessProxy));

    m_processes.remove(webProcessProxy);
    webProcessProxy.removeMessageReceiver(Messages::WebUserContentControllerProxy::messageReceiverName(), identifier());
}

void WebUserContentControllerProxy::addContentWorld(API::ContentWorld& world)
{
    if (world.identifier() == pageContentWorldIdentifier())
        return;

    auto addResult = m_associatedContentWorlds.add(world.identifier());
    if (!addResult.isNewEntry)
        return;

    world.addAssociatedUserContentControllerProxy(*this);
    for (Ref process : m_processes)
        process->send(Messages::WebUserContentController::AddContentWorlds({ world.worldData() }), identifier());
}

void WebUserContentControllerProxy::contentWorldDestroyed(API::ContentWorld& world)
{
    bool result = m_associatedContentWorlds.remove(world.identifier());
    ASSERT_UNUSED(result, result);

    for (Ref process : m_processes)
        process->send(Messages::WebUserContentController::RemoveContentWorlds({ world.identifier() }), identifier());
}

void WebUserContentControllerProxy::addUserScript(API::UserScript& userScript, InjectUserScriptImmediately immediately)
{
    Ref<API::ContentWorld> world = userScript.contentWorld();

    addContentWorld(world.get());

    m_userScripts->elements().append(&userScript);

    for (Ref process : m_processes)
        process->send(Messages::WebUserContentController::AddUserScripts({ { userScript.identifier(), world->identifier(), userScript.userScript() } }, immediately), identifier());
}

void WebUserContentControllerProxy::removeUserScript(API::UserScript& userScript)
{
    Ref<API::ContentWorld> world = userScript.contentWorld();

    for (Ref process : m_processes)
        process->send(Messages::WebUserContentController::RemoveUserScript(world->identifier(), userScript.identifier()), identifier());

    m_userScripts->elements().removeAll(&userScript);
}

void WebUserContentControllerProxy::removeAllUserScripts(API::ContentWorld& world)
{
    for (Ref process : m_processes)
        process->send(Messages::WebUserContentController::RemoveAllUserScripts({ world.identifier() }), identifier());

    m_userScripts->removeAllOfTypeMatching<API::UserScript>([&](const auto& userScript) {
        return &userScript->contentWorld() == &world;
    });
}

#if ENABLE(WK_WEB_EXTENSIONS)
void WebUserContentControllerProxy::removeAllUserScripts(RemoveWebExtensions removeWebExtensions)
#else
void WebUserContentControllerProxy::removeAllUserScripts()
#endif
{
#if ENABLE(WK_WEB_EXTENSIONS)
    if (removeWebExtensions == RemoveWebExtensions::Yes)
#endif
    {
        HashCountedSet<RefPtr<API::ContentWorld>> worlds;
        for (RefPtr userScript : m_userScripts->elementsOfType<API::UserScript>())
            worlds.add(const_cast<API::ContentWorld*>(&userScript->contentWorld()));

        auto worldIdentifiers = WTF::map(worlds, [](auto& entry) {
            return entry.key->identifier();
        });

        for (Ref process : m_processes)
            process->send(Messages::WebUserContentController::RemoveAllUserScripts(worldIdentifiers), identifier());

        m_userScripts->elements().clear();

        return;
    }

#if ENABLE(WK_WEB_EXTENSIONS)
    ASSERT(removeWebExtensions == RemoveWebExtensions::No);

    Vector<Ref<API::UserScript>> scriptsToRemove;
    for (RefPtr userScript : m_userScripts->elementsOfType<API::UserScript>()) {
        if (WebExtensionMatchPattern::isWebExtensionURL(userScript->userScript().url()))
            continue;
        scriptsToRemove.append(const_cast<API::UserScript&>(*userScript));
    }

    for (auto& userScript : scriptsToRemove)
        removeUserScript(userScript.get());
#endif
}

void WebUserContentControllerProxy::addUserStyleSheet(API::UserStyleSheet& userStyleSheet)
{
    Ref<API::ContentWorld> world = userStyleSheet.contentWorld();

    addContentWorld(world.get());

    m_userStyleSheets->elements().append(&userStyleSheet);

    for (Ref process : m_processes)
        process->send(Messages::WebUserContentController::AddUserStyleSheets({ { userStyleSheet.identifier(), world->identifier(), userStyleSheet.userStyleSheet() } }), identifier());
}

void WebUserContentControllerProxy::removeUserStyleSheet(API::UserStyleSheet& userStyleSheet)
{
    Ref<API::ContentWorld> world = userStyleSheet.contentWorld();

    for (Ref process : m_processes)
        process->send(Messages::WebUserContentController::RemoveUserStyleSheet(world->identifier(), userStyleSheet.identifier()), identifier());

    m_userStyleSheets->elements().removeAll(&userStyleSheet);
}

void WebUserContentControllerProxy::removeAllUserStyleSheets(API::ContentWorld& world)
{
    for (Ref process : m_processes)
        process->send(Messages::WebUserContentController::RemoveAllUserStyleSheets({ world.identifier() }), identifier());

    m_userStyleSheets->removeAllOfTypeMatching<API::UserStyleSheet>([&](const auto& userStyleSheet) {
        return &userStyleSheet->contentWorld() == &world;
    });
}

#if ENABLE(WK_WEB_EXTENSIONS)
void WebUserContentControllerProxy::removeAllUserStyleSheets(RemoveWebExtensions removeWebExtensions)
#else
void WebUserContentControllerProxy::removeAllUserStyleSheets()
#endif
{
#if ENABLE(WK_WEB_EXTENSIONS)
    if (removeWebExtensions == RemoveWebExtensions::Yes)
#endif
    {
        HashCountedSet<RefPtr<API::ContentWorld>> worlds;
        for (RefPtr userStyleSheet : m_userStyleSheets->elementsOfType<API::UserStyleSheet>())
            worlds.add(const_cast<API::ContentWorld*>(&userStyleSheet->contentWorld()));

        auto worldIdentifiers = WTF::map(worlds, [](auto& entry) {
            return entry.key->identifier();
        });

        for (Ref process : m_processes)
            process->send(Messages::WebUserContentController::RemoveAllUserStyleSheets(worldIdentifiers), identifier());

        m_userStyleSheets->elements().clear();

        return;
    }

#if ENABLE(WK_WEB_EXTENSIONS)
    ASSERT(removeWebExtensions == RemoveWebExtensions::No);

    Vector<Ref<API::UserStyleSheet>> styleSheetsToRemove;
    for (RefPtr userStyleSheet : m_userStyleSheets->elementsOfType<API::UserStyleSheet>()) {
        if (WebExtensionMatchPattern::isWebExtensionURL(userStyleSheet->userStyleSheet().url()))
            continue;
        styleSheetsToRemove.append(const_cast<API::UserStyleSheet&>(*userStyleSheet));
    }

    for (auto& userStyleSheet : styleSheetsToRemove)
        removeUserStyleSheet(userStyleSheet.get());
#endif
}

bool WebUserContentControllerProxy::addUserScriptMessageHandler(WebScriptMessageHandler& handler)
{
    Ref world = handler.world();

    for (auto& existingHandler : m_scriptMessageHandlers.values()) {
        if (existingHandler->name() == handler.name() && existingHandler->world().identifier() == world->identifier())
            return false;
    }

    addContentWorld(world);

    m_scriptMessageHandlers.add(handler.identifier(), &handler);

    for (Ref process : m_processes)
        process->send(Messages::WebUserContentController::AddUserScriptMessageHandlers({ { handler.identifier(), world->identifier(), handler.name() } }), identifier());
    
    return true;
}

void WebUserContentControllerProxy::removeUserMessageHandlerForName(const String& name, API::ContentWorld& world)
{
    for (auto it = m_scriptMessageHandlers.begin(), end = m_scriptMessageHandlers.end(); it != end; ++it) {
        if (it->value->name() == name && it->value->world().identifier() == world.identifier()) {
            for (Ref process : m_processes)
                process->send(Messages::WebUserContentController::RemoveUserScriptMessageHandler(world.identifier(), it->value->identifier()), identifier());

            m_scriptMessageHandlers.remove(it);

            return;
        }
    }
}

void WebUserContentControllerProxy::removeAllUserMessageHandlers(API::ContentWorld& world)
{
    for (Ref process : m_processes)
        process->send(Messages::WebUserContentController::RemoveAllUserScriptMessageHandlersForWorlds({ world.identifier() }), identifier());

    m_scriptMessageHandlers.removeIf([&](auto& entry) {
        return entry.value->world().identifier() == world.identifier();
    });
}

void WebUserContentControllerProxy::removeAllUserMessageHandlers()
{
    for (Ref process : m_processes)
        process->send(Messages::WebUserContentController::RemoveAllUserScriptMessageHandlers(), identifier());

    m_scriptMessageHandlers.clear();
}

void WebUserContentControllerProxy::didPostMessage(WebPageProxyIdentifier pageProxyID, FrameInfoData&& frameInfoData, ScriptMessageHandlerIdentifier messageHandlerID, JavaScriptEvaluationResult&& message, CompletionHandler<void(Expected<WebKit::JavaScriptEvaluationResult, String>&&)>&& reply)
{
    auto page = WebProcessProxy::webPage(pageProxyID);
    if (!page)
        return reply(makeUnexpected(String()));

    if (!HashMap<ScriptMessageHandlerIdentifier, RefPtr<WebScriptMessageHandler>>::isValidKey(messageHandlerID))
        return reply(makeUnexpected(String()));

    RefPtr<WebScriptMessageHandler> handler = m_scriptMessageHandlers.get(messageHandlerID);
    if (!handler)
        return reply(makeUnexpected(String()));

    if (!handler->client().supportsAsyncReply()) {
        handler->client().didPostMessage(*page, WTFMove(frameInfoData), handler->world(), WTFMove(message));
        return reply(makeUnexpected(String()));
    }

    handler->client().didPostMessageWithAsyncReply(*page, WTFMove(frameInfoData), handler->world(), WTFMove(message), WTFMove(reply));
}

#if ENABLE(CONTENT_EXTENSIONS)
void WebUserContentControllerProxy::addContentRuleList(API::ContentRuleList& contentRuleList, const WTF::URL& extensionBaseURL)
{
    m_contentRuleLists.set(contentRuleList.name(), std::make_pair(Ref { contentRuleList }, extensionBaseURL));

    auto& data = contentRuleList.compiledRuleList().data();

    for (Ref process : m_processes)
        process->send(Messages::WebUserContentController::AddContentRuleLists({ { data, extensionBaseURL } }), identifier());

    for (Ref process : m_networkProcesses)
        process->send(Messages::NetworkContentRuleListManager::AddContentRuleLists { identifier(), { { data, extensionBaseURL } } }, 0);
}

void WebUserContentControllerProxy::removeContentRuleList(const String& name)
{
    m_contentRuleLists.remove(name);

    for (Ref process : m_processes)
        process->send(Messages::WebUserContentController::RemoveContentRuleList(name), identifier());

    for (Ref process : m_networkProcesses)
        process->send(Messages::NetworkContentRuleListManager::RemoveContentRuleList { identifier(), name }, 0);
}

#if ENABLE(WK_WEB_EXTENSIONS)
void WebUserContentControllerProxy::removeAllContentRuleLists(RemoveWebExtensions removeWebExtensions)
#else
void WebUserContentControllerProxy::removeAllContentRuleLists()
#endif
{
#if ENABLE(WK_WEB_EXTENSIONS)
    if (removeWebExtensions == RemoveWebExtensions::Yes)
#endif
    {
        m_contentRuleLists.clear();

        for (Ref process : m_processes)
            process->send(Messages::WebUserContentController::RemoveAllContentRuleLists(), identifier());

        for (Ref process : m_networkProcesses)
            process->send(Messages::NetworkContentRuleListManager::RemoveAllContentRuleLists { identifier() }, 0);

        return;
    }

#if ENABLE(WK_WEB_EXTENSIONS)
    ASSERT(removeWebExtensions == RemoveWebExtensions::No);

    Vector<String> ruleListsToRemove;
    for (auto& entry : m_contentRuleLists) {
        const auto& contentRuleListName = entry.key;
        const auto& extensionBaseURL = std::get<URL>(entry.value);

        if (!WebExtensionMatchPattern::isWebExtensionURL(extensionBaseURL))
            ruleListsToRemove.append(contentRuleListName);
    }

    for (auto& name : ruleListsToRemove)
        removeContentRuleList(name);
#endif
}
#endif

} // namespace WebKit
