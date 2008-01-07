/*
 * This file is part of the internal font implementation.  It should not be included by anyone other than
 * FontMac.cpp, FontWin.cpp and Font.cpp.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

// This file has no guards on purpose in order to detect redundant includes. This is a private header
// and so this may catch someone trying to include this file in public cpp files.

#include "FontSelector.h"
#include "SimpleFontData.h"
#include <wtf/Forward.h>

namespace WebCore {

class Font;
class GraphicsContext;
class IntRect;
class FontDescription;
class FontPlatformData;
class FontSelector;

const int cAllFamiliesScanned = -1;

class FontFallbackList : public RefCounted<FontFallbackList> {
public:
    FontFallbackList();

    void invalidate(PassRefPtr<FontSelector>);
    
    bool isFixedPitch(const Font* f) const { if (m_pitch == UnknownPitch) determinePitch(f); return m_pitch == FixedPitch; };
    void determinePitch(const Font*) const;

    bool loadingCustomFonts() const { return m_loadingCustomFonts; }

    FontSelector* fontSelector() const { return m_fontSelector.get(); }

private:
    const FontData* primaryFont(const Font* f) const { return fontDataAt(f, 0); }
    const FontData* fontDataAt(const Font*, unsigned index) const;
    const FontData* fontDataForCharacters(const Font*, const UChar*, int length) const;
    
    void setPlatformFont(const FontPlatformData&);

    mutable Vector<const FontData*, 1> m_fontList;
    mutable int m_familyIndex;
    mutable Pitch m_pitch;
    mutable bool m_loadingCustomFonts;
    RefPtr<FontSelector> m_fontSelector;

    friend class Font;
};

}

