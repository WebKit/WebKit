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

#pragma once

#if ENABLE(ASYNC_SCROLLING)

#include "ScrollingConstraints.h"
#include "ScrollingTreeNode.h"
#include <wtf/RetainPtr.h>

OBJC_CLASS CALayer;

namespace WebCore {

class ScrollingTreeStickyNode : public ScrollingTreeNode {
public:
    WEBCORE_EXPORT static Ref<ScrollingTreeStickyNode> create(ScrollingTree&, ScrollingNodeID);

    virtual ~ScrollingTreeStickyNode();

    FloatSize scrollDeltaSinceLastCommit() const;

    CALayer *layer() { return m_layer.get(); }

private:
    ScrollingTreeStickyNode(ScrollingTree&, ScrollingNodeID);

    void commitStateBeforeChildren(const ScrollingStateNode&) override;
    void applyLayerPositions() override;

    FloatPoint computeLayerPosition() const;

    void dumpProperties(WTF::TextStream&, ScrollingStateTreeAsTextBehavior) const override;

    StickyPositionViewportConstraints m_constraints;
    RetainPtr<CALayer> m_layer;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_SCROLLING_NODE(ScrollingTreeStickyNode, isStickyNode())

#endif // ENABLE(ASYNC_SCROLLING)
