/*
 * Copyright (C) 2010-2017 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/CrossTaskToken.h>
#include <WebCore/DOMPasteAccess.h>
#include <wtf/CanMakeWeakPtr.h>
#include <wtf/Function.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Noncopyable.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/UUID.h>
#include <wtf/Vector.h>
#include <wtf/WeakHashSet.h>

namespace JSC {
class VM;
}
namespace WebCore {

class Document;
class WeakPtrImplWithEventTargetData;

enum class IsProcessingUserGesture : uint8_t { No, Yes, Potentially };

enum class CanRequestDOMPaste : bool { No, Yes };
enum class UserGestureType : uint8_t { EscapeKey, ActivationTriggering, Other };
enum class ProcessInteractionStyle { Immediate, Delayed, Never };
enum class GestureScope : bool { All, MediaOnly };

struct UserGestureTokenData {
    IsProcessingUserGesture isProcessingUserGesture { IsProcessingUserGesture::No };
    UserGestureType userGestureType { UserGestureType::ActivationTriggering };
    std::optional<WTF::UUID> authorizationToken;
    CanRequestDOMPaste canRequestDOMPaste { CanRequestDOMPaste::No };
    MonotonicTime startTime { MonotonicTime::now() };
    DOMPasteAccessPolicy domPasteAccessPolicy { DOMPasteAccessPolicy::NotRequestedYet };
    GestureScope scope { GestureScope::All };

    bool hasExpired(Seconds expirationInterval) const
    {
        return startTime + expirationInterval < MonotonicTime::now();
    }
};

class UserGestureToken : public JSC::CrossTaskToken {
public:
    using GestureScope = WebCore::GestureScope;

    static constexpr Seconds maximumIntervalForUserGestureForwarding { 1_s }; // One second matches Gecko.
    static const Seconds& NODELETE maximumIntervalForUserGestureForwardingForFetch();
    WEBCORE_EXPORT static void NODELETE setMaximumIntervalForUserGestureForwardingForFetchForTesting(Seconds);

    static Ref<UserGestureToken> create(IsProcessingUserGesture, UserGestureType, Document*, std::optional<WTF::UUID> authorizationToken, CanRequestDOMPaste, MonotonicTime, DOMPasteAccessPolicy, GestureScope);

    WEBCORE_EXPORT ~UserGestureToken();

    IsProcessingUserGesture isProcessingUserGesture() const { return m_data.isProcessingUserGesture; }
    bool processingUserGesture() const { return m_data.scope == GestureScope::All && m_data.isProcessingUserGesture == IsProcessingUserGesture::Yes; }
    bool processingUserGestureForMedia() const { return m_data.isProcessingUserGesture == IsProcessingUserGesture::Yes || m_data.isProcessingUserGesture == IsProcessingUserGesture::Potentially; }
    UserGestureType gestureType() const { return m_data.userGestureType; }

    void addDestructionObserver(Function<void(UserGestureToken&)>&& observer)
    {
        m_destructionObservers.append(WTF::move(observer));
    }

    DOMPasteAccessPolicy domPasteAccessPolicy() const { return m_data.domPasteAccessPolicy; }
    void didRequestDOMPasteAccess(DOMPasteAccessResponse response)
    {
        switch (response) {
        case DOMPasteAccessResponse::DeniedForGesture:
            m_data.domPasteAccessPolicy = DOMPasteAccessPolicy::Denied;
            break;
        case DOMPasteAccessResponse::GrantedForCommand:
            break;
        case DOMPasteAccessResponse::GrantedForGesture:
            m_data.domPasteAccessPolicy = DOMPasteAccessPolicy::Granted;
            break;
        }
    }
    void resetDOMPasteAccess() { m_data.domPasteAccessPolicy = DOMPasteAccessPolicy::NotRequestedYet; }

    void setScope(GestureScope scope) { m_data.scope = scope; }
    void resetScope() { m_data.scope = GestureScope::All; }
    GestureScope scope() const { return m_data.scope; }

    // Expand the following methods if more propagation sources are added later.
    enum class ShouldPropagateToMicroTask : bool { No, Yes };
    void setShouldPropagateToMicroTask(ShouldPropagateToMicroTask is) { CrossTaskToken::setShouldPropagateToMicroTask(is == ShouldPropagateToMicroTask::Yes); }

    bool hasExpired(Seconds expirationInterval) const
    {
        return m_data.hasExpired(expirationInterval);
    }

    MonotonicTime startTime() const { return m_data.startTime; }

    std::optional<WTF::UUID> authorizationToken() const { return m_data.authorizationToken; }

    bool canRequestDOMPaste() const { return m_data.canRequestDOMPaste == CanRequestDOMPaste::Yes; }

    bool NODELETE isValidForDocument(const Document&) const;

    void forEachImpactedDocument(Function<void(Document&)>&&);

    RefPtr<JSC::MicrotaskDispatcher> createMicrotaskDispatcher(JSC::VM&, JSC::JSGlobalObject*) override;

    const UserGestureTokenData& data() const { return m_data; }

private:
    UserGestureToken(IsProcessingUserGesture, UserGestureType, Document*, std::optional<WTF::UUID> authorizationToken, CanRequestDOMPaste, MonotonicTime, DOMPasteAccessPolicy, GestureScope);

    UserGestureTokenData m_data;
    Vector<Function<void(UserGestureToken&)>> m_destructionObservers;
    WeakHashSet<Document, WeakPtrImplWithEventTargetData> m_documentsImpactedByUserGesture;
};

class UserGestureIndicator {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(UserGestureIndicator, WEBCORE_EXPORT);
    WTF_MAKE_NONCOPYABLE(UserGestureIndicator);
public:
    WEBCORE_EXPORT static RefPtr<UserGestureToken> currentUserGesture();

    WEBCORE_EXPORT static bool processingUserGesture(const Document* = nullptr);
    WEBCORE_EXPORT static bool processingUserGestureForMedia();

    // If a document is provided, its last known user gesture timestamp is updated.
    using ProcessInteractionStyle = WebCore::ProcessInteractionStyle;
    WEBCORE_EXPORT explicit UserGestureIndicator(const UserGestureTokenData&, Document*);
    WEBCORE_EXPORT explicit UserGestureIndicator(std::optional<IsProcessingUserGesture>, Document* = nullptr, UserGestureType = UserGestureType::ActivationTriggering, ProcessInteractionStyle = ProcessInteractionStyle::Immediate, std::optional<WTF::UUID> authorizationToken = std::nullopt, CanRequestDOMPaste = CanRequestDOMPaste::Yes, MonotonicTime startTime = MonotonicTime::now(), DOMPasteAccessPolicy = DOMPasteAccessPolicy::NotRequestedYet, GestureScope = GestureScope::All);
    WEBCORE_EXPORT explicit UserGestureIndicator(RefPtr<UserGestureToken>, UserGestureToken::GestureScope = UserGestureToken::GestureScope::All, UserGestureToken::ShouldPropagateToMicroTask = UserGestureToken::ShouldPropagateToMicroTask::No);
    WEBCORE_EXPORT ~UserGestureIndicator();

    WEBCORE_EXPORT std::optional<WTF::UUID> authorizationToken() const;

private:
    RefPtr<UserGestureToken> m_previousToken;
};

}
