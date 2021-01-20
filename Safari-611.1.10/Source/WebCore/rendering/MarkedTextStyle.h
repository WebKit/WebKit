/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#pragma once

#include "MarkedText.h"
#include "ShadowData.h"
#include "TextDecorationPainter.h"
#include "TextPaintStyle.h"
#include <wtf/Optional.h>

namespace WebCore {

class RenderText;
class RenderedDocumentMarker;

struct MarkedTextStyle {
    static bool areBackgroundMarkedTextStylesEqual(const MarkedTextStyle& a, const MarkedTextStyle& b)
    {
        return a.backgroundColor == b.backgroundColor;
    }
    static bool areForegroundMarkedTextStylesEqual(const MarkedTextStyle& a, const MarkedTextStyle& b)
    {
        return a.textStyles == b.textStyles && a.textShadow == b.textShadow && a.alpha == b.alpha;
    }
    static bool areDecorationMarkedTextStylesEqual(const MarkedTextStyle& a, const MarkedTextStyle& b)
    {
        return a.textDecorationStyles == b.textDecorationStyles && a.textShadow == b.textShadow && a.alpha == b.alpha;
    }

    Color backgroundColor;
    TextPaintStyle textStyles;
    TextDecorationPainter::Styles textDecorationStyles;
    Optional<ShadowData> textShadow;
    float alpha;
};

struct StyledMarkedText : MarkedText {
    StyledMarkedText(const MarkedText& marker)
        : MarkedText { marker }
    {
    }

    MarkedTextStyle style;
};

Vector<StyledMarkedText> subdivideAndResolveStyle(const Vector<MarkedText>&, const RenderText&, bool isFirstLine, const PaintInfo&);

using MarkedTextStylesEqualityFunction = bool (*)(const MarkedTextStyle&, const MarkedTextStyle&);
Vector<StyledMarkedText> coalesceAdjacentMarkedTexts(const Vector<StyledMarkedText>&, MarkedTextStylesEqualityFunction);

}
