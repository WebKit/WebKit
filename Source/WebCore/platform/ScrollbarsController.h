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

#include "FloatPoint.h"
#include "FloatSize.h"
#include <wtf/FastMalloc.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Scrollbar;
class ScrollableArea;

class ScrollbarsController {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(ScrollbarsController);
public:
    static std::unique_ptr<ScrollbarsController> create(ScrollableArea&);

    explicit ScrollbarsController(ScrollableArea&);
    virtual ~ScrollbarsController() = default;
    
    ScrollableArea& scrollableArea() const { return m_scrollableArea; }

    bool scrollbarAnimationsUnsuspendedByUserInteraction() const { return m_scrollbarAnimationsUnsuspendedByUserInteraction; }
    void setScrollbarAnimationsUnsuspendedByUserInteraction(bool unsuspended) { m_scrollbarAnimationsUnsuspendedByUserInteraction = unsuspended; }
    
    bool shouldSuspendScrollbarAnimations() const;

    virtual void notifyContentAreaScrolled(const FloatSize&) { }

    virtual void cancelAnimations();

    virtual void didBeginScrollGesture();
    virtual void didEndScrollGesture();
    virtual void mayBeginScrollGesture();

    virtual void contentAreaWillPaint() const { }
    virtual void mouseEnteredContentArea() { }
    virtual void mouseExitedContentArea() { }
    virtual void mouseMovedInContentArea() { }
    virtual void mouseEnteredScrollbar(Scrollbar*) const { }
    virtual void mouseExitedScrollbar(Scrollbar*) const { }
    virtual void mouseIsDownInScrollbar(Scrollbar*, bool) const { }
    virtual void willStartLiveResize() { }
    virtual void contentsSizeChanged() const { }
    virtual void willEndLiveResize() { }
    virtual void contentAreaDidShow() { }
    virtual void contentAreaDidHide() { }

    virtual void lockOverlayScrollbarStateToHidden(bool) { }
    virtual bool scrollbarsCanBeActive() const { return true; }

    virtual void didAddVerticalScrollbar(Scrollbar*) { }
    virtual void willRemoveVerticalScrollbar(Scrollbar*) { }
    virtual void didAddHorizontalScrollbar(Scrollbar*) { }
    virtual void willRemoveHorizontalScrollbar(Scrollbar*) { }

    virtual void invalidateScrollbarPartLayers(Scrollbar*) { }

    virtual void verticalScrollbarLayerDidChange() { }
    virtual void horizontalScrollbarLayerDidChange() { }

    virtual bool shouldScrollbarParticipateInHitTesting(Scrollbar*) { return true; }

    virtual String horizontalScrollbarStateForTesting() const { return emptyString(); }
    virtual String verticalScrollbarStateForTesting() const { return emptyString(); }

private:
    ScrollableArea& m_scrollableArea;
    bool m_scrollbarAnimationsUnsuspendedByUserInteraction { true };
};

} // namespace WebCore
