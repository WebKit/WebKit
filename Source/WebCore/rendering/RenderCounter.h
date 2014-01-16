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

#ifndef RenderCounter_h
#define RenderCounter_h

#include "CounterContent.h"
#include "RenderText.h"

namespace WebCore {

class CounterNode;

class RenderCounter FINAL : public RenderText {
public:
    RenderCounter(Document&, const CounterContent&);
    virtual ~RenderCounter();

    static void destroyCounterNodes(RenderObject*);
    static void destroyCounterNode(RenderObject*, const AtomicString& identifier);
    static void rendererSubtreeAttached(RenderObject*);
    static void rendererRemovedFromTree(RenderObject&);
    static void rendererStyleChanged(RenderObject*, const RenderStyle* oldStyle, const RenderStyle* newStyle);

    void updateCounter();

protected:
    virtual void willBeDestroyed();

private:
    virtual const char* renderName() const;
    virtual bool isCounter() const;
    virtual String originalText() const;
    
    virtual void computePreferredLogicalWidths(float leadWidth) override;

    // Removes the reference to the CounterNode associated with this renderer.
    // This is used to cause a counter display update when the CounterNode tree changes.
    void invalidate();

    CounterContent m_counter;
    CounterNode* m_counterNode;
    RenderCounter* m_nextForSameCounter;
    friend class CounterNode;
};

template<> inline bool isRendererOfType<const RenderCounter>(const RenderObject& renderer) { return renderer.isCounter(); }
RENDER_OBJECT_TYPE_CASTS(RenderCounter, isCounter())

} // namespace WebCore

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
void showCounterRendererTree(const WebCore::RenderObject*, const char* counterName = 0);
#endif

#endif // RenderCounter_h
