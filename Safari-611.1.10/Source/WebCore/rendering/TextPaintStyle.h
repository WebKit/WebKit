/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "Color.h"
#include "GraphicsTypes.h"
#include "RenderStyleConstants.h"

namespace WebCore {

class Frame;
class GraphicsContext;
class RenderText;
class RenderStyle;
class ShadowData;
struct PaintInfo;

struct TextPaintStyle {
    TextPaintStyle() { }
    TextPaintStyle(const Color&);

    bool operator==(const TextPaintStyle&) const;
    bool operator!=(const TextPaintStyle& other) const { return !(*this == other); }

    Color fillColor;
    Color strokeColor;
    Color emphasisMarkColor;
    float strokeWidth { 0 };
#if ENABLE(LETTERPRESS)
    bool useLetterpressEffect { false };
#endif
#if HAVE(OS_DARK_MODE_SUPPORT)
    bool useDarkAppearance { false };
#endif
    PaintOrder paintOrder { PaintOrder::Normal };
    LineJoin lineJoin { MiterJoin };
    LineCap lineCap { ButtCap };
    float miterLimit { defaultMiterLimit };
};

bool textColorIsLegibleAgainstBackgroundColor(const Color& textColor, const Color& backgroundColor);
TextPaintStyle computeTextPaintStyle(const Frame&, const RenderStyle&, const PaintInfo&);
TextPaintStyle computeTextSelectionPaintStyle(const TextPaintStyle&, const RenderText&, const RenderStyle&, const PaintInfo&, Optional<ShadowData>& selectionShadow);

enum FillColorType { UseNormalFillColor, UseEmphasisMarkColor };
void updateGraphicsContext(GraphicsContext&, const TextPaintStyle&, FillColorType = UseNormalFillColor);

} // namespace WebCore
