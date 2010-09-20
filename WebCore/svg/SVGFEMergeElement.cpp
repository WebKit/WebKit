/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#include "config.h"

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "SVGFEMergeElement.h"

#include "SVGFEMergeNodeElement.h"

namespace WebCore {

inline SVGFEMergeElement::SVGFEMergeElement(const QualifiedName& tagName, Document* document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document)
{
}

PassRefPtr<SVGFEMergeElement> SVGFEMergeElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new SVGFEMergeElement(tagName, document));
}

PassRefPtr<FilterEffect> SVGFEMergeElement::build(SVGFilterBuilder* filterBuilder)
{
    RefPtr<FilterEffect> effect = FEMerge::create();
    FilterEffectVector& mergeInputs = effect->inputEffects();
    for (Node* n = firstChild(); n != 0; n = n->nextSibling()) {
        if (n->hasTagName(SVGNames::feMergeNodeTag)) {
            FilterEffect* mergeEffect = filterBuilder->getEffectById(static_cast<SVGFEMergeNodeElement*>(n)->in1());
            if (!mergeEffect)
                return 0;
            mergeInputs.append(mergeEffect);
        }
    }

    if (mergeInputs.isEmpty())
        return 0;

    return effect.release();
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
