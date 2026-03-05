/*
 * Copyright (C) 2006-2025 Apple Inc. All rights reserved.
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

#include "ContextDestructionObserverInlines.h"
#include "CSSParserContext.h"
#include "CSSParserMode.h"
#include "CSSPropertyNames.h"
#include "CSSPropertyParserConsumer+ColorInlines.h"
#include "CanvasGradient.h"
#include "CanvasPattern.h"
#include "ColorConversion.h"
#include "Gradient.h"
#include "GraphicsContext.h"
#include "HTMLCanvasElement.h"
#include "NodeDocument.h"
#include "NodeInlines.h"
#include "StyleProperties.h"

#if ENABLE(OFFSCREEN_CANVAS)
#include "OffscreenCanvas.h"
#endif

namespace WebCore {

class CanvasStyleColorResolutionDelegate final : public CSS::PlatformColorResolutionDelegate {
public:
    explicit CanvasStyleColorResolutionDelegate(Ref<HTMLCanvasElement> canvasElement)
        : m_canvasElement { WTF::move(canvasElement) }
    {
    }

    Color currentColor() const final;

    const Ref<HTMLCanvasElement> m_canvasElement;
};

Color CanvasStyleColorResolutionDelegate::currentColor() const
{
    if (!m_canvasElement->isConnected() || !m_canvasElement->inlineStyle())
        return Color::black;

    auto colorString = m_canvasElement->inlineStyle()->getPropertyValue(CSSPropertyColor);
    auto color = CSSPropertyParserHelpers::parseColorRaw(colorString, m_canvasElement->cssParserContext(), protect(m_canvasElement->document()).get());
    if (color.isValid())
        return color;
    return Color::black;
}

static OptionSet<CSS::ColorType> allowedColorTypes(ScriptExecutionContext* scriptExecutionContext)
{
    if (scriptExecutionContext && scriptExecutionContext->isDocument())
        return { CSS::ColorType::Absolute, CSS::ColorType::Current, CSS::ColorType::System };

    // FIXME: All canvas types should support all color types, but currently
    //        system colors are not thread safe so are disabled for non-document
    //        based canvases.
    return { CSS::ColorType::Absolute, CSS::ColorType::Current };
}

Color parseColor(const String& colorString, CanvasBase& canvasBase)
{
    using namespace CSSPropertyParserHelpers;
    auto cssParserContext = canvasBase.cssParserContext();
    auto color = parseColorRawSimple(colorString, cssParserContext);
    if (color.isValid())
        return color;

    if (RefPtr canvasElement = dynamicDowncast<HTMLCanvasElement>(canvasBase)) {
        RefPtr scriptExecutionContext = canvasElement->scriptExecutionContext();
        CanvasStyleColorResolutionDelegate delegate(canvasElement.releaseNonNull());
        CSSColorParsingOptions options;
        CSS::PlatformColorResolutionState state {
            .delegate = &delegate
        };
        return parseColorRawGeneral(colorString, cssParserContext, *scriptExecutionContext, options, state);
    }

    RefPtr scriptExecutionContext = canvasBase.scriptExecutionContext();
    CSSColorParsingOptions options {
        .allowedColorTypes = allowedColorTypes(scriptExecutionContext.get())
    };
    CSS::PlatformColorResolutionState state {
        .resolvedCurrentColor = Color::black
    };
    return parseColorRawGeneral(colorString, cssParserContext, *scriptExecutionContext, options, state);
}

Color parseColor(const String& colorString, ScriptExecutionContext& scriptExecutionContext)
{
    // FIXME: Add constructor for CSSParserContext that takes a ScriptExecutionContext to allow preferences to be
    //        checked correctly.
    using namespace CSSPropertyParserHelpers;
    auto color = parseColorRawSimple(colorString, CSSParserContext(HTMLStandardMode));
    if (color.isValid())
        return color;
    CSSColorParsingOptions options {
        .allowedColorTypes = allowedColorTypes(&scriptExecutionContext)
    };
    CSS::PlatformColorResolutionState state {
        .resolvedCurrentColor = Color::black
    };
    return parseColorRawGeneral(colorString, CSSParserContext(HTMLStandardMode), scriptExecutionContext, options, state);
}

}
