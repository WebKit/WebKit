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

#pragma once

#include "InspectorMediaPlayer.h"
#include "InspectorWebAgentBase.h"
#include <initializer_list>
#include <wtf/Forward.h>
#include <wtf/RobinHoodHashMap.h>
#include <wtf/RobinHoodHashSet.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace Inspector {
class InjectedScriptManager;
}

namespace WebCore {

class Page;

class InspectorMediaAgent final : public InspectorAgentBase, public Inspector::MediaBackendDispatcherHandler {
    WTF_MAKE_NONCOPYABLE(InspectorMediaAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    InspectorMediaAgent(PageAgentContext&);
    ~InspectorMediaAgent();

    // InspectorAgentBase
    void didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*);
    void willDestroyFrontendAndBackend(Inspector::DisconnectReason);
    void discardAgent();
    
    // MediaBackendDispatcherHandler
    Inspector::Protocol::ErrorStringOr<void> enable();
    Inspector::Protocol::ErrorStringOr<void> disable();
    Inspector::Protocol::ErrorStringOr<Inspector::Protocol::DOM::NodeId> requestNode(const Inspector::Protocol::Media::MediaPlayerId&);
    Inspector::Protocol::ErrorStringOr<Ref<Inspector::Protocol::Runtime::RemoteObject>> resolveElement(const Inspector::Protocol::Media::MediaPlayerId&, const String& objectGroup);
    Inspector::Protocol::ErrorStringOr<void> play(const Inspector::Protocol::Media::MediaPlayerId&);
    Inspector::Protocol::ErrorStringOr<void> pause(const Inspector::Protocol::Media::MediaPlayerId&);

    // InspectorInstrumentation
    void didCreateMediaPlayer(HTMLMediaElement&);
    void didDestroyMediaPlayer(HTMLMediaElement&);
    void didUpdateMediaPlayer(HTMLMediaElement&, const String&);

private:
    void reset();
    
    void bindPlayer(HTMLMediaElement&);
    void unbindPlayer(InspectorMediaPlayer&);
    RefPtr<InspectorMediaPlayer> assertInspectorPlayer(Inspector::Protocol::ErrorString&, const String& playerId);
    
    void playerDestroyedTimerFired();
    
//    InspectorCanvas& bindMediaPlayer(CanvasRenderingContext&, bool captureBacktrace);
//    void unbindCanvas(InspectorCanvas&);
//    RefPtr<InspectorCanvas> assertInspectorCanvas(Inspector::Protocol::ErrorString&, const String& canvasId);
//    RefPtr<InspectorCanvas> findInspectorCanvas(CanvasRenderingContext&);

    std::unique_ptr<Inspector::MediaFrontendDispatcher> m_frontendDispatcher;
    RefPtr<Inspector::MediaBackendDispatcher> m_backendDispatcher;

    Inspector::InjectedScriptManager& m_injectedScriptManager;
    Page& m_inspectedPage;

    MemoryCompactRobinHoodHashMap<String, RefPtr<InspectorMediaPlayer>> m_identifierToInspectorPlayer;
    Vector<String> m_removedPlayerIdentifiers;
    Timer m_playerDestroyedTimer;
};

} // namespace WebCore
