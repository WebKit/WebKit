/*
 * This file is part of the CSS implementation for KDE.
 *
 * Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
 *               1999 Waldo Bastian (bastian@kde.org)
 * Copyright (C) 2004, 2006 Apple Computer, Inc.
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
 */

#ifndef CSSNamespace_H
#define CSSNamespace_H

#include "AtomicString.h"

namespace WebCore {

    struct CSSNamespace {
        AtomicString m_prefix;
        AtomicString m_uri;
        CSSNamespace* m_parent;

        CSSNamespace(const AtomicString& p, const AtomicString& u, CSSNamespace* parent) 
            : m_prefix(p)
            , m_uri(u)
            , m_parent(parent) {}
        ~CSSNamespace() { delete m_parent; }
        
        const AtomicString& uri() { return m_uri; }
        const AtomicString& prefix() { return m_prefix; }
        
        CSSNamespace* namespaceForPrefix(const AtomicString& prefix)
        {
            if (prefix == m_prefix)
                return this;
            if (m_parent)
                return m_parent->namespaceForPrefix(prefix);
            return 0;
        }
    };
}

#endif
