/*
 * Copyright (C) 2015, 2020 Igalia S.L.
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
#include "StyleColor.h"
#include "Theme.h"

namespace WebCore {

class Path;

class ThemeAdwaita : public Theme {
public:
    enum class PaintRounded : bool { No, Yes };

    static void paintFocus(GraphicsContext&, const FloatRect&, int offset, const Color&, PaintRounded = PaintRounded::No);
    static void paintFocus(GraphicsContext&, const Path&, const Color&);
    static void paintFocus(GraphicsContext&, const Vector<FloatRect>&, const Color&, PaintRounded = PaintRounded::No);
    enum class ArrowDirection { Up, Down };
    static void paintArrow(GraphicsContext&, const FloatRect&, ArrowDirection, bool);

    virtual void platformColorsDidChange() { };

    void setAccentColor(const Color&);
    Color accentColor();
private:
    LengthSize controlSize(ControlPartType, const FontCascade&, const LengthSize&, float) const final;
    LengthSize minimumControlSize(ControlPartType, const FontCascade&, const LengthSize&, float) const final;
    LengthBox controlBorder(ControlPartType, const FontCascade&, const LengthBox&, float) const final;
    void paint(ControlPartType, ControlStates&, GraphicsContext&, const FloatRect&, float, ScrollView*, float, float, bool, bool, const Color&) final;

    void paintCheckbox(ControlStates&, GraphicsContext&, const FloatRect&, bool, const Color&);
    void paintRadio(ControlStates&, GraphicsContext&, const FloatRect&, bool, const Color&);
    void paintButton(ControlStates&, GraphicsContext&, const FloatRect&, bool);
    void paintSpinButton(ControlStates&, GraphicsContext&, const FloatRect&, bool);

    static Color focusColor(const Color&);

    Color m_accentColor { SRGBA<uint8_t> { 52, 132, 228 } };
};

} // namespace WebCore
