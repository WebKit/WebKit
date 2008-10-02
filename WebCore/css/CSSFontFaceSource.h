/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef CSSFontFaceSource_h
#define CSSFontFaceSource_h

#include "AtomicString.h"
#include "CachedResourceClient.h"
#include "CachedResourceHandle.h"
#include <wtf/HashMap.h>

#if ENABLE(SVG_FONTS)
#include "SVGFontFaceElement.h"
#endif

namespace WebCore {

class CachedFont;
class CSSFontFace;
class CSSFontSelector;
class FontDescription;
class SimpleFontData;

class CSSFontFaceSource : public CachedResourceClient {
public:
    CSSFontFaceSource(const String&, CachedFont* = 0);
    virtual ~CSSFontFaceSource();

    bool isLoaded() const;
    bool isValid() const;

    const AtomicString& string() const { return m_string; }

    void setFontFace(CSSFontFace* face) { m_face = face; }

    virtual void fontLoaded(CachedFont*);
    
    SimpleFontData* getFontData(const FontDescription&, bool syntheticBold, bool syntheticItalic, CSSFontSelector*);
    
    void pruneTable();

#if ENABLE(SVG_FONTS)
    SVGFontFaceElement* svgFontFaceElement() const { return m_svgFontFaceElement; }
    void setSVGFontFaceElement(SVGFontFaceElement* element) { m_svgFontFaceElement = element; }
#endif

private:
    AtomicString m_string; // URI for remote, built-in font name for local.
    CachedResourceHandle<CachedFont> m_font; // For remote fonts, a pointer to our cached resource.
    CSSFontFace* m_face; // Our owning font face.
    HashMap<unsigned, SimpleFontData*> m_fontDataTable; // The hash key is composed of size synthetic styles.

#if ENABLE(SVG_FONTS)
    SVGFontFaceElement* m_svgFontFaceElement;
    RefPtr<SVGFontElement> m_externalSVGFontElement;
#endif
};

}

#endif
