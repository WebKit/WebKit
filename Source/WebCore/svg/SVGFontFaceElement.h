/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(SVG_FONTS)

#include "SVGElement.h"

namespace WebCore {

class SVGFontElement;
class StyleRuleFontFace;

class SVGFontFaceElement final : public SVGElement {
    WTF_MAKE_ISO_ALLOCATED(SVGFontFaceElement);
public:
    static Ref<SVGFontFaceElement> create(const QualifiedName&, Document&);

    unsigned unitsPerEm() const;
    int xHeight() const;
    int capHeight() const;
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
    
    StyleRuleFontFace& fontFaceRule() { return m_fontFaceRule.get(); }

private:
    SVGFontFaceElement(const QualifiedName&, Document&);
    ~SVGFontFaceElement();

    void parseAttribute(const QualifiedName&, const AtomString&) final;

    void childrenChanged(const ChildChange&) final;
    InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode&) final;
    void removedFromAncestor(RemovalType, ContainerNode&) final;

    bool rendererIsNeeded(const RenderStyle&) final { return false; }

    Ref<StyleRuleFontFace> m_fontFaceRule;
    SVGFontElement* m_fontElement;
};

} // namespace WebCore

#endif // ENABLE(SVG_FONTS)
