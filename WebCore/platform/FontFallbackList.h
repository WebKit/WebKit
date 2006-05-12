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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

// This file has no guards on purpose in order to detect redundant includes. This is a private header
// and so this should catch anyone trying to include this file in public cpp files.

#include <wtf/Vector.h>

namespace WebCore {

class Font;
class GraphicsContext;
class IntRect;
class FontData;

class FontFallbackList : public Shared<FontFallbackList>, Noncopyable {
public:
    FontFallbackList();
    ~FontFallbackList();

    // FIXME: Eventually FontFallbackLists will be hashed themselves (by FontDescription), and so instead of invalidating we'll just
    // drop our reference.
    void invalidate();
    
    bool isFixedPitch(const FontDescription& f) const { if (m_pitch == UnknownPitch) determinePitch(f); return m_pitch == FixedPitch; };
    void determinePitch(const FontDescription&) const;

private:
    mutable Pitch m_pitch;
    
    FontData* primaryFont(const FontDescription&) const;

#if __APPLE__
    // FIXME: FontData is still doing too much and is basically handling functionality that will be lifted up into
    // FontFallbackList and Font.  That's why we still only have one FontData object and don't yet have the vector.
    mutable FontData* m_font;
    mutable FontPlatformData m_platformFont;

    const FontPlatformData& platformFont(const FontDescription&) const;
    void setPlatformFont(const FontPlatformData&);
#else
    mutable Vector<FontData*> m_fontList;
#endif

    friend class Font;
};

}

