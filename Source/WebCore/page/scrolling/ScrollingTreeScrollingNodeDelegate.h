/*
 * Copyright (C) 2017 Igalia S.L. All rights reserved.
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

#if ENABLE(ASYNC_SCROLLING)

#include "ScrollingTreeScrollingNode.h"

namespace WebCore {

class ScrollingTreeScrollingNodeDelegate {
public:
    WEBCORE_EXPORT explicit ScrollingTreeScrollingNodeDelegate(ScrollingTreeScrollingNode&);
    WEBCORE_EXPORT virtual ~ScrollingTreeScrollingNodeDelegate();

    ScrollingTreeScrollingNode& scrollingNode() { return m_scrollingNode; }
    const ScrollingTreeScrollingNode& scrollingNode() const { return m_scrollingNode; }
    
    virtual bool startAnimatedScrollToPosition(FloatPoint) = 0;
    virtual void stopAnimatedScroll() = 0;

    virtual void serviceScrollAnimation(MonotonicTime) = 0;

protected:
    WEBCORE_EXPORT ScrollingTree& scrollingTree() const;
    WEBCORE_EXPORT FloatPoint lastCommittedScrollPosition() const;
    WEBCORE_EXPORT const FloatSize& totalContentsSize();
    WEBCORE_EXPORT const FloatSize& reachableContentsSize();
    WEBCORE_EXPORT const IntPoint& scrollOrigin() const;

    FloatPoint currentScrollPosition() const { return m_scrollingNode.currentScrollPosition(); }
    FloatPoint minimumScrollPosition() const { return m_scrollingNode.minimumScrollPosition(); }
    FloatPoint maximumScrollPosition() const { return m_scrollingNode.maximumScrollPosition(); }

    FloatSize scrollableAreaSize() const { return m_scrollingNode.scrollableAreaSize(); }
    FloatSize totalContentsSize() const { return m_scrollingNode.totalContentsSize(); }

    bool allowsHorizontalScrolling() const { return m_scrollingNode.allowsHorizontalScrolling(); }
    bool allowsVerticalScrolling() const { return m_scrollingNode.allowsVerticalScrolling(); }

    ScrollElasticity horizontalScrollElasticity() const { return m_scrollingNode.horizontalScrollElasticity(); }
    ScrollElasticity verticalScrollElasticity() const { return m_scrollingNode.verticalScrollElasticity(); }

private:
    ScrollingTreeScrollingNode& m_scrollingNode;
};

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
