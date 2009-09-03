/*
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 * Copyright (C) 2009 Maxime Simon <simon.maxime@gmail.com>
 *
 * All rights reserved.
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

#ifndef FontPlatformData_H
#define FontPlatformData_H

#include "FontDescription.h"
#include "GlyphBuffer.h"
#include <interface/Font.h>

namespace WebCore {

    class FontPlatformData {
    public:
        FontPlatformData(WTF::HashTableDeletedValueType)
            : m_font(hashTableDeletedFontValue())
            { }

        FontPlatformData()
            : m_font(0)
            { }

        FontPlatformData(const FontDescription&, const AtomicString& family);
        FontPlatformData(float size, bool bold, bool oblique);
        FontPlatformData(const FontPlatformData&);

        ~FontPlatformData();

        BFont* font() const { return m_font; }

        bool isFixedPitch();
        float size() const { return m_size; }
        bool bold() const { return m_bold; }
        bool oblique() const { return m_oblique; }

        unsigned hash() const;
        bool isHashTableDeletedValue() const;

        bool operator==(const FontPlatformData&) const;

#ifndef NDEBUG
        String description() const;
#endif

        BFont* m_font;
        float m_size;
        bool m_bold;
        bool m_oblique;

    private:
        static BFont* hashTableDeletedFontValue() { return reinterpret_cast<BFont*>(-1); }
    };

} // namespace WebCore

#endif

