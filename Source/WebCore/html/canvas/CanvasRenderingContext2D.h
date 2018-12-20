/*
 * Copyright (C) 2006, 2007, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
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

#pragma once

#include "CanvasRenderingContext2DBase.h"
#include "CanvasTextAlign.h"
#include "CanvasTextBaseline.h"
#include "FontCascade.h"
#include "FontSelectorClient.h"
#include "HTMLCanvasElement.h"
#include <memory>

namespace WebCore {

class TextMetrics;

class CanvasRenderingContext2D final : public CanvasRenderingContext2DBase {
public:
    static std::unique_ptr<CanvasRenderingContext2D> create(CanvasBase&, bool usesCSSCompatibilityParseMode, bool usesDashboardCompatibilityMode);

    virtual ~CanvasRenderingContext2D();

    HTMLCanvasElement& canvas() const { return downcast<HTMLCanvasElement>(canvasBase()); }

    void drawFocusIfNeeded(Element&);
    void drawFocusIfNeeded(Path2D&, Element&);

    float webkitBackingStorePixelRatio() const { return 1; }

    String font() const;
    void setFont(const String&);

    CanvasTextAlign textAlign() const;
    void setTextAlign(CanvasTextAlign);

    CanvasTextBaseline textBaseline() const;
    void setTextBaseline(CanvasTextBaseline);

    CanvasDirection direction() const;
    void setDirection(CanvasDirection);

    void fillText(const String& text, float x, float y, Optional<float> maxWidth = WTF::nullopt);
    void strokeText(const String& text, float x, float y, Optional<float> maxWidth = WTF::nullopt);
    Ref<TextMetrics> measureText(const String& text);

    bool is2d() const override { return true; }

private:
    CanvasRenderingContext2D(CanvasBase&, bool usesCSSCompatibilityParseMode, bool usesDashboardCompatibilityMode);

    // The relationship between FontCascade and CanvasRenderingContext2D::FontProxy must hold certain invariants.
    // Therefore, all font operations must pass through the State.
    const FontProxy& fontProxy();

    void drawTextInternal(const String& text, float x, float y, bool fill, Optional<float> maxWidth = WTF::nullopt);

    void drawFocusIfNeededInternal(const Path&, Element&);

    void prepareGradientForDashboard(CanvasGradient& gradient) const;

    TextDirection toTextDirection(CanvasRenderingContext2DBase::Direction, const RenderStyle** computedStyle = nullptr) const;

    FloatPoint textOffset(float width, TextDirection);
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CANVASRENDERINGCONTEXT(WebCore::CanvasRenderingContext2D, is2d())
