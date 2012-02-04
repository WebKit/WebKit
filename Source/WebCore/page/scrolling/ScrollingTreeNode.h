/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef ScrollingTreeNode_h
#define ScrollingTreeNode_h

#if ENABLE(THREADED_SCROLLING)

#include "IntRect.h"
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class PlatformWheelEvent;
class ScrollingTree;
class ScrollingTreeState;

class ScrollingTreeNode {
public:
    static PassOwnPtr<ScrollingTreeNode> create(ScrollingTree*);
    virtual ~ScrollingTreeNode();

    virtual void update(ScrollingTreeState*);
    virtual void handleWheelEvent(const PlatformWheelEvent&) = 0;

protected:
    explicit ScrollingTreeNode(ScrollingTree*);

    ScrollingTree* scrollingTree() const { return m_scrollingTree; }
    const IntRect& viewportRect() const { return m_viewportRect; }
    const IntSize& contentsSize() const { return m_contentsSize; }

private:
    ScrollingTree* m_scrollingTree;

    IntRect m_viewportRect;
    IntSize m_contentsSize;
};

} // namespace WebCore

#endif // ENABLE(THREADED_SCROLLING)

#endif // ScrollingTreeNode_h
