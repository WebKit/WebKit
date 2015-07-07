/*
 * Copyright (C) 2011 Leo Yang <leoyang@webkit.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef SVGGlyphRefElement_h
#define SVGGlyphRefElement_h

#if ENABLE(SVG_FONTS)
#include "SVGElement.h"
#include "SVGURIReference.h"

namespace WebCore {

class SVGGlyphRefElement final : public SVGElement,
                                 public SVGURIReference {
public:
    static PassRefPtr<SVGGlyphRefElement> create(const QualifiedName&, Document&);

    bool hasValidGlyphElement(String& glyphName) const;
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) override;

    // DOM interface
    const AtomicString& glyphRef() const;
    void setGlyphRef(const AtomicString&, ExceptionCode&);
    float x() const { return m_x; }
    void setX(float, ExceptionCode&);
    float y() const { return m_y; }
    void setY(float, ExceptionCode&);
    float dx() const { return m_dx; }
    void setDx(float, ExceptionCode&);
    float dy() const { return m_dy; }
    void setDy(float, ExceptionCode&);

private:
    SVGGlyphRefElement(const QualifiedName&, Document&);

    virtual bool rendererIsNeeded(const RenderStyle&) override { return false; }

    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGGlyphRefElement)
        DECLARE_ANIMATED_STRING(Href, href)
    END_DECLARE_ANIMATED_PROPERTIES

    float m_x;
    float m_y;
    float m_dx;
    float m_dy;
};

NODE_TYPE_CASTS(SVGGlyphRefElement)

}

#endif
#endif
