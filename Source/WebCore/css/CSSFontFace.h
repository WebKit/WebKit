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
class CSSValue;
class CSSValueList;
class FontDescription;
class Font;

class CSSFontFace : public RefCounted<CSSFontFace> {
public:
    static Ref<CSSFontFace> create(bool isLocalFallback = false) { return adoptRef(*new CSSFontFace(isLocalFallback)); }
    virtual ~CSSFontFace();

    bool setFamilies(CSSValue&);
    bool setStyle(CSSValue&);
    bool setWeight(CSSValue&);
    bool setUnicodeRange(CSSValue&);
    bool setVariantLigatures(CSSValue&);
    bool setVariantPosition(CSSValue&);
    bool setVariantCaps(CSSValue&);
    bool setVariantNumeric(CSSValue&);
    bool setVariantAlternates(CSSValue&);
    bool setVariantEastAsian(CSSValue&);
    bool setFeatureSettings(CSSValue&);

    struct UnicodeRange;
    const CSSValueList* families() const { return m_families.get(); }
    FontTraitsMask traitsMask() const { return m_traitsMask; }
    const Vector<UnicodeRange>& ranges() const { return m_ranges; }
    const FontFeatureSettings& featureSettings() const { return m_featureSettings; }
    const FontVariantSettings& variantSettings() const { return m_variantSettings; }
    void setVariantSettings(const FontVariantSettings& variantSettings) { m_variantSettings = variantSettings; }
    void setTraitsMask(FontTraitsMask traitsMask) { m_traitsMask = traitsMask; }
    bool isLocalFallback() const { return m_isLocalFallback; }

    void addedToSegmentedFontFace(CSSSegmentedFontFace&);
    void removedFromSegmentedFontFace(CSSSegmentedFontFace&);

    bool allSourcesFailed() const;

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
    CSSFontFace(bool isLocalFallback);

    RefPtr<CSSValueList> m_families;
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
