/*
 * Copyright (C) 2006, 2007, 2008, 2010 Apple Inc.
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
#ifndef RefCountedHFONT_h
#define RefCountedHFONT_h

#include "StringImpl.h"
#include <wtf/RefCounted.h>

namespace WebCore {

class RefCountedHFONT : public RefCounted<RefCountedHFONT> {
public:
    static PassRefPtr<RefCountedHFONT> create(HFONT hfont) { return adoptRef(new RefCountedHFONT(hfont)); }
    static PassRefPtr<RefCountedHFONT> createDeleted() { return adoptRef(new RefCountedHFONT(reinterpret_cast<HFONT>(-1))); }

    ~RefCountedHFONT()
    {
        if (m_hfont != reinterpret_cast<HFONT>(-1))
            DeleteObject(m_hfont);
    }

    HFONT hfont() const { return m_hfont; }
    unsigned hash() const
    {
        return StringImpl::computeHash(reinterpret_cast<const UChar*>(&m_hfont), sizeof(HFONT) / sizeof(UChar));
    }

private:
    RefCountedHFONT(HFONT hfont)
        : m_hfont(hfont)
    {
    }

    HFONT m_hfont;
};

}

#endif
