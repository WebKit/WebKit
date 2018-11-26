/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2010 Dirk Schulze <krit@webkit.org>
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

#include "FilterEffect.h"
#include "SVGPreserveAspectRatioValue.h"

namespace WebCore {

class Document;
class Image;
class RenderElement;

class FEImage final : public FilterEffect {
public:
    static Ref<FEImage> createWithImage(Filter&, RefPtr<Image>, const SVGPreserveAspectRatioValue&);
    static Ref<FEImage> createWithIRIReference(Filter&, TreeScope&, const String&, const SVGPreserveAspectRatioValue&);

private:
    virtual ~FEImage() = default;
    FEImage(Filter&, RefPtr<Image>, const SVGPreserveAspectRatioValue&);
    FEImage(Filter&, TreeScope&, const String&, const SVGPreserveAspectRatioValue&);

    const char* filterName() const final { return "FEImage"; }

    FilterEffectType filterEffectType() const final { return FilterEffectTypeImage; }

    RenderElement* referencedRenderer() const;

    void platformApplySoftware() final;
    void determineAbsolutePaintRect() final;
    WTF::TextStream& externalRepresentation(WTF::TextStream&, RepresentationType) const final;

    RefPtr<Image> m_image;

    // m_treeScope will never be a dangling reference. See https://bugs.webkit.org/show_bug.cgi?id=99243
    TreeScope* m_treeScope { nullptr };
    String m_href;
    SVGPreserveAspectRatioValue m_preserveAspectRatio;
};

} // namespace WebCore
