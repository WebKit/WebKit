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

#ifndef ProcessThrottler_h
#define ProcessThrottler_h

#include "ProcessAssertion.h"

#include <WebCore/Timer.h>
#include <wtf/WeakPtr.h>

#if PLATFORM(IOS)

namespace WebKit {
    
class ProcessThrottler {
public:
    enum class Visibility {
        Visible,
        Hiding,
        Hidden
    };
    
    class VisibilityToken {
    public:
        VisibilityToken(ProcessThrottler&, Visibility);
        ~VisibilityToken();
        
        Visibility visibility() const
        {
            return m_visibility;
        }
        
        void setVisibility(Visibility visibility)
        {
            if (m_visibility != visibility)
                setVisibilityInternal(visibility);
            
            if (visibility == Visibility::Hiding)
                m_hideTimer.startOneShot(30);
            else
                m_hideTimer.stop();
        }
        
    private:
        void hideTimerFired(WebCore::Timer<VisibilityToken>*);
        void setVisibilityInternal(Visibility);
        
        WeakPtr<ProcessThrottler> m_throttler;
        Visibility m_visibility;
        WebCore::Timer<VisibilityToken> m_hideTimer;
    };
    
    ProcessThrottler()
        : m_weakPtrFactory(this)
        , m_visibleCount(0)
        , m_hidingCount(0)
    {
    }
    
    std::unique_ptr<VisibilityToken> visibilityToken(Visibility visibility)
    {
        return std::make_unique<VisibilityToken>(*this, visibility);
    }
    
    void didConnnectToProcess(pid_t pid)
    {
        m_assertion = std::make_unique<ProcessAssertion>(pid, assertionState());
    }
    
private:
    friend class VisibilityToken;
    WeakPtr<ProcessThrottler> weakPtr() { return m_weakPtrFactory.createWeakPtr(); }
    
    AssertionState assertionState()
    {
        if (m_visibleCount)
            return AssertionState::Foreground;
        if (m_hidingCount)
            return AssertionState::Background;
        return AssertionState::Suspended;
    }
    
    void updateAssertion()
    {
        if (m_assertion)
            m_assertion->setState(assertionState());
    }
    
    WeakPtrFactory<ProcessThrottler> m_weakPtrFactory;
    std::unique_ptr<ProcessAssertion> m_assertion;
    unsigned m_visibleCount;
    unsigned m_hidingCount;
};
    
}

#endif // PLATFORM(IOS)

#endif // ProcessThrottler_h
