/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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
#include "SVGParserUtilities.h"

namespace WebCore {

// Describe an SVG <hkern>/<vkern> element
struct SVGKerningPair {
    UnicodeRanges unicodeRange1;
    HashSet<String> unicodeName1;
    HashSet<String> glyphName1;

    UnicodeRanges unicodeRange2;
    HashSet<String> unicodeName2;
    HashSet<String> glyphName2;
    float kerning { 0 };
};

class SVGFontElement final : public SVGElement {
    WTF_MAKE_ISO_ALLOCATED(SVGFontElement);
public:
    static Ref<SVGFontElement> create(const QualifiedName&, Document&);

private:
    SVGFontElement(const QualifiedName&, Document&);

    bool rendererIsNeeded(const RenderStyle&) final { return false; }

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGFontElement, SVGElement>;
    const SVGPropertyRegistry& propertyRegistry() const final { return m_propertyRegistry; }

    PropertyRegistry m_propertyRegistry { *this };
};

} // namespace WebCore

#endif // ENABLE(SVG_FONTS)
