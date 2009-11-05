/*
    Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef SVGTests_h
#define SVGTests_h

#if ENABLE(SVG)
#include <wtf/RefPtr.h>

namespace WebCore {

    class MappedAttribute;
    class String;
    class SVGStringList;
    class QualifiedName;

    class SVGTests {
    public:
        SVGTests();
        virtual ~SVGTests();

        SVGStringList* requiredFeatures() const;
        SVGStringList* requiredExtensions() const;
        SVGStringList* systemLanguage() const;

        bool hasExtension(const String&) const;

        bool isValid() const;
        
        bool parseMappedAttribute(MappedAttribute*);
        bool isKnownAttribute(const QualifiedName&);

    private:
        mutable RefPtr<SVGStringList> m_features;
        mutable RefPtr<SVGStringList> m_extensions;
        mutable RefPtr<SVGStringList> m_systemLanguage;
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGTests_h
