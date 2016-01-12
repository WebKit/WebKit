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

CSSFontFaceSource::CSSFontFaceSource(const String& str, CachedFont* font)
    : m_string(str)
    , m_font(font)
    , m_face(0)
#if ENABLE(SVG_FONTS)
    , m_hasExternalSVGFont(false)
#endif
{
    if (m_font)
        m_font->addClient(this);
}

CSSFontFaceSource::~CSSFontFaceSource()
{
    if (m_font)
        m_font->removeClient(this);
    pruneTable();
}

void CSSFontFaceSource::pruneTable()
{
    if (m_fontTable.isEmpty())
        return;

    m_fontTable.clear();
}

bool CSSFontFaceSource::isValid() const
{
    if (m_font)
        return !m_font->errorOccurred();
    return true;
}

void CSSFontFaceSource::fontLoaded(CachedFont*)
{
    pruneTable();
    if (m_face)
        m_face->fontLoaded(this);
}

RefPtr<Font> CSSFontFaceSource::font(const FontDescription& fontDescription, bool syntheticBold, bool syntheticItalic, CSSFontSelector* fontSelector, const FontFeatureSettings& fontFaceFeatures, const FontVariantSettings& fontFaceVariantSettings)
{
    // If the font hasn't loaded or an error occurred, then we've got nothing.
    if (!isValid())
        return nullptr;

    if (!m_font
#if ENABLE(SVG_FONTS)
            && !m_svgFontFaceElement
#endif
    ) {
        // We're local. Just return a Font from the normal cache.
        // We don't want to check alternate font family names here, so pass true as the checkingAlternateName parameter.
        return FontCache::singleton().fontForFamily(fontDescription, m_string, true);
    }

    unsigned hashKey = (fontDescription.computedPixelSize() + 1) << 5 | fontDescription.widthVariant() << 3
                       | (fontDescription.orientation() == Vertical ? 4 : 0) | (syntheticBold ? 2 : 0) | (syntheticItalic ? 1 : 0);

    RefPtr<Font> font = m_fontTable.add(hashKey, nullptr).iterator->value;
    if (font)
        return font.release();

    if (!m_font || m_font->isLoaded()) {
        if (m_font) {
            bool hasExternalSVGFont = false;
#if ENABLE(SVG_FONTS)
            hasExternalSVGFont = m_hasExternalSVGFont;
#endif
            if (!m_font->ensureCustomFontData(hasExternalSVGFont, m_string))
                return nullptr;

            font = m_font->createFont(fontDescription, m_string, syntheticBold, syntheticItalic, hasExternalSVGFont, fontFaceFeatures, fontFaceVariantSettings);
        } else {
#if ENABLE(SVG_FONTS)
            // In-Document SVG Fonts
            if (m_svgFontFaceElement) {
#if ENABLE(SVG_OTF_CONVERTER)
                if (!m_svgFontFaceElement->parentNode() || !is<SVGFontElement>(m_svgFontFaceElement->parentNode()))
                    return nullptr;
                SVGFontElement& fontElement = downcast<SVGFontElement>(*m_svgFontFaceElement->parentNode());
                // FIXME: Re-run this when script modifies the element or any of its descendents
                // FIXME: We might have already converted this font. Make existing conversions discoverable.
                if (auto otfFont = convertSVGToOTFFont(fontElement))
                    m_generatedOTFBuffer = SharedBuffer::adoptVector(otfFont.value());
                if (!m_generatedOTFBuffer)
                    return nullptr;
                auto customPlatformData = createFontCustomPlatformData(*m_generatedOTFBuffer);
                if (!customPlatformData)
                    return nullptr;
                font = Font::create(customPlatformData->fontPlatformData(fontDescription, syntheticBold, syntheticItalic, fontFaceFeatures, fontFaceVariantSettings), true, false);
#else
                font = Font::create(std::make_unique<SVGFontData>(m_svgFontFaceElement.get()), fontDescription.computedPixelSize(), syntheticBold, syntheticItalic);
#endif
            }
#endif
        }
    } else {
        // Kick off the load. Do it soon rather than now, because we may be in the middle of layout,
        // and the loader may invoke arbitrary delegate or event handler code.
        fontSelector->beginLoadingFontSoon(m_font.get());

        Ref<Font> placeholderFont = FontCache::singleton().lastResortFallbackFont(fontDescription);
        Ref<Font> placeholderFontCopyInLoadingState = Font::create(placeholderFont->platformData(), true, true);
        return WTFMove(placeholderFontCopyInLoadingState);
    }

    return font.release();
}

#if ENABLE(FONT_LOAD_EVENTS)
bool CSSFontFaceSource::isDecodeError() const
{
    if (m_font)
        return m_font->status() == CachedResource::DecodeError;
    return false;
}

bool CSSFontFaceSource::ensureFontData()
{
    if (!m_font)
        return false;
    return m_font->ensureCustomFontData(m_hasExternalSVGFont, m_string);
}
#endif

}
