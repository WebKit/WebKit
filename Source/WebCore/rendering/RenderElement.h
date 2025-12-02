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

#include <WebCore/HitTestRequest.h>
#include <WebCore/RenderObject.h>
#include <WebCore/RenderPtr.h>
#include <WebCore/RenderStyle.h>
#include <wtf/CheckedRef.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Packed.h>

namespace WebCore {

class ContainerNode;
class BlendingKeyframes;
class GraphicsLayerAnimation;
class ReferencedSVGResources;
class RenderBlock;
class RenderStyle;
class RenderTreeBuilder;
class StyleImage;
struct ImageOrientation;

struct MarginRect {
    LayoutRect marginRect;
    LayoutRect anchorRect;
};

namespace Layout {
class ElementBox;
}

namespace Style {
struct Content;
}

class RenderElement : public RenderObject {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(RenderElement);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderElement);
public:
    virtual ~RenderElement();

    static bool isContentDataSupported(const Style::Content&);

    enum class ConstructBlockLevelRendererFor {
        Inline           = 1 << 0,
        ListItem         = 1 << 1,
        TableOrTablePart = 1 << 2
    };
    static RenderPtr<RenderElement> createFor(Element&, RenderStyle&&, OptionSet<ConstructBlockLevelRendererFor> = { });

    bool hasInitializedStyle() const { return m_hasInitializedStyle; }

    const RenderStyle& style() const { return m_style; }
    // FIXME: Remove checkedStyle once https://github.com/llvm/llvm-project/pull/142485 lands. This is a false positive.
    const CheckedRef<const RenderStyle> checkedStyle() const { return m_style; }
    const RenderStyle* parentStyle() const { return !m_parent ? nullptr : &m_parent->style(); }
    const RenderStyle& firstLineStyle() const;

    // FIXME: Style shouldn't be mutated.
    RenderStyle& mutableStyle() { return m_style; }

    void initializeStyle();

    // Calling with minimalStyleDifference > StyleDifference::Equal indicates that
    // out-of-band state (e.g. animations) requires that styleDidChange processing
    // continue even if the style isn't different from the current style.
    void setStyle(RenderStyle&&, StyleDifference minimalStyleDifference = StyleDifference::Equal);

    // The pseudo element style can be cached or uncached. Use the uncached method if the pseudo element
    // has the concept of changing state (like ::-webkit-scrollbar-thumb:hover), or if it takes additional
    // parameters (like ::highlight(name)).
    const RenderStyle* getCachedPseudoStyle(const Style::PseudoElementIdentifier&, const RenderStyle* parentStyle = nullptr) const;
    std::unique_ptr<RenderStyle> getUncachedPseudoStyle(const Style::PseudoElementRequest&, const RenderStyle* parentStyle = nullptr, const RenderStyle* ownStyle = nullptr) const;

    // This is null for anonymous renderers.
    inline Element* element() const; // Defined in RenderElementInlines.h
    inline RefPtr<Element> protectedElement() const; // Defined in RenderElementInlines.h
    inline Element* nonPseudoElement() const; // Defined in RenderElementInlines.h
    inline RefPtr<Element> protectedNonPseudoElement() const; // Defined in RenderElementInlines.h
    inline Element* generatingElement() const; // Defined in RenderElementInlines.h

    RenderObject* firstChild() const { return m_firstChild.get(); }
    RenderObject* lastChild() const { return m_lastChild.get(); }
    RenderObject* firstInFlowChild() const;
    RenderObject* lastInFlowChild() const;

    Layout::ElementBox* layoutBox();
    const Layout::ElementBox* layoutBox() const;

    // Note that even if these 2 "canContain" functions return true for a particular renderer, it does not necessarily mean the renderer is the containing block (see containingBlockForAbsolute(Fixed)Position).
    inline bool canContainFixedPositionObjects(const RenderStyle* styleToUse = nullptr) const; // Defined in RenderElementStyleInlines.h.
    inline bool canContainAbsolutelyPositionedObjects(const RenderStyle* styleToUse = nullptr) const; // Defined in RenderElementStyleInlines.h.
    bool canEstablishContainingBlockWithTransform() const;

    inline bool shouldApplyLayoutContainment(const RenderStyle* styleToUse = nullptr) const; // Defined in RenderElementStyleInlines.h
    inline bool shouldApplySizeContainment() const; // Defined in RenderElementStyleInlines.h
    inline bool shouldApplyInlineSizeContainment() const; // Defined in RenderElementStyleInlines.h.
    inline bool shouldApplySizeOrInlineSizeContainment() const; // Defined in RenderElementStyleInlines.h
    inline bool shouldApplyStyleContainment() const; // Defined in RenderElementStyleInlines.h.
    inline bool shouldApplyPaintContainment(const RenderStyle* styleToUse = nullptr) const; // Defined in RenderElementStyleInlines.h.
    inline bool shouldApplyAnyContainment() const; // Defined in RenderElementStyleInlines.h.

    bool hasEligibleContainmentForSizeQuery() const;

    Color selectionColor(CSSPropertyID) const;
    std::unique_ptr<RenderStyle> selectionPseudoStyle() const;

    // Obtains the selection colors that should be used when painting a selection.
    Color selectionBackgroundColor() const;
    Color selectionForegroundColor() const;
    Color selectionEmphasisMarkColor() const;

    const RenderStyle* spellingErrorPseudoStyle() const;
    const RenderStyle* grammarErrorPseudoStyle() const;
    const RenderStyle* targetTextPseudoStyle() const;

    virtual bool isChildAllowed(const RenderObject&, const RenderStyle&) const { return true; }
    void didAttachChild(RenderObject& child, RenderObject* beforeChild);

    // The following functions are used when the render tree hierarchy changes to make sure layers get
    // properly added and removed. Since containership can be implemented by any subclass, and since a hierarchy
    // can contain a mixture of boxes and other object types, these functions need to be in the base class.
    RenderLayer* layerParent() const;
    RenderLayer* layerNextSibling(RenderLayer& parentLayer) const;
    void removeLayers();
    void moveLayers(RenderLayer& newParent);

    virtual void dirtyLineFromChangedChild() { }

    void setChildNeedsLayout(MarkingBehavior = MarkContainingBlockChain);
    void setOutOfFlowChildNeedsStaticPositionLayout();
    void clearChildNeedsLayout();
    void setNeedsOutOfFlowMovementLayout(const RenderStyle* oldStyle);
    void setNeedsLayoutForStyleDifference(StyleDifference, const RenderStyle* oldStyle);
    void setNeedsLayoutForOverflowChange();

    // paintOffset is the offset from the origin of the GraphicsContext at which to paint the current object.
    virtual void paint(PaintInfo&, const LayoutPoint& paintOffset) = 0;

    // inline-block elements paint all phases atomically. This function ensures that. Certain other elements
    // (grid items, flex items) require this behavior as well, and this function exists as a helper for them.
    // It is expected that the caller will call this function independent of the value of paintInfo.phase.
    void paintAsInlineBlock(PaintInfo&, const LayoutPoint&);

    // Recursive function that computes the size and position of this object and all its descendants.
    virtual void layout();

    /* This function performs a layout only if one is needed. */
    void layoutIfNeeded();

    // Updates only the local style ptr of the object. Does not update the state of the object,
    // and so only should be called when the style is known not to have changed (or from setStyle).
    void setStyleInternal(RenderStyle&& style) { m_style = WTFMove(style); }

    // Repaint only if our old bounds and new bounds are different. The caller may pass in newBounds and newOutlineBox if they are known.
    bool repaintAfterLayoutIfNeeded(SingleThreadWeakPtr<const RenderLayerModelObject>&& repaintContainer, RequiresFullRepaint, const RepaintRects& oldRects, const RepaintRects& newRects);

    void repaintClientsOfReferencedSVGResources() const;
    void repaintRendererOrClientsOfReferencedSVGResources() const;
    void repaintOldAndNewPositionsForSVGRenderer() const;

    bool borderImageIsLoadedAndCanBeRendered() const;
    bool isVisibleIgnoringGeometry() const;
    bool mayCauseRepaintInsideViewport(const IntRect* visibleRect = nullptr) const;
    bool isVisibleInDocumentRect(const IntRect& documentRect) const;
    bool isInsideEntirelyHiddenLayer() const;

    // Returns true if this renderer requires a new stacking context.
    static bool createsGroupForStyle(const RenderStyle&); // Defined in RenderElementStyleInlines.h.
    bool createsGroup() const { return createsGroupForStyle(style()); }

    inline bool isTransparent() const; // FIXME: This function is incorrectly named. It's isNotOpaque, sometimes called hasOpacity, not isEntirelyTransparent. Defined in RenderElementStyleInlines.h.
    inline float opacity() const; // Defined in RenderElementStyleInlines.h.

    inline bool visibleToHitTesting(const std::optional<HitTestRequest>& = std::nullopt) const; // Defined in RenderElementStyleInlines.h.

    inline bool hasBackground() const; // Defined in RenderElementStyleInlines.h.
    inline bool hasMask() const; // Defined in RenderElementStyleInlines.h.
    inline bool hasClip() const; // Defined in RenderElementStyleInlines.h.
    inline bool hasClipOrNonVisibleOverflow() const; // Defined in RenderElementStyleInlines.h.
    inline bool hasClipPath() const; // Defined in RenderElementStyleInlines.h.
    inline bool hasHiddenBackface() const; // Defined in RenderElementStyleInlines.h.
    bool hasViewTransitionName() const;
    bool isViewTransitionRoot() const;
    bool requiresRenderingConsolidationForViewTransition() const;
    bool hasOutlineAnnotation() const;
    inline bool hasOutline() const; // Defined in RenderElementStyleInlines.h.
    bool hasSelfPaintingLayer() const;

    bool checkForRepaintDuringLayout() const;

    // absoluteAnchorRect() is conceptually similar to absoluteBoundingBoxRect(), but is intended for scrolling to an
    // anchor. For inline renderers, this gets the logical top left of the first leaf child and the logical bottom
    // right of the last leaf child, converts them to absolute coordinates, and makes a box out of them.
    LayoutRect absoluteAnchorRect(bool* insideFixed = nullptr) const;

    // absoluteAnchorRectWithScrollMargin() is similar to absoluteAnchorRect, but it also takes into account any
    // CSS scroll-margin that is set in the style of this RenderElement.
    MarginRect absoluteAnchorRectWithScrollMargin(bool* insideFixed = nullptr) const;

    inline bool hasFilter() const; // Defined in RenderElementStyleInlines.h.
    inline bool hasBackdropFilter() const; // Defined in RenderElementStyleInlines.h.
    inline bool hasBlendMode() const; // Defined in RenderElementStyleInlines.h.
    inline bool hasShapeOutside() const; // Defined in RenderElementStyleInlines.h.

#if HAVE(CORE_MATERIAL)
    inline bool hasAppleVisualEffect() const; // Defined in RenderElementStyleInlines.h.
    inline bool hasAppleVisualEffectRequiringBackdropFilter() const; // Defined in RenderElementStyleInlines.h.
#endif

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

#if HAVE(SUPPORT_HDR_DISPLAY)
    bool hasHDRImages() const { return m_hasHDRImages; }
    void setHasHDRImages(bool b) { m_hasHDRImages = b; }
#endif

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

    virtual bool startAnimation(double /* timeOffset */, const GraphicsLayerAnimation&, const BlendingKeyframes&) { return false; }
    virtual void animationPaused(double /* timeOffset */, const BlendingKeyframes&) { }
    virtual void animationFinished(const BlendingKeyframes&) { }
    virtual void transformRelatedPropertyDidChange() { }

    // https://www.w3.org/TR/css-transforms-1/#transform-box
    inline FloatRect transformReferenceBoxRect(const RenderStyle&) const; // Defined in RenderElementStyleInlines.h.
    inline FloatRect transformReferenceBoxRect() const; // Defined in RenderElementStyleInlines.h.

    // https://www.w3.org/TR/css-transforms-1/#reference-box
    virtual FloatRect referenceBoxRect(CSSBoxType) const;

    virtual void suspendAnimations(MonotonicTime = MonotonicTime()) { }
    std::unique_ptr<RenderStyle> animatedStyle();

    SingleThreadWeakPtr<RenderBlockFlow> backdropRenderer() const;
    void setBackdropRenderer(RenderBlockFlow&);

    ReferencedSVGResources& ensureReferencedSVGResources();

    Overflow effectiveOverflowX() const;
    Overflow effectiveOverflowY() const;
    inline Overflow effectiveOverflowInlineDirection() const;
    inline Overflow effectiveOverflowBlockDirection() const;
    virtual bool overflowChangesMayAffectLayout() const { return false; }

    bool isWritingModeRoot() const { return !parent() || parent()->style().writingMode().computedWritingMode() != style().writingMode().computedWritingMode(); }

    bool isDeprecatedFlexItem() const { return !isInline() && !isFloatingOrOutOfFlowPositioned() && parent() && parent()->isRenderDeprecatedFlexibleBox(); }
    bool isFlexItemIncludingDeprecated() const { return !isInline() && !isFloatingOrOutOfFlowPositioned() && parent() && parent()->isFlexibleBoxIncludingDeprecated(); }

    virtual LayoutRect paintRectToClipOutFromBorder(const LayoutRect&) { return { }; }

    static void markRendererDirtyAfterTopLayerChange(RenderElement* renderer, RenderBlock* containingBlockBeforeStyleResolution);

    void clearNeedsLayoutForSkippedContent();

    void setRenderBoxHasShapeOutsideInfo(bool b) { m_renderBoxHasShapeOutsideInfo = b; }
    void setHasCachedSVGResource(bool b) { m_hasCachedSVGResource = b; }
    bool renderBoxHasShapeOutsideInfo() const { return m_renderBoxHasShapeOutsideInfo; }
    bool hasCachedSVGResource() const { return m_hasCachedSVGResource; }

    bool isAnonymousBlock() const;
    bool isAnonymousForPercentageResolution() const { return isAnonymous() && !isViewTransitionPseudo(); }
    inline bool isBlockBox() const;
    inline bool isBlockLevelBox() const;
    inline bool isBlockContainer() const;

    RenderBoxModelObject* offsetParent() const;
    // Pushes state onto RenderGeometryMap about how to map coordinates from this renderer to its container, or ancestorToStopAt (whichever is encountered first).
    // Returns the renderer which was mapped to (container or ancestorToStopAt).
    virtual const RenderElement* pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap&) const;

    bool isFixedPositioned() const { return isOutOfFlowPositioned() && style().position() == PositionType::Fixed; }
    bool isAbsolutelyPositioned() const { return isOutOfFlowPositioned() && style().position() == PositionType::Absolute; }

    bool isViewTransitionContainer() const { return style().pseudoElementType() == PseudoElementType::ViewTransition || style().pseudoElementType() == PseudoElementType::ViewTransitionGroup || style().pseudoElementType() == PseudoElementType::ViewTransitionImagePair; }
    bool isViewTransitionPseudo() const { return isRenderViewTransitionCapture() || isViewTransitionContainer(); }

    inline bool hasPotentiallyScrollableOverflow() const;

    inline bool isBeforeContent() const;
    inline bool isAfterContent() const;
    inline bool isBeforeOrAfterContent() const;
    static bool isBeforeContent(const RenderElement*);
    static bool isAfterContent(const RenderElement*);
    static bool isBeforeOrAfterContent(const RenderElement*);

    WritingMode writingMode() const { return style().writingMode(); }

protected:
    RenderElement(Type, Element&, RenderStyle&&, OptionSet<TypeFlag>, TypeSpecificFlags);
    RenderElement(Type, Document&, RenderStyle&&, OptionSet<TypeFlag>, TypeSpecificFlags);

    bool layerCreationAllowedForSubtree() const;

    enum class StylePropagationType {
        AllChildren,
        BlockAndRubyChildren
    };
    void propagateStyleToAnonymousChildren(StylePropagationType);

    bool repaintBeforeStyleChange(StyleDifference, const RenderStyle& oldStyle, const RenderStyle& newStyle);

    virtual void styleWillChange(StyleDifference, const RenderStyle& newStyle);
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle);

    void insertedIntoTree() override;
    void willBeRemovedFromTree() override;
    void willBeDestroyed() override;
    void notifyFinished(CachedResource&, const NetworkLoadMetrics&, LoadWillContinueInAnotherProcess) override;

    void pushOntoGeometryMap(RenderGeometryMap&, const RenderLayerModelObject* repaintContainer, RenderElement* container, bool containerSkipped) const;

    void setHasContinuationChainNode(bool b) { m_hasContinuationChainNode = b; }

    void setRenderBlockHasMarginBeforeQuirk(bool b) { m_renderBlockHasMarginBeforeQuirk = b; }
    void setRenderBlockHasMarginAfterQuirk(bool b) { m_renderBlockHasMarginAfterQuirk = b; }
    void setRenderBlockShouldForceRelayoutChildren(bool b) { m_renderBlockShouldForceRelayoutChildren = b; }
    void setRenderBlockHasRareData(bool b) { m_renderBlockHasRareData = b; }
    bool renderBlockHasMarginBeforeQuirk() const { return m_renderBlockHasMarginBeforeQuirk; }
    bool renderBlockHasMarginAfterQuirk() const { return m_renderBlockHasMarginAfterQuirk; }
    bool renderBlockShouldForceRelayoutChildren() const { return m_renderBlockShouldForceRelayoutChildren; }
    bool renderBlockHasRareData() const { return m_renderBlockHasRareData; }

    void setRenderBlockFlowLineLayoutPath(unsigned u) { m_renderBlockFlowLineLayoutPath = u; }
    unsigned renderBlockFlowLineLayoutPath() const { return m_renderBlockFlowLineLayoutPath; }

    void paintOutline(PaintInfo&, const LayoutRect&);
    void updateOutlineAutoAncestor(bool hasOutlineAuto);

    void removeFromRenderFragmentedFlowIncludingDescendants(bool shouldUpdateState);
    void adjustFragmentedFlowStateOnContainingBlockChangeIfNeeded(const RenderStyle& oldStyle, const RenderStyle& newStyle);

    bool isVisibleInViewport() const;

    bool shouldApplyLayoutOrPaintContainment(bool) const;
    inline bool shouldApplySizeOrStyleContainment(bool) const;

private:
    RenderElement(Type, ContainerNode&, RenderStyle&&, OptionSet<TypeFlag>, TypeSpecificFlags);
    void node() const = delete;
    void nonPseudoNode() const = delete;
    void isRenderText() const = delete;
    void isRenderElement() const = delete;

    RenderObject* firstChildSlow() const final { return firstChild(); }
    RenderObject* lastChildSlow() const final { return lastChild(); }

    inline bool mayContainOutOfFlowPositionedObjects(const RenderStyle* styleToUse = nullptr) const; // Defined in RenderElementStyleInlines.h.

    RenderElement* rendererForPseudoStyleAcrossShadowBoundary() const;

    // Called when an object that was floating or positioned becomes a normal flow object
    // again.  We have to make sure the render tree updates as needed to accommodate the new
    // normal flow object.
    void handleDynamicFloatPositionChange();

    bool shouldRepaintForStyleDifference(StyleDifference) const;

    template<typename FillLayerType> void updateFillImages(const FillLayerType*, const FillLayerType*);
    void updateImage(StyleImage*, StyleImage*);
    void updateShapeImage(const Style::ShapeOutside*, const Style::ShapeOutside*);

    StyleDifference adjustStyleDifference(StyleDifference, OptionSet<StyleDifferenceContextSensitiveProperty>) const;

    bool canDestroyDecodedData() const final { return !isVisibleInViewport(); }
    VisibleInViewportState imageFrameAvailable(CachedImage&, ImageAnimatingState, const IntRect* changeRect) final;
    VisibleInViewportState imageVisibleInViewport(const Document&) const final;
    void didRemoveCachedImageClient(CachedImage&) final;
    void imageContentChanged(CachedImage&) final;
    void scheduleRenderingUpdateForImage(CachedImage&) final;

    bool getLeadingCorner(FloatPoint& output, bool& insideFixed) const;
    bool getTrailingCorner(FloatPoint& output, bool& insideFixed) const;

    void clearSubtreeLayoutRootIfNeeded() const;
    
    bool shouldWillChangeCreateStackingContext() const;
    void issueRepaintForOutlineAuto(float outlineSize);
    
    void updateReferencedSVGResources();
    void clearReferencedSVGResources();

    const RenderStyle* textSegmentPseudoStyle(PseudoElementType) const;

    SingleThreadPackedWeakPtr<RenderObject> m_firstChild;
    unsigned m_hasInitializedStyle : 1;

    unsigned m_hasPausedImageAnimations : 1;
    unsigned m_hasCounterNodeMap : 1;
    unsigned m_hasContinuationChainNode : 1;
#if HAVE(SUPPORT_HDR_DISPLAY)
    unsigned m_hasHDRImages : 1;
#endif

    unsigned m_isContinuation : 1;
    unsigned m_isFirstLetter : 1;
    unsigned m_renderBlockHasMarginBeforeQuirk : 1;
    unsigned m_renderBlockHasMarginAfterQuirk : 1;
    unsigned m_renderBlockShouldForceRelayoutChildren : 1;
    unsigned m_renderBlockHasRareData : 1 { false };
    unsigned m_renderBoxHasShapeOutsideInfo : 1 { false };
    unsigned m_hasCachedSVGResource : 1 { false };
    unsigned m_renderBlockFlowLineLayoutPath : 3;
    // 1 bit free.

    SingleThreadPackedWeakPtr<RenderObject> m_lastChild;

    unsigned m_isRegisteredForVisibleInViewportCallback : 1;
    unsigned m_visibleInViewportState : 2;
    unsigned m_didContributeToVisuallyNonEmptyPixelCount : 1;
    // 12 bits free.

    RenderStyle m_style;
};

inline int adjustForAbsoluteZoom(int, const RenderElement&); // Defined in RenderElementStyleInlines.h.
inline LayoutUnit adjustLayoutUnitForAbsoluteZoom(LayoutUnit, const RenderElement&); // Defined in RenderElementStyleInlines.h.
inline LayoutSize adjustLayoutSizeForAbsoluteZoom(LayoutSize, const RenderElement&); // Defined in RenderElementStyleInlines.h.

inline void RenderElement::setChildNeedsLayout(MarkingBehavior markParents)
{
    ASSERT(!isSetNeedsLayoutForbidden());
    if (normalChildNeedsLayout())
        return;
    setNormalChildNeedsLayoutBit(true);
    if (markParents == MarkContainingBlockChain)
        scheduleLayout(markContainingBlocksForLayout());
}

inline bool RenderElement::canEstablishContainingBlockWithTransform() const
{
    return isRenderBlock() || (isTablePart() && !isRenderTableCol());
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

inline RenderElement* RenderObject::parent() const
{
    return m_parent.get();
}

inline CheckedPtr<RenderElement> RenderObject::checkedParent() const
{
    return m_parent.get();
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderElement, isRenderElement())
