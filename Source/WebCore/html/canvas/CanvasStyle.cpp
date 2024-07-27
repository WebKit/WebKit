/*
 * Copyright (C) 2006-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "CanvasStyle.h"

#include "CSSParser.h"
#include "CSSPropertyNames.h"
#include "CSSPropertyParserConsumer+Color.h"
#include "CanvasGradient.h"
#include "CanvasPattern.h"
#include "ColorConversion.h"
#include "Gradient.h"
#include "GraphicsContext.h"
#include "HTMLCanvasElement.h"
#include "StyleProperties.h"

#if ENABLE(OFFSCREEN_CANVAS)
#include "OffscreenCanvas.h"
#endif

namespace WebCore {

class CanvasStyleColorResolutionDelegate final : public CSSUnresolvedColorResolutionDelegate {
public:
    explicit CanvasStyleColorResolutionDelegate(Ref<HTMLCanvasElement> canvasElement)
        : m_canvasElement { WTFMove(canvasElement) }
    {
    }

    Color currentColor() const final;

    Ref<HTMLCanvasElement> m_canvasElement;
};

using LazySlowPathColorParsingParameters = std::tuple<
    CSSPropertyParserHelpers::CSSColorParsingOptions,
    CSSUnresolvedColorResolutionContext,
    std::optional<CanvasStyleColorResolutionDelegate>
>;

Color CanvasStyleColorResolutionDelegate::currentColor() const
{
    if (!m_canvasElement->isConnected() || !m_canvasElement->inlineStyle())
        return Color::black;

    auto colorString = m_canvasElement->inlineStyle()->getPropertyValue(CSSPropertyColor);
    auto color = CSSPropertyParserHelpers::parseColorRaw(WTFMove(colorString), m_canvasElement->cssParserContext(), [] {
        return LazySlowPathColorParsingParameters { { }, { }, std::nullopt };
    });
    if (!color.isValid())
        return Color::black;
    return color;
}

static OptionSet<StyleColor::CSSColorType> allowedColorTypes(ScriptExecutionContext* scriptExecutionContext)
{
    if (scriptExecutionContext && scriptExecutionContext->isDocument())
        return { StyleColor::CSSColorType::Absolute, StyleColor::CSSColorType::Current, StyleColor::CSSColorType::System };

    // FIXME: All canvas types should support all color types, but currently
    //        system colors are not thread safe so are disabled for non-document
    //        based canvases.
    return { StyleColor::CSSColorType::Absolute, StyleColor::CSSColorType::Current };
}

static LazySlowPathColorParsingParameters elementlessColorParsingParameters(ScriptExecutionContext* scriptExecutionContext)
{
    return {
        CSSPropertyParserHelpers::CSSColorParsingOptions {
            .allowedColorTypes = allowedColorTypes(scriptExecutionContext)
        },
        CSSUnresolvedColorResolutionContext {
            .resolvedCurrentColor = Color::black
        },
        std::nullopt
    };
}

static LazySlowPathColorParsingParameters colorParsingParameters(CanvasBase& canvasBase)
{
    RefPtr canvasElement = dynamicDowncast<HTMLCanvasElement>(canvasBase);
    if (!canvasElement)
        return elementlessColorParsingParameters(canvasBase.scriptExecutionContext());

    return {
        CSSPropertyParserHelpers::CSSColorParsingOptions { },
        CSSUnresolvedColorResolutionContext { },
        CanvasStyleColorResolutionDelegate(canvasElement.releaseNonNull())
    };
}

Color parseColor(const String& colorString, CanvasBase& canvasBase)
{
    return CSSPropertyParserHelpers::parseColorRaw(colorString, canvasBase.cssParserContext(), [&] {
        return colorParsingParameters(canvasBase);
    });
}

Color parseColor(const String& colorString, ScriptExecutionContext& scriptExecutionContext)
{
    // FIXME: Add constructor for CSSParserContext that takes a ScriptExecutionContext to allow preferences to be
    //        checked correctly.

    return CSSPropertyParserHelpers::parseColorRaw(colorString, CSSParserContext(HTMLStandardMode), [&] {
        return elementlessColorParsingParameters(&scriptExecutionContext);
    });
}

CanvasStyle::CanvasStyle(Color color)
    : m_style(color)
{
}

CanvasStyle::CanvasStyle(const SRGBA<float>& colorComponents)
    : m_style(convertColor<SRGBA<uint8_t>>(colorComponents))
{
}

CanvasStyle::CanvasStyle(CanvasGradient& gradient)
    : m_style(gradient)
{
}

CanvasStyle::CanvasStyle(CanvasPattern& pattern)
    : m_style(pattern)
{
}

std::optional<CanvasStyle> CanvasStyle::createFromString(const String& colorString, CanvasBase& canvasBase)
{
    auto color = parseColor(colorString, canvasBase);
    if (!color.isValid())
        return { };

    return { color };
}

std::optional<CanvasStyle> CanvasStyle::createFromStringWithOverrideAlpha(const String& colorString, float alpha, CanvasBase& canvasBase)
{
    auto color = parseColor(colorString, canvasBase);
    if (!color.isValid())
        return { };

    return { color.colorWithAlpha(alpha) };
}

bool CanvasStyle::isEquivalentColor(const CanvasStyle& other) const
{
    if (std::holds_alternative<Color>(m_style) && std::holds_alternative<Color>(other.m_style))
        return std::get<Color>(m_style) == std::get<Color>(other.m_style);

    return false;
}

bool CanvasStyle::isEquivalent(const SRGBA<float>& components) const
{
    return std::holds_alternative<Color>(m_style) && std::get<Color>(m_style) == convertColor<SRGBA<uint8_t>>(components);
}

void CanvasStyle::applyStrokeColor(GraphicsContext& context) const
{
    WTF::switchOn(m_style,
        [&context](const Color& color) {
            context.setStrokeColor(color);
        },
        [&context](const Ref<CanvasGradient>& gradient) {
            context.setStrokeGradient(gradient->gradient());
        },
        [&context](const Ref<CanvasPattern>& pattern) {
            context.setStrokePattern(pattern->pattern());
        }
    );
}

void CanvasStyle::applyFillColor(GraphicsContext& context) const
{
    WTF::switchOn(m_style,
        [&context](const Color& color) {
            context.setFillColor(color);
        },
        [&context](const Ref<CanvasGradient>& gradient) {
            context.setFillGradient(gradient->gradient());
        },
        [&context](const Ref<CanvasPattern>& pattern) {
            context.setFillPattern(pattern->pattern());
        }
    );
}

}
