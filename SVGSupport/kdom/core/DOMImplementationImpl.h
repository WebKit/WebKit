/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    Based on khtml code by:
    Copyright (C) 1999 Lars Knoll (knoll@kde.org)
              (C) 1999 Antti Koivisto (koivisto@kde.org)
              (C) 2001 Dirk Mueller (mueller@kde.org)
              (C) 2002-2003 Apple Computer, Inc.

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

#ifndef KDOM_DOMImplementationImpl_H
#define KDOM_DOMImplementationImpl_H

#include <kdom/ls/DOMImplementationLSImpl.h>

namespace KDOM
{
    class KDOMView;
    class DOMString;
    class DocumentImpl;
    class CDFInterface;
    class DOMObjectImpl;
    class DOMStringImpl;
    class LSSerializerImpl;
    class DocumentTypeImpl;
    class CSSStyleSheetImpl;

    class DOMImplementationImpl : public DOMImplementationLSImpl
    {
    public:
        DOMImplementationImpl();
        virtual ~DOMImplementationImpl();

        static DOMImplementationImpl *self();

        // Gives access to shared css/ecma/... handler
        CDFInterface *cdfInterface() const;

        // 'DOMImplementationImpl' functions
        virtual bool hasFeature(DOMStringImpl *feature, DOMStringImpl *version) const; // DOM Level 2
        virtual DOMObjectImpl *getFeature(DOMStringImpl *feature, DOMStringImpl *version) const; // DOM Level 3

        virtual DocumentTypeImpl *createDocumentType(DOMStringImpl *qualifiedName, DOMStringImpl *publicId, DOMStringImpl *systemId) const;

        /**
         * @param createDocElement this is outside the specification, and is used internally for avoiding
         * creating document elements even if a @p qualifiedName and @p namespaceURI is supplied. If true,
         * it follows the specification, e.g, creates an element if the other arguments allows so.
         */
        virtual DocumentImpl *createDocument(DOMStringImpl *namespaceURI, DOMStringImpl *qualifiedName, DocumentTypeImpl *doctype, bool createDocElement = true, KDOMView *view = 0) const;

        virtual CSSStyleSheetImpl *createCSSStyleSheet(DOMStringImpl *title, DOMStringImpl *media) const;

        // Map events to types...
        virtual int typeToId(DOMStringImpl *type);
        virtual DOMStringImpl *idToType(int id);

        virtual DocumentTypeImpl *defaultDocumentType() const;

    protected:
        virtual CDFInterface *createCDFInterface() const;

    private:
        mutable CDFInterface *m_cdfInterface;

        static DOMImplementationImpl *s_instance;
    };
};

#endif

// vim:ts=4:noet
