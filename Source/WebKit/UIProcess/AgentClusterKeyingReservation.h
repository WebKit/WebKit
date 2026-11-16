/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <WebCore/OriginKeyed.h>
#include <WebCore/SecurityOriginData.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

class BrowsingContextGroup;

class AgentClusterKeyingReservation : public RefCountedAndCanMakeWeakPtr<AgentClusterKeyingReservation> {
public:
    static Ref<AgentClusterKeyingReservation> create(BrowsingContextGroup& group, const WebCore::SecurityOriginData& origin, WebCore::OriginKeyed keying) { return adoptRef(*new AgentClusterKeyingReservation(group, origin, keying)); }
    ~AgentClusterKeyingReservation();

    const WebCore::SecurityOriginData& origin() const LIFETIME_BOUND { return m_origin; }
    WebCore::OriginKeyed keying() const { return m_keying; }

private:
    AgentClusterKeyingReservation(BrowsingContextGroup&, const WebCore::SecurityOriginData&, WebCore::OriginKeyed);

    const WeakPtr<BrowsingContextGroup> m_group;
    const WebCore::SecurityOriginData m_origin;
    const WebCore::OriginKeyed m_keying;
};

enum class AgentClusterKeyingRecording : bool { Now, OnCommit };

} // namespace WebKit
