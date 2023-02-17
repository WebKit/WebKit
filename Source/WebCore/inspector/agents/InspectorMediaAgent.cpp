/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "InspectorMediaAgent.h"

#include "HTMLMediaElement.h"
#include "JSHTMLMediaElement.h"
#include "MediaPlayer.h"
#include <variant>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

using namespace Inspector;

InspectorMediaAgent::InspectorMediaAgent(PageAgentContext& context)
    : InspectorAgentBase("Media"_s, context)
    , m_frontendDispatcher(makeUnique<Inspector::MediaFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(Inspector::MediaBackendDispatcher::create(context.backendDispatcher, this))
    , m_injectedScriptManager(context.injectedScriptManager)
    , m_inspectedPage(context.inspectedPage)
    , m_playerDestroyedTimer(*this, &InspectorMediaAgent::playerDestroyedTimerFired)
{
}

InspectorMediaAgent::~InspectorMediaAgent() = default;

void InspectorMediaAgent::didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*)
{
}

void InspectorMediaAgent::willDestroyFrontendAndBackend(Inspector::DisconnectReason)
{
    disable();
}

void InspectorMediaAgent::discardAgent()
{
    reset();
}

Protocol::ErrorStringOr<void> InspectorMediaAgent::enable()
{
    if (m_instrumentingAgents.enabledMediaAgent() == this)
        return { };
    
    m_instrumentingAgents.setEnabledMediaAgent(this);
    
    for (auto* mediaElement : HTMLMediaElement::allMediaElements()) {
        if (mediaElement->document().page() == &m_inspectedPage)
            bindPlayer(*mediaElement);
    }
    
    return { };
}

Protocol::ErrorStringOr<void> InspectorMediaAgent::disable()
{
    m_instrumentingAgents.setEnabledMediaAgent(nullptr);
    reset();
    return { };
}

Protocol::ErrorStringOr<Protocol::DOM::NodeId> InspectorMediaAgent::requestNode(const Protocol::Media::MediaPlayerId& playerId)
{
    Protocol::ErrorString errorString;
    
    auto inspectorPlayer = assertInspectorPlayer(errorString, playerId);
    if (!inspectorPlayer)
        return makeUnexpected(errorString);
    
    auto* node = inspectorPlayer->mediaElement();
    if (!node)
        return makeUnexpected("Missing element of player for given playerId"_s);
    
    int documentNodeId = m_instrumentingAgents.persistentDOMAgent()->boundNodeId(&node->document());
    if (!documentNodeId)
        return makeUnexpected("Document must have been requested"_s);
    
    return m_instrumentingAgents.persistentDOMAgent()->pushNodeToFrontend(errorString, documentNodeId, node);
}

Inspector::Protocol::ErrorStringOr<Ref<Protocol::Runtime::RemoteObject>> InspectorMediaAgent::resolveElement(const Inspector::Protocol::Media::MediaPlayerId& playerId, const String& objectGroup)
{
    Protocol::ErrorString errorString;
    
    auto inspectorPlayer = assertInspectorPlayer(errorString, playerId);
    if (!inspectorPlayer)
        return makeUnexpected(errorString);
    
    auto* node = inspectorPlayer->mediaElement();
    if (!node)
        return makeUnexpected("Missing element of player for given playerId"_s);
    
    auto* state = node->scriptExecutionContext()->globalObject();
    auto injectedScript = m_injectedScriptManager.injectedScriptFor(state);
    ASSERT(!injectedScript.hasNoValue());
    
    JSC::JSValue value;
    {
        JSC::JSLockHolder lock(state);

        auto* globalObject = deprecatedGlobalObjectForPrototype(state);
        value = toJS(state, globalObject, node);
    }
    
    if (!value) {
        ASSERT_NOT_REACHED();
        return makeUnexpected("Internal error: unknown HTMLMediaElement for given playerId"_s);
    }

    auto object = injectedScript.wrapObject(value, objectGroup);
    if (!object)
        return makeUnexpected("Internal error: unable to cast HTMLMediaElement"_s);

    return object.releaseNonNull();
}

Protocol::ErrorStringOr<void> InspectorMediaAgent::play(const Protocol::Media::MediaPlayerId& playerId)
{
    Protocol::ErrorString errorString;
    
    auto inspectorPlayer = assertInspectorPlayer(errorString, playerId);
    if (!inspectorPlayer)
        return makeUnexpected(errorString);
    
    inspectorPlayer->play();
    return { };
}

Protocol::ErrorStringOr<void> InspectorMediaAgent::pause(const Protocol::Media::MediaPlayerId& playerId)
{
    Protocol::ErrorString errorString;
    
    auto inspectorPlayer = assertInspectorPlayer(errorString, playerId);
    if (!inspectorPlayer)
        return makeUnexpected(errorString);
    
    inspectorPlayer->pause();
    return { };
}

void InspectorMediaAgent::didCreateMediaPlayer(HTMLMediaElement& mediaElement)
{
    if (!mediaElement.identifier()) {
        ASSERT_NOT_REACHED();
        return;
    }
    
    bindPlayer(mediaElement);
}

void InspectorMediaAgent::didDestroyMediaPlayer(HTMLMediaElement& mediaElement)
{
    if (!mediaElement.identifier()) {
        ASSERT_NOT_REACHED();
        return;
    }
    
    auto playerId = mediaElement.identifier().loggingString();
    auto inspectorPlayer = m_identifierToInspectorPlayer.get(playerId);
    if (!inspectorPlayer)
        return;
    
    unbindPlayer(*inspectorPlayer);
}

void InspectorMediaAgent::didUpdateMediaPlayer(HTMLMediaElement& mediaElement, const String& eventString)
{
    auto event = Protocol::Helpers::parseEnumValueFromString<Protocol::Media::MediaPlayerEvent>(eventString);
    if (!event || !mediaElement.identifier()) {
        ASSERT_NOT_REACHED();
        return;
    }
    
    auto playerId = mediaElement.identifier().loggingString();
    auto inspectorPlayer = m_identifierToInspectorPlayer.get(playerId);
    if (!inspectorPlayer) {
        bindPlayer(mediaElement);
        inspectorPlayer = m_identifierToInspectorPlayer.get(playerId);
    }
    
    if (!inspectorPlayer || !inspectorPlayer->mediaPlayer())
        return;
    
    m_frontendDispatcher->playerUpdated(inspectorPlayer->buildObjectForMediaPlayer(), *event, JSON::Value::create(String("hello"_s)));
}

void InspectorMediaAgent::reset()
{
    m_identifierToInspectorPlayer.clear();
    m_removedPlayerIdentifiers.clear();
}

void InspectorMediaAgent::bindPlayer(HTMLMediaElement& mediaElement)
{
    if (!mediaElement.player())
        return;
    
    auto inspectorPlayer = InspectorMediaPlayer::create(mediaElement);
    m_identifierToInspectorPlayer.set(inspectorPlayer->identifier(), inspectorPlayer.copyRef());
    
    m_frontendDispatcher->playerAdded(inspectorPlayer->buildObjectForMediaPlayer());
}

void InspectorMediaAgent::unbindPlayer(InspectorMediaPlayer& inspectorPlayer)
{
    auto identifier = inspectorPlayer.identifier();
    m_identifierToInspectorPlayer.remove(identifier);
    
    // This can be called in response to GC. Due to the single-process model used in WebKit1, the
    // event must be dispatched from a timer to prevent the frontend from making JS allocations
    // while the GC is still active.
    m_removedPlayerIdentifiers.append(identifier);

    if (!m_playerDestroyedTimer.isActive())
        m_playerDestroyedTimer.startOneShot(0_s);
}

RefPtr<InspectorMediaPlayer> InspectorMediaAgent::assertInspectorPlayer(Protocol::ErrorString& errorString, const String& playerId)
{
    auto inspectorPlayer = m_identifierToInspectorPlayer.get(playerId);
    if (!inspectorPlayer) {
        errorString = "Missing player for given playerId"_s;
        return nullptr;
    }
    return inspectorPlayer;
}

void InspectorMediaAgent::playerDestroyedTimerFired()
{
    if (!m_removedPlayerIdentifiers.size())
        return;

    for (auto& identifier : m_removedPlayerIdentifiers)
        m_frontendDispatcher->playerRemoved(identifier);

    m_removedPlayerIdentifiers.clear();
}

} // namespace WebCore
