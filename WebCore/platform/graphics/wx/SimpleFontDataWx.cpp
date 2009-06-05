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

#include <wx/defs.h>
#include <wx/dcscreen.h>
#include "fontprops.h"

namespace WebCore
{

void SimpleFontData::platformInit()
{    
    wxFont *font = m_platformData.font();
    if (font && font->IsOk()) {
        wxFontProperties props = wxFontProperties(font);
        m_ascent = props.GetAscent();
        m_descent = props.GetDescent();
        m_lineSpacing = props.GetLineSpacing();
        m_xHeight = props.GetXHeight();
        m_unitsPerEm = 1; // FIXME!
        m_lineGap = props.GetLineGap();
    }
}

void SimpleFontData::platformCharWidthInit()
{
    m_avgCharWidth = 0.f;
    m_maxCharWidth = 0.f;
    initCharWidths();
}

void SimpleFontData::platformDestroy()
{
    delete m_smallCapsFontData;
    m_smallCapsFontData = 0;
}

SimpleFontData* SimpleFontData::smallCapsFontData(const FontDescription& fontDescription) const
{
    if (!m_smallCapsFontData){
        FontDescription desc = FontDescription(fontDescription);
        desc.setSpecifiedSize(0.70f*fontDescription.computedSize());
        const FontPlatformData* pdata = new FontPlatformData(desc, desc.family().family());
        m_smallCapsFontData = new SimpleFontData(*pdata);
    }
    return m_smallCapsFontData;
}

bool SimpleFontData::containsCharacters(const UChar* characters, int length) const
{
    // FIXME: We will need to implement this to load non-ASCII encoding sites
    return true;
}

void SimpleFontData::determinePitch()
{
    if (m_platformData.font() && m_platformData.font()->Ok())
        m_treatAsFixedPitch = m_platformData.font()->IsFixedWidth();
    else
        m_treatAsFixedPitch = false;
}

float SimpleFontData::platformWidthForGlyph(Glyph glyph) const
{
    // TODO: fix this! Make GetTextExtents a method of wxFont in 2.9
    int width = 10;
    GetTextExtent(*m_platformData.font(), (wxChar)glyph, &width, NULL);
    return width;
}

}
