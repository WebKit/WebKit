/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2006, 2007, 2009, 2010 Apple Inc. All right reserved.
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

#ifndef BidiContext_h
#define BidiContext_h

#include <wtf/Assertions.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/unicode/Unicode.h>

namespace WebCore {

// Used to keep track of explicit embeddings.
class BidiContext : public RefCounted<BidiContext> {
public:
    static PassRefPtr<BidiContext> create(unsigned char level, WTF::Unicode::Direction direction, bool override = false, BidiContext* parent = 0);

    BidiContext* parent() const { return m_parent.get(); }
    unsigned char level() const { return m_level; }
    WTF::Unicode::Direction dir() const { return static_cast<WTF::Unicode::Direction>(m_direction); }
    bool override() const { return m_override; }

private:
    BidiContext(unsigned char level, WTF::Unicode::Direction direction, bool override, BidiContext* parent)
        : m_level(level)
        , m_direction(direction)
        , m_override(override)
        , m_parent(parent)
    {
    }

    static PassRefPtr<BidiContext> createUncached(unsigned char level, WTF::Unicode::Direction, bool override, BidiContext* parent);

    unsigned char m_level;
    unsigned m_direction : 5; // Direction
    bool m_override : 1;
    RefPtr<BidiContext> m_parent;
};

bool operator==(const BidiContext&, const BidiContext&);

} // namespace WebCore

#endif // BidiContext_h
