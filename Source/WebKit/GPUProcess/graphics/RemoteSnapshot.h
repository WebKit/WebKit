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

#if ENABLE(GPU_PROCESS)

#include <WebCore/DisplayList.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/ShareableBitmap.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/CompletionHandler.h>
#include <wtf/FunctionDispatcher.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebKit {

// RemoteSnapshot represents a web page rendering. Solves the problem of generating the rendering from various different WebContent processes that
// should not have any access to data of each other.
// Each display list receives a placeholder for their subframe display lists. The placeholders are resolved through applyFrame().
// The snapshot is complete once every placeholder has been resolved, either with a display list or by the frame being
// abandoned, in which case it draws nothing. The root is unresolved from the start, and a frame's placeholders are
// recorded before its own display list is sunk, so nothing left unresolved means nothing is still to come.
class RemoteSnapshot final : public ThreadSafeRefCounted<RemoteSnapshot> {
    WTF_MAKE_NONCOPYABLE(RemoteSnapshot);
    WTF_MAKE_TZONE_ALLOCATED(RemoteSnapshot);
public:
    // Without a root, for a recorder whose snapshot no longer exists. It records into nothing.
    static Ref<RemoteSnapshot> create(std::optional<WebCore::FrameIdentifier> rootFrameIdentifier);
    ~RemoteSnapshot();
    std::optional<WebCore::FrameIdentifier> rootFrameIdentifier() const { return m_rootFrameIdentifier; }
    // The size the root was recorded at, which is what the snapshot is drawn at.
    WebCore::FloatSize size() const;
    void setSize(const WebCore::FloatSize&);
    [[nodiscard]] bool addFrameReference(WebCore::FrameIdentifier);
    [[nodiscard]] bool setFrame(WebCore::FrameIdentifier, Ref<const WebCore::DisplayList::DisplayList>&&, SerialFunctionDispatcher&);
    // Resolves a frame that will not be recorded, so that the snapshot does not wait for it.
    void abandonFrame(WebCore::FrameIdentifier);
    // Notes which process was asked to record a frame, so that the frame can be abandoned if that process goes away.
    void setFrameOwner(WebCore::FrameIdentifier, WebCore::ProcessIdentifier);
    void abandonFramesOwnedBy(WebCore::ProcessIdentifier);
    // Gives up on every frame still to come, so that the snapshot completes without them.
    void abandonUnresolvedFrames();
    // Completes every waiter unsuccessfully, for a snapshot that has been released before it completed.
    void fail();
    bool isComplete() const;
    // Whether it completed without its root, which leaves nothing to draw.
    bool hasFailed() const;
    // Called on the main thread, in order, once the snapshot is complete or has failed.
    void whenComplete(CompletionHandler<void(bool success)>&&);
    // Whether anything is waiting for it to complete.
    bool isAwaited() const;
    std::optional<RefPtr<WebCore::SharedBuffer>> drawToPDF(const WebCore::FloatSize&, WebCore::FrameIdentifier rootFrameIdentifier);
    std::optional<WebCore::ShareableBitmap::Handle> drawToBitmap(const WebCore::FloatSize&, WebCore::FrameIdentifier rootFrameIdentifier);
    [[nodiscard]] bool applyFrame(WebCore::FrameIdentifier, WebCore::GraphicsContext&) const;

private:
    explicit RemoteSnapshot(std::optional<WebCore::FrameIdentifier> rootFrameIdentifier);

    // DisplayList isn't generally threadsafe, but should be fine to replay on a different
    // thread in the GPU (where Font objects don't get mutated). Make sure we manually
    // return the refs to the originating work queue to avoid ref counting races.
    class DisplayListAndReleaseDispatcher {
    public:
        DisplayListAndReleaseDispatcher(Ref<const WebCore::DisplayList::DisplayList>&&, SerialFunctionDispatcher&);
        DisplayListAndReleaseDispatcher(DisplayListAndReleaseDispatcher&&) = default;
        DisplayListAndReleaseDispatcher& operator=(DisplayListAndReleaseDispatcher&&) = default;
        ~DisplayListAndReleaseDispatcher();

        const WebCore::DisplayList::DisplayList* displayList() const { return m_displayList.get(); }

    private:
        RefPtr<const WebCore::DisplayList::DisplayList> m_displayList;
        Ref<SerialFunctionDispatcher> m_dispatcher;
    };

    struct Frame {
        std::optional<DisplayListAndReleaseDispatcher> displayList;
        bool isAbandoned { false };
        bool isResolved() const { return displayList || isAbandoned; }
    };

    bool isCompleteWithLockHeld() const WTF_REQUIRES_LOCK(m_lock) { return m_hasFailed || !m_unresolvedFrames; }
    bool hasFailedWithLockHeld() const WTF_REQUIRES_LOCK(m_lock);
    void resolveFrameWithLockHeld(Frame&) WTF_REQUIRES_LOCK(m_lock);
    void dispatchCompletionHandlersIfComplete() WTF_REQUIRES_LOCK(m_lock);

    const std::optional<WebCore::FrameIdentifier> m_rootFrameIdentifier;
    mutable Lock m_lock;
    WebCore::FloatSize m_size WTF_GUARDED_BY_LOCK(m_lock);
    // A frame is added unresolved when it is referenced, and resolved when its display list comes in or it is
    // abandoned. A frame can also come in before it is referenced.
    HashMap<WebCore::FrameIdentifier, Frame> m_frames WTF_GUARDED_BY_LOCK(m_lock);
    HashMap<WebCore::FrameIdentifier, WebCore::ProcessIdentifier> m_frameOwners WTF_GUARDED_BY_LOCK(m_lock);
    size_t m_unresolvedFrames WTF_GUARDED_BY_LOCK(m_lock) { 0 };
    bool m_hasFailed WTF_GUARDED_BY_LOCK(m_lock) { false };
    Vector<CompletionHandler<void(bool)>> m_completionHandlers WTF_GUARDED_BY_LOCK(m_lock);
};

}

#endif
