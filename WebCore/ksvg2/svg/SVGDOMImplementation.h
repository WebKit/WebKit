/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

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

#ifndef KSVG_SVGDOMImplementationImpl_H
#define KSVG_SVGDOMImplementationImpl_H
#if SVG_SUPPORT

#include "DOMImplementation.h"
#include "DeprecatedStringList.h"

namespace WebCore {
    class CSSStyleSheet;
    class Document;
    class DocumentType;
    class String;
    class StringImpl;

    typedef FrameView KDOMView;

    class SVGDOMImplementation : public DOMImplementation
    {
    public:
        SVGDOMImplementation();
        virtual ~SVGDOMImplementation();

        static SVGDOMImplementation *self();

        // 'SVGDOMImplementation' functions
        bool hasFeature(StringImpl *feature, StringImpl *version) const;
        PassRefPtr<DocumentType> createDocumentType(StringImpl *qualifiedName, StringImpl *publicId, StringImpl *systemId, ExceptionCode&) const;
        PassRefPtr<Document> createDocument(StringImpl *namespaceURI, StringImpl *qualifiedName, DocumentType *doctype, ExceptionCode&) const;
        PassRefPtr<Document> createDocument(StringImpl *namespaceURI, StringImpl *qualifiedName, DocumentType *doctype, bool createDocElement, FrameView*, ExceptionCode&) const;

        virtual PassRefPtr<CSSStyleSheet> createCSSStyleSheet(StringImpl *title, StringImpl *media) const;

        virtual DocumentType *defaultDocumentType() const;

        bool inAnimationContext() const;
        void setAnimationContext(bool value);

    private:
        bool m_animationContext : 1;

        static SVGDOMImplementation *s_instance;
        static DeprecatedStringList s_features;
    };
}

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
