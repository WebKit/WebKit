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

#include "DOMPasteAccess.h"
#include <wtf/Function.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class Document;

enum ProcessingUserGestureState {
    ProcessingUserGesture,
    ProcessingPotentialUserGesture,
    NotProcessingUserGesture
};

enum class UserGestureType { EscapeKey, Other };

class UserGestureToken : public RefCounted<UserGestureToken> {
public:
    static Ref<UserGestureToken> create(ProcessingUserGestureState state, UserGestureType gestureType)
    {
        return adoptRef(*new UserGestureToken(state, gestureType));
    }

    WEBCORE_EXPORT ~UserGestureToken();

    ProcessingUserGestureState state() const { return m_state; }
    bool processingUserGesture() const { return m_scope == GestureScope::All && m_state == ProcessingUserGesture; }
    bool processingUserGestureForMedia() const { return m_state == ProcessingUserGesture || m_state == ProcessingPotentialUserGesture; }
    UserGestureType gestureType() const { return m_gestureType; }

    void addDestructionObserver(WTF::Function<void (UserGestureToken&)>&& observer)
    {
        m_destructionObservers.append(WTFMove(observer));
    }

    DOMPasteAccessPolicy domPasteAccessPolicy() const { return m_domPasteAccessPolicy; }
    void didRequestDOMPasteAccess(DOMPasteAccessResponse response)
    {
        switch (response) {
        case DOMPasteAccessResponse::DeniedForGesture:
            m_domPasteAccessPolicy = DOMPasteAccessPolicy::Denied;
            break;
        case DOMPasteAccessResponse::GrantedForCommand:
            break;
        case DOMPasteAccessResponse::GrantedForGesture:
            m_domPasteAccessPolicy = DOMPasteAccessPolicy::Granted;
            break;
        }
    }
    void resetDOMPasteAccess() { m_domPasteAccessPolicy = DOMPasteAccessPolicy::NotRequestedYet; }

    enum class GestureScope { All, MediaOnly };
    void setScope(GestureScope scope) { m_scope = scope; }
    void resetScope() { m_scope = GestureScope::All; }

    bool hasExpired(Seconds expirationInterval) const
    {
        return m_startTime + expirationInterval < MonotonicTime::now();
    }

private:
    UserGestureToken(ProcessingUserGestureState state, UserGestureType gestureType)
        : m_state(state)
        , m_gestureType(gestureType)
    {
    }

    ProcessingUserGestureState m_state = NotProcessingUserGesture;
    Vector<WTF::Function<void (UserGestureToken&)>> m_destructionObservers;
    UserGestureType m_gestureType;
    DOMPasteAccessPolicy m_domPasteAccessPolicy { DOMPasteAccessPolicy::NotRequestedYet };
    GestureScope m_scope { GestureScope::All };
    MonotonicTime m_startTime { MonotonicTime::now() };
};

class UserGestureIndicator {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(UserGestureIndicator);
public:
    WEBCORE_EXPORT static RefPtr<UserGestureToken> currentUserGesture();

    WEBCORE_EXPORT static bool processingUserGesture();
    WEBCORE_EXPORT static bool processingUserGestureForMedia();

    // If a document is provided, its last known user gesture timestamp is updated.
    enum class ProcessInteractionStyle { Immediate, Delayed };
    WEBCORE_EXPORT explicit UserGestureIndicator(Optional<ProcessingUserGestureState>, Document* = nullptr, UserGestureType = UserGestureType::Other, ProcessInteractionStyle = ProcessInteractionStyle::Immediate);
    WEBCORE_EXPORT explicit UserGestureIndicator(RefPtr<UserGestureToken>, UserGestureToken::GestureScope = UserGestureToken::GestureScope::All);
    WEBCORE_EXPORT ~UserGestureIndicator();

private:
    RefPtr<UserGestureToken> m_previousToken;
};

}
