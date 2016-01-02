/*
 * Copyright (C) 2008, 2013 Apple Inc. All rights reserved.
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
#include "CSSSegmentedFontFace.h"

#include "CSSFontFace.h"
#include "CSSFontFaceSource.h"
#include "CSSFontSelector.h"
#include "Document.h"
#include "Font.h"
#include "FontCache.h"
#include "FontDescription.h"
#include "RuntimeEnabledFeatures.h"

namespace WebCore {

CSSSegmentedFontFace::CSSSegmentedFontFace(CSSFontSelector* fontSelector)
    : m_fontSelector(fontSelector)
{
}

CSSSegmentedFontFace::~CSSSegmentedFontFace()
{
    pruneTable();
    for (auto& face : m_fontFaces)
        face->removedFromSegmentedFontFace(this);
}

void CSSSegmentedFontFace::pruneTable()
{
    m_descriptionToRangesMap.clear();
}

bool CSSSegmentedFontFace::isValid() const
{
    // Valid if at least one font face is valid.
    for (auto& face : m_fontFaces) {
        if (face->isValid())
            return true;
    }
    return false;
}

void CSSSegmentedFontFace::fontLoaded(CSSFontFace*)
{
    pruneTable();

#if ENABLE(FONT_LOAD_EVENTS)
    if (RuntimeEnabledFeatures::sharedFeatures().fontLoadEventsEnabled() && !isLoading()) {
        Vector<RefPtr<LoadFontCallback>> callbacks;
        m_callbacks.swap(callbacks);
        for (size_t index = 0; index < callbacks.size(); ++index) {
            if (checkFont())
                callbacks[index]->notifyLoaded();
            else
                callbacks[index]->notifyError();
        }
    }
#endif
}

void CSSSegmentedFontFace::appendFontFace(Ref<CSSFontFace>&& fontFace)
{
    pruneTable();
    fontFace->addedToSegmentedFontFace(this);
    m_fontFaces.append(WTFMove(fontFace));
}

static void appendFontWithInvalidUnicodeRangeIfLoading(FontRanges& ranges, Ref<Font>&& font, const Vector<CSSFontFace::UnicodeRange>& unicodeRanges)
{
    if (font->isLoading()) {
        ranges.appendRange(FontRanges::Range(0, 0, WTFMove(font)));
        return;
    }

    unsigned numRanges = unicodeRanges.size();
    if (!numRanges) {
        ranges.appendRange(FontRanges::Range(0, 0x7FFFFFFF, WTFMove(font)));
        return;
    }

    for (unsigned j = 0; j < numRanges; ++j)
        ranges.appendRange(FontRanges::Range(unicodeRanges[j].from(), unicodeRanges[j].to(), font.copyRef()));
}

FontRanges CSSSegmentedFontFace::fontRanges(const FontDescription& fontDescription)
{
    if (!isValid())
        return FontRanges();

    FontTraitsMask desiredTraitsMask = fontDescription.traitsMask();

    auto addResult = m_descriptionToRangesMap.add(FontDescriptionKey(fontDescription), FontRanges());
    auto& fontRanges = addResult.iterator->value;

    if (addResult.isNewEntry) {
        for (auto& face : m_fontFaces) {
            if (!face->isValid())
                continue;

            FontTraitsMask traitsMask = face->traitsMask();
            bool syntheticBold = (fontDescription.fontSynthesis() & FontSynthesisWeight) && !(traitsMask & (FontWeight600Mask | FontWeight700Mask | FontWeight800Mask | FontWeight900Mask)) && (desiredTraitsMask & (FontWeight600Mask | FontWeight700Mask | FontWeight800Mask | FontWeight900Mask));
            bool syntheticItalic = (fontDescription.fontSynthesis() & FontSynthesisStyle) && !(traitsMask & FontStyleItalicMask) && (desiredTraitsMask & FontStyleItalicMask);

            if (RefPtr<Font> faceFont = face->font(fontDescription, syntheticBold, syntheticItalic))
                appendFontWithInvalidUnicodeRangeIfLoading(fontRanges, faceFont.releaseNonNull(), face->ranges());
        }
    }
    return fontRanges;
}

#if ENABLE(FONT_LOAD_EVENTS)
bool CSSSegmentedFontFace::isLoading() const
{
    for (auto& face : m_fontFaces) {
        if (face->loadState() == CSSFontFace::Loading)
            return true;
    }
    return false;
}

bool CSSSegmentedFontFace::checkFont() const
{
    for (auto& face : m_fontFaces) {
        if (face->loadState() != CSSFontFace::Loaded)
            return false;
    }
    return true;
}

void CSSSegmentedFontFace::loadFont(const FontDescription& fontDescription, PassRefPtr<LoadFontCallback> callback)
{
    fontRanges(fontDescription); // Kick off the load.

    if (callback) {
        if (isLoading())
            m_callbacks.append(callback);
        else if (checkFont())
            callback->notifyLoaded();
        else
            callback->notifyError();
    }
}
#endif

}
