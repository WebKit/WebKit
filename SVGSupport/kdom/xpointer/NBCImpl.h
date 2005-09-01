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

#ifndef KDOM_XPointer_NBCImpl_H
#define KDOM_XPointer_NBCImpl_H

#include <qmap.h>
#include <qstring.h>

#include <kdom/Shared.h>
// #include <kdom/xpath/impl/XPathNSResolverImpl.h>

namespace KDOM
{

class DOMStringImpl;

namespace XPointer
{
    class NBCImpl : public Shared // : public XPathNSResolverImpl
    {
    public:
        NBCImpl(NBCImpl *parent);
        virtual ~NBCImpl();

        /**
         * Does not check syntactical validness of @prefix and @p ns.
         *
         * @returns true if the insertion succeeded.
         */
        bool addMapping(DOMStringImpl *prefix, DOMStringImpl *s);

        /**
         * The parent NBC.
         */
        NBCImpl *parentNBC() const;

        virtual DOMStringImpl *lookupNamespaceURI(DOMStringImpl *prefix) const;

    private:
        QMap<QString, QString> m_mapping;
        NBCImpl *m_parent;
    };
};

};

#endif

// vim:ts=4:noet
