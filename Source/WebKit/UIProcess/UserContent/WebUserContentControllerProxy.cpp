/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "APIUserContentWorld.h"
#include "APIUserScript.h"
#include "APIUserStyleSheet.h"
#include "DataReference.h"
#include "InjectUserScriptImmediately.h"
#include "NetworkContentRuleListManagerMessages.h"
#include "NetworkProcessProxy.h"
#include "WebPageCreationParameters.h"
#include "WebProcessProxy.h"
#include "WebScriptMessageHandler.h"
#include "WebUserContentControllerDataTypes.h"
#include "WebUserContentControllerMessages.h"
#include "WebUserContentControllerProxyMessages.h"
#include <WebCore/SerializedScriptValue.h>

#if ENABLE(CONTENT_EXTENSIONS)
#include "APIContentRuleList.h"
#include "WebCompiledContentRuleList.h"
#endif

namespace WebKit {

using namespace WebCore;

static HashMap<UserContentControllerIdentifier, WebUserContentControllerProxy*>& webUserContentControllerProxies()
{
    static NeverDestroyed<HashMap<UserContentControllerIdentifier, WebUserContentControllerProxy*>> proxies;
    return proxies;
}


WebUserContentControllerProxy* WebUserContentControllerProxy::get(UserContentControllerIdentifier identifier)
{
    return webUserContentControllerProxies().get(identifier);
}
    
WebUserContentControllerProxy::WebUserContentControllerProxy()
    : m_identifier(UserContentControllerIdentifier::generate())
    , m_userScripts(API::Array::create())
    , m_userStyleSheets(API::Array::create())
{
    webUserContentControllerProxies().add(m_identifier, this);
}

WebUserContentControllerProxy::~WebUserContentControllerProxy()
{
    webUserContentControllerProxies().remove(m_identifier);
    for (auto& process : m_processes) {
        process.removeMessageReceiver(Messages::WebUserContentControllerProxy::messageReceiverName(), identifier());
        process.didDestroyWebUserContentControllerProxy(*this);
    }
#if ENABLE(CONTENT_EXTENSIONS)
    for (auto& process : m_networkProcesses)
        process.didDestroyWebUserContentControllerProxy(*this);
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

void WebUserContentControllerProxy::addProcess(WebProcessProxy& webProcessProxy, WebPageCreationParameters& parameters)
{
    ASSERT(!m_processes.hasNullReferences());

    if (m_processes.add(webProcessProxy).isNewEntry)
        webProcessProxy.addMessageReceiver(Messages::WebUserContentControllerProxy::messageReceiverName(), identifier(), *this);

    ASSERT(parameters.userContentWorlds.isEmpty());
    for (const auto& world : m_contentWorlds)
        parameters.userContentWorlds.append(world.key->worldData());

    ASSERT(parameters.userScripts.isEmpty());
    for (auto userScript : m_userScripts->elementsOfType<API::UserScript>())
        parameters.userScripts.append({ userScript->identifier(), userScript->userContentWorld().identifier(), userScript->userScript() });

    ASSERT(parameters.userStyleSheets.isEmpty());
    for (auto userStyleSheet : m_userStyleSheets->elementsOfType<API::UserStyleSheet>())
        parameters.userStyleSheets.append({ userStyleSheet->identifier(), userStyleSheet->userContentWorld().identifier(), userStyleSheet->userStyleSheet() });

    ASSERT(parameters.messageHandlers.isEmpty());
    for (auto& handler : m_scriptMessageHandlers.values())
        parameters.messageHandlers.append({ handler->identifier(), handler->world().identifier(), handler->name() });

#if ENABLE(CONTENT_EXTENSIONS)
    ASSERT(parameters.contentRuleLists.isEmpty());
    parameters.contentRuleLists = contentRuleListData();
#endif
}

#if ENABLE(CONTENT_EXTENSIONS)
Vector<std::pair<String, WebCompiledContentRuleListData>> WebUserContentControllerProxy::contentRuleListData()
{
    Vector<std::pair<String, WebCompiledContentRuleListData>> data;
    data.reserveInitialCapacity(m_contentRuleLists.size());
    for (const auto& contentRuleList : m_contentRuleLists.values())
        data.uncheckedAppend(std::make_pair(contentRuleList->name(), contentRuleList->compiledRuleList().data()));
    return data;
}
#endif

void WebUserContentControllerProxy::removeProcess(WebProcessProxy& webProcessProxy)
{
    ASSERT(m_processes.contains(webProcessProxy));
    ASSERT(!m_processes.hasNullReferences());

    m_processes.remove(webProcessProxy);
    webProcessProxy.removeMessageReceiver(Messages::WebUserContentControllerProxy::messageReceiverName(), identifier());
}

void WebUserContentControllerProxy::addUserContentWorldUse(API::ContentWorldBase& world)
{
    if (&world == &API::UserContentWorld::normalWorld())
        return;

    auto addResult = m_contentWorlds.add(&world);
    if (addResult.isNewEntry) {
        for (auto& process : m_processes)
            process.send(Messages::WebUserContentController::AddUserContentWorlds({ world.worldData() }), identifier());
    }
}

bool WebUserContentControllerProxy::shouldSendRemoveUserContentWorldsMessage(API::UserContentWorld& world, unsigned numberOfUsesToRemove)
{
    if (&world == &API::UserContentWorld::normalWorld())
        return false;

    auto it = m_contentWorlds.find(&world);
    for (unsigned i = 0; i < numberOfUsesToRemove; ++i) {
        if (m_contentWorlds.remove(it)) {
            ASSERT(i == (numberOfUsesToRemove - 1));
            return true;
        }
    }
    
    return false;
}

void WebUserContentControllerProxy::removeUserContentWorldUses(API::UserContentWorld& world, unsigned numberOfUsesToRemove)
{
    if (shouldSendRemoveUserContentWorldsMessage(world, numberOfUsesToRemove)) {
        for (auto& process : m_processes)
            process.send(Messages::WebUserContentController::RemoveUserContentWorlds({ world.identifier() }), identifier());
    }
}

void WebUserContentControllerProxy::removeUserContentWorldUses(HashCountedSet<RefPtr<API::UserContentWorld>>& worlds)
{
    Vector<ContentWorldIdentifier> worldsToRemove;
    for (auto& worldUsePair : worlds) {
        if (shouldSendRemoveUserContentWorldsMessage(*worldUsePair.key.get(), worldUsePair.value))
            worldsToRemove.append(worldUsePair.key->identifier());
    }

    for (auto& process : m_processes)
        process.send(Messages::WebUserContentController::RemoveUserContentWorlds(worldsToRemove), identifier());
}

void WebUserContentControllerProxy::addUserScript(API::UserScript& userScript, InjectUserScriptImmediately immediately)
{
    Ref<API::UserContentWorld> world = userScript.userContentWorld();

    addUserContentWorldUse(world.get());

    m_userScripts->elements().append(&userScript);

    for (auto& process : m_processes)
        process.send(Messages::WebUserContentController::AddUserScripts({ { userScript.identifier(), world->identifier(), userScript.userScript() } }, immediately), identifier());
}

void WebUserContentControllerProxy::removeUserScript(API::UserScript& userScript)
{
    Ref<API::UserContentWorld> world = userScript.userContentWorld();

    for (auto& process : m_processes)
        process.send(Messages::WebUserContentController::RemoveUserScript(world->identifier(), userScript.identifier()), identifier());

    m_userScripts->elements().removeAll(&userScript);

    removeUserContentWorldUses(world.get(), 1);
}

void WebUserContentControllerProxy::removeAllUserScripts(API::UserContentWorld& world)
{
    for (auto& process : m_processes)
        process.send(Messages::WebUserContentController::RemoveAllUserScripts({ world.identifier() }), identifier());

    unsigned userScriptsRemoved = m_userScripts->removeAllOfTypeMatching<API::UserScript>([&](const auto& userScript) {
        return &userScript->userContentWorld() == &world;
    });

    removeUserContentWorldUses(world, userScriptsRemoved);
}

void WebUserContentControllerProxy::removeAllUserScripts()
{
    HashCountedSet<RefPtr<API::UserContentWorld>> worlds;
    for (auto userScript : m_userScripts->elementsOfType<API::UserScript>())
        worlds.add(const_cast<API::UserContentWorld*>(&userScript->userContentWorld()));

    Vector<ContentWorldIdentifier> worldIdentifiers;
    worldIdentifiers.reserveInitialCapacity(worlds.size());
    for (const auto& worldCountPair : worlds)
        worldIdentifiers.uncheckedAppend(worldCountPair.key->identifier());

    for (auto& process : m_processes)
        process.send(Messages::WebUserContentController::RemoveAllUserScripts(worldIdentifiers), identifier());

    m_userScripts->elements().clear();

    removeUserContentWorldUses(worlds);
}

void WebUserContentControllerProxy::addUserStyleSheet(API::UserStyleSheet& userStyleSheet)
{
    Ref<API::UserContentWorld> world = userStyleSheet.userContentWorld();

    addUserContentWorldUse(world.get());

    m_userStyleSheets->elements().append(&userStyleSheet);

    for (auto& process : m_processes)
        process.send(Messages::WebUserContentController::AddUserStyleSheets({ { userStyleSheet.identifier(), world->identifier(), userStyleSheet.userStyleSheet() } }), identifier());
}

void WebUserContentControllerProxy::removeUserStyleSheet(API::UserStyleSheet& userStyleSheet)
{
    Ref<API::UserContentWorld> world = userStyleSheet.userContentWorld();

    for (auto& process : m_processes)
        process.send(Messages::WebUserContentController::RemoveUserStyleSheet(world->identifier(), userStyleSheet.identifier()), identifier());

    m_userStyleSheets->elements().removeAll(&userStyleSheet);

    removeUserContentWorldUses(world.get(), 1);
}

void WebUserContentControllerProxy::removeAllUserStyleSheets(API::UserContentWorld& world)
{
    for (auto& process : m_processes)
        process.send(Messages::WebUserContentController::RemoveAllUserStyleSheets({ world.identifier() }), identifier());

    unsigned userStyleSheetsRemoved = m_userStyleSheets->removeAllOfTypeMatching<API::UserStyleSheet>([&](const auto& userStyleSheet) {
        return &userStyleSheet->userContentWorld() == &world;
    });

    removeUserContentWorldUses(world, userStyleSheetsRemoved);
}

void WebUserContentControllerProxy::removeAllUserStyleSheets()
{
    HashCountedSet<RefPtr<API::UserContentWorld>> worlds;
    for (auto userStyleSheet : m_userStyleSheets->elementsOfType<API::UserStyleSheet>())
        worlds.add(const_cast<API::UserContentWorld*>(&userStyleSheet->userContentWorld()));

    Vector<ContentWorldIdentifier> worldIdentifiers;
    worldIdentifiers.reserveInitialCapacity(worlds.size());
    for (const auto& worldCountPair : worlds)
        worldIdentifiers.uncheckedAppend(worldCountPair.key->identifier());

    for (auto& process : m_processes)
        process.send(Messages::WebUserContentController::RemoveAllUserStyleSheets(worldIdentifiers), identifier());

    m_userStyleSheets->elements().clear();

    removeUserContentWorldUses(worlds);
}

bool WebUserContentControllerProxy::addUserScriptMessageHandler(WebScriptMessageHandler& handler)
{
    auto& world = handler.world();

    for (auto& existingHandler : m_scriptMessageHandlers.values()) {
        if (existingHandler->name() == handler.name() && existingHandler->world().identifier() == world.identifier())
            return false;
    }

    addUserContentWorldUse(world);

    m_scriptMessageHandlers.add(handler.identifier(), &handler);

    for (auto& process : m_processes)
        process.send(Messages::WebUserContentController::AddUserScriptMessageHandlers({ { handler.identifier(), world.identifier(), handler.name() } }), identifier());
    
    return true;
}

void WebUserContentControllerProxy::removeUserMessageHandlerForName(const String& name, API::UserContentWorld& world)
{
    for (auto it = m_scriptMessageHandlers.begin(), end = m_scriptMessageHandlers.end(); it != end; ++it) {
        if (it->value->name() == name && it->value->world().identifier() == world.identifier()) {
            for (auto& process : m_processes)
                process.send(Messages::WebUserContentController::RemoveUserScriptMessageHandler(world.identifier(), it->value->identifier()), identifier());

            m_scriptMessageHandlers.remove(it);

            removeUserContentWorldUses(world, 1);
            return;
        }
    }
}

void WebUserContentControllerProxy::removeAllUserMessageHandlers(API::UserContentWorld& world)
{
    for (auto& process : m_processes)
        process.send(Messages::WebUserContentController::RemoveAllUserScriptMessageHandlers({ world.identifier() }), identifier());

    unsigned numberRemoved = 0;
    m_scriptMessageHandlers.removeIf([&](auto& entry) {
        if (entry.value->world().identifier() == world.identifier()) {
            ++numberRemoved;
            return true;
        }
        return false;
    });

    removeUserContentWorldUses(world, numberRemoved);
}

void WebUserContentControllerProxy::didPostMessage(IPC::Connection& connection, WebPageProxyIdentifier pageProxyID, FrameInfoData&& frameInfoData, uint64_t messageHandlerID, const IPC::DataReference& dataReference)
{
    WebPageProxy* page = WebProcessProxy::webPage(pageProxyID);
    if (!page)
        return;

    if (!HashMap<uint64_t, RefPtr<WebScriptMessageHandler>>::isValidKey(messageHandlerID))
        return;

    RefPtr<WebScriptMessageHandler> handler = m_scriptMessageHandlers.get(messageHandlerID);
    if (!handler)
        return;

    handler->client().didPostMessage(*page, WTFMove(frameInfoData), WebCore::SerializedScriptValue::adopt(dataReference.vector()));
}

#if ENABLE(CONTENT_EXTENSIONS)
void WebUserContentControllerProxy::addContentRuleList(API::ContentRuleList& contentRuleList)
{
    m_contentRuleLists.set(contentRuleList.name(), &contentRuleList);

    auto pair = std::make_pair(contentRuleList.name(), contentRuleList.compiledRuleList().data());

    for (auto& process : m_processes)
        process.send(Messages::WebUserContentController::AddContentRuleLists({ pair }), identifier());

    for (auto& process : m_networkProcesses)
        process.send(Messages::NetworkContentRuleListManager::AddContentRuleLists { identifier(), { pair } }, 0);
}

void WebUserContentControllerProxy::removeContentRuleList(const String& name)
{
    m_contentRuleLists.remove(name);

    for (auto& process : m_processes)
        process.send(Messages::WebUserContentController::RemoveContentRuleList(name), identifier());

    for (auto& process : m_networkProcesses)
        process.send(Messages::NetworkContentRuleListManager::RemoveContentRuleList { identifier(), name }, 0);
}

void WebUserContentControllerProxy::removeAllContentRuleLists()
{
    m_contentRuleLists.clear();

    for (auto& process : m_processes)
        process.send(Messages::WebUserContentController::RemoveAllContentRuleLists(), identifier());

    for (auto& process : m_networkProcesses)
        process.send(Messages::NetworkContentRuleListManager::RemoveAllContentRuleLists { identifier() }, 0);
}
#endif

} // namespace WebKit
