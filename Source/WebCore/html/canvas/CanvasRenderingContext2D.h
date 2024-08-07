/*
 * Copyright (C) 2006-2024 Apple Inc. All rights reserved.
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
#include "HTMLCanvasElement.h"
#include <memory>

namespace WebCore {

class TextMetrics;

class CanvasRenderingContext2D final : public CanvasRenderingContext2DBase {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(CanvasRenderingContext2D);
public:
    static std::unique_ptr<CanvasRenderingContext2D> create(CanvasBase&, CanvasRenderingContext2DSettings&&, bool usesCSSCompatibilityParseMode);

    virtual ~CanvasRenderingContext2D();

    HTMLCanvasElement& canvas() const { return downcast<HTMLCanvasElement>(canvasBase()); }

    void drawFocusIfNeeded(Element&);
    void drawFocusIfNeeded(Path2D&, Element&);

    void setFont(const String&);

    CanvasDirection direction() const;

    void fillText(const String& text, double x, double y, std::optional<double> maxWidth = std::nullopt);
    void strokeText(const String& text, double x, double y, std::optional<double> maxWidth = std::nullopt);
    Ref<TextMetrics> measureText(const String& text);

private:
    CanvasRenderingContext2D(CanvasBase&, CanvasRenderingContext2DSettings&&, bool usesCSSCompatibilityParseMode);

    bool is2d() const final { return true; }
    const FontProxy* fontProxy() final;

    std::optional<FilterOperations> setFilterStringWithoutUpdatingStyle(const String&) override;
    RefPtr<Filter> createFilter(const FloatRect& bounds) const override;
    IntOutsets calculateFilterOutsets(const FloatRect& bounds) const override;

    void setFontWithoutUpdatingStyle(const String&);

    void drawTextInternal(const String& text, double x, double y, bool fill, std::optional<double> maxWidth = std::nullopt);

    void drawFocusIfNeededInternal(const Path&, Element&);

    TextDirection toTextDirection(CanvasRenderingContext2DBase::Direction, const RenderStyle** computedStyle = nullptr) const;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CANVASRENDERINGCONTEXT(WebCore::CanvasRenderingContext2D, is2d())
