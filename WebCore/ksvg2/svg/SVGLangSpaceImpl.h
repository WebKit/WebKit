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

#ifndef KSVG_SVGLangSpaceImpl_H
#define KSVG_SVGLangSpaceImpl_H
#if SVG_SUPPORT

#include "dom_atomicstring.h"

namespace KDOM
{
    class DOMStringImpl;
    class MappedAttributeImpl;
};

namespace KSVG
{
    class SVGAnimatedLengthImpl;
    class SVGAnimatedStringImpl;

    class SVGLangSpaceImpl
    {
    public:
        SVGLangSpaceImpl();
        virtual ~SVGLangSpaceImpl();

        // 'SVGLangSpace' functions
        const KDOM::AtomicString& xmllang() const;
        void setXmllang(const KDOM::AtomicString& xmlLang);

        const KDOM::AtomicString& xmlspace() const;
        void setXmlspace(const KDOM::AtomicString& xmlSpace);

        bool parseMappedAttribute(KDOM::MappedAttributeImpl *attr);

    private:
        KDOM::AtomicString m_lang;
        KDOM::AtomicString m_space;
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
