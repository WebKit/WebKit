/*
 * Copyright (c) 2016 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ScrollAnimator.h"

namespace WebCore {

// A Mock implementation of ScrollAnimator used to test the scroll events
// received by the scroll animator. Tests can enable this scroll animator using
// the internal setting enableMockScrollAnimator.
class ScrollAnimatorMock final : public ScrollAnimator {
public:
    ScrollAnimatorMock(ScrollableArea&, WTF::Function<void(const String&)>&&);
    virtual ~ScrollAnimatorMock();

private:

#if ENABLE(RUBBER_BANDING)
    bool allowsHorizontalStretching(const PlatformWheelEvent&) const override { return false; }
    bool allowsVerticalStretching(const PlatformWheelEvent&) const override { return false; }
    IntSize stretchAmount() const override { return IntSize(); }
    bool pinnedInDirection(const FloatSize&) const override { return false; }
    bool canScrollHorizontally() const override { return false; }
    bool canScrollVertically() const override { return false; }
    bool shouldRubberBandInDirection(ScrollDirection) const override { return false; }
    void immediateScrollBy(const FloatSize&) override { }
    void immediateScrollByWithoutContentEdgeConstraints(const FloatSize&) override { }
    void adjustScrollPositionToBoundsIfNecessary() override { }
#endif

    void didAddVerticalScrollbar(Scrollbar*) override;
    void didAddHorizontalScrollbar(Scrollbar*) override;
    void willRemoveVerticalScrollbar(Scrollbar*) override;
    void willRemoveHorizontalScrollbar(Scrollbar*) override;
    void mouseEnteredContentArea() override;
    void mouseMovedInContentArea() override;
    void mouseExitedContentArea() override;
    void mouseEnteredScrollbar(Scrollbar*) const override;
    void mouseExitedScrollbar(Scrollbar*) const override;
    void mouseIsDownInScrollbar(Scrollbar*, bool) const override;

    WTF::Function<void(const String&)> m_logger;
    Scrollbar* m_verticalScrollbar { nullptr };
    Scrollbar* m_horizontalScrollbar { nullptr };
};

} // namespace WebCore

