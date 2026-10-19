/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
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
#include "StylePaintImage.h"

#include "CSSPaintImageValue.h"
#include "CSSPropertyNames.h"
#include "CSSPropertyParser.h"
#include "CSSVariableData.h"
#include "ContextDestructionObserverInlines.h"
#include "CustomPaintCanvas.h"
#include "DeprecatedCSSOMValue.h"
#include "GraphicsContext.h"
#include "HashMapStylePropertyMapReadOnly.h"
#include "ImageBuffer.h"
#include "JSCSSPaintCallback.h"
#include "JSDOMExceptionHandling.h"
#include "PaintRenderingContext2D.h"
#include "PaintWorkletGlobalScope.h"
#include "RenderElement.h"
#include "RenderElementInlines.h"
#include "RenderObjectInlines.h"
#include "StyleExtractor.h"
#include <JavaScriptCore/ArgList.h>
#include <JavaScriptCore/ConstructData.h>
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <wtf/PointerComparison.h>

namespace WebCore {
namespace Style {

PaintImage::PaintImage(CustomIdent&& name, Ref<CSSVariableData>&& arguments)
    : GeneratedImage { Type::PaintImage }
    , m_name { WTF::move(name) }
    , m_arguments { WTF::move(arguments) }
{
}

PaintImage::~PaintImage() = default;

bool PaintImage::operator==(const Image& other) const
{
    // FIXME: Should probably also compare arguments?
    auto* otherPaintImage = dynamicDowncast<PaintImage>(other);
    return otherPaintImage && otherPaintImage->m_name == m_name;
}

Ref<CSSValue> PaintImage::computedStyleValue(const Style::ComputedStyle& style, PropertyValueType) const
{
    return CSSPaintImageValue::create(toCSS(m_name, style), m_arguments);
}

Ref<DeprecatedCSSOMValue> PaintImage::computedStyleDeprecatedCSSOMValue(CSSValuePool&, const Style::ComputedStyle& style, CSSStyleDeclaration& owner) const
{
    return computedStyleValue(style, PropertyValueType::Resolved)->createDeprecatedCSSOMWrapper(owner);
}

bool PaintImage::isPending() const
{
    return false;
}

void PaintImage::load(CachedResourceLoader&, const ResourceLoaderOptions&)
{
}

ImageDrawResult PaintImage::draw(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatRect& source, ImagePaintingOptions options, bool) const
{
    auto size = concreteObjectSize.size() * concreteObjectSize.zoom();
    if (!canDrawAtSize(renderer, size))
        return ImageDrawResult::DidNothing;

    return drawIntoDestination(context, destination, source, options, [&](GraphicsContext& context) {
        return paint(context, renderer, size);
    });
}

ImageDrawResult PaintImage::drawAsPattern(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options, bool) const
{
    auto size = concreteObjectSize.size() * concreteObjectSize.zoom();
    if (!canDrawAtSize(renderer, size) || context.paintingDisabled())
        return ImageDrawResult::DidNothing;

    RefPtr buffer = context.createAlignedImageBuffer(size);
    if (!buffer)
        return ImageDrawResult::DidNothing;

    // Factor in the destination context's scale to generate at the best resolution.
    auto contextCTM = context.getCTM(GraphicsContext::DefinitelyIncludeDeviceScale);
    double xScale = std::abs(contextCTM.xScale());
    double yScale = std::abs(contextCTM.yScale());
    auto adjustedPatternCTM = patternTransform;
    adjustedPatternCTM.scale(1.0 / xScale, 1.0 / yScale);
    auto adjustedTile = tile;
    adjustedTile.scale(xScale, yScale);

    paint(buffer->context(), renderer, size);

    if (options.drawLuminanceMask() == DrawLuminanceMask::Yes)
        buffer->convertToLuminanceMask();

    context.drawPattern(*buffer, destination, adjustedTile, adjustedPatternCTM, phase, spacing, options);

    return ImageDrawResult::DidDraw;
}

Vector<WTF::String> PaintImage::paintArguments() const
{
    // FIXME: Check if argument list matches syntax.
    Vector<WTF::String> arguments;
    CSSParserTokenRange localRange(m_arguments->tokenRange());

    while (!localRange.atEnd()) {
        StringBuilder builder;
        while (!localRange.atEnd() && localRange.peek() != CommaToken) {
            if (localRange.peek() == CommentToken)
                localRange.consume();
            else if (localRange.peek().getBlockType() == CSSParserToken::BlockStart) {
                localRange.peek().serialize(builder);
                builder.append(localRange.consumeBlock().serialize(), ')');
            } else
                localRange.consume().serialize(builder);
        }
        if (!localRange.atEnd())
            localRange.consume(); // comma token
        arguments.append(builder.toString());
    }

    return arguments;
}

static RefPtr<CSSValue> extractComputedProperty(const AtomString& name, Element& element)
{
    Extractor extractor(&element);

    if (isCustomPropertyName(name))
        return extractor.customPropertyValue(name);

    CSSPropertyID propertyID = cssPropertyID(name);
    if (propertyID == CSSPropertyID::Invalid)
        return nullptr;

    return extractor.propertyValue(propertyID, Extractor::UpdateLayout::No);
}

ImageDrawResult PaintImage::paint(GraphicsContext& context, const RenderElement& renderer, FloatSize paintSize) const
{
    RefPtr element = renderer.element();
    if (!element)
        return ImageDrawResult::DidNothing;

    RefPtr globalScope = protect(renderer.document())->paintWorkletGlobalScopeForName(m_name.value);
    if (!globalScope)
        return ImageDrawResult::DidNothing;

    // The definition is read under the worklet's lock, and the callback, which is script, runs without it.
    WeakPtr<PaintDefinition> paintDefinition;
    Vector<AtomString> inputProperties;
    {
        Locker locker { globalScope->paintDefinitionLock() };
        CheckedPtr registration = globalScope->paintDefinitionMap().get(m_name.value);
        if (!registration)
            return ImageDrawResult::DidNothing;

        paintDefinition = *registration;
        inputProperties = registration->inputProperties;
    }

    CheckedPtr definition = paintDefinition.get();
    if (!definition)
        return ImageDrawResult::DidNothing;

    JSC::JSValue paintConstructor = definition->paintConstructor;
    if (!paintConstructor)
        return ImageDrawResult::DidNothing;

    ASSERT(!renderer.needsLayout());
    ASSERT(!element->document().needsStyleRecalc());

    Ref callback = definition->paintCallback.get();
    RefPtr scriptExecutionContext = callback->scriptExecutionContext();
    if (!scriptExecutionContext)
        return ImageDrawResult::DidNothing;

    Ref canvas = CustomPaintCanvas::create(*scriptExecutionContext, paintSize.width(), paintSize.height());
    RefPtr canvasContext = canvas->getContext();

    HashMap<AtomString, RefPtr<CSSValue>> propertyValues;
    for (auto& name : inputProperties)
        propertyValues.add(name, extractComputedProperty(name, *element));

    auto size = CSSPaintSize::create(paintSize.width(), paintSize.height());
    Ref<StylePropertyMapReadOnly> propertyMap = HashMapStylePropertyMapReadOnly::create(WTF::move(propertyValues));

    auto& vm = paintConstructor.getObject()->vm();
    JSC::JSLockHolder lock(vm);
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto& globalObject = *paintConstructor.getObject()->realm();

    auto& lexicalGlobalObject = globalObject;
    JSC::ArgList noArgs;
    JSC::JSValue thisObject = { JSC::construct(&lexicalGlobalObject, paintConstructor, noArgs, "Failed to construct paint class"_s) };

    if (scope.exception()) [[unlikely]] {
        reportException(&lexicalGlobalObject, scope.exception());
        return ImageDrawResult::DidNothing;
    }

    auto result = callback->invoke(WTF::move(thisObject), *canvasContext, size, propertyMap, paintArguments());
    if (result.type() != CallbackResultType::Success)
        return ImageDrawResult::DidNothing;

    canvas->replayDisplayList(context);

    return ImageDrawResult::DidDraw;
}

bool PaintImage::knownToBeOpaque(const RenderElement&) const
{
    return false;
}

bool PaintImage::canDraw(const RenderElement& renderer) const
{
    RefPtr selectedGlobalScope = protect(renderer.document())->paintWorkletGlobalScopeForName(m_name.value);
    if (!selectedGlobalScope)
        return false;

    Locker locker { selectedGlobalScope->paintDefinitionLock() };
    return !!selectedGlobalScope->paintDefinitionMap().get(m_name.value);
}

bool PaintImage::canDrawAtSize(const RenderElement& renderer, const FloatSize& size) const
{
    return !size.isEmpty() && canDraw(renderer);
}

} // namespace Style
} // namespace WebCore
