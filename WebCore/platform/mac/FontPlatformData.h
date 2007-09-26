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

#include <CoreFoundation/CFBase.h>
#include <objc/objc-auto.h>

namespace WebCore {

struct FontPlatformData {
    class Deleted {};

    FontPlatformData(Deleted)
    : syntheticBold(false), syntheticOblique(false), m_font((NSFont*)-1)
    {}

    FontPlatformData(NSFont* f = 0, bool b = false, bool o = false)
    : syntheticBold(b), syntheticOblique(o), m_font(f)
    {
        if (f) 
            CFRetain(f);
    }

    FontPlatformData(const FontPlatformData& f)
    {
        m_font = (f.m_font && f.m_font != (NSFont*)-1) ? (NSFont*)CFRetain(f.m_font) : f.m_font;
        syntheticBold = f.syntheticBold;
        syntheticOblique = f.syntheticOblique;
    }

    ~FontPlatformData()
    {
        if (m_font && m_font != (NSFont*)-1)
            CFRelease(m_font);
    }

    bool syntheticBold;
    bool syntheticOblique;

    unsigned hash() const
    { 
        uintptr_t hashCodes[2] = { (uintptr_t)m_font, syntheticBold << 1 | syntheticOblique };
        return StringImpl::computeHash(reinterpret_cast<UChar*>(hashCodes), sizeof(hashCodes) / sizeof(UChar));
    }

    bool operator==(const FontPlatformData& other) const
    { 
        return m_font == other.m_font && syntheticBold == other.syntheticBold && syntheticOblique == other.syntheticOblique;
    }
    NSFont *font() const { return m_font; }
    void setFont(NSFont* font) {
        if (m_font == font)
            return;
        if (font && font != (NSFont*)-1)
            CFRetain(font);
        if (m_font && m_font != (NSFont*)-1)
            CFRelease(m_font);
        m_font = font;
    }
private:
    NSFont *m_font;
};

}

#endif
