/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if ENABLE(THREADED_ANIMATIONS)

#include "RemoteAnimationTimeline.h"
#include <WebCore/ProgressResolutionData.h>
#include <WebCore/ScrollingNodeID.h>
#include <wtf/Ref.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {
class AcceleratedTimeline;
class ScrollingTreeScrollingNode;
}

namespace WebKit {

class RemoteProgressBasedTimeline final : public RemoteAnimationTimeline {
    WTF_MAKE_TZONE_ALLOCATED(RemoteProgressBasedTimeline);
public:
    static Ref<RemoteProgressBasedTimeline> create(TimelineID, const WebCore::ProgressResolutionData&);

    WebCore::ScrollingNodeID source() const { return m_resolutionData.source; }
    WebCore::ProgressResolutionData resolutionData() const { return m_resolutionData; }
    void setResolutionData(const WebCore::ScrollingTreeScrollingNode*, WebCore::ProgressResolutionData);

    void updateCurrentTime(const WebCore::ScrollingTreeScrollingNode&);

private:
    RemoteProgressBasedTimeline(TimelineID, const WebCore::ProgressResolutionData&);

    void updateCurrentTime();

    WebCore::ProgressResolutionData m_resolutionData;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_REMOTE_ANIMATION_TIMELINE(RemoteProgressBasedTimeline, isProgressBased())

#endif // ENABLE(THREADED_ANIMATIONS)
