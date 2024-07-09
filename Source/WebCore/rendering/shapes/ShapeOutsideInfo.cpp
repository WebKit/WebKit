/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "ShapeOutsideInfo.h"

#include "BoxShape.h"
#include "FloatingObjects.h"
#include "LengthFunctions.h"
#include "RenderBlockFlow.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderFragmentContainer.h"
#include "RenderImage.h"
#include "RenderView.h"
#include <wtf/text/MakeString.h>

namespace WebCore {

static LayoutUnit logicalLeftOffset(const RenderBox&);
static LayoutUnit logicalTopOffset(const RenderBox&);

LayoutRect ShapeOutsideInfo::computedShapePhysicalBoundingBox() const
{
    LayoutRect physicalBoundingBox = computedShape().shapeMarginLogicalBoundingBox();
    if (m_renderer.style().isFlippedBlocksWritingMode())
        physicalBoundingBox.setY(m_renderer.logicalHeight() - physicalBoundingBox.maxY());
    if (!m_renderer.style().isHorizontalWritingMode())
        physicalBoundingBox = physicalBoundingBox.transposedRect();
    return physicalBoundingBox;
}

FloatPoint ShapeOutsideInfo::shapeToRendererPoint(const FloatPoint& point) const
{
    FloatPoint result = point;
    if (m_renderer.style().isFlippedBlocksWritingMode())
        result.setY(m_renderer.logicalHeight() - result.y());
    if (!m_renderer.style().isHorizontalWritingMode())
        result = result.transposedPoint();
    return result;
}

static LayoutSize computeLogicalBoxSize(const RenderBox& renderer, bool isHorizontalWritingMode)
{
    auto& shapeValue = *renderer.style().shapeOutside();
    auto size = isHorizontalWritingMode ? renderer.size() : renderer.size().transposedSize();
    switch (shapeValue.effectiveCSSBox()) {
    case CSSBoxType::MarginBox:
        if (isHorizontalWritingMode)
            size.expand(renderer.horizontalMarginExtent(), renderer.verticalMarginExtent());
        else
            size.expand(renderer.verticalMarginExtent(), renderer.horizontalMarginExtent());
        break;
    case CSSBoxType::BorderBox:
        break;
    case CSSBoxType::PaddingBox:
        if (isHorizontalWritingMode)
            size.shrink(renderer.horizontalBorderExtent(), renderer.verticalBorderExtent());
        else
            size.shrink(renderer.verticalBorderExtent(), renderer.horizontalBorderExtent());
        break;
    case CSSBoxType::ContentBox:
        if (isHorizontalWritingMode)
            size.shrink(renderer.horizontalBorderAndPaddingExtent(), renderer.verticalBorderAndPaddingExtent());
        else
            size.shrink(renderer.verticalBorderAndPaddingExtent(), renderer.horizontalBorderAndPaddingExtent());
        break;
    case CSSBoxType::FillBox:
    case CSSBoxType::StrokeBox:
    case CSSBoxType::ViewBox:
    case CSSBoxType::BoxMissing:
        ASSERT_NOT_REACHED();
        break;
    }
    return size;
}

void ShapeOutsideInfo::invalidateForSizeChangeIfNeeded()
{
    auto newSize = computeLogicalBoxSize(m_renderer, m_renderer.containingBlock()->isHorizontalWritingMode());
    if (m_cachedShapeLogicalSize == newSize)
        return;

    markShapeAsDirty();
    m_cachedShapeLogicalSize = newSize;
}

static LayoutRect getShapeImageMarginRect(const RenderBox& renderBox, const LayoutSize& referenceBoxLogicalSize)
{
    LayoutPoint marginBoxOrigin(-renderBox.marginLogicalLeft() - renderBox.borderAndPaddingLogicalLeft(), -renderBox.marginBefore() - renderBox.borderBefore() - renderBox.paddingBefore());
    LayoutSize marginBoxSizeDelta(renderBox.marginLogicalWidth() + renderBox.borderAndPaddingLogicalWidth(), renderBox.marginLogicalHeight() + renderBox.borderAndPaddingLogicalHeight());
    LayoutSize marginRectSize(referenceBoxLogicalSize + marginBoxSizeDelta);
    marginRectSize.clampNegativeToZero();
    return LayoutRect(marginBoxOrigin, marginRectSize);
}

Ref<const Shape> makeShapeForShapeOutside(const RenderBox& renderer)
{
    auto& style = renderer.style();
    auto& containingBlock = *renderer.containingBlock();
    auto writingMode = containingBlock.style().writingMode();
    bool isHorizontalWritingMode = containingBlock.isHorizontalWritingMode();
    float shapeImageThreshold = style.shapeImageThreshold();
    auto& shapeValue = *style.shapeOutside();

    auto boxSize = computeLogicalBoxSize(renderer, isHorizontalWritingMode);

    auto margin = [&] {
        auto shapeMargin = floatValueForLength(style.shapeMargin(), containingBlock.contentWidth());
        return isnan(shapeMargin) ? 0.0f : shapeMargin;
    }();


    switch (shapeValue.type()) {
    case ShapeValue::Type::Shape: {
        ASSERT(shapeValue.shape());
        auto offset = LayoutPoint { logicalLeftOffset(renderer), logicalTopOffset(renderer) };
        return Shape::createShape(*shapeValue.shape(), offset, boxSize, writingMode, margin);
    }
    case ShapeValue::Type::Image: {
        ASSERT(shapeValue.isImageValid());
        auto* styleImage = shapeValue.image();
        auto imageSize = renderer.calculateImageIntrinsicDimensions(styleImage, boxSize, RenderImage::ScaleByUsedZoom::Yes);
        styleImage->setContainerContextForRenderer(renderer, imageSize, style.usedZoom());

        auto marginRect = getShapeImageMarginRect(renderer, boxSize);
        auto* renderImage = dynamicDowncast<RenderImage>(renderer);
        auto imageRect = renderImage ? renderImage->replacedContentRect() : LayoutRect { { }, imageSize };

        ASSERT(!styleImage->isPending());
        RefPtr<Image> image = styleImage->image(const_cast<RenderBox*>(&renderer), imageSize);
        return Shape::createRasterShape(image.get(), shapeImageThreshold, imageRect, marginRect, writingMode, margin);
    }
    case ShapeValue::Type::Box: {
        auto shapeRect = computeRoundedRectForBoxShape(shapeValue.effectiveCSSBox(), renderer);
        if (!isHorizontalWritingMode)
            shapeRect = shapeRect.transposedRect();
        return Shape::createBoxShape(shapeRect, writingMode, margin);
    }
    }
    ASSERT_NOT_REACHED();
    return Shape::createBoxShape(RoundedRect { { } }, writingMode, 0);
}

static inline bool checkShapeImageOrigin(Document& document, const StyleImage& styleImage)
{
    if (styleImage.isGeneratedImage())
        return true;

    ASSERT(styleImage.cachedImage());
    CachedImage& cachedImage = *(styleImage.cachedImage());
    if (cachedImage.isOriginClean(&document.securityOrigin()))
        return true;

    const URL& url = cachedImage.url();
    String urlString = url.isNull() ? "''"_s : url.stringCenterEllipsizedToLength();
    document.addConsoleMessage(MessageSource::Security, MessageLevel::Error, makeString("Unsafe attempt to load URL "_s, urlString, '.'));

    return false;
}

const Shape& ShapeOutsideInfo::computedShape() const
{
    if (!m_shape)
        m_shape = makeShapeForShapeOutside(m_renderer);

    return *m_shape;
}

static inline LayoutUnit borderBeforeInWritingMode(const RenderBox& renderer, WritingMode writingMode)
{
    auto blockFlowDirection = writingModeToBlockFlowDirection(writingMode);
    switch (blockFlowDirection) {
    case BlockFlowDirection::TopToBottom: return renderer.borderTop();
    case BlockFlowDirection::BottomToTop: return renderer.borderBottom();
    case BlockFlowDirection::LeftToRight: return renderer.borderLeft();
    case BlockFlowDirection::RightToLeft: return renderer.borderRight();
    }

    ASSERT_NOT_REACHED();
    return renderer.borderBefore();
}

static inline LayoutUnit borderAndPaddingBeforeInWritingMode(const RenderBox& renderer, WritingMode writingMode)
{
    auto blockFlowDirection = writingModeToBlockFlowDirection(writingMode);
    switch (blockFlowDirection) {
    case BlockFlowDirection::TopToBottom: return renderer.borderTop() + renderer.paddingTop();
    case BlockFlowDirection::BottomToTop: return renderer.borderBottom() + renderer.paddingBottom();
    case BlockFlowDirection::LeftToRight: return renderer.borderLeft() + renderer.paddingLeft();
    case BlockFlowDirection::RightToLeft: return renderer.borderRight() + renderer.paddingRight();
    }

    ASSERT_NOT_REACHED();
    return renderer.borderAndPaddingBefore();
}

static LayoutUnit logicalTopOffset(const RenderBox& renderer)
{
    switch (renderer.style().shapeOutside()->effectiveCSSBox()) {
    case CSSBoxType::MarginBox:
        return -renderer.marginBefore(&renderer.containingBlock()->style());
    case CSSBoxType::BorderBox:
        return 0_lu;
    case CSSBoxType::PaddingBox:
        return borderBeforeInWritingMode(renderer, renderer.containingBlock()->style().writingMode());
    case CSSBoxType::ContentBox:
        return borderAndPaddingBeforeInWritingMode(renderer, renderer.containingBlock()->style().writingMode());
    case CSSBoxType::FillBox:
        break;
    case CSSBoxType::StrokeBox:
        break;
    case CSSBoxType::ViewBox:
        break;
    case CSSBoxType::BoxMissing:
        break;
    }
    
    ASSERT_NOT_REACHED();
    return 0_lu;
}

static inline LayoutUnit borderStartWithStyleForWritingMode(const RenderBox& renderer, const RenderStyle& style)
{
    if (style.isHorizontalWritingMode()) {
        if (style.isLeftToRightDirection())
            return renderer.borderLeft();
        
        return renderer.borderRight();
    }
    if (style.isLeftToRightDirection())
        return renderer.borderTop();
    
    return renderer.borderBottom();
}

static inline LayoutUnit borderAndPaddingStartWithStyleForWritingMode(const RenderBox& renderer, const RenderStyle& style)
{
    if (style.isHorizontalWritingMode()) {
        if (style.isLeftToRightDirection())
            return renderer.borderLeft() + renderer.paddingLeft();
        
        return renderer.borderRight() + renderer.paddingRight();
    }
    if (style.isLeftToRightDirection())
        return renderer.borderTop() + renderer.paddingTop();
    
    return renderer.borderBottom() + renderer.paddingBottom();
}

static LayoutUnit logicalLeftOffset(const RenderBox& renderer)
{
    if (renderer.isRenderFragmentContainer())
        return 0_lu;
    
    switch (renderer.style().shapeOutside()->effectiveCSSBox()) {
    case CSSBoxType::MarginBox:
        return -renderer.marginStart(&renderer.containingBlock()->style());
    case CSSBoxType::BorderBox:
        return 0_lu;
    case CSSBoxType::PaddingBox:
        return borderStartWithStyleForWritingMode(renderer, renderer.containingBlock()->style());
    case CSSBoxType::ContentBox:
        return borderAndPaddingStartWithStyleForWritingMode(renderer, renderer.containingBlock()->style());
    case CSSBoxType::FillBox:
        break;
    case CSSBoxType::StrokeBox:
        break;
    case CSSBoxType::ViewBox:
        break;
    case CSSBoxType::BoxMissing:
        break;
    }

    ASSERT_NOT_REACHED();
    return 0_lu;
}

bool ShapeOutsideInfo::isEnabledFor(const RenderBox& box)
{
    ShapeValue* shapeValue = box.style().shapeOutside();
    if (!box.isFloating() || !shapeValue)
        return false;

    switch (shapeValue->type()) {
    case ShapeValue::Type::Shape: return shapeValue->shape();
    case ShapeValue::Type::Image: return shapeValue->isImageValid() && checkShapeImageOrigin(box.document(), *(shapeValue->image()));
    case ShapeValue::Type::Box: return true;
    }

    ASSERT_NOT_REACHED();
    return false;
}

ShapeOutsideDeltas ShapeOutsideInfo::computeDeltasForContainingBlockLine(const RenderBlockFlow& containingBlock, const FloatingObject& floatingObject, LayoutUnit lineTop, LayoutUnit lineHeight)
{
    // If we never constructed this shape during layout, we propably don't need to know about it outside of layout in the context of "containing block line".
    if (!m_shape && !containingBlock.view().frameView().layoutContext().isInLayout())
        return { };

    ASSERT(lineHeight >= 0);
    LayoutUnit borderBoxTop = containingBlock.logicalTopForFloat(floatingObject) + containingBlock.marginBeforeForChild(m_renderer);
    LayoutUnit borderBoxLineTop = lineTop - borderBoxTop;

    if (isShapeDirty() || !m_shapeOutsideDeltas.isForLine(borderBoxLineTop, lineHeight)) {
        LayoutUnit floatMarginBoxWidth = std::max<LayoutUnit>(0_lu, containingBlock.logicalWidthForFloat(floatingObject));

        if (computedShape().lineOverlapsShapeMarginBounds(borderBoxLineTop, lineHeight)) {
            LineSegment segment = computedShape().getExcludedInterval(borderBoxLineTop, std::min(lineHeight, shapeLogicalBottom() - borderBoxLineTop));
            if (segment.isValid) {
                LayoutUnit logicalLeftMargin = containingBlock.style().isLeftToRightDirection() ? containingBlock.marginStartForChild(m_renderer) : containingBlock.marginEndForChild(m_renderer);
                LayoutUnit rawLeftMarginBoxDelta { segment.logicalLeft + logicalLeftMargin };
                LayoutUnit leftMarginBoxDelta = clampTo<LayoutUnit>(rawLeftMarginBoxDelta, 0_lu, floatMarginBoxWidth);

                LayoutUnit logicalRightMargin = containingBlock.style().isLeftToRightDirection() ? containingBlock.marginEndForChild(m_renderer) : containingBlock.marginStartForChild(m_renderer);
                LayoutUnit rawRightMarginBoxDelta { segment.logicalRight - containingBlock.logicalWidthForChild(m_renderer) - logicalRightMargin };
                LayoutUnit rightMarginBoxDelta = clampTo<LayoutUnit>(rawRightMarginBoxDelta, -floatMarginBoxWidth, 0_lu);

                m_shapeOutsideDeltas = ShapeOutsideDeltas(leftMarginBoxDelta, rightMarginBoxDelta, true, borderBoxLineTop, lineHeight);
                return m_shapeOutsideDeltas;
            }
        }

        // Lines that do not overlap the shape should act as if the float
        // wasn't there for layout purposes. So we set the deltas to remove the
        // entire width of the float
        m_shapeOutsideDeltas = ShapeOutsideDeltas(floatMarginBoxWidth, -floatMarginBoxWidth, false, borderBoxLineTop, lineHeight);
    }

    return m_shapeOutsideDeltas;
}

}
