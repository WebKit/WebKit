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

class RenderCounter : public RenderText {
public:
    RenderCounter(Document*, const CounterContent&);
    virtual ~RenderCounter();

    // Removes the reference to the CounterNode associated with this renderer
    // if its identifier matches the argument.
    // This is used to cause a counter display update when the CounterNode
    // tree for identifier changes.
    void invalidate(const AtomicString& identifier);

    static void destroyCounterNodes(RenderObject*);
    static void destroyCounterNode(RenderObject*, const AtomicString& identifier);
    static void rendererSubtreeAttached(RenderObject*);
    static void rendererStyleChanged(RenderObject*, const RenderStyle* oldStyle, const RenderStyle* newStyle);

private:
    virtual const char* renderName() const;
    virtual bool isCounter() const;
    virtual PassRefPtr<StringImpl> originalText() const;
    
    virtual void computePreferredLogicalWidths(int leadWidth);

    CounterContent m_counter;
    mutable CounterNode* m_counterNode;
};

inline RenderCounter* toRenderCounter(RenderObject* object)
{
    ASSERT(!object || object->isCounter());
    return static_cast<RenderCounter*>(object);
}

// This will catch anyone doing an unnecessary cast.
void toRenderCounter(const RenderCounter*);

} // namespace WebCore

#endif // RenderCounter_h
