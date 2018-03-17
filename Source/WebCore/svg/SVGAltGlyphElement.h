/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#include "SVGTextPositioningElement.h"
#include "SVGURIReference.h"

namespace WebCore {

class SVGGlyphElement;

class SVGAltGlyphElement final : public SVGTextPositioningElement, public SVGURIReference {
    WTF_MAKE_ISO_ALLOCATED(SVGAltGlyphElement);
public:
    static Ref<SVGAltGlyphElement> create(const QualifiedName&, Document&);

    const AtomicString& glyphRef() const;
    ExceptionOr<void> setGlyphRef(const AtomicString&);
    const AtomicString& format() const;
    ExceptionOr<void> setFormat(const AtomicString&);

    bool hasValidGlyphElements(Vector<String>& glyphNames) const;

private:
    SVGAltGlyphElement(const QualifiedName&, Document&);

    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) override;
    bool childShouldCreateRenderer(const Node&) const override;

    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGAltGlyphElement)
        DECLARE_ANIMATED_STRING_OVERRIDE(Href, href)
    END_DECLARE_ANIMATED_PROPERTIES
};

} // namespace WebCore

#endif
