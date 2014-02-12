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

    static RenderPtr<RenderElement> createFor(Element&, PassRef<RenderStyle>);

    bool hasInitializedStyle() const { return m_hasInitializedStyle; }

    RenderStyle& style() const { return const_cast<RenderStyle&>(m_style.get()); }
    RenderStyle& firstLineStyle() const;

    void initializeStyle();

    void setStyle(PassRef<RenderStyle>);
    // Called to update a style that is allowed to trigger animations.
    void setAnimatableStyle(PassRef<RenderStyle>);

    // This is null for anonymous renderers.
    Element* element() const { return toElement(RenderObject::node()); }
    Element* nonPseudoElement() const { return toElement(RenderObject::nonPseudoNode()); }
    Element* generatingElement() const;

    RenderObject* firstChild() const { return m_firstChild; }
    RenderObject* lastChild() const { return m_lastChild; }

    // FIXME: Make these standalone and move to relevant files.
    bool isRenderLayerModelObject() const;
    bool isBoxModelObject() const;
    bool isRenderBlock() const;
    bool isRenderBlockFlow() const;
    bool isRenderReplaced() const;
    bool isRenderInline() const;
    bool isRenderNamedFlowFragmentContainer() const;

    virtual bool isChildAllowed(const RenderObject&, const RenderStyle&) const { return true; }
    virtual void addChild(RenderObject* newChild, RenderObject* beforeChild = 0);
    virtual void addChildIgnoringContinuation(RenderObject* newChild, RenderObject* beforeChild = 0) { return addChild(newChild, beforeChild); }
    virtual void removeChild(RenderObject&);

    // The following functions are used when the render tree hierarchy changes to make sure layers get
    // properly added and removed. Since containership can be implemented by any subclass, and since a hierarchy
    // can contain a mixture of boxes and other object types, these functions need to be in the base class.
    void addLayers(RenderLayer* parentLayer);
    void removeLayers(RenderLayer* parentLayer);
    void moveLayers(RenderLayer* oldParent, RenderLayer* newParent);
    RenderLayer* findNextLayer(RenderLayer* parentLayer, RenderObject* startPoint, bool checkParent = true);

    enum NotifyChildrenType { NotifyChildren, DontNotifyChildren };
    void insertChildInternal(RenderObject*, RenderObject* beforeChild, NotifyChildrenType);
    void removeChildInternal(RenderObject&, NotifyChildrenType);

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
    RenderElement& rendererForRootBackground();

    // Used only by Element::pseudoStyleCacheIsInvalid to get a first line style based off of a
    // given new style, without accessing the cache.
    PassRefPtr<RenderStyle> uncachedFirstLineStyle(RenderStyle*) const;

    // Updates only the local style ptr of the object. Does not update the state of the object,
    // and so only should be called when the style is known not to have changed (or from setStyle).
    void setStyleInternal(PassRef<RenderStyle> style) { m_style = std::move(style); }

    // Repaint only if our old bounds and new bounds are different. The caller may pass in newBounds and newOutlineBox if they are known.
    bool repaintAfterLayoutIfNeeded(const RenderLayerModelObject* repaintContainer, const LayoutRect& oldBounds, const LayoutRect& oldOutlineBox, const LayoutRect* newBoundsPtr = nullptr, const LayoutRect* newOutlineBoxPtr = nullptr);

    bool borderImageIsLoadedAndCanBeRendered() const;

    // Returns true if this renderer requires a new stacking context.
    bool createsGroup() const { return isTransparent() || hasMask() || hasFilter() || hasBlendMode(); }

    bool isTransparent() const { return style().opacity() < 1.0f; }
    float opacity() const { return style().opacity(); }

    bool visibleToHitTesting() const { return style().visibility() == VISIBLE && style().pointerEvents() != PE_NONE; }

    bool hasBackground() const { return style().hasBackground(); }
    bool hasMask() const { return style().hasMask(); }
    bool hasClip() const { return isOutOfFlowPositioned() && style().hasClip(); }
    bool hasClipOrOverflowClip() const { return hasClip() || hasOverflowClip(); }
    bool hasClipPath() const { return style().clipPath(); }
    bool hasHiddenBackface() const { return style().backfaceVisibility() == BackfaceVisibilityHidden; }

#if ENABLE(CSS_FILTERS)
    bool hasFilter() const { return style().hasFilter(); }
#else
    bool hasFilter() const { return false; }
#endif

#if ENABLE(CSS_COMPOSITING)
    bool hasBlendMode() const { return style().hasBlendMode(); }
#else
    bool hasBlendMode() const { return false; }
#endif

    RenderNamedFlowThread* renderNamedFlowThreadWrapper();

protected:
    enum BaseTypeFlags {
        RenderLayerModelObjectFlag = 1 << 0,
        RenderBoxModelObjectFlag = 1 << 1,
        RenderInlineFlag = 1 << 2,
        RenderReplacedFlag = 1 << 3,
        RenderBlockFlag = 1 << 4,
        RenderBlockFlowFlag = 1 << 5,
    };

    RenderElement(Element&, PassRef<RenderStyle>, unsigned baseTypeFlags);
    RenderElement(Document&, PassRef<RenderStyle>, unsigned baseTypeFlags);

    bool layerCreationAllowedForSubtree() const;

    enum StylePropagationType { PropagateToAllChildren, PropagateToBlockChildrenOnly };
    void propagateStyleToAnonymousChildren(StylePropagationType);

    LayoutUnit valueForLength(const Length&, LayoutUnit maximumValue, bool roundPercentages = false) const;
    LayoutUnit minimumValueForLength(const Length&, LayoutUnit maximumValue, bool roundPercentages = false) const;

    void setFirstChild(RenderObject* child) { m_firstChild = child; }
    void setLastChild(RenderObject* child) { m_lastChild = child; }
    void destroyLeftoverChildren();

    virtual void styleWillChange(StyleDifference, const RenderStyle& newStyle);
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle);

    virtual void insertedIntoTree() override;
    virtual void willBeRemovedFromTree() override;
    virtual void willBeDestroyed() override;

    void setRenderInlineAlwaysCreatesLineBoxes(bool b) { m_renderInlineAlwaysCreatesLineBoxes = b; }
    bool renderInlineAlwaysCreatesLineBoxes() const { return m_renderInlineAlwaysCreatesLineBoxes; }

private:
    void node() const = delete;
    void nonPseudoNode() const = delete;
    void generatingNode() const = delete;
    void isText() const = delete;
    void isRenderElement() const = delete;

    virtual RenderObject* firstChildSlow() const override final { return firstChild(); }
    virtual RenderObject* lastChildSlow() const override final { return lastChild(); }

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
    bool m_hasInitializedStyle : 1;

    // Specific to RenderInline.
    bool m_renderInlineAlwaysCreatesLineBoxes : 1;

    RenderObject* m_firstChild;
    RenderObject* m_lastChild;

    Ref<RenderStyle> m_style;

    // FIXME: Get rid of this hack.
    // Store state between styleWillChange and styleDidChange
    static bool s_affectsParentBlock;
    static bool s_noLongerAffectsParentBlock;
};

RENDER_OBJECT_TYPE_CASTS(RenderElement, isRenderElement())

inline RenderStyle& RenderElement::firstLineStyle() const
{
    return document().styleSheetCollection().usesFirstLineRules() ? *cachedFirstLineStyle() : style();
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

inline Element* RenderElement::generatingElement() const
{
    if (parent() && isRenderNamedFlowFragment())
        return parent()->generatingElement();
    return toElement(RenderObject::generatingNode());
}

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

inline RenderStyle& RenderObject::style() const
{
    if (isText())
        return m_parent->style();
    return toRenderElement(this)->style();
}

inline RenderStyle& RenderObject::firstLineStyle() const
{
    if (isText())
        return m_parent->firstLineStyle();
    return toRenderElement(this)->firstLineStyle();
}

inline RenderElement* ContainerNode::renderer() const
{
    return toRenderElement(Node::renderer());
}

inline int adjustForAbsoluteZoom(int value, const RenderElement& renderer)
{
    return adjustForAbsoluteZoom(value, renderer.style());
}

#if ENABLE(SUBPIXEL_LAYOUT)
inline LayoutUnit adjustLayoutUnitForAbsoluteZoom(LayoutUnit value, const RenderElement& renderer)
{
    return adjustLayoutUnitForAbsoluteZoom(value, renderer.style());
}
#endif

} // namespace WebCore

#endif // RenderElement_h
