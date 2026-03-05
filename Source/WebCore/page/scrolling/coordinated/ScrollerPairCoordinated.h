/*
 * Copyright (C) 2026 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if USE(COORDINATED_GRAPHICS_ASYNC_SCROLLBAR)

#include "ScrollingStateScrollingNode.h"
#include <wtf/Forward.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

class ScrollerCoordinated;
class ScrollingTreeScrollingNode;

class ScrollerPairCoordinated : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<ScrollerPairCoordinated> {
    WTF_MAKE_TZONE_ALLOCATED(ScrollerPairCoordinated);
    friend class ScrollerCoordinated;
public:
    static Ref<ScrollerPairCoordinated> create(ScrollingTreeScrollingNode& node)
    {
        return adoptRef(*new ScrollerPairCoordinated(node));
    }

    ~ScrollerPairCoordinated();

protected:
    friend class ScrollingTreeScrollingNodeDelegateCoordinated;
    ScrollerCoordinated& verticalScroller() { return m_verticalScroller.get(); }
    ScrollerCoordinated& horizontalScroller() { return m_horizontalScroller.get(); }

private:
    ScrollerPairCoordinated(ScrollingTreeScrollingNode&);

    void updateValues();

    struct Values {
        float value { 0 };
        float proportion { 0 };
        float visibleSize { 0 };

        friend bool operator==(const Values&, const Values&) = default;
    };
    Values valuesForOrientation(ScrollbarOrientation);

    ThreadSafeWeakPtr<ScrollingTreeScrollingNode> m_scrollingNode;
    const UniqueRef<ScrollerCoordinated> m_verticalScroller;
    const UniqueRef<ScrollerCoordinated> m_horizontalScroller;
};

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS_ASYNC_SCROLLBAR)
