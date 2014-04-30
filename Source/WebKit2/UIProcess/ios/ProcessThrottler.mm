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
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO , PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "ProcessThrottler.h"

#if PLATFORM(IOS)

namespace WebKit {

ProcessThrottler::VisibilityToken::VisibilityToken(ProcessThrottler& throttler, Visibility visibility)
    : m_throttler(throttler.weakPtr())
    , m_visibility(Visibility::Hidden)
    , m_hideTimer(this, &VisibilityToken::hideTimerFired)
{
    setVisibility(visibility);
}

ProcessThrottler::VisibilityToken::~VisibilityToken()
{
    setVisibility(Visibility::Hidden);
}

void ProcessThrottler::VisibilityToken::hideTimerFired(WebCore::Timer<VisibilityToken>*)
{
    ASSERT(m_visibility == Visibility::Hiding);
    m_visibility = Visibility::Hidden;
}

void ProcessThrottler::VisibilityToken::setVisibilityInternal(Visibility visibility)
{
    ProcessThrottler* throttler = m_throttler.get();
    if (!throttler) {
        m_visibility = visibility;
        return;
    }
    
    if (m_visibility == Visibility::Visible)
        throttler->m_visibleCount--;
    else if (m_visibility == Visibility::Hiding)
        throttler->m_hidingCount--;
    
    m_visibility = visibility;
    
    if (m_visibility == Visibility::Visible)
        throttler->m_visibleCount++;
    else if (m_visibility == Visibility::Hiding)
        throttler->m_hidingCount++;
    
    throttler->updateAssertion();
}
    
}

#endif // PLATFORM(IOS)
