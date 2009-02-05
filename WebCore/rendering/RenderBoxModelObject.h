/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#ifndef RenderBoxModelObject_h
#define RenderBoxModelObject_h

#include "RenderObject.h"

namespace WebCore {

// This class is the base for all objects that adhere to the CSS box model as described
// at http://www.w3.org/TR/CSS21/box.html

class RenderBoxModelObject : public RenderObject {
public:
    RenderBoxModelObject(Node*);
    virtual ~RenderBoxModelObject();
    
    virtual void destroy();

    int relativePositionOffsetX() const;
    int relativePositionOffsetY() const;
    IntSize relativePositionOffset() const { return IntSize(relativePositionOffsetX(), relativePositionOffsetY()); }

    virtual void styleWillChange(StyleDifference, const RenderStyle* newStyle);
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle);
    virtual void updateBoxModelInfoFromStyle();

    RenderLayer* layer() const { return m_layer; }
    virtual bool requiresLayer() const { return isRoot() || isPositioned() || isRelPositioned() || isTransparent() || hasOverflowClip() || hasTransform() || hasMask() || hasReflection(); }

private:
    virtual bool isBoxModelObject() const { return true; }

    friend class RenderView;

    RenderLayer* m_layer;
    
    // Used to store state between styleWillChange and styleDidChange
    static bool s_wasFloating;
};

inline RenderBoxModelObject* toRenderBoxModelObject(RenderObject* o)
{ 
    ASSERT(!o || o->isBoxModelObject());
    return static_cast<RenderBoxModelObject*>(o);
}

inline const RenderBoxModelObject* toRenderBoxModelObject(const RenderObject* o)
{ 
    ASSERT(!o || o->isBoxModelObject());
    return static_cast<const RenderBoxModelObject*>(o);
}

// This will catch anyone doing an unnecessary cast.
void toRenderBoxModelObject(const RenderBox*);

} // namespace WebCore

#endif // RenderBoxModelObject_h
