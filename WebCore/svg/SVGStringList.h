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

#ifndef SVGStringList_h
#define SVGStringList_h

#if ENABLE(SVG)
#include "PlatformString.h"
#include "SVGList.h"

namespace WebCore {

    class SVGStringList : public SVGList<String> {
    public:
        static PassRefPtr<SVGStringList> create(const QualifiedName& attributeName) { return adoptRef(new SVGStringList(attributeName)); }
        virtual ~SVGStringList();

        void reset(const String& str);
        void parse(const String& data, UChar delimiter = ',');
        
    private:
        SVGStringList(const QualifiedName&);
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGStringList_h
