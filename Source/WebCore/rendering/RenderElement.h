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

    RenderStyle* style() const { return m_style.get(); }
    RenderStyle* firstLineStyle() const;

    virtual void setStyle(PassRefPtr<RenderStyle>);
    // Called to update a style that is allowed to trigger animations.
    void setAnimatableStyle(PassRefPtr<RenderStyle>);

    // This is null for anonymous renderers.
    Element* element() const { return toElement(RenderObject::node()); }
    Element* nonPseudoElement() const { return toElement(RenderObject::nonPseudoNode()); }
    Element* generatingElement() const { return toElement(RenderObject::generatingNode()); }

    RenderObject* firstChild() const { return m_firstChild; }
    RenderObject* lastChild() const { return m_lastChild; }

    // FIXME: Make these standalone and move to relevant files.
    bool isRenderLayerModelObject() const;
    bool isBoxModelObject() const;
    bool isRenderBlock() const;
    bool isRenderBlockFlow() const;
    bool isRenderReplaced() const;
    bool isRenderInline() const;

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

    enum NotifyChildrenType { NotifyChildren, DontNotifyChildren };
    void insertChildInternal(RenderObject*, RenderObject* beforeChild, NotifyChildrenType);
    void removeChildInternal(RenderObject*, NotifyChildrenType);

    virtual RenderElement* hoverAncestor() const;

    virtual void dirtyLinesFromChangedChild(RenderObject*) { }

    bool ancestorLineBoxDirty() const { return m_ancestorLineBoxDirty; }
    void setAncestorLineBoxDirty(bool f = true);

    void setChildNeedsLayout(MarkingBehavior = MarkContainingBlockChain);
    void clearChildNeedsLayout();
    void setNeedsPositionedMovementLayout(const RenderStyle* oldStyle);
    void setNeedsSimplifiedNormalFlowLayout();

    virtual void paint(PaintInfo&, const LayoutPoint&) = 0;

    // Recursive function that computes the size and position of this object and all its descendants.
    virtual void layout();

    /* This function performs a layout only if one is needed. */
    void layoutIfNeeded() { if (needsLayout()) layout(); }

    // Return the renderer whose background style is used to paint the root background. Should only be called on the renderer for which isRoot() is true.
    RenderElement* rendererForRootBackground();

    // Used only by Element::pseudoStyleCacheIsInvalid to get a first line style based off of a
    // given new style, without accessing the cache.
    PassRefPtr<RenderStyle> uncachedFirstLineStyle(RenderStyle*) const;

    // Updates only the local style ptr of the object. Does not update the state of the object,
    // and so only should be called when the style is known not to have changed (or from setStyle).
    void setStyleInternal(PassRefPtr<RenderStyle> style) { m_style = style; }

protected:
    enum BaseTypeFlags {
        RenderLayerModelObjectFlag = 1 << 0,
        RenderBoxModelObjectFlag = 1 << 1,
        RenderInlineFlag = 1 << 2,
        RenderReplacedFlag = 1 << 3,
        RenderBlockFlag = 1 << 4,
        RenderBlockFlowFlag = 1 << 5,
    };

    RenderElement(Element&, unsigned baseTypeFlags);
    RenderElement(Document&, unsigned baseTypeFlags);

    bool layerCreationAllowedForSubtree() const;

    enum StylePropagationType { PropagateToAllChildren, PropagateToBlockChildrenOnly };
    void propagateStyleToAnonymousChildren(StylePropagationType);

    LayoutUnit valueForLength(const Length&, LayoutUnit maximumValue, bool roundPercentages = false) const;
    LayoutUnit minimumValueForLength(const Length&, LayoutUnit maximumValue, bool roundPercentages = false) const;

    void setFirstChild(RenderObject* child) { m_firstChild = child; }
    void setLastChild(RenderObject* child) { m_lastChild = child; }
    void destroyLeftoverChildren();

    virtual void styleWillChange(StyleDifference, const RenderStyle*);
    virtual void styleDidChange(StyleDifference, const RenderStyle*);

    virtual void insertedIntoTree() OVERRIDE;
    virtual void willBeRemovedFromTree() OVERRIDE;
    virtual void willBeDestroyed() OVERRIDE;

private:
    void node() const WTF_DELETED_FUNCTION;
    void nonPseudoNode() const WTF_DELETED_FUNCTION;
    void generatingNode() const WTF_DELETED_FUNCTION;
    void isText() const WTF_DELETED_FUNCTION;
    void isRenderElement() const WTF_DELETED_FUNCTION;

    virtual RenderObject* firstChildSlow() const OVERRIDE FINAL { return firstChild(); }
    virtual RenderObject* lastChildSlow() const OVERRIDE FINAL { return lastChild(); }

    bool shouldRepaintForStyleDifference(StyleDifference) const;
    bool hasImmediateNonWhitespaceTextChildOrBorderOrOutline() const;

    void updateFillImages(const FillLayer*, const FillLayer*);
    void updateImage(StyleImage*, StyleImage*);
#if ENABLE(CSS_SHAPES)
    void updateShapeImage(const ShapeValue*, const ShapeValue*);
#endif

    StyleDifference adjustStyleDifference(StyleDifference, unsigned contextSensitiveProperties) const;
    RenderStyle* cachedFirstLineStyle() const;

    unsigned m_baseTypeFlags : 6;
    bool m_ancestorLineBoxDirty : 1;

    RenderObject* m_firstChild;
    RenderObject* m_lastChild;

    RefPtr<RenderStyle> m_style;

    // FIXME: Get rid of this hack.
    // Store state between styleWillChange and styleDidChange
    static bool s_affectsParentBlock;
    static bool s_noLongerAffectsParentBlock;
};

inline RenderStyle* RenderElement::firstLineStyle() const
{
    return document().styleSheetCollection().usesFirstLineRules() ? cachedFirstLineStyle() : style();
}

inline void RenderElement::setAncestorLineBoxDirty(bool f)
{
    m_ancestorLineBoxDirty = f;
    if (m_ancestorLineBoxDirty)
        setNeedsLayout();
}

inline void RenderElement::setChildNeedsLayout(MarkingBehavior markParents)
{
    ASSERT(!isSetNeedsLayoutForbidden());
    if (normalChildNeedsLayout())
        return;
    setNormalChildNeedsLayoutBit(true);
    if (markParents == MarkContainingBlockChain)
        markContainingBlocksForLayout();
}

inline LayoutUnit RenderElement::valueForLength(const Length& length, LayoutUnit maximumValue, bool roundPercentages) const
{
    return WebCore::valueForLength(length, maximumValue, &view(), roundPercentages);
}

inline LayoutUnit RenderElement::minimumValueForLength(const Length& length, LayoutUnit maximumValue, bool roundPercentages) const
{
    return WebCore::minimumValueForLength(length, maximumValue, &view(), roundPercentages);
}

inline bool RenderElement::isRenderLayerModelObject() const
{
    return m_baseTypeFlags & RenderLayerModelObjectFlag;
}

inline bool RenderElement::isBoxModelObject() const
{
    return m_baseTypeFlags & RenderBoxModelObjectFlag;
}

inline bool RenderElement::isRenderBlock() const
{
    return m_baseTypeFlags & RenderBlockFlag;
}

inline bool RenderElement::isRenderBlockFlow() const
{
    return m_baseTypeFlags & RenderBlockFlowFlag;
}

inline bool RenderElement::isRenderReplaced() const
{
    return m_baseTypeFlags & RenderReplacedFlag;
}

inline bool RenderElement::isRenderInline() const
{
    return m_baseTypeFlags & RenderInlineFlag;
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

inline bool RenderObject::isRenderLayerModelObject() const
{
    return isRenderElement() && toRenderElement(this)->isRenderLayerModelObject();
}

inline bool RenderObject::isBoxModelObject() const
{
    return isRenderElement() && toRenderElement(this)->isBoxModelObject();
}

inline bool RenderObject::isRenderBlock() const
{
    return isRenderElement() && toRenderElement(this)->isRenderBlock();
}

inline bool RenderObject::isRenderBlockFlow() const
{
    return isRenderElement() && toRenderElement(this)->isRenderBlockFlow();
}

inline bool RenderObject::isRenderReplaced() const
{
    return isRenderElement() && toRenderElement(this)->isRenderReplaced();
}

inline bool RenderObject::isRenderInline() const
{
    return isRenderElement() && toRenderElement(this)->isRenderInline();
}

inline RenderStyle* RenderObject::style() const
{
    if (isText())
        return m_parent->style();
    return toRenderElement(this)->style();
}

inline RenderStyle* RenderObject::firstLineStyle() const
{
    if (isText())
        return m_parent->firstLineStyle();
    return toRenderElement(this)->firstLineStyle();
}

inline RenderElement* ContainerNode::renderer() const
{
    return toRenderElement(Node::renderer());
}

} // namespace WebCore

#endif // RenderElement_h
