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
#include "SVGFEMergeElement.h"

#include "ElementIterator.h"
#include "FEMerge.h"
#include "FilterEffect.h"
#include "SVGFEMergeNodeElement.h"
#include "SVGFilterBuilder.h"
#include "SVGNames.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGFEMergeElement);

inline SVGFEMergeElement::SVGFEMergeElement(const QualifiedName& tagName, Document& document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document)
{
    ASSERT(hasTagName(SVGNames::feMergeTag));
}

Ref<SVGFEMergeElement> SVGFEMergeElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFEMergeElement(tagName, document));
}

RefPtr<FilterEffect> SVGFEMergeElement::build(SVGFilterBuilder* filterBuilder, Filter& filter) const
{
    auto effect = FEMerge::create(filter);
    FilterEffectVector& mergeInputs = effect->inputEffects();

    for (auto& mergeNode : childrenOfType<SVGFEMergeNodeElement>(*this)) {
        auto mergeEffect = filterBuilder->getEffectById(mergeNode.in1());
        if (!mergeEffect)
            return nullptr;
        mergeInputs.append(WTFMove(mergeEffect));
    }

    if (mergeInputs.isEmpty())
        return nullptr;

    return effect;
}

}
