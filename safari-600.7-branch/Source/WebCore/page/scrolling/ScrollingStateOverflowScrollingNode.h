/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef ScrollingStateOverflowScrollingNode_h
#define ScrollingStateOverflowScrollingNode_h

#if ENABLE(ASYNC_SCROLLING) || USE(COORDINATED_GRAPHICS)

#include "ScrollingStateScrollingNode.h"

namespace WebCore {

class ScrollingStateOverflowScrollingNode : public ScrollingStateScrollingNode {
public:
    static PassRefPtr<ScrollingStateOverflowScrollingNode> create(ScrollingStateTree&, ScrollingNodeID);

    virtual PassRefPtr<ScrollingStateNode> clone(ScrollingStateTree&);

    virtual ~ScrollingStateOverflowScrollingNode();

    enum ChangedProperty {
        ScrolledContentsLayer = NumScrollingStateNodeBits
    };

    // This is a layer with the contents that move.
    const LayerRepresentation& scrolledContentsLayer() const { return m_scrolledContentsLayer; }
    void setScrolledContentsLayer(const LayerRepresentation&);
    
    virtual void dumpProperties(TextStream&, int indent) const override;

private:
    ScrollingStateOverflowScrollingNode(ScrollingStateTree&, ScrollingNodeID);
    ScrollingStateOverflowScrollingNode(const ScrollingStateOverflowScrollingNode&, ScrollingStateTree&);
    
    LayerRepresentation m_scrolledContentsLayer;    
};

SCROLLING_STATE_NODE_TYPE_CASTS(ScrollingStateOverflowScrollingNode, isOverflowScrollingNode());

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) || USE(COORDINATED_GRAPHICS)

#endif // ScrollingStateScrollingNode_h
