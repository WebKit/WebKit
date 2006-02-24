/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2005 Apple Computer, Inc.
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
#ifndef QualifiedName_h
#define QualifiedName_h

#include "AtomicString.h"

namespace WebCore {

class QualifiedName {
public:
    class QualifiedNameImpl : public Shared<QualifiedNameImpl> {
    public:
        QualifiedNameImpl(const AtomicString& p, const AtomicString& l, const AtomicString& n) :m_prefix(p), m_localName(l), m_namespace(n) {}

        AtomicString m_prefix;
        AtomicString m_localName;
        AtomicString m_namespace;
    };

    QualifiedName(const AtomicString& p, const AtomicString& l, const AtomicString& n);
    ~QualifiedName();

    QualifiedName(const QualifiedName& other);
    const QualifiedName& operator=(const QualifiedName& other);

    bool operator==(const QualifiedName& other) const { return m_impl == other.m_impl; }
    bool operator!=(const QualifiedName& other) const { return !(*this == other); }

    bool matches(const QualifiedName& other) const { return m_impl == other.m_impl || (localName() == other.localName() && namespaceURI() == other.namespaceURI()); }

    bool hasPrefix() const { return m_impl->m_prefix != nullAtom; }
    void setPrefix(const AtomicString& prefix);

    const AtomicString& prefix() const { return m_impl->m_prefix; }
    const AtomicString& localName() const { return m_impl->m_localName; }
    const AtomicString& namespaceURI() const { return m_impl->m_namespace; }

    DOMString toString() const;

    // Init routine for globals
    static void init();

private:

    void ref() { m_impl->ref(); } 
    void deref();
    
    QualifiedNameImpl* m_impl;
};

#if !KHTML_QNAME_HIDE_GLOBALS
extern const QualifiedName anyName;
inline const QualifiedName& anyQName() { return anyName; }
#endif

inline bool operator==(const AtomicString& a, const QualifiedName& q) { return a == q.localName(); }
inline bool operator!=(const AtomicString& a, const QualifiedName& q) { return a != q.localName(); }
inline bool operator==(const QualifiedName& q, const AtomicString& a) { return a == q.localName(); }
inline bool operator!=(const QualifiedName& q, const AtomicString& a) { return a != q.localName(); }

}
#endif
