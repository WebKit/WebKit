/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2022-2025 Apple Inc. All rights reserved.
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
#include "LegacyRenderSVGResourceGradient.h"

#include "GradientAttributes.h"
#include "GraphicsContext.h"
#include "RenderSVGText.h"
#include "RenderStyle+GettersInlines.h"
#include "SVGRenderingContext.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(LegacyRenderSVGResourceGradient);

LegacyRenderSVGResourceGradient::LegacyRenderSVGResourceGradient(Type type, SVGGradientElement& node, RenderStyle&& style)
    : LegacyRenderSVGResourceContainer(type, node, WTF::move(style))
{
}

LegacyRenderSVGResourceGradient::~LegacyRenderSVGResourceGradient() = default;

void LegacyRenderSVGResourceGradient::removeAllClientsFromCache()
{
    m_gradientMap.clear();
    m_shouldCollectGradientAttributes = true;
}

void LegacyRenderSVGResourceGradient::removeAllClientsFromCacheAndMarkForInvalidationIfNeeded(bool markForInvalidation, SingleThreadWeakHashSet<RenderObject>* visitedRenderers)
{
    removeAllClientsFromCache();
    markAllClientsForInvalidationIfNeeded(markForInvalidation ? RepaintInvalidation : ParentOnlyInvalidation, visitedRenderers);
}

void LegacyRenderSVGResourceGradient::removeClientFromCache(RenderElement& client)
{
    m_gradientMap.remove(&client);
}

GradientData::Inputs LegacyRenderSVGResourceGradient::computeInputs(RenderElement& renderer, OptionSet<RenderSVGResourceMode> resourceMode)
{
    std::optional<FloatRect> objectBoundingBox;
    if (gradientUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX)
        objectBoundingBox = renderer.objectBoundingBox();

    float textPaintingScale = 1;
    if (resourceMode.contains(RenderSVGResourceMode::ApplyToText))
        textPaintingScale = computeTextPaintingScale(renderer);

    return { objectBoundingBox, textPaintingScale };
}

GradientData* LegacyRenderSVGResourceGradient::gradientDataForRenderer(RenderElement& renderer, const RenderStyle& style, OptionSet<RenderSVGResourceMode> resourceMode)
{
    // Be sure to synchronize all SVG properties on the gradientElement _before_ processing any further.
    // Otherwhise the call to collectGradientAttributes() in createTileImage(), may cause the SVG DOM property
    // synchronization to kick in, which causes removeAllClientsFromCacheAndMarkForInvalidation() to be called, which in turn deletes our
    // GradientData object! Leaving out the line below will cause svg/dynamic-updates/SVG*GradientElement-svgdom* to crash.
    if (m_shouldCollectGradientAttributes) {
        gradientElement().synchronizeAllAttributes();
        if (!collectGradientAttributes())
            return nullptr;

        m_shouldCollectGradientAttributes = false;
    }

    // Spec: When the geometry of the applicable element has no width or height and objectBoundingBox is specified,
    // then the given effect (e.g. a gradient or a filter) will be ignored.
    auto inputs = computeInputs(renderer, resourceMode);
    if (inputs.objectBoundingBox && inputs.objectBoundingBox->isEmpty())
        return nullptr;

    auto& gradientData = *m_gradientMap.ensure(&renderer, [&]() {
        return makeUnique<GradientData>();
    }).iterator->value;

    if (!gradientTransform().isInvertible())
        return nullptr;

    if (gradientData.invalidate(inputs)) {
        gradientData.gradient = buildGradient(style);
        ASSERT(gradientData.userspaceTransform.isIdentity());

        // CG platforms will handle the gradient space transform for text after applying the
        // resource, so don't apply it here. For non-CG platforms, we want the text bounding
        // box applied to the gradient space transform now, so the gradient shader can use it.
        if (gradientData.inputs.objectBoundingBox
#if USE(CG)
            && !resourceMode.contains(RenderSVGResourceMode::ApplyToText)
#endif
        ) {
            gradientData.userspaceTransform.translate(gradientData.inputs.objectBoundingBox->location());
            gradientData.userspaceTransform.scale(gradientData.inputs.objectBoundingBox->size());
        }

        gradientData.userspaceTransform *= gradientTransform();

        // Depending on font scaling factor, we may need to rescale the gradient here since
        // text painting removes the scale factor from the context.
        if (gradientData.inputs.textPaintingScale != 1)
            gradientData.userspaceTransform.scale(gradientData.inputs.textPaintingScale);
    }

    return &gradientData;
}

static inline void applyGradientResource(RenderElement& renderer, const RenderStyle& style, GraphicsContext& context, const GradientData& gradientData, OptionSet<RenderSVGResourceMode> resourceMode)
{
    if (resourceMode.contains(RenderSVGResourceMode::ApplyToText))
        context.setTextDrawingMode(resourceMode.contains(RenderSVGResourceMode::ApplyToFill) ? TextDrawingMode::Fill : TextDrawingMode::Stroke);

    auto userspaceTransform = gradientData.userspaceTransform;

    if (resourceMode.contains(RenderSVGResourceMode::ApplyToFill)) {
        context.setAlpha(style.fillOpacity().value.value);
        context.setFillGradient(*gradientData.gradient, userspaceTransform);
        context.setFillRule(style.fillRule());
    } else if (resourceMode.contains(RenderSVGResourceMode::ApplyToStroke)) {
        if (style.vectorEffect() == VectorEffect::NonScalingStroke)
            userspaceTransform = LegacyRenderSVGResourceContainer::transformOnNonScalingStroke(&renderer, gradientData.userspaceTransform);
        context.setAlpha(style.strokeOpacity().value.value);
        context.setStrokeGradient(*gradientData.gradient, userspaceTransform);
        SVGRenderSupport::applyStrokeStyleToContext(context, style, renderer);
    }
}

class PathOrShapeGradientApplier : public GradientApplier {
    WTF_MAKE_TZONE_ALLOCATED(PathOrShapeGradientApplier);
    WTF_MAKE_NONCOPYABLE(PathOrShapeGradientApplier);
public:
    PathOrShapeGradientApplier() = default;

private:
    bool applyResource(RenderElement&, const RenderStyle&, GraphicsContext*&, const GradientData&, OptionSet<RenderSVGResourceMode>) final;
    void postApplyResource(RenderElement&, GraphicsContext*&, const GradientData&, SVGUnitTypes::SVGUnitType gradientUnits, const AffineTransform& gradientTransform, OptionSet<RenderSVGResourceMode>, const Path*, const RenderElement*) final;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(PathOrShapeGradientApplier);

bool PathOrShapeGradientApplier::applyResource(RenderElement& renderer, const RenderStyle& style, GraphicsContext*& context, const GradientData& gradientData, OptionSet<RenderSVGResourceMode> resourceMode)
{
    context->save();
    applyGradientResource(renderer, style, *context, gradientData, resourceMode);
    return true;
}

void PathOrShapeGradientApplier::postApplyResource(RenderElement&, GraphicsContext*& context, const GradientData&, SVGUnitTypes::SVGUnitType, const AffineTransform&, OptionSet<RenderSVGResourceMode> resourceMode, const Path* path, const RenderElement* shape)
{
    LegacyRenderSVGResource::fillAndStrokePathOrShape(*context, resourceMode, path, shape);
    context->restore();
}

#if USE(CG)
class TextGradientClipper : public GradientApplier {
    WTF_MAKE_TZONE_ALLOCATED(TextGradientClipper);
    WTF_MAKE_NONCOPYABLE(TextGradientClipper);
public:
    TextGradientClipper() = default;

private:
    bool applyResource(RenderElement&, const RenderStyle&, GraphicsContext*&, const GradientData&, OptionSet<RenderSVGResourceMode>) final;
    void postApplyResource(RenderElement&, GraphicsContext*&, const GradientData&, SVGUnitTypes::SVGUnitType gradientUnits, const AffineTransform& gradientTransform, OptionSet<RenderSVGResourceMode>, const Path*, const RenderElement*) final;

    GraphicsContext* m_savedContext { nullptr };
    RefPtr<ImageBuffer> m_imageBuffer;
};

static inline std::tuple<FloatRect, FloatSize> calculateGradientGeometry(RenderElement& renderer)
{
    auto* textRootBlock = RenderSVGText::locateRenderSVGTextAncestor(renderer);
    ASSERT(textRootBlock);

    FloatRect decoratedBounds = textRootBlock->decoratedBoundingBox();

    AffineTransform absoluteTransform = SVGRenderingContext::calculateTransformationToOutermostCoordinateSystem(*textRootBlock);

    // Ignore 2D rotation, as it doesn't affect the size of the target.
    FloatSize scale(absoluteTransform.xScale(), absoluteTransform.yScale());

    return { decoratedBounds, scale };
}

static inline AffineTransform calculateGradientUserspaceTransform(RenderElement& renderer, SVGUnitTypes::SVGUnitType gradientUnits, const AffineTransform& gradientTransform)
{
    if (gradientUnits != SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX)
        return gradientTransform;

    auto* textRootBlock = RenderSVGText::locateRenderSVGTextAncestor(renderer);
    ASSERT(textRootBlock);

    auto boundingBox = textRootBlock->objectBoundingBox();

    AffineTransform userspaceTransform;
    userspaceTransform.translate(boundingBox.location());
    userspaceTransform.scale(boundingBox.size());
    userspaceTransform *= gradientTransform;

    return userspaceTransform;
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(TextGradientClipper);

bool TextGradientClipper::applyResource(RenderElement& renderer, const RenderStyle& style, GraphicsContext*& context, const GradientData& gradientData, OptionSet<RenderSVGResourceMode> resourceMode)
{
    ASSERT(resourceMode.contains(RenderSVGResourceMode::ApplyToText));

    auto [targetRect, scale] = calculateGradientGeometry(renderer);
    ImageBuffer::sizeNeedsClamping(targetRect.size(), scale);

    m_imageBuffer = context->createScaledImageBuffer(targetRect, scale);
    if (!m_imageBuffer)
        return false;

    m_savedContext = context;
    context = &m_imageBuffer->context();

    applyGradientResource(renderer, style, *context, gradientData, resourceMode);
    return true;
}

void TextGradientClipper::postApplyResource(RenderElement& renderer, GraphicsContext*& context, const GradientData& gradientData, SVGUnitTypes::SVGUnitType gradientUnits, const AffineTransform& gradientTransform, OptionSet<RenderSVGResourceMode>, const Path*, const RenderElement*)
{
    if (!m_savedContext)
        return;

    auto [targetRect, scale] = calculateGradientGeometry(renderer);
    ImageBuffer::sizeNeedsClamping(targetRect.size(), scale);

    Ref gradient = *gradientData.gradient;
    auto userspaceTransform = calculateGradientUserspaceTransform(renderer, gradientUnits, gradientTransform);

    // Restore on-screen drawing context
    context = std::exchange(m_savedContext, nullptr);

    GraphicsContextStateSaver stateSaver(*context);

    SVGRenderingContext::clipToImageBuffer(*context, targetRect, scale, m_imageBuffer, false);

    context->setFillGradient(WTF::move(gradient), userspaceTransform);
    context->fillRect(targetRect);

    m_imageBuffer = nullptr;
}

class TextGradientCompositor : public GradientApplier {
    WTF_MAKE_TZONE_ALLOCATED(TextGradientCompositor);
    WTF_MAKE_NONCOPYABLE(TextGradientCompositor);
public:
    TextGradientCompositor() = default;

private:
    bool applyResource(RenderElement&, const RenderStyle&, GraphicsContext*&, const GradientData&, OptionSet<RenderSVGResourceMode>) final;
    void postApplyResource(RenderElement&, GraphicsContext*&, const GradientData&, SVGUnitTypes::SVGUnitType gradientUnits, const AffineTransform& gradientTransform, OptionSet<RenderSVGResourceMode>, const Path*, const RenderElement*) final;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(TextGradientCompositor);

bool TextGradientCompositor::applyResource(RenderElement&, const RenderStyle&, GraphicsContext*& context, const GradientData&, OptionSet<RenderSVGResourceMode>)
{
    context->save();

    context->beginTransparencyLayer(1);
    return true;
}

void TextGradientCompositor::postApplyResource(RenderElement& renderer, GraphicsContext*& context, const GradientData& gradientData, SVGUnitTypes::SVGUnitType gradientUnits, const AffineTransform& gradientTransform, OptionSet<RenderSVGResourceMode>, const Path*, const RenderElement*)
{
    context->setCompositeOperation(CompositeOperator::SourceIn);
    context->beginTransparencyLayer(1);
    context->setCompositeOperation(CompositeOperator::SourceOver);

    [[maybe_unused]] auto [targetRect, scale] = calculateGradientGeometry(renderer);

    Ref gradient = *gradientData.gradient;
    auto userspaceTransform = calculateGradientUserspaceTransform(renderer, gradientUnits, gradientTransform);

    context->setFillGradient(WTF::move(gradient), userspaceTransform);
    context->fillRect(targetRect);

    context->endTransparencyLayer();
    context->endTransparencyLayer();

    context->restore();
}
#endif

auto LegacyRenderSVGResourceGradient::applyResource(RenderElement& renderer, const RenderStyle& style, GraphicsContext*& context, OptionSet<RenderSVGResourceMode> resourceMode) -> OptionSet<ApplyResult>
{
    ASSERT(context);
    ASSERT(!resourceMode.isEmpty());

    auto gradientData = gradientDataForRenderer(renderer, style, resourceMode);
    if (!gradientData)
        return { };

#if USE(CG)
    if (resourceMode.contains(RenderSVGResourceMode::ApplyToText)) {
        // PDF does not support some CompositeOperation
        if (context->renderingMode() != RenderingMode::PDFDocument && style.paintOrder() == Style::SVGPaintOrder::Type::FillStrokeMarkers)
            m_gradientApplier = makeUnique<TextGradientCompositor>();
        else
            m_gradientApplier = makeUnique<TextGradientClipper>();
    }
#endif

    if (!m_gradientApplier)
        m_gradientApplier = makeUnique<PathOrShapeGradientApplier>();

    if (!m_gradientApplier->applyResource(renderer, style, context, *gradientData, resourceMode)) {
        m_gradientApplier = nullptr;
        return { };
    }

    return { ApplyResult::ResourceApplied };
}

void LegacyRenderSVGResourceGradient::postApplyResource(RenderElement& renderer, GraphicsContext*& context, OptionSet<RenderSVGResourceMode> resourceMode, const Path* path, const RenderElement* shape)
{
    ASSERT(context);
    ASSERT(!resourceMode.isEmpty());

    if (!m_gradientApplier)
        return;

    auto gradientData = m_gradientMap.find(&renderer);
    if (gradientData != m_gradientMap.end())
        m_gradientApplier->postApplyResource(renderer, context, *gradientData->value, gradientUnits(), gradientTransform(), resourceMode, path, shape);

    m_gradientApplier = nullptr;
}

GradientColorStops LegacyRenderSVGResourceGradient::stopsByApplyingColorFilter(const GradientColorStops& stops, const RenderStyle& style)
{
    if (style.appleColorFilter().isNone())
        return stops;

    Style::ColorResolver colorResolver { style };
    return stops.mapColors([&](auto& color) {
        return colorResolver.colorApplyingColorFilter(color);
    });
}

GradientSpreadMethod LegacyRenderSVGResourceGradient::platformSpreadMethodFromSVGType(SVGSpreadMethodType method)
{
    switch (method) {
    case SVGSpreadMethodUnknown:
    case SVGSpreadMethodPad:
        return GradientSpreadMethod::Pad;
    case SVGSpreadMethodReflect:
        return GradientSpreadMethod::Reflect;
    case SVGSpreadMethodRepeat:
        return GradientSpreadMethod::Repeat;
    }

    ASSERT_NOT_REACHED();
    return GradientSpreadMethod::Pad;
}

ColorInterpolationMethod LegacyRenderSVGResourceGradient::gradientColorInterpolationMethod() const
{
    switch (style().colorInterpolation()) {
    case ColorInterpolation::Auto:
    case ColorInterpolation::SRGB:
        return { ColorInterpolationMethod::SRGB { }, AlphaPremultiplication::Unpremultiplied };
    case ColorInterpolation::LinearRGB:
        return { ColorInterpolationMethod::SRGBLinear { }, AlphaPremultiplication::Unpremultiplied };
    }

    ASSERT_NOT_REACHED();
    return { ColorInterpolationMethod::SRGB { }, AlphaPremultiplication::Unpremultiplied };
}

} // namespace WebCore
