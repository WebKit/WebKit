/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
#include "WebCoreTextRenderer.h"

#include "FontCascade.h"
#include "FontDescription.h"
#include "GraphicsContext.h"
#include "StringTruncator.h"
#include "TextRun.h"

namespace WebCore {

static bool shouldUseFontSmoothing = true;

static bool isOneLeftToRightRun(const TextRun& run)
{
    for (int i = 0; i < run.length(); i++) {
        UCharDirection direction = u_charDirection(run[i]);
        if (direction == U_RIGHT_TO_LEFT || direction > U_OTHER_NEUTRAL)
            return false;
    }
    return true;
}

static void doDrawTextAtPoint(GraphicsContext& context, const String& text, const IntPoint& point, const FontCascade& font, const Color& color, int underlinedIndex)
{
    TextRun run(text);

    context.setFillColor(color);
    if (isOneLeftToRightRun(run))
        font.drawText(context, run, point);
    else
        context.drawBidiText(font, run, point);

    if (underlinedIndex >= 0) {
        ASSERT_WITH_SECURITY_IMPLICATION(underlinedIndex < static_cast<int>(text.length()));

        int beforeWidth;
        if (underlinedIndex > 0) {
            TextRun beforeRun(StringView(text).substring(0, underlinedIndex));
            beforeWidth = font.width(beforeRun);
        } else
            beforeWidth = 0;

        TextRun underlinedRun(StringView(text).substring(underlinedIndex, 1));
        int underlinedWidth = font.width(underlinedRun);

        IntPoint underlinePoint(point);
        underlinePoint.move(beforeWidth, 1);

        context.setStrokeColor(color);
        context.drawLineForText(FloatRect(underlinePoint, FloatSize(underlinedWidth, font.size() / 16)), false);
    }
}

void WebCoreDrawDoubledTextAtPoint(GraphicsContext& context, const String& text, const IntPoint& point, const FontCascade& font, const Color& topColor, const Color& bottomColor, int underlinedIndex)
{
    context.save();

    IntPoint textPos = point;

    doDrawTextAtPoint(context, text, textPos, font, bottomColor, underlinedIndex);
    textPos.move(0, -1);
    doDrawTextAtPoint(context, text, textPos, font, topColor, underlinedIndex);

    context.restore();
}

float WebCoreTextFloatWidth(const String& text, const FontCascade& font)
{
    return StringTruncator::width(text, font);
}

void WebCoreSetShouldUseFontSmoothing(bool smooth)
{
    shouldUseFontSmoothing = smooth;
}

bool WebCoreShouldUseFontSmoothing()
{
    return shouldUseFontSmoothing;
}

void WebCoreSetAlwaysUsesComplexTextCodePath(bool complex)
{
    FontCascade::setCodePath(complex ? FontCascade::Complex : FontCascade::Auto);
}

bool WebCoreAlwaysUsesComplexTextCodePath()
{
    return FontCascade::codePath() == FontCascade::Complex;
}

} // namespace WebCore
