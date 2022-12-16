/*
 * Copyright (C) 2008-2017 Apple Inc. All rights reserved.
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

#include "ThemeCocoa.h"

#if PLATFORM(MAC)

namespace WebCore {

class ThemeMac final : public ThemeCocoa {
public:
#if HAVE(LARGE_CONTROL_SIZE)
    static bool supportsLargeFormControls();
#endif

    static NSView *ensuredView(ScrollView*, const ControlStates&, bool useUnparentedView = false);
    WEBCORE_EXPORT static void setUseFormSemanticContext(bool);
    static bool useFormSemanticContext();
    static void setFocusRingClipRect(const FloatRect&);
    static bool drawCellOrFocusRingWithViewIntoContext(NSCell *, GraphicsContext&, const FloatRect&, NSView *, bool drawButtonCell, bool drawFocusRing, float deviceScaleFactor);

private:
    friend NeverDestroyed<ThemeMac>;
    ThemeMac() = default;

    int baselinePositionAdjustment(ControlPartType) const final;

    std::optional<FontCascadeDescription> controlFont(ControlPartType, const FontCascade&, float zoomFactor) const final;

    LengthSize controlSize(ControlPartType, const FontCascade&, const LengthSize&, float zoomFactor) const final;
    LengthSize minimumControlSize(ControlPartType, const FontCascade&, const LengthSize&, float zoomFactor) const final;

    LengthBox controlPadding(ControlPartType, const FontCascade&, const LengthBox& zoomedBox, float zoomFactor) const final;
    LengthBox controlBorder(ControlPartType, const FontCascade&, const LengthBox& zoomedBox, float zoomFactor) const final;

    bool controlRequiresPreWhiteSpace(ControlPartType type) const final { return type == ControlPartType::PushButton; }

    void paint(ControlPartType, ControlStates&, GraphicsContext&, const FloatRect&, float zoomFactor, ScrollView*, float deviceScaleFactor, float pageScaleFactor, bool useSystemAppearance, bool useDarkAppearance, const Color& tintColor) final;
    void inflateControlPaintRect(ControlPartType, const ControlStates&, FloatRect&, float zoomFactor) const final;

    bool userPrefersReducedMotion() const final;
    bool userPrefersContrast() const final;
};

} // namespace WebCore

#endif // PLATFORM(MAC)
