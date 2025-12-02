/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2018 Google Inc. All rights reserved.
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

#include "BackgroundPainter.h"
#include "BorderShape.h"
#include "DocumentMarkerController.h"
#include "ElementRuleCollector.h"
#include "FloatRoundedRect.h"
#include "GraphicsContext.h"
#include "HTMLElement.h"
#include "HTMLImageElement.h"
#include "HTMLParserIdioms.h"
#include "HighlightRegistry.h"
#include "InlineIteratorBox.h"
#include "InlineIteratorLineBoxInlines.h"
#include "LayoutRepainter.h"
#include "LineSelection.h"
#include "LocalFrame.h"
#include "PositionedLayoutConstraints.h"
#include "RenderBlock.h"
#include "RenderBoxInlines.h"
#include "RenderChildIterator.h"
#include "RenderElementStyleInlines.h"
#include "RenderFlexibleBox.h"
#include "RenderFragmentedFlow.h"
#include "RenderHTMLCanvas.h"
#include "RenderHighlight.h"
#include "RenderImage.h"
#include "RenderLayer.h"
#include "RenderLayoutState.h"
#include "RenderObjectInlines.h"
#include "RenderStyleInlines.h"
#include "RenderStyleSetters.h"
#include "RenderTheme.h"
#include "RenderVideo.h"
#include "RenderView.h"
#include "RenderedDocumentMarker.h"
#include "Settings.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "VisiblePosition.h"
#include <wtf/StackStats.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/TypeCasts.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(RenderReplaced);

const int cDefaultWidth = 300;
const int cDefaultHeight = 150;

RenderReplaced::RenderReplaced(Type type, Element& element, RenderStyle&& style, OptionSet<ReplacedFlag> flags)
    : RenderBox(type, element, WTFMove(style), { }, flags)
    , m_intrinsicSize(cDefaultWidth, cDefaultHeight)
{
    ASSERT(element.isReplaced(&this->style()) || type == Type::Image);
    setBlockLevelReplacedOrAtomicInline(true);
    ASSERT(isRenderReplaced());
}

RenderReplaced::RenderReplaced(Type type, Element& element, RenderStyle&& style, const LayoutSize& intrinsicSize, OptionSet<ReplacedFlag> flags)
    : RenderBox(type, element, WTFMove(style), { }, flags)
    , m_intrinsicSize(intrinsicSize)
{
    ASSERT(element.isReplaced(&this->style()) || type == Type::Image);
    setBlockLevelReplacedOrAtomicInline(true);
    ASSERT(isRenderReplaced());
}

RenderReplaced::RenderReplaced(Type type, Document& document, RenderStyle&& style, const LayoutSize& intrinsicSize, OptionSet<ReplacedFlag> flags)
    : RenderBox(type, document, WTFMove(style), { }, flags)
    , m_intrinsicSize(intrinsicSize)
{
    setBlockLevelReplacedOrAtomicInline(true);
    ASSERT(isRenderReplaced());
}

RenderReplaced::~RenderReplaced() = default;

bool RenderReplaced::shouldRespectZeroIntrinsicWidth() const
{
    // Per CSSWG resolution, 0px intrinsic width should be respected for SVG
    // items and coerce the intrinsic height to 0px as well. Note that this is
    // not the case for 0px intrinsic height SVGs or for other RenderReplaced items.
    return false;
}

void RenderReplaced::willBeDestroyed()
{
    if (!renderTreeBeingDestroyed() && parent())
        parent()->dirtyLineFromChangedChild();

    RenderBox::willBeDestroyed();
}

void RenderReplaced::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBox::styleDidChange(diff, oldStyle);
    auto previousUsedZoom = oldStyle ? oldStyle->usedZoom() : RenderStyle::initialZoom();
    if (previousUsedZoom != style().usedZoom())
        intrinsicSizeChanged();
}

static bool shouldRepaintOnSizeChange(RenderReplaced& renderer)
{
    if (is<RenderHTMLCanvas>(renderer))
        return true;

#if ENABLE(VIDEO)
    if (auto* renderImage = dynamicDowncast<RenderImage>(renderer); renderImage && !is<RenderMedia>(*renderImage) && !renderImage->isShowingMissingOrImageError())
        return true;
#endif

    return false;
}

void RenderReplaced::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    ASSERT(needsLayout());
    
    LayoutRepainter repainter(*this);

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

    if (replacedContentRect() != oldContentRect) {
        setNeedsPreferredWidthsUpdate();
        if (shouldRepaintOnSizeChange(*this))
            repaint();
    }
}

void RenderReplaced::intrinsicSizeChanged()
{
    int scaledWidth = static_cast<int>(cDefaultWidth * style().usedZoom());
    int scaledHeight = static_cast<int>(cDefaultHeight * style().usedZoom());
    m_intrinsicSize = IntSize(scaledWidth, scaledHeight);
    setNeedsLayoutAndPreferredWidthsUpdate();
}

bool RenderReplaced::shouldDrawSelectionTint() const
{
    return selectionState() != HighlightState::None && !document().printing();
}

inline static bool contentContainsReplacedElement(const Vector<WeakPtr<RenderedDocumentMarker>>& markers, const Element& element)
{
    for (auto& marker : markers) {
        if (marker->type() == DocumentMarkerType::DraggedContent) {
            if (std::get<RefPtr<Node>>(marker->data()) == &element)
                return true;
        } else if (marker->type() == DocumentMarkerType::TransparentContent) {
            if (std::get<DocumentMarker::TransparentContentData>(marker->data()).node == &element)
                return true;
        }
    }
    return false;
}

Color RenderReplaced::calculateHighlightColor() const
{
    RenderHighlight renderHighlight;
#if ENABLE(APP_HIGHLIGHTS)
    if (auto appHighlightRegistry = document().appHighlightRegistryIfExists()) {
        if (appHighlightRegistry->highlightsVisibility() == HighlightVisibility::Visible) {
            for (auto& highlight : appHighlightRegistry->map()) {
                for (auto& highlightRange : highlight.value->highlightRanges()) {
                    if (!renderHighlight.setRenderRange(highlightRange))
                        continue;

                    auto state = renderHighlight.highlightStateForRenderer(*this);
                    if (!isHighlighted(state, renderHighlight))
                        continue;

                    OptionSet<StyleColorOptions> styleColorOptions = { StyleColorOptions::UseSystemAppearance };
                    return theme().annotationHighlightBackgroundColor(styleColorOptions);
                }
            }
        }
    }
#endif
    if (auto highlightRegistry = document().highlightRegistryIfExists()) {
        for (auto& highlight : highlightRegistry->map()) {
            for (auto& highlightRange : highlight.value->highlightRanges()) {
                if (!renderHighlight.setRenderRange(highlightRange))
                    continue;

                auto state = renderHighlight.highlightStateForRenderer(*this);
                if (!isHighlighted(state, renderHighlight))
                    continue;

                if (auto highlightStyle = getCachedPseudoStyle({ PseudoElementType::Highlight, highlight.key }, &style()))
                    return highlightStyle->colorResolvingCurrentColor(highlightStyle->backgroundColor());
            }
        }
    }

    if (document().settings().scrollToTextFragmentEnabled()) {
        if (auto highlightRegistry = document().fragmentHighlightRegistryIfExists()) {
            for (auto& highlight : highlightRegistry->map()) {
                for (auto& highlightRange : highlight.value->highlightRanges()) {
                    if (!renderHighlight.setRenderRange(highlightRange))
                        continue;

                    auto state = renderHighlight.highlightStateForRenderer(*this);
                    if (!isHighlighted(state, renderHighlight))
                        continue;

                    OptionSet<StyleColorOptions> styleColorOptions = { StyleColorOptions::UseSystemAppearance };
                    return theme().annotationHighlightBackgroundColor(styleColorOptions);
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
#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
        if (isRenderOrLegacyRenderSVGRoot() && !isSkippedContentRoot(*this))
            paintReplaced(paintInfo, adjustedPaintOffset);
        else if (visibleToHitTesting()) {
#else
        if (visibleToHitTesting()) {
#endif
            auto borderRect = LayoutRect(adjustedPaintOffset, size());
            auto borderShape = BorderShape::shapeForBorderRect(style(), borderRect);
            paintInfo.eventRegionContext()->unite(borderShape.deprecatedPixelSnappedRoundedRect(document().deviceScaleFactor()), *this, style());
        }
        return;
    }

    if (paintInfo.phase == PaintPhase::Accessibility) {
        paintInfo.accessibilityRegionContext()->takeBounds(*this, adjustedPaintOffset);
        return;
    }

    SetLayoutNeededForbiddenScope scope(*this);

    GraphicsContextStateSaver savedGraphicsContext(paintInfo.context(), false);
    if (element() && element()->parentOrShadowHostElement()) {
        auto* parentContainer = element()->parentOrShadowHostElement();
        ASSERT(parentContainer);
        CheckedPtr markers = document().markersIfExists();
        if (markers) {
            if (contentContainsReplacedElement(markers->markersFor(*parentContainer, DocumentMarkerType::DraggedContent), *element())) {
                savedGraphicsContext.save();
                paintInfo.context().setAlpha(0.25);
            }
            if (contentContainsReplacedElement(markers->markersFor(*parentContainer, DocumentMarkerType::TransparentContent), *element())) {
                savedGraphicsContext.save();
                paintInfo.context().setAlpha(0.0);
            }
        }
    }

    if (hasVisibleBoxDecorations() && paintInfo.phase == PaintPhase::Foreground)
        paintBoxDecorations(paintInfo, adjustedPaintOffset);
    
    if (paintInfo.phase == PaintPhase::Mask) {
        paintMask(paintInfo, adjustedPaintOffset);
        return;
    }

    if (paintInfo.phase == PaintPhase::ClippingMask && style().usedVisibility() == Visibility::Visible) {
        paintClippingMask(paintInfo, adjustedPaintOffset);
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
            clipToContentBoxShape(paintInfo.context(), adjustedPaintOffset, document().deviceScaleFactor());
        }
    }

    if (!completelyClippedOut) {
        if (!isSkippedContentRoot(*this))
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

    if (paintInfo.paintBehavior.contains(PaintBehavior::ExcludeReplacedContentExceptForIFrames) && !isRenderIFrame())
        return false;

    if (paintInfo.phase != PaintPhase::Foreground
        && paintInfo.phase != PaintPhase::Outline
        && paintInfo.phase != PaintPhase::SelfOutline
        && paintInfo.phase != PaintPhase::Selection
        && paintInfo.phase != PaintPhase::Mask
        && paintInfo.phase != PaintPhase::ClippingMask
        && paintInfo.phase != PaintPhase::EventRegion
        && paintInfo.phase != PaintPhase::Accessibility)
        return false;

    if (!paintInfo.shouldPaintWithinRoot(*this))
        return false;
        
    // if we're invisible or haven't received a layout yet, then just bail.
    if (style().usedVisibility() != Visibility::Visible)
        return false;
    
    LayoutRect paintRect(visualOverflowRect());
    paintRect.moveBy(paintOffset + location());

    // Early exit if the element touches the edges.
    LayoutUnit top = paintRect.y();
    LayoutUnit bottom = paintRect.maxY();

    LayoutRect localRepaintRect = paintInfo.rect;
    if (paintRect.x() >= localRepaintRect.maxX() || paintRect.maxX() <= localRepaintRect.x())
        return false;

    if (top >= localRepaintRect.maxY() || bottom <= localRepaintRect.y())
        return false;

    return true;
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
    setNeedsPreferredWidthsUpdate();

    // If the actual area occupied by the image has changed and it is not constrained by style then a layout is required.
    bool imageSizeIsConstrained = style().logicalWidth().isSpecified() && style().logicalHeight().isSpecified()
        && !style().logicalMinWidth().isIntrinsic() && !style().logicalMaxWidth().isIntrinsic()
        && !hasAutoHeightOrContainingBlockWithAutoHeight(UpdatePercentageHeightDescendants::No);

    // FIXME: We only need to recompute the containing block's preferred size
    // if the containing block's size depends on the image's size (i.e., the container uses shrink-to-fit sizing).
    // There's no easy way to detect that shrink-to-fit is needed, always force a layout.
    bool containingBlockNeedsToRecomputePreferredSize =
        style().logicalWidth().isPercentOrCalculated()
        || style().logicalMaxWidth().isPercentOrCalculated()
        || style().logicalMinWidth().isPercentOrCalculated();

    // Flex and grid layout use the intrinsic image width/height even if width/height are specified.
    if (!imageSizeIsConstrained || containingBlockNeedsToRecomputePreferredSize || isFlexItem() || isGridItem()) {
        setNeedsLayout();
        return true;
    }

    return false;
}

static bool isVideoWithDefaultObjectSize(const RenderReplaced* maybeVideo)
{
#if ENABLE(VIDEO)
    if (auto* video = dynamicDowncast<RenderVideo>(maybeVideo))
        return video->hasDefaultObjectSize();
#else
    UNUSED_PARAM(maybeVideo);
#endif
    return false;
} 

void RenderReplaced::computeAspectRatioInformationForRenderBox(RenderBox* contentRenderer, FloatSize& constrainedSize, FloatSize& preferredAspectRatio) const
{
    FloatSize intrinsicSize;
    if (shouldApplySizeOrInlineSizeContainment()) {
        intrinsicSize = RenderReplaced::computeIntrinsicSize();
        preferredAspectRatio = RenderReplaced::preferredAspectRatio();
    } else if (contentRenderer) {
        if (auto* renderReplaced = dynamicDowncast<RenderReplaced>(contentRenderer)) {
            intrinsicSize = renderReplaced->computeIntrinsicSize();
            preferredAspectRatio = renderReplaced->preferredAspectRatio();
        }
        if (style().aspectRatio().isRatio() || (style().aspectRatio().isAutoAndRatio() && preferredAspectRatio.isEmpty()))
            preferredAspectRatio = FloatSize::narrowPrecision(style().aspectRatio().width().value, style().aspectRatio().height().value);

        // Handle zoom & vertical writing modes here, as the embedded document doesn't know about them.
        intrinsicSize.scale(style().usedZoom());

        if (auto* image = dynamicDowncast<RenderImage>(*this))
            intrinsicSize.scale(image->imageDevicePixelRatio());

        // Update our intrinsic size to match what the content renderer has computed, so that when we
        // constrain the size below, the correct intrinsic size will be obtained for comparison against
        // min and max widths.
        if (!preferredAspectRatio.isEmpty() && !intrinsicSize.isZero())
            m_intrinsicSize = LayoutSize(intrinsicSize);

        if (!isHorizontalWritingMode()) {
            if (!preferredAspectRatio.isEmpty())
                preferredAspectRatio = preferredAspectRatio.transposedSize();
            intrinsicSize = intrinsicSize.transposedSize();
        }
    } else {
        intrinsicSize = computeIntrinsicSize();
        preferredAspectRatio = this->preferredAspectRatio();
        if (!preferredAspectRatio.isEmpty() && !intrinsicSize.isZero())
            m_intrinsicSize = LayoutSize(isHorizontalWritingMode() ? intrinsicSize : intrinsicSize.transposedSize());
    }
    constrainedSize = intrinsicSize;
}

void RenderReplaced::computeIntrinsicSizesConstrainedByTransferredMinMaxSizes(RenderBox* contentRenderer, FloatSize& intrinsicSize, FloatSize& intrinsicRatio) const
{
    computeAspectRatioInformationForRenderBox(contentRenderer, intrinsicSize, intrinsicRatio);

    // Now constrain the intrinsic size along each axis according to minimum and maximum width/heights along the
    // opposite axis. So for example a maximum width that shrinks our width will result in the height we compute here
    // having to shrink in order to preserve the aspect ratio. Because we compute these values independently along
    // each axis, the final returned size may in fact not preserve the aspect ratio.
    auto& style = this->style();
    auto computedLogicalHeight = style.logicalHeight();
    bool logicalHeightBehavesAsAuto = computedLogicalHeight.isAuto() || (computedLogicalHeight.isPercentOrCalculated() && !percentageLogicalHeightIsResolvable());
    if (!intrinsicRatio.isZero() && style.logicalWidth().isAuto() && logicalHeightBehavesAsAuto) {
        auto removeBorderAndPaddingFromMinMaxSizes = [](LayoutUnit& minSize, LayoutUnit &maxSize, LayoutUnit borderAndPadding) {
            minSize = std::max(0_lu, minSize - borderAndPadding);
            maxSize = std::max(0_lu, maxSize - borderAndPadding);
        };

        auto [minLogicalWidth, maxLogicalWidth] = computeMinMaxLogicalWidthFromAspectRatio();
        removeBorderAndPaddingFromMinMaxSizes(minLogicalWidth, maxLogicalWidth, borderAndPaddingLogicalWidth());

        auto [minLogicalHeight, maxLogicalHeight] = computeMinMaxLogicalHeightFromAspectRatio();
        removeBorderAndPaddingFromMinMaxSizes(minLogicalHeight, maxLogicalHeight, borderAndPaddingLogicalHeight());

        intrinsicSize.setWidth(std::clamp(LayoutUnit { intrinsicSize.width() }, minLogicalWidth, maxLogicalWidth));
        intrinsicSize.setHeight(std::clamp(LayoutUnit { intrinsicSize.height() }, minLogicalHeight, maxLogicalHeight));
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
        [[fallthrough]];
    case ObjectFit::None:
        finalRect.setSize(intrinsicSize);
        break;
    case ObjectFit::Fill:
        break;
    }

    auto& objectPosition = style().objectPosition();

    auto xOffset = Style::evaluate<LayoutUnit>(objectPosition.x, contentRect.width() - finalRect.width(), Style::ZoomNeeded { });
    auto yOffset = Style::evaluate<LayoutUnit>(objectPosition.y, contentRect.height() - finalRect.height(), Style::ZoomNeeded { });

    finalRect.move(xOffset, yOffset);

    return finalRect;
}

double RenderReplaced::computeIntrinsicAspectRatio() const
{
    FloatSize intrinsicRatio;
    FloatSize intrinsicSize;
    computeAspectRatioInformationForRenderBox(embeddedContentBox(), intrinsicSize, intrinsicRatio);
    return intrinsicRatio.aspectRatioDouble();
}

FloatSize RenderReplaced::computeIntrinsicSize() const
{
    // If there's an embeddedContentBox() of a remote, referenced document available, this code-path should never be used.
    ASSERT(!embeddedContentBox() || shouldApplySizeOrInlineSizeContainment());
    return { intrinsicLogicalWidth(), intrinsicLogicalHeight() };
}

FloatSize RenderReplaced::preferredAspectRatio() const
{
    // If there's an embeddedContentBox() of a remote, referenced document available, this code-path should never be used.
    ASSERT(!embeddedContentBox() || shouldApplySizeOrInlineSizeContainment());
    auto intrinsicSize = FloatSize(intrinsicLogicalWidth(), intrinsicLogicalHeight());
    FloatSize preferredAspectRatio;

    if (style().hasAspectRatio()) {
        preferredAspectRatio = FloatSize::narrowPrecision(style().aspectRatioLogicalWidth().value, style().aspectRatioLogicalHeight().value);
        if (style().aspectRatio().isRatio() || isVideoWithDefaultObjectSize(this))
            return preferredAspectRatio;
    }
    // Figure out if we need to compute an intrinsic ratio.
    if (!RenderBox::hasIntrinsicAspectRatio() && !isRenderOrLegacyRenderSVGRoot())
        return preferredAspectRatio;

    // After supporting contain-intrinsic-size, the intrinsicSize of size containment is not always empty.
    if (intrinsicSize.isEmpty() || shouldApplySizeContainment())
        return preferredAspectRatio;

    return intrinsicSize;
}

LayoutUnit RenderReplaced::computeConstrainedLogicalWidth() const
{
    // The aforementioned 'constraint equation' used for block-level, non-replaced
    // elements in normal flow:
    // 'margin-left' + 'border-left-width' + 'padding-left' + 'width' +
    // 'padding-right' + 'border-right-width' + 'margin-right' = width of
    // containing block
    // see https://www.w3.org/TR/CSS22/visudet.html#blockwidth
    LayoutUnit logicalWidth = isOutOfFlowPositioned() ? containingBlock()->clientLogicalWidth() : containingBlock()->contentBoxLogicalWidth();

    // This solves above equation for 'width' (== logicalWidth).
    auto marginStart = Style::evaluateMinimum<LayoutUnit>(style().marginStart(), logicalWidth, style().usedZoomForLength());
    auto marginEnd = Style::evaluateMinimum<LayoutUnit>(style().marginEnd(), logicalWidth, style().usedZoomForLength());

    return std::max(0_lu, (logicalWidth - (marginStart + marginEnd + borderLeft() + borderRight() + paddingLeft() + paddingRight())));
}

void RenderReplaced::computeAspectRatioAdjustedIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    computeIntrinsicLogicalWidths(minLogicalWidth, maxLogicalWidth);

    if (!hasIntrinsicAspectRatio())
        return;

    auto& style = this->style();
    auto computedAspectRatio = computeIntrinsicAspectRatio();
    auto computedIntrinsicLogicalWidth = minLogicalWidth;

    if (auto fixedLogicalHeight = style.logicalHeight().tryFixed())
        computedIntrinsicLogicalWidth = fixedLogicalHeight->resolveZoom(style.usedZoomForLength()) * computedAspectRatio;

    if (auto fixedLogicalMaxHeight = style.logicalMaxHeight().tryFixed())
        computedIntrinsicLogicalWidth = std::min(computedIntrinsicLogicalWidth, LayoutUnit { fixedLogicalMaxHeight->resolveZoom(style.usedZoomForLength()) * computedAspectRatio });

    if (auto fixedLogicalMinHeight = style.logicalMinHeight().tryFixed())
        computedIntrinsicLogicalWidth = std::max(computedIntrinsicLogicalWidth, LayoutUnit { fixedLogicalMinHeight->resolveZoom(style.usedZoomForLength()) * computedAspectRatio });

    minLogicalWidth = computedIntrinsicLogicalWidth;
    maxLogicalWidth = minLogicalWidth;
}

static inline LayoutUnit resolveWidthForRatio(LayoutUnit borderAndPaddingLogicalHeight, LayoutUnit borderAndPaddingLogicalWidth, LayoutUnit logicalHeight, double aspectRatio, BoxSizing boxSizing)
{
    if (boxSizing == BoxSizing::BorderBox)
        return LayoutUnit((logicalHeight + borderAndPaddingLogicalHeight) * aspectRatio) - borderAndPaddingLogicalWidth;
    return LayoutUnit(logicalHeight * aspectRatio);
}

static inline bool hasIntrinsicSize(RenderBox*contentRenderer, bool hasIntrinsicWidth, bool hasIntrinsicHeight )
{
    if (hasIntrinsicWidth && hasIntrinsicHeight)
        return true;
    if (hasIntrinsicWidth || hasIntrinsicHeight)
        return contentRenderer && contentRenderer->isRenderOrLegacyRenderSVGRoot();
    return false;
}

LayoutUnit RenderReplaced::computeReplacedLogicalWidth(ShouldComputePreferred shouldComputePreferred) const
{
    auto& style = this->style();
    if (style.logicalWidth().isSpecified())
        return computeReplacedLogicalWidthRespectingMinMaxWidth(computeReplacedLogicalWidthUsing(style.logicalWidth()), shouldComputePreferred);
    if (style.logicalWidth().isIntrinsic())
        return computeReplacedLogicalWidthRespectingMinMaxWidth(computeReplacedLogicalWidthUsing(style.logicalWidth()), shouldComputePreferred);

    RenderBox* contentRenderer = embeddedContentBox();

    // 10.3.2 Inline, replaced elements: http://www.w3.org/TR/CSS21/visudet.html#inline-replaced-width
    FloatSize intrinsicRatio;
    FloatSize constrainedSize;
    computeIntrinsicSizesConstrainedByTransferredMinMaxSizes(contentRenderer, constrainedSize, intrinsicRatio);

    if (style.logicalWidth().isAuto()) {
        bool computedHeightIsAuto = style.logicalHeight().isAuto();
        bool hasIntrinsicWidth = constrainedSize.width() > 0 || (!constrainedSize.width() && shouldRespectZeroIntrinsicWidth()) || shouldApplySizeOrInlineSizeContainment();
        bool hasIntrinsicHeight = constrainedSize.height() > 0 || shouldApplySizeContainment();

        // For flex or grid items where the logical height has been overriden then we should use that size to compute the replaced width as long as the flex or
        // grid item has an intrinsic size. It is possible (indeed, common) for an SVG graphic to have an intrinsic aspect ratio but not to have an intrinsic
        // width or height. There are also elements with intrinsic sizes but without intrinsic ratio (like an iframe).
        if (auto overridingLogicalHeight = (!intrinsicRatio.isEmpty() && (isFlexItem() || isGridItem()) && hasIntrinsicSize(contentRenderer, hasIntrinsicWidth, hasIntrinsicHeight) ? this->overridingBorderBoxLogicalHeight() : std::nullopt))
            return computeReplacedLogicalWidthRespectingMinMaxWidth(contentBoxLogicalHeight(*overridingLogicalHeight) * intrinsicRatio.aspectRatioDouble(), shouldComputePreferred);

        // If 'height' and 'width' both have computed values of 'auto' and the element also has an intrinsic width, then that intrinsic width is the used value of 'width'.
        if (computedHeightIsAuto && hasIntrinsicWidth)
            return computeReplacedLogicalWidthRespectingMinMaxWidth(constrainedSize.width(), shouldComputePreferred);

        if (!intrinsicRatio.isEmpty()) {
            // If 'height' and 'width' both have computed values of 'auto' and the element has no intrinsic width, but does have an intrinsic height and intrinsic ratio;
            // or if 'width' has a computed value of 'auto', 'height' has some other computed value, and the element does have an intrinsic ratio; then the used value
            // of 'width' is: (used height) * (intrinsic ratio)
            if (!computedHeightIsAuto || (!hasIntrinsicWidth && hasIntrinsicHeight)) {
                auto estimatedUsedWidth = [&] {
                    if (hasIntrinsicWidth)
                        return LayoutUnit(constrainedSize.width());

                    if (shouldComputePreferred == ShouldComputePreferred::ComputePreferred)
                        return computeReplacedLogicalWidthRespectingMinMaxWidth(0_lu, ShouldComputePreferred::ComputePreferred);

                    auto constrainedLogicalWidth = computeConstrainedLogicalWidth();
                    return computeReplacedLogicalWidthRespectingMinMaxWidth(constrainedLogicalWidth, ShouldComputePreferred::ComputeActual);
                }();

                LayoutUnit logicalHeight = computeReplacedLogicalHeight(std::optional<LayoutUnit>(estimatedUsedWidth));
                auto boxSizing = style.hasAspectRatio() ? style.boxSizingForAspectRatio() : BoxSizing::ContentBox;
                return computeReplacedLogicalWidthRespectingMinMaxWidth(resolveWidthForRatio(borderAndPaddingLogicalHeight(), borderAndPaddingLogicalWidth(), logicalHeight, intrinsicRatio.aspectRatioDouble(), boxSizing), shouldComputePreferred);
            }

            // If 'height' and 'width' both have computed values of 'auto' and the
            // element has an intrinsic ratio but no intrinsic height or width, then
            // the used value of 'width' is undefined in CSS 2.1. However, it is
            // suggested that, if the containing block's width does not itself depend
            // on the replaced element's width, then the used value of 'width' is
            // calculated from the constraint equation used for block-level,
            // non-replaced elements in normal flow.
            if (computedHeightIsAuto && !hasIntrinsicWidth && !hasIntrinsicHeight) {
                bool isFlexItemComputingBaseSize = isFlexItem() && downcast<RenderFlexibleBox>(parent())->isComputingFlexBaseSizes();
                if (shouldComputePreferred == ShouldComputePreferred::ComputePreferred && !isFlexItemComputingBaseSize)
                    return computeReplacedLogicalWidthRespectingMinMaxWidth(0_lu, ShouldComputePreferred::ComputePreferred);

                auto constrainedLogicalWidth = computeConstrainedLogicalWidth();
                auto [transferredMinLogicalWidth, transferredMaxLogicalWidth] = computeMinMaxLogicalWidthFromAspectRatio();
                ASSERT(transferredMinLogicalWidth <= transferredMaxLogicalWidth);
                constrainedLogicalWidth = std::clamp(constrainedLogicalWidth, transferredMinLogicalWidth, transferredMaxLogicalWidth);
                return computeReplacedLogicalWidthRespectingMinMaxWidth(constrainedLogicalWidth, ShouldComputePreferred::ComputeActual);
            }
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

LayoutUnit RenderReplaced::computeReplacedLogicalHeight(std::optional<LayoutUnit> estimatedUsedWidth) const
{
    // 10.5 Content height: the 'height' property: http://www.w3.org/TR/CSS21/visudet.html#propdef-height
    if (hasReplacedLogicalHeight())
        return computeReplacedLogicalHeightRespectingMinMaxHeight(computeReplacedLogicalHeightUsing(style().logicalHeight()));

    RenderBox* contentRenderer = embeddedContentBox();

    // 10.6.2 Inline, replaced elements: http://www.w3.org/TR/CSS21/visudet.html#inline-replaced-height
    FloatSize intrinsicRatio;
    FloatSize constrainedSize;
    computeIntrinsicSizesConstrainedByTransferredMinMaxSizes(contentRenderer, constrainedSize, intrinsicRatio);

    bool widthIsAuto = style().logicalWidth().isAuto();
    bool hasIntrinsicWidth = constrainedSize.width() > 0 || (!constrainedSize.width() && shouldRespectZeroIntrinsicWidth()) || shouldApplySizeOrInlineSizeContainment();
    bool hasIntrinsicHeight = constrainedSize.height() > 0 || shouldApplySizeContainment();

    // See computeReplacedLogicalHeight() for a similar check for heights.
    if (auto overridinglogicalWidth = (!intrinsicRatio.isEmpty() && (isFlexItem() || isGridItem()) && hasIntrinsicSize(contentRenderer, hasIntrinsicWidth, hasIntrinsicHeight) ? overridingBorderBoxLogicalWidth() : std::nullopt))
        return computeReplacedLogicalHeightRespectingMinMaxHeight(contentBoxLogicalWidth(*overridinglogicalWidth) * intrinsicRatio.transposedSize().aspectRatioDouble());

    // If 'height' and 'width' both have computed values of 'auto' and the element also has an intrinsic height, then that intrinsic height is the used value of 'height'.
    if (widthIsAuto && hasIntrinsicHeight)
        return computeReplacedLogicalHeightRespectingMinMaxHeight(constrainedSize.height());

    // Otherwise, if 'height' has a computed value of 'auto', and the element has an intrinsic ratio then the used value of 'height' is:
    // (used width) / (intrinsic ratio)
    if (!intrinsicRatio.isEmpty()) {
        LayoutUnit usedWidth = estimatedUsedWidth ? estimatedUsedWidth.value() : contentBoxLogicalWidth();
        BoxSizing boxSizing = BoxSizing::ContentBox;
        if (style().hasAspectRatio())
            boxSizing = style().boxSizingForAspectRatio();
        return computeReplacedLogicalHeightRespectingMinMaxHeight(resolveHeightForRatio(borderAndPaddingLogicalWidth(), borderAndPaddingLogicalHeight(), usedWidth, intrinsicRatio.transposedSize().aspectRatioDouble(), boxSizing));
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
    ASSERT(needsPreferredLogicalWidthsUpdate());

    // We cannot resolve any percent logical width here as the available logical
    // width may not be set on our containing block.
    if (style().logicalWidth().isPercentOrCalculated())
        computeAspectRatioAdjustedIntrinsicLogicalWidths(m_minPreferredLogicalWidth, m_maxPreferredLogicalWidth);
    else
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = computeReplacedLogicalWidth(ShouldComputePreferred::ComputePreferred);

    bool ignoreMinMaxSizes = shouldIgnoreLogicalMinMaxWidthSizes();
    const RenderStyle& styleToUse = style();
    if (styleToUse.logicalWidth().isPercentOrCalculated() || styleToUse.logicalMaxWidth().isPercentOrCalculated())
        m_minPreferredLogicalWidth = 0;

    if (auto fixedLogicalMinWidth = styleToUse.logicalMinWidth().tryFixed(); !ignoreMinMaxSizes && fixedLogicalMinWidth && fixedLogicalMinWidth->isPositive()) {
        m_maxPreferredLogicalWidth = std::max(m_maxPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(*fixedLogicalMinWidth));
        m_minPreferredLogicalWidth = std::max(m_minPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(*fixedLogicalMinWidth));
    }
    
    if (auto fixedLogicalMaxWidth = styleToUse.logicalMaxWidth().tryFixed(); !ignoreMinMaxSizes && fixedLogicalMaxWidth) {
        m_maxPreferredLogicalWidth = std::min(m_maxPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(*fixedLogicalMaxWidth));
        m_minPreferredLogicalWidth = std::min(m_minPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(*fixedLogicalMaxWidth));
    }

    LayoutUnit borderAndPadding = borderAndPaddingLogicalWidth();
    m_minPreferredLogicalWidth += borderAndPadding;
    m_maxPreferredLogicalWidth += borderAndPadding;

    clearNeedsPreferredWidthsUpdate();
}

PositionWithAffinity RenderReplaced::positionForPoint(const LayoutPoint& point, HitTestSource source, const RenderFragmentContainer* fragment)
{
    auto [top, bottom] = [&]() -> std::pair<float, float> {
        if (auto run = InlineIterator::boxFor(*this)) {
            auto lineBox = run->lineBox();
            auto lineContentTop = LayoutUnit { std::min(previousLineBoxContentBottomOrBorderAndPadding(*lineBox), lineBox->contentLogicalTop()) };
            return std::make_pair(lineContentTop, LineSelection::logicalBottom(*lineBox));
        }
        return std::make_pair(logicalTop(), logicalBottom());
    }();

    LayoutUnit blockDirectionPosition = isHorizontalWritingMode() ? point.y() + y() : point.x() + x();
    LayoutUnit lineDirectionPosition = isHorizontalWritingMode() ? point.x() + x() : point.y() + y();
    
    if (blockDirectionPosition < top)
        return createPositionWithAffinity(caretMinOffset(), Affinity::Downstream); // coordinates are above

    if (blockDirectionPosition >= bottom)
        return createPositionWithAffinity(caretMaxOffset(), Affinity::Downstream); // coordinates are below

    if (element()) {
        if (lineDirectionPosition <= logicalLeft() + (logicalWidth() / 2))
            return createPositionWithAffinity(0, Affinity::Downstream);
        return createPositionWithAffinity(1, Affinity::Downstream);
    }

    return RenderBox::positionForPoint(point, source, fragment);
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

    return LayoutRect(LayoutPoint(), size());
}

bool RenderReplaced::isSelected() const
{
    return isHighlighted(selectionState(), view().selection());
}

bool RenderReplaced::isHighlighted(HighlightState state, const RenderHighlight& rangeData) const
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

auto RenderReplaced::localRectsForRepaint(RepaintOutlineBounds repaintOutlineBounds) const -> RepaintRects
{
    if (isInsideEntirelyHiddenLayer())
        return { };

    // The selectionRect can project outside of the overflowRect, so take their union
    // for repainting to avoid selection painting glitches.
    auto overflowRect = unionRect(localSelectionRect(false), visualOverflowRect());

    // FIXME: layoutDelta needs to be applied in parts before/after transforms and
    // repaint containers. https://bugs.webkit.org/show_bug.cgi?id=23308
    overflowRect.move(view().frameView().layoutContext().layoutDelta());

    auto rects = RepaintRects { overflowRect };
    if (repaintOutlineBounds == RepaintOutlineBounds::Yes)
        rects.outlineBoundsRect = localOutlineBoundsRepaintRect();

    return rects;
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

bool RenderReplaced::shouldInvalidatePreferredWidths() const
{
    // If the height is a percentage and the width is auto, then the containingBlocks's height changing can cause this node to change it's preferred width because it maintains aspect ratio.
    return (hasRelativeLogicalHeight() || (isGridItem() && hasStretchedLogicalHeight())) && style().logicalWidth().isAuto();
}

LayoutSize RenderReplaced::intrinsicSize() const
{
    if (!view().frameView().layoutContext().isInRenderTreeLayout()) {
        // 'contain' removes the natural aspect ratio / width / height only for the purposes of sizing and layout of the box.
        return m_intrinsicSize;
    }

    auto size = m_intrinsicSize;
    auto zoomValue = style().usedZoom();
    if (isHorizontalWritingMode() ? shouldApplySizeOrInlineSizeContainment() : shouldApplySizeContainment())
        size.setWidth(explicitIntrinsicInnerWidth().value_or(0_lu) * zoomValue);
    if (isHorizontalWritingMode() ? shouldApplySizeContainment() : shouldApplySizeOrInlineSizeContainment())
        size.setHeight(explicitIntrinsicInnerHeight().value_or(0_lu) * zoomValue);
    return size;
}

void RenderReplaced::layoutShadowContent(const LayoutSize& oldSize)
{
    for (auto& renderBox : childrenOfType<RenderBox>(*this)) {
        auto newSize = contentBoxRect().size();

        if (is<RenderImage>(this)) {
            bool childNeedsLayout = renderBox.needsLayout();
            // If the region chain has changed we also need to relayout the children to update the region box info.
            // FIXME: We can do better once we compute region box info for RenderReplaced, not only for RenderBlock.
            if (CheckedPtr fragmentedFlow = enclosingFragmentedFlow(); fragmentedFlow && !childNeedsLayout) {
                if (fragmentedFlow->pageLogicalSizeChanged())
                    childNeedsLayout = true;
            }

            if (newSize == oldSize && !childNeedsLayout)
                continue;
        }

        // When calling layout() on a child node, a parent must either push a LayoutStateMaintainer, or
        // instantiate LayoutStateDisabler. Since using a LayoutStateMaintainer is slightly more efficient,
        // and this method might be called many times per second during video playback, use a LayoutStateMaintainer:
        LayoutStateMaintainer statePusher(*this, locationOffset(), isTransformed() || hasReflection() || writingMode().isBlockFlipped());
        renderBox.setLocation(LayoutPoint(borderLeft(), borderTop()) + LayoutSize(paddingLeft(), paddingTop()));
        renderBox.mutableStyle().setHeight(Style::PreferredSize::Fixed { newSize.height() });
        renderBox.mutableStyle().setWidth(Style::PreferredSize::Fixed { newSize.width() });
        renderBox.setNeedsLayout(MarkOnlyThis);
        renderBox.layout();
    }

    clearChildNeedsLayout();
}

FloatSize RenderReplaced::intrinsicRatio() const
{
    FloatSize intrinsicRatio;
    FloatSize constrainedSize;
    computeAspectRatioInformationForRenderBox(embeddedContentBox(), constrainedSize, intrinsicRatio);
    return intrinsicRatio;
}

void RenderReplaced::computeReplacedOutOfFlowPositionedLogicalWidth(LogicalExtentComputedValues& computedValues) const
{
    PositionedLayoutConstraints inlineConstraints(*this, LogicalBoxAxis::Inline);
    inlineConstraints.computeInsets();

    // NOTE: This value of width is final in that the min/max width calculations
    // are dealt with in computeReplacedWidth(). This means that the steps to produce
    // correct max/min in the non-replaced version, are not necessary.
    computedValues.m_extent = computeReplacedLogicalWidth() + borderAndPaddingLogicalWidth();

    inlineConstraints.resolvePosition(computedValues);
    inlineConstraints.fixupLogicalLeftPosition(computedValues);
}

void RenderReplaced::computeReplacedOutOfFlowPositionedLogicalHeight(LogicalExtentComputedValues& computedValues) const
{
    PositionedLayoutConstraints blockConstraints(*this, LogicalBoxAxis::Block);
    blockConstraints.computeInsets();

    // NOTE: This value of height is final in that the min/max height calculations
    // are dealt with in computeReplacedHeight(). This means that the steps to produce
    // correct max/min in the non-replaced version, are not necessary.
    computedValues.m_extent = computeReplacedLogicalHeight() + borderAndPaddingLogicalHeight();

    blockConstraints.resolvePosition(computedValues);
    blockConstraints.adjustLogicalTopWithLogicalHeightIfNeeded(computedValues);
}

LayoutUnit RenderReplaced::computeReplacedLogicalWidthRespectingMinMaxWidth(LayoutUnit logicalWidth, ShouldComputePreferred shouldComputePreferred) const
{
    if (shouldIgnoreLogicalMinMaxWidthSizes())
        return logicalWidth;

    auto& logicalMinWidth = style().logicalMinWidth();
    auto& logicalMaxWidth = style().logicalMaxWidth();
    bool useLogicalWidthForMinWidth = (shouldComputePreferred == ShouldComputePreferred::ComputePreferred && logicalMinWidth.isPercentOrCalculated());
    bool useLogicalWidthForMaxWidth = (shouldComputePreferred == ShouldComputePreferred::ComputePreferred && logicalMaxWidth.isPercentOrCalculated()) || logicalMaxWidth.isNone();
    auto minLogicalWidth =  useLogicalWidthForMinWidth ? logicalWidth : computeReplacedLogicalWidthUsing(logicalMinWidth);
    auto maxLogicalWidth =  useLogicalWidthForMaxWidth ? logicalWidth : computeReplacedLogicalWidthUsing(logicalMaxWidth);
    return std::max(minLogicalWidth, std::min(logicalWidth, maxLogicalWidth));
}

template<typename SizeType>
LayoutUnit RenderReplaced::computeReplacedLogicalWidthUsing(const SizeType& logicalWidth) const
{
    auto calculateContainerWidth = [&] {
        if (isOutOfFlowPositioned()) {
            PositionedLayoutConstraints constraints(*this, LogicalBoxAxis::Inline);
            return constraints.containingSize();
        }
        if (isHorizontalWritingMode() == containingBlock()->isHorizontalWritingMode())
            return containingBlockLogicalWidthForContent();
        return perpendicularContainingBlockLogicalHeight();
    };

    auto percentageOrCalc = [&](Style::IsPercentageOrCalc auto const& logicalWidth) {
        // FIXME: Handle cases when containing block width is calculated or viewport percent.
        // https://bugs.webkit.org/show_bug.cgi?id=91071
        if (auto containerWidth = calculateContainerWidth(); containerWidth > 0 || (!containerWidth && (containingBlock()->style().logicalWidth().isSpecified()))) {
            if constexpr (Style::IsPercentage<std::decay_t<decltype(logicalWidth)>>)
                return adjustContentBoxLogicalWidthForBoxSizing(Style::evaluate<LayoutUnit>(logicalWidth, containerWidth));
            else
                return adjustContentBoxLogicalWidthForBoxSizing(Style::evaluate<LayoutUnit>(logicalWidth, containerWidth, style().usedZoomForLength()));
        }
        return 0_lu;
    };

    auto content = [&](const auto& keyword, const auto& availableLogicalWidth) {
        // FIXME: Handle cases when containing block width is calculated or viewport percent.
        // https://bugs.webkit.org/show_bug.cgi?id=91071
        return computeIntrinsicLogicalWidthUsing(keyword, availableLogicalWidth, borderAndPaddingLogicalWidth()) - borderAndPaddingLogicalWidth();
    };

    return WTF::switchOn(logicalWidth,
        [&](const typename SizeType::Fixed& fixedLogicalWidth) -> LayoutUnit {
            return adjustContentBoxLogicalWidthForBoxSizing(fixedLogicalWidth);
        },
        [&](const typename SizeType::Percentage& percentageLogicalWidth) -> LayoutUnit {
            return percentageOrCalc(percentageLogicalWidth);
        },
        [&](const typename SizeType::Calc& calculatedLogicalWidth) -> LayoutUnit {
            return percentageOrCalc(calculatedLogicalWidth);
        },
        [&](const CSS::Keyword::FitContent& keyword) -> LayoutUnit {
            return content(keyword, calculateContainerWidth());
        },
        [&](const CSS::Keyword::WebkitFillAvailable& keyword) -> LayoutUnit {
            return content(keyword, calculateContainerWidth());
        },
        [&](const CSS::Keyword::MinContent& keyword) -> LayoutUnit {
            // min-content/max-content don't need the availableLogicalWidth argument.
            return content(keyword, 0_lu);
        },
        [&](const CSS::Keyword::MaxContent& keyword) -> LayoutUnit {
            // min-content/max-content don't need the availableLogicalWidth argument.
            return content(keyword, 0_lu);
        },
        [&](const CSS::Keyword::Intrinsic&) -> LayoutUnit {
            return intrinsicLogicalWidth();
        },
        [&](const CSS::Keyword::MinIntrinsic&) -> LayoutUnit {
            return intrinsicLogicalWidth();
        },
        [&](const CSS::Keyword::Auto&) -> LayoutUnit {
            if constexpr (std::same_as<SizeType, Style::MinimumSize>)
                return 0_lu;
            else
                return intrinsicLogicalWidth();
        },
        [&](const CSS::Keyword::None&) -> LayoutUnit {
            return intrinsicLogicalWidth();
        }
    );
}

bool RenderReplaced::replacedMinMaxLogicalHeightComputesAsNone(const auto& logicalHeight, const auto& initialLogicalHeight) const
{
    if (logicalHeight == initialLogicalHeight)
        return true;

    if (isGridItem() && logicalHeight.isPercentOrCalculated()) {
        if (auto gridAreaContentLogicalHeight = this->gridAreaContentLogicalHeight())
            return !*gridAreaContentLogicalHeight;
    }

    // Make sure % min-height and % max-height resolve to none if the containing block has auto height.
    // Note that the "height" case for replaced elements was handled by hasReplacedLogicalHeight, which is why
    // min and max-height are the only ones handled here.
    // FIXME: For now we put in a quirk for Apple Books until we can move them to viewport units.
#if PLATFORM(COCOA)
    // Allow min-max percentages in auto height blocks quirk.
    if (WTF::CocoaApplication::isAppleBooks())
        return false;
#endif
    if (CheckedPtr containingBlock = containingBlockForAutoHeightDetection(logicalHeight))
        return containingBlock->hasAutoHeightOrContainingBlockWithAutoHeight();
    return false;
}

bool RenderReplaced::replacedMinLogicalHeightComputesAsNone() const
{
    return replacedMinMaxLogicalHeightComputesAsNone(style().logicalMinHeight(), RenderStyle::initialMinSize());
}

bool RenderReplaced::replacedMaxLogicalHeightComputesAsNone() const
{
    return replacedMinMaxLogicalHeightComputesAsNone(style().logicalMaxHeight(), RenderStyle::initialMaxSize());
}

LayoutUnit RenderReplaced::computeReplacedLogicalHeightRespectingMinMaxHeight(LayoutUnit logicalHeight) const
{
    if (shouldIgnoreLogicalMinMaxHeightSizes())
        return logicalHeight;

    LayoutUnit minLogicalHeight;
    if (!replacedMinLogicalHeightComputesAsNone())
        minLogicalHeight = computeReplacedLogicalHeightUsing(style().logicalMinHeight());
    LayoutUnit maxLogicalHeight = logicalHeight;
    if (!replacedMaxLogicalHeightComputesAsNone())
        maxLogicalHeight = computeReplacedLogicalHeightUsing(style().logicalMaxHeight());
    return std::max(minLogicalHeight, std::min(logicalHeight, maxLogicalHeight));
}

template<typename SizeType>
LayoutUnit RenderReplaced::computeReplacedLogicalHeightUsingGeneric(const SizeType& logicalHeight) const
{
#if ASSERT_ENABLED
    // This function should get called with Style::MinimumSize/Style::MaximumSize only if replaced[Min|Max]LogicalHeightComputesAsNone
    // returns false, otherwise we should not try to compute those values as they may be incorrect. The caller should make sure this
    // condition holds before calling this function
    if constexpr (std::same_as<SizeType, Style::MinimumSize>)
        ASSERT(!replacedMinLogicalHeightComputesAsNone());
    else if constexpr (std::same_as<SizeType, Style::MaximumSize>)
        ASSERT(!replacedMaxLogicalHeightComputesAsNone());
#endif

    auto percentageOrCalculated = [&](Style::IsPercentageOrCalc auto const& logicalHeight) {
        auto* container = isOutOfFlowPositioned() ? this->container() : containingBlock();
        while (container && container->isAnonymousForPercentageResolution()) {
            // Stop at rendering context root.
            if (is<RenderView>(*container))
                break;
            container = container->containingBlock();
        }
        bool hasPerpendicularContainingBlock = container->isHorizontalWritingMode() != isHorizontalWritingMode();
        std::optional<LayoutUnit> stretchedHeight;
        if (auto* block = dynamicDowncast<RenderBlock>(container)) {
            block->addPercentHeightDescendant(*const_cast<RenderReplaced*>(this));
            if (auto usedFlexItemOverridingLogicalHeightForPercentageResolutionForFlex = (block->isFlexItem() ? downcast<RenderFlexibleBox>(block->parent())->usedFlexItemOverridingLogicalHeightForPercentageResolution(*block) : std::nullopt))
                stretchedHeight = block->contentBoxLogicalHeight(*usedFlexItemOverridingLogicalHeightForPercentageResolutionForFlex);
            else if (auto usedChildOverridingLogicalHeightForGrid = (block->isGridItem() && !hasPerpendicularContainingBlock ? block->overridingBorderBoxLogicalHeight() : std::nullopt))
                stretchedHeight = block->contentBoxLogicalHeight(*usedChildOverridingLogicalHeightForGrid);
        }

        // FIXME: This calculation is not patched for block-flow yet.
        // https://bugs.webkit.org/show_bug.cgi?id=46500
        if (container->isOutOfFlowPositioned()
            && container->style().height().isAuto()
            && !(container->style().top().isAuto() || container->style().bottom().isAuto())) {
            auto& block = downcast<RenderBlock>(*container);
            auto computedValues = block.computeLogicalHeight(block.logicalHeight(), 0);
            LayoutUnit borderPaddingAdjustment = isOutOfFlowPositioned() ? block.borderLogicalHeight() : block.borderAndPaddingLogicalHeight();
            LayoutUnit newContentHeight = computedValues.m_extent - block.scrollbarLogicalHeight() - borderPaddingAdjustment;

            if constexpr (Style::IsPercentage<std::decay_t<decltype(logicalHeight)>>)
                return adjustContentBoxLogicalHeightForBoxSizing(Style::evaluate<LayoutUnit>(logicalHeight, newContentHeight));
            else
                return adjustContentBoxLogicalHeightForBoxSizing(Style::evaluate<LayoutUnit>(logicalHeight, newContentHeight, style().usedZoomForLength()));
        }

        LayoutUnit availableHeight;
        if (isOutOfFlowPositioned()) {
            PositionedLayoutConstraints constraints(*this, LogicalBoxAxis::Block);
            availableHeight = constraints.containingSize();
        } else if (stretchedHeight)
            availableHeight = stretchedHeight.value();
        else if (auto gridAreaLogicalHeight = isGridItem() ? this->gridAreaContentLogicalHeight() : std::nullopt; gridAreaLogicalHeight && *gridAreaLogicalHeight)
            availableHeight = gridAreaLogicalHeight->value();
        else {
            availableHeight = hasPerpendicularContainingBlock ? containingBlockLogicalWidthForContent() : containingBlockLogicalHeightForContent(AvailableLogicalHeightType::IncludeMarginBorderPadding);
            // It is necessary to use the border-box to match WinIE's broken
            // box model. This is essential for sizing inside
            // table cells using percentage heights.
            // FIXME: This needs to be made block-flow-aware. If the cell and image are perpendicular block-flows, this isn't right.
            // https://bugs.webkit.org/show_bug.cgi?id=46997
            while (container && !is<RenderView>(*container)
                && (container->style().logicalHeight().isAuto() || container->style().logicalHeight().isPercentOrCalculated())) {
                if (container->isRenderTableCell()) {
                    // Don't let table cells squeeze percent-height replaced elements
                    // <http://bugs.webkit.org/show_bug.cgi?id=15359>
                    availableHeight = std::max(availableHeight, intrinsicLogicalHeight());
                    if constexpr (Style::IsPercentage<std::decay_t<decltype(logicalHeight)>>)
                        return Style::evaluate<LayoutUnit>(logicalHeight, availableHeight - borderAndPaddingLogicalHeight());
                    else
                        return Style::evaluate<LayoutUnit>(logicalHeight, availableHeight - borderAndPaddingLogicalHeight(), style().usedZoomForLength());
                }
                downcast<RenderBlock>(*container).addPercentHeightDescendant(const_cast<RenderReplaced&>(*this));
                container = container->containingBlock();
            }
        }

        if constexpr (Style::IsPercentage<std::decay_t<decltype(logicalHeight)>>)
            return adjustContentBoxLogicalHeightForBoxSizing(Style::evaluate<LayoutUnit>(logicalHeight, availableHeight));
        else
            return adjustContentBoxLogicalHeightForBoxSizing(Style::evaluate<LayoutUnit>(logicalHeight, availableHeight, style().usedZoomForLength()));
    };

    auto content = [&] {
        return adjustContentBoxLogicalHeightForBoxSizing(computeIntrinsicLogicalContentHeightUsing(logicalHeight, intrinsicLogicalHeight(), borderAndPaddingLogicalHeight()));
    };

    return WTF::switchOn(logicalHeight,
        [&](const typename SizeType::Fixed& fixedLogicalHeight) -> LayoutUnit {
            return adjustContentBoxLogicalHeightForBoxSizing(LayoutUnit { fixedLogicalHeight.resolveZoom(style().usedZoomForLength()) });
        },
        [&](const typename SizeType::Percentage& percentageLogicalHeight) -> LayoutUnit {
            return percentageOrCalculated(percentageLogicalHeight);
        },
        [&](const typename SizeType::Calc& calculatedLogicalHeight) -> LayoutUnit {
            return percentageOrCalculated(calculatedLogicalHeight);
        },
        [&](const CSS::Keyword::FitContent&) -> LayoutUnit {
            return content();
        },
        [&](const CSS::Keyword::WebkitFillAvailable&) -> LayoutUnit {
            return content();
        },
        [&](const CSS::Keyword::MinContent&) -> LayoutUnit {
            return content();
        },
        [&](const CSS::Keyword::MaxContent&) -> LayoutUnit {
            return content();
        },
        [&](const CSS::Keyword::Intrinsic&) -> LayoutUnit {
            return intrinsicLogicalHeight();
        },
        [&](const CSS::Keyword::MinIntrinsic&) -> LayoutUnit {
            return intrinsicLogicalHeight();
        },
        [&](const CSS::Keyword::Auto&) -> LayoutUnit  {
            if constexpr (std::same_as<SizeType, Style::MinimumSize>)
                return adjustContentBoxLogicalHeightForBoxSizing(LayoutUnit { 0 });
            else
                return intrinsicLogicalHeight();
        },
        [&](const CSS::Keyword::None&) -> LayoutUnit  {
            return intrinsicLogicalHeight();
        }
    );
}

LayoutUnit RenderReplaced::computeReplacedLogicalHeightUsing(const Style::PreferredSize& logicalHeight) const
{
    return computeReplacedLogicalHeightUsingGeneric(logicalHeight);
}

LayoutUnit RenderReplaced::computeReplacedLogicalHeightUsing(const Style::MinimumSize& logicalHeight) const
{
    return computeReplacedLogicalHeightUsingGeneric(logicalHeight);
}

LayoutUnit RenderReplaced::computeReplacedLogicalHeightUsing(const Style::MaximumSize& logicalHeight) const
{
    return computeReplacedLogicalHeightUsingGeneric(logicalHeight);
}

}
