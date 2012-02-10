/*
 * Copyright (C) 2009, 2010, 2011 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef TileIndex_h
#define TileIndex_h

#include <limits>

namespace BlackBerry {
namespace WebKit {

class TileIndex {
public:
    TileIndex()
        : m_i(std::numeric_limits<unsigned int>::max())
        , m_j(std::numeric_limits<unsigned int>::max()) { }
    TileIndex(unsigned int i, unsigned int j)
        : m_i(i)
        , m_j(j) { }
    ~TileIndex() { }

    unsigned int i() const { return m_i; }
    unsigned int j() const { return m_j; }
    void setIndex(unsigned int i, unsigned int j)
    {
        m_i = i;
        m_j = j;
    }

private:
    bool m_isValid;
    unsigned int m_i;
    unsigned int m_j;
};

inline bool operator==(const BlackBerry::WebKit::TileIndex& a, const BlackBerry::WebKit::TileIndex& b)
{
    return a.i() == b.i() && a.j() == b.j();
}

inline bool operator!=(const BlackBerry::WebKit::TileIndex& a, const BlackBerry::WebKit::TileIndex& b)
{
    return a.i() != b.i() || a.j() != b.j();
}
}
}

#endif // TileIndex_h
