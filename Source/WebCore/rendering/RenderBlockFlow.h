/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2003-2013,  Apple Inc. All rights reserved.
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
 */

#ifndef RenderBlockFlow_h
#define RenderBlockFlow_h

#include "RenderBlock.h"

namespace WebCore {

class RenderBlockFlow : public RenderBlock {
public:
    explicit RenderBlockFlow(Element*);
    virtual ~RenderBlockFlow();
    
    virtual bool isRenderBlockFlow() const OVERRIDE FINAL { return true; }
    
    virtual void layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight = 0) OVERRIDE;
    
protected:
    // This method is called at the start of layout to wipe away all of the floats in our floating objects list. It also
    // repopulates the list with any floats that intrude from previous siblings or parents. Floats that were added by
    // descendants are gone when this call completes and will get added back later on after the children have gotten
    // a relayout.
    void clearFloats();

    // RenderBlockFlow always contains either lines or paragraphs. When the children are all blocks (e.g. paragraphs), we call layoutBlockChildren.
    // When the children are are all inline (e.g., lines), we call layoutInlineChildren.
    void layoutBlockChildren(bool relayoutChildren, LayoutUnit& maxFloatLogicalBottom);
    void layoutInlineChildren(bool relayoutChildren, LayoutUnit& repaintLogicalTop, LayoutUnit& repaintLogicalBottom);
};

inline RenderBlockFlow& toRenderBlockFlow(RenderObject& object)
{
    ASSERT_WITH_SECURITY_IMPLICATION(object.isRenderBlockFlow());
    return static_cast<RenderBlockFlow&>(object);
}

inline const RenderBlockFlow& toRenderBlockFlow(const RenderObject& object)
{
    ASSERT_WITH_SECURITY_IMPLICATION(object.isRenderBlockFlow());
    return static_cast<const RenderBlockFlow&>(object);
}

inline RenderBlockFlow* toRenderBlockFlow(RenderObject* object)
{ 
    ASSERT_WITH_SECURITY_IMPLICATION(!object || object->isRenderBlockFlow());
    return static_cast<RenderBlockFlow*>(object);
}

inline const RenderBlockFlow* toRenderBlockFlow(const RenderObject* object)
{ 
    ASSERT_WITH_SECURITY_IMPLICATION(!object || object->isRenderBlockFlow());
    return static_cast<const RenderBlockFlow*>(object);
}

// This will catch anyone doing an unnecessary cast.
void toRenderBlockFlow(const RenderBlockFlow*);
void toRenderBlockFlow(const RenderBlockFlow&);

} // namespace WebCore

#endif // RenderBlockFlow_h
