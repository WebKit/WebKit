/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#include "FontCache.h"
#include "FloatRect.h"
#include "FontDescription.h"
#include <wtf/MathExtras.h>
#include <unicode/uchar.h>
#include <unicode/unorm.h>

#if OS(DARWIN)
#include "WebCoreSystemInterface.h"
#endif

#include <wx/defs.h>
#include <wx/dcscreen.h>
#include <wx/string.h>
#include "fontprops.h"

namespace WebCore
{

void SimpleFontData::platformInit()
{    
    wxFont *font = m_platformData.font();
    if (font && font->IsOk()) {
        wxFontProperties props = wxFontProperties(font);
        m_fontMetrics.setAscent(props.GetAscent());
        m_fontMetrics.setDescent(props.GetDescent());
        m_fontMetrics.setXHeight(props.GetXHeight());
        m_fontMetrics.setUnitsPerEm(1); // FIXME!
        m_fontMetrics.setLineGap(props.GetLineGap());
        m_fontMetrics.setLineSpacing(props.GetLineSpacing());
    }

    m_syntheticBoldOffset = 0.0f;

#if OS(WINDOWS)
    m_scriptCache = 0;
    m_scriptFontProperties = 0;
    m_isSystemFont = false;
#endif
}

void SimpleFontData::platformCharWidthInit()
{
    m_avgCharWidth = 0.f;
    m_maxCharWidth = 0.f;
    initCharWidths();
}

void SimpleFontData::platformDestroy()
{
#if OS(WINDOWS)
    if (m_scriptFontProperties) {
        delete m_scriptFontProperties;
        m_scriptFontProperties = 0;
    }

    if (m_scriptCache)
        ScriptFreeCache(&m_scriptCache);
#endif
}

PassOwnPtr<SimpleFontData> SimpleFontData::createScaledFontData(const FontDescription& fontDescription, float scaleFactor) const
{
    FontDescription desc = FontDescription(fontDescription);
    desc.setSpecifiedSize(scaleFactor * fontDescription.computedSize());
    FontPlatformData platformData(desc, desc.family().family());
    return adoptPtr(new SimpleFontData(platformData, isCustomFont(), false));
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
#if OS(WINDOWS)
    return wxFontContainsCharacters(m_platformData.hfont(), characters, length);
#elif OS(DARWIN)
    return wxFontContainsCharacters(m_platformData.nsFont(), characters, length);
#endif
    return true;
}

void SimpleFontData::determinePitch()
{
    if (m_platformData.font() && m_platformData.font()->Ok())
        m_treatAsFixedPitch = m_platformData.font()->IsFixedWidth();
    else
        m_treatAsFixedPitch = false;
}

FloatRect SimpleFontData::platformBoundsForGlyph(Glyph) const
{
    return FloatRect();
}

float SimpleFontData::platformWidthForGlyph(Glyph glyph) const
{
#if __WXMSW__
    // under Windows / wxMSW we currently always use GDI fonts.
    return widthForGDIGlyph(glyph);
#elif OS(DARWIN)
    float pointSize = m_platformData.size();
    CGAffineTransform m = CGAffineTransformMakeScale(pointSize, pointSize);
    CGSize advance;
    NSFont* nsfont = (NSFont*)m_platformData.nsFont();
    if (!wkGetGlyphTransformedAdvances(m_platformData.cgFont(), nsfont, &m, &glyph, &advance)) {
        // LOG_ERROR("Unable to cache glyph widths for %@ %f", [nsfont displayName], pointSize);
        advance.width = 0;
    }
    return advance.width + m_syntheticBoldOffset;
#else
    // TODO: fix this! Make GetTextExtents a method of wxFont in 2.9
    int width = 10;
    GetTextExtent(*m_platformData.font(), (wxChar)glyph, &width, NULL);
    return width;
#endif
}

#if OS(WINDOWS)
SCRIPT_FONTPROPERTIES* SimpleFontData::scriptFontProperties() const
{
    // AFAICT this is never called even by the Win port anymore.
    return 0;
}

void SimpleFontData::initGDIFont()
{
    // unused by wx port
}

void SimpleFontData::platformCommonDestroy()
{
    // unused by wx port
}

float SimpleFontData::widthForGDIGlyph(Glyph glyph) const
{
    HDC hdc = GetDC(0);
    HGDIOBJ oldFont = SelectObject(hdc, m_platformData.hfont());
    int width;
    GetCharWidthI(hdc, glyph, 1, 0, &width);
    SelectObject(hdc, oldFont);
    ReleaseDC(0, hdc);
    return width;
}
#endif
    
#if OS(DARWIN)
const SimpleFontData* SimpleFontData::getCompositeFontReferenceFontData(NSFont *key) const
{
    return 0;
}
    
#endif

}
