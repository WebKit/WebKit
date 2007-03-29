/*
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2006 George Staikos <staikos@kde.org> 
 *
 * All rights reserved.
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
#include "FontData.h"

#include "Font.h"
#include "FloatRect.h"
#include "FontCache.h"
#include "GlyphBuffer.h"
#include "FontDescription.h"

#include <QFont>
#include <QFontMetrics>

namespace WebCore {

void FontData::platformInit()
{
    QFontMetrics metrics(m_font.font());
    m_ascent = metrics.ascent();
    m_descent = metrics.descent();
    m_lineSpacing = metrics.lineSpacing();
    m_lineGap = metrics.leading();
    m_xHeight = metrics.xHeight();
}

void FontData::platformDestroy()
{
    delete m_font.fontPtr();
    delete m_smallCapsFontData;
}

FontData* FontData::smallCapsFontData(const FontDescription& fontDescription) const
{
    if (!m_smallCapsFontData) {
        FontDescription desc = FontDescription(fontDescription);
        desc.setSpecifiedSize(0.70f * fontDescription.computedSize());
        const FontPlatformData* pdata = new FontPlatformData(desc, desc.family().family());
        m_smallCapsFontData = new FontData(*pdata);
    }

    return m_smallCapsFontData;
}

bool FontData::containsCharacters(const UChar* characters, int length) const
{
    for (int i = 0; i < length; i++) {
        if (!QFontMetrics(m_font.font()).inFont(QChar(ushort(characters[i]))))
            return false;
    }

    return true;
}

void FontData::determinePitch()
{
    m_treatAsFixedPitch = m_font.isFixedPitch();
}

float FontData::platformWidthForGlyph(Glyph glyph) const
{
    return QFontMetricsF(m_font.font()).width(QChar(ushort(glyph)));
}

}

// vim: ts=4 sw=4 et
