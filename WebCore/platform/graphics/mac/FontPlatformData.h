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

#ifndef FontPlatformData_h
#define FontPlatformData_h

#include "StringImpl.h"

#ifdef __OBJC__
@class NSFont;
#else
class NSFont;
#endif

typedef struct CGFont* CGFontRef;
typedef UInt32 ATSUFontID;

#include <CoreFoundation/CFBase.h>
#include <objc/objc-auto.h>

namespace WebCore {

struct FontPlatformData {
    class Deleted {};

    FontPlatformData(Deleted)
    : m_syntheticBold(false), m_syntheticOblique(false), m_cgFont(0), m_atsuFontID(0), m_size(0), m_font((NSFont*)-1)
    {}

    FontPlatformData(float s, bool b, bool o)
        : m_syntheticBold(b)
        , m_syntheticOblique(o)
        , m_cgFont(0)
        , m_atsuFontID(0)
        , m_size(s)
        , m_font(0)
    {
    }

    FontPlatformData(NSFont* f = 0, bool b = false, bool o = false);
    
    FontPlatformData(CGFontRef f, ATSUFontID fontID, float s, bool b , bool o)
    : m_syntheticBold(b), m_syntheticOblique(o), m_cgFont(f), m_atsuFontID(fontID), m_size(s), m_font(0)
    {
    }

    FontPlatformData(const FontPlatformData& f);
    
    ~FontPlatformData();

    float size() const { return m_size; }

    bool m_syntheticBold;
    bool m_syntheticOblique;
    
    CGFontRef m_cgFont; // It is not necessary to refcount this, since either an NSFont owns it or some CachedFont has it referenced.
    ATSUFontID m_atsuFontID;
    float m_size;

    unsigned hash() const
    {
        ASSERT(m_font != 0 || m_cgFont == 0);
        uintptr_t hashCodes[2] = { (uintptr_t)m_font, m_syntheticBold << 1 | m_syntheticOblique };
        return StringImpl::computeHash(reinterpret_cast<UChar*>(hashCodes), sizeof(hashCodes) / sizeof(UChar));
    }

    bool operator==(const FontPlatformData& other) const
    { 
        return m_font == other.m_font && m_syntheticBold == other.m_syntheticBold && m_syntheticOblique == other.m_syntheticOblique && 
               m_cgFont == other.m_cgFont && m_size == other.m_size && m_atsuFontID == other.m_atsuFontID;
    }

    NSFont *font() const { return m_font; }
    void setFont(NSFont* font);

private:
    NSFont *m_font;
};

}

#endif
