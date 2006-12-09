/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

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
#ifdef SVG_SUPPORT

#include <SVGElement.h>
#include "StyleElement.h"

namespace WebCore {

    class SVGStyleElement : public SVGElement, public StyleElement {
    public:
        SVGStyleElement(const QualifiedName&, Document*);

        // Derived from: 'Element'
        virtual void insertedIntoDocument();
        virtual void removedFromDocument();
        virtual void childrenChanged();

        // 'SVGStyleElement' functions
        const AtomicString& xmlspace() const;
        void setXmlspace(const AtomicString&, ExceptionCode&);

        virtual const AtomicString& type() const;
        void setType(const AtomicString&, ExceptionCode&);

        const AtomicString& media() const;
        void setMedia(const AtomicString&, ExceptionCode&);

        const AtomicString& title() const;
        void setTitle(const AtomicString&, ExceptionCode&);
    };

} // namespace WebCore

#endif // SVG_SUPPORT
#endif // KSVG_SVGStyleElementImpl_H

// vim:ts=4:noet
