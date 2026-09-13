/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "RenderStyleConstants.h"
#include "RenderText.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

// A glyph the UA draws itself (see TextBoxPainter) rather than rendering as text.
class RenderGlyph final : public RenderText {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(RenderGlyph);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderGlyph);
public:
    RenderGlyph(Document& document, SynthesizedGlyph glyph)
        : RenderText(Type::Glyph, document, fallbackText(glyph))
        , m_glyph(glyph)
    {
        ASSERT(isRenderGlyph());
    }

    SynthesizedGlyph glyph() const { return m_glyph; }
    float advanceRatio() const { return 0.52f; }

    // Null (unspecified) is distinct from empty string.
    const String& altText() const LIFETIME_BOUND { return m_altText; }
    void setAltText(const String& altText) { m_altText = altText; }

private:
    ASCIILiteral renderName() const override { return "RenderGlyph"_s; }
    bool canBeSelectionLeaf() const final { return false; }

    const SynthesizedGlyph m_glyph;
    String m_altText;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderGlyph, isRenderGlyph())
