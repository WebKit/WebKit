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
#include <WebCore/SharedBuffer.h>
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
// The snapshot starts with the root frame pending as if the frame reference was added with it.
class RemoteSnapshot final : public ThreadSafeRefCounted<RemoteSnapshot> {
    WTF_MAKE_NONCOPYABLE(RemoteSnapshot);
    WTF_MAKE_TZONE_ALLOCATED(RemoteSnapshot);
public:
    static Ref<RemoteSnapshot> create();
    ~RemoteSnapshot();
    [[nodiscard]] bool addFrameReference(WebCore::FrameIdentifier);
    [[nodiscard]] bool setFrame(WebCore::FrameIdentifier, Ref<const WebCore::DisplayList::DisplayList>&&, SerialFunctionDispatcher&);
    bool isComplete() const;
    std::optional<RefPtr<WebCore::SharedBuffer>> drawToPDF(const WebCore::FloatSize&, WebCore::FrameIdentifier rootFrameIdentifier);
    std::optional<WebCore::ShareableBitmap::Handle> drawToBitmap(const WebCore::FloatSize&, WebCore::FrameIdentifier rootFrameIdentifier);
    [[nodiscard]] bool applyFrame(WebCore::FrameIdentifier, WebCore::GraphicsContext&) const;

private:
    RemoteSnapshot();

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

    mutable Lock m_lock;
    // The map stores std::nullopt for the "referenced" and a value when the final value comes in.
    HashMap<WebCore::FrameIdentifier, std::optional<DisplayListAndReleaseDispatcher>> m_frameDisplayLists WTF_GUARDED_BY_LOCK(m_lock);
    size_t m_referencedFrames WTF_GUARDED_BY_LOCK(m_lock) { 1 }; // 1 means at least root is pending.
    size_t m_completedFrames WTF_GUARDED_BY_LOCK(m_lock) { 0 };
};

}

#endif
