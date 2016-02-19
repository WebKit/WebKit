/*
 * Copyright (C) 2007, 2008, 2010, 2011 Apple Inc. All rights reserved.
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

#include "config.h"
#include "CSSFontFaceSource.h"

#include "CSSFontFace.h"
#include "CSSFontSelector.h"
#include "CachedFont.h"
#include "CachedResourceLoader.h"
#include "Document.h"
#include "ElementIterator.h"
#include "Font.h"
#include "FontCache.h"
#include "FontDescription.h"

#if ENABLE(SVG_OTF_CONVERTER)
#include "FontCustomPlatformData.h"
#include "SVGToOTFFontConversion.h"
#endif

#if ENABLE(SVG_FONTS)
#include "CachedSVGFont.h"
#include "FontCustomPlatformData.h"
#include "SVGFontData.h"
#include "SVGFontElement.h"
#include "SVGFontFaceElement.h"
#include "SVGNames.h"
#include "SVGURIReference.h"
#endif

namespace WebCore {

inline void CSSFontFaceSource::setStatus(Status newStatus)
{
    switch (newStatus) {
    case Status::Pending:
        ASSERT_NOT_REACHED();
        break;
    case Status::Loading:
        ASSERT(status() == Status::Pending);
        break;
    case Status::Success:
        ASSERT(status() == Status::Loading);
        break;
    case Status::Failure:
        ASSERT(status() == Status::Loading);
        break;
    }
    m_status = newStatus;
}

CSSFontFaceSource::CSSFontFaceSource(CSSFontFace& owner, const String& familyNameOrURI, CachedFont* font, SVGFontFaceElement* fontFace)
    : m_familyNameOrURI(familyNameOrURI)
    , m_font(font)
    , m_face(owner)
#if ENABLE(SVG_FONTS) || ENABLE(SVG_OTF_CONVERTER)
    , m_svgFontFaceElement(fontFace)
#endif
{
#if !(ENABLE(SVG_FONTS) || ENABLE(SVG_OTF_CONVERTER))
    UNUSED_PARAM(fontFace);
#endif

    // This may synchronously call fontLoaded().
    if (m_font)
        m_font->addClient(this);

    if (status() == Status::Pending && (!m_font || m_font->isLoaded())) {
        setStatus(Status::Loading);
        if (m_font && m_font->errorOccurred())
            setStatus(Status::Failure);
        else
            setStatus(Status::Success);
    }
}

CSSFontFaceSource::~CSSFontFaceSource()
{
    if (m_font)
        m_font->removeClient(this);
}

void CSSFontFaceSource::fontLoaded(CachedFont& loadedFont)
{
    ASSERT_UNUSED(loadedFont, &loadedFont == m_font.get());

    // If the font is in the cache, this will be synchronously called from CachedFont::addClient().
    if (m_status == Status::Pending)
        setStatus(Status::Loading);

    if (m_font->errorOccurred())
        setStatus(Status::Failure);
    else
        setStatus(Status::Success);
    m_face.fontLoaded(*this);
}

void CSSFontFaceSource::load(CSSFontSelector& fontSelector)
{
    setStatus(Status::Loading);

    fontSelector.beginLoadingFontSoon(m_font.get());
}

RefPtr<Font> CSSFontFaceSource::font(const FontDescription& fontDescription, bool syntheticBold, bool syntheticItalic, const FontFeatureSettings& fontFaceFeatures, const FontVariantSettings& fontFaceVariantSettings)
{
    ASSERT(status() == Status::Success);

    SVGFontFaceElement* fontFaceElement = nullptr;
#if ENABLE(SVG_FONTS)
    fontFaceElement = m_svgFontFaceElement.get();
#endif

    if (!m_font && !fontFaceElement) {
        // We're local. Just return a Font from the normal cache.
        // We don't want to check alternate font family names here, so pass true as the checkingAlternateName parameter.
        return FontCache::singleton().fontForFamily(fontDescription, m_familyNameOrURI, true);
    }

    if (m_font) {
        if (!m_font->ensureCustomFontData(m_familyNameOrURI))
            return nullptr;

        return m_font->createFont(fontDescription, m_familyNameOrURI, syntheticBold, syntheticItalic, fontFaceFeatures, fontFaceVariantSettings);
    }

    // In-Document SVG Fonts
    if (!fontFaceElement)
        return nullptr;

#if ENABLE(SVG_FONTS)
#if ENABLE(SVG_OTF_CONVERTER)
    if (!is<SVGFontElement>(m_svgFontFaceElement->parentNode()))
        return nullptr;
    if (!m_inDocumentCustomPlatformData) {
        SVGFontElement& fontElement = downcast<SVGFontElement>(*m_svgFontFaceElement->parentNode());
        if (auto otfFont = convertSVGToOTFFont(fontElement))
            m_generatedOTFBuffer = SharedBuffer::adoptVector(otfFont.value());
        if (!m_generatedOTFBuffer)
            return nullptr;
        m_inDocumentCustomPlatformData = createFontCustomPlatformData(*m_generatedOTFBuffer);
    }
    if (!m_inDocumentCustomPlatformData)
        return nullptr;
#if PLATFORM(COCOA)
    return Font::create(m_inDocumentCustomPlatformData->fontPlatformData(fontDescription, syntheticBold, syntheticItalic, fontFaceFeatures, fontFaceVariantSettings), true, false);
#else
    return Font::create(m_inDocumentCustomPlatformData->fontPlatformData(fontDescription, syntheticBold, syntheticItalic), true, false);
#endif
#else
    return Font::create(std::make_unique<SVGFontData>(m_svgFontFaceElement.get()), fontDescription.computedPixelSize(), syntheticBold, syntheticItalic);
#endif
#endif

    ASSERT_NOT_REACHED();
    return nullptr;
}

#if ENABLE(SVG_FONTS)
bool CSSFontFaceSource::isSVGFontFaceSource() const
{
    return m_svgFontFaceElement || is<CachedSVGFont>(m_font.get());
}
#endif

}
