/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007, 2008, 2009 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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

#include "config.h"

#if ENABLE(SVG)
#include "RenderSVGRoot.h"

#include "Chrome.h"
#include "ChromeClient.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "LayoutRepainter.h"
#include "Page.h"
#include "RenderPart.h"
#include "RenderSVGContainer.h"
#include "RenderSVGResource.h"
#include "RenderView.h"
#include "SVGLength.h"
#include "SVGRenderSupport.h"
#include "SVGResources.h"
#include "SVGResourcesCache.h"
#include "SVGSVGElement.h"
#include "SVGStyledElement.h"
#include "SVGViewSpec.h"
#include "TransformState.h"

#if ENABLE(FILTERS)
#include "RenderSVGResourceFilter.h"
#endif

using namespace std;

namespace WebCore {

RenderSVGRoot::RenderSVGRoot(SVGStyledElement* node)
    : RenderReplaced(node)
    , m_isLayoutSizeChanged(false)
    , m_needsBoundariesOrTransformUpdate(true)
{
}

RenderSVGRoot::~RenderSVGRoot()
{
}

void RenderSVGRoot::computeIntrinsicRatioInformation(FloatSize& intrinsicSize, double& intrinsicRatio, bool& isPercentageIntrinsicSize) const
{
    // Spec: http://www.w3.org/TR/SVG/coords.html#IntrinsicSizing
    // SVG needs to specify how to calculate some intrinsic sizing properties to enable inclusion within other languages.
    // The intrinsic width and height of the viewport of SVG content must be determined from the ‘width’ and ‘height’ attributes.
    // If either of these are not specified, a value of '100%' must be assumed. Note: the ‘width’ and ‘height’ attributes are not
    // the same as the CSS width and height properties. Specifically, percentage values do not provide an intrinsic width or height,
    // and do not indicate a percentage of the containing block. Rather, once the viewport is established, they indicate the portion
    // of the viewport that is actually covered by image data.
    SVGSVGElement* svg = static_cast<SVGSVGElement*>(node());
    Length intrinsicWidthAttribute = svg->intrinsicWidth(SVGSVGElement::IgnoreCSSProperties);
    Length intrinsicHeightAttribute = svg->intrinsicHeight(SVGSVGElement::IgnoreCSSProperties);

    // The intrinsic aspect ratio of the viewport of SVG content is necessary for example, when including SVG from an ‘object’
    // element in HTML styled with CSS. It is possible (indeed, common) for an SVG graphic to have an intrinsic aspect ratio but
    // not to have an intrinsic width or height. The intrinsic aspect ratio must be calculated based upon the following rules:
    // - The aspect ratio is calculated by dividing a width by a height.
    // - If the ‘width’ and ‘height’ of the rootmost ‘svg’ element are both specified with unit identifiers (in, mm, cm, pt, pc,
    //   px, em, ex) or in user units, then the aspect ratio is calculated from the ‘width’ and ‘height’ attributes after
    //   resolving both values to user units.
    if (intrinsicWidthAttribute.isFixed() || intrinsicHeightAttribute.isFixed()) {
        if (intrinsicWidthAttribute.isFixed())
            intrinsicSize.setWidth(intrinsicWidthAttribute.calcFloatValue(0));
        if (intrinsicHeightAttribute.isFixed())
            intrinsicSize.setHeight(intrinsicHeightAttribute.calcFloatValue(0));
        if (!intrinsicSize.isEmpty())
            intrinsicRatio = intrinsicSize.width() / static_cast<double>(intrinsicSize.height());
        return;
    }

    // - If either/both of the ‘width’ and ‘height’ of the rootmost ‘svg’ element are in percentage units (or omitted), the
    //   aspect ratio is calculated from the width and height values of the ‘viewBox’ specified for the current SVG document
    //   fragment. If the ‘viewBox’ is not correctly specified, or set to 'none', the intrinsic aspect ratio cannot be
    //   calculated and is considered unspecified.
    intrinsicSize = svg->viewBox().size();
    if (!intrinsicSize.isEmpty()) {
        // The viewBox can only yield an intrinsic ratio, not an intrinsic size.
        intrinsicRatio = intrinsicSize.width() / static_cast<double>(intrinsicSize.height());
        intrinsicSize = FloatSize();
        return;
    }

    // If our intrinsic size is in percentage units, return those to the caller through the intrinsicSize. Notify the caller
    // about the special situation, by setting isPercentageIntrinsicSize=true, so it knows how to interpret the return values.
    if (intrinsicWidthAttribute.isPercent() && intrinsicHeightAttribute.isPercent()) {
        isPercentageIntrinsicSize = true;
        intrinsicSize = FloatSize(intrinsicWidthAttribute.percent(), intrinsicHeightAttribute.percent());
    }
}

bool RenderSVGRoot::isEmbeddedThroughSVGImage() const
{
    if (!node())
        return false;

    Frame* frame = node()->document()->frame();
    if (!frame)
        return false;

    // Test whether we're embedded through an img.
    if (!frame->page() || !frame->page()->chrome())
        return false;

    ChromeClient* chromeClient = frame->page()->chrome()->client();
    if (!chromeClient || !chromeClient->isSVGImageChromeClient())
        return false;

    return true;
}

bool RenderSVGRoot::isEmbeddedThroughFrameContainingSVGDocument() const
{
    if (!node())
        return false;

    Frame* frame = node()->document()->frame();
    if (!frame)
        return false;

    // If our frame has an owner renderer, we're embedded through eg. object/embed/iframe,
    // but we only negotiate if we're in an SVG document.
    if (!frame->ownerRenderer())
        return false;
    return frame->document()->isSVGDocument();
}

static inline LayoutUnit resolveLengthAttributeForSVG(const Length& length, float scale, float maxSize)
{
    return static_cast<LayoutUnit>(length.calcValue(maxSize) * (length.isFixed() ? scale : 1));
}

LayoutUnit RenderSVGRoot::computeReplacedLogicalWidth(bool includeMaxWidth) const
{
    SVGSVGElement* svg = static_cast<SVGSVGElement*>(node());
    ASSERT(svg);

    // When we're embedded through SVGImage (border-image/background-image/<html:img>/...) we're forced to resize to a specific size.
    if (!m_containerSize.isEmpty())
        return m_containerSize.width();

    if (style()->logicalWidth().isSpecified())
        return RenderReplaced::computeReplacedLogicalWidth(includeMaxWidth);

    if (svg->widthAttributeEstablishesViewport())
        return resolveLengthAttributeForSVG(svg->intrinsicWidth(SVGSVGElement::IgnoreCSSProperties), style()->effectiveZoom(), containingBlock()->availableLogicalWidth());

    // Only SVGs embedded in <object> reach this point.
    ASSERT(isEmbeddedThroughFrameContainingSVGDocument());
    return document()->frame()->ownerRenderer()->availableLogicalWidth();
}

LayoutUnit RenderSVGRoot::computeReplacedLogicalHeight() const
{
    SVGSVGElement* svg = static_cast<SVGSVGElement*>(node());
    ASSERT(svg);

    // When we're embedded through SVGImage (border-image/background-image/<html:img>/...) we're forced to resize to a specific size.
    if (!m_containerSize.isEmpty())
        return m_containerSize.height();

    if (hasReplacedLogicalHeight())
        return RenderReplaced::computeReplacedLogicalHeight();

    if (svg->heightAttributeEstablishesViewport())
        return resolveLengthAttributeForSVG(svg->intrinsicHeight(SVGSVGElement::IgnoreCSSProperties), style()->effectiveZoom(), containingBlock()->availableLogicalHeight());

    // Only SVGs embedded in <object> reach this point.
    ASSERT(isEmbeddedThroughFrameContainingSVGDocument());
    return document()->frame()->ownerRenderer()->availableLogicalHeight();
}

void RenderSVGRoot::layout()
{
    ASSERT(needsLayout());

    // Arbitrary affine transforms are incompatible with LayoutState.
    LayoutStateDisabler layoutStateDisabler(view());

    bool needsLayout = selfNeedsLayout();
    LayoutRepainter repainter(*this, checkForRepaintDuringLayout() && needsLayout);

    LayoutSize oldSize(width(), height());
    computeLogicalWidth();
    computeLogicalHeight();
    buildLocalToBorderBoxTransform();

    SVGSVGElement* svg = static_cast<SVGSVGElement*>(node());
    m_isLayoutSizeChanged = needsLayout || (svg->hasRelativeLengths() && oldSize != size());
    SVGRenderSupport::layoutChildren(this, needsLayout || SVGRenderSupport::filtersForceContainerLayout(this));
    m_isLayoutSizeChanged = false;

    // At this point LayoutRepainter already grabbed the old bounds,
    // recalculate them now so repaintAfterLayout() uses the new bounds.
    if (m_needsBoundariesOrTransformUpdate) {
        updateCachedBoundaries();
        m_needsBoundariesOrTransformUpdate = false;
    }

    repainter.repaintAfterLayout();

    setNeedsLayout(false);
}

void RenderSVGRoot::paintReplaced(PaintInfo& paintInfo, const LayoutPoint& adjustedPaintOffset)
{
    // An empty viewport disables rendering.
    if (borderBoxRect().isEmpty())
        return;

    // Don't paint, if the context explicitely disabled it.
    if (paintInfo.context->paintingDisabled())
        return;

    // Don't paint if we don't have kids, except if we have filters we should paint those.
    if (!firstChild()) {
        SVGResources* resources = SVGResourcesCache::cachedResourcesForRenderObject(this);
        if (!resources || !resources->filter())
            return;
    }

    // Make a copy of the PaintInfo because applyTransform will modify the damage rect.
    PaintInfo childPaintInfo(paintInfo);
    childPaintInfo.context->save();

    // Apply initial viewport clip - not affected by overflow handling
    childPaintInfo.context->clip(overflowClipRect(adjustedPaintOffset, paintInfo.renderRegion));

    // Convert from container offsets (html renderers) to a relative transform (svg renderers).
    // Transform from our paint container's coordinate system to our local coords.
    childPaintInfo.applyTransform(AffineTransform::translation(adjustedPaintOffset.x() - x(), adjustedPaintOffset.y() - y()) * localToParentTransform());

    bool continueRendering = true;
    if (childPaintInfo.phase == PaintPhaseForeground)
        continueRendering = SVGRenderSupport::prepareToRenderSVGContent(this, childPaintInfo);

    if (continueRendering)
        RenderBox::paint(childPaintInfo, LayoutPoint());

    if (childPaintInfo.phase == PaintPhaseForeground)
        SVGRenderSupport::finishRenderSVGContent(this, childPaintInfo, paintInfo.context);

    childPaintInfo.context->restore();
}

void RenderSVGRoot::willBeDestroyed()
{
    SVGResourcesCache::clientDestroyed(this);
    RenderReplaced::willBeDestroyed();
}

void RenderSVGRoot::styleWillChange(StyleDifference diff, const RenderStyle* newStyle)
{
    if (diff == StyleDifferenceLayout)
        setNeedsBoundariesUpdate();
    RenderReplaced::styleWillChange(diff, newStyle);
}

void RenderSVGRoot::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderReplaced::styleDidChange(diff, oldStyle);
    SVGResourcesCache::clientStyleChanged(this, diff, style());
}

void RenderSVGRoot::updateFromElement()
{
    RenderReplaced::updateFromElement();
    SVGResourcesCache::clientUpdatedFromElement(this, style());
}

// RenderBox methods will expect coordinates w/o any transforms in coordinates
// relative to our borderBox origin.  This method gives us exactly that.
void RenderSVGRoot::buildLocalToBorderBoxTransform()
{
    SVGSVGElement* svg = static_cast<SVGSVGElement*>(node());
    float scale = style()->effectiveZoom();
    FloatPoint translate = svg->currentTranslate();
    LayoutSize borderAndPadding(borderLeft() + paddingLeft(), borderTop() + paddingTop());
    m_localToBorderBoxTransform = svg->viewBoxToViewTransform(width() / scale, height() / scale);
    if (borderAndPadding.isEmpty() && scale == 1 && translate == FloatPoint::zero())
        return;
    m_localToBorderBoxTransform = AffineTransform(scale, 0, 0, scale, borderAndPadding.width() + translate.x(), borderAndPadding.height() + translate.y()) * m_localToBorderBoxTransform;
}

const AffineTransform& RenderSVGRoot::localToParentTransform() const
{
    // Slightly optimized version of m_localToParentTransform = AffineTransform::translation(x(), y()) * m_localToBorderBoxTransform;
    m_localToParentTransform = m_localToBorderBoxTransform;
    if (x())
        m_localToParentTransform.setE(m_localToParentTransform.e() + x());
    if (y())
        m_localToParentTransform.setF(m_localToParentTransform.f() + y());
    return m_localToParentTransform;
}

LayoutRect RenderSVGRoot::clippedOverflowRectForRepaint(RenderBoxModelObject* repaintContainer) const
{
    return SVGRenderSupport::clippedOverflowRectForRepaint(this, repaintContainer);
}

void RenderSVGRoot::computeFloatRectForRepaint(RenderBoxModelObject* repaintContainer, FloatRect& repaintRect, bool fixed) const
{
    // Apply our local transforms (except for x/y translation), then our shadow, 
    // and then call RenderBox's method to handle all the normal CSS Box model bits
    repaintRect = m_localToBorderBoxTransform.mapRect(repaintRect);

    // Apply initial viewport clip - not affected by overflow settings    
    repaintRect.intersect(borderBoxRect());

    const SVGRenderStyle* svgStyle = style()->svgStyle();
    if (const ShadowData* shadow = svgStyle->shadow())
        shadow->adjustRectForShadow(repaintRect);

    LayoutRect rect = enclosingIntRect(repaintRect);
    RenderReplaced::computeRectForRepaint(repaintContainer, rect, fixed);
    repaintRect = rect;
}

void RenderSVGRoot::mapLocalToContainer(RenderBoxModelObject* repaintContainer, bool fixed, bool useTransforms, TransformState& transformState, bool* wasFixed) const
{
    ASSERT(!fixed); // We should have no fixed content in the SVG rendering tree.
    ASSERT(useTransforms); // mapping a point through SVG w/o respecting trasnforms is useless.

    // Transform to our border box and let RenderBox transform the rest of the way.
    transformState.applyTransform(m_localToBorderBoxTransform);
    RenderReplaced::mapLocalToContainer(repaintContainer, fixed, useTransforms, transformState, wasFixed);
}

void RenderSVGRoot::updateCachedBoundaries()
{
    m_objectBoundingBox = FloatRect();
    m_strokeBoundingBox = FloatRect();
    m_repaintBoundingBox = FloatRect();

    SVGRenderSupport::computeContainerBoundingBoxes(this, m_objectBoundingBox, m_strokeBoundingBox, m_repaintBoundingBox);
    SVGRenderSupport::intersectRepaintRectWithResources(this, m_repaintBoundingBox);
    m_repaintBoundingBox.inflate(borderAndPaddingWidth());
}

bool RenderSVGRoot::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const LayoutPoint& pointInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    LayoutPoint pointInParent = pointInContainer - toLayoutSize(accumulatedOffset);
    LayoutPoint pointInBorderBox(pointInParent.x() - x(), pointInParent.y() - y());

    // Note: For now, we're ignoring hits to border and padding for <svg>
    if (!contentBoxRect().contains(pointInBorderBox))
        return false;

    LayoutPoint localPoint = localToParentTransform().inverse().mapPoint(pointInParent);

    for (RenderObject* child = lastChild(); child; child = child->previousSibling()) {
        if (child->nodeAtFloatPoint(request, result, localPoint, hitTestAction)) {
            // FIXME: CSS/HTML assumes the local point is relative to the border box, right?
            updateHitTestResult(result, pointInBorderBox);
            // FIXME: nodeAtFloatPoint() doesn't handle rect-based hit tests yet.
            result.addNodeToRectBasedTestResult(child->node(), pointInContainer);
            return true;
        }
    }

    // If we didn't early exit above, we've just hit the container <svg> element. Unlike SVG 1.1, 2nd Edition allows container elements to be hit.
    if (hitTestAction == HitTestBlockBackground && style()->pointerEvents() != PE_NONE) {
        // Only return true here, if the last hit testing phase 'BlockBackground' is executed. If we'd return true in the 'Foreground' phase,
        // hit testing would stop immediately. For SVG only trees this doesn't matter. Though when we have a <foreignObject> subtree we need
        // to be able to detect hits on the background of a <div> element. If we'd return true here in the 'Foreground' phase, we are not able 
        // to detect these hits anymore.
        updateHitTestResult(result, roundedLayoutPoint(localPoint));
        return true;
    }

    return false;
}

}

#endif // ENABLE(SVG)
