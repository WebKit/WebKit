/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
#include "SVGFontElement.h"

namespace WebCore {

class SVGVKernElement final : public SVGElement {
    WTF_MAKE_ISO_ALLOCATED(SVGVKernElement);
public:
    static Ref<SVGVKernElement> create(const QualifiedName&, Document&);

    bool buildVerticalKerningPair(SVGKerningPair& kerningPair) const;

private:
    SVGVKernElement(const QualifiedName&, Document&);

    bool rendererIsNeeded(const RenderStyle&) final { return false; }
};

} // namespace WebCore

#endif // ENABLE(SVG_FONTS)
