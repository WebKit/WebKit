/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003-2022 Apple Inc. All rights reserved.
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

#pragma once

#include "HitTestRequest.h"
#include "LengthFunctions.h"
#include "RenderObject.h"

namespace WebCore {

class Animation;
class ContentData;
class ControlStates;
class KeyframeList;
class ReferencedSVGResources;
class RenderBlock;
class RenderStyle;
class RenderTreeBuilder;

struct MarginRect {
    LayoutRect marginRect;
    LayoutRect anchorRect;
};

namespace Layout {
class ElementBox;
}

class RenderElement : public RenderObject {
    WTF_MAKE_ISO_ALLOCATED(RenderElement);
public:
    virtual ~RenderElement();

    static bool isContentDataSupported(const ContentData&);

    enum class ConstructBlockLevelRendererFor {
        Inline           = 1 << 0,
        ListItem         = 1 << 1,
        TableOrTablePart = 1 << 2
    };
    static RenderPtr<RenderElement> createFor(Element&, RenderStyle&&, OptionSet<ConstructBlockLevelRendererFor> = { });

    bool hasInitializedStyle() const { return m_hasInitializedStyle; }

    const RenderStyle& style() const { return m_style; }
    const RenderStyle* parentStyle() const { return !m_parent ? nullptr : &m_parent->style(); }
    const RenderStyle& firstLineStyle() const;

    // FIXME: Style shouldn't be mutated.
    RenderStyle& mutableStyle() { return m_style; }

    void initializeStyle();

    // Calling with minimalStyleDifference > StyleDifference::Equal indicates that
    // out-of-band state (e.g. animations) requires that styleDidChange processing
    // continue even if the style isn't different from the current style.
    void setStyle(RenderStyle&&, StyleDifference minimalStyleDifference = StyleDifference::Equal);

    // The pseudo element style can be cached or uncached.  Use the cached method if the pseudo element doesn't respect
    // any pseudo classes (and therefore has no concept of changing state).
    const RenderStyle* getCachedPseudoStyle(PseudoId, const RenderStyle* parentStyle = nullptr) const;
    std::unique_ptr<RenderStyle> getUncachedPseudoStyle(const Style::PseudoElementRequest&, const RenderStyle* parentStyle = nullptr, const RenderStyle* ownStyle = nullptr) const;

    // This is null for anonymous renderers.
    Element* element() const { return downcast<Element>(RenderObject::node()); }
    Element* nonPseudoElement() const { return downcast<Element>(RenderObject::nonPseudoNode()); }
    Element* generatingElement() const;

    RenderObject* firstChild() const { return m_firstChild; }
    RenderObject* lastChild() const { return m_lastChild; }
    RenderObject* firstInFlowChild() const;
    RenderObject* lastInFlowChild() const;

    Layout::ElementBox* layoutBox();
    const Layout::ElementBox* layoutBox() const;

    // Note that even if these 2 "canContain" functions return true for a particular renderer, it does not necessarily mean the renderer is the containing block (see containingBlockForAbsolute(Fixed)Position).
    inline bool canContainFixedPositionObjects() const;
    inline bool canContainAbsolutelyPositionedObjects() const;
    bool canEstablishContainingBlockWithTransform() const;

    inline bool shouldApplyLayoutContainment() const;
    inline bool shouldApplySizeContainment() const;
    inline bool shouldApplyInlineSizeContainment() const;
    inline bool shouldApplySizeOrInlineSizeContainment() const;
    inline bool shouldApplyStyleContainment() const;
    inline bool shouldApplyPaintContainment() const;
    inline bool shouldApplyLayoutOrPaintContainment() const;
    inline bool shouldApplyAnyContainment() const;

    bool hasEligibleContainmentForSizeQuery() const;

    Color selectionColor(CSSPropertyID) const;
    std::unique_ptr<RenderStyle> selectionPseudoStyle() const;

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

    virtual bool isChildAllowed(const RenderObject&, const RenderStyle&) const { return true; }
    void didAttachChild(RenderObject& child, RenderObject* beforeChild);

    // The following functions are used when the render tree hierarchy changes to make sure layers get
    // properly added and removed. Since containership can be implemented by any subclass, and since a hierarchy
    // can contain a mixture of boxes and other object types, these functions need to be in the base class.
    RenderLayer* layerParent() const;
    RenderLayer* layerNextSibling(RenderLayer& parentLayer) const;
    void removeLayers();
    void moveLayers(RenderLayer& newParent);

    virtual void dirtyLinesFromChangedChild(RenderObject&) { }

    bool ancestorLineBoxDirty() const { return m_ancestorLineBoxDirty; }
    void setAncestorLineBoxDirty(bool f = true);

    void setChildNeedsLayout(MarkingBehavior = MarkContainingBlockChain);
    void clearChildNeedsLayout();
    void setNeedsPositionedMovementLayout(const RenderStyle* oldStyle);
    void setNeedsSimplifiedNormalFlowLayout();

    // paintOffset is the offset from the origin of the GraphicsContext at which to paint the current object.
    virtual void paint(PaintInfo&, const LayoutPoint& paintOffset) = 0;

    // inline-block elements paint all phases atomically. This function ensures that. Certain other elements
    // (grid items, flex items) require this behavior as well, and this function exists as a helper for them.
    // It is expected that the caller will call this function independent of the value of paintInfo.phase.
    void paintAsInlineBlock(PaintInfo&, const LayoutPoint&);

    // Recursive function that computes the size and position of this object and all its descendants.
    virtual void layout();

    /* This function performs a layout only if one is needed. */
    void layoutIfNeeded() { if (needsLayout()) layout(); }

    // Updates only the local style ptr of the object. Does not update the state of the object,
    // and so only should be called when the style is known not to have changed (or from setStyle).
    void setStyleInternal(RenderStyle&& style) { m_style = WTFMove(style); }

    // Repaint only if our old bounds and new bounds are different. The caller may pass in newBounds and newOutlineBox if they are known.
    bool repaintAfterLayoutIfNeeded(const RenderLayerModelObject* repaintContainer, const LayoutRect& oldBounds, const LayoutRect& oldOutlineBox, const LayoutRect* newBoundsPtr = nullptr, const LayoutRect* newOutlineBoxPtr = nullptr);

    bool borderImageIsLoadedAndCanBeRendered() const;
    bool isVisibleIgnoringGeometry() const;
    bool mayCauseRepaintInsideViewport(const IntRect* visibleRect = nullptr) const;
    bool isVisibleInDocumentRect(const IntRect& documentRect) const;
    bool isInsideEntirelyHiddenLayer() const;

    // Returns true if this renderer requires a new stacking context.
    static bool createsGroupForStyle(const RenderStyle&);
    bool createsGroup() const { return createsGroupForStyle(style()); }

    inline bool isTransparent() const; // FIXME: This function is incorrectly named. It's isNotOpaque, sometimes called hasOpacity, not isEntirelyTransparent.
    inline float opacity() const;

    inline bool visibleToHitTesting(const std::optional<HitTestRequest>& = std::nullopt) const;

    inline bool hasBackground() const;
    inline bool hasMask() const;
    inline bool hasClip() const;
    inline bool hasClipOrNonVisibleOverflow() const;
    inline bool hasClipPath() const;
    inline bool hasHiddenBackface() const;
    bool hasOutlineAnnotation() const;
    inline bool hasOutline() const;
    bool hasSelfPaintingLayer() const;

    bool checkForRepaintDuringLayout() const;

    // absoluteAnchorRect() is conceptually similar to absoluteBoundingBoxRect(), but is intended for scrolling to an
    // anchor. For inline renderers, this gets the logical top left of the first leaf child and the logical bottom
    // right of the last leaf child, converts them to absolute coordinates, and makes a box out of them.
    LayoutRect absoluteAnchorRect(bool* insideFixed = nullptr) const;

    // absoluteAnchorRectWithScrollMargin() is similar to absoluteAnchorRect, but it also takes into account any
    // CSS scroll-margin that is set in the style of this RenderElement.
    MarginRect absoluteAnchorRectWithScrollMargin(bool* insideFixed = nullptr) const;

    inline bool hasFilter() const;
    inline bool hasBackdropFilter() const;
    inline bool hasBlendMode() const;
    inline bool hasShapeOutside() const;

    void registerForVisibleInViewportCallback();
    void unregisterForVisibleInViewportCallback();

    VisibleInViewportState visibleInViewportState() const { return static_cast<VisibleInViewportState>(m_visibleInViewportState); }
    void setVisibleInViewportState(VisibleInViewportState);
    virtual void visibleInViewportStateChanged();

    bool didContibuteToVisuallyNonEmptyPixelCount() const { return m_didContributeToVisuallyNonEmptyPixelCount; }
    void setDidContibuteToVisuallyNonEmptyPixelCount() { m_didContributeToVisuallyNonEmptyPixelCount = true; }

    bool allowsAnimation() const final;
    bool repaintForPausedImageAnimationsIfNeeded(const IntRect& visibleRect, CachedImage&);
    bool hasPausedImageAnimations() const { return m_hasPausedImageAnimations; }
    void setHasPausedImageAnimations(bool b) { m_hasPausedImageAnimations = b; }

    void setRenderBoxNeedsLazyRepaint(bool b) { m_renderBoxNeedsLazyRepaint = b; }
    bool renderBoxNeedsLazyRepaint() const { return m_renderBoxNeedsLazyRepaint; }

    bool hasCounterNodeMap() const { return m_hasCounterNodeMap; }
    void setHasCounterNodeMap(bool f) { m_hasCounterNodeMap = f; }

#if ENABLE(TEXT_AUTOSIZING)
    void adjustComputedFontSizesOnBlocks(float size, float visibleWidth);
    WEBCORE_EXPORT void resetTextAutosizing();
#endif

    WEBCORE_EXPORT ImageOrientation imageOrientation() const;

    void removeFromRenderFragmentedFlow();
    virtual void resetEnclosingFragmentedFlowAndChildInfoIncludingDescendants(RenderFragmentedFlow*);

    // Called before anonymousChild.setStyle(). Override to set custom styles for
    // the child.
    virtual void updateAnonymousChildStyle(RenderStyle&) const { };

    bool hasContinuationChainNode() const { return m_hasContinuationChainNode; }
    bool isContinuation() const { return m_isContinuation; }
    void setIsContinuation() { m_isContinuation = true; }
    bool isFirstLetter() const { return m_isFirstLetter; }
    void setIsFirstLetter() { m_isFirstLetter = true; }

    RenderObject* attachRendererInternal(RenderPtr<RenderObject> child, RenderObject* beforeChild);
    RenderPtr<RenderObject> detachRendererInternal(RenderObject&);

    virtual bool startAnimation(double /* timeOffset */, const Animation&, const KeyframeList&) { return false; }
    virtual void animationPaused(double /* timeOffset */, const String& /* name */) { }
    virtual void animationFinished(const String& /* name */) { }
    virtual void transformRelatedPropertyDidChange() { }

    // https://www.w3.org/TR/css-transforms-1/#transform-box
    inline FloatRect transformReferenceBoxRect(const RenderStyle&) const;
    inline FloatRect transformReferenceBoxRect() const;

    // https://www.w3.org/TR/css-transforms-1/#reference-box
    virtual FloatRect referenceBoxRect(CSSBoxType) const;

    virtual void suspendAnimations(MonotonicTime = MonotonicTime()) { }
    std::unique_ptr<RenderStyle> animatedStyle();

    WeakPtr<RenderBlockFlow> backdropRenderer() const;
    void setBackdropRenderer(RenderBlockFlow&);

    ReferencedSVGResources& ensureReferencedSVGResources();

    Overflow effectiveOverflowX() const;
    Overflow effectiveOverflowY() const;
    inline Overflow effectiveOverflowInlineDirection() const;
    inline Overflow effectiveOverflowBlockDirection() const;

    bool isWritingModeRoot() const { return !parent() || parent()->style().writingMode() != style().writingMode(); }

    bool isDeprecatedFlexItem() const { return !isInline() && !isFloatingOrOutOfFlowPositioned() && parent() && parent()->isDeprecatedFlexibleBox(); }
    bool isFlexItemIncludingDeprecated() const { return !isInline() && !isFloatingOrOutOfFlowPositioned() && parent() && parent()->isFlexibleBoxIncludingDeprecated(); }

    virtual LayoutRect paintRectToClipOutFromBorder(const LayoutRect&) { return { }; }
    void paintFocusRing(const PaintInfo&, const RenderStyle&, const Vector<LayoutRect>& focusRingRects) const;

    virtual bool establishesIndependentFormattingContext() const;
    bool createsNewFormattingContext() const;

    bool isSkippedContentRoot() const;

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

    RenderElement(Element&, RenderStyle&&, BaseTypeFlags);
    RenderElement(Document&, RenderStyle&&, BaseTypeFlags);

    bool layerCreationAllowedForSubtree() const;

    enum StylePropagationType { PropagateToAllChildren, PropagateToBlockChildrenOnly };
    void propagateStyleToAnonymousChildren(StylePropagationType);

    bool repaintBeforeStyleChange(StyleDifference, const RenderStyle& oldStyle, const RenderStyle& newStyle);

    virtual void styleWillChange(StyleDifference, const RenderStyle& newStyle);
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle);

    void insertedIntoTree(IsInternalMove) override;
    void willBeRemovedFromTree(IsInternalMove) override;
    void willBeDestroyed() override;
    void notifyFinished(CachedResource&, const NetworkLoadMetrics&) override;

    void setHasContinuationChainNode(bool b) { m_hasContinuationChainNode = b; }

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

    void paintOutline(PaintInfo&, const LayoutRect&);
    void updateOutlineAutoAncestor(bool hasOutlineAuto);

    void removeFromRenderFragmentedFlowIncludingDescendants(bool shouldUpdateState);
    void adjustFragmentedFlowStateOnContainingBlockChangeIfNeeded(const RenderStyle& oldStyle, const RenderStyle& newStyle);

    bool isVisibleInViewport() const;

    bool shouldApplyLayoutOrPaintContainment(bool) const;
    inline bool shouldApplySizeOrStyleContainment(bool) const;

private:
    RenderElement(ContainerNode&, RenderStyle&&, BaseTypeFlags);
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

    bool shouldRepaintForStyleDifference(StyleDifference) const;

    void updateFillImages(const FillLayer*, const FillLayer*);
    void updateImage(StyleImage*, StyleImage*);
    void updateShapeImage(const ShapeValue*, const ShapeValue*);

    StyleDifference adjustStyleDifference(StyleDifference, OptionSet<StyleDifferenceContextSensitiveProperty>) const;

    bool canDestroyDecodedData() final { return !isVisibleInViewport(); }
    VisibleInViewportState imageFrameAvailable(CachedImage&, ImageAnimatingState, const IntRect* changeRect) final;
    VisibleInViewportState imageVisibleInViewport(const Document&) const final;
    void didRemoveCachedImageClient(CachedImage&) final;
    void scheduleRenderingUpdateForImage(CachedImage&) final;

    bool getLeadingCorner(FloatPoint& output, bool& insideFixed) const;
    bool getTrailingCorner(FloatPoint& output, bool& insideFixed) const;

    void clearSubtreeLayoutRootIfNeeded() const;
    
    bool shouldWillChangeCreateStackingContext() const;
    void issueRepaintForOutlineAuto(float outlineSize);
    
    void updateReferencedSVGResources();
    void clearReferencedSVGResources();

    unsigned m_baseTypeFlags : 6;
    unsigned m_ancestorLineBoxDirty : 1;
    unsigned m_hasInitializedStyle : 1;

    unsigned m_renderBoxNeedsLazyRepaint : 1;
    unsigned m_hasPausedImageAnimations : 1;
    unsigned m_hasCounterNodeMap : 1;
    unsigned m_hasContinuationChainNode : 1;
    unsigned m_isContinuation : 1;
    unsigned m_isFirstLetter : 1;

    unsigned m_renderBlockHasMarginBeforeQuirk : 1;
    unsigned m_renderBlockHasMarginAfterQuirk : 1;
    unsigned m_renderBlockShouldForceRelayoutChildren : 1;
    unsigned m_renderBlockFlowHasMarkupTruncation : 1;
    unsigned m_renderBlockFlowLineLayoutPath : 3;

    unsigned m_isRegisteredForVisibleInViewportCallback : 1;
    unsigned m_visibleInViewportState : 2;

    unsigned m_didContributeToVisuallyNonEmptyPixelCount : 1;

    RenderObject* m_firstChild;
    RenderObject* m_lastChild;

    RenderStyle m_style;
};

inline int adjustForAbsoluteZoom(int, const RenderElement&);
inline LayoutUnit adjustLayoutUnitForAbsoluteZoom(LayoutUnit, const RenderElement&);
inline LayoutSize adjustLayoutSizeForAbsoluteZoom(LayoutSize, const RenderElement&);

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
    return downcast<Element>(RenderObject::generatingNode());
}

inline bool RenderElement::canEstablishContainingBlockWithTransform() const
{
    return isRenderBlock() || (isTablePart() && !isRenderTableCol());
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

inline const RenderStyle& RenderObject::style() const
{
    if (isText())
        return m_parent->style();
    return downcast<RenderElement>(*this).style();
}

inline const RenderStyle& RenderObject::firstLineStyle() const
{
    if (isText())
        return m_parent->firstLineStyle();
    return downcast<RenderElement>(*this).firstLineStyle();
}

inline RenderElement* ContainerNode::renderer() const
{
    return downcast<RenderElement>(Node::renderer());
}

inline RenderObject* RenderElement::firstInFlowChild() const
{
    if (auto* firstChild = this->firstChild()) {
        if (firstChild->isInFlow())
            return firstChild;
        return firstChild->nextInFlowSibling();
    }
    return nullptr;
}

inline RenderObject* RenderElement::lastInFlowChild() const
{
    if (auto* lastChild = this->lastChild()) {
        if (lastChild->isInFlow())
            return lastChild;
        return lastChild->previousInFlowSibling();
    }
    return nullptr;
}

inline bool RenderObject::isSkippedContentRoot() const
{
    if (isText())
        return false;
    return downcast<RenderElement>(*this).isSkippedContentRoot();
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderElement, isRenderElement())
