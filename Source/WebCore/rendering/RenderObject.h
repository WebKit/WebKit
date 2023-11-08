/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 *           (C) 2004 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2003-2022 Apple Inc. All rights reserved.
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
#include "FrameDestructionObserverInlines.h"
#include "HTMLNames.h"
#include "InspectorInstrumentationPublic.h"
#include "LayoutRect.h"
#include "LocalFrame.h"
#include "Page.h"
#include "RenderObjectEnums.h"
#include "RenderStyle.h"
#include "ScrollAlignment.h"
#include "StyleImage.h"
#include "TextAffinity.h"
#include <wtf/CheckedPtr.h>
#include <wtf/IsoMalloc.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

class AffineTransform;
class Color;
class ControlPart;
class Cursor;
class Document;
class HitTestLocation;
class HitTestRequest;
class HitTestResult;
class HostWindow;
class LegacyInlineBox;
class Path;
class Position;
class ReferencedSVGResources;
class RenderBoxModelObject;
class RenderInline;
class RenderBlock;
class RenderBlockFlow;
class RenderElement;
class RenderFragmentedFlow;
class RenderGeometryMap;
class RenderLayer;
class RenderLayerModelObject;
class RenderFragmentContainer;
class RenderTheme;
class RenderTreeBuilder;
class RenderHighlight;
class TransformState;
class VisiblePosition;

#if PLATFORM(IOS_FAMILY)
class SelectionGeometry;
#endif

struct InlineBoxAndOffset;
struct PaintInfo;
struct SimpleRange;

struct ScrollRectToVisibleOptions;

namespace Layout {
class Box;
}

namespace Style {
class PseudoElementRequest;
}

enum class RepaintRectCalculation { Fast, Accurate };

// Base class for all rendering tree objects.
class RenderObject : public CachedImageClient, public CanMakeCheckedPtr {
    WTF_MAKE_ISO_ALLOCATED(RenderObject);
    friend class RenderBlock;
    friend class RenderBlockFlow;
    friend class RenderBox;
    friend class RenderElement;
    friend class RenderLayer;
    friend class RenderLayerScrollableArea;
public:
    enum class Type : uint8_t {
#if ENABLE(ATTACHMENT_ELEMENT)
        Attachment,
#endif
        BlockFlow,
        Button,
        CombineText,
        Counter,
        DeprecatedFlexibleBox,
        DetailsMarker,
        EmbeddedObject,
        FileUploadControl,
        FlexibleBox,
        Frame,
        FrameSet,
        Grid,
        HTMLCanvas,
        IFrame,
        Image,
        Inline,
        LineBreak,
        ListBox,
        ListItem,
        ListMarker,
        Media,
        MenuList,
        Meter,
#if ENABLE(MODEL_ELEMENT)
        Model,
#endif
        MultiColumnFlow,
        MultiColumnSet,
        MultiColumnSpannerPlaceholder,
        Progress,
        Quote,
        Replica,
        RubyAsInline,
        RubyAsBlock,
        RubyBase,
        RubyRun,
        RubyText,
        ScrollbarPart,
        SearchField,
        Slider,
        SliderContainer,
        Table,
        TableCaption,
        TableCell,
        TableCol,
        TableRow,
        TableSection,
        Text,
        TextControlInnerBlock,
        TextControlInnerContainer,
        TextControlMultiLine,
        TextControlSingleLine,
        TextFragment,
        VTTCue,
        Video,
        View,
#if ENABLE(MATHML)
        MathMLBlock,
        MathMLFenced,
        MathMLFencedOperator,
        MathMLFraction,
        MathMLMath,
        MathMLMenclose,
        MathMLOperator,
        MathMLPadded,
        MathMLRoot,
        MathMLRow,
        MathMLScripts,
        MathMLSpace,
        MathMLTable,
        MathMLToken,
        MathMLUnderOver,
#endif
        SVGEllipse,
        SVGForeignObject,
        SVGGradientStop,
        SVGHiddenContainer,
        SVGImage,
        SVGInline,
        SVGInlineText,
        SVGPath,
        SVGRect,
        SVGResourceClipper,
        SVGResourceFilter,
        SVGResourceFilterPrimitive,
        SVGResourceLinearGradient,
        SVGResourceMarker,
        SVGResourceMasker,
        SVGResourcePattern,
        SVGResourceRadialGradient,
        SVGRoot,
        SVGTSpan,
        SVGText,
        SVGTextPath,
        SVGTransformableContainer,
        SVGViewportContainer,
        LegacySVGEllipse,
        LegacySVGForeignObject,
        LegacySVGHiddenContainer,
        LegacySVGImage,
        LegacySVGPath,
        LegacySVGRect,
        LegacySVGResourceClipper,
        LegacySVGRoot,
        LegacySVGTransformableContainer,
        LegacySVGViewportContainer
    };

    // Anonymous objects should pass the document as their node, and they will then automatically be
    // marked as anonymous in the constructor.
    RenderObject(Type, Node&);
    virtual ~RenderObject();

    Type type() const { return m_type; }
    Layout::Box* layoutBox() { return m_layoutBox.get(); }
    const Layout::Box* layoutBox() const { return m_layoutBox.get(); }
    void setLayoutBox(Layout::Box&);
    void clearLayoutBox();

    RenderTheme& theme() const;

    virtual ASCIILiteral renderName() const = 0;

    inline RenderElement* parent() const; // Defined in RenderElement.h.
    inline CheckedPtr<RenderElement> checkedParent() const; // Defined in RenderElement.h.
    bool isDescendantOf(const RenderObject*) const;

    RenderObject* previousSibling() const { return m_previous.get(); }
    RenderObject* nextSibling() const { return m_next.get(); }
    RenderObject* previousInFlowSibling() const;
    RenderObject* nextInFlowSibling() const;

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

    RenderElement* firstNonAnonymousAncestor() const;

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
    RenderObject* traverseNext(const RenderObject* stayWithin, HeightTypeTraverseNextInclusionFunction, int& currentDepth, int& newFixedDepth) const;
#endif

    WEBCORE_EXPORT RenderLayer* enclosingLayer() const;

    WEBCORE_EXPORT RenderBox& enclosingBox() const;
    RenderBoxModelObject& enclosingBoxModelObject() const;
    RenderBox* enclosingScrollableContainerForSnapping() const;

    // Return our enclosing flow thread if we are contained inside one. Follows the containing block chain.
    RenderFragmentedFlow* enclosingFragmentedFlow() const;

    WEBCORE_EXPORT bool useDarkAppearance() const;
    OptionSet<StyleColorOptions> styleColorOptions() const;

#if ASSERT_ENABLED
    void setHasAXObject(bool flag) { m_hasAXObject = flag; }
    bool hasAXObject() const { return m_hasAXObject; }
#endif

    // Creates a scope where this object will assert on calls to setNeedsLayout().
    class SetLayoutNeededForbiddenScope;
    
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

    bool isRenderElement() const { return !isRenderText(); }
    bool isRenderReplaced() const;
    bool isRenderBoxModelObject() const;
    bool isRenderBlock() const;
    bool isRenderBlockFlow() const;
    bool isRenderInline() const;
    bool isRenderLayerModelObject() const;

    inline bool isAtomicInlineLevelBox() const;

    bool isRenderCounter() const { return type() == Type::Counter; }
    bool isRenderQuote() const { return type() == Type::Quote; }

    bool isRenderDetailsMarker() const { return type() == Type::DetailsMarker; }
    bool isRenderEmbeddedObject() const { return type() == Type::EmbeddedObject; }
    bool isFieldset() const;
    bool isRenderFileUploadControl() const { return type() == Type::FileUploadControl; }
    bool isRenderFrame() const { return type() == Type::Frame; }
    bool isRenderFrameSet() const { return type() == Type::FrameSet; }
    virtual bool isImage() const { return false; }
    virtual bool isInlineBlockOrInlineTable() const { return false; }
    bool isRenderListBox() const { return type() == Type::ListBox; }
    bool isRenderListItem() const { return type() == Type::ListItem; }
    bool isRenderListMarker() const { return type() == Type::ListMarker; }
    virtual bool isRenderMedia() const { return false; }
    bool isRenderMenuList() const { return type() == Type::MenuList; }
    bool isRenderMeter() const { return type() == Type::Meter; }
    bool isRenderProgress() const { return type() == Type::Progress; }
    bool isRenderButton() const { return type() == Type::Button; }
    bool isRenderIFrame() const { return type() == Type::IFrame; }
    virtual bool isRenderImage() const { return false; }
    bool isRenderTextFragment() const { return type() == Type::TextFragment; }
#if ENABLE(MODEL_ELEMENT)
    bool isRenderModel() const { return type() == Type::Model; }
#endif
    virtual bool isRenderFragmentContainer() const { return false; }
    bool isRenderReplica() const { return type() == Type::Replica; }

    bool isRenderRubyAsInline() const { return type() == Type::RubyAsInline; }
    bool isRenderRubyAsBlock() const { return type() == Type::RubyAsBlock; }
    bool isRenderRubyBase() const { return type() == Type::RubyBase; }
    bool isRenderRubyRun() const { return type() == Type::RubyRun; }
    bool isRenderRubyText() const { return type() == Type::RubyText; }

    bool isRenderSlider() const { return type() == Type::Slider; }
    bool isRenderTable() const;
    bool isRenderTableCell() const { return type() == Type::TableCell; }
    bool isRenderTableCol() const { return type() == Type::TableCol; }
    bool isRenderTableCaption() const { return type() == Type::TableCaption; }
    bool isRenderTableSection() const { return type() == Type::TableSection; }
    inline bool isRenderTextControl() const; // Defined in RenderElement.h.
    bool isRenderTextControlMultiLine() const { return type() == Type::TextControlMultiLine; }
    virtual bool isRenderTextControlSingleLine() const { return false; }
    bool isRenderSearchField() const { return type() == Type::SearchField; }
    bool isRenderTextControlInnerBlock() const { return type() == Type::TextControlInnerBlock; }
    bool isRenderVideo() const { return type() == Type::Video; }
    virtual bool isRenderWidget() const { return false; }
    bool isRenderHTMLCanvas() const { return type() == Type::HTMLCanvas; }
#if ENABLE(ATTACHMENT_ELEMENT)
    bool isRenderAttachment() const { return type() == Type::Attachment; }
#endif
    bool isRenderGrid() const { return type() == Type::Grid; }

    virtual bool isMultiColumnBlockFlow() const { return false; }
    bool isRenderMultiColumnSet() const { return type() == Type::MultiColumnSet; }
    bool isRenderMultiColumnFlow() const { return type() == Type::MultiColumnFlow; }
    bool isRenderMultiColumnSpannerPlaceholder() const { return type() == Type::MultiColumnSpannerPlaceholder; }

    bool isRenderScrollbarPart() const { return type() == Type::ScrollbarPart; }
    bool isRenderVTTCue() const { return type() == Type::VTTCue; }

    bool isDocumentElementRenderer() const { return document().documentElement() == m_node.ptr(); }
    bool isBody() const { return node() && node()->hasTagName(HTMLNames::bodyTag); }
    bool isHR() const { return node() && node()->hasTagName(HTMLNames::hrTag); }
    bool isLegend() const;

    bool isHTMLMarquee() const;

    bool isTablePart() const { return isRenderTableCell() || isRenderTableCol() || isRenderTableCaption() || isRenderTableRow() || isRenderTableSection(); }

    inline bool isBeforeContent() const;
    inline bool isAfterContent() const;
    inline bool isBeforeOrAfterContent() const;
    static inline bool isBeforeContent(const RenderObject* obj) { return obj && obj->isBeforeContent(); }
    static inline bool isAfterContent(const RenderObject* obj) { return obj && obj->isAfterContent(); }
    static inline bool isBeforeOrAfterContent(const RenderObject* obj) { return obj && obj->isBeforeOrAfterContent(); }

    bool beingDestroyed() const { return m_bitfields.beingDestroyed(); }

    bool everHadLayout() const { return m_bitfields.everHadLayout(); }

    bool childrenInline() const { return m_bitfields.childrenInline(); }
    virtual void setChildrenInline(bool b) { m_bitfields.setChildrenInline(b); }
    
    enum FragmentedFlowState {
        NotInsideFragmentedFlow = 0,
        InsideInFragmentedFlow = 1,
    };

    enum class SkipDescendentFragmentedFlow { No, Yes };
    void setFragmentedFlowStateIncludingDescendants(FragmentedFlowState, SkipDescendentFragmentedFlow = SkipDescendentFragmentedFlow::Yes);

    FragmentedFlowState fragmentedFlowState() const { return m_bitfields.fragmentedFlowState(); }
    void setFragmentedFlowState(FragmentedFlowState state) { m_bitfields.setFragmentedFlowState(state); }

#if ENABLE(MATHML)
    virtual bool isRenderMathMLBlock() const { return false; }
    bool isRenderMathMLTable() const { return type() == Type::MathMLTable; }
    virtual bool isRenderMathMLOperator() const { return false; }
    bool isRenderMathMLRow() const;
    bool isRenderMathMLMath() const { return type() == Type::MathMLMath; }
    bool isRenderMathMLMenclose() const { return type() == Type::MathMLMenclose; }
    bool isRenderMathMLFenced() const { return type() == Type::MathMLFenced; }
    bool isRenderMathMLFencedOperator() const { return type() == Type::MathMLFencedOperator; }
    bool isRenderMathMLFraction() const { return type() == Type::MathMLFraction; }
    bool isRenderMathMLPadded() const { return type() == Type::MathMLPadded; }
    bool isRenderMathMLRoot() const { return type() == Type::MathMLRoot; }
    bool isRenderMathMLSpace() const { return type() == Type::MathMLSpace; }
    virtual bool isRenderMathMLSquareRoot() const { return false; }
    virtual bool isRenderMathMLScripts() const { return false; }
    virtual bool isRenderMathMLToken() const { return false; }
    bool isRenderMathMLUnderOver() const { return type() == Type::MathMLUnderOver; }
#endif // ENABLE(MATHML)

    virtual bool isLegacyRenderSVGModelObject() const { return false; }
    virtual bool isRenderSVGModelObject() const { return false; }
    virtual bool isRenderSVGBlock() const { return false; }
    bool isLegacyRenderSVGRoot() const { return type() == Type::LegacySVGRoot; }
    bool isRenderSVGRoot() const { return type() == Type::SVGRoot; }
    virtual bool isRenderSVGContainer() const { return false; }
    virtual bool isLegacyRenderSVGContainer() const { return false; }
    bool isRenderSVGTransformableContainer() const { return type() == Type::SVGTransformableContainer; }
    bool isLegacyRenderSVGTransformableContainer() const { return type() == Type::LegacySVGTransformableContainer; }
    bool isRenderSVGViewportContainer() const { return type() == Type::SVGViewportContainer; }
    bool isLegacyRenderSVGViewportContainer() const { return type() == Type::LegacySVGViewportContainer; }
    bool isRenderSVGGradientStop() const { return type() == Type::SVGGradientStop; }
    virtual bool isLegacyRenderSVGHiddenContainer() const { return false; }
    bool isRenderSVGHiddenContainer() const { return type() == Type::SVGHiddenContainer || isRenderSVGResourceContainer(); }
    bool isLegacyRenderSVGPath() const { return type() == Type::LegacySVGPath; }
    bool isRenderSVGPath() const { return type() == Type::SVGPath; }
    virtual bool isRenderSVGShape() const { return false; }
    virtual bool isLegacyRenderSVGShape() const { return false; }
    bool isRenderSVGText() const { return type() == Type::SVGText; }
    bool isRenderSVGTextPath() const { return type() == Type::SVGTextPath; }
    bool isRenderSVGTSpan() const { return type() == Type::SVGTSpan; }
    bool isRenderSVGInline() const { return type() == Type::SVGInline || type() == Type::SVGTSpan || type() == Type::SVGTextPath; }
    bool isRenderSVGInlineText() const { return type() == Type::SVGInlineText; }
    bool isLegacyRenderSVGImage() const { return type() == Type::LegacySVGImage; }
    bool isRenderSVGImage() const { return type() == Type::SVGImage; }
    bool isLegacyRenderSVGForeignObject() const { return type() == Type::LegacySVGForeignObject; }
    bool isRenderSVGForeignObject() const { return type() == Type::SVGForeignObject; }
    virtual bool isLegacyRenderSVGResourceContainer() const { return false; }
    bool isRenderSVGResourceContainer() const { return type() == Type::SVGResourceClipper; }
    bool isRenderSVGResourceFilter() const { return type() == Type::SVGResourceFilter; }
    bool isLegacyRenderSVGResourceClipper() const { return type() == Type::LegacySVGResourceClipper; }
    bool isRenderSVGResourceClipper() const { return type() == Type::SVGResourceClipper; }
    bool isRenderSVGResourceFilterPrimitive() const { return type() == Type::SVGResourceFilterPrimitive; }
    bool isRenderOrLegacyRenderSVGRoot() const { return isRenderSVGRoot() || isLegacyRenderSVGRoot(); }
    bool isRenderOrLegacyRenderSVGShape() const { return isRenderSVGShape() || isLegacyRenderSVGShape(); }
    bool isRenderOrLegacyRenderSVGPath() const { return isRenderSVGPath() || isLegacyRenderSVGPath(); }
    bool isRenderOrLegacyRenderSVGImage() const { return isRenderSVGImage() || isLegacyRenderSVGImage(); }
    bool isRenderOrLegacyRenderSVGForeignObject() const { return isRenderSVGForeignObject() || isLegacyRenderSVGForeignObject(); }
    bool isRenderOrLegacyRenderSVGModelObject() const { return isRenderSVGModelObject() || isLegacyRenderSVGModelObject(); }
    bool isSVGLayerAwareRenderer() const { return isRenderSVGRoot() || isRenderSVGModelObject() || isRenderSVGText() || isRenderSVGInline() || isRenderSVGForeignObject(); }

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

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    // The objectBoundingBox of a SVG container is affected by the transformations applied on its children -- the container
    // bounding box is a union of all child bounding boxes, mapped through their transformation matrices.
    //
    // This method ignores all transformations and computes the objectBoundingBox, without mapping through the child
    // transformation matrices. The SVG render tree is constructed in such a way, that it can be mapped to CSS equivalents:
    // The SVG render tree underneath the outermost <svg> behaves as a set of absolutely positioned, possibly nested, boxes.
    // They are laid out in such a way that transformations do NOT affect layout, as in HTML/CSS world, but take affect during
    // painting, hit-testing etc. This allows to minimize the amount of re-layouts when animating transformations in SVG
    // (not using CSS Animations/Transitions / Web Animations, but e.g. SMIL <animateTransform>, JS, ...).
    virtual FloatRect objectBoundingBoxWithoutTransformations() const { return objectBoundingBox(); }
#endif

    // Returns the smallest rectangle enclosing all of the painted content
    // respecting clipping, masking, filters, opacity, stroke-width and markers
    // This returns approximate rectangle for SVG renderers when RepaintRectCalculation::Fast is specified.
    virtual FloatRect repaintRectInLocalCoordinates(RepaintRectCalculation = RepaintRectCalculation::Fast) const;

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

    virtual bool hasIntrinsicAspectRatio() const { return isReplacedOrInlineBlock() && (isImage() || isRenderVideo() || isRenderHTMLCanvas()); }
    bool isAnonymous() const { return m_bitfields.isAnonymous(); }
    bool isAnonymousBlock() const;
    bool isBlockContainer() const;

    bool isFloating() const { return m_bitfields.floating(); }

    bool isPositioned() const { return m_bitfields.isPositioned(); }
    bool isInFlowPositioned() const { return m_bitfields.isRelativelyPositioned() || m_bitfields.isStickilyPositioned(); }
    bool isOutOfFlowPositioned() const { return m_bitfields.isOutOfFlowPositioned(); } // absolute or fixed positioning
    bool isFixedPositioned() const { return isOutOfFlowPositioned() && style().position() == PositionType::Fixed; }
    bool isAbsolutelyPositioned() const { return isOutOfFlowPositioned() && style().position() == PositionType::Absolute; }
    bool isRelativelyPositioned() const { return m_bitfields.isRelativelyPositioned(); }
    bool isStickilyPositioned() const { return m_bitfields.isStickilyPositioned(); }
    bool shouldUsePositionedClipping() const { return isAbsolutelyPositioned() || isRenderSVGForeignObject(); }

    bool isRenderText() const { return m_bitfields.isRenderText(); }
    bool isRenderLineBreak() const { return type() == Type::LineBreak; }
    bool isBR() const { return isRenderLineBreak() && !isWBR(); }
    bool isLineBreakOpportunity() const { return isRenderLineBreak() && isWBR(); }
    bool isRenderTextOrLineBreak() const { return isRenderText() || isRenderLineBreak(); }
    bool isRenderBox() const { return m_bitfields.isRenderBox(); }
    bool isRenderTableRow() const { return type() == Type::TableRow; }
    bool isRenderView() const  { return type() == Type::View; }
    bool isInline() const { return m_bitfields.isInline(); } // inline object
    bool isReplacedOrInlineBlock() const { return m_bitfields.isReplacedOrInlineBlock(); }
    bool isHorizontalWritingMode() const { return m_bitfields.horizontalWritingMode(); }

    bool hasReflection() const { return m_bitfields.hasRareData() && rareData().hasReflection(); }
    bool isRenderFragmentedFlow() const { return m_bitfields.hasRareData() && rareData().isRenderFragmentedFlow(); }
    bool hasOutlineAutoAncestor() const { return m_bitfields.hasRareData() && rareData().hasOutlineAutoAncestor(); }
    bool paintContainmentApplies() const { return m_bitfields.hasRareData() && rareData().paintContainmentApplies(); }

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    bool hasSVGTransform() const { return m_bitfields.hasRareData() && rareData().hasSVGTransform(); }
#else
    bool hasSVGTransform() const { return false; }
#endif

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

    bool needsLayout() const;
    bool selfNeedsLayout() const { return m_bitfields.needsLayout(); }
    bool needsPositionedMovementLayout() const { return m_bitfields.needsPositionedMovementLayout(); }
    bool needsPositionedMovementLayoutOnly() const;

    bool posChildNeedsLayout() const { return m_bitfields.posChildNeedsLayout(); }
    bool needsSimplifiedNormalFlowLayout() const { return m_bitfields.needsSimplifiedNormalFlowLayout(); }
    bool needsSimplifiedNormalFlowLayoutOnly() const;
    bool normalChildNeedsLayout() const { return m_bitfields.normalChildNeedsLayout(); }
    
    bool preferredLogicalWidthsDirty() const { return m_bitfields.preferredLogicalWidthsDirty(); }

    bool isSelectionBorder() const;

    bool hasNonVisibleOverflow() const { return m_bitfields.hasNonVisibleOverflow(); }

    bool hasPotentiallyScrollableOverflow() const;

    bool hasTransformRelatedProperty() const { return m_bitfields.hasTransformRelatedProperty(); } // Transform, perspective or transform-style: preserve-3d.
    inline bool isTransformed() const;
    inline bool hasTransformOrPerspective() const;

    inline bool preservesNewline() const;

    RenderView& view() const { return *document().renderView(); }
    CheckedRef<RenderView> checkedView() const;

    HostWindow* hostWindow() const;

    // Returns true if this renderer is rooted.
    bool isRooted() const;

    Node* node() const
    { 
        if (isAnonymous())
            return nullptr;
        return m_node.ptr();
    }
    RefPtr<Node> protectedNode() const { return node(); }

    Node* nonPseudoNode() const { return isPseudoElement() ? nullptr : node(); }

    // Returns the styled node that caused the generation of this renderer.
    // This is the same as node() except for renderers of :before and :after
    // pseudo elements for which their parent node is returned.
    Node* generatingNode() const { return isPseudoElement() ? generatingPseudoHostElement() : node(); }

    Document& document() const { return m_node.get().document(); }
    inline Ref<Document> protectedDocument() const; // Defined in RenderObjectInlines.h.
    TreeScope& treeScopeForSVGReferences() const { return m_node.get().treeScopeForSVGReferences(); }
    LocalFrame& frame() const;
    Ref<LocalFrame> protectedFrame() const { return frame(); }
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
    
    void setNeedsLayoutAndPrefWidthsRecalc();

    void setPositionState(PositionType);
    void clearPositionedState() { m_bitfields.clearPositionedState(); }

    void setFloating(bool b = true) { m_bitfields.setFloating(b); }
    void setInline(bool b = true) { m_bitfields.setIsInline(b); }

    void setHasVisibleBoxDecorations(bool = true);
    void invalidateBackgroundObscurationStatus();
    virtual bool computeBackgroundIsKnownToBeObscured(const LayoutPoint&) { return false; }

    void setIsRenderText() { ASSERT(!isRenderBox()); m_bitfields.setIsRenderText(true); }
    void setIsRenderBox() { m_bitfields.setIsRenderBox(true); }
    void setReplacedOrInlineBlock(bool b = true) { m_bitfields.setIsReplacedOrInlineBlock(b); }
    void setHorizontalWritingMode(bool b = true) { m_bitfields.setHorizontalWritingMode(b); }
    void setHasNonVisibleOverflow(bool b = true) { m_bitfields.setHasNonVisibleOverflow(b); }
    void setHasLayer(bool b = true) { m_bitfields.setHasLayer(b); }
    void setHasTransformRelatedProperty(bool b = true) { m_bitfields.setHasTransformRelatedProperty(b); }

    void setHasReflection(bool = true);
    void setIsRenderFragmentedFlow(bool = true);
    void setHasOutlineAutoAncestor(bool = true);
    void setPaintContainmentApplies(bool = true);
#if ENABLE(LAYER_BASED_SVG_ENGINE)
    void setHasSVGTransform(bool = true);
#endif

    // Hook so that RenderTextControl can return the line height of its inner renderer.
    // For other renderers, the value is the same as lineHeight(false).
    virtual int innerLineHeight() const;

    // used for element state updates that cannot be fixed with a
    // repaint and do not need a relayout
    virtual void updateFromElement() { }

    bool isComposited() const;

    bool hitTest(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestFilter = HitTestAll);
    virtual Node* nodeForHitTest() const;
    virtual void updateHitTestResult(HitTestResult&, const LayoutPoint&);

    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction);

    virtual Position positionForPoint(const LayoutPoint&);
    virtual VisiblePosition positionForPoint(const LayoutPoint&, const RenderFragmentContainer*);
    VisiblePosition createVisiblePosition(int offset, Affinity) const;
    VisiblePosition createVisiblePosition(const Position&) const;

    // Returns the containing block level element for this element.
    WEBCORE_EXPORT RenderBlock* containingBlock() const;
    CheckedPtr<RenderBlock> checkedContainingBlock() const;
    static RenderBlock* containingBlockForPositionType(PositionType, const RenderObject&);

    // Convert the given local point to absolute coordinates. If OptionSet<MapCoordinatesMode> includes UseTransforms, take transforms into account.
    WEBCORE_EXPORT FloatPoint localToAbsolute(const FloatPoint& localPoint = FloatPoint(), OptionSet<MapCoordinatesMode> = { }, bool* wasFixed = nullptr) const;
    FloatPoint absoluteToLocal(const FloatPoint&, OptionSet<MapCoordinatesMode> = { }) const;

    // Convert a local quad to absolute coordinates, taking transforms into account.
    FloatQuad localToAbsoluteQuad(const FloatQuad&, OptionSet<MapCoordinatesMode> = UseTransforms, bool* wasFixed = nullptr) const;
    // Convert an absolute quad to local coordinates.
    FloatQuad absoluteToLocalQuad(const FloatQuad&, OptionSet<MapCoordinatesMode> = UseTransforms) const;

    // Convert a local quad into the coordinate system of container, taking transforms into account.
    WEBCORE_EXPORT FloatQuad localToContainerQuad(const FloatQuad&, const RenderLayerModelObject* container, OptionSet<MapCoordinatesMode> = UseTransforms, bool* wasFixed = nullptr) const;
    WEBCORE_EXPORT FloatPoint localToContainerPoint(const FloatPoint&, const RenderLayerModelObject* container, OptionSet<MapCoordinatesMode> = UseTransforms, bool* wasFixed = nullptr) const;

    // Return the offset from the container() renderer (excluding transforms). In multi-column layout,
    // different offsets apply at different points, so return the offset that applies to the given point.
    virtual LayoutSize offsetFromContainer(RenderElement&, const LayoutPoint&, bool* offsetDependsOnPoint = nullptr) const;
    // Return the offset from an object up the container() chain. Asserts that none of the intermediate objects have transforms.
    LayoutSize offsetFromAncestorContainer(const RenderElement&) const;

#if PLATFORM(IOS_FAMILY)
    virtual void collectSelectionGeometries(Vector<SelectionGeometry>&, unsigned startOffset = 0, unsigned endOffset = std::numeric_limits<unsigned>::max());
    virtual void absoluteQuadsForSelection(Vector<FloatQuad>& quads) const { absoluteQuads(quads); }
    WEBCORE_EXPORT static Vector<SelectionGeometry> collectSelectionGeometries(const SimpleRange&);
    WEBCORE_EXPORT static Vector<SelectionGeometry> collectSelectionGeometriesWithoutUnionInteriorLines(const SimpleRange&);
#endif

    virtual void boundingRects(Vector<LayoutRect>&, const LayoutPoint& /* offsetFromRoot */) const { }

    WEBCORE_EXPORT IntRect absoluteBoundingBoxRect(bool useTransform = true, bool* wasFixed = nullptr) const;
    IntRect absoluteBoundingBoxRectIgnoringTransforms() const { return absoluteBoundingBoxRect(false); }

    // Build an array of quads in absolute coords for line boxes
    virtual void absoluteQuads(Vector<FloatQuad>&, bool* /*wasFixed*/ = nullptr) const { }
    virtual void absoluteFocusRingQuads(Vector<FloatQuad>&);

    enum class BoundingRectBehavior : uint8_t {
        RespectClipping = 1 << 0,
        UseVisibleBounds = 1 << 1,
        IgnoreTinyRects = 1 << 2,
        IgnoreEmptyTextSelections = 1 << 3,
        UseSelectionHeight = 1 << 4,
        ComputeIndividualCharacterRects = 1 << 5,
    };
    WEBCORE_EXPORT static Vector<FloatQuad> absoluteTextQuads(const SimpleRange&, OptionSet<BoundingRectBehavior> = { });
    WEBCORE_EXPORT static Vector<IntRect> absoluteTextRects(const SimpleRange&, OptionSet<BoundingRectBehavior> = { });
    WEBCORE_EXPORT static Vector<FloatRect> absoluteBorderAndTextRects(const SimpleRange&, OptionSet<BoundingRectBehavior> = { });
    static Vector<FloatRect> clientBorderAndTextRects(const SimpleRange&);

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
    // if painting is root-relative. This is the container that should be passed to the 'forRepaint' functions.
    struct RepaintContainerStatus {
        bool fullRepaintIsScheduled { false }; // Either the repaint container or a layer in-between has already been scheduled for full repaint.
        CheckedPtr<const RenderLayerModelObject> renderer { nullptr };
    };
    RepaintContainerStatus containerForRepaint() const;
    // Actually do the repaint of rect r for this object which has been computed in the coordinate space
    // of repaintContainer. If repaintContainer is nullptr, repaint via the view.
    void repaintUsingContainer(const RenderLayerModelObject* repaintContainer, const LayoutRect&, bool shouldClipToLayer = true) const;
    
    // Repaint the entire object.  Called when, e.g., the color of a border changes, or when a border
    // style changes.
    void repaint() const;

    // Repaint a specific subrectangle within a given object.  The rect |r| is in the object's coordinate space.
    WEBCORE_EXPORT void repaintRectangle(const LayoutRect&, bool shouldClipToLayer = true) const;

    enum class ClipRepaintToLayer : bool { No, Yes };
    enum class ForceRepaint : bool { No, Yes };
    void repaintRectangle(const LayoutRect&, ClipRepaintToLayer, ForceRepaint, std::optional<LayoutBoxExtent> additionalRepaintOutsets = std::nullopt) const;

    // Repaint a slow repaint object, which, at this time, means we are repainting an object with background-attachment:fixed.
    void repaintSlowRepaintObject() const;

    enum class VisibleRectContextOption {
        UseEdgeInclusiveIntersection = 1 << 0,
        ApplyCompositedClips = 1 << 1,
        ApplyCompositedContainerScrolls  = 1 << 2,
        ApplyContainerClip = 1 << 3,
        CalculateAccurateRepaintRect = 1 << 4,
    };
    struct VisibleRectContext {
        VisibleRectContext(bool hasPositionFixedDescendant = false, bool dirtyRectIsFlipped = false, OptionSet<VisibleRectContextOption> options = { })
            : hasPositionFixedDescendant(hasPositionFixedDescendant)
            , dirtyRectIsFlipped(dirtyRectIsFlipped)
            , options(options)
            {
            }

        RepaintRectCalculation repaintRectCalculation() const
        {
            return options.contains(VisibleRectContextOption::CalculateAccurateRepaintRect) ? RepaintRectCalculation::Accurate : RepaintRectCalculation::Fast;
        }

        bool hasPositionFixedDescendant { false };
        bool dirtyRectIsFlipped { false };
        bool descendantNeedsEnclosingIntRect { false };
        OptionSet<VisibleRectContextOption> options;
    };

    // Returns the rect that should be repainted whenever this object changes. The rect is in the view's
    // coordinate space. This method deals with outlines and overflow.
    LayoutRect absoluteClippedOverflowRectForRepaint() const { return clippedOverflowRect(nullptr, visibleRectContextForRepaint()); }
    LayoutRect absoluteClippedOverflowRectForSpatialNavigation() const { return clippedOverflowRect(nullptr, visibleRectContextForSpatialNavigation()); }
    LayoutRect absoluteClippedOverflowRectForRenderTreeAsText() const { return clippedOverflowRect(nullptr, visibleRectContextForRenderTreeAsText()); }
    WEBCORE_EXPORT IntRect pixelSnappedAbsoluteClippedOverflowRect() const;
    virtual LayoutRect clippedOverflowRect(const RenderLayerModelObject* repaintContainer, VisibleRectContext) const;
    LayoutRect clippedOverflowRectForRepaint(const RenderLayerModelObject* repaintContainer) const { return clippedOverflowRect(repaintContainer, visibleRectContextForRepaint()); }
    virtual LayoutRect rectWithOutlineForRepaint(const RenderLayerModelObject* repaintContainer, LayoutUnit outlineWidth) const;
    virtual LayoutRect outlineBoundsForRepaint(const RenderLayerModelObject* /*repaintContainer*/, const RenderGeometryMap* = nullptr) const { return LayoutRect(); }

    // Given a rect in the object's coordinate space, compute a rect suitable for repainting
    // that rect in view coordinates.
    LayoutRect computeAbsoluteRepaintRect(const LayoutRect& rect) const { return computeRect(rect, nullptr, visibleRectContextForRepaint()); }
    // Given a rect in the object's coordinate space, compute a rect  in the coordinate space
    // of repaintContainer suitable for the given VisibleRectContext.
    LayoutRect computeRect(const LayoutRect&, const RenderLayerModelObject* repaintContainer, VisibleRectContext) const;
    virtual LayoutRect computeRectForRepaint(const LayoutRect& rect, const RenderLayerModelObject* repaintContainer) const { return computeRect(rect, repaintContainer, visibleRectContextForRepaint()); }
    FloatRect computeFloatRectForRepaint(const FloatRect&, const RenderLayerModelObject* repaintContainer) const;

    // Given a rect in the object's coordinate space, compute the location in container space where this rect is visible,
    // when clipping and scrolling as specified by the context. When using edge-inclusive intersection, return std::nullopt
    // rather than an empty rect if the rect is completely clipped out in container space.
    virtual std::optional<LayoutRect> computeVisibleRectInContainer(const LayoutRect&, const RenderLayerModelObject* repaintContainer, VisibleRectContext) const;
    virtual std::optional<FloatRect> computeFloatVisibleRectInContainer(const FloatRect&, const RenderLayerModelObject* repaintContainer, VisibleRectContext) const;

    WEBCORE_EXPORT bool hasNonEmptyVisibleRectRespectingParentFrames() const;

    virtual unsigned length() const { return 1; }

    bool isFloatingOrOutOfFlowPositioned() const { return (isFloating() || isOutOfFlowPositioned()); }
    bool isInFlow() const { return !isFloatingOrOutOfFlowPositioned(); }

    enum HighlightState {
        None, // The object is not selected.
        Start, // The object either contains the start of a selection run or is the start of a run
        Inside, // The object is fully encompassed by a selection run
        End, // The object either contains the end of a selection run or is the end of a run
        Both // The object contains an entire run or is the sole selected object in that run
    };

    // The current selection state for an object.  For blocks, the state refers to the state of the leaf
    // descendants (as described above in the HighlightState enum declaration).
    HighlightState selectionState() const { return m_bitfields.selectionState(); }
    virtual void setSelectionState(HighlightState);
    inline void setSelectionStateIfNeeded(HighlightState);
    bool canUpdateSelectionOnRootLineBoxes();

    // A single rectangle that encompasses all of the selected objects within this object.  Used to determine the tightest
    // possible bounding box for the selection. The rect returned is in the coordinate space of the paint invalidation container's backing.
    virtual LayoutRect selectionRectForRepaint(const RenderLayerModelObject* /*repaintContainer*/, bool /*clipToVisibleContent*/ = true) { return LayoutRect(); }

    virtual bool canBeSelectionLeaf() const { return false; }

    // Whether or not a given block needs to paint selection gaps.
    virtual bool shouldPaintSelectionGaps() const { return false; }

    // When performing a global document tear-down, or when going into the back/forward cache, the renderer of the document is cleared.
    bool renderTreeBeingDestroyed() const;

    void destroy();

    // Virtual function helpers for the deprecated Flexible Box Layout (display: -webkit-box).
    virtual bool isRenderDeprecatedFlexibleBox() const { return false; }
    // Virtual function helper for the new FlexibleBox Layout (display: -webkit-flex).
    inline bool isRenderFlexibleBox() const; // Defined in RenderElement.h.
    inline bool isFlexibleBoxIncludingDeprecated() const; // Defined in RenderElement.h.

    bool isRenderCombineText() const { return type() == Type::CombineText; }

    virtual int caretMinOffset() const;
    virtual int caretMaxOffset() const;

    virtual int previousOffset(int current) const;
    virtual int previousOffsetForBackwardDeletion(int current) const;
    virtual int nextOffset(int current) const;

    void imageChanged(CachedImage*, const IntRect* = nullptr) override;
    virtual void imageChanged(WrappedImagePtr, const IntRect* = nullptr) { }

    // Map points and quads through elements, potentially via 3d transforms. You should never need to call these directly; use
    // localToAbsolute/absoluteToLocal methods instead.
    virtual void mapLocalToContainer(const RenderLayerModelObject* repaintContainer, TransformState&, OptionSet<MapCoordinatesMode>, bool* wasFixed = nullptr) const;
    virtual void mapAbsoluteToLocalPoint(OptionSet<MapCoordinatesMode>, TransformState&) const;

    // Pushes state onto RenderGeometryMap about how to map coordinates from this renderer to its container, or ancestorToStopAt (whichever is encountered first).
    // Returns the renderer which was mapped to (container or ancestorToStopAt).
    virtual const RenderObject* pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap&) const;
    
    bool shouldUseTransformFromContainer(const RenderObject* container) const;
    void getTransformFromContainer(const RenderObject* container, const LayoutSize& offsetInContainer, TransformationMatrix&) const;
    
    void pushOntoTransformState(TransformState&, OptionSet<MapCoordinatesMode>, const RenderLayerModelObject* repaintContainer, const RenderElement* container, const LayoutSize& offsetInContainer, bool containerSkipped) const;
    void pushOntoGeometryMap(RenderGeometryMap&, const RenderLayerModelObject* repaintContainer, RenderElement* container, bool containerSkipped) const;

    bool participatesInPreserve3D(const RenderElement* container) const;

    virtual void addFocusRingRects(Vector<LayoutRect>&, const LayoutPoint& /* additionalOffset */, const RenderLayerModelObject* /* paintContainer */ = nullptr) const { };

    LayoutRect absoluteOutlineBounds() const { return outlineBoundsForRepaint(nullptr); }

    // FIXME: Renderers should not need to be notified about internal reparenting (webkit.org/b/224143).
    enum class IsInternalMove : bool { No, Yes };
    virtual void insertedIntoTree(IsInternalMove = IsInternalMove::No);
    virtual void willBeRemovedFromTree(IsInternalMove = IsInternalMove::No);

    void resetFragmentedFlowStateOnRemoval();
    void initializeFragmentedFlowStateOnInsertion();

    virtual String description() const;
    virtual String debugDescription() const;

    void addPDFURLRect(const PaintInfo&, const LayoutPoint&) const;

    bool isSkippedContent() const;

    bool isSkippedContentRoot() const;
    bool isSkippedContentForLayout() const;

protected:
    //////////////////////////////////////////
    // Helper functions. Dangerous to use!
    void setPreviousSibling(RenderObject* previous) { m_previous = previous; }
    void setNextSibling(RenderObject* next) { m_next = next; }
    void setParent(RenderElement*);
    //////////////////////////////////////////
    Node& nodeForNonAnonymous() const { ASSERT(!isAnonymous()); return m_node.get(); }

    void adjustRectForOutlineAndShadow(LayoutRect&) const;

    virtual void willBeDestroyed();

    void setNeedsPositionedMovementLayoutBit(bool b) { m_bitfields.setNeedsPositionedMovementLayout(b); }
    void setNormalChildNeedsLayoutBit(bool b) { m_bitfields.setNormalChildNeedsLayout(b); }
    void setPosChildNeedsLayoutBit(bool b) { m_bitfields.setPosChildNeedsLayout(b); }
    void setNeedsSimplifiedNormalFlowLayoutBit(bool b) { m_bitfields.setNeedsSimplifiedNormalFlowLayout(b); }

    virtual RenderFragmentedFlow* locateEnclosingFragmentedFlow() const;

    static FragmentedFlowState computedFragmentedFlowState(const RenderObject&);

    static VisibleRectContext visibleRectContextForRepaint();
    static VisibleRectContext visibleRectContextForSpatialNavigation();
    static VisibleRectContext visibleRectContextForRenderTreeAsText();

    bool isSetNeedsLayoutForbidden() const;

    void issueRepaint(std::optional<LayoutRect> partialRepaintRect = std::nullopt, ClipRepaintToLayer = ClipRepaintToLayer::No, ForceRepaint = ForceRepaint::No, std::optional<LayoutBoxExtent> additionalRepaintOutsets = std::nullopt) const;

private:
    void addAbsoluteRectForLayer(LayoutRect& result);
    void setLayerNeedsFullRepaint();
    void setLayerNeedsFullRepaintForPositionedMovementLayout();

#if PLATFORM(IOS_FAMILY)
    struct SelectionGeometries {
        Vector<SelectionGeometry> geometries;
        int maxLineNumber;
    };
    WEBCORE_EXPORT static SelectionGeometries collectSelectionGeometriesInternal(const SimpleRange&);
#endif

    Node* generatingPseudoHostElement() const;

    void propagateRepaintToParentWithOutlineAutoIfNeeded(const RenderLayerModelObject& repaintContainer, const LayoutRect& repaintRect) const;

    virtual bool isWBR() const { ASSERT_NOT_REACHED(); return false; }

    void setEverHadLayout(bool b) { m_bitfields.setEverHadLayout(b); }

    bool hasRareData() const { return m_bitfields.hasRareData(); }
    void setHasRareData(bool b) { m_bitfields.setHasRareData(b); }

#if ASSERT_ENABLED
    void setNeedsLayoutIsForbidden(bool flag) const { m_setNeedsLayoutForbidden = flag; }
    void checkBlockPositionedObjectsNeedLayout();
#endif

#if ASSERT_ENABLED
    bool m_hasAXObject : 1;
    mutable bool m_setNeedsLayoutForbidden : 1;
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
            , m_isRenderText(false)
            , m_isRenderBox(false)
            , m_isInline(true)
            , m_isReplacedOrInlineBlock(false)
            , m_horizontalWritingMode(true)
            , m_hasLayer(false)
            , m_hasNonVisibleOverflow(false)
            , m_hasTransformRelatedProperty(false)
            , m_everHadLayout(false)
            , m_childrenInline(false)
            , m_isExcludedFromNormalLayout(false)
            , m_positionedState(IsStaticallyPositioned)
            , m_selectionState(HighlightState::None)
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
        ADD_BOOLEAN_BITFIELD(isRenderText, IsRenderText);
        ADD_BOOLEAN_BITFIELD(isRenderBox, IsRenderBox);
        ADD_BOOLEAN_BITFIELD(isInline, IsInline);
        ADD_BOOLEAN_BITFIELD(isReplacedOrInlineBlock, IsReplacedOrInlineBlock);
        ADD_BOOLEAN_BITFIELD(horizontalWritingMode, HorizontalWritingMode);

        ADD_BOOLEAN_BITFIELD(hasLayer, HasLayer);
        ADD_BOOLEAN_BITFIELD(hasNonVisibleOverflow, HasNonVisibleOverflow); // Set in the case of overflow:auto/scroll/hidden
        ADD_BOOLEAN_BITFIELD(hasTransformRelatedProperty, HasTransformRelatedProperty);

        ADD_BOOLEAN_BITFIELD(everHadLayout, EverHadLayout);

        // from RenderBlock
        ADD_BOOLEAN_BITFIELD(childrenInline, ChildrenInline);
        
        ADD_BOOLEAN_BITFIELD(isExcludedFromNormalLayout, IsExcludedFromNormalLayout);

        // 3 bits remaining.

    private:
        unsigned m_positionedState : 2; // PositionedState
        unsigned m_selectionState : 3; // HighlightState
        unsigned m_fragmentedFlowState : 1; // FragmentedFlowState
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

        ALWAYS_INLINE HighlightState selectionState() const { return static_cast<HighlightState>(m_selectionState); }
        ALWAYS_INLINE void setSelectionState(HighlightState selectionState) { m_selectionState = selectionState; }
        
        ALWAYS_INLINE FragmentedFlowState fragmentedFlowState() const { return static_cast<FragmentedFlowState>(m_fragmentedFlowState); }
        ALWAYS_INLINE void setFragmentedFlowState(FragmentedFlowState fragmentedFlowState) { m_fragmentedFlowState = fragmentedFlowState; }

        ALWAYS_INLINE BoxDecorationState boxDecorationState() const { return static_cast<BoxDecorationState>(m_boxDecorationState); }
        ALWAYS_INLINE void setBoxDecorationState(BoxDecorationState boxDecorationState) { m_boxDecorationState = boxDecorationState; }
    };

    RenderObjectBitfields m_bitfields;

    CheckedRef<Node> m_node;

    CheckedPtr<RenderElement> m_parent;
    CheckedPtr<RenderObject> m_previous;
    PackedCheckedPtr<RenderObject> m_next;
    Type m_type;

    CheckedPtr<Layout::Box> m_layoutBox;

    // FIXME: This should be RenderElementRareData.
    class RenderObjectRareData {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        RenderObjectRareData();
        ~RenderObjectRareData();

        ADD_BOOLEAN_BITFIELD(hasReflection, HasReflection);
        ADD_BOOLEAN_BITFIELD(isRenderFragmentedFlow, IsRenderFragmentedFlow);
        ADD_BOOLEAN_BITFIELD(hasOutlineAutoAncestor, HasOutlineAutoAncestor);
        ADD_BOOLEAN_BITFIELD(paintContainmentApplies, PaintContainmentApplies);
#if ENABLE(LAYER_BASED_SVG_ENGINE)
        ADD_BOOLEAN_BITFIELD(hasSVGTransform, HasSVGTransform);
#endif
        ADD_ENUM_BITFIELD(trimmedMargins, TrimmedMargins, unsigned, 4);

        // From RenderElement
        std::unique_ptr<ReferencedSVGResources> referencedSVGResources;
        WeakPtr<RenderBlockFlow> backdropRenderer;

        // From RenderBox
        RefPtr<ControlPart> controlPart;
    };
    
    WEBCORE_EXPORT const RenderObject::RenderObjectRareData& rareData() const;
    RenderObjectRareData& ensureRareData();
    void removeRareData();
    
    typedef HashMap<const RenderObject*, std::unique_ptr<RenderObjectRareData>> RareDataMap;

    static RareDataMap& rareDataMap();

#undef ADD_BOOLEAN_BITFIELD
};

class RenderObject::SetLayoutNeededForbiddenScope {
public:
    explicit SetLayoutNeededForbiddenScope(const RenderObject&, bool isForbidden = true);
#if ASSERT_ENABLED
    ~SetLayoutNeededForbiddenScope();
private:
    CheckedRef<const RenderObject> m_renderObject;
    bool m_preexistingForbidden;
#endif
};

inline LocalFrame& RenderObject::frame() const
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

inline bool RenderObject::renderTreeBeingDestroyed() const
{
    return document().renderTreeBeingDestroyed();
}

inline bool RenderObject::isBeforeContent() const
{
    // Text nodes don't have their own styles, so ignore the style on a text node.
    if (isRenderText())
        return false;
    if (style().styleType() != PseudoId::Before)
        return false;
    return true;
}

inline bool RenderObject::isAfterContent() const
{
    // Text nodes don't have their own styles, so ignore the style on a text node.
    if (isRenderText())
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

inline void RenderObject::setSelectionStateIfNeeded(HighlightState state)
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

inline RenderFragmentedFlow* RenderObject::enclosingFragmentedFlow() const
{
    if (fragmentedFlowState() == NotInsideFragmentedFlow)
        return nullptr;

    return locateEnclosingFragmentedFlow();
}

inline bool RenderObject::isAnonymousBlock() const
{
    // This function must be kept in sync with anonymous block creation conditions in RenderBlock::createAnonymousBlock().
    // FIXME: That seems difficult. Can we come up with a simpler way to make behavior correct?
    // FIXME: Does this relatively long function benefit from being inlined?
    return isAnonymous()
        && (style().display() == DisplayType::Block || style().display() == DisplayType::Box)
        && style().styleType() == PseudoId::None
        && isRenderBlock()
#if ENABLE(MATHML)
        && !isRenderMathMLBlock()
#endif
        && !isRenderListMarker()
        && !isRenderFragmentedFlow()
        && !isRenderMultiColumnSet()
        && !isRenderView();
}

inline bool RenderObject::needsLayout() const
{
    return m_bitfields.needsLayout()
        || m_bitfields.normalChildNeedsLayout()
        || m_bitfields.posChildNeedsLayout()
        || m_bitfields.needsSimplifiedNormalFlowLayout()
        || m_bitfields.needsPositionedMovementLayout();
}

inline bool RenderObject::needsPositionedMovementLayoutOnly() const
{
    return m_bitfields.needsPositionedMovementLayout()
        && !m_bitfields.needsLayout()
        && !m_bitfields.normalChildNeedsLayout()
        && !m_bitfields.posChildNeedsLayout()
        && !m_bitfields.needsSimplifiedNormalFlowLayout();
}

inline void RenderObject::setNeedsLayoutAndPrefWidthsRecalc()
{
    setNeedsLayout();
    setPreferredLogicalWidthsDirty(true);
}

inline void RenderObject::setPositionState(PositionType position)
{
    ASSERT((position != PositionType::Absolute && position != PositionType::Fixed) || isRenderBox());
    m_bitfields.setPositionedState(static_cast<int>(position));
}

inline FloatQuad RenderObject::localToAbsoluteQuad(const FloatQuad& quad, OptionSet<MapCoordinatesMode> mode, bool* wasFixed) const
{
    return localToContainerQuad(quad, nullptr, mode, wasFixed);
}

inline auto RenderObject::visibleRectContextForRepaint() -> VisibleRectContext
{
    return { false, false, { VisibleRectContextOption::ApplyContainerClip, VisibleRectContextOption::ApplyCompositedContainerScrolls } };
}

inline auto RenderObject::visibleRectContextForSpatialNavigation() -> VisibleRectContext
{
    return { false, false, { VisibleRectContextOption::ApplyContainerClip, VisibleRectContextOption::ApplyCompositedContainerScrolls, VisibleRectContextOption::ApplyCompositedClips } };
}

inline auto RenderObject::visibleRectContextForRenderTreeAsText() -> VisibleRectContext
{
    return { false, false, { VisibleRectContextOption::ApplyContainerClip, VisibleRectContextOption::ApplyCompositedContainerScrolls, VisibleRectContextOption::ApplyCompositedClips, VisibleRectContextOption::CalculateAccurateRepaintRect  } };
}

inline bool RenderObject::isSetNeedsLayoutForbidden() const
{
#if ASSERT_ENABLED
    return m_setNeedsLayoutForbidden;
#else
    return false;
#endif
}

#if !ASSERT_ENABLED

inline RenderObject::SetLayoutNeededForbiddenScope::SetLayoutNeededForbiddenScope(const RenderObject&, bool)
{
}

#endif

inline void Node::setRenderer(RenderObject* renderer)
{
    m_rendererWithStyleFlags.setPointer(renderer);

    if (UNLIKELY(InspectorInstrumentationPublic::hasFrontends()))
        notifyInspectorOfRendererChange();
}

inline RenderObject* RenderObject::previousInFlowSibling() const
{
    auto* previousSibling = this->previousSibling();
    while (previousSibling && !previousSibling->isInFlow())
        previousSibling = previousSibling->previousSibling();
    return previousSibling;
}

inline RenderObject* RenderObject::nextInFlowSibling() const
{
    auto* nextSibling = this->nextSibling();
    while (nextSibling && !nextSibling->isInFlow())
        nextSibling = nextSibling->nextSibling();
    return nextSibling;
}

inline bool RenderObject::hasPotentiallyScrollableOverflow() const
{
    // We only need to test one overflow dimension since 'visible' and 'clip' always get accompanied
    // with 'clip' or 'visible' in the other dimension (see Style::Adjuster::adjust).
    return hasNonVisibleOverflow() && style().overflowX() != Overflow::Clip && style().overflowX() != Overflow::Visible;
}

#if ENABLE(MATHML)
inline bool RenderObject::isRenderMathMLRow() const
{
    switch (type()) {
    case Type::MathMLFenced:
    case Type::MathMLMath:
    case Type::MathMLMenclose:
    case Type::MathMLPadded:
    case Type::MathMLRoot:
    case Type::MathMLRow:
        return true;
    default:
        break;
    }
    return false;
}
#endif

inline bool RenderObject::isRenderTable() const
{
    switch (type()) {
    case Type::Table:
#if ENABLE(MATHML)
    case Type::MathMLTable:
#endif
        return true;
    default:
        break;
    }
    return false;
}

WTF::TextStream& operator<<(WTF::TextStream&, const RenderObject&);

#if ENABLE(TREE_DEBUGGING)
void printPaintOrderTreeForLiveDocuments();
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

namespace WTF {

template<> struct EnumTraits<WebCore::RepaintRectCalculation> {
    using values = EnumValues<
        WebCore::RepaintRectCalculation,
        WebCore::RepaintRectCalculation::Fast,
        WebCore::RepaintRectCalculation::Accurate
    >;
};

} // namespace WTF
