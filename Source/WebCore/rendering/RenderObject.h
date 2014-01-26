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

#ifndef RenderObject_h
#define RenderObject_h

#include "CachedImageClient.h"
#include "DocumentStyleSheetCollection.h"
#include "Element.h"
#include "FloatQuad.h"
#include "LayoutRect.h"
#include "PaintPhase.h"
#include "RenderStyle.h"
#include "ScrollBehavior.h"
#include "StyleInheritedData.h"
#include "TextAffinity.h"
#include <wtf/HashSet.h>

namespace WebCore {

class AffineTransform;
class AnimationController;
class Cursor;
class Document;
class HitTestLocation;
class HitTestResult;
class InlineBox;
class Path;
class Position;
class PseudoStyleRequest;
class RenderBoxModelObject;
class RenderInline;
class RenderBlock;
class RenderElement;
class RenderFlowThread;
class RenderGeometryMap;
class RenderLayer;
class RenderLayerModelObject;
class RenderNamedFlowThread;
class RenderTheme;
class TransformState;
class VisiblePosition;
#if ENABLE(SVG)
class RenderSVGResourceContainer;
#endif
#if PLATFORM(IOS)
class SelectionRect;
#endif

struct PaintInfo;

enum CursorDirective {
    SetCursorBasedOnStyle,
    SetCursor,
    DoNotSetCursor
};

enum HitTestFilter {
    HitTestAll,
    HitTestSelf,
    HitTestDescendants
};

enum HitTestAction {
    HitTestBlockBackground,
    HitTestChildBlockBackground,
    HitTestChildBlockBackgrounds,
    HitTestFloat,
    HitTestForeground
};

// Sides used when drawing borders and outlines. The values should run clockwise from top.
enum BoxSide {
    BSTop,
    BSRight,
    BSBottom,
    BSLeft
};

enum MarkingBehavior {
    MarkOnlyThis,
    MarkContainingBlockChain,
};

enum MapCoordinatesMode {
    IsFixed = 1 << 0,
    UseTransforms = 1 << 1,
    ApplyContainerFlip = 1 << 2
};
typedef unsigned MapCoordinatesFlags;

#if PLATFORM(IOS)
const int caretWidth = 2; // This value should be kept in sync with UIKit. See <rdar://problem/15580601>.
#else
const int caretWidth = 1;
#endif

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

#ifndef NDEBUG
const int showTreeCharacterOffset = 39;
#endif

// Base class for all rendering tree objects.
class RenderObject : public CachedImageClient {
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
    RenderObject* childAt(unsigned) const;

    RenderObject* firstLeafChild() const;
    RenderObject* lastLeafChild() const;

#if ENABLE(IOS_TEXT_AUTOSIZING)
    // Minimal distance between the block with fixed height and overflowing content and the text block to apply text autosizing.
    // The greater this constant is the more potential places we have where autosizing is turned off.
    // So it should be as low as possible. There are sites that break at 2.
    static const int TextAutoSizingFixedHeightDepth = 3;

    enum BlockContentHeightType {
        FixedHeight,
        FlexibleHeight,
        OverflowHeight
    };

    RenderObject* traverseNext(const RenderObject* stayWithin) const;
    typedef bool (*TraverseNextInclusionFunction)(const RenderObject*);
    typedef BlockContentHeightType (*HeightTypeTraverseNextInclusionFunction)(const RenderObject*);

    RenderObject* traverseNext(const RenderObject* stayWithin, TraverseNextInclusionFunction) const;
    RenderObject* traverseNext(const RenderObject* stayWithin, HeightTypeTraverseNextInclusionFunction, int& currentDepth,  int& newFixedDepth) const;

    void adjustComputedFontSizesOnBlocks(float size, float visibleWidth);
    void resetTextAutosizing();
#endif

    RenderLayer* enclosingLayer() const;

    // Scrolling is a RenderBox concept, however some code just cares about recursively scrolling our enclosing ScrollableArea(s).
    bool scrollRectToVisible(const LayoutRect&, const ScrollAlignment& alignX = ScrollAlignment::alignCenterIfNeeded, const ScrollAlignment& alignY = ScrollAlignment::alignCenterIfNeeded);

    // Convenience function for getting to the nearest enclosing box of a RenderObject.
    RenderBox* enclosingBox() const;
    RenderBoxModelObject* enclosingBoxModelObject() const;

    bool fixedPositionedWithNamedFlowContainingBlock() const;
    // Function to return our enclosing flow thread if we are contained inside one. This
    // function follows the containing block chain.
    RenderFlowThread* flowThreadContainingBlock() const
    {
        if (flowThreadState() == NotInsideFlowThread)
            return 0;
        return locateFlowThreadContainingBlock();
    }

    RenderNamedFlowThread* renderNamedFlowThreadWrapper() const;

    // FIXME: The meaning of this function is unclear.
    virtual bool isEmpty() const { return !firstChildSlow(); }

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

    // Called when an object that was floating or positioned becomes a normal flow object
    // again.  We have to make sure the render tree updates as needed to accommodate the new
    // normal flow object.
    void handleDynamicFloatPositionChange();
    void removeAnonymousWrappersForInlinesIfNecessary();
    
    // RenderObject tree manipulation
    //////////////////////////////////////////
    virtual bool canHaveChildren() const = 0;
    virtual bool canHaveGeneratedChildren() const;
    virtual bool createsAnonymousWrapper() const { return false; }
    //////////////////////////////////////////

protected:
    //////////////////////////////////////////
    // Helper functions. Dangerous to use!
    void setPreviousSibling(RenderObject* previous) { m_previous = previous; }
    void setNextSibling(RenderObject* next) { m_next = next; }
    void setParent(RenderElement*);
    //////////////////////////////////////////
private:
#ifndef NDEBUG
    bool isSetNeedsLayoutForbidden() const { return m_setNeedsLayoutForbidden; }
    void setNeedsLayoutIsForbidden(bool flag) { m_setNeedsLayoutForbidden = flag; }
#endif

    void addAbsoluteRectForLayer(LayoutRect& result);
    void setLayerNeedsFullRepaint();
    void setLayerNeedsFullRepaintForPositionedMovementLayout();

public:
#ifndef NDEBUG
    void showTreeForThis() const;
    void showRenderTreeForThis() const;
    void showLineTreeForThis() const;

    void showRenderObject() const;
    // We don't make printedCharacters an optional parameter so that
    // showRenderObject can be called from gdb easily.
    void showRenderObject(int printedCharacters) const;
    void showRenderTreeAndMark(const RenderObject* markedObject1 = 0, const char* markedLabel1 = 0, const RenderObject* markedObject2 = 0, const char* markedLabel2 = 0, int depth = 0) const;
#endif

public:
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

#if ENABLE(DETAILS_ELEMENT)
    virtual bool isDetailsMarker() const { return false; }
#endif
    virtual bool isEmbeddedObject() const { return false; }
    virtual bool isFieldset() const { return false; }
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
#if ENABLE(PROGRESS_ELEMENT)
    virtual bool isProgress() const { return false; }
#endif
    virtual bool isRenderSVGBlock() const { return false; };
    virtual bool isRenderButton() const { return false; }
    virtual bool isRenderIFrame() const { return false; }
    virtual bool isRenderImage() const { return false; }
    virtual bool isRenderRegion() const { return false; }
    virtual bool isRenderNamedFlowFragment() const { return false; }
    virtual bool isReplica() const { return false; }

    virtual bool isRuby() const { return false; }
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
    virtual bool isTextControlInnerBlock() const { return false; }
    virtual bool isVideo() const { return false; }
    virtual bool isWidget() const { return false; }
    virtual bool isCanvas() const { return false; }
#if ENABLE(FULLSCREEN_API)
    virtual bool isRenderFullScreen() const { return false; }
    virtual bool isRenderFullScreenPlaceholder() const { return false; }
#endif

    virtual bool isRenderGrid() const { return false; }

    virtual bool isRenderFlowThread() const { return false; }
    virtual bool isRenderNamedFlowThread() const { return false; }
    bool isInFlowRenderFlowThread() const { return isRenderFlowThread() && !isOutOfFlowPositioned(); }
    bool isOutOfFlowRenderFlowThread() const { return isRenderFlowThread() && isOutOfFlowPositioned(); }

    virtual bool isMultiColumnBlockFlow() const { return false; }
    virtual bool isRenderMultiColumnSet() const { return false; }

    virtual bool isRenderScrollbarPart() const { return false; }

    bool isRoot() const { return document().documentElement() == &m_node; }
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

    bool hasCounterNodeMap() const { return m_bitfields.hasCounterNodeMap(); }
    void setHasCounterNodeMap(bool hasCounterNodeMap) { m_bitfields.setHasCounterNodeMap(hasCounterNodeMap); }
    bool everHadLayout() const { return m_bitfields.everHadLayout(); }

    bool childrenInline() const { return m_bitfields.childrenInline(); }
    void setChildrenInline(bool b) { m_bitfields.setChildrenInline(b); }
    bool hasColumns() const { return m_bitfields.hasColumns(); }
    void setHasColumns(bool b = true) { m_bitfields.setHasColumns(b); }

    enum FlowThreadState {
        NotInsideFlowThread = 0,
        InsideOutOfFlowThread = 1,
        InsideInFlowThread = 2,
    };

    void setFlowThreadStateIncludingDescendants(FlowThreadState);

    FlowThreadState flowThreadState() const { return m_bitfields.flowThreadState(); }
    void setFlowThreadState(FlowThreadState state) { m_bitfields.setFlowThreadState(state); }

    virtual bool requiresForcedStyleRecalcPropagation() const { return false; }

#if ENABLE(MATHML)
    virtual bool isRenderMathMLBlock() const { return false; }
    virtual bool isRenderMathMLOperator() const { return false; }
    virtual bool isRenderMathMLRow() const { return false; }
    virtual bool isRenderMathMLMath() const { return false; }
    virtual bool isRenderMathMLFenced() const { return false; }
    virtual bool isRenderMathMLFraction() const { return false; }
    virtual bool isRenderMathMLRoot() const { return false; }
    virtual bool isRenderMathMLSpace() const { return false; }
    virtual bool isRenderMathMLSquareRoot() const { return false; }
    virtual bool isRenderMathMLScripts() const { return false; }
    virtual bool isRenderMathMLScriptsWrapper() const { return false; }
    virtual bool isRenderMathMLUnderOver() const { return false; }
#endif // ENABLE(MATHML)

#if ENABLE(SVG)
    // FIXME: Until all SVG renders can be subclasses of RenderSVGModelObject we have
    // to add SVG renderer methods to RenderObject with an ASSERT_NOT_REACHED() default implementation.
    virtual bool isRenderSVGModelObject() const { return false; }
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
    virtual bool isSVGInline() const { return false; }
    virtual bool isSVGInlineText() const { return false; }
    virtual bool isSVGImage() const { return false; }
    virtual bool isSVGForeignObject() const { return false; }
    virtual bool isSVGResourceContainer() const { return false; }
    virtual bool isSVGResourceFilter() const { return false; }
    virtual bool isSVGResourceFilterPrimitive() const { return false; }

    virtual RenderSVGResourceContainer* toRenderSVGResourceContainer();

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
#endif

    bool hasAspectRatio() const { return isReplaced() && (isImage() || isVideo() || isCanvas()); }
    bool isAnonymous() const { return m_bitfields.isAnonymous(); }
    bool isAnonymousBlock() const
    {
        // This function is kept in sync with anonymous block creation conditions in
        // RenderBlock::createAnonymousBlock(). This includes creating an anonymous
        // RenderBlock having a BLOCK or BOX display. Other classes such as RenderTextFragment
        // are not RenderBlocks and will return false. See https://bugs.webkit.org/show_bug.cgi?id=56709. 
        return isAnonymous() && (style().display() == BLOCK || style().display() == BOX) && style().styleType() == NOPSEUDO && isRenderBlock() && !isListMarker() && !isRenderFlowThread() && !isRenderView()
#if ENABLE(FULLSCREEN_API)
            && !isRenderFullScreen()
            && !isRenderFullScreenPlaceholder()
#endif
#if ENABLE(MATHML)
            && !isRenderMathMLBlock()
#endif
            ;
    }
    bool isAnonymousColumnsBlock() const { return style().specifiesColumns() && isAnonymousBlock(); }
    bool isAnonymousColumnSpanBlock() const { return style().columnSpan() && isAnonymousBlock(); }
    bool isElementContinuation() const { return node() && node()->renderer() != this; }
    bool isInlineElementContinuation() const { return isElementContinuation() && isInline(); }
    bool isBlockElementContinuation() const { return isElementContinuation() && !isInline(); }
    virtual RenderBoxModelObject* virtualContinuation() const { return 0; }

    bool isFloating() const { return m_bitfields.floating(); }

    bool isOutOfFlowPositioned() const { return m_bitfields.isOutOfFlowPositioned(); } // absolute or fixed positioning
    bool isInFlowPositioned() const { return m_bitfields.isRelPositioned() || m_bitfields.isStickyPositioned(); } // relative or sticky positioning
    bool isRelPositioned() const { return m_bitfields.isRelPositioned(); } // relative positioning
    bool isStickyPositioned() const { return m_bitfields.isStickyPositioned(); }
    bool isPositioned() const { return m_bitfields.isPositioned(); }

    bool isText() const  { return !m_bitfields.isBox() && m_bitfields.isTextOrRenderView(); }
    bool isLineBreak() const { return m_bitfields.isLineBreak(); }
    bool isBR() const { return isLineBreak() && !isWBR(); }
    bool isLineBreakOpportunity() const { return isLineBreak() && isWBR(); }
    bool isTextOrLineBreak() const { return isText() || isLineBreak(); }
    bool isBox() const { return m_bitfields.isBox(); }
    bool isRenderView() const  { return m_bitfields.isBox() && m_bitfields.isTextOrRenderView(); }
    bool isInline() const { return m_bitfields.isInline(); } // inline object
    bool isRunIn() const { return style().display() == RUN_IN; } // run-in object
    bool isDragging() const { return m_bitfields.isDragging(); }
    bool isReplaced() const { return m_bitfields.isReplaced(); } // a "replaced" element (see CSS)
    bool isHorizontalWritingMode() const { return m_bitfields.horizontalWritingMode(); }

    bool hasLayer() const { return m_bitfields.hasLayer(); }

    enum BoxDecorationState {
        NoBoxDecorations,
        HasBoxDecorationsAndBackgroundObscurationStatusInvalid,
        HasBoxDecorationsAndBackgroundIsKnownToBeObscured,
        HasBoxDecorationsAndBackgroundMayBeVisible,
    };
    bool hasBoxDecorations() const { return m_bitfields.boxDecorationState() != NoBoxDecorations; }
    bool backgroundIsKnownToBeObscured();
    bool hasEntirelyFixedBackground() const;

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
    bool normalChildNeedsLayout() const { return m_bitfields.normalChildNeedsLayout(); }
    
    bool preferredLogicalWidthsDirty() const { return m_bitfields.preferredLogicalWidthsDirty(); }

    bool isSelectionBorder() const;

    bool hasOverflowClip() const { return m_bitfields.hasOverflowClip(); }

    bool hasTransform() const { return m_bitfields.hasTransform(); }

    inline bool preservesNewline() const;

    // The pseudo element style can be cached or uncached.  Use the cached method if the pseudo element doesn't respect
    // any pseudo classes (and therefore has no concept of changing state).
    RenderStyle* getCachedPseudoStyle(PseudoId, RenderStyle* parentStyle = 0) const;
    PassRefPtr<RenderStyle> getUncachedPseudoStyle(const PseudoStyleRequest&, RenderStyle* parentStyle = 0, RenderStyle* ownStyle = 0) const;
    
    virtual void updateDragState(bool dragOn);

    RenderView& view() const { return *document().renderView(); };

    // Returns true if this renderer is rooted, and optionally returns the hosting view (the root of the hierarchy).
    bool isRooted(RenderView** = 0) const;

    Node* node() const { return isAnonymous() ? 0 : &m_node; }
    Node* nonPseudoNode() const { return isPseudoElement() ? 0 : node(); }

    // Returns the styled node that caused the generation of this renderer.
    // This is the same as node() except for renderers of :before and :after
    // pseudo elements for which their parent node is returned.
    Node* generatingNode() const { return isPseudoElement() ? generatingPseudoHostElement() : node(); }

    Document& document() const { return m_node.document(); }
    Frame& frame() const; // Defined in RenderView.h

    bool hasOutlineAnnotation() const;
    bool hasOutline() const { return style().hasOutline() || hasOutlineAnnotation(); }

    // Returns the object containing this one. Can be different from parent for positioned elements.
    // If repaintContainer and repaintContainerSkipped are not null, on return *repaintContainerSkipped
    // is true if the renderer returned is an ancestor of repaintContainer.
    RenderElement* container(const RenderLayerModelObject* repaintContainer = 0, bool* repaintContainerSkipped = 0) const;

    RenderBoxModelObject* offsetParent() const;

    void markContainingBlocksForLayout(bool scheduleRelayout = true, RenderElement* newRoot = 0);
    void setNeedsLayout(MarkingBehavior = MarkContainingBlockChain);
    void clearNeedsLayout();
    void setPreferredLogicalWidthsDirty(bool, MarkingBehavior = MarkContainingBlockChain);
    void invalidateContainerPreferredLogicalWidths();
    
    void setNeedsLayoutAndPrefWidthsRecalc()
    {
        setNeedsLayout();
        setPreferredLogicalWidthsDirty(true);
    }

    void setPositionState(EPosition position)
    {
        ASSERT((position != AbsolutePosition && position != FixedPosition) || isBox());
        m_bitfields.setPositionedState(position);
    }
    void clearPositionedState() { m_bitfields.clearPositionedState(); }

    void setFloating(bool b = true) { m_bitfields.setFloating(b); }
    void setInline(bool b = true) { m_bitfields.setIsInline(b); }

    void setHasBoxDecorations(bool = true);
    void invalidateBackgroundObscurationStatus();
    virtual bool computeBackgroundIsKnownToBeObscured() { return false; }

    void setIsText() { ASSERT(!isBox()); m_bitfields.setIsTextOrRenderView(true); }
    void setIsLineBreak() { m_bitfields.setIsLineBreak(true); }
    void setIsBox() { m_bitfields.setIsBox(true); }
    void setIsRenderView() { ASSERT(isBox()); m_bitfields.setIsTextOrRenderView(true); }
    void setReplaced(bool b = true) { m_bitfields.setIsReplaced(b); }
    void setHorizontalWritingMode(bool b = true) { m_bitfields.setHorizontalWritingMode(b); }
    void setHasOverflowClip(bool b = true) { m_bitfields.setHasOverflowClip(b); }
    void setHasLayer(bool b = true) { m_bitfields.setHasLayer(b); }
    void setHasTransform(bool b = true) { m_bitfields.setHasTransform(b); }
    void setHasReflection(bool b = true) { m_bitfields.setHasReflection(b); }

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

    virtual VisiblePosition positionForPoint(const LayoutPoint&);
    VisiblePosition createVisiblePosition(int offset, EAffinity) const;
    VisiblePosition createVisiblePosition(const Position&) const;

    // returns the containing block level element for this element.
    RenderBlock* containingBlock() const;

    bool canContainFixedPositionObjects() const
    {
        return isRenderView() || (hasTransform() && isRenderBlock())
#if ENABLE(SVG)
                || isSVGForeignObject()
#endif
                || isOutOfFlowRenderFlowThread();
    }

    // Convert the given local point to absolute coordinates
    // FIXME: Temporary. If UseTransforms is true, take transforms into account. Eventually localToAbsolute() will always be transform-aware.
    FloatPoint localToAbsolute(const FloatPoint& localPoint = FloatPoint(), MapCoordinatesFlags = 0) const;
    FloatPoint absoluteToLocal(const FloatPoint&, MapCoordinatesFlags = 0) const;

    // Convert a local quad to absolute coordinates, taking transforms into account.
    FloatQuad localToAbsoluteQuad(const FloatQuad& quad, MapCoordinatesFlags mode = 0, bool* wasFixed = 0) const
    {
        return localToContainerQuad(quad, 0, mode, wasFixed);
    }
    // Convert an absolute quad to local coordinates.
    FloatQuad absoluteToLocalQuad(const FloatQuad&, MapCoordinatesFlags mode = 0) const;

    // Convert a local quad into the coordinate system of container, taking transforms into account.
    FloatQuad localToContainerQuad(const FloatQuad&, const RenderLayerModelObject* repaintContainer, MapCoordinatesFlags = 0, bool* wasFixed = 0) const;
    FloatPoint localToContainerPoint(const FloatPoint&, const RenderLayerModelObject* repaintContainer, MapCoordinatesFlags = 0, bool* wasFixed = 0) const;

    // Return the offset from the container() renderer (excluding transforms). In multi-column layout,
    // different offsets apply at different points, so return the offset that applies to the given point.
    virtual LayoutSize offsetFromContainer(RenderObject*, const LayoutPoint&, bool* offsetDependsOnPoint = 0) const;
    // Return the offset from an object up the container() chain. Asserts that none of the intermediate objects have transforms.
    LayoutSize offsetFromAncestorContainer(RenderObject*) const;

#if PLATFORM(IOS)
    virtual void collectSelectionRects(Vector<SelectionRect>&, unsigned startOffset = 0, unsigned endOffset = std::numeric_limits<unsigned>::max());
    virtual void absoluteQuadsForSelection(Vector<FloatQuad>& quads) const { absoluteQuads(quads); }
#endif

    virtual void absoluteRects(Vector<IntRect>&, const LayoutPoint&) const { }

    // FIXME: useTransforms should go away eventually
    IntRect absoluteBoundingBoxRect(bool useTransform = true) const;
    IntRect absoluteBoundingBoxRectIgnoringTransforms() const { return absoluteBoundingBoxRect(false); }

    // Build an array of quads in absolute coords for line boxes
    virtual void absoluteQuads(Vector<FloatQuad>&, bool* /*wasFixed*/ = 0) const { }

    virtual void absoluteFocusRingQuads(Vector<FloatQuad>&);

    static FloatRect absoluteBoundingBoxRectForRange(const Range*);

    // the rect that will be painted if this object is passed as the paintingRoot
    LayoutRect paintingRootRect(LayoutRect& topLevelRect);

    virtual LayoutUnit minPreferredLogicalWidth() const { return 0; }
    virtual LayoutUnit maxPreferredLogicalWidth() const { return 0; }

    RenderStyle& style() const;
    RenderStyle& firstLineStyle() const;

    // Anonymous blocks that are part of of a continuation chain will return their inline continuation's outline style instead.
    // This is typically only relevant when repainting.
    virtual const RenderStyle& outlineStyleForRepaint() const { return style(); }
    
    virtual CursorDirective getCursor(const LayoutPoint&, Cursor&) const;

    void getTextDecorationColors(int decorations, Color& underline, Color& overline, Color& linethrough, bool quirksMode = false, bool firstlineStyle = false);

    // Return the RenderLayerModelObject in the container chain which is responsible for painting this object, or 0
    // if painting is root-relative. This is the container that should be passed to the 'forRepaint'
    // methods.
    RenderLayerModelObject* containerForRepaint() const;
    // Actually do the repaint of rect r for this object which has been computed in the coordinate space
    // of repaintContainer. If repaintContainer is 0, repaint via the view.
    void repaintUsingContainer(const RenderLayerModelObject* repaintContainer, const IntRect&, bool immediate = false, bool shouldClipToLayer = true) const;
    
    // Repaint the entire object.  Called when, e.g., the color of a border changes, or when a border
    // style changes.
    void repaint(bool immediate = false) const;

    // Repaint a specific subrectangle within a given object.  The rect |r| is in the object's coordinate space.
    void repaintRectangle(const LayoutRect&, bool immediate = false, bool shouldClipToLayer = true) const;

    bool checkForRepaintDuringLayout() const;

    // Returns the rect that should be repainted whenever this object changes.  The rect is in the view's
    // coordinate space.  This method deals with outlines and overflow.
    LayoutRect absoluteClippedOverflowRect() const
    {
        return clippedOverflowRectForRepaint(0);
    }
    IntRect pixelSnappedAbsoluteClippedOverflowRect() const;
    virtual LayoutRect clippedOverflowRectForRepaint(const RenderLayerModelObject* repaintContainer) const;
    virtual LayoutRect rectWithOutlineForRepaint(const RenderLayerModelObject* repaintContainer, LayoutUnit outlineWidth) const;
    virtual LayoutRect outlineBoundsForRepaint(const RenderLayerModelObject* /*repaintContainer*/, const RenderGeometryMap* = 0) const { return LayoutRect(); }

    // Given a rect in the object's coordinate space, compute a rect suitable for repainting
    // that rect in view coordinates.
    void computeAbsoluteRepaintRect(LayoutRect& r, bool fixed = false) const
    {
        computeRectForRepaint(0, r, fixed);
    }
    // Given a rect in the object's coordinate space, compute a rect suitable for repainting
    // that rect in the coordinate space of repaintContainer.
    virtual void computeRectForRepaint(const RenderLayerModelObject* repaintContainer, LayoutRect&, bool fixed = false) const;
    virtual void computeFloatRectForRepaint(const RenderLayerModelObject* repaintContainer, FloatRect& repaintRect, bool fixed = false) const;

    // If multiple-column layout results in applying an offset to the given point, add the same
    // offset to the given size.
    virtual void adjustForColumns(LayoutSize&, const LayoutPoint&) const { }
    LayoutSize offsetForColumns(const LayoutPoint& point) const
    {
        LayoutSize offset;
        adjustForColumns(offset, point);
        return offset;
    }

    virtual unsigned int length() const { return 1; }

    bool isFloatingOrOutOfFlowPositioned() const { return (isFloating() || isOutOfFlowPositioned()); }

    bool hasReflection() const { return m_bitfields.hasReflection(); }

    // Applied as a "slop" to dirty rect checks during the outline painting phase's dirty-rect checks.
    int maximalOutlineSize(PaintPhase) const;

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
    LayoutRect selectionRect(bool clipToVisibleContent = true) { return selectionRectForRepaint(0, clipToVisibleContent); }
    virtual LayoutRect selectionRectForRepaint(const RenderLayerModelObject* /*repaintContainer*/, bool /*clipToVisibleContent*/ = true) { return LayoutRect(); }

    virtual bool canBeSelectionLeaf() const { return false; }
    bool hasSelectedChildren() const { return selectionState() != SelectionNone; }

    // Obtains the selection colors that should be used when painting a selection.
    Color selectionBackgroundColor() const;
    Color selectionForegroundColor() const;
    Color selectionEmphasisMarkColor() const;

    // Whether or not a given block needs to paint selection gaps.
    virtual bool shouldPaintSelectionGaps() const { return false; }

    /**
     * Returns the local coordinates of the caret within this render object.
     * @param caretOffset zero-based offset determining position within the render object.
     * @param extraWidthToEndOfLine optional out arg to give extra width to end of line -
     * useful for character range rect computations
     */
    virtual LayoutRect localCaretRect(InlineBox*, int caretOffset, LayoutUnit* extraWidthToEndOfLine = 0);

    // When performing a global document tear-down, the renderer of the document is cleared.  We use this
    // as a hook to detect the case of document destruction and don't waste time doing unnecessary work.
    bool documentBeingDestroyed() const;

    void destroyAndCleanupAnonymousWrappers();
    void destroy();

    // Virtual function helpers for the deprecated Flexible Box Layout (display: -webkit-box).
    virtual bool isDeprecatedFlexibleBox() const { return false; }
    virtual bool isStretchingChildren() const { return false; }

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

    virtual void imageChanged(CachedImage*, const IntRect* = 0) override;
    virtual void imageChanged(WrappedImagePtr, const IntRect* = 0) { }
    virtual bool willRenderImage(CachedImage*) override;

    void selectionStartEnd(int& spos, int& epos) const;
    
    void removeFromParent();

    AnimationController& animation() const;

    // Map points and quads through elements, potentially via 3d transforms. You should never need to call these directly; use
    // localToAbsolute/absoluteToLocal methods instead.
    virtual void mapLocalToContainer(const RenderLayerModelObject* repaintContainer, TransformState&, MapCoordinatesFlags = ApplyContainerFlip, bool* wasFixed = 0) const;
    virtual void mapAbsoluteToLocalPoint(MapCoordinatesFlags, TransformState&) const;

    // Pushes state onto RenderGeometryMap about how to map coordinates from this renderer to its container, or ancestorToStopAt (whichever is encountered first).
    // Returns the renderer which was mapped to (container or ancestorToStopAt).
    virtual const RenderObject* pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap&) const;
    
    bool shouldUseTransformFromContainer(const RenderObject* container) const;
    void getTransformFromContainer(const RenderObject* container, const LayoutSize& offsetInContainer, TransformationMatrix&) const;
    
    virtual void addFocusRingRects(Vector<IntRect>&, const LayoutPoint& /* additionalOffset */, const RenderLayerModelObject* /* paintContainer */ = 0) { };

    LayoutRect absoluteOutlineBounds() const
    {
        return outlineBoundsForRepaint(0);
    }

    RespectImageOrientationEnum shouldRespectImageOrientation() const;

    void drawLineForBoxSide(GraphicsContext*, int x1, int y1, int x2, int y2, BoxSide,
                            Color, EBorderStyle, int adjbw1, int adjbw2, bool antialias = false);
protected:
    int columnNumberForOffset(int offset);

    void paintFocusRing(PaintInfo&, const LayoutPoint&, RenderStyle*);
    void paintOutline(PaintInfo&, const LayoutRect&);
    void addPDFURLRect(GraphicsContext*, const LayoutRect&);
    Node& nodeForNonAnonymous() const { ASSERT(!isAnonymous()); return m_node; }

    void adjustRectForOutlineAndShadow(LayoutRect&) const;

    void clearLayoutRootIfNeeded() const;
    virtual void willBeDestroyed();

    virtual bool canBeReplacedWithInlineRunIn() const;

    virtual void insertedIntoTree();
    virtual void willBeRemovedFromTree();

    void setNeedsPositionedMovementLayoutBit(bool b) { m_bitfields.setNeedsPositionedMovementLayout(b); }
    void setNormalChildNeedsLayoutBit(bool b) { m_bitfields.setNormalChildNeedsLayout(b); }
    void setPosChildNeedsLayoutBit(bool b) { m_bitfields.setPosChildNeedsLayout(b); }
    void setNeedsSimplifiedNormalFlowLayoutBit(bool b) { m_bitfields.setNeedsSimplifiedNormalFlowLayout(b); }

private:
    RenderFlowThread* locateFlowThreadContainingBlock() const;
    void removeFromRenderFlowThread();
    void removeFromRenderFlowThreadRecursive(RenderFlowThread*);

    Color selectionColor(int colorProperty) const;

    Node* generatingPseudoHostElement() const;

    virtual bool isWBR() const { ASSERT_NOT_REACHED(); return false; }

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

    class RenderObjectBitfields {
        enum PositionedState {
            IsStaticallyPositioned = 0,
            IsRelativelyPositioned = 1,
            IsOutOfFlowPositioned = 2,
            IsStickyPositioned = 3
        };

    public:
        RenderObjectBitfields(const Node& node)
            : m_needsLayout(false)
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
            , m_isDragging(false)
            , m_hasLayer(false)
            , m_hasOverflowClip(false)
            , m_hasTransform(false)
            , m_hasReflection(false)
            , m_hasCounterNodeMap(false)
            , m_everHadLayout(false)
            , m_childrenInline(false)
            , m_hasColumns(false)
            , m_positionedState(IsStaticallyPositioned)
            , m_selectionState(SelectionNone)
            , m_flowThreadState(NotInsideFlowThread)
            , m_boxDecorationState(NoBoxDecorations)
        {
        }
        
        // 32 bits have been used here. There are no bits available.
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
        ADD_BOOLEAN_BITFIELD(isDragging, IsDragging);

        ADD_BOOLEAN_BITFIELD(hasLayer, HasLayer);
        ADD_BOOLEAN_BITFIELD(hasOverflowClip, HasOverflowClip); // Set in the case of overflow:auto/scroll/hidden
        ADD_BOOLEAN_BITFIELD(hasTransform, HasTransform);
        ADD_BOOLEAN_BITFIELD(hasReflection, HasReflection);

        ADD_BOOLEAN_BITFIELD(hasCounterNodeMap, HasCounterNodeMap);
        ADD_BOOLEAN_BITFIELD(everHadLayout, EverHadLayout);

        // from RenderBlock
        ADD_BOOLEAN_BITFIELD(childrenInline, ChildrenInline);
        ADD_BOOLEAN_BITFIELD(hasColumns, HasColumns);

    private:
        unsigned m_positionedState : 2; // PositionedState
        unsigned m_selectionState : 3; // SelectionState
        unsigned m_flowThreadState : 2; // FlowThreadState
        unsigned m_boxDecorationState : 2; // BoxDecorationState

    public:
        bool isOutOfFlowPositioned() const { return m_positionedState == IsOutOfFlowPositioned; }
        bool isRelPositioned() const { return m_positionedState == IsRelativelyPositioned; }
        bool isStickyPositioned() const { return m_positionedState == IsStickyPositioned; }
        bool isPositioned() const { return m_positionedState != IsStaticallyPositioned; }

        void setPositionedState(int positionState)
        {
            // This mask maps FixedPosition and AbsolutePosition to IsOutOfFlowPositioned, saving one bit.
            m_positionedState = static_cast<PositionedState>(positionState & 0x3);
        }
        void clearPositionedState() { m_positionedState = StaticPosition; }

        ALWAYS_INLINE SelectionState selectionState() const { return static_cast<SelectionState>(m_selectionState); }
        ALWAYS_INLINE void setSelectionState(SelectionState selectionState) { m_selectionState = selectionState; }
        
        ALWAYS_INLINE FlowThreadState flowThreadState() const { return static_cast<FlowThreadState>(m_flowThreadState); }
        ALWAYS_INLINE void setFlowThreadState(FlowThreadState flowThreadState) { m_flowThreadState = flowThreadState; }

        ALWAYS_INLINE BoxDecorationState boxDecorationState() const { return static_cast<BoxDecorationState>(m_boxDecorationState); }
        ALWAYS_INLINE void setBoxDecorationState(BoxDecorationState boxDecorationState) { m_boxDecorationState = boxDecorationState; }
    };

#undef ADD_BOOLEAN_BITFIELD

    RenderObjectBitfields m_bitfields;

    void setIsDragging(bool b) { m_bitfields.setIsDragging(b); }
    void setEverHadLayout(bool b) { m_bitfields.setEverHadLayout(b); }
};

template <typename Type> bool isRendererOfType(const RenderObject&);
template <> inline bool isRendererOfType<const RenderObject>(const RenderObject&) { return true; }

inline bool RenderObject::documentBeingDestroyed() const
{
    return document().renderTreeBeingDestroyed();
}

inline bool RenderObject::isBeforeContent() const
{
    // Text nodes don't have their own styles, so ignore the style on a text node.
    if (isText())
        return false;
    if (style().styleType() != BEFORE)
        return false;
    return true;
}

inline bool RenderObject::isAfterContent() const
{
    // Text nodes don't have their own styles, so ignore the style on a text node.
    if (isText())
        return false;
    if (style().styleType() != AFTER)
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
#if ENABLE(SVG)
    if (isSVGInlineText())
        return false;
#endif
        
    return style().preserveNewline();
}

inline void RenderObject::setSelectionStateIfNeeded(SelectionState state)
{
    if (selectionState() == state)
        return;

    setSelectionState(state);
}

inline void RenderObject::setHasBoxDecorations(bool b)
{
    if (!b) {
        m_bitfields.setBoxDecorationState(NoBoxDecorations);
        return;
    }
    if (hasBoxDecorations())
        return;
    m_bitfields.setBoxDecorationState(HasBoxDecorationsAndBackgroundObscurationStatusInvalid);
}

inline void RenderObject::invalidateBackgroundObscurationStatus()
{
    if (!hasBoxDecorations())
        return;
    m_bitfields.setBoxDecorationState(HasBoxDecorationsAndBackgroundObscurationStatusInvalid);
}

inline bool RenderObject::backgroundIsKnownToBeObscured()
{
    if (m_bitfields.boxDecorationState() == HasBoxDecorationsAndBackgroundObscurationStatusInvalid) {
        BoxDecorationState boxDecorationState = computeBackgroundIsKnownToBeObscured() ? HasBoxDecorationsAndBackgroundIsKnownToBeObscured : HasBoxDecorationsAndBackgroundMayBeVisible;
        m_bitfields.setBoxDecorationState(boxDecorationState);
    }
    return m_bitfields.boxDecorationState() == HasBoxDecorationsAndBackgroundIsKnownToBeObscured;
}

#define RENDER_OBJECT_TYPE_CASTS(ToValueTypeName, predicate) \
    TYPE_CASTS_BASE(ToValueTypeName, RenderObject, object, object->predicate, object.predicate)

} // namespace WebCore

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
void showTree(const WebCore::RenderObject*);
void showLineTree(const WebCore::RenderObject*);
void showRenderTree(const WebCore::RenderObject* object1);
// We don't make object2 an optional parameter so that showRenderTree
// can be called from gdb easily.
void showRenderTree(const WebCore::RenderObject* object1, const WebCore::RenderObject* object2);
#endif

#endif // RenderObject_h
