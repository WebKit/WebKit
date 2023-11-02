/*
 * Copyright (C) 2007-2022 Apple Inc. All rights reserved.
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

#include "CSSParserContext.h"
#include "CSSValue.h"
#include "CachedResourceHandle.h"
#include "ResourceLoaderOptions.h"
#include <wtf/Vector.h>

namespace WebCore {

class CachedFont;
class FontLoadRequest;
class SVGFontFaceElement;
class ScriptExecutionContext;
class WeakPtrImplWithEventTargetData;

class CSSFontFaceSrcLocalValue final : public CSSValue {
public:
    static Ref<CSSFontFaceSrcLocalValue> create(AtomString fontFaceName);
    ~CSSFontFaceSrcLocalValue();

    bool isEmpty() const { return m_fontFaceName.isEmpty(); }
    const AtomString& fontFaceName() const { return m_fontFaceName; }

    SVGFontFaceElement* svgFontFaceElement() const;
    void setSVGFontFaceElement(SVGFontFaceElement&);

    String customCSSText() const;
    bool equals(const CSSFontFaceSrcLocalValue&) const;

private:
    explicit CSSFontFaceSrcLocalValue(AtomString&&);

    AtomString m_fontFaceName;
    WeakPtr<SVGFontFaceElement, WeakPtrImplWithEventTargetData> m_element;
};

enum class FontTechnology : uint8_t {
    ColorColrv0,
    ColorColrv1,
    ColorCbdt,
    ColorSbix,
    ColorSvg,
    FeaturesAat,
    FeaturesGraphite,
    FeaturesOpentype,
    Incremental,
    Palettes,
    Variations,
    // Reserved for invalid conversion result.
    Invalid
};

inline ASCIILiteral cssTextFromFontTech(FontTechnology tech)
{
    switch (tech) {
    case FontTechnology::ColorColrv0:
        return "color-colrv0"_s;
    case FontTechnology::ColorColrv1:
        return "color-colrv1"_s;
    case FontTechnology::ColorCbdt:
        return "color-cbdt"_s;
    case FontTechnology::ColorSbix:
        return "color-sbix"_s;
    case FontTechnology::ColorSvg:
        return "color-svg"_s;
    case FontTechnology::FeaturesAat:
        return "features-aat"_s;
    case FontTechnology::FeaturesGraphite:
        return "features-graphite"_s;
    case FontTechnology::FeaturesOpentype:
        return "features-opentype"_s;
    case FontTechnology::Incremental:
        return "incremental"_s;
    case FontTechnology::Palettes:
        return "palettes"_s;
    case FontTechnology::Variations:
        return "variations"_s;
    default:
        return ""_s;
    }
}

class CSSFontFaceSrcResourceValue final : public CSSValue {
public:

    static Ref<CSSFontFaceSrcResourceValue> create(ResolvedURL, String format, Vector<FontTechnology>&& technologies, LoadedFromOpaqueSource = LoadedFromOpaqueSource::No);

    bool isEmpty() const { return m_location.specifiedURLString.isEmpty(); }
    std::unique_ptr<FontLoadRequest> fontLoadRequest(ScriptExecutionContext&, bool isInitiatingElementInUserAgentShadowTree);

    String customCSSText() const;
    bool customTraverseSubresources(const Function<bool(const CachedResource&)>&) const;
    void customSetReplacementURLForSubresources(const HashMap<String, String>&);
    void customClearReplacementURLForSubresources();
    bool equals(const CSSFontFaceSrcResourceValue&) const;

private:
    explicit CSSFontFaceSrcResourceValue(ResolvedURL&&, String&& format, Vector<FontTechnology>&& technologies, LoadedFromOpaqueSource);

    ResolvedURL m_location;
    String m_format;
    Vector<FontTechnology> m_technologies;
    LoadedFromOpaqueSource m_loadedFromOpaqueSource { LoadedFromOpaqueSource::No };
    CachedResourceHandle<CachedFont> m_cachedFont;
    String m_replacementURLString;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSFontFaceSrcLocalValue, isFontFaceSrcLocalValue())
SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSFontFaceSrcResourceValue, isFontFaceSrcResourceValue())
