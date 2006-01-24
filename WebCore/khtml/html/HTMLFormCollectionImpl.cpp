/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
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
#include "config.h"
#include "HTMLFormCollectionImpl.h"
#include "HTMLGenericFormElementImpl.h"
#include "HTMLFormElementImpl.h"
#include "html_imageimpl.h"
#include "htmlnames.h"
#include <qptrvector.h>

namespace WebCore {

using namespace HTMLNames;

// since the collections are to be "live", we have to do the
// calculation every time if anything has changed

template<class T> static void appendToVector(QPtrVector<T> *vec, T *item)
{
    unsigned size = vec->size();
    unsigned count = vec->count();
    if (size == count)
        vec->resize(size == 0 ? 8 : (int)(size * 1.5));
    vec->insert(count, item);
}

HTMLFormCollectionImpl::HTMLFormCollectionImpl(NodeImpl* _base)
    : HTMLCollectionImpl(_base, 0)
{
    HTMLFormElementImpl *formBase = static_cast<HTMLFormElementImpl*>(m_base.get());
    if (!formBase->collectionInfo) {
        formBase->collectionInfo = new CollectionInfo();
    }
    info = formBase->collectionInfo;
}

HTMLFormCollectionImpl::~HTMLFormCollectionImpl()
{
}

unsigned HTMLFormCollectionImpl::calcLength() const
{
    QPtrVector<HTMLGenericFormElementImpl> &l = static_cast<HTMLFormElementImpl*>(m_base.get())->formElements;

    int len = 0;
    for ( unsigned i = 0; i < l.count(); i++ )
        if ( l.at( i )->isEnumeratable() )
            ++len;

    return len;
}

NodeImpl *HTMLFormCollectionImpl::item(unsigned index) const
{
    resetCollectionInfo();

    if (info->current && info->position == index) {
        return info->current;
    }
    if (info->haslength && info->length <= index) {
        return 0;
    }
    if (!info->current || info->position > index) {
        info->current = 0;
        info->position = 0;
        info->elementsArrayPosition = 0;
    }

    QPtrVector<HTMLGenericFormElementImpl> &l = static_cast<HTMLFormElementImpl*>(m_base.get())->formElements;
    unsigned currentIndex = info->position;
    
    for (unsigned i = info->elementsArrayPosition; i < l.count(); i++) {
        if (l[i]->isEnumeratable() ) {
            if (index == currentIndex) {
                info->position = index;
                info->current = l[i];
                info->elementsArrayPosition = i;
                return l[i];
            }

            currentIndex++;
        }
    }

    return 0;
}

NodeImpl* HTMLFormCollectionImpl::getNamedItem(NodeImpl*, const QualifiedName& attrName, const DOMString& name, bool caseSensitive) const
{
    info->position = 0;
    return getNamedFormItem(attrName, name, 0, caseSensitive);
}

NodeImpl* HTMLFormCollectionImpl::getNamedFormItem(const QualifiedName& attrName, const DOMString& name, int duplicateNumber, bool caseSensitive) const
{
    if (m_base->isElementNode()) {
        HTMLElementImpl* baseElement = static_cast<HTMLElementImpl*>(m_base.get());
        bool foundInputElements = false;
        if (baseElement->hasLocalName(formTag)) {
            HTMLFormElementImpl* f = static_cast<HTMLFormElementImpl*>(baseElement);
            for (unsigned i = 0; i < f->formElements.count(); ++i) {
                HTMLGenericFormElementImpl* e = f->formElements[i];
                if (e->isEnumeratable()) {
                    bool found;
                    if (caseSensitive)
                        found = e->getAttribute(attrName) == name;
                    else
                        found = e->getAttribute(attrName).domString().lower() == name.lower();
                    if (found) {
                        foundInputElements = true;
                        if (!duplicateNumber)
                            return e;
                        --duplicateNumber;
                    }
                }
            }
        }

        if (!foundInputElements) {
            HTMLFormElementImpl* f = static_cast<HTMLFormElementImpl*>(baseElement);

            for(unsigned i = 0; i < f->imgElements.count(); ++i)
            {
                HTMLImageElementImpl* e = f->imgElements[i];
                bool found;
                if (caseSensitive)
                    found = e->getAttribute(attrName) == name;
                else
                    found = e->getAttribute(attrName).domString().lower() == name.lower();
                if (found) {
                    if (!duplicateNumber)
                        return e;
                    --duplicateNumber;
                }
            }
        }
    }
    return 0;
}

NodeImpl * HTMLFormCollectionImpl::firstItem() const
{
    return item(0);
}

NodeImpl * HTMLFormCollectionImpl::nextItem() const
{
    return item(info->position + 1);
}

NodeImpl * HTMLFormCollectionImpl::nextNamedItemInternal(const DOMString &name) const
{
    NodeImpl *retval = getNamedFormItem( idsDone ? nameAttr : idAttr, name, ++info->position, true );
    if (retval)
        return retval;
    if (idsDone) // we're done
        return 0;
    // After doing id, do name
    idsDone = true;
    return getNamedItem(m_base->firstChild(), nameAttr, name, true);
}

NodeImpl *HTMLFormCollectionImpl::namedItem( const DOMString &name, bool caseSensitive ) const
{
    // http://msdn.microsoft.com/workshop/author/dhtml/reference/methods/nameditem.asp
    // This method first searches for an object with a matching id
    // attribute. If a match is not found, the method then searches for an
    // object with a matching name attribute, but only on those elements
    // that are allowed a name attribute.
    resetCollectionInfo();
    idsDone = false;
    info->current = getNamedItem(m_base->firstChild(), idAttr, name, true);
    if(info->current)
        return info->current;
    idsDone = true;
    info->current = getNamedItem(m_base->firstChild(), nameAttr, name, true);
    return info->current;
}

NodeImpl *HTMLFormCollectionImpl::nextNamedItem( const DOMString &name ) const
{
    // nextNamedItemInternal can return an item that has both id=<name> and name=<name>
    // Here, we have to filter out such cases.
    NodeImpl *impl = nextNamedItemInternal( name );
    if (!idsDone) // looking for id=<name> -> no filtering
        return impl;
    // looking for name=<name> -> filter out if id=<name>
    bool ok = false;
    while (impl && !ok)
    {
        if(impl->isElementNode())
        {
            HTMLElementImpl *e = static_cast<HTMLElementImpl *>(impl);
            ok = (e->getAttribute(idAttr) != name);
            if (!ok)
                impl = nextNamedItemInternal( name );
        } else // can't happen
            ok = true;
    }
    return impl;
}

void HTMLFormCollectionImpl::updateNameCache() const
{
    if (info->hasNameCache)
        return;

    HashSet<DOMStringImpl*, PointerHash<DOMStringImpl*> > foundInputElements;

    if (!m_base->hasTagName(formTag)) {
        info->hasNameCache = true;
        return;
    }

    HTMLElementImpl* baseElement = static_cast<HTMLElementImpl*>(m_base.get());

    HTMLFormElementImpl* f = static_cast<HTMLFormElementImpl*>(baseElement);
    for (unsigned i = 0; i < f->formElements.count(); ++i) {
        HTMLGenericFormElementImpl* e = f->formElements[i];
        if (e->isEnumeratable()) {
            const AtomicString& idAttrVal = e->getAttribute(idAttr);
            const AtomicString& nameAttrVal = e->getAttribute(nameAttr);
            if (!idAttrVal.isEmpty()) {
                // add to id cache
                QPtrVector<NodeImpl> *idVector = info->idCache.get(idAttrVal.impl());
                if (!idVector) {
                    idVector = new QPtrVector<NodeImpl>;
                    info->idCache.add(idAttrVal.impl(), idVector);
                }
                appendToVector(idVector, static_cast<NodeImpl *>(e));
                foundInputElements.add(idAttrVal.impl());
            }
            if (!nameAttrVal.isEmpty() && idAttrVal != nameAttrVal) {
                // add to name cache
                QPtrVector<NodeImpl> *nameVector = info->nameCache.get(nameAttrVal.impl());
                if (!nameVector) {
                    nameVector = new QPtrVector<NodeImpl>;
                    info->nameCache.add(nameAttrVal.impl(), nameVector);
                }
                appendToVector(nameVector, static_cast<NodeImpl *>(e));
                foundInputElements.add(nameAttrVal.impl());
            }
        }
    }

    for (unsigned i = 0; i < f->imgElements.count(); ++i) {
        HTMLImageElementImpl* e = f->imgElements[i];
        const AtomicString& idAttrVal = e->getAttribute(idAttr);
        const AtomicString& nameAttrVal = e->getAttribute(nameAttr);
        if (!idAttrVal.isEmpty() && !foundInputElements.contains(idAttrVal.impl())) {
            // add to id cache
            QPtrVector<NodeImpl> *idVector = info->idCache.get(idAttrVal.impl());
            if (!idVector) {
                idVector = new QPtrVector<NodeImpl>;
                info->idCache.add(idAttrVal.impl(), idVector);
            }
            appendToVector(idVector, static_cast<NodeImpl *>(e));
        }
        if (!nameAttrVal.isEmpty() && idAttrVal != nameAttrVal && !foundInputElements.contains(nameAttrVal.impl())) {
            // add to name cache
            QPtrVector<NodeImpl> *nameVector = info->nameCache.get(nameAttrVal.impl());
            if (!nameVector) {
                nameVector = new QPtrVector<NodeImpl>;
                info->nameCache.add(nameAttrVal.impl(), nameVector);
            }
            appendToVector(nameVector, static_cast<NodeImpl *>(e));
        }
    }

    info->hasNameCache = true;
}

}
