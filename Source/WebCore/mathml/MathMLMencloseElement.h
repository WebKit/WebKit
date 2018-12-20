/*
 * Copyright (C) 2014 Gurpreet Kaur (k.gurpreet@samsung.com). All rights reserved.
 * Copyright (C) 2016 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(MATHML)

#include "Element.h"
#include "MathMLRowElement.h"

namespace WebCore {

class MathMLMencloseElement final: public MathMLRowElement {
    WTF_MAKE_ISO_ALLOCATED(MathMLMencloseElement);
public:
    static Ref<MathMLMencloseElement> create(const QualifiedName& tagName, Document&);

    enum MencloseNotationFlag {
        LongDiv = 1 << 1,
        RoundedBox = 1 << 2,
        Circle = 1 << 3,
        Left = 1 << 4,
        Right = 1 << 5,
        Top = 1 << 6,
        Bottom = 1 << 7,
        UpDiagonalStrike = 1 << 8,
        DownDiagonalStrike = 1 << 9,
        VerticalStrike = 1 << 10,
        HorizontalStrike = 1 << 11,
        UpDiagonalArrow = 1 << 12, // FIXME: updiagonalarrow is not implemented. See http://wkb.ug/127466
        PhasorAngle = 1 << 13 // FIXME: phasorangle is not implemented. See http://wkb.ug/127466
        // We do not implement the Radical notation. Authors should instead use the <msqrt> element.
    };
    bool hasNotation(MencloseNotationFlag);

private:
    MathMLMencloseElement(const QualifiedName&, Document&);
    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;
    void parseAttribute(const QualifiedName&, const AtomicString&) final;
    void parseNotationAttribute();
    void clearNotations() { m_notationFlags = 0; }
    void addNotation(MencloseNotationFlag notationFlag) { m_notationFlags.value() |= notationFlag; }
    void addNotationFlags(StringView notation);
    Optional<uint16_t> m_notationFlags;
};

}

#endif // ENABLE(MATHML)
