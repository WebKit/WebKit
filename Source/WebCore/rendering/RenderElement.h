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

#include "AnimationController.h"
#include "RenderObject.h"
#include "StyleInheritedData.h"

namespace WebCore {

class ControlStates;
class RenderBlock;

class RenderElement : public RenderObject {
public:
    virtual ~RenderElement();

    static RenderPtr<RenderElement> createFor(Element&, Ref<RenderStyle>&&);

    bool hasInitializedStyle() const { return m_hasInitializedStyle; }

    RenderStyle& style() const { return const_cast<RenderStyle&>(m_style.get()); }
    RenderStyle& firstLineStyle() const;

    void initializeStyle();

    // Calling with minimalStyleDifference > StyleDifferenceEqual indicates that
    // out-of-band state (e.g. animations) requires that styleDidChange processing
    // continue even if the style isn't different from the current style.
    void setStyle(Ref<RenderStyle>&&, StyleDifference minimalStyleDifference = StyleDifferenceEqual);

    // Called to update a style that is allowed to trigger animations.
    void setAnimatableStyle(Ref<RenderStyle>&&, StyleDifference minimalStyleDifference);

    // The pseudo element style can be cached or uncached.  Use the cached method if the pseudo element doesn't respect
    // any pseudo classes (and therefore has no concept of changing state).
    RenderStyle* getCachedPseudoStyle(PseudoId, RenderStyle* parentStyle = nullptr) const;
    PassRefPtr<RenderStyle> getUncachedPseudoStyle(const PseudoStyleRequest&, RenderStyle* parentStyle = nullptr, RenderStyle* ownStyle = nullptr) const;

    // This is null for anonymous renderers.
    Element* element() const { return downcast<Element>(RenderObject::node()); }
    Element* nonPseudoElement() const { return downcast<Element>(RenderObject::nonPseudoNode()); }
    Element* generatingElement() const;

    RenderObject* firstChild() const { return m_firstChild; }
    RenderObject* lastChild() const { return m_lastChild; }

    bool isEmpty() const override { return !firstChild(); }

    bool canContainFixedPositionObjects() const;
    bool canContainAbsolutelyPositionedObjects() const;

    Color selectionColor(int colorProperty) const;
    PassRefPtr<RenderStyle> selectionPseudoStyle() const;

    // Obtains the selection colors that should be used when painting a selection.
    Color selectionBackgroundColor() const;
    Color selectionForegroundColor() const;
    Color selectionEmphasisMarkColor() const;

    // FIXME: Make these standalone and move to relevant files.
    bool isRenderLayerModelObject() const;
    bool isBoxModelObject() const;
    bool isRenderBlock() const;
    bool isRenderBlockFlow() const;
    bool isRenderReplaced() const;
    bool isRenderInline() const;
    bool isRenderNamedFlowFragmentContainer() const;

    virtual bool isChildAllowed(const RenderObject&, const RenderStyle&) const { return true; }
    virtual void addChild(RenderObject* newChild, RenderObject* beforeChild = nullptr);
    virtual void addChildIgnoringContinuation(RenderObject* newChild, RenderObject* beforeChild = nullptr) { return addChild(newChild, beforeChild); }
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

    virtual void dirtyLinesFromChangedChild(RenderObject&) { }

    bool ancestorLineBoxDirty() const { return m_ancestorLineBoxDirty; }
    void setAncestorLineBoxDirty(bool f = true);

    void setChildNeedsLayout(MarkingBehavior = MarkContainingBlockChain);
    void clearChildNeedsLayout();
    void setNeedsPositionedMovementLayout(const RenderStyle* oldStyle);
    void setNeedsSimplifiedNormalFlowLayout();

    virtual void paint(PaintInfo&, const LayoutPoint&) = 0;

    // inline-block elements paint all phases atomically. This function ensures that. Certain other elements
    // (grid items, flex items) require this behavior as well, and this function exists as a helper for them.
    // It is expected that the caller will call this function independent of the value of paintInfo.phase.
    void paintAsInlineBlock(PaintInfo&, const LayoutPoint&);

    // Recursive function that computes the size and position of this object and all its descendants.
    virtual void layout();

    /* This function performs a layout only if one is needed. */
    void layoutIfNeeded() { if (needsLayout()) layout(); }

    // Return the renderer whose background style is used to paint the root background. Should only be called on the renderer for which isDocumentElementRenderer() is true.
    RenderElement& rendererForRootBackground();

    // Used only by Element::pseudoStyleCacheIsInvalid to get a first line style based off of a
    // given new style, without accessing the cache.
    PassRefPtr<RenderStyle> uncachedFirstLineStyle(RenderStyle*) const;

    // Updates only the local style ptr of the object. Does not update the state of the object,
    // and so only should be called when the style is known not to have changed (or from setStyle).
    void setStyleInternal(Ref<RenderStyle>&& style) { m_style = WTFMove(style); }

    // Repaint only if our old bounds and new bounds are different. The caller may pass in newBounds and newOutlineBox if they are known.
    bool repaintAfterLayoutIfNeeded(const RenderLayerModelObject* repaintContainer, const LayoutRect& oldBounds, const LayoutRect& oldOutlineBox, const LayoutRect* newBoundsPtr = nullptr, const LayoutRect* newOutlineBoxPtr = nullptr);

    bool borderImageIsLoadedAndCanBeRendered() const;
    bool mayCauseRepaintInsideViewport(const IntRect* visibleRect = nullptr) const;

    // Returns true if this renderer requires a new stacking context.
    bool createsGroup() const { return isTransparent() || hasMask() || hasClipPath() || hasFilter() || hasBackdropFilter() || hasBlendMode(); }

    bool isTransparent() const { return style().opacity() < 1.0f; }
    float opacity() const { return style().opacity(); }

    bool visibleToHitTesting() const { return style().visibility() == VISIBLE && style().pointerEvents() != PE_NONE; }

    bool hasBackground() const { return style().hasBackground(); }
    bool hasMask() const { return style().hasMask(); }
    bool hasClip() const { return isOutOfFlowPositioned() && style().hasClip(); }
    bool hasClipOrOverflowClip() const { return hasClip() || hasOverflowClip(); }
    bool hasClipPath() const { return style().clipPath(); }
    bool hasHiddenBackface() const { return style().backfaceVisibility() == BackfaceVisibilityHidden; }

    // anchorRect() is conceptually similar to absoluteBoundingBoxRect(), but is intended for scrolling to an anchor.
    // For inline renderers, this gets the logical top left of the first leaf child and the logical bottom right of the
    // last leaf child, converts them to absolute coordinates, and makes a box out of them.
    LayoutRect anchorRect() const;

    bool hasFilter() const { return style().hasFilter(); }
    bool hasBackdropFilter() const
    {
#if ENABLE(FILTERS_LEVEL_2)
        return style().hasBackdropFilter();
#else
        return false;
#endif
    }

#if ENABLE(CSS_COMPOSITING)
    bool hasBlendMode() const { return style().hasBlendMode(); }
#else
    bool hasBlendMode() const { return false; }
#endif

#if ENABLE(CSS_SHAPES)
    bool hasShapeOutside() const { return style().shapeOutside(); }
#else
    bool hasShapeOutside() const { return false; }
#endif

    void registerForVisibleInViewportCallback();
    void unregisterForVisibleInViewportCallback();
    void visibleInViewportStateChanged(VisibleInViewportState);

    bool repaintForPausedImageAnimationsIfNeeded(const IntRect& visibleRect);
    bool hasPausedImageAnimations() const { return m_hasPausedImageAnimations; }
    void setHasPausedImageAnimations(bool b) { m_hasPausedImageAnimations = b; }

    RenderNamedFlowThread* renderNamedFlowThreadWrapper();

    void setRenderBoxNeedsLazyRepaint(bool b) { m_renderBoxNeedsLazyRepaint = b; }
    bool renderBoxNeedsLazyRepaint() const { return m_renderBoxNeedsLazyRepaint; }

    bool hasCounterNodeMap() const { return m_hasCounterNodeMap; }
    void setHasCounterNodeMap(bool f) { m_hasCounterNodeMap = f; }

    bool isCSSAnimating() const { return m_isCSSAnimating; }
    void setIsCSSAnimating(bool b) { m_isCSSAnimating = b; }
    
    const RenderElement* enclosingRendererWithTextDecoration(TextDecoration, bool firstLine) const;
    void drawLineForBoxSide(GraphicsContext&, const FloatRect&, BoxSide, Color, EBorderStyle, float adjacentWidth1, float adjacentWidth2, bool antialias = false) const;

    bool childRequiresTable(const RenderObject& child) const;
    bool hasContinuation() const { return m_hasContinuation; }

protected:
    enum BaseTypeFlag {
        RenderLayerModelObjectFlag  = 1 << 0,
        RenderBoxModelObjectFlag    = 1 << 1,
        RenderInlineFlag            = 1 << 2,
        RenderReplacedFlag          = 1 << 3,
        RenderBlockFlag             = 1 << 4,
        RenderBlockFlowFlag         = 1 << 5,
    };
    
    typedef unsigned BaseTypeFlags;

    RenderElement(Element&, Ref<RenderStyle>&&, BaseTypeFlags);
    RenderElement(Document&, Ref<RenderStyle>&&, BaseTypeFlags);

    bool layerCreationAllowedForSubtree() const;

    enum StylePropagationType { PropagateToAllChildren, PropagateToBlockChildrenOnly };
    void propagateStyleToAnonymousChildren(StylePropagationType);

    LayoutUnit valueForLength(const Length&, LayoutUnit maximumValue) const;
    LayoutUnit minimumValueForLength(const Length&, LayoutUnit maximumValue, bool roundPercentages = false) const;

    void setFirstChild(RenderObject* child) { m_firstChild = child; }
    void setLastChild(RenderObject* child) { m_lastChild = child; }
    void destroyLeftoverChildren();

    virtual void styleWillChange(StyleDifference, const RenderStyle& newStyle);
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle);

    void insertedIntoTree() override;
    void willBeRemovedFromTree() override;
    void willBeDestroyed() override;

    void setRenderInlineAlwaysCreatesLineBoxes(bool b) { m_renderInlineAlwaysCreatesLineBoxes = b; }
    bool renderInlineAlwaysCreatesLineBoxes() const { return m_renderInlineAlwaysCreatesLineBoxes; }

    void setHasContinuation(bool b) { m_hasContinuation = b; }

    static bool hasControlStatesForRenderer(const RenderObject*);
    static ControlStates* controlStatesForRenderer(const RenderObject*);
    static void removeControlStatesForRenderer(const RenderObject*);
    static void addControlStatesForRenderer(const RenderObject*, ControlStates*);

    void setRenderBlockHasMarginBeforeQuirk(bool b) { m_renderBlockHasMarginBeforeQuirk = b; }
    void setRenderBlockHasMarginAfterQuirk(bool b) { m_renderBlockHasMarginAfterQuirk = b; }
    void setRenderBlockShouldForceRelayoutChildren(bool b) { m_renderBlockShouldForceRelayoutChildren = b; }
    bool renderBlockHasMarginBeforeQuirk() const { return m_renderBlockHasMarginBeforeQuirk; }
    bool renderBlockHasMarginAfterQuirk() const { return m_renderBlockHasMarginAfterQuirk; }
    bool renderBlockShouldForceRelayoutChildren() const { return m_renderBlockShouldForceRelayoutChildren; }

    void setRenderBlockFlowLineLayoutPath(unsigned u) { m_renderBlockFlowLineLayoutPath = u; }
    void setRenderBlockFlowHasMarkupTruncation(bool b) { m_renderBlockFlowHasMarkupTruncation = b; }
    unsigned renderBlockFlowLineLayoutPath() const { return m_renderBlockFlowLineLayoutPath; }
    bool renderBlockFlowHasMarkupTruncation() const { return m_renderBlockFlowHasMarkupTruncation; }

    void paintFocusRing(PaintInfo&, const LayoutPoint&, const RenderStyle&);
    void paintOutline(PaintInfo&, const LayoutRect&);
    void updateOutlineAutoAncestor(bool hasOutlineAuto) const;

private:
    RenderElement(ContainerNode&, Ref<RenderStyle>&&, BaseTypeFlags);
    void node() const = delete;
    void nonPseudoNode() const = delete;
    void generatingNode() const = delete;
    void isText() const = delete;
    void isRenderElement() const = delete;

    RenderObject* firstChildSlow() const final { return firstChild(); }
    RenderObject* lastChildSlow() const final { return lastChild(); }

    // Called when an object that was floating or positioned becomes a normal flow object
    // again.  We have to make sure the render tree updates as needed to accommodate the new
    // normal flow object.
    void handleDynamicFloatPositionChange();
    void removeAnonymousWrappersForInlinesIfNecessary();

    bool shouldRepaintForStyleDifference(StyleDifference) const;
    bool hasImmediateNonWhitespaceTextChildOrBorderOrOutline() const;

    void updateFillImages(const FillLayer*, const FillLayer*);
    void updateImage(StyleImage*, StyleImage*);
#if ENABLE(CSS_SHAPES)
    void updateShapeImage(const ShapeValue*, const ShapeValue*);
#endif

    StyleDifference adjustStyleDifference(StyleDifference, unsigned contextSensitiveProperties) const;
    RenderStyle* cachedFirstLineStyle() const;

    void newImageAnimationFrameAvailable(CachedImage&) final;

    bool getLeadingCorner(FloatPoint& output) const;
    bool getTrailingCorner(FloatPoint& output) const;

    void clearLayoutRootIfNeeded() const;
    
    bool shouldWillChangeCreateStackingContext() const;
    void issueRepaintForOutlineAuto(float outlineSize);

    unsigned m_baseTypeFlags : 6;
    unsigned m_ancestorLineBoxDirty : 1;
    unsigned m_hasInitializedStyle : 1;

    unsigned m_renderInlineAlwaysCreatesLineBoxes : 1;
    unsigned m_renderBoxNeedsLazyRepaint : 1;
    unsigned m_hasPausedImageAnimations : 1;
    unsigned m_hasCounterNodeMap : 1;
    unsigned m_isCSSAnimating : 1;
    unsigned m_hasContinuation : 1;

    unsigned m_renderBlockHasMarginBeforeQuirk : 1;
    unsigned m_renderBlockHasMarginAfterQuirk : 1;
    unsigned m_renderBlockShouldForceRelayoutChildren : 1;
    unsigned m_renderBlockFlowHasMarkupTruncation : 1;
    unsigned m_renderBlockFlowLineLayoutPath : 2;

    VisibleInViewportState m_visibleInViewportState { VisibilityUnknown };

    RenderObject* m_firstChild;
    RenderObject* m_lastChild;

    Ref<RenderStyle> m_style;

    // FIXME: Get rid of this hack.
    // Store state between styleWillChange and styleDidChange
    static bool s_affectsParentBlock;
    static bool s_noLongerAffectsParentBlock;
};

inline void RenderElement::setAnimatableStyle(Ref<RenderStyle>&& style, StyleDifference minimalStyleDifference)
{
    Ref<RenderStyle> animatedStyle = WTFMove(style);
    if (animation().updateAnimations(*this, animatedStyle, animatedStyle))
        minimalStyleDifference = std::max(minimalStyleDifference, StyleDifferenceRecompositeLayer);
    
    setStyle(WTFMove(animatedStyle), minimalStyleDifference);
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

inline LayoutUnit RenderElement::valueForLength(const Length& length, LayoutUnit maximumValue) const
{
    return WebCore::valueForLength(length, maximumValue);
}

inline LayoutUnit RenderElement::minimumValueForLength(const Length& length, LayoutUnit maximumValue, bool roundPercentages) const
{
    return WebCore::minimumValueForLength(length, maximumValue, roundPercentages);
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
    return downcast<Element>(RenderObject::generatingNode());
}

inline bool RenderElement::canContainFixedPositionObjects() const
{
    return isRenderView()
        || (hasTransform() && isRenderBlock())
        || isSVGForeignObject()
        || isOutOfFlowRenderFlowThread();
}

inline bool RenderElement::canContainAbsolutelyPositionedObjects() const
{
    return style().position() != StaticPosition
        || (isRenderBlock() && hasTransformRelatedProperty())
        || isSVGForeignObject()
        || isRenderView();
}

inline bool RenderObject::isRenderLayerModelObject() const
{
    return is<RenderElement>(*this) && downcast<RenderElement>(*this).isRenderLayerModelObject();
}

inline bool RenderObject::isBoxModelObject() const
{
    return is<RenderElement>(*this) && downcast<RenderElement>(*this).isBoxModelObject();
}

inline bool RenderObject::isRenderBlock() const
{
    return is<RenderElement>(*this) && downcast<RenderElement>(*this).isRenderBlock();
}

inline bool RenderObject::isRenderBlockFlow() const
{
    return is<RenderElement>(*this) && downcast<RenderElement>(*this).isRenderBlockFlow();
}

inline bool RenderObject::isRenderReplaced() const
{
    return is<RenderElement>(*this) && downcast<RenderElement>(*this).isRenderReplaced();
}

inline bool RenderObject::isRenderInline() const
{
    return is<RenderElement>(*this) && downcast<RenderElement>(*this).isRenderInline();
}

inline RenderStyle& RenderObject::style() const
{
    if (isText())
        return m_parent->style();
    return downcast<RenderElement>(*this).style();
}

inline RenderStyle& RenderObject::firstLineStyle() const
{
    if (isText())
        return m_parent->firstLineStyle();
    return downcast<RenderElement>(*this).firstLineStyle();
}

inline RenderElement* ContainerNode::renderer() const
{
    return downcast<RenderElement>(Node::renderer());
}

inline int adjustForAbsoluteZoom(int value, const RenderElement& renderer)
{
    return adjustForAbsoluteZoom(value, renderer.style());
}

inline LayoutUnit adjustLayoutUnitForAbsoluteZoom(LayoutUnit value, const RenderElement& renderer)
{
    return adjustLayoutUnitForAbsoluteZoom(value, renderer.style());
}

RenderBlock* containingBlockForFixedPosition(const RenderElement*);
RenderBlock* containingBlockForAbsolutePosition(const RenderElement*);
RenderBlock* containingBlockForObjectInFlow(const RenderElement*);
} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderElement, isRenderElement())

#endif // RenderElement_h
