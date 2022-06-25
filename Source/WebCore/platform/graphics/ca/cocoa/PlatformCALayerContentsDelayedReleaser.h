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

#pragma once

#if PLATFORM(MAC)

#include <wtf/Lock.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class PlatformCALayer;

// This class exists to work around rdar://85892959, where CABackingStore objects would get released on the ScrollingThread
// during scrolling commits, which can take long enough to cause scrolling frame drops.
class PlatformCALayerContentsDelayedReleaser : ThreadSafeRefCounted<PlatformCALayerContentsDelayedReleaser> {
    WTF_MAKE_NONCOPYABLE(PlatformCALayerContentsDelayedReleaser);
public:
    static PlatformCALayerContentsDelayedReleaser& singleton();

    void takeLayerContents(PlatformCALayer&);
    
    void mainThreadCommitWillStart();
    void mainThreadCommitDidEnd();

    void scrollingThreadCommitWillStart();
    void scrollingThreadCommitDidEnd();

private:
    friend LazyNeverDestroyed<PlatformCALayerContentsDelayedReleaser>;
    
    PlatformCALayerContentsDelayedReleaser();

    void updateSawOverlappingCommit() WTF_REQUIRES_LOCK(m_lock);
    void clearRetainedContents();

    Vector<RetainPtr<CFTypeRef>> m_retainedContents;

    Lock m_lock;
    unsigned m_inMainThreadCommitEntryCount WTF_GUARDED_BY_LOCK(m_lock) { 0 };
    unsigned m_scrollingThreadCommitEntryCount WTF_GUARDED_BY_LOCK(m_lock) { 0 };
    bool m_hadOverlappingCommit WTF_GUARDED_BY_LOCK(m_lock) { false };
};

} // namespace WebCore

#endif // PLATFORM(MAC)
