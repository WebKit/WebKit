/*
 * Copyright (C) 2004 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include "CounterContent.h"
#include "RenderText.h"

namespace WebCore {

class CounterNode;

class RenderCounter final : public RenderText {
    WTF_MAKE_ISO_ALLOCATED(RenderCounter);
public:
    RenderCounter(Document&, const CounterContent&);
    virtual ~RenderCounter();

    static void destroyCounterNodes(RenderElement&);
    static void destroyCounterNode(RenderElement&, const AtomString& identifier);
    static void rendererSubtreeAttached(RenderElement&);
    static void rendererRemovedFromTree(RenderElement&);
    static void rendererStyleChanged(RenderElement&, const RenderStyle* oldStyle, const RenderStyle* newStyle);

    void updateCounter();

private:
    void willBeDestroyed() override;
    
    const char* renderName() const override;
    bool isCounter() const override;
    String originalText() const override;
    
    void computePreferredLogicalWidths(float leadWidth) override;

    CounterContent m_counter;
    CounterNode* m_counterNode { nullptr };
    RenderCounter* m_nextForSameCounter { nullptr };
    friend class CounterNode;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderCounter, isCounter())

#if ENABLE(TREE_DEBUGGING)
// Outside the WebCore namespace for ease of invocation from the debugger.
void showCounterRendererTree(const WebCore::RenderObject*, const char* counterName = nullptr);
#endif
