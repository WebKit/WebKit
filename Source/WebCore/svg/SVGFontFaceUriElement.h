/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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

#include "CachedFontClient.h"
#include "CachedResourceHandle.h"
#include "SVGElement.h"

namespace WebCore {

class CSSFontFaceSrcValue;

class SVGFontFaceUriElement final : public SVGElement, public CachedFontClient {
    WTF_MAKE_ISO_ALLOCATED(SVGFontFaceUriElement);
public:
    static Ref<SVGFontFaceUriElement> create(const QualifiedName&, Document&);

    virtual ~SVGFontFaceUriElement();

    Ref<CSSFontFaceSrcValue> srcValue() const;

private:
    SVGFontFaceUriElement(const QualifiedName&, Document&);
    
    void parseAttribute(const QualifiedName&, const AtomString&) final;
    void childrenChanged(const ChildChange&) final;
    InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode&) final;
    bool rendererIsNeeded(const RenderStyle&) final { return false; }

    void loadFont();

    CachedResourceHandle<CachedFont> m_cachedFont;
};

} // namespace WebCore

#endif // ENABLE(SVG_FONTS)
