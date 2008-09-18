/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#ifndef BindingURI_h
#define BindingURI_h
#if ENABLE(XBL)

#include "StringImpl.h"

namespace WebCore {

// This struct holds information about shadows for the text-shadow and box-shadow properties.

struct BindingURI {
    BindingURI(StringImpl*);
    ~BindingURI();

    BindingURI* copy();

    bool operator==(const BindingURI& o) const;
    bool operator!=(const BindingURI& o) const
    {
        return !(*this == o);
    }

    BindingURI* next() { return m_next; }
    void setNext(BindingURI* n) { m_next = n; }

    StringImpl* uri() { return m_uri; }

    BindingURI* m_next;
    StringImpl* m_uri;
};

} // namespace WebCore

#endif // ENABLE(XBL)
#endif // BindingURI_h
