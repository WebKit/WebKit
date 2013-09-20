/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2006, 2007, 2009, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2010, 2012 Google Inc. All rights reserved.
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

#ifndef RenderElement_h
#define RenderElement_h

#include "RenderObject.h"

namespace WebCore {

class RenderElement : public RenderObject {
public:
    virtual ~RenderElement();

    static RenderElement* createFor(Element&, RenderStyle&);

    // This is null for anonymous renderers.
    Element* element() const { return toElement(RenderObject::node()); }
    Element* nonPseudoElement() const { return toElement(RenderObject::nonPseudoNode()); }
    Element* generatingElement() const { return toElement(RenderObject::generatingNode()); }

    virtual bool isChildAllowed(RenderObject*, RenderStyle*) const { return true; }
    virtual void addChild(RenderObject* newChild, RenderObject* beforeChild = 0);
    virtual void addChildIgnoringContinuation(RenderObject* newChild, RenderObject* beforeChild = 0) { return addChild(newChild, beforeChild); }
    virtual void removeChild(RenderObject*);

    // The following functions are used when the render tree hierarchy changes to make sure layers get
    // properly added and removed. Since containership can be implemented by any subclass, and since a hierarchy
    // can contain a mixture of boxes and other object types, these functions need to be in the base class.
    void addLayers(RenderLayer* parentLayer);
    void removeLayers(RenderLayer* parentLayer);
    void moveLayers(RenderLayer* oldParent, RenderLayer* newParent);
    RenderLayer* findNextLayer(RenderLayer* parentLayer, RenderObject* startPoint, bool checkParent = true);

protected:
    explicit RenderElement(Element*);

    bool layerCreationAllowedForSubtree() const;

    LayoutUnit valueForLength(const Length&, LayoutUnit maximumValue, bool roundPercentages = false) const;
    LayoutUnit minimumValueForLength(const Length&, LayoutUnit maximumValue, bool roundPercentages = false) const;

    virtual void insertedIntoTree() OVERRIDE;
    virtual void willBeRemovedFromTree() OVERRIDE;

private:
    void node() const WTF_DELETED_FUNCTION;
    void nonPseudoNode() const WTF_DELETED_FUNCTION;
    void generatingNode() const WTF_DELETED_FUNCTION;
    void isText() const WTF_DELETED_FUNCTION;
};

inline LayoutUnit RenderElement::valueForLength(const Length& length, LayoutUnit maximumValue, bool roundPercentages) const
{
    return WebCore::valueForLength(length, maximumValue, &view(), roundPercentages);
}

inline LayoutUnit RenderElement::minimumValueForLength(const Length& length, LayoutUnit maximumValue, bool roundPercentages) const
{
    return WebCore::minimumValueForLength(length, maximumValue, &view(), roundPercentages);
}

inline RenderElement& toRenderElement(RenderObject& object)
{
    ASSERT_WITH_SECURITY_IMPLICATION(object.isRenderElement());
    return static_cast<RenderElement&>(object);
}

inline const RenderElement& toRenderElement(const RenderObject& object)
{
    ASSERT_WITH_SECURITY_IMPLICATION(object.isRenderElement());
    return static_cast<const RenderElement&>(object);
}

inline RenderElement* toRenderElement(RenderObject* object)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!object || object->isRenderElement());
    return static_cast<RenderElement*>(object);
}

inline const RenderElement* toRenderElement(const RenderObject* object)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!object || object->isRenderElement());
    return static_cast<const RenderElement*>(object);
}

// This will catch anyone doing an unnecessary cast.
void toRenderElement(const RenderElement*);
void toRenderElement(const RenderElement&);

inline RenderElement* Element::renderer() const
{
    return toRenderElement(Node::renderer());
}

} // namespace WebCore

#endif // RenderElement_h
