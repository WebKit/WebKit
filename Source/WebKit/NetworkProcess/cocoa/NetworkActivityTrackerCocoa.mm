/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "NetworkActivityTracker.h"

#if HAVE(NW_ACTIVITY)

namespace WebKit {

NetworkActivityTracker::NetworkActivityTracker(Label label, Domain domain)
    : m_domain(domain)
    , m_label(label)
    , m_networkActivity(adoptNS(nw_activity_create(static_cast<uint32_t>(m_domain), static_cast<uint32_t>(m_label))))
{
}

NetworkActivityTracker::~NetworkActivityTracker()
{
}

void NetworkActivityTracker::setParent(NetworkActivityTracker& parent)
{
    ASSERT(m_networkActivity.get());
    ASSERT(parent.m_networkActivity.get());
    nw_activity_set_parent_activity(m_networkActivity.get(), parent.m_networkActivity.get());
}

void NetworkActivityTracker::start()
{
    ASSERT(m_networkActivity.get());
    nw_activity_activate(m_networkActivity.get());
}

void NetworkActivityTracker::complete(CompletionCode code)
{
    if (m_isCompleted)
        return;

    m_isCompleted = true;

    ASSERT(m_networkActivity.get());
    auto reason =
        code == CompletionCode::None ? nw_activity_completion_reason_none :
        code == CompletionCode::Success ? nw_activity_completion_reason_success :
        code == CompletionCode::Failure ? nw_activity_completion_reason_failure :
        nw_activity_completion_reason_invalid;
    nw_activity_complete_with_reason(m_networkActivity.get(), reason);
    m_networkActivity.clear();
}

} // namespace WebKit

#endif // HAVE(NW_ACTIVITY)
