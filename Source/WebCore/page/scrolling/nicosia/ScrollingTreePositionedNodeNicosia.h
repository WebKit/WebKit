/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2019 Igalia S.L.
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

#if ENABLE(ASYNC_SCROLLING) && USE(NICOSIA)

#include "NicosiaCompositionLayer.h"
#include "ScrollingTreePositionedNode.h"

namespace WebCore {

class ScrollingTreePositionedNodeNicosia final : public ScrollingTreePositionedNode {
public:
    static Ref<ScrollingTreePositionedNodeNicosia> create(ScrollingTree&, ScrollingNodeID);
    virtual ~ScrollingTreePositionedNodeNicosia();

    Nicosia::CompositionLayer* layer() const override { return m_layer.get(); }

private:
    ScrollingTreePositionedNodeNicosia(ScrollingTree&, ScrollingNodeID);

    bool commitStateBeforeChildren(const ScrollingStateNode&) override;
    void applyLayerPositions() override;

    RefPtr<Nicosia::CompositionLayer> m_layer;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_SCROLLING_NODE(ScrollingTreePositionedNodeNicosia, isPositionedNodeNicosia())

#endif // ENABLE(ASYNC_SCROLLING) && USE(NICOSIA)
