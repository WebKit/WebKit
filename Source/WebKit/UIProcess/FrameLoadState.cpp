/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "FrameLoadState.h"

namespace WebKit {

FrameLoadState::~FrameLoadState()
{
}

void FrameLoadState::addObserver(Observer& observer)
{
    auto result = m_observers.add(observer);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void FrameLoadState::removeObserver(Observer& observer)
{
    auto result = m_observers.remove(observer);
    ASSERT_UNUSED(result, result);
}

void FrameLoadState::didStartProvisionalLoad(const URL& url)
{
    ASSERT(m_provisionalURL.isEmpty());

    m_state = State::Provisional;
    m_provisionalURL = url;
}

void FrameLoadState::didExplicitOpen(const URL& url)
{
    m_url = url;
    m_provisionalURL = { };
}

void FrameLoadState::didReceiveServerRedirectForProvisionalLoad(const URL& url)
{
    ASSERT(m_state == State::Provisional);

    m_provisionalURL = url;
}

void FrameLoadState::didFailProvisionalLoad()
{
    ASSERT(m_state == State::Provisional);

    m_state = State::Finished;
    m_provisionalURL = { };
    m_unreachableURL = m_lastUnreachableURL;
}

void FrameLoadState::didCommitLoad()
{
    ASSERT(m_state == State::Provisional);

    m_state = State::Committed;
    m_url = m_provisionalURL;
    m_provisionalURL = { };
}

void FrameLoadState::didFinishLoad()
{
    ASSERT(m_state == State::Committed);
    ASSERT(m_provisionalURL.isEmpty());

    m_state = State::Finished;

    Vector<Observer*> observersCopy;
    for (auto& observer : m_observers)
        observersCopy.append(&observer);
    for (auto* observer : observersCopy)
        observer->didFinishLoad();
}

void FrameLoadState::didFailLoad()
{
    ASSERT(m_state == State::Committed);
    ASSERT(m_provisionalURL.isEmpty());

    m_state = State::Finished;
}

void FrameLoadState::didSameDocumentNotification(const URL& url)
{
    m_url = url;
}

void FrameLoadState::setUnreachableURL(const URL& unreachableURL)
{
    m_lastUnreachableURL = m_unreachableURL;
    m_unreachableURL = unreachableURL;
}

} // namespace WebKit
