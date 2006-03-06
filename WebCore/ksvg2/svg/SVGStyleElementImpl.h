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

#ifndef KSVG_SVGStyleElementImpl_H
#define KSVG_SVGStyleElementImpl_H
#if SVG_SUPPORT

#include <SVGElementImpl.h>

namespace WebCore
{
    class CSSStyleSheetImpl;
};

namespace WebCore
{
    class SVGStyleElementImpl : public SVGElementImpl
    {
    public:
        SVGStyleElementImpl(const QualifiedName& tagName, DocumentImpl *doc);
        virtual ~SVGStyleElementImpl();

        // Derived from: 'ElementImpl'
        virtual void childrenChanged();

        // 'SVGStyleElement' functions
        AtomicString xmlspace() const;
        void setXmlspace(const AtomicString&, ExceptionCode&);

        AtomicString type() const;
        void setType(const AtomicString&, ExceptionCode&);

        AtomicString media() const;
        void setMedia(const AtomicString&, ExceptionCode&);

        AtomicString title() const;
        void setTitle(const AtomicString&, ExceptionCode&);

        CSSStyleSheetImpl *sheet();

        // Internal
        bool isLoading() const;

    protected:
        RefPtr<CSSStyleSheetImpl> m_sheet;
        bool m_loading;
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
