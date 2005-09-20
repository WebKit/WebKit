/*
    Copyright (C) 2004-2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004-2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef KDOM_CDFInterface_H
#define KDOM_CDFInterface_H

#include <kdom/core/NodeImpl.h>

namespace KDOM
{
    /**
     * The CDFInterface class has to be implemented by 'layers on top'
     * of kdom (ie. ksvg2/khtml2). This class connects kdom with the
     * layer on top in following ways:
     *
     * - CSS values/properties access / render-style
     * - XML attribute/tag names access
     * - EcmaScript global object / EcmaInterface object
     *
     **/
    class RenderStyle;
    class GlobalObject;
    class EcmaInterface;
    class CDFInterface
    {
    public:
        CDFInterface();
        virtual ~CDFInterface();

        // CSS values/properties
        virtual const char *getValueName(NodeImpl::Id id) const;
        virtual const char *getPropertyName(NodeImpl::Id id) const;
        
        virtual int getValueID(const char *valStr, int len) const;
        virtual int getPropertyID(const char *propStr, int len) const;

        virtual RenderStyle *renderStyle() const;
        virtual bool cssPropertyApplyFirst(int id) const;

        // XML tags/attributes 
        virtual const char *getTagName(NodeImpl::Id id) const;
        virtual const char *getAttrName(NodeImpl::Id id) const;
        
        virtual int getTagID(const char *tagStr, int len) const;
        virtual int getAttrID(const char *attrStr, int len) const;

        // EcmaScript interface
        virtual EcmaInterface *ecmaInterface() const;
        virtual GlobalObject *globalObject(DocumentImpl *doc) const;
    };
};

#endif

// vim:ts=4:noet
