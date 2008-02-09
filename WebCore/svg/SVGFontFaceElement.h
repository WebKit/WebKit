/*
   Copyright (C) 2007 Eric Seidel <eric@webkit.org>
   Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>

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

#ifndef SVGFontFaceElement_h
#define SVGFontFaceElement_h

#if ENABLE(SVG_FONTS)
#include "SVGElement.h"

namespace WebCore {

    class CSSFontFaceRule;
    class CSSMutableStyleDeclaration;
    class SVGFontElement;

    class SVGFontFaceElement : public SVGElement {
    public:
        SVGFontFaceElement(const QualifiedName&, Document*);
        virtual ~SVGFontFaceElement();

        virtual void parseMappedAttribute(MappedAttribute*);

        virtual void childrenChanged(bool changedByParser = false);
        virtual void insertedIntoDocument();

        unsigned unitsPerEm() const;
        int xHeight() const;
        float horizontalOriginX() const;
        float horizontalOriginY() const;
        float horizontalAdvanceX() const;
        float verticalOriginX() const;
        float verticalOriginY() const;
        float verticalAdvanceY() const;
        int ascent() const;
        int descent() const;
        String fontFamily() const;

        SVGFontElement* associatedFontElement() const;
        void rebuildFontFace();

    private:
        RefPtr<CSSFontFaceRule> m_fontFaceRule;
        RefPtr<CSSMutableStyleDeclaration> m_styleDeclaration;

        RefPtr<SVGFontElement> m_fontElement;
    };

} // namespace WebCore

#endif // ENABLE(SVG_FONTS)
#endif

// vim:ts=4:noet
