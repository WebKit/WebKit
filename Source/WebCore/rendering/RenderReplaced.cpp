/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011-2012. All rights reserved.
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

#include "config.h"
#include "RenderReplaced.h"

#include "DocumentMarkerController.h"
#include "ElementRuleCollector.h"
#include "FloatRoundedRect.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "HTMLElement.h"
#include "HTMLImageElement.h"
#include "HTMLParserIdioms.h"
#include "HighlightData.h"
#include "HighlightRegister.h"
#include "LayoutIntegrationLineIterator.h"
#include "LayoutIntegrationRunIterator.h"
#include "LayoutRepainter.h"
#include "RenderBlock.h"
#include "RenderFragmentedFlow.h"
#include "RenderImage.h"
#include "RenderLayer.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "RenderedDocumentMarker.h"
#include "RuntimeEnabledFeatures.h"
#include "Settings.h"
#include "VisiblePosition.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/StackStats.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderReplaced);

const int cDefaultWidth = 300;
const int cDefaultHeight = 150;

RenderReplaced::RenderReplaced(Element& element, RenderStyle&& style)
    : RenderBox(element, WTFMove(style), RenderReplacedFlag)
    , m_intrinsicSize(cDefaultWidth, cDefaultHeight)
{
    setReplaced(true);
}

RenderReplaced::RenderReplaced(Element& element, RenderStyle&& style, const LayoutSize& intrinsicSize)
    : RenderBox(element, WTFMove(style), RenderReplacedFlag)
    , m_intrinsicSize(intrinsicSize)
{
    setReplaced(true);
}

RenderReplaced::RenderReplaced(Document& document, RenderStyle&& style, const LayoutSize& intrinsicSize)
    : RenderBox(document, WTFMove(style), RenderReplacedFlag)
    , m_intrinsicSize(intrinsicSize)
{
    setReplaced(true);
}

RenderReplaced::~RenderReplaced() = default;

void RenderReplaced::willBeDestroyed()
{
    if (!renderTreeBeingDestroyed() && parent())
        parent()->dirtyLinesFromChangedChild(*this);

    RenderBox::willBeDestroyed();
}

void RenderReplaced::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBox::styleDidChange(diff, oldStyle);
    auto previousEffectiveZoom = oldStyle ? oldStyle->effectiveZoom() : RenderStyle::initialZoom();
    if (previousEffectiveZoom != style().effectiveZoom())
        intrinsicSizeChanged();
}

void RenderReplaced::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    ASSERT(needsLayout());
    
    LayoutRepainter repainter(*this, checkForRepaintDuringLayout());

    LayoutRect oldContentRect = replacedContentRect();
    
    setHeight(minimumReplacedHeight());

    updateLogicalWidth();
    updateLogicalHeight();

    clearOverflow();
    addVisualEffectOverflow();
    updateLayerTransform();
    invalidateBackgroundObscurationStatus();

    repainter.repaintAfterLayout();
    clearNeedsLayout();

    if (replacedContentRect() != oldContentRect)
        setPreferredLogicalWidthsDirty(true);
}

void RenderReplaced::intrinsicSizeChanged()
{
    int scaledWidth = static_cast<int>(cDefaultWidth * style().effectiveZoom());
    int scaledHeight = static_cast<int>(cDefaultHeight * style().effectiveZoom());
    m_intrinsicSize = IntSize(scaledWidth, scaledHeight);
    setNeedsLayoutAndPrefWidthsRecalc();
}

bool RenderReplaced::shouldDrawSelectionTint() const
{
    return selectionState() != HighlightState::None && !document().printing();
}

inline static bool draggedContentContainsReplacedElement(const Vector<RenderedDocumentMarker*>& markers, const Element& element)
{
    for (auto* marker : markers) {
        if (WTF::get<RefPtr<Node>>(marker->data()) == &element)
            return true;
    }
    return false;
}

Color RenderReplaced::calculateHighlightColor() const
{
    HighlightData highlightData;
#if ENABLE(APP_HIGHLIGHTS)
    if (auto appHighlightRegister = document().appHighlightRegisterIfExists()) {
        if (appHighlightRegister->highlightsVisibility() == HighlightVisibility::Visible) {
            for (auto& highlight : appHighlightRegister->map()) {
                for (auto& rangeData : highlight.value->rangesData()) {
                    if (!highlightData.setRenderRange(rangeData))
                        continue;

                    auto state = highlightData.highlightStateForRenderer(*this);
                    if (!isHighlighted(state, highlightData))
                        continue;

                    OptionSet<StyleColor::Options> styleColorOptions = { StyleColor::Options::UseSystemAppearance };
                    return theme().appHighlightColor(styleColorOptions);
                }
            }
        }
    }
#endif
    if (RuntimeEnabledFeatures::sharedFeatures().highlightAPIEnabled()) {
        if (auto highlightRegister = document().highlightRegisterIfExists()) {
            for (auto& highlight : highlightRegister->map()) {
                for (auto& rangeData : highlight.value->rangesData()) {
                    if (!highlightData.setRenderRange(rangeData))
                        continue;

                    auto state = highlightData.highlightStateForRenderer(*this);
                    if (!isHighlighted(state, highlightData))
                        continue;

                    if (auto highlightStyle = getUncachedPseudoStyle({ PseudoId::Highlight, highlight.key }, &style()))
                        return highlightStyle->backgroundColor();
                }
            }
        }
    }
    return Color();
}

void RenderReplaced::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!shouldPaint(paintInfo, paintOffset))
        return;

    LayoutPoint adjustedPaintOffset = paintOffset + location();

    if (paintInfo.phase == PaintPhase::EventRegion) {
        if (visibleToHitTesting()) {
            auto borderRect = LayoutRect(adjustedPaintOffset, size());
            auto borderRegion = approximateAsRegion(style().getRoundedBorderFor(borderRect));
            paintInfo.eventRegionContext->unite(borderRegion, style());
        }
        return;
    }

    SetLayoutNeededForbiddenScope scope(*this);

    GraphicsContextStateSaver savedGraphicsContext(paintInfo.context(), false);
    if (element() && element()->parentOrShadowHostElement()) {
        auto* parentContainer = element()->parentOrShadowHostElement();
        ASSERT(parentContainer);
        if (draggedContentContainsReplacedElement(document().markers().markersFor(*parentContainer, DocumentMarker::DraggedContent), *element())) {
            savedGraphicsContext.save();
            paintInfo.context().setAlpha(0.25);
        }
    }

    if (hasVisibleBoxDecorations() && paintInfo.phase == PaintPhase::Foreground)
        paintBoxDecorations(paintInfo, adjustedPaintOffset);
    
    if (paintInfo.phase == PaintPhase::Mask) {
        paintMask(paintInfo, adjustedPaintOffset);
        return;
    }

    LayoutRect paintRect = LayoutRect(adjustedPaintOffset, size());
    if (paintInfo.phase == PaintPhase::Outline || paintInfo.phase == PaintPhase::SelfOutline) {
        if (style().outlineWidth())
            paintOutline(paintInfo, paintRect);
        return;
    }

    if (paintInfo.phase != PaintPhase::Foreground && paintInfo.phase != PaintPhase::Selection)
        return;
    
    if (!paintInfo.shouldPaintWithinRoot(*this))
        return;
    
    Color highlightColor;
    if (!document().printing() && !paintInfo.paintBehavior.contains(PaintBehavior::ExcludeSelection))
        highlightColor = calculateHighlightColor();
    
    bool drawSelectionTint = shouldDrawSelectionTint();
    if (paintInfo.phase == PaintPhase::Selection) {
        if (selectionState() == HighlightState::None)
            return;
        drawSelectionTint = false;
    }

    bool completelyClippedOut = false;
    if (style().hasBorderRadius()) {
        completelyClippedOut = size().isEmpty();
        if (!completelyClippedOut) {
            // Push a clip if we have a border radius, since we want to round the foreground content that gets painted.
            paintInfo.context().save();
            auto pixelSnappedRoundedRect = style().getRoundedInnerBorderFor(paintRect,
                paddingTop() + borderTop(), paddingBottom() + borderBottom(), paddingLeft() + borderLeft(), paddingRight() + borderRight(), true, true).pixelSnappedRoundedRectForPainting(document().deviceScaleFactor());
            clipRoundedInnerRect(paintInfo.context(), paintRect, pixelSnappedRoundedRect);
        }
    }

    if (!completelyClippedOut) {
        paintReplaced(paintInfo, adjustedPaintOffset);

        if (style().hasBorderRadius())
            paintInfo.context().restore();
    }
        
    // The selection tint never gets clipped by border-radius rounding, since we want it to run right up to the edges of
    // surrounding content.
    if (drawSelectionTint) {
        LayoutRect selectionPaintingRect = localSelectionRect();
        selectionPaintingRect.moveBy(adjustedPaintOffset);
        paintInfo.context().fillRect(snappedIntRect(selectionPaintingRect), selectionBackgroundColor());
    }
    
    if (highlightColor.isVisible()) {
        auto selectionPaintingRect = localSelectionRect(false);
        selectionPaintingRect.moveBy(adjustedPaintOffset);
        paintInfo.context().fillRect(snappedIntRect(selectionPaintingRect), highlightColor);
    }
}

bool RenderReplaced::shouldPaint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if ((paintInfo.paintBehavior.contains(PaintBehavior::ExcludeSelection)) && isSelected())
        return false;

    if (paintInfo.phase != PaintPhase::Foreground
        && paintInfo.phase != PaintPhase::Outline
        && paintInfo.phase != PaintPhase::SelfOutline
        && paintInfo.phase != PaintPhase::Selection
        && paintInfo.phase != PaintPhase::Mask
        && paintInfo.phase != PaintPhase::EventRegion)
        return false;

    if (!paintInfo.shouldPaintWithinRoot(*this))
        return false;
        
    // if we're invisible or haven't received a layout yet, then just bail.
    if (style().visibility() != Visibility::Visible)
        return false;
    
    LayoutPoint adjustedPaintOffset = paintOffset + location();

    // Early exit if the element touches the edges.
    LayoutUnit top = adjustedPaintOffset.y() + visualOverflowRect().y();
    LayoutUnit bottom = adjustedPaintOffset.y() + visualOverflowRect().maxY();
    if (isSelected() && m_inlineBoxWrapper) {
        const LegacyRootInlineBox& rootBox = m_inlineBoxWrapper->root();
        LayoutUnit selTop = paintOffset.y() + rootBox.selectionTop();
        LayoutUnit selBottom = paintOffset.y() + selTop + rootBox.selectionHeight();
        top = std::min(selTop, top);
        bottom = std::max(selBottom, bottom);
    }
    
    LayoutRect localRepaintRect = paintInfo.rect;
    if (adjustedPaintOffset.x() + visualOverflowRect().x() >= localRepaintRect.maxX() || adjustedPaintOffset.x() + visualOverflowRect().maxX() <= localRepaintRect.x())
        return false;

    if (top >= localRepaintRect.maxY() || bottom <= localRepaintRect.y())
        return false;

    return true;
}

static inline RenderBlock* firstContainingBlockWithLogicalWidth(const RenderReplaced* replaced)
{
    // We have to lookup the containing block, which has an explicit width, which must not be equal to our direct containing block.
    // If the embedded document appears _after_ we performed the initial layout, our intrinsic size is 300x150. If our containing
    // block doesn't provide an explicit width, it's set to the 300 default, coming from the initial layout run.
    RenderBlock* containingBlock = replaced->containingBlock();
    if (!containingBlock)
        return 0;

    for (; containingBlock && !is<RenderView>(*containingBlock) && !containingBlock->isBody(); containingBlock = containingBlock->containingBlock()) {
        if (containingBlock->style().logicalWidth().isSpecified())
            return containingBlock;
    }

    return 0;
}

bool RenderReplaced::hasReplacedLogicalWidth() const
{
    if (style().logicalWidth().isSpecified())
        return true;

    if (style().logicalWidth().isAuto())
        return false;

    return firstContainingBlockWithLogicalWidth(this);
}

bool RenderReplaced::hasReplacedLogicalHeight() const
{
    if (style().logicalHeight().isAuto())
        return false;

    if (style().logicalHeight().isFixed())
        return true;

    if (style().logicalHeight().isPercentOrCalculated())
        return !hasAutoHeightOrContainingBlockWithAutoHeight();

    if (style().logicalHeight().isIntrinsic())
        return !style().hasAspectRatio();

    return false;
}

bool RenderReplaced::setNeedsLayoutIfNeededAfterIntrinsicSizeChange()
{
    setPreferredLogicalWidthsDirty(true);
    
    // If the actual area occupied by the image has changed and it is not constrained by style then a layout is required.
    bool imageSizeIsConstrained = style().logicalWidth().isSpecified() && style().logicalHeight().isSpecified();
    
    // FIXME: We only need to recompute the containing block's preferred size
    // if the containing block's size depends on the image's size (i.e., the container uses shrink-to-fit sizing).
    // There's no easy way to detect that shrink-to-fit is needed, always force a layout.
    bool containingBlockNeedsToRecomputePreferredSize =
        style().logicalWidth().isPercentOrCalculated()
        || style().logicalMaxWidth().isPercentOrCalculated()
        || style().logicalMinWidth().isPercentOrCalculated();
    
    // Flex layout algorithm uses the intrinsic image width/height even if width/height are specified.
    if (!imageSizeIsConstrained || containingBlockNeedsToRecomputePreferredSize || isFlexItem()) {
        setNeedsLayout();
        return true;
    }

    return false;
}
    
void RenderReplaced::computeAspectRatioInformationForRenderBox(RenderBox* contentRenderer, FloatSize& constrainedSize, double& intrinsicRatio) const
{
    FloatSize intrinsicSize;
    if (shouldApplySizeContainment(*this))
        RenderReplaced::computeIntrinsicRatioInformation(intrinsicSize, intrinsicRatio);
    else if (contentRenderer) {
        contentRenderer->computeIntrinsicRatioInformation(intrinsicSize, intrinsicRatio);

        if (style().aspectRatioType() == AspectRatioType::Ratio || (style().aspectRatioType() == AspectRatioType::AutoAndRatio && !intrinsicRatio))
            intrinsicRatio = style().aspectRatioWidth() / style().aspectRatioHeight();

        // Handle zoom & vertical writing modes here, as the embedded document doesn't know about them.
        intrinsicSize.scale(style().effectiveZoom());

        if (is<RenderImage>(*this))
            intrinsicSize.scale(downcast<RenderImage>(*this).imageDevicePixelRatio());

        // Update our intrinsic size to match what the content renderer has computed, so that when we
        // constrain the size below, the correct intrinsic size will be obtained for comparison against
        // min and max widths.
        if (intrinsicRatio && !intrinsicSize.isZero())
            m_intrinsicSize = LayoutSize(intrinsicSize);

        if (!isHorizontalWritingMode()) {
            if (intrinsicRatio)
                intrinsicRatio = 1 / intrinsicRatio;
            intrinsicSize = intrinsicSize.transposedSize();
        }
    } else {
        computeIntrinsicRatioInformation(intrinsicSize, intrinsicRatio);
        if (intrinsicRatio && !intrinsicSize.isZero())
            m_intrinsicSize = LayoutSize(isHorizontalWritingMode() ? intrinsicSize : intrinsicSize.transposedSize());
    }

    // Now constrain the intrinsic size along each axis according to minimum and maximum width/heights along the
    // opposite axis. So for example a maximum width that shrinks our width will result in the height we compute here
    // having to shrink in order to preserve the aspect ratio. Because we compute these values independently along
    // each axis, the final returned size may in fact not preserve the aspect ratio.
    // FIXME: In the long term, it might be better to just return this code more to the way it used to be before this
    // function was added, since all it has done is make the code more unclear.
    constrainedSize = intrinsicSize;
    if (intrinsicRatio && !intrinsicSize.isEmpty() && style().logicalWidth().isAuto() && style().logicalHeight().isAuto()) {
        // We can't multiply or divide by 'intrinsicRatio' here, it breaks tests, like fast/images/zoomed-img-size.html, which
        // can only be fixed once subpixel precision is available for things like intrinsicWidth/Height - which include zoom!
        constrainedSize.setWidth(RenderBox::computeReplacedLogicalHeight() * intrinsicSize.width() / intrinsicSize.height());
        constrainedSize.setHeight(RenderBox::computeReplacedLogicalWidth() * intrinsicSize.height() / intrinsicSize.width());
    }
}

LayoutRect RenderReplaced::replacedContentRect(const LayoutSize& intrinsicSize) const
{
    LayoutRect contentRect = contentBoxRect();
    if (intrinsicSize.isEmpty())
        return contentRect;

    ObjectFit objectFit = style().objectFit();

    LayoutRect finalRect = contentRect;
    switch (objectFit) {
    case ObjectFit::Contain:
    case ObjectFit::ScaleDown:
    case ObjectFit::Cover:
        finalRect.setSize(finalRect.size().fitToAspectRatio(intrinsicSize, objectFit == ObjectFit::Cover ? AspectRatioFitGrow : AspectRatioFitShrink));
        if (objectFit != ObjectFit::ScaleDown || finalRect.width() <= intrinsicSize.width())
            break;
        FALLTHROUGH;
    case ObjectFit::None:
        finalRect.setSize(intrinsicSize);
        break;
    case ObjectFit::Fill:
        break;
    }

    LengthPoint objectPosition = style().objectPosition();

    LayoutUnit xOffset = minimumValueForLength(objectPosition.x(), contentRect.width() - finalRect.width());
    LayoutUnit yOffset = minimumValueForLength(objectPosition.y(), contentRect.height() - finalRect.height());

    finalRect.move(xOffset, yOffset);

    return finalRect;
}

RoundedRect RenderReplaced::roundedContentBoxRect() const
{
    return style().getRoundedInnerBorderFor(borderBoxRect(),
        borderTop() + paddingTop(), borderBottom() + paddingBottom(),
        borderLeft() + paddingLeft(), borderRight() + paddingRight());
}

void RenderReplaced::computeIntrinsicRatioInformation(FloatSize& intrinsicSize, double& intrinsicRatio) const
{
    // If there's an embeddedContentBox() of a remote, referenced document available, this code-path should never be used.
    ASSERT(!embeddedContentBox() || shouldApplySizeContainment(*this));
    intrinsicSize = FloatSize(intrinsicLogicalWidth(), intrinsicLogicalHeight());

    if (style().hasAspectRatio()) {
        intrinsicRatio = style().logicalAspectRatio();
        if (style().aspectRatioType() == AspectRatioType::Ratio)
            return;
    }
    // Figure out if we need to compute an intrinsic ratio.
    if (!hasIntrinsicAspectRatio())
        return;

    if (intrinsicSize.isEmpty())
        return;

    intrinsicRatio = intrinsicSize.width() / intrinsicSize.height();
}

LayoutUnit RenderReplaced::computeConstrainedLogicalWidth(ShouldComputePreferred shouldComputePreferred) const
{
    if (shouldComputePreferred == ComputePreferred)
        return computeReplacedLogicalWidthRespectingMinMaxWidth(0_lu, ComputePreferred);

    // The aforementioned 'constraint equation' used for block-level, non-replaced
    // elements in normal flow:
    // 'margin-left' + 'border-left-width' + 'padding-left' + 'width' +
    // 'padding-right' + 'border-right-width' + 'margin-right' = width of
    // containing block
    LayoutUnit logicalWidth = containingBlock()->availableLogicalWidth();
    
    // This solves above equation for 'width' (== logicalWidth).
    LayoutUnit marginStart = minimumValueForLength(style().marginStart(), logicalWidth);
    LayoutUnit marginEnd = minimumValueForLength(style().marginEnd(), logicalWidth); 

    // FIXME: This expression does not align with the comment above, which is quoting https://www.w3.org/TR/CSS22/visudet.html#blockwidth.
    logicalWidth = std::max(0_lu, (logicalWidth - (marginStart + marginEnd + borderLeft() + borderRight())));
    return computeReplacedLogicalWidthRespectingMinMaxWidth(logicalWidth, shouldComputePreferred);
}

static inline LayoutUnit resolveWidthForRatio(LayoutUnit borderAndPaddingLogicalHeight, LayoutUnit borderAndPaddingLogicalWidth, LayoutUnit logicalHeight, double aspectRatio, BoxSizing boxSizing)
{
    if (boxSizing == BoxSizing::BorderBox)
        return LayoutUnit(round((logicalHeight + borderAndPaddingLogicalHeight) * aspectRatio)) - borderAndPaddingLogicalWidth;
    return LayoutUnit(round(logicalHeight * aspectRatio));
}

LayoutUnit RenderReplaced::computeReplacedLogicalWidth(ShouldComputePreferred shouldComputePreferred) const
{
    if (style().logicalWidth().isSpecified() || style().logicalWidth().isIntrinsic())
        return computeReplacedLogicalWidthRespectingMinMaxWidth(computeReplacedLogicalWidthUsing(MainOrPreferredSize, style().logicalWidth()), shouldComputePreferred);

    RenderBox* contentRenderer = embeddedContentBox();

    // 10.3.2 Inline, replaced elements: http://www.w3.org/TR/CSS21/visudet.html#inline-replaced-width
    double intrinsicRatio = 0;
    FloatSize constrainedSize;
    computeAspectRatioInformationForRenderBox(contentRenderer, constrainedSize, intrinsicRatio);

    if (style().logicalWidth().isAuto()) {
        bool computedHeightIsAuto = style().logicalHeight().isAuto();
        // FIXME: The hasIntrinsicWidth and hasIntrinsicHeight flags here are only updated for SVG elements. Ideally, it can be used in
        // all the cases so we don't need constrainedSize.width() > 0 and constrainedSize.height() > 0 clauses.
        bool hasIntrinsicWidth = constrainedSize.hasIntrinsicWidth || constrainedSize.width() > 0;
        bool hasIntrinsicHeight = constrainedSize.hasIntrinsicHeight || constrainedSize.height() > 0;

        // For flex or grid items where the logical height has been overriden then we should use that size to compute the replaced width as long as the flex or
        // grid item has an intrinsic size. It is possible (indeed, common) for an SVG graphic to have an intrinsic aspect ratio but not to have an intrinsic
        // width or height. There are also elements with intrinsic sizes but without intrinsic ratio (like an iframe).
        if (intrinsicRatio && (isFlexItem() || isGridItem()) && hasOverridingLogicalHeight() && ((hasIntrinsicWidth && hasIntrinsicHeight) || (contentRenderer && contentRenderer->isSVGRoot() && (hasIntrinsicWidth || hasIntrinsicHeight))))
            return computeReplacedLogicalWidthRespectingMinMaxWidth(roundToInt(round(overridingContentLogicalHeight() * intrinsicRatio)), shouldComputePreferred);

        // If 'height' and 'width' both have computed values of 'auto' and the element also has an intrinsic width, then that intrinsic width is the used value of 'width'.
        if (computedHeightIsAuto && hasIntrinsicWidth)
            return computeReplacedLogicalWidthRespectingMinMaxWidth(constrainedSize.width(), shouldComputePreferred);

        if (intrinsicRatio) {
            // If 'height' and 'width' both have computed values of 'auto' and the element has no intrinsic width, but does have an intrinsic height and intrinsic ratio;
            // or if 'width' has a computed value of 'auto', 'height' has some other computed value, and the element does have an intrinsic ratio; then the used value
            // of 'width' is: (used height) * (intrinsic ratio)
            if (!computedHeightIsAuto || (!hasIntrinsicWidth && hasIntrinsicHeight)) {
                LayoutUnit estimatedUsedWidth = hasIntrinsicWidth ? LayoutUnit(constrainedSize.width()) : computeConstrainedLogicalWidth(shouldComputePreferred);
                LayoutUnit logicalHeight = computeReplacedLogicalHeight(std::optional<LayoutUnit>(estimatedUsedWidth));
                BoxSizing boxSizing = style().hasAspectRatio() ? style().boxSizingForAspectRatio() : BoxSizing::ContentBox;
                return computeReplacedLogicalWidthRespectingMinMaxWidth(resolveWidthForRatio(borderAndPaddingLogicalHeight(), borderAndPaddingLogicalWidth(), logicalHeight, intrinsicRatio, boxSizing), shouldComputePreferred);
            }

            // If 'height' and 'width' both have computed values of 'auto' and the
            // element has an intrinsic ratio but no intrinsic height or width, then
            // the used value of 'width' is undefined in CSS 2.1. However, it is
            // suggested that, if the containing block's width does not itself depend
            // on the replaced element's width, then the used value of 'width' is
            // calculated from the constraint equation used for block-level,
            // non-replaced elements in normal flow.
            if (computedHeightIsAuto && !hasIntrinsicWidth && !hasIntrinsicHeight)
                return computeConstrainedLogicalWidth(shouldComputePreferred);
        }

        // Otherwise, if 'width' has a computed value of 'auto', and the element has an intrinsic width, then that intrinsic width is the used value of 'width'.
        if (hasIntrinsicWidth)
            return computeReplacedLogicalWidthRespectingMinMaxWidth(constrainedSize.width(), shouldComputePreferred);

        // Otherwise, if 'width' has a computed value of 'auto', but none of the conditions above are met, then the used value of 'width' becomes 300px. If 300px is too
        // wide to fit the device, UAs should use the width of the largest rectangle that has a 2:1 ratio and fits the device instead.
        // Note: We fall through and instead return intrinsicLogicalWidth() here - to preserve existing WebKit behavior, which might or might not be correct, or desired.
        // Changing this to return cDefaultWidth, will affect lots of test results. Eg. some tests assume that a blank <img> tag (which implies width/height=auto)
        // has no intrinsic size, which is wrong per CSS 2.1, but matches our behavior since a long time.
    }

    return computeReplacedLogicalWidthRespectingMinMaxWidth(intrinsicLogicalWidth(), shouldComputePreferred);
}

static inline LayoutUnit resolveHeightForRatio(LayoutUnit borderAndPaddingLogicalWidth, LayoutUnit borderAndPaddingLogicalHeight, LayoutUnit logicalWidth, double aspectRatio, BoxSizing boxSizing)
{
    if (boxSizing == BoxSizing::BorderBox)
        return LayoutUnit(round((logicalWidth + borderAndPaddingLogicalWidth) / aspectRatio)) - borderAndPaddingLogicalHeight;
    return LayoutUnit(round(logicalWidth / aspectRatio));
}

LayoutUnit RenderReplaced::computeReplacedLogicalHeight(std::optional<LayoutUnit> estimatedUsedWidth) const
{
    // 10.5 Content height: the 'height' property: http://www.w3.org/TR/CSS21/visudet.html#propdef-height
    if (hasReplacedLogicalHeight())
        return computeReplacedLogicalHeightRespectingMinMaxHeight(computeReplacedLogicalHeightUsing(MainOrPreferredSize, style().logicalHeight()));

    RenderBox* contentRenderer = embeddedContentBox();

    // 10.6.2 Inline, replaced elements: http://www.w3.org/TR/CSS21/visudet.html#inline-replaced-height
    double intrinsicRatio = 0;
    FloatSize constrainedSize;
    computeAspectRatioInformationForRenderBox(contentRenderer, constrainedSize, intrinsicRatio);

    bool widthIsAuto = style().logicalWidth().isAuto();
    // FIXME: The hasIntrinsicHeight and hasIntrinsicWidth flags here are only updated for SVG elements. Ideally, it can be used in
    // all the cases so we don't need constrainedSize.height() > 0 and constrainedSize.width() > 0 clauses.
    bool hasIntrinsicHeight = constrainedSize.hasIntrinsicHeight || constrainedSize.height() > 0;
    bool hasIntrinsicWidth = constrainedSize.hasIntrinsicWidth || constrainedSize.width() > 0;

    // See computeReplacedLogicalHeight() for a similar check for heights.
    if (intrinsicRatio && (isFlexItem() || isGridItem()) && hasOverridingLogicalWidth() && ((hasIntrinsicWidth && hasIntrinsicHeight) || (contentRenderer && contentRenderer->isSVGRoot() && (hasIntrinsicWidth || hasIntrinsicHeight))))
        return computeReplacedLogicalHeightRespectingMinMaxHeight(roundToInt(round(overridingContentLogicalWidth() / intrinsicRatio)));

    // If 'height' and 'width' both have computed values of 'auto' and the element also has an intrinsic height, then that intrinsic height is the used value of 'height'.
    if (widthIsAuto && hasIntrinsicHeight)
        return computeReplacedLogicalHeightRespectingMinMaxHeight(constrainedSize.height());

    // Otherwise, if 'height' has a computed value of 'auto', and the element has an intrinsic ratio then the used value of 'height' is:
    // (used width) / (intrinsic ratio)
    if (intrinsicRatio) {
        LayoutUnit usedWidth = estimatedUsedWidth ? estimatedUsedWidth.value() : availableLogicalWidth();
        BoxSizing boxSizing = BoxSizing::ContentBox;
        if (style().hasAspectRatio())
            boxSizing = style().boxSizingForAspectRatio();
        return computeReplacedLogicalHeightRespectingMinMaxHeight(resolveHeightForRatio(borderAndPaddingLogicalWidth(), borderAndPaddingLogicalHeight(), usedWidth, intrinsicRatio, boxSizing));
    }

    // Otherwise, if 'height' has a computed value of 'auto', and the element has an intrinsic height, then that intrinsic height is the used value of 'height'.
    if (hasIntrinsicHeight)
        return computeReplacedLogicalHeightRespectingMinMaxHeight(constrainedSize.height());

    // Otherwise, if 'height' has a computed value of 'auto', but none of the conditions above are met, then the used value of 'height' must be set to the height
    // of the largest rectangle that has a 2:1 ratio, has a height not greater than 150px, and has a width not greater than the device width.
    return computeReplacedLogicalHeightRespectingMinMaxHeight(intrinsicLogicalHeight());
}

void RenderReplaced::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    minLogicalWidth = maxLogicalWidth = intrinsicLogicalWidth();
}

void RenderReplaced::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    // We cannot resolve any percent logical width here as the available logical
    // width may not be set on our containing block.
    if (style().logicalWidth().isPercentOrCalculated())
        computeIntrinsicLogicalWidths(m_minPreferredLogicalWidth, m_maxPreferredLogicalWidth);
    else
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = computeReplacedLogicalWidth(ComputePreferred);

    const RenderStyle& styleToUse = style();
    if (styleToUse.logicalWidth().isPercentOrCalculated() || styleToUse.logicalMaxWidth().isPercentOrCalculated())
        m_minPreferredLogicalWidth = 0;

    if (styleToUse.logicalMinWidth().isFixed() && styleToUse.logicalMinWidth().value() > 0) {
        m_maxPreferredLogicalWidth = std::max(m_maxPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse.logicalMinWidth()));
        m_minPreferredLogicalWidth = std::max(m_minPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse.logicalMinWidth()));
    }
    
    if (styleToUse.logicalMaxWidth().isFixed()) {
        m_maxPreferredLogicalWidth = std::min(m_maxPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse.logicalMaxWidth()));
        m_minPreferredLogicalWidth = std::min(m_minPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse.logicalMaxWidth()));
    }

    LayoutUnit borderAndPadding = borderAndPaddingLogicalWidth();
    m_minPreferredLogicalWidth += borderAndPadding;
    m_maxPreferredLogicalWidth += borderAndPadding;

    setPreferredLogicalWidthsDirty(false);
}

VisiblePosition RenderReplaced::positionForPoint(const LayoutPoint& point, const RenderFragmentContainer* fragment)
{
    auto [top, bottom] = [&] {
        if (auto run = LayoutIntegration::runFor(*this)) {
            auto line = run.line();
            return std::make_pair(line->selectionTopForHitTesting(), line->selectionBottom());
        }
        return std::make_pair(logicalTop(), logicalBottom());
    }();

    LayoutUnit blockDirectionPosition = isHorizontalWritingMode() ? point.y() + y() : point.x() + x();
    LayoutUnit lineDirectionPosition = isHorizontalWritingMode() ? point.x() + x() : point.y() + y();
    
    if (blockDirectionPosition < top)
        return createVisiblePosition(caretMinOffset(), Affinity::Downstream); // coordinates are above
    
    if (blockDirectionPosition >= bottom)
        return createVisiblePosition(caretMaxOffset(), Affinity::Downstream); // coordinates are below
    
    if (element()) {
        if (lineDirectionPosition <= logicalLeft() + (logicalWidth() / 2))
            return createVisiblePosition(0, Affinity::Downstream);
        return createVisiblePosition(1, Affinity::Downstream);
    }

    return RenderBox::positionForPoint(point, fragment);
}

LayoutRect RenderReplaced::selectionRectForRepaint(const RenderLayerModelObject* repaintContainer, bool clipToVisibleContent)
{
    ASSERT(!needsLayout());

    if (!isSelected())
        return LayoutRect();
    
    LayoutRect rect = localSelectionRect();
    if (clipToVisibleContent)
        return computeRectForRepaint(rect, repaintContainer);
    return localToContainerQuad(FloatRect(rect), repaintContainer).enclosingBoundingBox();
}

LayoutRect RenderReplaced::localSelectionRect(bool checkWhetherSelected) const
{
    if (checkWhetherSelected && !isSelected())
        return LayoutRect();

    if (!m_inlineBoxWrapper)
        // We're a block-level replaced element.  Just return our own dimensions.
        return LayoutRect(LayoutPoint(), size());
    
    const LegacyRootInlineBox& rootBox = m_inlineBoxWrapper->root();
    LayoutUnit newLogicalTop { rootBox.blockFlow().style().isFlippedBlocksWritingMode() ? m_inlineBoxWrapper->logicalBottom() - rootBox.selectionBottom() : rootBox.selectionTop() - m_inlineBoxWrapper->logicalTop() };
    if (rootBox.blockFlow().style().isHorizontalWritingMode())
        return LayoutRect(0_lu, newLogicalTop, width(), rootBox.selectionHeight());
    return LayoutRect(newLogicalTop, 0_lu, rootBox.selectionHeight(), height());
}

bool RenderReplaced::isSelected() const
{
    return isHighlighted(selectionState(), view().selection());
}

bool RenderReplaced::isHighlighted(HighlightState state, const HighlightData& rangeData) const
{
    if (state == HighlightState::None)
        return false;
    if (state == HighlightState::Inside)
        return true;

    auto selectionStart = rangeData.startOffset();
    auto selectionEnd = rangeData.endOffset();
    if (state == HighlightState::Start)
        return !selectionStart;

    unsigned end = element()->hasChildNodes() ? element()->countChildNodes() : 1;
    if (state == HighlightState::End)
        return selectionEnd == end;
    if (state == HighlightState::Both)
        return !selectionStart && selectionEnd == end;
    ASSERT_NOT_REACHED();
    return false;
}

LayoutRect RenderReplaced::clippedOverflowRect(const RenderLayerModelObject* repaintContainer, VisibleRectContext context) const
{
    if (style().visibility() != Visibility::Visible && !enclosingLayer()->hasVisibleContent())
        return LayoutRect();

    // The selectionRect can project outside of the overflowRect, so take their union
    // for repainting to avoid selection painting glitches.
    LayoutRect r = unionRect(localSelectionRect(false), visualOverflowRect());
    // FIXME: layoutDelta needs to be applied in parts before/after transforms and
    // repaint containers. https://bugs.webkit.org/show_bug.cgi?id=23308
    r.move(view().frameView().layoutContext().layoutDelta());
    return computeRect(r, repaintContainer, context);
}

bool RenderReplaced::isContentLikelyVisibleInViewport()
{
    if (!isVisibleIgnoringGeometry())
        return false;

    auto& frameView = view().frameView();
    auto visibleRect = LayoutRect(frameView.windowToContents(frameView.windowClipRect()));
    auto contentRect = computeRectForRepaint(replacedContentRect(), nullptr);

    // Content rectangle may be empty because it is intrinsically sized and the content has not loaded yet.
    if (contentRect.isEmpty() && (style().logicalWidth().isAuto() || style().logicalHeight().isAuto()))
        return visibleRect.contains(contentRect.location());

    return visibleRect.intersects(contentRect);
}

bool RenderReplaced::needsPreferredWidthsRecalculation() const
{
    // If the height is a percentage and the width is auto, then the containingBlocks's height changing can cause this node to change it's preferred width because it maintains aspect ratio.
    return hasRelativeLogicalHeight() && style().logicalWidth().isAuto();
}

}
