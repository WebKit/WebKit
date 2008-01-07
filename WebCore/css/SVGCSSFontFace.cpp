/*
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if ENABLE(SVG_FONTS)
#include "SVGCSSFontFace.h"

#include "FontDescription.h"
#include "SVGFontFaceElement.h"
#include "SimpleFontData.h"

namespace WebCore {

SVGCSSFontFace::SVGCSSFontFace(CSSFontSelector* selector, SVGFontFaceElement* fontFace)
    : CSSFontFace(selector)
    , m_fontFaceElement(fontFace)
{
    ASSERT(fontFace);
}

SVGCSSFontFace::~SVGCSSFontFace()
{
}

bool SVGCSSFontFace::isValid() const
{
    return true;
}
    
void SVGCSSFontFace::addSource(CSSFontFaceSource*)
{
    // no-op
}

SimpleFontData* SVGCSSFontFace::getFontData(const FontDescription& fontDescription, bool syntheticBold, bool syntheticItalic)
{
    if (!isValid())
        return 0;

    // TODO: Eventually we'll switch to CSSFontFaceSource here, which handles the caching for us.
    // (When implementing support for external SVG Fonts this we'll decide about that.)
    if (!m_fontData)
        m_fontData.set(m_fontFaceElement->createFontData(fontDescription));

    return m_fontData.get();
}

}

#endif
