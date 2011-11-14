/*
 * Copyright (C) 2006 Kevin Ollivier  All rights reserved.
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
 
#ifndef FontPlatformData_h
#define FontPlatformData_h

#include "FontDescription.h"
#include "FontWidthVariant.h"
#include "FontOrientation.h"
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringImpl.h>

#include <wx/defs.h>
#include <wx/font.h>
#include <wx/gdicmn.h>

#if OS(DARWIN)
#include <ApplicationServices/ApplicationServices.h>

#if __OBJC__
@class NSFont;
#else
class NSFont;
#endif

inline CTFontRef toCTFontRef(NSFont *nsFont) { return reinterpret_cast<CTFontRef>(nsFont); }

#endif

namespace WebCore {

class FontHolder: public WTF::RefCounted<FontHolder>
{
public:
    FontHolder()
        : m_font(0)
    {}

    FontHolder(wxFont* font)
        : m_font(font)
    {}

    wxFont* font() { return m_font; }

private:
    wxFont* m_font;
};

class FontPlatformData {
public:
    enum FontState { UNINITIALIZED, DELETED, VALID };

    FontPlatformData(WTF::HashTableDeletedValueType)
    : m_fontState(DELETED)
    , m_font(0)
    , m_size(0)
#if OS(DARWIN)
    , m_atsuFontID(0)
#endif
    { }

    ~FontPlatformData();

    FontPlatformData(const FontDescription&, const AtomicString&);
    
    FontPlatformData(float size, bool bold, bool italic)
    : m_fontState(UNINITIALIZED)
    , m_font(0)
    , m_size(size)
#if OS(DARWIN)
    , m_atsuFontID(0)
#endif
    {
    }
    
    FontPlatformData() 
    : m_fontState(UNINITIALIZED)
    , m_font(0)
    , m_size(0)
#if OS(DARWIN)
    , m_atsuFontID(0)
#endif
    {
    }
    
    wxFont* font() const {
        return m_font->font();
    }
    
    unsigned hash() const {
        switch (m_fontState) {
        case DELETED:
            return -1;
        case UNINITIALIZED:
            return 0;
        case VALID:
            return computeHash();              
        }
    }

    unsigned computeHash() const;
    
    float size() const { return m_size; }

    bool operator==(const FontPlatformData& other) const
    { 
        if (m_font && m_fontState == VALID && other.m_fontState == VALID && other.m_font) {
            wxFont* thisFont = m_font->font();
            wxFont* otherFont = other.m_font->font();
            return thisFont->IsOk() && otherFont->IsOk() && thisFont->IsSameAs(*otherFont);
        }
        else
            return m_fontState == other.m_fontState;
    }

    bool isHashTableDeletedValue() const { return m_fontState == DELETED; }
    
    bool roundsGlyphAdvances() const { return false; }
    
    bool allowsLigatures() const { return false; }
    
    FontOrientation orientation() const { return Horizontal; } // FIXME: Implement.
    void setOrientation(FontOrientation) { } // FIXME: Implement.

    // We don't support this yet, so just return the default value for now.
    FontWidthVariant widthVariant() const { return RegularWidth; }

#if OS(WINDOWS)
    bool useGDI() const;
    HFONT hfont() const;
#endif

#if OS(DARWIN)
    ATSUFontID m_atsuFontID;
    CGFontRef cgFont() const;
    NSFont* nsFont() const { return m_nsFont; }
    CTFontRef ctFont() const { return reinterpret_cast<CTFontRef>(m_nsFont); }
    void cacheNSFont();
#endif

#ifndef NDEBUG
    String description() const;
#endif

private:
    FontState m_fontState;
    WTF::RefPtr<FontHolder> m_font;
    float m_size;
#if OS(DARWIN)
    NSFont* m_nsFont;
#endif
};

}

#endif
