/*
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CustomPaintImage.h"

#if ENABLE(CSS_PAINTING_API)

#include "CSSComputedStyleDeclaration.h"
#include "CSSImageValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParser.h"
#include "CustomPaintCanvas.h"
#include "GraphicsContext.h"
#include "ImageBitmap.h"
#include "ImageBuffer.h"
#include "JSCSSPaintCallback.h"
#include "JSDOMExceptionHandling.h"
#include "PaintRenderingContext2D.h"
#include "RenderElement.h"
#include "StylePropertyMap.h"
#include "TypedOMCSSImageValue.h"
#include "TypedOMCSSUnitValue.h"
#include "TypedOMCSSUnparsedValue.h"
#include <JavaScriptCore/ConstructData.h>

namespace WebCore {

CustomPaintImage::CustomPaintImage(PaintWorkletGlobalScope::PaintDefinition& definition, const FloatSize& size, RenderElement& element, const Vector<String>& arguments)
    : m_paintDefinition(makeWeakPtr(definition))
    , m_inputProperties(definition.inputProperties)
    , m_element(makeWeakPtr(element))
    , m_arguments(arguments)
{
    setContainerSize(size);
}

CustomPaintImage::~CustomPaintImage() = default;

static RefPtr<TypedOMCSSStyleValue> extractComputedProperty(const String& name, Element& element)
{
    ComputedStyleExtractor extractor(&element);

    if (isCustomPropertyName(name)) {
        auto value = extractor.customPropertyValue(name);
        return StylePropertyMapReadOnly::customPropertyValueOrDefault(name, element.document(), value.get(), &element);
    }

    CSSPropertyID propertyID = cssPropertyID(name);
    if (!propertyID)
        return nullptr;

    auto value = extractor.propertyValue(propertyID, DoNotUpdateLayout);
    return StylePropertyMapReadOnly::reifyValue(value.get(), element.document(), &element);
}

class HashMapStylePropertyMap final : public StylePropertyMap {
public:
    static Ref<StylePropertyMap> create(HashMap<String, RefPtr<TypedOMCSSStyleValue>>&& map)
    {
        return adoptRef(*new HashMapStylePropertyMap(WTFMove(map)));
    }

    static RefPtr<TypedOMCSSStyleValue> extractComputedProperty(const String& name, Element& element)
    {
        ComputedStyleExtractor extractor(&element);

        if (isCustomPropertyName(name)) {
            auto value = extractor.customPropertyValue(name);
            return StylePropertyMapReadOnly::customPropertyValueOrDefault(name, element.document(), value.get(), &element);
        }

        CSSPropertyID propertyID = cssPropertyID(name);
        if (!propertyID)
            return nullptr;

        auto value = extractor.propertyValue(propertyID, DoNotUpdateLayout);
        return StylePropertyMapReadOnly::reifyValue(value.get(), element.document(), &element);
    }

private:
    explicit HashMapStylePropertyMap(HashMap<String, RefPtr<TypedOMCSSStyleValue>>&& map)
        : m_map(WTFMove(map))
    {
    }

    void clearElement() override { }

    RefPtr<TypedOMCSSStyleValue> get(const String& property) const final { return makeRefPtr(m_map.get(property)); }

    HashMap<String, RefPtr<TypedOMCSSStyleValue>> m_map;
};

ImageDrawResult CustomPaintImage::doCustomPaint(GraphicsContext& destContext, const FloatSize& destSize)
{
    if (!m_element || !m_element->element() || !m_paintDefinition)
        return ImageDrawResult::DidNothing;

    JSC::JSValue paintConstructor = m_paintDefinition->paintConstructor;

    if (!paintConstructor)
        return ImageDrawResult::DidNothing;

    ASSERT(!m_element->needsLayout());
    ASSERT(!m_element->element()->document().needsStyleRecalc());

    JSCSSPaintCallback& callback = static_cast<JSCSSPaintCallback&>(m_paintDefinition->paintCallback.get());
    auto* scriptExecutionContext = callback.scriptExecutionContext();
    if (!scriptExecutionContext)
        return ImageDrawResult::DidNothing;

    auto canvas = CustomPaintCanvas::create(*scriptExecutionContext, destSize.width(), destSize.height());
    ExceptionOr<RefPtr<PaintRenderingContext2D>> contextOrException = canvas->getContext();

    if (contextOrException.hasException())
        return ImageDrawResult::DidNothing;
    auto context = contextOrException.releaseReturnValue();

    HashMap<String, RefPtr<TypedOMCSSStyleValue>> propertyValues;

    if (auto* element = m_element->element()) {
        for (auto& name : m_inputProperties)
            propertyValues.add(name, extractComputedProperty(name, *element));
    }

    auto size = CSSPaintSize::create(destSize.width(), destSize.height());
    Ref<StylePropertyMapReadOnly> propertyMap = HashMapStylePropertyMap::create(WTFMove(propertyValues));

    auto& vm = paintConstructor.getObject()->vm();
    JSC::JSLockHolder lock(vm);
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto& globalObject = *paintConstructor.getObject()->globalObject();

    auto& lexicalGlobalObject = globalObject;
    JSC::ArgList noArgs;
    JSC::JSValue thisObject = { JSC::construct(&lexicalGlobalObject, paintConstructor, noArgs, "Failed to construct paint class") };

    if (UNLIKELY(scope.exception())) {
        reportException(&lexicalGlobalObject, scope.exception());
        return ImageDrawResult::DidNothing;
    }

    auto result = callback.handleEvent(WTFMove(thisObject), *context, size, propertyMap, m_arguments);
    if (result.type() != CallbackResultType::Success)
        return ImageDrawResult::DidNothing;

    canvas->replayDisplayList(&destContext);

    return ImageDrawResult::DidDraw;
}

ImageDrawResult CustomPaintImage::draw(GraphicsContext& destContext, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
{
    GraphicsContextStateSaver stateSaver(destContext);
    destContext.setCompositeOperation(options.compositeOperator(), options.blendMode());
    destContext.clip(destRect);
    destContext.translate(destRect.location());
    if (destRect.size() != srcRect.size())
        destContext.scale(destRect.size() / srcRect.size());
    destContext.translate(-srcRect.location());
    return doCustomPaint(destContext, size());
}

void CustomPaintImage::drawPattern(GraphicsContext& destContext, const FloatRect& destRect, const FloatRect& srcRect, const AffineTransform& patternTransform,
    const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& options)
{
    // Allow the generator to provide visually-equivalent tiling parameters for better performance.
    FloatSize adjustedSize = size();
    FloatRect adjustedSrcRect = srcRect;

    // Factor in the destination context's scale to generate at the best resolution
    AffineTransform destContextCTM = destContext.getCTM(GraphicsContext::DefinitelyIncludeDeviceScale);
    double xScale = fabs(destContextCTM.xScale());
    double yScale = fabs(destContextCTM.yScale());
    AffineTransform adjustedPatternCTM = patternTransform;
    adjustedPatternCTM.scale(1.0 / xScale, 1.0 / yScale);
    adjustedSrcRect.scale(xScale, yScale);

    auto buffer = ImageBuffer::createCompatibleBuffer(adjustedSize, ColorSpace::SRGB, destContext);
    if (!buffer)
        return;
    doCustomPaint(buffer->context(), adjustedSize);

    if (destContext.drawLuminanceMask())
        buffer->convertToLuminanceMask();

    buffer->drawPattern(destContext, destRect, adjustedSrcRect, adjustedPatternCTM, phase, spacing, options);
    destContext.setDrawLuminanceMask(false);
}

}
#endif
