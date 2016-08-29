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
#include "APIUserContentWorld.h"
#include "APIUserScript.h"
#include "APIUserStyleSheet.h"
#include "DataReference.h"
#include "WebProcessProxy.h"
#include "WebScriptMessageHandler.h"
#include "WebUserContentControllerDataTypes.h"
#include "WebUserContentControllerMessages.h"
#include "WebUserContentControllerProxyMessages.h"
#include <WebCore/SerializedScriptValue.h>

#if ENABLE(CONTENT_EXTENSIONS)
#include "APIUserContentExtension.h"
#include "WebCompiledContentExtension.h"
#endif

namespace WebKit {

static uint64_t generateIdentifier()
{
    static uint64_t identifier;

    return ++identifier;
}

WebUserContentControllerProxy::WebUserContentControllerProxy()
    : m_identifier(generateIdentifier())
    , m_userScripts(API::Array::create())
    , m_userStyleSheets(API::Array::create())
{
}

WebUserContentControllerProxy::~WebUserContentControllerProxy()
{
    for (WebProcessProxy* process : m_processes) {
        process->removeMessageReceiver(Messages::WebUserContentControllerProxy::messageReceiverName(), m_identifier);
        process->didDestroyWebUserContentControllerProxy(*this);
    }
}

void WebUserContentControllerProxy::addProcess(WebProcessProxy& webProcessProxy)
{
    ASSERT(webProcessProxy.state() == WebProcessProxy::State::Running);

    if (!m_processes.add(&webProcessProxy).isNewEntry)
        return;

    webProcessProxy.addMessageReceiver(Messages::WebUserContentControllerProxy::messageReceiverName(), m_identifier, *this);

    Vector<std::pair<uint64_t, String>> userContentWorlds;
    for (const auto& world : m_userContentWorlds)
        userContentWorlds.append(std::make_pair(world.key->identifier(), world.key->name()));
    webProcessProxy.connection()->send(Messages::WebUserContentController::AddUserContentWorlds(userContentWorlds), m_identifier);

    Vector<WebUserScriptData> userScripts;
    for (const auto& userScript : m_userScripts->elementsOfType<API::UserScript>())
        userScripts.append({ userScript->identifier(), userScript->userContentWorld().identifier(), userScript->userScript() });
    webProcessProxy.connection()->send(Messages::WebUserContentController::AddUserScripts(userScripts), m_identifier);

    Vector<WebUserStyleSheetData> userStyleSheets;
    for (const auto& userStyleSheet : m_userStyleSheets->elementsOfType<API::UserStyleSheet>())
        userStyleSheets.append({ userStyleSheet->identifier(), userStyleSheet->userContentWorld().identifier(), userStyleSheet->userStyleSheet() });
    webProcessProxy.connection()->send(Messages::WebUserContentController::AddUserStyleSheets(userStyleSheets), m_identifier);

    Vector<WebScriptMessageHandlerData> messageHandlers;
    for (auto& handler : m_scriptMessageHandlers.values())
        messageHandlers.append({ handler->identifier(), handler->userContentWorld().identifier(), handler->name() });
    webProcessProxy.connection()->send(Messages::WebUserContentController::AddUserScriptMessageHandlers(messageHandlers), m_identifier);

#if ENABLE(CONTENT_EXTENSIONS)
    Vector<std::pair<String, WebCompiledContentExtensionData>> userContentExtensions;
    for (const auto& userContentExtension : m_userContentExtensions.values())
        userContentExtensions.append(std::make_pair(userContentExtension->name(), userContentExtension->compiledExtension().data()));
    webProcessProxy.connection()->send(Messages::WebUserContentController::AddUserContentExtensions(userContentExtensions), m_identifier);
#endif
}

void WebUserContentControllerProxy::removeProcess(WebProcessProxy& webProcessProxy)
{
    ASSERT(m_processes.contains(&webProcessProxy));

    m_processes.remove(&webProcessProxy);
    webProcessProxy.removeMessageReceiver(Messages::WebUserContentControllerProxy::messageReceiverName(), m_identifier);
}

void WebUserContentControllerProxy::addUserContentWorldUse(API::UserContentWorld& world)
{
    if (&world == &API::UserContentWorld::normalWorld())
        return;

    auto addResult = m_userContentWorlds.add(&world);
    if (addResult.isNewEntry) {
        for (WebProcessProxy* process : m_processes)
            process->connection()->send(Messages::WebUserContentController::AddUserContentWorlds({ std::make_pair(world.identifier(), world.name()) }), m_identifier);
    }
}

bool WebUserContentControllerProxy::shouldSendRemoveUserContentWorldsMessage(API::UserContentWorld& world, unsigned numberOfUsesToRemove)
{
    if (&world == &API::UserContentWorld::normalWorld())
        return false;

    auto it = m_userContentWorlds.find(&world);
    for (unsigned i = 0; i < numberOfUsesToRemove; ++i) {
        if (m_userContentWorlds.remove(it)) {
            ASSERT(i == (numberOfUsesToRemove - 1));
            return true;
        }
    }
    
    return false;
}

void WebUserContentControllerProxy::removeUserContentWorldUses(API::UserContentWorld& world, unsigned numberOfUsesToRemove)
{
    if (shouldSendRemoveUserContentWorldsMessage(world, numberOfUsesToRemove)) {
        for (WebProcessProxy* process : m_processes)
            process->connection()->send(Messages::WebUserContentController::RemoveUserContentWorlds({ world.identifier() }), m_identifier);
    }
}

void WebUserContentControllerProxy::removeUserContentWorldUses(HashCountedSet<RefPtr<API::UserContentWorld>>& worlds)
{
    Vector<uint64_t> worldsToRemove;
    for (auto& worldUsePair : worlds) {
        if (shouldSendRemoveUserContentWorldsMessage(*worldUsePair.key.get(), worldUsePair.value))
            worldsToRemove.append(worldUsePair.key->identifier());
    }

    for (WebProcessProxy* process : m_processes)
        process->connection()->send(Messages::WebUserContentController::RemoveUserContentWorlds(worldsToRemove), m_identifier);
}

void WebUserContentControllerProxy::addUserScript(API::UserScript& userScript)
{
    Ref<API::UserContentWorld> world = userScript.userContentWorld();

    addUserContentWorldUse(world.get());

    m_userScripts->elements().append(&userScript);

    for (WebProcessProxy* process : m_processes)
        process->connection()->send(Messages::WebUserContentController::AddUserScripts({ { userScript.identifier(), world->identifier(), userScript.userScript() } }), m_identifier);
}

void WebUserContentControllerProxy::removeUserScript(API::UserScript& userScript)
{
    Ref<API::UserContentWorld> world = userScript.userContentWorld();

    for (WebProcessProxy* process : m_processes)
        process->connection()->send(Messages::WebUserContentController::RemoveUserScript(world->identifier(), userScript.identifier()), m_identifier);

    m_userScripts->elements().removeAll(&userScript);

    removeUserContentWorldUses(world.get(), 1);
}

void WebUserContentControllerProxy::removeAllUserScripts(API::UserContentWorld& world)
{
    for (WebProcessProxy* process : m_processes)
        process->connection()->send(Messages::WebUserContentController::RemoveAllUserScripts({ world.identifier() }), m_identifier);

    unsigned userScriptsRemoved = m_userScripts->removeAllOfTypeMatching<API::UserScript>([&](const auto& userScript) {
        return &userScript->userContentWorld() == &world;
    });

    removeUserContentWorldUses(world, userScriptsRemoved);
}

void WebUserContentControllerProxy::removeAllUserScripts()
{
    HashCountedSet<RefPtr<API::UserContentWorld>> worlds;
    for (const auto& userScript : m_userScripts->elementsOfType<API::UserScript>())
        worlds.add(const_cast<API::UserContentWorld*>(&userScript->userContentWorld()));

    Vector<uint64_t> worldIdentifiers;
    worldIdentifiers.reserveInitialCapacity(worlds.size());
    for (const auto& worldCountPair : worlds)
        worldIdentifiers.uncheckedAppend(worldCountPair.key->identifier());

    for (WebProcessProxy* process : m_processes)
        process->connection()->send(Messages::WebUserContentController::RemoveAllUserScripts(worldIdentifiers), m_identifier);

    m_userScripts->elements().clear();

    removeUserContentWorldUses(worlds);
}

void WebUserContentControllerProxy::addUserStyleSheet(API::UserStyleSheet& userStyleSheet)
{
    Ref<API::UserContentWorld> world = userStyleSheet.userContentWorld();

    addUserContentWorldUse(world.get());

    m_userStyleSheets->elements().append(&userStyleSheet);

    for (WebProcessProxy* process : m_processes)
        process->connection()->send(Messages::WebUserContentController::AddUserStyleSheets({ { userStyleSheet.identifier(), world->identifier(), userStyleSheet.userStyleSheet() } }), m_identifier);
}

void WebUserContentControllerProxy::removeUserStyleSheet(API::UserStyleSheet& userStyleSheet)
{
    Ref<API::UserContentWorld> world = userStyleSheet.userContentWorld();

    for (WebProcessProxy* process : m_processes)
        process->connection()->send(Messages::WebUserContentController::RemoveUserStyleSheet(world->identifier(), userStyleSheet.identifier()), m_identifier);

    m_userStyleSheets->elements().removeAll(&userStyleSheet);

    removeUserContentWorldUses(world.get(), 1);
}

void WebUserContentControllerProxy::removeAllUserStyleSheets(API::UserContentWorld& world)
{
    for (WebProcessProxy* process : m_processes)
        process->connection()->send(Messages::WebUserContentController::RemoveAllUserStyleSheets({ world.identifier() }), m_identifier);

    unsigned userStyleSheetsRemoved = m_userStyleSheets->removeAllOfTypeMatching<API::UserStyleSheet>([&](const auto& userStyleSheet) {
        return &userStyleSheet->userContentWorld() == &world;
    });

    removeUserContentWorldUses(world, userStyleSheetsRemoved);
}

void WebUserContentControllerProxy::removeAllUserStyleSheets()
{
    HashCountedSet<RefPtr<API::UserContentWorld>> worlds;
    for (const auto& userStyleSheet : m_userStyleSheets->elementsOfType<API::UserStyleSheet>())
        worlds.add(const_cast<API::UserContentWorld*>(&userStyleSheet->userContentWorld()));

    Vector<uint64_t> worldIdentifiers;
    worldIdentifiers.reserveInitialCapacity(worlds.size());
    for (const auto& worldCountPair : worlds)
        worldIdentifiers.uncheckedAppend(worldCountPair.key->identifier());

    for (WebProcessProxy* process : m_processes)
        process->connection()->send(Messages::WebUserContentController::RemoveAllUserStyleSheets(worldIdentifiers), m_identifier);

    m_userStyleSheets->elements().clear();

    removeUserContentWorldUses(worlds);
}

bool WebUserContentControllerProxy::addUserScriptMessageHandler(WebScriptMessageHandler& handler)
{
    Ref<API::UserContentWorld> world = handler.userContentWorld();

    for (auto& existingHandler : m_scriptMessageHandlers.values()) {
        if (existingHandler->name() == handler.name() && &existingHandler->userContentWorld() == world.ptr())
            return false;
    }

    addUserContentWorldUse(world.get());

    m_scriptMessageHandlers.add(handler.identifier(), &handler);

    for (WebProcessProxy* process : m_processes)
        process->connection()->send(Messages::WebUserContentController::AddUserScriptMessageHandlers({ { handler.identifier(), world->identifier(), handler.name() } }), m_identifier);
    
    return true;
}

void WebUserContentControllerProxy::removeUserMessageHandlerForName(const String& name, API::UserContentWorld& world)
{
    for (auto it = m_scriptMessageHandlers.begin(), end = m_scriptMessageHandlers.end(); it != end; ++it) {
        if (it->value->name() == name && &it->value->userContentWorld() == &world) {
            for (WebProcessProxy* process : m_processes)
                process->connection()->send(Messages::WebUserContentController::RemoveUserScriptMessageHandler(world.identifier(), it->value->identifier()), m_identifier);

            m_scriptMessageHandlers.remove(it);

            removeUserContentWorldUses(world, 1);
            return;
        }
    }
}

void WebUserContentControllerProxy::removeAllUserMessageHandlers(API::UserContentWorld& world)
{
    for (WebProcessProxy* process : m_processes)
        process->connection()->send(Messages::WebUserContentController::RemoveAllUserScriptMessageHandlers({ world.identifier() }), m_identifier);

    unsigned numberRemoved = 0;
    m_scriptMessageHandlers.removeIf([&](auto& entry) {
        if (&entry.value->userContentWorld() == &world) {
            ++numberRemoved;
            return true;
        }
        return false;
    });

    removeUserContentWorldUses(world, numberRemoved);
}

void WebUserContentControllerProxy::didPostMessage(IPC::Connection& connection, uint64_t pageID, const FrameInfoData& frameInfoData, uint64_t messageHandlerID, const IPC::DataReference& dataReference)
{
    WebPageProxy* page = WebProcessProxy::webPage(pageID);
    if (!page)
        return;

    if (!HashMap<uint64_t, RefPtr<WebScriptMessageHandler>>::isValidKey(messageHandlerID))
        return;

    RefPtr<WebScriptMessageHandler> handler = m_scriptMessageHandlers.get(messageHandlerID);
    if (!handler)
        return;

    handler->client().didPostMessage(*page, frameInfoData, WebCore::SerializedScriptValue::adopt(dataReference.vector()));
}

#if ENABLE(CONTENT_EXTENSIONS)
void WebUserContentControllerProxy::addUserContentExtension(API::UserContentExtension& userContentExtension)
{
    m_userContentExtensions.set(userContentExtension.name(), &userContentExtension);

    auto pair = std::make_pair(userContentExtension.name(), userContentExtension.compiledExtension().data());

    for (WebProcessProxy* process : m_processes)
        process->connection()->send(Messages::WebUserContentController::AddUserContentExtensions({ pair }), m_identifier);
}

void WebUserContentControllerProxy::removeUserContentExtension(const String& name)
{
    m_userContentExtensions.remove(name);

    for (WebProcessProxy* process : m_processes)
        process->connection()->send(Messages::WebUserContentController::RemoveUserContentExtension(name), m_identifier);
}

void WebUserContentControllerProxy::removeAllUserContentExtensions()
{
    m_userContentExtensions.clear();

    for (WebProcessProxy* process : m_processes)
        process->connection()->send(Messages::WebUserContentController::RemoveAllUserContentExtensions(), m_identifier);
}
#endif

} // namespace WebKit
