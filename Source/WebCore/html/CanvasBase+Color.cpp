/*
 * Copyright (C) 2024 Apple Inc.  All rights reserved.
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
#include "CanvasBase+Color.h"

#include "CSSPropertyParserConsumer+Color.h"
#include "HTMLCanvasElement.h"
#include "ScriptExecutionContext+Color.h"

namespace WebCore {

class CanvasBaseColorResolutionDelegate final : public CSSUnresolvedColorResolutionDelegate {
public:
    explicit CanvasBaseColorResolutionDelegate(Ref<HTMLCanvasElement> canvasElement)
        : m_canvasElement { WTFMove(canvasElement) }
    {
    }

    Color currentColor() const final;

    Ref<HTMLCanvasElement> m_canvasElement;
};

Color CanvasBaseColorResolutionDelegate::currentColor() const
{
    if (!m_canvasElement->isConnected() || !m_canvasElement->inlineStyle())
        return Color::black;

    auto colorString = m_canvasElement->inlineStyle()->getPropertyValue(CSSPropertyColor);
    auto color = CSSPropertyParserHelpers::parseColorRaw(WTFMove(colorString), m_canvasElement->cssParserContext(), [] {
        return CSSPropertyParserHelpers::ColorParsingParameters { { }, { } };
    });

    if (!color.isValid())
        return Color::black;

    return color;
}

using ColorParsingParametersWithCanvasDelegate = std::tuple<
    CSSPropertyParserHelpers::CSSColorParsingOptions,
    CSSUnresolvedColorResolutionContext,
    CanvasBaseColorResolutionDelegate
>;

Color parseColor(const String& colorString, CanvasBase& canvasBase)
{
    RefPtr canvasElement = dynamicDowncast<HTMLCanvasElement>(canvasBase);
    if (!canvasElement)
        return parseColor(colorString, canvasBase.scriptExecutionContext());

    return CSSPropertyParserHelpers::parseColorRawWithDelegate(colorString, canvasBase.cssParserContext(), [&] {
        return ColorParsingParametersWithCanvasDelegate {
            CSSPropertyParserHelpers::CSSColorParsingOptions { },
            CSSUnresolvedColorResolutionContext { },
            CanvasBaseColorResolutionDelegate(canvasElement.releaseNonNull())
        };
    });
}

} // namespace WebCore
