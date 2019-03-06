/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 *           (C) 2004 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#pragma once

#include "CachedImageClient.h"
#include "Element.h"
#include "FloatQuad.h"
#include "Frame.h"
#include "LayoutRect.h"
#include "Page.h"
#include "RenderObjectEnums.h"
#include "RenderStyle.h"
#include "ScrollAlignment.h"
#include "StyleImage.h"
#include "TextAffinity.h"
#include <wtf/IsoMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class AffineTransform;
class CSSAnimationController;
class Color;
class Cursor;
class Document;
class DocumentTimeline;
class HitTestLocation;
class HitTestRequest;
class HitTestResult;
class InlineBox;
class Path;
class Position;
class PseudoStyleRequest;
class RenderBoxModelObject;
class RenderInline;
class RenderBlock;
class RenderElement;
class RenderFragmentedFlow;
class RenderGeometryMap;
class RenderLayer;
class RenderLayerModelObject;
class RenderFragmentContainer;
class RenderTheme;
class RenderTreeBuilder;
class SelectionRangeData;
class TransformState;
class VisiblePosition;

#if PLATFORM(IOS_FAMILY)
class SelectionRect;
#endif

struct PaintInfo;

#if PLATFORM(IOS_FAMILY)
const int caretWidth = 2; // This value should be kept in sync with UIKit. See <rdar://problem/15580601>.
#else
const int caretWidth = 1;
#endif

enum class ShouldAllowCrossOriginScrolling { No, Yes };

struct ScrollRectToVisibleOptions;

#if ENABLE(DASHBOARD_SUPPORT)
struct AnnotatedRegionValue {
    bool operator==(const AnnotatedRegionValue& o) const
    {
        return type == o.type && bounds == o.bounds && clip == o.clip && label == o.label;
    }
    bool operator!=(const AnnotatedRegionValue& o) const
    {
        return !(*this == o);
    }

    LayoutRect bounds;
    String label;
    LayoutRect clip;
    int type;
};
#endif

// Base class for all rendering tree objects.
class RenderObject : public CachedImageClient, public CanMakeWeakPtr<RenderObject> {
    WTF_MAKE_ISO_ALLOCATED(RenderObject);
    friend class RenderBlock;
    friend class RenderBlockFlow;
    friend class RenderElement;
    friend class RenderLayer;
public:
    // Anonymous objects should pass the document as their node, and they will then automatically be
    // marked as anonymous in the constructor.
    explicit RenderObject(Node&);
    virtual ~RenderObject();

    RenderTheme& theme() const;

    virtual const char* renderName() const = 0;

    RenderElement* parent() const { return m_parent; }
    bool isDescendantOf(const RenderObject*) const;

    RenderObject* previousSibling() const { return m_previous; }
    RenderObject* nextSibling() const { return m_next; }

    // Use RenderElement versions instead.
    virtual RenderObject* firstChildSlow() const { return nullptr; }
    virtual RenderObject* lastChildSlow() const { return nullptr; }

    RenderObject* nextInPreOrder() const;
    RenderObject* nextInPreOrder(const RenderObject* stayWithin) const;
    RenderObject* nextInPreOrderAfterChildren() const;
    RenderObject* nextInPreOrderAfterChildren(const RenderObject* stayWithin) const;
    RenderObject* previousInPreOrder() const;
    RenderObject* previousInPreOrder(const RenderObject* stayWithin) const;
    WEBCORE_EXPORT RenderObject* childAt(unsigned) const;

    RenderObject* firstLeafChild() const;
    RenderObject* lastLeafChild() const;

#if ENABLE(TEXT_AUTOSIZING)
    // Minimal distance between the block with fixed height and overflowing content and the text block to apply text autosizing.
    // The greater this constant is the more potential places we have where autosizing is turned off.
    // So it should be as low as possible. There are sites that break at 2.
    static const int TextAutoSizingFixedHeightDepth = 3;

    enum BlockContentHeightType {
        FixedHeight,
        FlexibleHeight,
        OverflowHeight
    };

    typedef BlockContentHeightType (*HeightTypeTraverseNextInclusionFunction)(const RenderObject&);
    RenderObject* traverseNext(const RenderObject* stayWithin, HeightTypeTraverseNextInclusionFunction, int& currentDepth,  int& newFixedDepth) const;
#endif

    WEBCORE_EXPORT RenderLayer* enclosingLayer() const;

    // Scrolling is a RenderBox concept, however some code just cares about recursively scrolling our enclosing ScrollableArea(s).
    WEBCORE_EXPORT bool scrollRectToVisible(const LayoutRect& absoluteRect, bool insideFixed, const ScrollRectToVisibleOptions&);

    // Convenience function for getting to the nearest enclosing box of a RenderObject.
    WEBCORE_EXPORT RenderBox& enclosingBox() const;
    RenderBoxModelObject& enclosingBoxModelObject() const;
    const RenderBox* enclosingScrollableContainerForSnapping() const;

    // Function to return our enclosing flow thread if we are contained inside one. This
    // function follows the containing block chain.
    RenderFragmentedFlow* enclosingFragmentedFlow() const
    {
        if (fragmentedFlowState() == NotInsideFragmentedFlow)
            return nullptr;

        return locateEnclosingFragmentedFlow();
    }

    WEBCORE_EXPORT bool useDarkAppearance() const;
    OptionSet<StyleColor::Options> styleColorOptions() const;

#ifndef NDEBUG
    void setHasAXObject(bool flag) { m_hasAXObject = flag; }
    bool hasAXObject() const { return m_hasAXObject; }

    // Helper class forbidding calls to setNeedsLayout() during its lifetime.
    class SetLayoutNeededForbiddenScope {
    public:
        explicit SetLayoutNeededForbiddenScope(RenderObject*, bool isForbidden = true);
        ~SetLayoutNeededForbiddenScope();
    private:
        RenderObject* m_renderObject;
        bool m_preexistingForbidden;
    };
#endif

    // Obtains the nearest enclosing block (including this block) that contributes a first-line style to our inline
    // children.
    virtual RenderBlock* firstLineBlock() const;
    
    // RenderObject tree manipulation
    //////////////////////////////////////////
    virtual bool canHaveChildren() const = 0;
    virtual bool canHaveGeneratedChildren() const;
    virtual bool createsAnonymousWrapper() const { return false; }
    //////////////////////////////////////////

#if ENABLE(TREE_DEBUGGING)
    void showNodeTreeForThis() const;
    void showRenderTreeForThis() const;
    void showLineTreeForThis() const;

    void outputRenderObject(WTF::TextStream&, bool mark, int depth) const;
    void outputRenderSubTreeAndMark(WTF::TextStream&, const RenderObject* markedObject, int depth) const;
    void outputRegionsInformation(WTF::TextStream&) const;
#endif

    bool isPseudoElement() const { return node() && node()->isPseudoElement(); }

    bool isRenderElement() const { return !isText(); }
    bool isRenderReplaced() const;
    bool isBoxModelObject() const;
    bool isRenderBlock() const;
    bool isRenderBlockFlow() const;
    bool isRenderInline() const;
    bool isRenderLayerModelObject() const;

    virtual bool isCounter() const { return false; }
    virtual bool isQuote() const { return false; }

    virtual bool isDetailsMarker() const { return false; }
    virtual bool isEmbeddedObject() const { return false; }
    bool isFieldset() const;
    virtual bool isFileUploadControl() const { return false; }
    virtual bool isFrame() const { return false; }
    virtual bool isFrameSet() const { return false; }
    virtual bool isImage() const { return false; }
    virtual bool isInlineBlockOrInlineTable() const { return false; }
    virtual bool isListBox() const { return false; }
    virtual bool isListItem() const { return false; }
    virtual bool isListMarker() const { return false; }
    virtual bool isMedia() const { return false; }
    virtual bool isMenuList() const { return false; }
#if ENABLE(METER_ELEMENT)
    virtual bool isMeter() const { return false; }
#endif
    virtual bool isSnapshottedPlugIn() const { return false; }
    virtual bool isProgress() const { return false; }
    virtual bool isRenderButton() const { return false; }
    virtual bool isRenderIFrame() const { return false; }
    virtual bool isRenderImage() const { return false; }
    virtual bool isRenderFragmentContainer() const { return false; }
    virtual bool isReplica() const { return false; }

    virtual bool isRubyInline() const { return false; }
    virtual bool isRubyBlock() const { return false; }
    virtual bool isRubyBase() const { return false; }
    virtual bool isRubyRun() const { return false; }
    virtual bool isRubyText() const { return false; }

    virtual bool isSlider() const { return false; }
    virtual bool isSliderThumb() const { return false; }
    virtual bool isTable() const { return false; }
    virtual bool isTableCell() const { return false; }
    virtual bool isRenderTableCol() const { return false; }
    virtual bool isTableCaption() const { return false; }
    virtual bool isTableRow() const { return false; }
    virtual bool isTableSection() const { return false; }
    virtual bool isTextControl() const { return false; }
    virtual bool isTextArea() const { return false; }
    virtual bool isTextField() const { return false; }
    virtual bool isSearchField() const { return false; }
    virtual bool isTextControlInnerBlock() const { return false; }
    virtual bool isVideo() const { return false; }
    virtual bool isWidget() const { return false; }
    virtual bool isCanvas() const { return false; }
#if ENABLE(ATTACHMENT_ELEMENT)
    virtual bool isAttachment() const { return false; }
#endif
#if ENABLE(FULLSCREEN_API)
    virtual bool isRenderFullScreen() const { return false; }
    virtual bool isRenderFullScreenPlaceholder() const { return false; }
#endif
    virtual bool isRenderGrid() const { return false; }
    bool isInFlowRenderFragmentedFlow() const { return isRenderFragmentedFlow() && !isOutOfFlowPositioned(); }
    bool isOutOfFlowRenderFragmentedFlow() const { return isRenderFragmentedFlow() && isOutOfFlowPositioned(); }

    virtual bool isMultiColumnBlockFlow() const { return false; }
    virtual bool isRenderMultiColumnSet() const { return false; }
    virtual bool isRenderMultiColumnFlow() const { return false; }
    virtual bool isRenderMultiColumnSpannerPlaceholder() const { return false; }

    virtual bool isRenderScrollbarPart() const { return false; }

    bool isDocumentElementRenderer() const { return document().documentElement() == &m_node; }
    bool isBody() const { return node() && node()->hasTagName(HTMLNames::bodyTag); }
    bool isHR() const { return node() && node()->hasTagName(HTMLNames::hrTag); }
    bool isLegend() const;

    bool isHTMLMarquee() const;

    bool isTablePart() const { return isTableCell() || isRenderTableCol() || isTableCaption() || isTableRow() || isTableSection(); }

    inline bool isBeforeContent() const;
    inline bool isAfterContent() const;
    inline bool isBeforeOrAfterContent() const;
    static inline bool isBeforeContent(const RenderObject* obj) { return obj && obj->isBeforeContent(); }
    static inline bool isAfterContent(const RenderObject* obj) { return obj && obj->isAfterContent(); }
    static inline bool isBeforeOrAfterContent(const RenderObject* obj) { return obj && obj->isBeforeOrAfterContent(); }

    bool beingDestroyed() const { return m_bitfields.beingDestroyed(); }

    bool everHadLayout() const { return m_bitfields.everHadLayout(); }

    bool childrenInline() const { return m_bitfields.childrenInline(); }
    void setChildrenInline(bool b) { m_bitfields.setChildrenInline(b); }
    
    enum FragmentedFlowState {
        NotInsideFragmentedFlow = 0,
        InsideInFragmentedFlow = 1,
    };

    void setFragmentedFlowStateIncludingDescendants(FragmentedFlowState);

    FragmentedFlowState fragmentedFlowState() const { return m_bitfields.fragmentedFlowState(); }
    void setFragmentedFlowState(FragmentedFlowState state) { m_bitfields.setFragmentedFlowState(state); }

#if ENABLE(MATHML)
    virtual bool isRenderMathMLBlock() const { return false; }
    virtual bool isRenderMathMLTable() const { return false; }
    virtual bool isRenderMathMLOperator() const { return false; }
    virtual bool isRenderMathMLRow() const { return false; }
    virtual bool isRenderMathMLMath() const { return false; }
    virtual bool isRenderMathMLMenclose() const { return false; }
    virtual bool isRenderMathMLFenced() const { return false; }
    virtual bool isRenderMathMLFencedOperator() const { return false; }
    virtual bool isRenderMathMLFraction() const { return false; }
    virtual bool isRenderMathMLPadded() const { return false; }
    virtual bool isRenderMathMLRoot() const { return false; }
    virtual bool isRenderMathMLSpace() const { return false; }
    virtual bool isRenderMathMLSquareRoot() const { return false; }
    virtual bool isRenderMathMLScripts() const { return false; }
    virtual bool isRenderMathMLToken() const { return false; }
    virtual bool isRenderMathMLUnderOver() const { return false; }
#endif // ENABLE(MATHML)

    // FIXME: Until all SVG renders can be subclasses of RenderSVGModelObject we have
    // to add SVG renderer methods to RenderObject with an ASSERT_NOT_REACHED() default implementation.
    virtual bool isRenderSVGModelObject() const { return false; }
    virtual bool isRenderSVGBlock() const { return false; };
    virtual bool isSVGRoot() const { return false; }
    virtual bool isSVGContainer() const { return false; }
    virtual bool isSVGTransformableContainer() const { return false; }
    virtual bool isSVGViewportContainer() const { return false; }
    virtual bool isSVGGradientStop() const { return false; }
    virtual bool isSVGHiddenContainer() const { return false; }
    virtual bool isSVGPath() const { return false; }
    virtual bool isSVGShape() const { return false; }
    virtual bool isSVGText() const { return false; }
    virtual bool isSVGTextPath() const { return false; }
    virtual bool isSVGTSpan() const { return false; }
    virtual bool isSVGInline() const { return false; }
    virtual bool isSVGInlineText() const { return false; }
    virtual bool isSVGImage() const { return false; }
    virtual bool isSVGForeignObject() const { return false; }
    virtual bool isSVGResourceContainer() const { return false; }
    virtual bool isSVGResourceFilter() const { return false; }
    virtual bool isSVGResourceClipper() const { return false; }
    virtual bool isSVGResourceFilterPrimitive() const { return false; }

    // FIXME: Those belong into a SVG specific base-class for all renderers (see above)
    // Unfortunately we don't have such a class yet, because it's not possible for all renderers
    // to inherit from RenderSVGObject -> RenderObject (some need RenderBlock inheritance for instance)
    virtual void setNeedsTransformUpdate() { }
    virtual void setNeedsBoundariesUpdate();
    virtual bool needsBoundariesUpdate() { return false; }

    // Per SVG 1.1 objectBoundingBox ignores clipping, masking, filter effects, opacity and stroke-width.
    // This is used for all computation of objectBoundingBox relative units and by SVGLocatable::getBBox().
    // NOTE: Markers are not specifically ignored here by SVG 1.1 spec, but we ignore them
    // since stroke-width is ignored (and marker size can depend on stroke-width).
    // objectBoundingBox is returned local coordinates.
    // The name objectBoundingBox is taken from the SVG 1.1 spec.
    virtual FloatRect objectBoundingBox() const;
    virtual FloatRect strokeBoundingBox() const;

    // Returns the smallest rectangle enclosing all of the painted content
    // respecting clipping, masking, filters, opacity, stroke-width and markers
    virtual FloatRect repaintRectInLocalCoordinates() const;

    // This only returns the transform="" value from the element
    // most callsites want localToParentTransform() instead.
    virtual AffineTransform localTransform() const;

    // Returns the full transform mapping from local coordinates to local coords for the parent SVG renderer
    // This includes any viewport transforms and x/y offsets as well as the transform="" value off the element.
    virtual const AffineTransform& localToParentTransform() const;

    // SVG uses FloatPoint precise hit testing, and passes the point in parent
    // coordinates instead of in repaint container coordinates.  Eventually the
    // rest of the rendering tree will move to a similar model.
    virtual bool nodeAtFloatPoint(const HitTestRequest&, HitTestResult&, const FloatPoint& pointInParent, HitTestAction);

    bool hasAspectRatio() const { return isReplaced() && (isImage() || isVideo() || isCanvas()); }
    bool isAnonymous() const { return m_bitfields.isAnonymous(); }
    bool isAnonymousBlock() const
    {
        // This function is kept in sync with anonymous block creation conditions in
        // RenderBlock::createAnonymousBlock(). This includes creating an anonymous
        // RenderBlock having a DisplayType::Block or DisplayType::Box display. Other classes such as RenderTextFragment
        // are not RenderBlocks and will return false. See https://bugs.webkit.org/show_bug.cgi?id=56709. 
        return isAnonymous() && (style().display() == DisplayType::Block || style().display() == DisplayType::Box) && style().styleType() == PseudoId::None && isRenderBlock() && !isListMarker() && !isRenderFragmentedFlow() && !isRenderMultiColumnSet() && !isRenderView()
#if ENABLE(FULLSCREEN_API)
            && !isRenderFullScreen()
            && !isRenderFullScreenPlaceholder()
#endif
#if ENABLE(MATHML)
            && !isRenderMathMLBlock()
#endif
            ;
    }

    bool isFloating() const { return m_bitfields.floating(); }

    bool isPositioned() const { return m_bitfields.isPositioned(); }
    bool isInFlowPositioned() const { return m_bitfields.isRelativelyPositioned() || m_bitfields.isStickilyPositioned(); }
    bool isOutOfFlowPositioned() const { return m_bitfields.isOutOfFlowPositioned(); } // absolute or fixed positioning
    bool isFixedPositioned() const { return isOutOfFlowPositioned() && style().position() == PositionType::Fixed; }
    bool isAbsolutelyPositioned() const { return isOutOfFlowPositioned() && style().position() == PositionType::Absolute; }
    bool isRelativelyPositioned() const { return m_bitfields.isRelativelyPositioned(); }
    bool isStickilyPositioned() const { return m_bitfields.isStickilyPositioned(); }

    bool isText() const  { return !m_bitfields.isBox() && m_bitfields.isTextOrRenderView(); }
    bool isLineBreak() const { return m_bitfields.isLineBreak(); }
    bool isBR() const { return isLineBreak() && !isWBR(); }
    bool isLineBreakOpportunity() const { return isLineBreak() && isWBR(); }
    bool isTextOrLineBreak() const { return isText() || isLineBreak(); }
    bool isBox() const { return m_bitfields.isBox(); }
    bool isRenderView() const  { return m_bitfields.isBox() && m_bitfields.isTextOrRenderView(); }
    bool isInline() const { return m_bitfields.isInline(); } // inline object
    bool isReplaced() const { return m_bitfields.isReplaced(); } // a "replaced" element (see CSS)
    bool isHorizontalWritingMode() const { return m_bitfields.horizontalWritingMode(); }

    bool isDragging() const { return m_bitfields.hasRareData() && rareData().isDragging(); }
    bool hasReflection() const { return m_bitfields.hasRareData() && rareData().hasReflection(); }
    bool isRenderFragmentedFlow() const { return m_bitfields.hasRareData() && rareData().isRenderFragmentedFlow(); }
    bool hasOutlineAutoAncestor() const { return m_bitfields.hasRareData() && rareData().hasOutlineAutoAncestor(); }

    bool isExcludedFromNormalLayout() const { return m_bitfields.isExcludedFromNormalLayout(); }
    void setIsExcludedFromNormalLayout(bool excluded) { m_bitfields.setIsExcludedFromNormalLayout(excluded); }
    bool isExcludedAndPlacedInBorder() const { return isExcludedFromNormalLayout() && isLegend(); }

    bool hasLayer() const { return m_bitfields.hasLayer(); }

    enum BoxDecorationState {
        NoBoxDecorations,
        HasBoxDecorationsAndBackgroundObscurationStatusInvalid,
        HasBoxDecorationsAndBackgroundIsKnownToBeObscured,
        HasBoxDecorationsAndBackgroundMayBeVisible,
    };
    bool hasVisibleBoxDecorations() const { return m_bitfields.boxDecorationState() != NoBoxDecorations; }
    bool backgroundIsKnownToBeObscured(const LayoutPoint& paintOffset);

    bool needsLayout() const
    {
        return m_bitfields.needsLayout() || m_bitfields.normalChildNeedsLayout() || m_bitfields.posChildNeedsLayout()
            || m_bitfields.needsSimplifiedNormalFlowLayout() || m_bitfields.needsPositionedMovementLayout();
    }

    bool selfNeedsLayout() const { return m_bitfields.needsLayout(); }
    bool needsPositionedMovementLayout() const { return m_bitfields.needsPositionedMovementLayout(); }
    bool needsPositionedMovementLayoutOnly() const
    {
        return m_bitfields.needsPositionedMovementLayout() && !m_bitfields.needsLayout() && !m_bitfields.normalChildNeedsLayout()
            && !m_bitfields.posChildNeedsLayout() && !m_bitfields.needsSimplifiedNormalFlowLayout();
    }

    bool posChildNeedsLayout() const { return m_bitfields.posChildNeedsLayout(); }
    bool needsSimplifiedNormalFlowLayout() const { return m_bitfields.needsSimplifiedNormalFlowLayout(); }
    bool needsSimplifiedNormalFlowLayoutOnly() const;
    bool normalChildNeedsLayout() const { return m_bitfields.normalChildNeedsLayout(); }
    
    bool preferredLogicalWidthsDirty() const { return m_bitfields.preferredLogicalWidthsDirty(); }

    bool isSelectionBorder() const;

    bool hasOverflowClip() const { return m_bitfields.hasOverflowClip(); }

    bool hasTransformRelatedProperty() const { return m_bitfields.hasTransformRelatedProperty(); } // Transform, perspective or transform-style: preserve-3d.
    bool hasTransform() const { return hasTransformRelatedProperty() && style().hasTransform(); }

    inline bool preservesNewline() const;

    virtual void updateDragState(bool dragOn);

    RenderView& view() const { return *document().renderView(); };

    // Returns true if this renderer is rooted.
    bool isRooted() const;

    Node* node() const { return isAnonymous() ? nullptr : &m_node; }
    Node* nonPseudoNode() const { return isPseudoElement() ? nullptr : node(); }

    // Returns the styled node that caused the generation of this renderer.
    // This is the same as node() except for renderers of :before and :after
    // pseudo elements for which their parent node is returned.
    Node* generatingNode() const { return isPseudoElement() ? generatingPseudoHostElement() : node(); }

    Document& document() const { return m_node.document(); }
    Frame& frame() const;
    Page& page() const;
    Settings& settings() const { return page().settings(); }

    // Returns the object containing this one. Can be different from parent for positioned elements.
    // If repaintContainer and repaintContainerSkipped are not null, on return *repaintContainerSkipped
    // is true if the renderer returned is an ancestor of repaintContainer.
    RenderElement* container() const;
    RenderElement* container(const RenderLayerModelObject* repaintContainer, bool& repaintContainerSkipped) const;

    RenderBoxModelObject* offsetParent() const;

    void markContainingBlocksForLayout(ScheduleRelayout = ScheduleRelayout::Yes, RenderElement* newRoot = nullptr);
    void setNeedsLayout(MarkingBehavior = MarkContainingBlockChain);
    void clearNeedsLayout();
    void setPreferredLogicalWidthsDirty(bool, MarkingBehavior = MarkContainingBlockChain);
    void invalidateContainerPreferredLogicalWidths();
    
    void setNeedsLayoutAndPrefWidthsRecalc()
    {
        setNeedsLayout();
        setPreferredLogicalWidthsDirty(true);
    }

    void setPositionState(PositionType position)
    {
        ASSERT((position != PositionType::Absolute && position != PositionType::Fixed) || isBox());
        m_bitfields.setPositionedState(static_cast<int>(position));
    }
    void clearPositionedState() { m_bitfields.clearPositionedState(); }

    void setFloating(bool b = true) { m_bitfields.setFloating(b); }
    void setInline(bool b = true) { m_bitfields.setIsInline(b); }

    void setHasVisibleBoxDecorations(bool = true);
    void invalidateBackgroundObscurationStatus();
    virtual bool computeBackgroundIsKnownToBeObscured(const LayoutPoint&) { return false; }

    void setIsText() { ASSERT(!isBox()); m_bitfields.setIsTextOrRenderView(true); }
    void setIsLineBreak() { m_bitfields.setIsLineBreak(true); }
    void setIsBox() { m_bitfields.setIsBox(true); }
    void setIsRenderView() { ASSERT(isBox()); m_bitfields.setIsTextOrRenderView(true); }
    void setReplaced(bool b = true) { m_bitfields.setIsReplaced(b); }
    void setHorizontalWritingMode(bool b = true) { m_bitfields.setHorizontalWritingMode(b); }
    void setHasOverflowClip(bool b = true) { m_bitfields.setHasOverflowClip(b); }
    void setHasLayer(bool b = true) { m_bitfields.setHasLayer(b); }
    void setHasTransformRelatedProperty(bool b = true) { m_bitfields.setHasTransformRelatedProperty(b); }

    void setIsDragging(bool);
    void setHasReflection(bool = true);
    void setIsRenderFragmentedFlow(bool = true);
    void setHasOutlineAutoAncestor(bool = true);

    // Hook so that RenderTextControl can return the line height of its inner renderer.
    // For other renderers, the value is the same as lineHeight(false).
    virtual int innerLineHeight() const;

    // used for element state updates that cannot be fixed with a
    // repaint and do not need a relayout
    virtual void updateFromElement() { }

#if ENABLE(DASHBOARD_SUPPORT)
    virtual void addAnnotatedRegions(Vector<AnnotatedRegionValue>&);
    void collectAnnotatedRegions(Vector<AnnotatedRegionValue>&);
#endif

    bool isComposited() const;

    bool hitTest(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestFilter = HitTestAll);
    virtual void updateHitTestResult(HitTestResult&, const LayoutPoint&);
    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction);

    virtual Position positionForPoint(const LayoutPoint&);
    virtual VisiblePosition positionForPoint(const LayoutPoint&, const RenderFragmentContainer*);
    VisiblePosition createVisiblePosition(int offset, EAffinity) const;
    VisiblePosition createVisiblePosition(const Position&) const;

    // returns the containing block level element for this element.
    WEBCORE_EXPORT RenderBlock* containingBlock() const;
    RenderBlock* containingBlockForObjectInFlow() const;

    // Convert the given local point to absolute coordinates. If MapCoordinatesFlags includes UseTransforms, take transforms into account.
    WEBCORE_EXPORT FloatPoint localToAbsolute(const FloatPoint& localPoint = FloatPoint(), MapCoordinatesFlags = 0, bool* wasFixed = nullptr) const;
    FloatPoint absoluteToLocal(const FloatPoint&, MapCoordinatesFlags = 0) const;

    // Convert a local quad to absolute coordinates, taking transforms into account.
    FloatQuad localToAbsoluteQuad(const FloatQuad& quad, MapCoordinatesFlags mode = UseTransforms, bool* wasFixed = nullptr) const
    {
        return localToContainerQuad(quad, nullptr, mode, wasFixed);
    }
    // Convert an absolute quad to local coordinates.
    FloatQuad absoluteToLocalQuad(const FloatQuad&, MapCoordinatesFlags mode = UseTransforms) const;

    // Convert a local quad into the coordinate system of container, taking transforms into account.
    WEBCORE_EXPORT FloatQuad localToContainerQuad(const FloatQuad&, const RenderLayerModelObject* repaintContainer, MapCoordinatesFlags = UseTransforms, bool* wasFixed = nullptr) const;
    WEBCORE_EXPORT FloatPoint localToContainerPoint(const FloatPoint&, const RenderLayerModelObject* repaintContainer, MapCoordinatesFlags = UseTransforms, bool* wasFixed = nullptr) const;

    // Return the offset from the container() renderer (excluding transforms). In multi-column layout,
    // different offsets apply at different points, so return the offset that applies to the given point.
    virtual LayoutSize offsetFromContainer(RenderElement&, const LayoutPoint&, bool* offsetDependsOnPoint = nullptr) const;
    // Return the offset from an object up the container() chain. Asserts that none of the intermediate objects have transforms.
    LayoutSize offsetFromAncestorContainer(RenderElement&) const;

#if PLATFORM(IOS_FAMILY)
    virtual void collectSelectionRects(Vector<SelectionRect>&, unsigned startOffset = 0, unsigned endOffset = std::numeric_limits<unsigned>::max());
    virtual void absoluteQuadsForSelection(Vector<FloatQuad>& quads) const { absoluteQuads(quads); }
#endif

    virtual void absoluteRects(Vector<IntRect>&, const LayoutPoint&) const { }

    // FIXME: useTransforms should go away eventually
    WEBCORE_EXPORT IntRect absoluteBoundingBoxRect(bool useTransform = true, bool* wasFixed = nullptr) const;
    IntRect absoluteBoundingBoxRectIgnoringTransforms() const { return absoluteBoundingBoxRect(false); }

    // Build an array of quads in absolute coords for line boxes
    virtual void absoluteQuads(Vector<FloatQuad>&, bool* /*wasFixed*/ = nullptr) const { }

    virtual void absoluteFocusRingQuads(Vector<FloatQuad>&);

    static FloatRect absoluteBoundingBoxRectForRange(const Range*);

    // the rect that will be painted if this object is passed as the paintingRoot
    WEBCORE_EXPORT LayoutRect paintingRootRect(LayoutRect& topLevelRect);

    virtual LayoutUnit minPreferredLogicalWidth() const { return 0; }
    virtual LayoutUnit maxPreferredLogicalWidth() const { return 0; }

    const RenderStyle& style() const;
    const RenderStyle& firstLineStyle() const;

    // Anonymous blocks that are part of of a continuation chain will return their inline continuation's outline style instead.
    // This is typically only relevant when repainting.
    virtual const RenderStyle& outlineStyleForRepaint() const { return style(); }
    
    virtual CursorDirective getCursor(const LayoutPoint&, Cursor&) const;

    // Return the RenderLayerModelObject in the container chain which is responsible for painting this object, or nullptr
    // if painting is root-relative. This is the container that should be passed to the 'forRepaint'
    // methods.
    RenderLayerModelObject* containerForRepaint() const;
    // Actually do the repaint of rect r for this object which has been computed in the coordinate space
    // of repaintContainer. If repaintContainer is nullptr, repaint via the view.
    void repaintUsingContainer(const RenderLayerModelObject* repaintContainer, const LayoutRect&, bool shouldClipToLayer = true) const;
    
    // Repaint the entire object.  Called when, e.g., the color of a border changes, or when a border
    // style changes.
    void repaint() const;

    // Repaint a specific subrectangle within a given object.  The rect |r| is in the object's coordinate space.
    WEBCORE_EXPORT void repaintRectangle(const LayoutRect&, bool shouldClipToLayer = true) const;

    // Repaint a slow repaint object, which, at this time, means we are repainting an object with background-attachment:fixed.
    void repaintSlowRepaintObject() const;

    // Returns the rect that should be repainted whenever this object changes.  The rect is in the view's
    // coordinate space.  This method deals with outlines and overflow.
    LayoutRect absoluteClippedOverflowRect() const
    {
        return clippedOverflowRectForRepaint(nullptr);
    }
    WEBCORE_EXPORT IntRect pixelSnappedAbsoluteClippedOverflowRect() const;
    virtual LayoutRect clippedOverflowRectForRepaint(const RenderLayerModelObject* repaintContainer) const;
    virtual LayoutRect rectWithOutlineForRepaint(const RenderLayerModelObject* repaintContainer, LayoutUnit outlineWidth) const;
    virtual LayoutRect outlineBoundsForRepaint(const RenderLayerModelObject* /*repaintContainer*/, const RenderGeometryMap* = nullptr) const { return LayoutRect(); }

    // Given a rect in the object's coordinate space, compute a rect suitable for repainting
    // that rect in view coordinates.
    LayoutRect computeAbsoluteRepaintRect(const LayoutRect& r) const
    {
        return computeRectForRepaint(r, nullptr);
    }
    // Given a rect in the object's coordinate space, compute a rect suitable for repainting
    // that rect in the coordinate space of repaintContainer.
    LayoutRect computeRectForRepaint(const LayoutRect&, const RenderLayerModelObject* repaintContainer) const;
    FloatRect computeFloatRectForRepaint(const FloatRect&, const RenderLayerModelObject* repaintContainer) const;

    // Given a rect in the object's coordinate space, compute the location in container space where this rect is visible,
    // when clipping and scrolling as specified by the context. When using edge-inclusive intersection, return WTF::nullopt
    // rather than an empty rect if the rect is completely clipped out in container space.
    enum class VisibleRectContextOption {
        UseEdgeInclusiveIntersection = 1 << 0,
        ApplyCompositedClips = 1 << 1,
        ApplyCompositedContainerScrolls  = 1 << 2,
    };
    struct VisibleRectContext {
        VisibleRectContext(bool hasPositionFixedDescendant = false, bool dirtyRectIsFlipped = false, OptionSet<VisibleRectContextOption> options = { })
            : m_hasPositionFixedDescendant(hasPositionFixedDescendant)
            , m_dirtyRectIsFlipped(dirtyRectIsFlipped)
            , m_options(options)
            {
            }
        bool m_hasPositionFixedDescendant;
        bool m_dirtyRectIsFlipped;
        OptionSet<VisibleRectContextOption> m_options;
    };
    virtual Optional<LayoutRect> computeVisibleRectInContainer(const LayoutRect&, const RenderLayerModelObject* repaintContainer, VisibleRectContext) const;
    virtual Optional<FloatRect> computeFloatVisibleRectInContainer(const FloatRect&, const RenderLayerModelObject* repaintContainer, VisibleRectContext) const;

    virtual unsigned int length() const { return 1; }

    bool isFloatingOrOutOfFlowPositioned() const { return (isFloating() || isOutOfFlowPositioned()); }

    enum SelectionState {
        SelectionNone, // The object is not selected.
        SelectionStart, // The object either contains the start of a selection run or is the start of a run
        SelectionInside, // The object is fully encompassed by a selection run
        SelectionEnd, // The object either contains the end of a selection run or is the end of a run
        SelectionBoth // The object contains an entire run or is the sole selected object in that run
    };

    // The current selection state for an object.  For blocks, the state refers to the state of the leaf
    // descendants (as described above in the SelectionState enum declaration).
    SelectionState selectionState() const { return m_bitfields.selectionState(); }
    virtual void setSelectionState(SelectionState state) { m_bitfields.setSelectionState(state); }
    inline void setSelectionStateIfNeeded(SelectionState);
    bool canUpdateSelectionOnRootLineBoxes();

    // A single rectangle that encompasses all of the selected objects within this object.  Used to determine the tightest
    // possible bounding box for the selection.
    LayoutRect selectionRect(bool clipToVisibleContent = true) { return selectionRectForRepaint(nullptr, clipToVisibleContent); }
    virtual LayoutRect selectionRectForRepaint(const RenderLayerModelObject* /*repaintContainer*/, bool /*clipToVisibleContent*/ = true) { return LayoutRect(); }

    virtual bool canBeSelectionLeaf() const { return false; }

    // Whether or not a given block needs to paint selection gaps.
    virtual bool shouldPaintSelectionGaps() const { return false; }

    /**
     * Returns the local coordinates of the caret within this render object.
     * @param caretOffset zero-based offset determining position within the render object.
     * @param extraWidthToEndOfLine optional out arg to give extra width to end of line -
     * useful for character range rect computations
     */
    virtual LayoutRect localCaretRect(InlineBox*, unsigned caretOffset, LayoutUnit* extraWidthToEndOfLine = nullptr);

    // When performing a global document tear-down, or when going into the page cache, the renderer of the document is cleared.
    bool renderTreeBeingDestroyed() const;

    void destroy();

    // Virtual function helpers for the deprecated Flexible Box Layout (display: -webkit-box).
    virtual bool isDeprecatedFlexibleBox() const { return false; }

    // Virtual function helper for the new FlexibleBox Layout (display: -webkit-flex).
    virtual bool isFlexibleBox() const { return false; }

    bool isFlexibleBoxIncludingDeprecated() const
    {
        return isFlexibleBox() || isDeprecatedFlexibleBox();
    }

    virtual bool isCombineText() const { return false; }

    virtual int caretMinOffset() const;
    virtual int caretMaxOffset() const;

    virtual int previousOffset(int current) const;
    virtual int previousOffsetForBackwardDeletion(int current) const;
    virtual int nextOffset(int current) const;

    void imageChanged(CachedImage*, const IntRect* = nullptr) override;
    virtual void imageChanged(WrappedImagePtr, const IntRect* = nullptr) { }

    CSSAnimationController& animation() const;
    DocumentTimeline* documentTimeline() const;

    // Map points and quads through elements, potentially via 3d transforms. You should never need to call these directly; use
    // localToAbsolute/absoluteToLocal methods instead.
    virtual void mapLocalToContainer(const RenderLayerModelObject* repaintContainer, TransformState&, MapCoordinatesFlags, bool* wasFixed = nullptr) const;
    virtual void mapAbsoluteToLocalPoint(MapCoordinatesFlags, TransformState&) const;

    // Pushes state onto RenderGeometryMap about how to map coordinates from this renderer to its container, or ancestorToStopAt (whichever is encountered first).
    // Returns the renderer which was mapped to (container or ancestorToStopAt).
    virtual const RenderObject* pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap&) const;
    
    bool shouldUseTransformFromContainer(const RenderObject* container) const;
    void getTransformFromContainer(const RenderObject* container, const LayoutSize& offsetInContainer, TransformationMatrix&) const;
    
    virtual void addFocusRingRects(Vector<LayoutRect>&, const LayoutPoint& /* additionalOffset */, const RenderLayerModelObject* /* paintContainer */ = nullptr) { };

    LayoutRect absoluteOutlineBounds() const
    {
        return outlineBoundsForRepaint(nullptr);
    }

    virtual void willBeRemovedFromTree();
    void resetFragmentedFlowStateOnRemoval();
    void initializeFragmentedFlowStateOnInsertion();
    virtual void insertedIntoTree();

protected:
    //////////////////////////////////////////
    // Helper functions. Dangerous to use!
    void setPreviousSibling(RenderObject* previous) { m_previous = previous; }
    void setNextSibling(RenderObject* next) { m_next = next; }
    void setParent(RenderElement*);
    //////////////////////////////////////////
    void addPDFURLRect(PaintInfo&, const LayoutPoint&);
    Node& nodeForNonAnonymous() const { ASSERT(!isAnonymous()); return m_node; }

    void adjustRectForOutlineAndShadow(LayoutRect&) const;

    virtual void willBeDestroyed();

    void setNeedsPositionedMovementLayoutBit(bool b) { m_bitfields.setNeedsPositionedMovementLayout(b); }
    void setNormalChildNeedsLayoutBit(bool b) { m_bitfields.setNormalChildNeedsLayout(b); }
    void setPosChildNeedsLayoutBit(bool b) { m_bitfields.setPosChildNeedsLayout(b); }
    void setNeedsSimplifiedNormalFlowLayoutBit(bool b) { m_bitfields.setNeedsSimplifiedNormalFlowLayout(b); }

    virtual RenderFragmentedFlow* locateEnclosingFragmentedFlow() const;
    static void calculateBorderStyleColor(const BorderStyle&, const BoxSide&, Color&);

    static FragmentedFlowState computedFragmentedFlowState(const RenderObject&);

    static bool shouldApplyCompositedContainerScrollsForRepaint();

    static VisibleRectContext visibleRectContextForRepaint();

private:
#ifndef NDEBUG
    bool isSetNeedsLayoutForbidden() const { return m_setNeedsLayoutForbidden; }
    void setNeedsLayoutIsForbidden(bool flag) { m_setNeedsLayoutForbidden = flag; }
#endif

    void addAbsoluteRectForLayer(LayoutRect& result);
    void setLayerNeedsFullRepaint();
    void setLayerNeedsFullRepaintForPositionedMovementLayout();

    Node* generatingPseudoHostElement() const;

    void propagateRepaintToParentWithOutlineAutoIfNeeded(const RenderLayerModelObject& repaintContainer, const LayoutRect& repaintRect) const;

    virtual bool isWBR() const { ASSERT_NOT_REACHED(); return false; }

    void setEverHadLayout(bool b) { m_bitfields.setEverHadLayout(b); }

    bool hasRareData() const { return m_bitfields.hasRareData(); }
    void setHasRareData(bool b) { m_bitfields.setHasRareData(b); }

#ifndef NDEBUG
    void checkBlockPositionedObjectsNeedLayout();
#endif

    Node& m_node;

    RenderElement* m_parent;
    RenderObject* m_previous;
    RenderObject* m_next;

#ifndef NDEBUG
    bool m_hasAXObject             : 1;
    bool m_setNeedsLayoutForbidden : 1;
#endif

#define ADD_BOOLEAN_BITFIELD(name, Name) \
    private:\
        unsigned m_##name : 1;\
    public:\
        bool name() const { return m_##name; }\
        void set##Name(bool name) { m_##name = name; }\

#define ADD_ENUM_BITFIELD(name, Name, Type, width) \
    private:\
        unsigned m_##name : width;\
    public:\
        Type name() const { return static_cast<Type>(m_##name); }\
        void set##Name(Type name) { m_##name = static_cast<unsigned>(name); }\

    class RenderObjectBitfields {
        enum PositionedState {
            IsStaticallyPositioned = 0,
            IsRelativelyPositioned = 1,
            IsOutOfFlowPositioned = 2,
            IsStickilyPositioned = 3
        };

    public:
        RenderObjectBitfields(const Node& node)
            : m_hasRareData(false)
            , m_beingDestroyed(false)
            , m_needsLayout(false)
            , m_needsPositionedMovementLayout(false)
            , m_normalChildNeedsLayout(false)
            , m_posChildNeedsLayout(false)
            , m_needsSimplifiedNormalFlowLayout(false)
            , m_preferredLogicalWidthsDirty(false)
            , m_floating(false)
            , m_isAnonymous(node.isDocumentNode())
            , m_isTextOrRenderView(false)
            , m_isBox(false)
            , m_isInline(true)
            , m_isReplaced(false)
            , m_isLineBreak(false)
            , m_horizontalWritingMode(true)
            , m_hasLayer(false)
            , m_hasOverflowClip(false)
            , m_hasTransformRelatedProperty(false)
            , m_everHadLayout(false)
            , m_childrenInline(false)
            , m_isExcludedFromNormalLayout(false)
            , m_positionedState(IsStaticallyPositioned)
            , m_selectionState(SelectionNone)
            , m_fragmentedFlowState(NotInsideFragmentedFlow)
            , m_boxDecorationState(NoBoxDecorations)
        {
        }

        ADD_BOOLEAN_BITFIELD(hasRareData, HasRareData);
        
        ADD_BOOLEAN_BITFIELD(beingDestroyed, BeingDestroyed);
        ADD_BOOLEAN_BITFIELD(needsLayout, NeedsLayout);
        ADD_BOOLEAN_BITFIELD(needsPositionedMovementLayout, NeedsPositionedMovementLayout);
        ADD_BOOLEAN_BITFIELD(normalChildNeedsLayout, NormalChildNeedsLayout);
        ADD_BOOLEAN_BITFIELD(posChildNeedsLayout, PosChildNeedsLayout);
        ADD_BOOLEAN_BITFIELD(needsSimplifiedNormalFlowLayout, NeedsSimplifiedNormalFlowLayout);
        ADD_BOOLEAN_BITFIELD(preferredLogicalWidthsDirty, PreferredLogicalWidthsDirty);
        ADD_BOOLEAN_BITFIELD(floating, Floating);

        ADD_BOOLEAN_BITFIELD(isAnonymous, IsAnonymous);
        ADD_BOOLEAN_BITFIELD(isTextOrRenderView, IsTextOrRenderView);
        ADD_BOOLEAN_BITFIELD(isBox, IsBox);
        ADD_BOOLEAN_BITFIELD(isInline, IsInline);
        ADD_BOOLEAN_BITFIELD(isReplaced, IsReplaced);
        ADD_BOOLEAN_BITFIELD(isLineBreak, IsLineBreak);
        ADD_BOOLEAN_BITFIELD(horizontalWritingMode, HorizontalWritingMode);

        ADD_BOOLEAN_BITFIELD(hasLayer, HasLayer);
        ADD_BOOLEAN_BITFIELD(hasOverflowClip, HasOverflowClip); // Set in the case of overflow:auto/scroll/hidden
        ADD_BOOLEAN_BITFIELD(hasTransformRelatedProperty, HasTransformRelatedProperty);
        ADD_BOOLEAN_BITFIELD(unused, Unused);

        ADD_BOOLEAN_BITFIELD(everHadLayout, EverHadLayout);

        // from RenderBlock
        ADD_BOOLEAN_BITFIELD(childrenInline, ChildrenInline);
        
        ADD_BOOLEAN_BITFIELD(isExcludedFromNormalLayout, IsExcludedFromNormalLayout);

    private:
        unsigned m_positionedState : 2; // PositionedState
        unsigned m_selectionState : 3; // SelectionState
        unsigned m_fragmentedFlowState : 2; // FragmentedFlowState
        unsigned m_boxDecorationState : 2; // BoxDecorationState

    public:
        bool isOutOfFlowPositioned() const { return m_positionedState == IsOutOfFlowPositioned; }
        bool isRelativelyPositioned() const { return m_positionedState == IsRelativelyPositioned; }
        bool isStickilyPositioned() const { return m_positionedState == IsStickilyPositioned; }
        bool isPositioned() const { return m_positionedState != IsStaticallyPositioned; }

        void setPositionedState(int positionState)
        {
            // This mask maps PositionType::Fixed and PositionType::Absolute to IsOutOfFlowPositioned, saving one bit.
            m_positionedState = static_cast<PositionedState>(positionState & 0x3);
        }
        void clearPositionedState() { m_positionedState = static_cast<unsigned>(PositionType::Static); }

        ALWAYS_INLINE SelectionState selectionState() const { return static_cast<SelectionState>(m_selectionState); }
        ALWAYS_INLINE void setSelectionState(SelectionState selectionState) { m_selectionState = selectionState; }
        
        ALWAYS_INLINE FragmentedFlowState fragmentedFlowState() const { return static_cast<FragmentedFlowState>(m_fragmentedFlowState); }
        ALWAYS_INLINE void setFragmentedFlowState(FragmentedFlowState fragmentedFlowState) { m_fragmentedFlowState = fragmentedFlowState; }

        ALWAYS_INLINE BoxDecorationState boxDecorationState() const { return static_cast<BoxDecorationState>(m_boxDecorationState); }
        ALWAYS_INLINE void setBoxDecorationState(BoxDecorationState boxDecorationState) { m_boxDecorationState = boxDecorationState; }
    };

    RenderObjectBitfields m_bitfields;

    // FIXME: This should be RenderElementRareData.
    class RenderObjectRareData {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        RenderObjectRareData()
            : m_isDragging(false)
            , m_hasReflection(false)
            , m_isRenderFragmentedFlow(false)
            , m_hasOutlineAutoAncestor(false)
        {
        }
        ADD_BOOLEAN_BITFIELD(isDragging, IsDragging);
        ADD_BOOLEAN_BITFIELD(hasReflection, HasReflection);
        ADD_BOOLEAN_BITFIELD(isRenderFragmentedFlow, IsRenderFragmentedFlow);
        ADD_BOOLEAN_BITFIELD(hasOutlineAutoAncestor, HasOutlineAutoAncestor);

        // From RenderElement
        std::unique_ptr<RenderStyle> cachedFirstLineStyle;
    };
    
    const RenderObject::RenderObjectRareData& rareData() const;
    RenderObjectRareData& ensureRareData();
    void removeRareData();
    
    typedef HashMap<const RenderObject*, std::unique_ptr<RenderObjectRareData>> RareDataMap;

    static RareDataMap& rareDataMap();

#undef ADD_BOOLEAN_BITFIELD
};

inline Frame& RenderObject::frame() const
{
    return *document().frame();
}

inline Page& RenderObject::page() const
{
    // The render tree will always be torn down before Frame is disconnected from Page,
    // so it's safe to assume Frame::page() is non-null as long as there are live RenderObjects.
    ASSERT(frame().page());
    return *frame().page();
}

inline CSSAnimationController& RenderObject::animation() const
{
    return frame().animation();
}

inline DocumentTimeline* RenderObject::documentTimeline() const
{
    return document().existingTimeline();
}

inline bool RenderObject::renderTreeBeingDestroyed() const
{
    return document().renderTreeBeingDestroyed();
}

inline bool RenderObject::isBeforeContent() const
{
    // Text nodes don't have their own styles, so ignore the style on a text node.
    if (isText())
        return false;
    if (style().styleType() != PseudoId::Before)
        return false;
    return true;
}

inline bool RenderObject::isAfterContent() const
{
    // Text nodes don't have their own styles, so ignore the style on a text node.
    if (isText())
        return false;
    if (style().styleType() != PseudoId::After)
        return false;
    return true;
}

inline bool RenderObject::isBeforeOrAfterContent() const
{
    return isBeforeContent() || isAfterContent();
}

inline void RenderObject::setNeedsLayout(MarkingBehavior markParents)
{
    ASSERT(!isSetNeedsLayoutForbidden());
    if (m_bitfields.needsLayout())
        return;
    m_bitfields.setNeedsLayout(true);
    if (markParents == MarkContainingBlockChain)
        markContainingBlocksForLayout();
    if (hasLayer())
        setLayerNeedsFullRepaint();
}

inline bool RenderObject::preservesNewline() const
{
    if (isSVGInlineText())
        return false;
        
    return style().preserveNewline();
}

inline void RenderObject::setSelectionStateIfNeeded(SelectionState state)
{
    if (selectionState() == state)
        return;

    setSelectionState(state);
}

inline void RenderObject::setHasVisibleBoxDecorations(bool b)
{
    if (!b) {
        m_bitfields.setBoxDecorationState(NoBoxDecorations);
        return;
    }
    if (hasVisibleBoxDecorations())
        return;
    m_bitfields.setBoxDecorationState(HasBoxDecorationsAndBackgroundObscurationStatusInvalid);
}

inline void RenderObject::invalidateBackgroundObscurationStatus()
{
    if (!hasVisibleBoxDecorations())
        return;
    m_bitfields.setBoxDecorationState(HasBoxDecorationsAndBackgroundObscurationStatusInvalid);
}

inline bool RenderObject::backgroundIsKnownToBeObscured(const LayoutPoint& paintOffset)
{
    if (m_bitfields.boxDecorationState() == HasBoxDecorationsAndBackgroundObscurationStatusInvalid) {
        BoxDecorationState boxDecorationState = computeBackgroundIsKnownToBeObscured(paintOffset) ? HasBoxDecorationsAndBackgroundIsKnownToBeObscured : HasBoxDecorationsAndBackgroundMayBeVisible;
        m_bitfields.setBoxDecorationState(boxDecorationState);
    }
    return m_bitfields.boxDecorationState() == HasBoxDecorationsAndBackgroundIsKnownToBeObscured;
}

inline bool RenderObject::needsSimplifiedNormalFlowLayoutOnly() const
{
    return m_bitfields.needsSimplifiedNormalFlowLayout() && !m_bitfields.needsLayout() && !m_bitfields.normalChildNeedsLayout()
        && !m_bitfields.posChildNeedsLayout() && !m_bitfields.needsPositionedMovementLayout();
}

#if ENABLE(TREE_DEBUGGING)
void printRenderTreeForLiveDocuments();
void printLayerTreeForLiveDocuments();
void printGraphicsLayerTreeForLiveDocuments();
#endif

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName) \
    static bool isType(const WebCore::RenderObject& renderer) { return renderer.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

#if ENABLE(TREE_DEBUGGING)
// Outside the WebCore namespace for ease of invocation from the debugger.
void showNodeTree(const WebCore::RenderObject*);
void showLineTree(const WebCore::RenderObject*);
void showRenderTree(const WebCore::RenderObject*);
#endif
