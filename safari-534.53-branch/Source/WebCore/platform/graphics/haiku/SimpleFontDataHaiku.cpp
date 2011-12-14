/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2009 Maxime Simon <simon.maxime@gmail.com> All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SimpleFontData.h"

#include "FloatRect.h"
#include "FontCache.h"
#include "FontDescription.h"
#include "NotImplemented.h"
#include "TextEncoding.h"
#include <wtf/text/CString.h>


namespace WebCore {

void SimpleFontData::platformInit()
{
    const BFont* font = m_platformData.font();
    if (!font)
        return;

    font_height height;
    font->GetHeight(&height);
    m_fontMetrics.setAscent(height.ascent);
    m_fontMetrics.setDescent(height.descent);
    m_fontMetrics.setXHeight(height.ascent * 0.56f); // Hack taken from the win port.
    m_fontMetrics.setLineGap(height.leading);
    m_fontMetrics.setLineSpacing(lroundf(height.ascent) + lroundf(height.descent) + lroundf(height.leading));
}

void SimpleFontData::platformCharWidthInit()
{
    m_avgCharWidth = 0.f;
    m_maxCharWidth = 0.f;
    initCharWidths();
}

void SimpleFontData::platformDestroy()
{
}

PassOwnPtr<SimpleFontData> SimpleFontData::createScaledFontData(const FontDescription& fontDescription, float scaleFactor) const
{
    FontDescription desc = FontDescription(fontDescription);
    desc.setSpecifiedSize(scaleFactor * fontDescription.computedSize());
    FontPlatformData fontPlatformData(desc, desc.family().family());
    return adoptPtr(new SimpleFontData(fontPlatformData, isCustomFont(), false));
}

SimpleFontData* SimpleFontData::smallCapsFontData(const FontDescription& fontDescription) const
{
    if (!m_derivedFontData)
        m_derivedFontData = DerivedFontData::create(isCustomFont());
    if (!m_derivedFontData->smallCaps)
        m_derivedFontData->smallCaps = createScaledFontData(fontDescription, .7);

    return m_derivedFontData->smallCaps.get();
}

SimpleFontData* SimpleFontData::emphasisMarkFontData(const FontDescription& fontDescription) const
{
    if (!m_derivedFontData)
        m_derivedFontData = DerivedFontData::create(isCustomFont());
    if (!m_derivedFontData->emphasisMark)
        m_derivedFontData->emphasisMark = createScaledFontData(fontDescription, .5);

    return m_derivedFontData->emphasisMark.get();
}

bool SimpleFontData::containsCharacters(const UChar* characters, int length) const
{
    // FIXME: We will need to implement this to load non-ASCII encoding sites
    return true;
}

void SimpleFontData::determinePitch()
{
    m_treatAsFixedPitch = m_platformData.font() && m_platformData.font()->IsFixed();
}

GlyphMetrics SimpleFontData::platformMetricsForGlyph(Glyph glyph, GlyphMetricsMode) const
{
    GlyphMetrics metrics;
    if (!m_platformData.font())
        return metrics;

    CString encoded = UTF8Encoding().encode(static_cast<UChar*>(&glyph), 1,
                                            URLEncodedEntitiesForUnencodables);
    float escapements[1];
    m_platformData.font()->GetEscapements(encoded.data(), 1, escapements);
    metrics.horizontalAdvance = escapements[0] * m_platformData.font()->Size();
    return metrics;
}

} // namespace WebCore

