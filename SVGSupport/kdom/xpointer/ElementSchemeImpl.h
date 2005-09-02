/*
 * This file is part of the KDE libraries
 *
 * Copyright (C) 2005 Frans Englich     <frans.englich@telia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB. If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef KDOM_XPointer_ElementSchemeImpl_H
#define KDOM_XPointer_ElementSchemeImpl_H

#include <kdom/xpointer/impl/PointerPartImpl.h>

#include <q3valuelist.h>
#include <qstringlist.h>

namespace KDOM
{

class DOMString;

namespace XPointer
{
    class NBC;
    class ShortHandImpl;

    /**
     * An implementation of the element() scheme, as described in:
     * http://www.w3.org/TR/xptr-element/
     *
     * @author Frans Englich <frans.englich@telia.com>
     */
    class ElementSchemeImpl : public PointerPartImpl
    {
    public:
        ElementSchemeImpl(DOMStringImpl *schemeData);
        virtual ~ElementSchemeImpl();

        virtual XPointerResultImpl *evaluate(NodeImpl *context) const;

    private:
        void parseChildSequence(const QStringList &steps);

        ShortHandImpl *m_shorthand;
        Q3ValueList<unsigned int> m_childSequence;
    };
};

};

#endif

// vim:ts=4:noet
