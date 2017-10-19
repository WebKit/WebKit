/*
 * Copyright (C) 2008, 2009, 2015 Apple Inc. All rights reserved.
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
#include "FontRanges.h"

#include "Font.h"
#include "FontSelector.h"
#include <wtf/Assertions.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

const Font* FontRanges::Range::font(ExternalResourceDownloadPolicy policy) const
{
    return m_fontAccessor->font(policy);
}

FontRanges::FontRanges() = default;

class TrivialFontAccessor final : public FontAccessor {
public:
    static Ref<TrivialFontAccessor> create(Ref<Font>&& font)
    {
        return adoptRef(*new TrivialFontAccessor(WTFMove(font)));
    }

private:
    TrivialFontAccessor(RefPtr<Font>&& font)
        : m_font(WTFMove(font))
    {
    }

    const Font* font(ExternalResourceDownloadPolicy) const final
    {
        return m_font.get();
    }

    bool isLoading() const final
    {
        return m_font->isInterstitial();
    }

    RefPtr<Font> m_font;
};

FontRanges::FontRanges(RefPtr<Font>&& font)
{
    if (font)
        m_ranges.append(Range { 0, 0x7FFFFFFF, TrivialFontAccessor::create(font.releaseNonNull()) });
}

FontRanges::~FontRanges() = default;

GlyphData FontRanges::glyphDataForCharacter(UChar32 character, ExternalResourceDownloadPolicy policy) const
{
    const Font* resultFont = nullptr;
    for (auto& range : m_ranges) {
        if (range.from() <= character && character <= range.to()) {
            if (auto* font = range.font(policy)) {
                if (font->isInterstitial()) {
                    policy = ExternalResourceDownloadPolicy::Forbid;
                    if (!resultFont)
                        resultFont = font;
                } else {
                    auto glyphData = font->glyphDataForCharacter(character);
                    if (glyphData.glyph) {
                        auto* glyphDataFont = glyphData.font;
                        if (glyphDataFont && glyphDataFont->visibility() == Font::Visibility::Visible && resultFont && resultFont->visibility() == Font::Visibility::Invisible)
                            return GlyphData(glyphData.glyph, &glyphDataFont->invisibleFont());
                        return glyphData;
                    }
                }
            }
        }
    }
    if (resultFont) {
        // We want higher-level code to be able to differentiate between
        // "The interstitial font doesn't have the character" and
        // "The real downloaded font doesn't have the character".
        GlyphData result = resultFont->glyphDataForCharacter(character);
        if (!result.font)
            result.font = resultFont;
        return result;
    }
    return GlyphData();
}

const Font* FontRanges::fontForCharacter(UChar32 character) const
{
    return glyphDataForCharacter(character, ExternalResourceDownloadPolicy::Allow).font;
}

const Font& FontRanges::fontForFirstRange() const
{
    auto* font = m_ranges[0].font(ExternalResourceDownloadPolicy::Forbid);
    ASSERT(font);
    return *font;
}

bool FontRanges::isLoading() const
{
    for (auto& range : m_ranges) {
        if (range.fontAccessor().isLoading())
            return true;
    }
    return false;
}

} // namespace WebCore
