/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "UserGestureIndicator.h"

namespace WebCore {

namespace {

class GestureToken : public UserGestureToken {
public:
    static PassRefPtr<UserGestureToken> create() { return adoptRef(new GestureToken); }

    virtual ~GestureToken() { }
    virtual bool hasGestures() const OVERRIDE { return m_consumableGestures > 0; }

    void addGesture() { m_consumableGestures++; }
    bool consumeGesture()
    {
        if (!m_consumableGestures)
            return false;
        m_consumableGestures--;
        return true;
    }

private:
    GestureToken()
        : m_consumableGestures(0)
    {
    }

    size_t m_consumableGestures;
};

}

static bool isDefinite(ProcessingUserGestureState state)
{
    return state == DefinitelyProcessingNewUserGesture || state == DefinitelyProcessingUserGesture || state == DefinitelyNotProcessingUserGesture;
}

ProcessingUserGestureState UserGestureIndicator::s_state = DefinitelyNotProcessingUserGesture;
UserGestureIndicator* UserGestureIndicator::s_topmostIndicator = 0;

UserGestureIndicator::UserGestureIndicator(ProcessingUserGestureState state)
    : m_previousState(s_state)
{
    // We overwrite s_state only if the caller is definite about the gesture state.
    if (isDefinite(state)) {
        if (!s_topmostIndicator) {
            s_topmostIndicator = this;
            m_token = GestureToken::create();
        } else
            m_token = s_topmostIndicator->currentToken();
        s_state = state;
    }

    if (state == DefinitelyProcessingNewUserGesture)
        static_cast<GestureToken*>(m_token.get())->addGesture();
    else if (state == DefinitelyProcessingUserGesture && s_topmostIndicator == this)
        static_cast<GestureToken*>(m_token.get())->addGesture();
    ASSERT(isDefinite(s_state));
}

UserGestureIndicator::UserGestureIndicator(PassRefPtr<UserGestureToken> token)
    : m_previousState(s_state)
{
    if (token && static_cast<GestureToken*>(token.get())->hasGestures()) {
        if (!s_topmostIndicator) {
            s_topmostIndicator = this;
            m_token = token;
        } else {
            m_token = s_topmostIndicator->currentToken();
            static_cast<GestureToken*>(m_token.get())->addGesture();
            static_cast<GestureToken*>(token.get())->consumeGesture();
        }
        s_state = DefinitelyProcessingUserGesture;
    }

    ASSERT(isDefinite(s_state));
}

UserGestureIndicator::~UserGestureIndicator()
{
    s_state = m_previousState;
    if (s_topmostIndicator == this)
        s_topmostIndicator = 0;
    ASSERT(isDefinite(s_state));
}

bool UserGestureIndicator::processingUserGesture()
{
    return s_topmostIndicator && static_cast<GestureToken*>(s_topmostIndicator->currentToken())->hasGestures() && (s_state == DefinitelyProcessingNewUserGesture || s_state == DefinitelyProcessingUserGesture);
}

bool UserGestureIndicator::consumeUserGesture()
{
    if (!s_topmostIndicator)
        return false;
    return static_cast<GestureToken*>(s_topmostIndicator->currentToken())->consumeGesture();
}

UserGestureToken* UserGestureIndicator::currentToken()
{
    if (!s_topmostIndicator)
        return 0;
    return s_topmostIndicator->m_token.get();
}

UserGestureIndicatorDisabler::UserGestureIndicatorDisabler()
    : m_savedState(UserGestureIndicator::s_state)
    , m_savedIndicator(UserGestureIndicator::s_topmostIndicator)
{
    UserGestureIndicator::s_state = DefinitelyNotProcessingUserGesture;
    UserGestureIndicator::s_topmostIndicator = 0;
}

UserGestureIndicatorDisabler::~UserGestureIndicatorDisabler()
{
    UserGestureIndicator::s_state = m_savedState;
    UserGestureIndicator::s_topmostIndicator = m_savedIndicator;
}

}
