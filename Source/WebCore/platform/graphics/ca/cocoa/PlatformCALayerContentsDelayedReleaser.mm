/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"
#import "PlatformCALayerContentsDelayedReleaser.h"

#if PLATFORM(MAC)

#import "PlatformCALayer.h"
#import <wtf/RunLoop.h>

namespace WebCore {

PlatformCALayerContentsDelayedReleaser& PlatformCALayerContentsDelayedReleaser::singleton()
{
    static LazyNeverDestroyed<PlatformCALayerContentsDelayedReleaser> delayedReleaser;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        delayedReleaser.construct();
    });

    return delayedReleaser;
}

PlatformCALayerContentsDelayedReleaser::PlatformCALayerContentsDelayedReleaser() = default;

void PlatformCALayerContentsDelayedReleaser::takeLayerContents(PlatformCALayer& layer)
{
    ASSERT(isMainThread());

    auto retainedContents = RetainPtr { layer.contents() };
    if (retainedContents)
        m_retainedContents.append(WTFMove(retainedContents));
    layer.setContents(nullptr);
}

void PlatformCALayerContentsDelayedReleaser::mainThreadCommitWillStart()
{
    Locker locker { m_lock };
    ++m_inMainThreadCommitEntryCount;
    updateSawOverlappingCommit();
}

void PlatformCALayerContentsDelayedReleaser::mainThreadCommitDidEnd()
{
    bool bothCommitsDone;
    bool hadOverlappingCommit;
    {
        Locker locker { m_lock };
        ASSERT(m_inMainThreadCommitEntryCount);
        --m_inMainThreadCommitEntryCount;
        bothCommitsDone = !m_scrollingThreadCommitEntryCount && !m_inMainThreadCommitEntryCount;
        hadOverlappingCommit = m_hadOverlappingCommit;
    }

    if (bothCommitsDone) {
        if (hadOverlappingCommit && m_retainedContents.size()) {
            RunLoop::mainSingleton().dispatch([] {
                PlatformCALayerContentsDelayedReleaser::singleton().clearRetainedContents();
            });
            return;
        }

        clearRetainedContents();
    }
}

void PlatformCALayerContentsDelayedReleaser::scrollingThreadCommitWillStart()
{
    ASSERT(!isMainThread());
    Locker locker { m_lock };
    ++m_scrollingThreadCommitEntryCount;
    updateSawOverlappingCommit();
}

void PlatformCALayerContentsDelayedReleaser::scrollingThreadCommitDidEnd()
{
    ASSERT(!isMainThread());
    Locker locker { m_lock };

    ASSERT(m_scrollingThreadCommitEntryCount);
    --m_scrollingThreadCommitEntryCount;
    if (!m_scrollingThreadCommitEntryCount && !m_inMainThreadCommitEntryCount) {
        if (m_hadOverlappingCommit) {
            // m_retainedContents might be empty (it's not protected by the lock so we can't check it here),
            // so this might be a pointless dispatch, but m_hadOverlappingCommit is rare.
            RunLoop::mainSingleton().dispatch([] {
                PlatformCALayerContentsDelayedReleaser::singleton().clearRetainedContents();
            });
        }
    }
}

void PlatformCALayerContentsDelayedReleaser::updateSawOverlappingCommit()
{
    m_hadOverlappingCommit |= (m_inMainThreadCommitEntryCount && m_scrollingThreadCommitEntryCount);
}

void PlatformCALayerContentsDelayedReleaser::clearRetainedContents()
{
    ASSERT(isMainThread());
    m_retainedContents.clear();
}

} // namespace WebCore

#endif

