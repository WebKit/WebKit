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
#include "FontCache.h"
#include "FontDescription.h"
#include "SimpleFontData.h"

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
    if (m_fontDataTable.isEmpty())
        return;

    m_fontDataTable.clear();
}

bool CSSFontFaceSource::isLoaded() const
{
    if (m_font)
        return m_font->isLoaded();
    return true;
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

PassRefPtr<SimpleFontData> CSSFontFaceSource::getFontData(const FontDescription& fontDescription, bool syntheticBold, bool syntheticItalic, CSSFontSelector* fontSelector)
{
    // If the font hasn't loaded or an error occurred, then we've got nothing.
    if (!isValid())
        return 0;

    if (!m_font
#if ENABLE(SVG_FONTS)
            && !m_svgFontFaceElement
#endif
    ) {
        // We're local. Just return a SimpleFontData from the normal cache.
        // We don't want to check alternate font family names here, so pass true as the checkingAlternateName parameter.
        return fontCache().getCachedFontData(fontDescription, m_string, true);
    }

    // See if we have a mapping in our FontData cache.
    unsigned hashKey = (fontDescription.computedPixelSize() + 1) << 5 | fontDescription.widthVariant() << 3
                       | (fontDescription.orientation() == Vertical ? 4 : 0) | (syntheticBold ? 2 : 0) | (syntheticItalic ? 1 : 0);

    RefPtr<SimpleFontData> fontData = m_fontDataTable.add(hashKey, nullptr).iterator->value;
    if (fontData)
        return fontData.release();

    // If we are still loading, then we let the system pick a font.
    if (isLoaded()) {
        if (m_font) {
            // Create new FontPlatformData from our CGFontRef, point size and ATSFontRef.
            if (!m_font->ensureCustomFontData(m_hasExternalSVGFont))
                return nullptr;

            fontData = m_font->getFontData(fontDescription, m_string, syntheticBold, syntheticItalic, m_hasExternalSVGFont);
        } else {
#if ENABLE(SVG_FONTS)
            // In-Document SVG Fonts
            if (m_svgFontFaceElement)
                fontData = SimpleFontData::create(std::make_unique<SVGFontData>(m_svgFontFaceElement.get()), fontDescription.computedPixelSize(), syntheticBold, syntheticItalic);
#endif
        }
    } else {
        // Kick off the load. Do it soon rather than now, because we may be in the middle of layout,
        // and the loader may invoke arbitrary delegate or event handler code.
        fontSelector->beginLoadingFontSoon(m_font.get());

        // This temporary font is not retained and should not be returned.
        FontCachePurgePreventer fontCachePurgePreventer;
        SimpleFontData* temporaryFont = fontCache().getNonRetainedLastResortFallbackFont(fontDescription);
        fontData = SimpleFontData::create(temporaryFont->platformData(), true, true);
    }

    return fontData.release();
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
    return m_font->ensureCustomFontData(m_hasExternalSVGFont);
}
#endif

}
