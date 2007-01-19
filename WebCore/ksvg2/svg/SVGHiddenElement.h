/*
    Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>

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

#ifndef SVGHiddenElement_h
#define SVGHiddenElement_h

#ifdef SVG_SUPPORT

namespace WebCore {
    // Rather crude hack for <use> support. This class "hides" another SVGElement
    // derived class from the DOM, by assigning an arbitary node name, to exclude
    // it ie. from getElementsByTagName() operations.
    template<typename Type>
    class SVGHiddenElement : public Type {
    public:
        SVGHiddenElement<Type>(const QualifiedName& tagName, Document* document)
            : Type(tagName, document)
            , m_localName("webkitHiddenElement")
        {
        }

        virtual ~SVGHiddenElement()
        {
        }

        virtual const AtomicString& localName() const
        {
            return m_localName;
        }
 
    private:
        AtomicString m_localName;
   };

} // namespace WebCore

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
