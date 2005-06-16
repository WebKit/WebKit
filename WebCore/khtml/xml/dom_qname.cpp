/**
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
 */

#include <qptrdict.h>
#include "dom_qname.h"

namespace DOM {

static QPtrDict<QPtrDict<QPtrDict<QualifiedName::QualifiedNameImpl> > >* gNameCache = 0;

QualifiedName::QualifiedName(const AtomicString& p, const AtomicString& l, const AtomicString& n)
{
    // Obtain an appropriate inner from our dictionary.
    QPtrDict<QPtrDict<QualifiedNameImpl> >* namespaceDict = 0;
    QPtrDict<QualifiedNameImpl>* prefixDict = 0;
    if (gNameCache) {
        namespaceDict = gNameCache->find((void*)(n.implementation()));
        if (namespaceDict) {
            prefixDict = namespaceDict->find((void*)p.implementation());
            if (prefixDict)
                m_impl = prefixDict->find((void*)l.implementation());
        }
    }

    if (!m_impl) {
        m_impl = new QualifiedNameImpl(p, l, n);

        // Put the object into the hash.
        if (!gNameCache)
            gNameCache = new QPtrDict<QPtrDict<QPtrDict<QualifiedNameImpl> > >;
        
        if (!namespaceDict) {
            namespaceDict = new QPtrDict<QPtrDict<QualifiedNameImpl> >;
            namespaceDict->setAutoDelete(true);
            gNameCache->insert((void*)n.implementation(), namespaceDict);
        }
        
        if (!prefixDict) {
            prefixDict = new QPtrDict<QualifiedNameImpl>;
            namespaceDict->insert((void*)p.implementation(), prefixDict);
        }
        
        prefixDict->insert((void*)l.implementation(), m_impl);
    }
    
    ref();
}

QualifiedName::~QualifiedName()
{
    deref();
}

QualifiedName::QualifiedName(const QualifiedName& other)
{
    m_impl = other.m_impl;
    ref();
}

const QualifiedName& QualifiedName::operator=(const QualifiedName& other)
{
    if (m_impl != other.m_impl) {
        deref();
        m_impl = other.m_impl;
        ref();
    }
    
    return *this;
}

void QualifiedName::deref()
{
    if (m_impl->refCount() == 1) {
        // Before decrementing the ref to 0, remove ourselves from the hash table.
        QPtrDict<QPtrDict<QualifiedNameImpl> >* namespaceDict = 0;
        QPtrDict<QualifiedNameImpl>* prefixDict = 0;
        if (gNameCache) {
            namespaceDict = gNameCache->find((void*)(namespaceURI().implementation()));
            if (namespaceDict) {
                prefixDict = namespaceDict->find((void*)prefix().implementation());
                if (prefixDict)
                    prefixDict->remove((void*)localName().implementation());
            }
        }
    }
    
    m_impl->deref();
}

}
