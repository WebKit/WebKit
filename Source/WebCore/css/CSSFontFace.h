/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef CSSFontFace_h
#define CSSFontFace_h

#include "CSSFontFaceRule.h"
#include "CSSFontFaceSource.h"
#include "FontFeatureSettings.h"
#include "TextFlags.h"
#include <memory>
#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class CSSSegmentedFontFace;
class FontDescription;
class Font;

class CSSFontFace : public RefCounted<CSSFontFace> {
public:
    static Ref<CSSFontFace> create(FontTraitsMask traitsMask, bool isLocalFallback = false) { return adoptRef(*new CSSFontFace(traitsMask, isLocalFallback)); }

    FontTraitsMask traitsMask() const { return m_traitsMask; }

    struct UnicodeRange;

    void addRange(UChar32 from, UChar32 to) { m_ranges.append(UnicodeRange(from, to)); }
    const Vector<UnicodeRange>& ranges() const { return m_ranges; }

    void insertFeature(FontFeature&& feature) { m_featureSettings.insert(WTFMove(feature)); }

    void setVariantCommonLigatures(FontVariantLigatures ligatures) { m_variantSettings.commonLigatures = ligatures; }
    void setVariantDiscretionaryLigatures(FontVariantLigatures ligatures) { m_variantSettings.discretionaryLigatures = ligatures; }
    void setVariantHistoricalLigatures(FontVariantLigatures ligatures) { m_variantSettings.historicalLigatures = ligatures; }
    void setVariantContextualAlternates(FontVariantLigatures ligatures) { m_variantSettings.contextualAlternates = ligatures; }
    void setVariantPosition(FontVariantPosition position) { m_variantSettings.position = position; }
    void setVariantCaps(FontVariantCaps caps) { m_variantSettings.caps = caps; }
    void setVariantNumericFigure(FontVariantNumericFigure figure) { m_variantSettings.numericFigure = figure; }
    void setVariantNumericSpacing(FontVariantNumericSpacing spacing) { m_variantSettings.numericSpacing = spacing; }
    void setVariantNumericFraction(FontVariantNumericFraction fraction) { m_variantSettings.numericFraction = fraction; }
    void setVariantNumericOrdinal(FontVariantNumericOrdinal ordinal) { m_variantSettings.numericOrdinal = ordinal; }
    void setVariantNumericSlashedZero(FontVariantNumericSlashedZero slashedZero) { m_variantSettings.numericSlashedZero = slashedZero; }
    void setVariantAlternates(FontVariantAlternates alternates) { m_variantSettings.alternates = alternates; }
    void setVariantEastAsianVariant(FontVariantEastAsianVariant variant) { m_variantSettings.eastAsianVariant = variant; }
    void setVariantEastAsianWidth(FontVariantEastAsianWidth width) { m_variantSettings.eastAsianWidth = width; }
    void setVariantEastAsianRuby(FontVariantEastAsianRuby ruby) { m_variantSettings.eastAsianRuby = ruby; }

    void addedToSegmentedFontFace(CSSSegmentedFontFace&);
    void removedFromSegmentedFontFace(CSSSegmentedFontFace&);

    bool allSourcesFailed() const;

    bool isLocalFallback() const { return m_isLocalFallback; }

    void adoptSource(std::unique_ptr<CSSFontFaceSource>&&);

    void fontLoaded(CSSFontFaceSource&);

    RefPtr<Font> font(const FontDescription&, bool syntheticBold, bool syntheticItalic);

    struct UnicodeRange {
        UnicodeRange(UChar32 from, UChar32 to)
            : m_from(from)
            , m_to(to)
        {
        }

        UChar32 from() const { return m_from; }
        UChar32 to() const { return m_to; }

    private:
        UChar32 m_from;
        UChar32 m_to;
    };

#if ENABLE(SVG_FONTS)
    bool hasSVGFontFaceSource() const;
#endif

private:
    CSSFontFace(FontTraitsMask traitsMask, bool isLocalFallback)
        : m_traitsMask(traitsMask)
        , m_isLocalFallback(isLocalFallback)
    {
    }

    FontTraitsMask m_traitsMask;
    Vector<UnicodeRange> m_ranges;
    HashSet<CSSSegmentedFontFace*> m_segmentedFontFaces;
    FontFeatureSettings m_featureSettings;
    FontVariantSettings m_variantSettings;
    Vector<std::unique_ptr<CSSFontFaceSource>> m_sources;
    bool m_isLocalFallback;
};

}

#endif
