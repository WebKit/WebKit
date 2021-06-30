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

#if ENABLE(OFFSCREEN_CANVAS)

#include "CanvasRenderingContext2DBase.h"

#include "OffscreenCanvas.h"

namespace WebCore {

class OffscreenCanvasRenderingContext2D final : public CanvasRenderingContext2DBase {
    WTF_MAKE_ISO_ALLOCATED(OffscreenCanvasRenderingContext2D);
public:
    static bool enabledForContext(ScriptExecutionContext&);

    OffscreenCanvasRenderingContext2D(CanvasBase&, CanvasRenderingContext2DSettings&&);
    virtual ~OffscreenCanvasRenderingContext2D();

    OffscreenCanvas& canvas() const { return downcast<OffscreenCanvas>(canvasBase()); }

    void commit();

    void setFont(const String&);
    CanvasDirection direction() const;
    void fillText(const String& text, double x, double y, std::optional<double> maxWidth = std::nullopt);
    void strokeText(const String& text, double x, double y, std::optional<double> maxWidth = std::nullopt);
    Ref<TextMetrics> measureText(const String& text);

private:
    bool isOffscreen2d() const final { return true; }
    const FontProxy* fontProxy() final;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CANVASRENDERINGCONTEXT(WebCore::OffscreenCanvasRenderingContext2D, isOffscreen2d())

#endif
