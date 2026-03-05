/*
* Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "FloatRect.h"
#include "ScrollTypes.h"
#include <wtf/CheckedRef.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class Document;
class LocalFrameView;
class RenderBox;
class RenderElement;
class RenderObject;
class ScrollableArea;
class WeakPtrImplWithEventTargetData;

enum class AnchorSearchStatus : uint8_t {
    // Exclude this node from anchoring.
    Exclude,
    // Check children; if no anchor found, keep traversing later siblings.
    Continue,
    // Check children; if no anchor found, choose this node.
    Constrain,
    // Choose this node.
    Choose,
};

class ScrollAnchoringController : public CanMakeCheckedPtr<ScrollAnchoringController> {
    WTF_MAKE_TZONE_ALLOCATED(ScrollAnchoringController);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(ScrollAnchoringController);
public:
    explicit ScrollAnchoringController(ScrollableArea&);
    ~ScrollAnchoringController();

    bool shouldMaintainScrollAnchor() const;

    void scrollPositionDidChange();
    void scrollerDidLayout();

    void clearAnchor(bool includeAncestors = false);
    void updateBeforeLayout();
    void adjustScrollPositionForAnchoring();

    void NODELETE willDispatchScrollEvent();
    void NODELETE didDispatchScrollEvent();

    void notifyChildHadSuppressingStyleChange(RenderElement&);

    bool hasAnchorElement() const { return !!m_anchorObject; }

    // These nest.
    void NODELETE startSuppressingScrollAnchoring();
    void NODELETE stopSuppressingScrollAnchoring();

private:
    static bool isViableStatus(AnchorSearchStatus status)
    {
        return status == AnchorSearchStatus::Constrain || status == AnchorSearchStatus::Choose;
    }

    LocalFrameView& frameView() const;

    bool findPriorityCandidate(Document&);

    AnchorSearchStatus examineAnchorCandidate(RenderObject&) const;
    AnchorSearchStatus examinePriorityCandidate(RenderElement&) const;

    AnchorSearchStatus findAnchorInOutOfFlowObjects(RenderObject&);
    AnchorSearchStatus findAnchorRecursive(RenderObject*);

    RenderBox* scrollableAreaBox() const;

    struct Rects {
        FloatRect boundsRelativeToScrolledContent;
        FloatRect scrollerContentsVisibleRect; // Takes scroll-padding into account.
    };

    Rects computeScrollerRelativeRects(RenderObject&) const;

    FloatPoint computeOffsetFromOwningScroller(RenderObject&) const;

    void invalidate();
    void chooseAnchorElement(Document&);
    bool anchoringSuppressedByStyleChange() const;
    void updateScrollableAreaRegistration();

    CheckedRef<ScrollableArea> m_owningScrollableArea;
    SingleThreadWeakPtr<RenderObject> m_anchorObject;
    FloatPoint m_lastAnchorOffset;

    bool m_isUpdatingScrollPositionForAnchoring { false };
    bool m_isQueuedForScrollPositionUpdate { false };
    bool m_anchoringSuppressedByStyleChange { false };
    unsigned m_inScrollEventCount { 0 };
    unsigned m_suppressionCount { 0 };
};

} // namespace WebCore
